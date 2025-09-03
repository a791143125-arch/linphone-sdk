/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
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

#include "bctoolbox/defs.h"
#include "bctoolbox/tester.h"
#include "mediastreamer2/allfilters.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msfactory.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msinterfaces.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/mstranscript.h"
#include "mediastreamer2/msutils.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"
#include "ortp/port.h"
#include <ctype.h>
#include <vosk_api.h>
#include <whisper.h>

// TODO to clean
// #define AUDIO_FILE "sounds/hello16000.wav"    // english spoken
// #define AUDIO_FILE "sounds/hello16000-1s.wav" // english spoken
// #define AUDIO_FILE "sounds/audio_clean/2961-960-0001.wav" // english spoken
// #define AUDIO_FILE "sounds/hello8000.wav" // english spoken
#define AUDIO_FILE "sounds/367-130732-0026.wav" // english spoken
// #define AUDIO_FILE "sounds/367-130732-0026_blank_silence.wav" // english spoken
// #define AUDIO_FILE "sounds/237-126133-0002_noisy_long_silence.wav"
// #define AUDIO_FILE "sounds/audio_noisy_dirty/367-130732-0026_noisy.wav"

#define MAX_WORDS 1000
#define MAX_WORD_LEN 100

// Helper: Split string into lowercase words
int split_words(const char *input, char words[][MAX_WORD_LEN]) {
	char cleaned[4096] = {0}; // Large enough buffer
	int index = 0;

	// Remove punctuation and convert to lowercase
	for (size_t i = 0; input[i]; ++i) {
		if (!ispunct((unsigned char)input[i])) {
			cleaned[index++] = (char)tolower((unsigned char)input[i]);
		}
	}
	cleaned[index] = '\0';

	// Split into words
	int count = 0;
	char *token = strtok(cleaned, " \t\n\r");
	while (token && count < MAX_WORDS) {
		strncpy(words[count], token, MAX_WORD_LEN - 1);
		words[count][MAX_WORD_LEN - 1] = '\0'; // Ensure null-termination
		count++;
		token = strtok(NULL, " \t\n\r");
	}
	return count;
}

int levenshtein_distance(char ref[][MAX_WORD_LEN], int n, char hyp[][MAX_WORD_LEN], int m) {
	if (n < 0 || m < 0) return -1;

	int **dp = malloc((n + 1) * sizeof(int *));
	if (!dp) return -1;

	for (int i = 0; i <= n; ++i) {
		dp[i] = malloc((m + 1) * sizeof(int));
		if (!dp[i]) {
			// Cleanup in case of failure
			for (int k = 0; k < i; ++k)
				free(dp[k]);
			free(dp);
			return -1;
		}
	}

	for (int i = 0; i <= n; ++i)
		dp[i][0] = i;
	for (int j = 0; j <= m; ++j)
		dp[0][j] = j;

	for (int i = 1; i <= n; ++i) {
		for (int j = 1; j <= m; ++j) {
			if (strcmp(ref[i - 1], hyp[j - 1]) == 0) {
				dp[i][j] = dp[i - 1][j - 1];
			} else {
				int del = dp[i - 1][j] + 1;
				int ins = dp[i][j - 1] + 1;
				int sub = dp[i - 1][j - 1] + 1;
				dp[i][j] = (del < ins) ? (del < sub ? del : sub) : (ins < sub ? ins : sub);
			}
		}
	}

	// Make sure n and m are within bounds
	int result = (n >= 0 && m >= 0) ? dp[n][m] : 0;

	for (int i = 0; i <= n; ++i)
		free(dp[i]);
	free(dp);

	return result;
}

float compute_wer(const char *reference, const char *hypothesis) {
	char ref_words[MAX_WORDS][MAX_WORD_LEN];
	char hyp_words[MAX_WORDS][MAX_WORD_LEN];

	int ref_count = split_words(reference, ref_words);
	int hyp_count = split_words(hypothesis, hyp_words);

	if (ref_count == 0) {
		if (hyp_count == 0) return 0.0f; // Both empty => WER is 0
		else return 1.0f;                // Reference is empty, hypothesis not => WER is 100%
	}
	int dist = levenshtein_distance(ref_words, ref_count, hyp_words, hyp_count);
	return (float)dist / ref_count;
}

