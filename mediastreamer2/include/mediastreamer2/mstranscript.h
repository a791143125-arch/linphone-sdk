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

#ifndef mstranscript_h
#define mstranscript_h

#include "bctoolbox/list.h"
#include <mediastreamer2/msfilter.h>

/**
 * Structure carried as argument of the MS_TRANSCRIPT_EVENT
 **/

struct _MSTranscription {
	char transcribed_word[40];
	float timestamp; /**<time stamp of the transcribed word*/
	float begining;
	float confidence;
	bool_t is_final;
	bool_t end_of_sentence;
	uint32_t ssrc; // identifies the speaker
	bool_t correction;
	int sentence_id;
};
typedef struct _MSTranscription MSTranscription;

enum transcript_method { WHISPER_CPP_OVERLAP, VOSK };
struct _MSTranscriptEvent {
	MSTranscription transcription;
};
typedef struct _MSTranscriptEvent MSTranscriptEvent;

MSTranscription default_transcription_object(void);

/** Event generated when a word is transcribed */
#define MS_TRANSCRIPT_EVENT MS_FILTER_EVENT(MS_TRANSCRIPT_ID, 0, MSTranscriptEvent)

#define MS_TRANSCRIPT_SET_MODEL_PATH MS_FILTER_METHOD(MS_TRANSCRIPT_ID, 1, char *)
#define MS_TRANSCRIPT_INIT_MODEL MS_FILTER_METHOD(MS_TRANSCRIPT_ID, 2, enum transcript_method)
#define MS_TRANSCRIPT_START MS_FILTER_METHOD_NO_ARG(MS_TRANSCRIPT_ID, 3)
#define MS_TRANSCRIPT_PAUSE MS_FILTER_METHOD_NO_ARG(MS_TRANSCRIPT_ID, 4)
#define MS_TRANSCRIPT_FILE_DURATION MS_FILTER_METHOD(MS_TRANSCRIPT_ID, 5, int *)
#define MS_TRANSCRIPT_SET_CHUNK_DURATION MS_FILTER_METHOD(MS_TRANSCRIPT_ID, 6, float *)
#define MS_TRANSCRIPT_SET_OVERLAP_DURATION MS_FILTER_METHOD(MS_TRANSCRIPT_ID, 7, float *)
#define MS_TRANSCRIPT_SET_AUDIO_STREAM MS_FILTER_METHOD(MS_TRANSCRIPT_ID, 8, AudioStream *)

#endif
