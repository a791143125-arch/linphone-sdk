/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "bctoolbox/list.h"
#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msasync.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msqueue.h"
#include <bctoolbox/defs.h>
#include <cstdint>
#include <iostream>
#include <map>

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/abstract-transcript.h"
#include "mediastreamer2/mstranscript.h"
#include "mediastreamer2/vosk-transcript.h"
#include "mediastreamer2/whisper-cpp-overlap-transcript.h"
#include <cmath>
#include <fstream>
#include <vector>
#include <whisper.h>

static void ms_transcription_copy(MSTranscription *trscrpt_src, MSTranscription *trscrpt_dst) {
	trscrpt_dst->timestamp = trscrpt_src->timestamp;
	strncpy(trscrpt_dst->transcribed_word, trscrpt_src->transcribed_word, sizeof(trscrpt_dst->transcribed_word));
	trscrpt_dst->ssrc = trscrpt_src->ssrc;
	trscrpt_dst->beggining = trscrpt_src->beggining;
	trscrpt_dst->confidence = trscrpt_src->confidence;
	trscrpt_dst->correction = trscrpt_src->correction;
	trscrpt_dst->end_of_sentence = trscrpt_src->end_of_sentence;
	trscrpt_dst->is_final = trscrpt_src->is_final;
	trscrpt_dst->sentence_id = trscrpt_src->sentence_id;
}

MSTranscription default_transcription_object() {
	MSTranscription ret;
	ret.timestamp = -1;
	ret.beggining = -1;
	ret.confidence = -1;
	ret.correction = false;
	ret.end_of_sentence = false;
	ret.sentence_id = -1;
	ret.is_final = false;
	ret.ssrc = 0;
	return ret;
}

void show_word_in_other_terminal(const std::string &word, std::string terminal_id) {
	std::ofstream term_out(terminal_id); // Replace with your actual tty
	if (term_out.is_open()) {
		term_out << "[Live Transcription] " << word << "   \n" << std::flush;
	} else {
		std::cerr << "Failed to open /dev/pts/1" << std::endl;
	}
}

int get_packets(MSFilter *f, MSBufferizer *buf) {
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	mblk_t *m;
	while ((m = ms_queue_get(f->inputs[0])) != NULL) {
		int size_before = buf->size;
		ms_bufferizer_put(buf, m);
		transcript->sizeOfDataSinceBeg += buf->size - size_before;
	}
	return 0;
};

void transcript_init(MSFilter *f) {
	MSTranscript *s = new MSTranscript();
	f->data = s;
}

void transcript_pre_process(MSFilter *f) {
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	auto *obj = static_cast<AbstractTranscript *>(transcript->transcriptionObj);
	if (!obj) {
		ms_error("No transcription object, please use ms_filter_call_method(transcript, "
		         "MS_TRANSCRIPT_SET_MODEL_PATH, full_path) and then ms_filter_call_method(transcript, "
		         "MS_TRANSCRIPT_INIT_MODEL, transcription_method) to initialize.");
		return;
	}
	transcript->wth = ms_worker_thread_new("Transcription process");
	transcript->buf = ms_bufferizer_new();
}

uint32_t get_ssrc(MSFilter *f) {
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	uint32_t ret = 0;
	if (transcript->audio_stream) {
		ret = transcript->audio_stream->active_speaker_ssrc;
	}
	return ret;
}

uint32_t get_nearest_ssrc_before_timestamp(float timestamp, std::map<float, uint32_t> map) {
	auto it = map.upper_bound(timestamp);
	if (it == map.begin()) {
		return 0; // Aucun timestamp <= donné
	}
	--it;
	return it->second;
}

void event_sender(MSFilter *f, std::vector<MSTranscription> transcriptions) {
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	if (!transcriptions.empty()) {
		std::string sentence = "";
		for (auto t : transcriptions) {
			t.ssrc = get_nearest_ssrc_before_timestamp(t.timestamp, transcript->ssrc_map);
			MSTranscriptEvent event;
			ms_transcription_copy(&t, &event.transcription);
			if (!t.correction) {
				sentence += t.transcribed_word;
				sentence += " ";
			}
			ms_filter_notify(f, MS_TRANSCRIPT_EVENT, &event);
		}
		if (sentence != "") show_word_in_other_terminal(sentence, "/dev/pts/2");
		ms_message("%s\n", sentence.c_str());
		transcriptions.clear();
	}
}

static bool_t async_transcript_process(void *data) {
	MSFilter *f = static_cast<MSFilter *>(data);
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	auto *obj = static_cast<AbstractTranscript *>(transcript->transcriptionObj);
	std::vector<MSTranscription> transcriptions;
	if (get_ssrc(f) != transcript->currentSsrc) {
		transcript->ssrc_map[transcript->sizeOfDataSinceBeg / (16000 * 2.)] = get_ssrc(f);
		transcript->currentSsrc = get_ssrc(f);
		auto it = transcript->ssrc_map.lower_bound(transcript->sizeOfDataSinceBeg / (16000 * 2.) - 2);
		transcript->ssrc_map.erase(transcript->ssrc_map.begin(), it);
	}
	transcriptions = obj->process(f);
	event_sender(f, transcriptions);
	return true;
}

static bool_t async_transcript_post_process(void *data) {
	MSFilter *f = static_cast<MSFilter *>(data);
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	auto *obj = static_cast<AbstractTranscript *>(transcript->transcriptionObj);
	std::vector<MSTranscription> transcriptions;
	transcriptions = obj->postProcess(f);
	event_sender(f, transcriptions);
	return true;
}