typedef struct {
	char *final_transcript;
	double date_start;
	float sum_delay;
	int nb_of_delays;
} TranscriptionInfo;

static void append_word(TranscriptionInfo *trans_info, char *word) {
	char *new_str;
	new_str = malloc(strlen(trans_info->final_transcript) + strlen(word) + 2);
	new_str[0] = '\0'; // ensures the memory is an empty string
	strcat(new_str, trans_info->final_transcript);
	strcat(new_str, " ");
	strcat(new_str, word);
	free(trans_info->final_transcript);
	trans_info->final_transcript = new_str;
}

static void print_message_event(MSTranscription *transcript, TranscriptionInfo *trans_info) {
	if (transcript->correction) return;
	ms_message(" %s ( %f ) final = %i, end_of_sentence = %i, ssrc = %u", transcript->transcribed_word,
	           transcript->timestamp, transcript->is_final, transcript->end_of_sentence, transcript->ssrc);
	struct timeval now;
	bctbx_gettimeofday(&now, 0);
	trans_info->sum_delay += ((now.tv_sec + now.tv_usec * 1e-6) - trans_info->date_start) - transcript->timestamp;
	trans_info->nb_of_delays++;
	append_word(trans_info, transcript->transcribed_word);
}

static void
segment_transcribed_cb(void *data, BCTBX_UNUSED(MSFilter *f), BCTBX_UNUSED(unsigned int event_id), void *event) {
	MSTranscriptEvent *receivedTranscription = NULL;
	receivedTranscription = (MSTranscriptEvent *)event;
	MSTranscription transcription;
	transcription.timestamp = receivedTranscription->transcription.timestamp;
	strncpy(transcription.transcribed_word, receivedTranscription->transcription.transcribed_word,
	        sizeof(transcription.transcribed_word));
	transcription.is_final = receivedTranscription->transcription.is_final;
	transcription.end_of_sentence = receivedTranscription->transcription.end_of_sentence;
	transcription.ssrc = receivedTranscription->transcription.ssrc;
	transcription.correction = receivedTranscription->transcription.correction;
	print_message_event(&transcription, (TranscriptionInfo *)data);
}

typedef struct _test_config {
	int sampling_rate;
	int nchannels;
	char *audio_file;
	char *noise_file;
	char *mic_rec_file;
	char *record_file;
	char *reference;
	float wer_expected;
	float delay_expected;
	float wer;
	float delay;
	float overlap_size_whisper;
	float chunk_size_whisper;
} test_config;

static void init_config(test_config *config) {
	config->sampling_rate = 16000;
	config->nchannels = 1;
	config->audio_file = NULL;
	config->noise_file = NULL;
	config->record_file = NULL;
	config->mic_rec_file = bc_tester_file("input_mic.wav");
	config->reference = "";
	config->wer = -1;
	config->delay = -1;
	config->wer_expected = 1;
	config->delay_expected = -1;
	config->overlap_size_whisper = 1.0f;
	config->chunk_size_whisper = 3.0f;
}

static void uninit_config(test_config *config) {
	if (config->audio_file) free(config->audio_file);
	if (config->noise_file) free(config->noise_file);
	if (config->mic_rec_file) ms_free(config->mic_rec_file);
	if (config->record_file) {
		// unlink(config->record_file);
		ms_free(config->record_file);
	}
	config->audio_file = NULL;
	config->noise_file = NULL;
	config->mic_rec_file = NULL;
	config->record_file = NULL;
}

static MSFactory *msFactory = NULL;

static int tester_before_all(void) {
	msFactory = ms_tester_factory_new();
	ms_factory_enable_statistics(msFactory, TRUE);
	return 0;
}

static int tester_after_all(void) {
	ms_factory_destroy(msFactory);
	return 0;
}

