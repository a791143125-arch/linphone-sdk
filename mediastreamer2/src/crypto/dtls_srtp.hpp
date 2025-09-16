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

#ifndef ms_dtls_srtp_hpp
#define ms_dtls_srtp_hpp
#include "mediastreamer2/dtls_srtp.h"
#include <functional>
/** internal interface for DTLS-SRTP operation as transport for SCTP **/

/**
 * use to send data though an opened DTLS connection
 *
 * @param[in/out]	context		the DTLS-SRTP context
 * @param[in]		message		data to be sent
 * @param[in]		msg_len		length of the data to be sent
 *
 * @return  false if the message was not sent (handshake not completed or other error)
 */
bool ms_dtls_srtp_send(MSDtlsSrtpContext *context, const unsigned char *message, size_t msg_len);

using MsDtlsRecvCallback = std::function<void(unsigned char*, size_t)>;
/**
 * set the callback used to pass data received after the callback is complete
 *
 * @param[in/out]	context		the DTLS-SRTP context
 * @param[in]		recv_cb		function called when a message is received
 *
 * @return  false when the context is null
 */
bool ms_dtls_srtp_set_recv_cb(MSDtlsSrtpContext *context, MsDtlsRecvCallback recv_cb);
#endif // ms_dtls_srtp_hpp
