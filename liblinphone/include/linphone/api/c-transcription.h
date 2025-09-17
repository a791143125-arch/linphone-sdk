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

#ifndef L_C_LINPHONE_TRANSCRIPTION_H_
#define L_C_LINPHONE_TRANSCRIPTION_H_

#include "linphone/api/c-transcription-cbs.h"
#include "linphone/api/c-types.h"
#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/mstranscript.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Release a #LinphoneTranscription.
 * @param transcription the #LinphoneTranscription object @notnil
 */
LINPHONE_PUBLIC void linphone_transcription_unref(LinphoneTranscription *transcription);

/**
 * Set a user (application) pointer.
 * @param transcription #LinphoneTranscription object. @notnil
 * @param user_data The user data to set. @maybenil
 **/
LINPHONE_PUBLIC void linphone_transcription_set_user_data(LinphoneTranscription *transcription, void *user_data);

/**
 * Retrieve user pointer.
 * @param transcription #LinphoneTranscription object. @notnil
 * @return the user_data pointer or NULL. @maybenil
 **/
LINPHONE_PUBLIC void *linphone_transcription_get_user_data(const LinphoneTranscription *transcription);

/**
 * Take a reference on a #LinphoneTranscription.
 * @param transcription the #LinphoneTranscription object @notnil
 * @return the same #LinphoneTranscription object @notnil
 */
LINPHONE_PUBLIC LinphoneTranscription *linphone_transcription_ref(LinphoneTranscription *transcription);

/**
 * Add a listener in order to be notified of #LinphoneTranscription events.
 * @param transcription #LinphoneTranscription object to monitor. @notnil
 * @param cbs A #LinphoneTranscriptionCbs object holding the callbacks you need. @notnil
 */
LINPHONE_PUBLIC void linphone_transcription_add_callbacks(LinphoneTranscription *transcription,
                                                          LinphoneTranscriptionCbs *cbs);

/**
 * Remove a listener from a #LinphoneTranscription
 * @param transcription #LinphoneTranscription object @notnil
 * @param cbs #LinphoneTranscriptionCbs object to remove. @notnil
 */
LINPHONE_PUBLIC void linphone_transcription_remove_callbacks(LinphoneTranscription *transcription,
                                                             LinphoneTranscriptionCbs *cbs);

/**
 * Gets the current LinphoneTranscriptionCbs.
 * This is meant only to be called from a callback to be able to get the user_data associated with the
 * #LinphoneTranscriptionCbs that is calling the callback.
 * @param transcription #LinphoneTranscription object @notnil
 * @return The #LinphoneTranscriptionCbs that has called the last callback. @maybenil
 */
LINPHONE_PUBLIC LinphoneTranscriptionCbs *
linphone_transcription_get_current_callbacks(const LinphoneTranscription *transcription);

/**
 * @brief Gets the list of listener in the transcription.
 * @param transcription #LinphoneTranscription object. @notnil
 * @return The list of #LinphoneTranscriptionCbs. @maybenil
 * @donotwrap
 */
LINPHONE_PUBLIC const bctbx_list_t *
linphone_transcription_get_callbacks_list(const LinphoneTranscription *transcription);

/**
 * Turns the transcription ON/OFF. Can be used during a call.
 * @param transcription the #LinphoneTranscription object @notnil
 * @param activate turns the transcription on if true, off if false @notnil
 */
LINPHONE_PUBLIC void linphone_transcription_activate(LinphoneTranscription *transcription, bool_t activate);

/**
 * Get the sentence associated to the given sentence id #LinphoneTranscription.
 * @param transcription the #LinphoneTranscription object @notnil
 * @param sentence_id id associated to a sentence @notnil
 * @return the sentence associated to the given id as a char* #LinphoneTranscription object
 */
LINPHONE_PUBLIC const char *linphone_transcription_get_sentence_by_id(LinphoneTranscription *transcription,
                                                                      uint32_t sentence_id);

/**
 * Get the name of the speaker associated to a sentence id. Works only in a conference.
 * @param transcription the #LinphoneTranscription object @notnil
 * @param sentence_id id associated to a sentence @notnil
 * @return the name of the speaker associated to the given sentence id as a char* #LinphoneTranscription object
 */
LINPHONE_PUBLIC const char *linphone_transcription_get_name_by_id(LinphoneTranscription *transcription,
                                                                  uint32_t sentence_id);
// LINPHONE_PUBLIC uint32_t *linphone_transcription_get_corrected_by_id(LinphoneTranscription *transcription,
//                                                                      uint32_t sentence_id);
// LINPHONE_PUBLIC size_t linphone_transcription_get_corrected_size_by_id(LinphoneTranscription *transcription,
//                                                                        uint32_t sentence_id);

/**
 * Get the id of the last transcribed sentence. Necessary to have access to all the transcription data
 (transcription,
 * associated speaker...).
 * @param transcription the #LinphoneTranscription object @notnil
 * @return id of the last transcribed sentence
 */
LINPHONE_PUBLIC uint32_t linphone_transcription_get_last_sentence_id(LinphoneTranscription *transcription);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // L_C_LINPHONE_TRANSCRIPTION_H_
