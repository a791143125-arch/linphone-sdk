/**
 * Copyright (c) 2019-2021 Paul-Louis Ageneau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef RTC_IMPL_SCTP_TRANSPORT_H
#define RTC_IMPL_SCTP_TRANSPORT_H

// JOHAN
#define SCTP_DEBUG 1

#include "common.hpp"
#include "message.hpp"
// #include "configuration.hpp"
// #include "global.hpp"
// #include "processor.hpp"
#include "queue.hpp"
// #include "transport.hpp"
#include "../dtls_srtp.hpp"

#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <optional>

// from internal.hpp
const uint16_t DEFAULT_SCTP_PORT = 5000;                  // SCTP port to use by default
const size_t DEFAULT_LOCAL_MAX_MESSAGE_SIZE = 256 * 1024; // Default local max message size
const size_t DEFAULT_MTU = 1280;                          // IPv6 minimum guaranteed MTU
const uint16_t MAX_SCTP_STREAMS_COUNT = 1024;             // Max number of negotiated SCTP streams

#include "usrsctp.h"
struct SctpSettings {
	// For the following settings, not set means optimized default
	std::optional<size_t> recvBufferSize;                // in bytes
	std::optional<size_t> sendBufferSize;                // in bytes
	std::optional<size_t> maxChunksOnQueue;              // in chunks
	std::optional<size_t> initialCongestionWindow;       // in MTUs
	std::optional<size_t> maxBurst;                      // in MTUs
	std::optional<unsigned int> congestionControlModule; // 0: RFC2581, 1: HSTCP, 2: H-TCP, 3: RTCC
	std::optional<std::chrono::milliseconds> delayedSackTime;
	std::optional<std::chrono::milliseconds> minRetransmitTimeout;
	std::optional<std::chrono::milliseconds> maxRetransmitTimeout;
	std::optional<std::chrono::milliseconds> initialRetransmitTimeout;
	std::optional<unsigned int> maxRetransmitAttempts;
	std::optional<std::chrono::milliseconds> heartbeatInterval;
};

namespace rtc::impl {

class SctpTransport final : public std::enable_shared_from_this<SctpTransport> {
public:
	static void Init();
	static void SetSettings(const SctpSettings &s);
	static void Cleanup();

	using amount_callback = std::function<void(uint16_t streamId, size_t amount)>;
	// from transport.hpp
	enum class State { Disconnected, Connecting, Connected, Completed, Failed };
	using state_callback = std::function<void(State state)>;
	State state() const;

	struct Ports {
		uint16_t local = DEFAULT_SCTP_PORT;
		uint16_t remote = DEFAULT_SCTP_PORT;
	};
	struct Configuration {
		// Network MTU
		std::optional<size_t> mtu = std::nullopt;

		// Local maximum message size for Data Channels
		std::optional<size_t> maxMessageSize = std::nullopt;
	};

	SctpTransport(MSDtlsSrtpContext *DtlsContext,
	              const Configuration &config,
	              Ports ports,
	              message_callback recvCallback,
	              amount_callback bufferedAmountCallback,
		      state_callback stateChangeCallback);
	~SctpTransport();

	void onBufferedAmount(amount_callback callback);

	void start();
	void stop();
	bool send(message_ptr message); // false if buffered
	bool flush();
	void closeStream(unsigned int stream);
	void close();

	unsigned int maxStream() const;

	// Stats
	void clearStats();
	size_t bytesSent();
	size_t bytesReceived();
	std::optional<std::chrono::milliseconds> rtt();

	// TODOJ should be private
	void incoming(message_ptr message);

private:
	// from transport.hpp
	void changeState(State state);
	void recv(message_ptr message);
	synchronized_callback<State> mStateChangeCallback;
	synchronized_callback<message_ptr> mRecvCallback;
	std::atomic<State> mState = State::Disconnected;
	// end of from transport.hpp

	// Order seems wrong but these are the actual values
	// See https://datatracker.ietf.org/doc/html/draft-ietf-rtcweb-data-channel-13#section-8
	enum PayloadId : uint32_t {
		PPID_CONTROL = 50,
		PPID_STRING = 51,
		PPID_BINARY_PARTIAL = 52,
		PPID_BINARY = 53,
		PPID_STRING_PARTIAL = 54,
		PPID_STRING_EMPTY = 56,
		PPID_BINARY_EMPTY = 57
	};

	struct sockaddr_conn getSockAddrConn(uint16_t port);

	void connect();
	void shutdown();
	bool outgoing(message_ptr message);

	void doRecv();
	void doFlush();
	void enqueueRecv();
	void enqueueFlush();
	bool trySendQueue();
	bool trySendMessage(message_ptr message);
	void updateBufferedAmount(uint16_t streamId, ptrdiff_t delta);
	void triggerBufferedAmount(uint16_t streamId, size_t amount);
	void sendReset(uint16_t streamId);

	void handleUpcall() noexcept;
	int handleWrite(byte *data, size_t len, uint8_t tos, uint8_t set_df) noexcept;

	void processData(binary &&data, uint16_t streamId, PayloadId ppid);
	void processNotification(const union sctp_notification *notify, size_t len);

	const size_t mMaxMessageSize;
	const Ports mPorts;
	struct socket *mSock;
	std::optional<uint16_t> mNegotiatedStreamsCount;

	// Processor mProcessor;
	std::atomic<int> mPendingRecvCount = 0;
	std::atomic<int> mPendingFlushCount = 0;
	std::mutex mRecvMutex;
	std::recursive_mutex mSendMutex; // buffered amount callback is synchronous
	Queue<message_ptr> mSendQueue;
	bool mSendShutdown = false;
	std::map<uint16_t, size_t> mBufferedAmount;
	amount_callback mBufferedAmountCallback;

	std::mutex mWriteMutex;
	std::condition_variable mWrittenCondition;
	std::atomic<bool> mWritten = false;     // written outside lock
	std::atomic<bool> mWrittenOnce = false; // same

	binary mPartialMessage, mPartialNotification;
	binary mPartialStringData, mPartialBinaryData;
	MSDtlsSrtpContext *mDtlsContext;

	// Stats
	std::atomic<size_t> mBytesSent = 0, mBytesReceived = 0;

	static void UpcallCallback(struct socket *sock, void *arg, int flags);
	static int WriteCallback(void *sctp_ptr, void *data, size_t len, uint8_t tos, uint8_t set_df);
	static void DebugCallback(const char *format, ...);

	class InstancesSet;
	static InstancesSet *Instances;
};
std::ostream& operator<<(std::ostream& os, SctpTransport::State state);

} // namespace rtc::impl

#endif // RTC_IMPL_SCTP_TRANSPORT_H
