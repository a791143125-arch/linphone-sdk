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
// This part of the API is available to C++ only code
#include <optional>
#include <map>
#include <memory>
/**
 * Check if Datachannel is supported
 * @return true if Datachannel is supported
 */
bool ms_datachannel_supported(void);

/**
 * a class to describe the set of datachannel we want to open
 */
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
	std::map<uint16_t, ChannelParams> channels; /**< actual set of channels to be created, mapped by their id */

	MSDataChannelParams(uint16_t local_port, uint16_t remote_port) : sctp_local_port(local_port), sctp_remote_port(remote_port) {};
};

class MSDataChannel {
	public:
		struct Impl;
		MSDataChannel(struct _MSDtlsSrtpContext *dtls_ctx, MSDataChannelParams &&params);
		~MSDataChannel();
		/**
		 * Send a binary message on the datachannel given in id
 		 * @param[in]	id	datachannel id
		 * @param[in]	msg	binary message to be sent
		 * @param[in]	size	size of the binary buffer
		 * @return true on sending successful
		 */
		bool send(uint16_t id, const std::byte *msg, size_t size);

	private:
		std::shared_ptr<Impl> pImpl;
};

// alias the object name to the same typedef used for the C only version
typedef MSDataChannel MSDataChannelHandle;
// forward declaration of opaque structure defined in dtls_srtp.h
struct _MSDtlsSrtpContext;
/**
 * Factory function, Initiate a Sctp connexion with given parameters and open datachannel(s)
 * @param[in] 		dtls_ctx 	the DTLS context to use for SCTP transport
 * @param[in]		params		sctp and datachannels parameters
 * @return 		a pointer to the data channel object created, use the ms_datachannel_context_destroy function to destroy
 */
MSDataChannelHandle *ms_datachannel_create(struct _MSDtlsSrtpContext *dtls_ctx, MSDataChannelParams &&params);

#else  // __cplusplus : This code if for C only: just declare the MSDataChannelHandle so it can be used as an opaque pointer
typedef struct MSDataChannel MSDataChannelHandle;
#endif // __cplusplus


// Shared C/C++ interface
#ifdef __cplusplus
extern "C" {
#endif
MS2_PUBLIC void ms_datachannel_destroy(MSDataChannelHandle *ctx);
#ifdef __cplusplus
}
#endif

#endif /* ms_datachannel_h */
