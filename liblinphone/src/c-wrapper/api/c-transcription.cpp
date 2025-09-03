/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone
 * (see https://gitlab.linphone.org/BC/public/liblinphone).
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

#include "linphone/api/c-transcription.h"
#include "c-wrapper/c-wrapper.h"
#include "c-wrapper/internal/c-tools.h"
#include "linphone/api/c-callbacks.h"
#include "linphone/api/c-types.h"
#include "private.h"
#include "transcription/transcription.h"
#include <cstddef>
#include <cstdint>

using namespace LinphonePrivate;

LinphoneTranscription *linphone_transcription_new(LinphoneCore *lc) {
	return Transcription::createCObject(lc ? L_GET_CPP_PTR_FROM_C_OBJECT(lc) : nullptr);
}

// bool_t linphone_transcription_is_equal(const LinphoneTranscription *transcription,
//                                        const LinphoneTranscription *other_transcription) {
//     return Transcription::toCpp(transcription)->isEqual(*Transcription::toCpp(other_transcription));
// }

LinphoneTranscription *linphone_transcription_clone(const LinphoneTranscription *transcription) {
	return Transcription::toCpp(transcription)->clone()->toC();
}

LinphoneTranscription *linphone_transcription_ref(LinphoneTranscription *transcription) {
	if (transcription) {
		Transcription::toCpp(transcription)->ref();
		return transcription;
	}
	return NULL;
}

void linphone_transcription_unref(LinphoneTranscription *transcription) {
	if (transcription) {
		Transcription::toCpp(transcription)->unref();
	}
}

void linphone_transcription_add(LinphoneTranscription *transcription, MSTranscription tr) {
	if (transcription) {
		Transcription::toCpp(transcription)->addTranscription(tr);
	}
}

void linphone_transcription_add_cb(LinphoneTranscription *transcription, LinphoneTranscriptionCb cb) {
	if (transcription) {
		Transcription::toCpp(transcription)->addTranscriptionCb(cb);
	}
}

void linphone_transcription_set_display_cb(LinphoneTranscription *transcription, LinphoneTranscriptionDisplayCb cb) {
	if (transcription) {
		Transcription::toCpp(transcription)->setDisplayTranscriptionCb(cb);
	}
}

void linphone_transcription_set_audiostream(LinphoneTranscription *transcription, AudioStream *stream) {
	if (transcription) {
		Transcription::toCpp(transcription)->setAudioStream(stream);
	}
}

void linphone_transcription_activate(LinphoneTranscription *transcription, bool_t activate) {
	if (transcription) {
		Transcription::toCpp(transcription)->activate(activate);
	}
}

void linphone_transcription_set_conference(LinphoneTranscription *transcription, LinphoneConference *conference) {
	if (transcription) {
		Transcription::toCpp(transcription)->setConf(conference);
	}
}

const char *linphone_transcription_get_sentence_by_id(LinphoneTranscription *transcription, uint32_t id) {
	if (transcription) {
		return Transcription::toCpp(transcription)->getSentenceById(id);
	}
	return NULL;
}

const char *linphone_transcription_get_name_by_id(LinphoneTranscription *transcription, uint32_t sentence_id) {
	if (transcription) {
		return Transcription::toCpp(transcription)->getNameById(sentence_id);
	}
	return NULL;
}

uint32_t linphone_transcription_get_last_sentence_id(LinphoneTranscription *transcription) {
	if (transcription) {
		return Transcription::toCpp(transcription)->getLastSentenceId();
	}
	return 0;
}

// uint32_t *linphone_transcription_get_corrected_by_id(LinphoneTranscription *transcription, uint32_t sentence_id) {
// 	if (transcription) {
// 		return Transcription::toCpp(transcription)->getCorrectedById(sentence_id).data();
// 	}
// 	return 0;
// }
// size_t linphone_transcription_get_corrected_size_by_id(LinphoneTranscription *transcription, uint32_t sentence_id) {
// 	if (transcription) {
// 		return Transcription::toCpp(transcription)->getLastSentenceId();
// 	}
// 	return 0;
// }

// L_GET_CPP_PTR_FROM_C_OBJECT()