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
#include "core/core.h"
#include "linphone/api/c-transcription-cbs.h"
#include "transcription/transcription.h"

using namespace LinphonePrivate;

void _linphone_transcription_notify_result_to_display_available(LinphoneTranscription *transcription) {
	LINPHONE_HYBRID_OBJECT_INVOKE_CBS_NO_ARG(Transcription, Transcription::toCpp(transcription),
	                                         linphone_transcription_cbs_get_transcription_display);
}

LinphoneTranscription *linphone_core_create_transcription(LinphoneCore *core) {
	return Transcription::createCObject(core ? L_GET_CPP_PTR_FROM_C_OBJECT(core) : nullptr);
}

LinphoneTranscription *linphone_transcription_ref(LinphoneTranscription *transcription) {
	Transcription::toCpp(transcription)->ref();
	return transcription;
}

void linphone_transcription_unref(LinphoneTranscription *transcription) {
	Transcription::toCpp(transcription)->unref();
}

void linphone_transcription_set_user_data(LinphoneTranscription *transcription, void *user_data) {
	Transcription::toCpp(transcription)->setUserData(user_data);
}

void *linphone_transcription_get_user_data(const LinphoneTranscription *transcription) {
	return Transcription::toCpp(transcription)->getUserData();
}

void linphone_transcription_add_callbacks(LinphoneTranscription *transcription, LinphoneTranscriptionCbs *cbs) {
	Transcription::toCpp(transcription)->addCallbacks(TranscriptionCbs::toCpp(cbs)->getSharedFromThis());
}

void linphone_transcription_remove_callbacks(LinphoneTranscription *transcription, LinphoneTranscriptionCbs *cbs) {
	Transcription::toCpp(transcription)->removeCallbacks(TranscriptionCbs::toCpp(cbs)->getSharedFromThis());
}

LinphoneTranscriptionCbs *linphone_transcription_get_current_callbacks(const LinphoneTranscription *transcription) {
	return Transcription::toCpp(transcription)->getCurrentCallbacks()->toC();
}

const bctbx_list_t *linphone_transcription_get_callbacks_list(const LinphoneTranscription *transcription) {
	return Transcription::toCpp(transcription)->getCCallbacksList();
}

void linphone_transcription_start(LinphoneTranscription *transcription) {
	if (transcription) {
		Transcription::toCpp(transcription)->start();
	}
}

void linphone_transcription_pause(LinphoneTranscription *transcription) {
	if (transcription) {
		Transcription::toCpp(transcription)->pause();
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
