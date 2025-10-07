#ifndef MSWEBRTC_NOISE_SUPPRESSOR_H_
#define MSWEBRTC_NOISE_SUPPRESSOR_H_

#include <cstddef>
#include <memory>

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msqueue.h"

#include "api/audio/audio_processing.h"
#include "modules/audio_processing/ns/noise_suppressor.h"
#include "ns_config.h"

// #include "modules/audio_processing/aec3/echo_canceller3.h"
// #include "modules/audio_processing/high_pass_filter.h"

namespace mswebrtc_noise_suppressor {

/** @class mswebrtc_noise_suppressor
 * @brief Class to apply the noise suppressor filter of WebRTC to clean the audio.
 */
class mswebrtc_noise_suppressor {
private:
	// void configureFlowControlledBufferizer();

	std::unique_ptr<webrtc::NoiseSuppressor> mNoiseSuppressorInst;
	webrtc::NsConfig mConfig;
	size_t mNumChannels = 1;
	bool_t mBypassMode = false;
	std::unique_ptr<webrtc::AudioBuffer> mCaptureBuffer;
	webrtc::StreamConfig mStreamConfig;
	size_t mRateInHz = 16000;
	size_t mFrameSize = 160;
	const size_t kFrameSizeMs = 10;
	size_t mNbytes;
	MSBufferizer mAudio;

public:
	mswebrtc_noise_suppressor(MSFilter *filter);
	~mswebrtc_noise_suppressor(){};
	void uninit();
	void preprocess();
	void process(MSFilter *filter);
	void postprocess();
	int setRate(size_t rateInHz);
	int getRate();
	int getNbChannels();

	// TODO missing methods

	// int setSampleRate(int requestedRateInHz);
	// int getSampleRate() {
	// 	return mrateInHz;
	// }
	// void setDelay(int requestedDelayInMs) {
	// 	mDelayInMs = requestedDelayInMs;
	// }
	// int getDelay() {
	// 	return mDelayInMs;
	// }
	// int getErl() {
	// 	return mEchoReturnLoss;
	// }
	// int getErle() {
	// 	return mEchoReturnLossEnhancement;
	// }
	// void setBypassMode(bool bypass) {
	// 	mBypassMode = bypass;
	// 	ms_message("set EC bypass mode to [%i]", mBypassMode);
	// }
	// bool getBypassMode() {
	// 	return mBypassMode;
	// }
	// void setState(char *state) {
	// 	mStateStr = state;
	// }
	// char *getState() {
	// 	return mStateStr;
	// }
};

} // namespace mswebrtc_noise_suppressor

#endif // MSWEBRTC_NOISE_SUPPRESSOR_H_