static void
fileplay_eof(void *user_data, BCTBX_UNUSED(MSFilter *f), unsigned int event, BCTBX_UNUSED(void *event_data)) {
	if (event == MS_FILE_PLAYER_EOF) {
		int *done = (int *)user_data;
		*done = TRUE;
	}
}

static bool_t create_player(MSFilter **player, char *input_file, int expected_nchannels) {
	if (!input_file) {
		BC_FAIL("no file to play");
		return FALSE;
	}
	int sampling_rate = 0;
	int nchannels = 0;
	*player = ms_factory_create_filter(msFactory, MS_FILE_PLAYER_ID);
	ms_filter_call_method_noarg(*player, MS_FILE_PLAYER_CLOSE);
	ms_filter_call_method(*player, MS_FILE_PLAYER_OPEN, input_file);
	ms_filter_call_method(*player, MS_FILTER_GET_SAMPLE_RATE, &sampling_rate);
	ms_filter_call_method(*player, MS_FILTER_GET_NCHANNELS, &nchannels);
	if (nchannels != expected_nchannels) {
		ms_filter_call_method_noarg(*player, MS_FILE_PLAYER_CLOSE);
		BC_FAIL("Audio file does not have the expected channel number");
		return FALSE;
	}
	if (!BC_ASSERT_PTR_NOT_NULL(*player)) {
		return FALSE;
	}
	return TRUE;
}

static bool_t
create_resampler(MSFilter **player, MSFilter **resampler, int expected_sampling_rate, int expected_nchannels) {
	int sampling_rate = 0;
	int nchannels = 0;
	ms_filter_call_method(*player, MS_FILTER_GET_SAMPLE_RATE, &sampling_rate);
	ms_filter_call_method(*player, MS_FILTER_GET_NCHANNELS, &nchannels);
	if (sampling_rate != expected_sampling_rate) {
		*resampler = ms_factory_create_filter(msFactory, MS_RESAMPLE_ID);
		ms_filter_call_method(*resampler, MS_FILTER_SET_SAMPLE_RATE, &sampling_rate);
		ms_filter_call_method(*resampler, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &expected_sampling_rate);
		ms_filter_call_method(*resampler, MS_FILTER_SET_NCHANNELS, &nchannels);
		ms_filter_call_method(*resampler, MS_FILTER_SET_OUTPUT_NCHANNELS, &expected_nchannels);
		ms_message("resample audio at rate %d to get %d Hz", sampling_rate, expected_sampling_rate);
	} else {
		return false;
	}
	return true;
}

