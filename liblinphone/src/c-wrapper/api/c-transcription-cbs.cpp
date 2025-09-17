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

#include "linphone/api/c-transcription-cbs.h"
#include "transcription/transcription.h"

// =============================================================================

using namespace LinphonePrivate;

LinphoneTranscriptionCbs *linphone_transcription_cbs_new(void) {
	return TranscriptionCbs::createCObject();
}

LinphoneTranscriptionCbs *linphone_transcription_cbs_ref(LinphoneTranscriptionCbs *cbs) {
	TranscriptionCbs::toCpp(cbs)->ref();
	return cbs;
}

void linphone_transcription_cbs_unref(LinphoneTranscriptionCbs *cbs) {
	TranscriptionCbs::toCpp(cbs)->unref();
}

void *linphone_transcription_cbs_get_user_data(const LinphoneTranscriptionCbs *cbs) {
	return TranscriptionCbs::toCpp(cbs)->getUserData();
}

void linphone_transcription_cbs_set_user_data(LinphoneTranscriptionCbs *cbs, void *ud) {
	TranscriptionCbs::toCpp(cbs)->setUserData(ud);
}

LinphoneTranscriptionCbsDisplayCb linphone_transcription_cbs_get_transcription_display(LinphoneTranscriptionCbs *cbs) {
	return TranscriptionCbs::toCpp(cbs)->getTranscriptionDisplay();
}

void linphone_transcription_cbs_set_transcription_display(LinphoneTranscriptionCbs *cbs,
                                                          LinphoneTranscriptionCbsDisplayCb cb) {
	TranscriptionCbs::toCpp(cbs)->setTranscriptionDisplay(cb);
}
