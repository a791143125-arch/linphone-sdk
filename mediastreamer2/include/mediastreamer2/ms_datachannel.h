/*
 * Copyright (c) 2025 Belledonne Communications SARL.
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

#ifndef ms_datachannel_h
#define ms_datachannel_h

#include "mediastreamer2/mscommon.h"

#ifdef __cplusplus
extern "C" {
#endif
/* defined in mediastream.h */
struct _MSMediaStreamSessions;
/* an opaque structure containing all context data needed by datachannel */
typedef struct _MSDataChannelContext MSDataChannelContext;

MS2_PUBLIC MSDataChannelContext *ms_datachannel_context_new(struct _MSMediaStreamSessions *sessions);
MS2_PUBLIC void ms_datachannel_context_start(struct _MSMediaStreamSessions *sessions);
MS2_PUBLIC void ms_datachannel_context_destroy(MSDataChannelContext *ctx);
#ifdef __cplusplus
}
/**
 * Check if Datachannel is supported
 * @return true if Datachannel is supported
 */
bool ms_datachannel_supported(void);

struct MSDataChannelParams {
	uint16_t sctp_local_port;
	uint16_t sctp_remote_port;
};
/**
 * Initiate a Sctp connexion with given parameters and open datachannel(s)
 * @param[in/out] 	sessions 	set of sessions associated to a stream, get the DTLS context from it and set the datachannel context in it
 * @param[in]		params		sctp and datachannels parameters
 */
void ms_datachannel_create(struct _MSMediaStreamSessions *sessions, MSDataChannelParams &&params);

#endif // __cplusplus
#endif /* ms_datachannel_h */