static bool test_base(test_config *config) {

	if (!config->audio_file) {
		BC_FAIL("audio file missing.");
		return FALSE;
	}

	bool_t transcription_done = TRUE;
	MSFilter *player_audio = NULL;
	MSFilter *transcript = NULL;
	MSFilter *sound_rec = NULL;
	MSFilter *resampler_audio = NULL;
	MSFilter *recorder_mixer = NULL;
	MSFilter *tee = NULL;
	MSFilter *mixer_tee = NULL;
	unsigned int filter_mask =
	    FILTER_MASK_VOIDSINK | FILTER_MASK_FILEREC | FILTER_MASK_FILEPLAY | FILTER_MASK_VOIDSOURCE;
	const int expected_sampling_rate = config->sampling_rate;
	const int expected_nchannels = config->nchannels;
	int output_sampling_rate = expected_sampling_rate;
	int output_nchannels = expected_nchannels;
	int audio_done = 0;
	float waiting_time_ms = 30000.;
	bool_t send_silence = true;

	ms_factory_reset_statistics(msFactory);
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, msFactory);

	// Void_source config
	ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_SAMPLE_RATE, &config->sampling_rate);
	ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_NCHANNELS, &config->nchannels);
	ms_filter_call_method(ms_tester_voidsource, MS_VOID_SOURCE_SEND_SILENCE, &send_silence);

	// audio player
	if (!create_player(&player_audio, config->audio_file, config->nchannels)) {
		transcription_done = FALSE;
		goto end;
	}

	// mixer to mix silence and audio player
	recorder_mixer = ms_factory_create_filter(msFactory, MS_AUDIO_MIXER_ID);
	ms_filter_call_method(recorder_mixer, MS_FILTER_SET_SAMPLE_RATE, &config->sampling_rate);
	ms_filter_call_method(recorder_mixer, MS_FILTER_SET_NCHANNELS, &config->nchannels);
	mixer_tee = ms_factory_create_filter(msFactory, MS_TEE_ID);

	// audio transcript filter
	MSFilterDesc *filter_desc = ms_factory_lookup_filter_by_name(msFactory, "MSTranscript");
	transcript = ms_factory_create_filter_from_desc(msFactory, filter_desc);
	if (!BC_ASSERT_PTR_NOT_NULL(transcript)) goto end;
	char full_path[512];
	snprintf(full_path, sizeof(full_path), "%s/ggml-%s.bin", MODEL_PATH, MODEL_NAME);
	// snprintf(full_path, sizeof(full_path),
	//          "/home/antoine/Documents/linphone_sdk/linphone-sdk/external/vosk/vosk-model-en-us-0.22-lgraph");
	// snprintf(full_path, sizeof(full_path),
	//          "/home/antoine/Documents/linphone_sdk/linphone-sdk/external/vosk/vosk-model-small-fr-0.22");
	ms_filter_call_method(transcript, MS_TRANSCRIPT_SET_CHUNK_DURATION, &config->chunk_size_whisper);
	ms_filter_call_method(transcript, MS_TRANSCRIPT_SET_OVERLAP_DURATION, &config->overlap_size_whisper);
	ms_filter_call_method(transcript, MS_TRANSCRIPT_SET_MODEL_PATH, full_path);
	enum transcript_method transcription_method = WHISPER_CPP_OVERLAP;
	// enum transcript_method transcription_method = VOSK;
	if (ms_filter_call_method(transcript, MS_TRANSCRIPT_INIT_MODEL, &transcription_method) == -1) {
		ms_filter_destroy(transcript);
		transcript = NULL;
		ms_message("Transcription filter destroyed.");
	} else {
		create_resampler(&player_audio, &resampler_audio, config->sampling_rate, config->nchannels);
		tee = ms_factory_create_filter(msFactory, MS_TEE_ID);
	}
	// TODO: test enable/disable vers la liste des tests à la fin du fichier, on l'implémentera plus tard.
	int duration_ms;
	ms_filter_call_method(player_audio, MS_PLAYER_GET_DURATION, &duration_ms);
	ms_filter_call_method(transcript, MS_TRANSCRIPT_FILE_DURATION, &duration_ms);

	TranscriptionInfo *trans_info = ms_new0(TranscriptionInfo, 1);
	trans_info->date_start = 0;
	trans_info->final_transcript = strdup("");
	trans_info->nb_of_delays = 0;
	trans_info->sum_delay = 0;

	if (transcript)
		ms_filter_add_notify_callback(transcript, (MSFilterNotifyFunc)segment_transcribed_cb, trans_info, TRUE);

	if (resampler_audio) {
		ms_filter_call_method(player_audio, MS_FILTER_GET_SAMPLE_RATE, &output_sampling_rate);
		ms_filter_call_method(player_audio, MS_FILTER_GET_NCHANNELS, &output_nchannels);
	}

	// output record
	sound_rec = ms_factory_create_filter(msFactory, MS_FILE_REC_ID);
	ms_filter_call_method(sound_rec, MS_FILTER_SET_SAMPLE_RATE, &output_sampling_rate);
	ms_filter_call_method(sound_rec, MS_FILTER_SET_NCHANNELS, &output_nchannels);
	ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_CLOSE);
	ms_filter_call_method(sound_rec, MS_FILE_REC_OPEN, config->record_file);
	if (!BC_ASSERT_PTR_NOT_NULL(sound_rec)) {
		transcription_done = FALSE;
		goto end;
	}

	// filter graph
	MSConnectionHelper h;
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, player_audio, -1, 0);
	ms_connection_helper_link(&h, recorder_mixer, 0, 0);
	if (transcript) ms_connection_helper_link(&h, tee, 0, 0);
	ms_connection_helper_link(&h, sound_rec, 0, -1);

	if (transcript) {
		if (resampler_audio) {
			ms_filter_link(tee, 1, resampler_audio, 0);
			ms_filter_link(resampler_audio, 0, transcript, 0);
		} else {
			ms_filter_link(tee, 1, transcript, 0);
		}
	}

	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_link(&h, mixer_tee, 0, -1);

	if (mixer_tee) {
		ms_filter_link(mixer_tee, 1, recorder_mixer, 1);
	}

	ms_ticker_attach(ms_tester_ticker, player_audio);
	ms_filter_add_notify_callback(player_audio, fileplay_eof, &audio_done, TRUE);

	// play audio
	if (sound_rec) ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_START);
	if (ms_filter_call_method_noarg(player_audio, MS_PLAYER_START) == -1) {
		ms_error("Could not play audio. Playing filter failed to start");
	}
	struct timeval start_time;
	struct timeval now;
	float elapsed = 0.;
	int time_step_usec = 10000;
	bctbx_gettimeofday(&start_time, NULL);
	trans_info->date_start = start_time.tv_sec + start_time.tv_usec * 1e-6;
	while (audio_done != 1 && elapsed < waiting_time_ms) {
		bctbx_gettimeofday(&now, NULL);
		elapsed = ((now.tv_sec - start_time.tv_sec) * 1000.0f) + ((now.tv_usec - start_time.tv_usec) / 1000.0f);
		ms_usleep(time_step_usec);
	}

	// Sleep necessary to add blank audio in transcription to transcribe the end of the audio file
	ms_sleep(2);

	if (player_audio) ms_filter_call_method_noarg(player_audio, MS_FILE_PLAYER_CLOSE);
	if (sound_rec) ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_CLOSE);
	ms_ticker_detach(ms_tester_ticker, player_audio);
	float mean_delays = (trans_info->nb_of_delays != 0) ? trans_info->sum_delay / trans_info->nb_of_delays : -1;
	config->delay = mean_delays;
	ms_message("Mean delay = %f", mean_delays);
	BC_ASSERT_LOWER((float)mean_delays, (float)config->delay_expected, float, "%f");
	ms_message("Final transcript = %s", trans_info->final_transcript);
	float wer = compute_wer(config->reference, trans_info->final_transcript);
	ms_message("WER = %f", wer);
	config->wer = wer;
	BC_ASSERT_LOWER((float)wer, (float)config->wer_expected + 0.001, float, "%f");
	if (wer < config->wer_expected)
		ms_message("WER lower than before: old=%f, now=%f, update the associated transcript file", config->wer_expected,
		           wer);
	free(trans_info->final_transcript);
	ms_free(trans_info);

	// unlink filter graph
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, player_audio, -1, 0);
	ms_connection_helper_unlink(&h, recorder_mixer, 0, 0);
	if (transcript) {
		ms_connection_helper_unlink(&h, tee, 0, 0);
		if (resampler_audio) {
			ms_filter_unlink(tee, 1, resampler_audio, 0);
			ms_filter_unlink(resampler_audio, 0, transcript, 0);
		} else {
			ms_filter_unlink(tee, 1, transcript, 0);
		}
	}
	ms_connection_helper_unlink(&h, sound_rec, 0, -1);

	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_unlink(&h, mixer_tee, 0, -1);
	ms_filter_unlink(mixer_tee, 1, recorder_mixer, 1);

