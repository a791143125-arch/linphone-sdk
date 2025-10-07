/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
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

#include "mswebrtc_noise_suppressor.h"
#include "mediastreamer2/mscommon.h"
#include "noise_suppressor.h"
#include "ns_config.h"
#include <cstdint>
#include <memory>
namespace mswebrtc_noise_suppressor {

// TODO stereo

mswebrtc_noise_suppressor::mswebrtc_noise_suppressor(MSFilter *filter) {

	mFrameSize = kFrameSizeMs * mRateInHz / 1000;
	mNbytes = mFrameSize * sizeof(int16_t);
}

void mswebrtc_noise_suppressor::uninit() {
}

void mswebrtc_noise_suppressor::preprocess() {

	// mNumSamples = rtc::CheckedDivExact(mRateInHz, 100);
	// mNbytes = mNumSamples * sizeof(int16_t);

	// TODO: set 6, 12, 18 or 21 dB?
	mConfig.target_level = webrtc::NsConfig::SuppressionLevel::k18dB;

	mNoiseSuppressorInst = std::make_unique<webrtc::NoiseSuppressor>(mConfig, mRateInHz, mNumChannels);

	if (!mNoiseSuppressorInst) {
		ms_error("[Noise suppressor] cannot be created, check the sample rate. Accepted values are 16000, 32000 or "
		         "48000 Hz.");
		mBypassMode = true;
		ms_error("[Noise suppressor] entering bypass mode");
		return;
	}

	// Initialize audio buffer
	ms_bufferizer_init(&mAudio);

	mCaptureBuffer = std::make_unique<webrtc::AudioBuffer>(mRateInHz, mNumChannels, mRateInHz, mNumChannels, mRateInHz,
	                                                       mNumChannels);
	mStreamConfig = webrtc::StreamConfig(mRateInHz, mNumChannels);

	mNbytes = (int)mFrameSize * sizeof(int16_t);
	printf("*** preprocess with rate %lu Hz, %lu channels, frame size %lu ms - %lu samples ***\n", mRateInHz,
	       mNumChannels, kFrameSizeMs, mFrameSize);
	ms_message("[Noise suppressor] rate %lu Hz, %lu channels, frame size %lu ms - %lu samples", mRateInHz, mNumChannels,
	           kFrameSizeMs, mFrameSize);

	return;
}

void mswebrtc_noise_suppressor::process(MSFilter *filter) {

	if (mBypassMode) {
		mblk_t *im;
		while ((im = ms_queue_get(filter->inputs[0])) != NULL) {
			ms_queue_put(filter->outputs[0], im);
		}
	}

	int16_t *audioData;
	audioData = (int16_t *)alloca(mNbytes);
	ms_bufferizer_put_from_queue(&mAudio, filter->inputs[0]);

	while (ms_bufferizer_read(&mAudio, (uint8_t *)audioData, mNbytes) >= mNbytes) {
		mblk_t *om = allocb(mNbytes, 0);

		// fill audio buffer
		mCaptureBuffer->webrtc::AudioBuffer::CopyFrom(audioData, mStreamConfig);

		if (mRateInHz > webrtc::AudioProcessing::kSampleRate16kHz) {
			mCaptureBuffer->SplitIntoFrequencyBands();
		}

		mNoiseSuppressorInst->Analyze(*mCaptureBuffer.get());
		mNoiseSuppressorInst->Process(mCaptureBuffer.get());

		// get processed capture
		if (mRateInHz > webrtc::AudioProcessing::kSampleRate16kHz) {
			mCaptureBuffer->MergeFrequencyBands();
		}

		mCaptureBuffer->CopyTo(mStreamConfig, (int16_t *)om->b_wptr);
		om->b_wptr += mNbytes;
		ms_queue_put(filter->outputs[0], om);
	}
}

void mswebrtc_noise_suppressor::postprocess() {

	ms_bufferizer_flush(&mAudio);
	if (mCaptureBuffer != nullptr) {
		mCaptureBuffer.reset();
	}
}

int mswebrtc_noise_suppressor::setRate(size_t rateInHz) {
	mRateInHz = static_cast<size_t>(rateInHz);
	mFrameSize = kFrameSizeMs * mRateInHz / 1000;
	ms_message("[Noise suppressor] set rate %lu Hz, framesize is %lu (%lu ms)", mRateInHz, mFrameSize, kFrameSizeMs);
	return 0;
}

int mswebrtc_noise_suppressor::getRate() {
	return static_cast<int>(mRateInHz);
}

int mswebrtc_noise_suppressor::getNbChannels() {
	return static_cast<int>(mNumChannels);
}

} // namespace mswebrtc_noise_suppressor
