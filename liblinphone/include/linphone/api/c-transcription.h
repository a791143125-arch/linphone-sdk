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
#ifndef LINPHONE_TRANSCRIPTION_API_CONFIG_H
#define LINPHONE_TRANSCRIPTION_API_CONFIG_H

#include "linphone/api/c-callbacks.h"
#include "linphone/api/c-types.h"
#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/mstranscript.h"

#ifdef __cplusplus
extern "C" {
#endif

LINPHONE_PUBLIC LinphoneTranscription *linphone_transcription_new(LinphoneCore *lc);

/**
 * Instantiate a new Transcription parameters with values from source.
 * @param transcription The #LinphoneTranscription object to be cloned. @notnil
 * @return The newly created #LinphoneTranscription object. @notnil
 */
LINPHONE_PUBLIC LinphoneTranscription *linphone_transcription_clone(const LinphoneTranscription *transcription);

// /**
//  * Checks if two Transcriptions are identical
//  * @param transcription The #LinphoneTranscription object to be compared. @notnil
//  * @param other_transcription The #LinphoneTranscription object to compare to. @notnil
//  * @return True only if the two Transcriptions are identical. @notnil
//  */
// LINPHONE_PUBLIC bool_t linphone_transcription_is_equal(const LinphoneTranscription *transcription,
//                                                        const LinphoneTranscription *other_transcription);

/**
 * Release a #LinphoneTranscription.
 * @param transcription the #LinphoneTranscription object @notnil
 */
LINPHONE_PUBLIC void linphone_transcription_unref(LinphoneTranscription *transcription);

/**
 * Take a reference on a #LinphoneTranscription.
 * @param transcription the #LinphoneTranscription object @notnil
 * @return the same #LinphoneTranscription object @notnil
 */
LINPHONE_PUBLIC LinphoneTranscription *linphone_transcription_ref(LinphoneTranscription *transcription);

/**
 * Add a MSTranscription to be processed in #LinphoneTranscription.
 * @param transcription the #LinphoneTranscription object @notnil
 * @param tr the trasncription from mediastreamer2 @notnil
 */
LINPHONE_PUBLIC void linphone_transcription_add(LinphoneTranscription *transcription, MSTranscription tr);
LINPHONE_PUBLIC void linphone_transcription_add_cb(LinphoneTranscription *transcription,
                                                   LinphoneTranscriptionCb cb); // TO REMOVE

/**
 * Add a display callback to be called at each actualisation of the transcription by #LinphoneTranscription.
 * @param transcription the #LinphoneTranscription object @notnil
 * @param cb the display callback @notnil
 */
LINPHONE_PUBLIC void linphone_transcription_set_display_cb(LinphoneTranscription *transcription,
                                                           LinphoneTranscriptionDisplayCb cb);

/**
 * Sets a pointer to an audiostream in #LinphoneTranscription.
 * @param transcription the #LinphoneTranscription object @notnil
 * @param stream pointer to the audiostream @notnil
 */
LINPHONE_PUBLIC void linphone_transcription_set_audiostream(LinphoneTranscription *transcription, AudioStream *stream);

/**
 * Turns the transcription ON/OFF. Can be used during a call.
 * @param transcription the #LinphoneTranscription object @notnil
 * @param activate turns the transcription on if true, off if false @notnil
 */
LINPHONE_PUBLIC void linphone_transcription_activate(LinphoneTranscription *transcription, bool_t activate);

/**
 * Sets a pointer to a conference in #LinphoneTranscription.
 * @param transcription the #LinphoneTranscription object @notnil
 * @param conference pointer to the conference @notnil
 */
LINPHONE_PUBLIC void linphone_transcription_set_conference(LinphoneTranscription *transcription,
                                                           LinphoneConference *conference);

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
 * Get the id of the last transcribed sentence. Necessary to have access to all the transcription data (transcription,
 * associated speaker...).
 * @param transcription the #LinphoneTranscription object @notnil
 * @return id of the last transcribed sentence
 */
LINPHONE_PUBLIC uint32_t linphone_transcription_get_last_sentence_id(LinphoneTranscription *transcription);

#ifdef __cplusplus
}
#endif

#endif /* LINPHONE_TRANSCRIPTION_API_CONFIG_H */