/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
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

#ifndef _L_C_TRANSCRIPTION_CBS_H_
#define _L_C_TRANSCRIPTION_CBS_H_

#include "linphone/api/c-callbacks.h"
#include "linphone/api/c-types.h"

// =============================================================================

#ifdef __cplusplus
extern "C" {
#endif // ifdef __cplusplus

/**
 * @addtogroup call
 * @{
 */

/**
 * Create a new transcription callbacks object.
 * @return The #LinphoneTranscriptionCbs object. @notnil
 **/
LinphoneTranscriptionCbs *linphone_transcription_cbs_new(void);

/**
 * Acquire a reference to the transcription callbacks object.
 * @param cbs The #LinphoneTranscriptionCbs object. @notnil
 * @return The same transcription callbacks object. @notnil
 **/
LINPHONE_PUBLIC LinphoneTranscriptionCbs *linphone_transcription_cbs_ref(LinphoneTranscriptionCbs *cbs);

/**
 * Release reference to the transcription callbacks object.
 * @param cbs The #LinphoneTranscriptionCbs object. @notnil
 **/
LINPHONE_PUBLIC void linphone_transcription_cbs_unref(LinphoneTranscriptionCbs *cbs);

/**
 * Retrieve the user pointer associated with the transcription callbacks object.
 * @param cbs The #LinphoneTranscriptionCbs object. @notnil
 * @return The user pointer associated with the transcription callbacks object. @maybenil
 **/
LINPHONE_PUBLIC void *linphone_transcription_cbs_get_user_data(const LinphoneTranscriptionCbs *cbs);

/**
 * Assign a user pointer to the transcription callbacks object.
 * @param cbs The #LinphoneTranscriptionCbs object. @notnil
 * @param user_data The user pointer to associate with the transcription callbacks object. @maybenil
 **/
LINPHONE_PUBLIC void linphone_transcription_cbs_set_user_data(LinphoneTranscriptionCbs *cbs, void *user_data);

/**
 * Get the transcription to display callback.
 * @param cbs #LinphoneTranscriptionCbs object. @notnil
 * @return The current transcription to display callback.
 */
LINPHONE_PUBLIC LinphoneTranscriptionCbsDisplayCb
linphone_transcription_cbs_get_transcription_display(LinphoneTranscriptionCbs *cbs);

/**
 * Set the transcription to display callback.
 * @param cbs #LinphoneTranscriptionCbs object. @notnil
 * @param cb The transcription to display callback to be used.
 */
LINPHONE_PUBLIC void linphone_transcription_cbs_set_transcription_display(LinphoneTranscriptionCbs *cbs,
                                                                          LinphoneTranscriptionCbsDisplayCb cb);

/**
 * @}
 */
#ifdef __cplusplus
}
#endif // ifdef __cplusplus

#endif // ifndef _L_C_TRANSCRIPTION_CBS_H_