end:
	ms_factory_log_statistics(msFactory);
	if (player_audio) ms_filter_destroy(player_audio);
	if (resampler_audio) ms_filter_destroy(resampler_audio);
	if (transcript) ms_filter_destroy(transcript);
	if (sound_rec) ms_filter_destroy(sound_rec);
	if (tee) ms_filter_destroy(tee);
	if (mixer_tee) ms_filter_destroy(mixer_tee);
	if (recorder_mixer) ms_filter_destroy(recorder_mixer);
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
	return transcription_done;
}

static void english_talk(void) {
	test_config config;
	init_config(&config);
	config.audio_file = bc_tester_res(AUDIO_FILE);
	config.record_file = bc_tester_file("output_audio.wav");
	config.reference =
	    "TAKE THIRTY CRAWFISH FROM WHICH REMOVE THE GUT CONTAINING THE GALL IN THE FOLLOWING MANNER TAKE FIRM "
	    "HOLD OF THE CRAWFISH WITH THE LEFT HAND SO AS TO AVOID BEING PINCHED BY ITS CLAWS WITH THE THUMB AND "
	    "FOREFINGER OF THE RIGHT HAND PINCH THE EXTREME END OF THE CENTRAL FIN OF THE TAIL AND WITH A SUDDEN "
	    "JERK THE GUT WILL BE WITHDRAWN";
	config.wer_expected = 0.031250;
	config.delay_expected = 4.;
	remove(config.record_file);
	if (test_base(&config)) {
		BC_ASSERT(TRUE);
	}
	uninit_config(&config);
}

