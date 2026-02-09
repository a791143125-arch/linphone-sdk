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

MS2_PUBLIC void ms_datachannel_context_destroy(MSDataChannelContext *ctx);
#ifdef __cplusplus
}
#include <optional>
#include <map>
/**
 * Check if Datachannel is supported
 * @return true if Datachannel is supported
 */
bool ms_datachannel_supported(void);

struct MSDataChannelParams {
	struct ChannelParams {
		std::string protocol;
		std::string label;
		std::optional<uint32_t> max_retr;
		std::optional<uint32_t> max_time;
		bool ordered;
		ChannelParams(const std::string protocol, const std::string label, std::optional<uint32_t> max_retr, std::optional<uint32_t> max_time, bool ordered) : protocol(protocol), label(label), max_retr(max_retr), max_time(max_time), ordered(ordered) {}; 
	};
	uint16_t sctp_local_port;
	uint16_t sctp_remote_port;
	std::map<uint16_t, ChannelParams> channels;

	MSDataChannelParams(uint16_t local_port, uint16_t remote_port) : sctp_local_port(local_port), sctp_remote_port(remote_port) {};
};
/**
 * Initiate a Sctp connexion with given parameters and open datachannel(s)
 * @param[in/out] 	sessions 	set of sessions associated to a stream, get the DTLS context from it and set the datachannel context in it
 * @param[in]		params		sctp and datachannels parameters
 */
void ms_datachannel_create(struct _MSMediaStreamSessions *sessions, MSDataChannelParams &&params);

/**
 * Send a binary message on the datachannel given in id
 * @param[in]	ctx	the opaque datachannel context
 * @param[in]	id	datachannel id
 * @param[in]	msg	binary message to be sent
 * @param[in]	size	size of the binary buffer
 * @return true on sending successful
 */
bool ms_datachannel_send(MSDataChannelContext *ctx, uint16_t id, const std::byte *msg, size_t size);
#endif // __cplusplus
#endif /* ms_datachannel_h */