void transcript_process(MSFilter *f) {
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	auto *obj = static_cast<AbstractTranscript *>(transcript->transcriptionObj);
	if (!obj) {
		ms_queue_flush(f->inputs[0]);
		return;
	}
	if (!transcript->enable) {
		ms_queue_flush(f->inputs[0]);
		return;
	}
	get_packets(f, transcript->buf);
	ms_worker_thread_add_task(transcript->wth, async_transcript_process, f);
}

void transcript_post_process(MSFilter *f) {
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	auto *obj = static_cast<AbstractTranscript *>(transcript->transcriptionObj);
	if (!obj) {
		ms_queue_flush(f->inputs[0]);
		return;
	}
	if (!transcript->enable) {
		ms_queue_flush(f->inputs[0]);
		return;
	}
	get_packets(f, transcript->buf);
	bctbx_list_for_each(transcript->wth->tasks, (bctbx_list_iterate_func)ms_task_cancel_and_destroy);
	ms_bufferizer_flush(transcript->buf);
	MSTask *task = ms_worker_thread_add_waitable_task(transcript->wth, async_transcript_post_process, f);
	ms_task_wait_completion(task);
	ms_task_destroy(task);
}

void transcript_uninit(MSFilter *f) {
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	auto *obj = static_cast<AbstractTranscript *>(transcript->transcriptionObj);
	if (transcript->wth) ms_worker_thread_destroy(transcript->wth, true);
	if (transcript->buf) ms_bufferizer_destroy(transcript->buf);
	if (obj) {
		obj->uninit(f);
		delete obj;
	}
	delete transcript;
	f->data = nullptr;
}

static int ms_transcript_set_model_path(MSFilter *f, void *arg) {
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	if (arg != nullptr) transcript->modelPath = std::string((char *)arg);
	return 0;
}

static int ms_transcript_init_model(MSFilter *f, void *arg) {
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	enum transcript_method transcriptionSolution = *(enum transcript_method *)arg;
	if (transcriptionSolution == WHISPER_CPP_OVERLAP) {
		if (transcript->modelPath.empty()) {
			ms_error("No model_path initialized, please use ms_filter_call_method(transcript_filter, "
			         "MS_TRANSCRIPT_SET_MODEL_PATH, full_path) before initializing the transcription object");
			return -1;
		}
		transcript->transcriptionObj = new WhisperCPPOverlapTranscript(
		    transcript->modelPath, 16000, transcript->chunk_duration, transcript->overlap_duration);
	} else if (transcriptionSolution == VOSK) {
		if (transcript->modelPath.empty()) {
			ms_error("No model_path initialized, please use ms_filter_call_method(transcript_filter, "
			         "MS_TRANSCRIPT_SET_MODEL_PATH, full_path) before initializing the transcription object");
			return -1;
		}
		transcript->transcriptionObj = new VoskTranscript(transcript->modelPath, 16000);
	} else {
		ms_error("Transcription solution given does not exist, make sure to use a transcription solution from enum "
		         "transcript_method.");
		return -1;
	}
	auto *obj = static_cast<AbstractTranscript *>(transcript->transcriptionObj);
	if (obj->init(f) == -1) {
		return -1;
	}
	return 0;
}

static int ms_transcript_set_overlap_duration(MSFilter *f, void *arg) {
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	transcript->overlap_duration = *(float *)arg;
	return 0;
}

static int ms_transcript_set_chunk_duration(MSFilter *f, void *arg) {
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	transcript->chunk_duration = *(float *)arg;
	return 0;
}

static int ms_transcript_start_transcription(MSFilter *f, void *arg) {
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	transcript->enable = *(bool_t *)arg;
	if (transcript->enable) {
		ms_message("Transcription started");
	} else {
		ms_message("Transcription stopped");
	}
	return 0;
}

static int ms_transcript_set_file_duration(MSFilter *f, void *arg) {
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	auto *obj = static_cast<AbstractTranscript *>(transcript->transcriptionObj);
	if (obj) obj->setFileDuration(*(int *)arg);
	return 0;
}

static int ms_transcript_set_audio_stream(MSFilter *f, void *arg) {
	MSTranscript *transcript = static_cast<MSTranscript *>(f->data);
	transcript->audio_stream = (AudioStream *)arg;
	return 0;
}

static MSFilterMethod transcript_methods[] = {{MS_TRANSCRIPT_SET_MODEL_PATH, ms_transcript_set_model_path},
                                              {MS_TRANSCRIPT_INIT_MODEL, ms_transcript_init_model},
                                              {MS_TRANSCRIPT_START, ms_transcript_start_transcription},
                                              {MS_TRANSCRIPT_FILE_DURATION, ms_transcript_set_file_duration},
                                              {MS_TRANSCRIPT_SET_CHUNK_DURATION, ms_transcript_set_chunk_duration},
                                              {MS_TRANSCRIPT_SET_OVERLAP_DURATION, ms_transcript_set_overlap_duration},
                                              {MS_TRANSCRIPT_SET_AUDIO_STREAM, ms_transcript_set_audio_stream},
                                              {0, NULL}};

extern "C" {

MSFilterDesc ms_transcript_desc = {MS_TRANSCRIPT_ID,
                                   "MSTranscript",
                                   "Audio transcript filter.",
                                   MS_FILTER_OTHER,
                                   nullptr,
                                   1,
                                   0,
                                   transcript_init,
                                   transcript_pre_process,
                                   transcript_process,
                                   transcript_post_process,
                                   transcript_uninit,
                                   transcript_methods};
}

MS_FILTER_DESC_EXPORT(ms_transcript_desc)