// this test is to verify that we do not transcribe all accumulated audio when the audio is done.
static void test_end_delay(void) {
	test_config config;
	init_config(&config);
	config.audio_file = bc_tester_res("sounds/hello16000.wav");
	config.record_file = bc_tester_file("output_audio.wav");
	config.reference = "&é";
	config.wer_expected = 1103;
	config.delay_expected = 1616161;
	config.overlap_size_whisper = 0.95f;
	config.chunk_size_whisper = 1.f;
	remove(config.record_file);
	struct timeval start_time;
	struct timeval now;
	bctbx_gettimeofday(&start_time, NULL);
	if (test_base(&config)) {
		BC_ASSERT(TRUE);
	}
	bctbx_gettimeofday(&now, NULL);
	BC_ASSERT_LOWER((now.tv_sec - start_time.tv_sec) * 1., 16., double, "%f");
	uninit_config(&config);
}

static void run_test_for_audio(const char *path,
                               const char *audio_id,
                               const bool is_noisy,
                               const char *reference,
                               const float wer,
                               const float delay,
                               float *sum_wer,
                               float *sum_delay) {
	test_config config;
	init_config(&config);

	char audio_path[512];
	if (is_noisy) {
		snprintf(audio_path, sizeof(audio_path), "sounds/%s/%s_noisy.wav", path, audio_id);
	} else {
		snprintf(audio_path, sizeof(audio_path), "sounds/%s/%s.wav", path, audio_id);
	}
	config.audio_file = bc_tester_res(audio_path);

	config.record_file = bc_tester_file("output_audio.wav");

	config.reference = strdup(reference);
	config.wer_expected = wer;
	config.delay_expected = delay;
	double similar = 1;
	const MSAudioDiffParams audio_cmp_params = {10, 200};
	remove(config.record_file);
	if (test_base(&config)) {
		BC_ASSERT_EQUAL(ms_audio_diff(config.audio_file, config.record_file, &similar, &audio_cmp_params, NULL, NULL),
		                0, int, "%d");
		BC_ASSERT(TRUE);
	}
	*sum_wer += config.wer;
	*sum_delay += config.delay;

	uninit_config(&config);
	free((void *)config.reference);
}

