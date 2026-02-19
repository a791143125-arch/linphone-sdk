/**
 * Copyright (c) 2019 Paul-Louis Ageneau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef RTC_IMPL_DATA_CHANNEL_H
#define RTC_IMPL_DATA_CHANNEL_H

#include "channel.hpp"
#include "common.hpp"
#include "message.hpp"
#include "queue.hpp"
#include "reliability.hpp"
#include "sctptransport.hpp"

#include <atomic>
#include <shared_mutex>

#include "mediastreamer2/ms_datachannel.h"
namespace rtc::impl {

struct DataChannel : Channel, std::enable_shared_from_this<DataChannel> {
	DataChannel(weak_ptr<MSDataChannel::Impl> pc, uint16_t id, string label, string protocol, Reliability reliability);
	~DataChannel();

	void open();
	void close();
	void remoteClose();
	bool outgoing(message_ptr message);
	void incoming(message_ptr message);

	optional<Message> receive() override;
	optional<message_variant> peek() override;
	size_t availableAmount() const override;

	uint16_t id() const;
	string label() const;
	string protocol() const;
	Reliability reliability() const;

//	bool isOpen(void) const;
//	bool isClosed(void) const;
	size_t maxMessageSize() const;

protected:
	const weak_ptr<MSDataChannel::Impl> mPeerConnection;

	uint16_t mId;
	string mLabel;
	string mProtocol;
	shared_ptr<Reliability> mReliability;

	bool mIsOpen = false;
	bool mIsClosed = false;

private:
	Queue<message_ptr> mRecvQueue;
};

} // namespace rtc::impl

#endif