void test_on_audio_directory(char *transcript_path, char *path, const bool is_noisy) {
	char *full_transcript_path = bc_tester_res(transcript_path);
	FILE *file = fopen(full_transcript_path, "r");
	if (!file) {
		ms_error("Impossible to open %s\n", full_transcript_path);
		return;
	}

	float sum_delay = 0;
	float sum_wer = 0;
	float nb_delay = 0;
	float nb_wer = 0;
	char line[2048];
	while (fgets(line, sizeof(line), file)) {
		char id[256];
		char reference[1800];
		float wer = 0;
		float delay = 0;

		if (sscanf(line, "%255s %[^\n]", id, reference) == 2) {
			ms_message("Reference  = %s", reference);
			ms_message("ID = %s", id);
			if (fgets(line, sizeof(line), file)) wer = strtof(line, NULL);
			if (fgets(line, sizeof(line), file)) delay = strtof(line, NULL);
			run_test_for_audio(path, id, is_noisy, reference, wer, delay, &sum_wer, &sum_delay);
			nb_delay++;
			nb_wer++;
		} else {
			ms_error("Line ignored (wrong format) : %s", line);
		}
	}
	fclose(file);
	if (nb_wer) ms_message("Mean of wer on all files : %f", sum_wer / nb_wer);
	if (nb_delay) ms_message("Mean of delays on all files : %f", sum_delay / nb_delay);
	free(full_transcript_path);
}

static void english_talk_clean(void) {
	test_on_audio_directory("sounds/audio_clean/transcript_whispercpp.txt", "audio_clean", false);
}

static void english_talk_dirty(void) {
	test_on_audio_directory("sounds/audio_dirty/transcript_whispercpp.txt", "audio_dirty", false);
}

static void english_talk_noisy(void) {
	test_on_audio_directory("sounds/audio_noisy/transcript_whispercpp.txt", "audio_noisy", true);
}

static void english_talk_noisy_dirty(void) {
	test_on_audio_directory("sounds/audio_noisy_dirty/transcript_whispercpp.txt", "audio_noisy_dirty", true);
}

static void french_talk(void) {
	test_on_audio_directory("sounds/audio_fr/transcript_vosk.txt", "audio_fr", false);
}

static void french_talk_dirty(void) {
	test_on_audio_directory("sounds/audio_sale/transcript_vosk.txt", "audio_sale", false);
}

static void vosk_test_basic(void) {

	test_config config;
	init_config(&config);
	config.audio_file = bc_tester_res(AUDIO_FILE);
	// const char* model_path = "vosk-model-small-fr-0.22";  // Path to your Vosk model
	const char *model_path =
	    "/home/antoine/Documents/linphone_sdk/linphone-sdk/external/vosk/vosk-model-en-us-0.22-lgraph"; // Path to your
	                                                                                                    // Vosk model
	VoskModel *model = vosk_model_new(model_path);
	if (!model) {
		ms_error("Failed to load model");
	}

	VoskRecognizer *recognizer = vosk_recognizer_new(model, 16000.0);
	if (!recognizer) {
		ms_error("Failed to create recognizer");
	}

	FILE *wavin;
	char buf[3200];
	int nread, final;

	wavin = fopen(config.audio_file, "rb");
	fseek(wavin, 44, SEEK_SET);
	while (!feof(wavin)) {
		nread = fread(buf, 1, sizeof(buf), wavin);
		final = vosk_recognizer_accept_waveform(recognizer, buf, nread);
		if (final) {
			printf("%s\n", vosk_recognizer_result(recognizer));
		} else {
			printf("%s\n", vosk_recognizer_partial_result(recognizer));
		}
	}
	printf("%s\n", vosk_recognizer_final_result(recognizer));

	// Feed recognizer with audio and read results here...

	vosk_recognizer_free(recognizer);
	vosk_model_free(model);
	uninit_config(&config);
}

static test_t tests[] = {
    TEST_NO_TAG("english talk", english_talk),
    TEST_NO_TAG("english talk clean", english_talk_clean),
    TEST_NO_TAG("english talk dirty", english_talk_dirty),
    TEST_NO_TAG("english talk noisy", english_talk_noisy),
    TEST_NO_TAG("english talk noisy dirty", english_talk_noisy_dirty),
    TEST_NO_TAG("french talk", french_talk),
    TEST_NO_TAG("french talk dirty", french_talk_dirty),
    TEST_NO_TAG("accumulating end delay", test_end_delay),
    TEST_NO_TAG("vosk basic", vosk_test_basic),
};

test_suite_t audio_transcription_test_suite = {
    "Audio transcription", tester_before_all, tester_after_all, NULL, NULL, sizeof(tests) / sizeof(tests[0]), tests, 0};
