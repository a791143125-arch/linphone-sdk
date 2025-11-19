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

#include "transcription.h"
#include "bctoolbox/list.h"
#include "bctoolbox/logging.h"
#include "conference/conference.h"
#include "conference/participant-device.h"
#include "core/core-accessor.h"
#include "linphone/api/c-callbacks.h"
#include "linphone/api/c-conference.h"
#include "linphone/api/c-participant-device.h"
#include "linphone/api/c-types.h"
#include "linphone/core.h"
#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/mstranscript.h"
#include "xml/patch-ops.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

using namespace LinphonePrivate;

static void
segment_transcribed_cb(void *data, BCTBX_UNUSED(MSFilter *f), BCTBX_UNUSED(unsigned int event_id), void *event) {
	MSTranscriptEvent *receivedTranscription = NULL;
	receivedTranscription = (MSTranscriptEvent *)event;
	LinphoneTranscription *ptr = static_cast<LinphoneTranscription *>(data);
	Transcription::toCpp(ptr)->addTranscription(receivedTranscription->transcription);
}

Transcription::Transcription(const std::shared_ptr<Core> &core) : CoreAccessor(core) {
	mTest = "hello word";
	mConfCtx = nullptr;
	mMaxLenghtOfSentence = 100;
	mMaxDurationOfSentence = 20;
	mLastId = 0;
	mModified = false;
}

Transcription::Transcription(const Transcription &other) : HybridObject(other), CoreAccessor(other.getCore()) {
	mTranscriptions = other.mTranscriptions;
	// TO UPDATE
}

Transcription *Transcription::clone() const {
	return nullptr;
}

Transcription &Transcription::operator=(const Transcription &other) {
	if (this != &other) {
		mTranscriptions = other.mTranscriptions;
		// TO UPDATE
	}
	return *this;
}

void Transcription::addTranscription(MSTranscription transcription) {
	mTranscriptions.push(transcription);
	processTranscriptionsForApp();
	if (mModified) {
		_linphone_transcription_notify_result_to_display_available(this->toC());
		mModified = false;
	}
}

void Transcription::setAudioStream(AudioStream *stream) {
	mStream = stream;
	audio_stream_set_transcription_callback(stream, segment_transcribed_cb, this->toC());
}

void Transcription::start() {
	if (!mStream) {
		ms_warning("The audiostream is not yet initialized.");
		return;
	}
	if (mStream->transcript) {
		ms_filter_call_method_noarg(mStream->transcript, MS_TRANSCRIPT_START);
	} else {
		ms_warning("No transcription filter in the audio stream, cannot start transcription.");
	}
}

// TODO set the sentence id to the next value in case of pause, or flush the previous transcription
void Transcription::pause() {
	if (!mStream) {
		ms_warning("The audiostream is not yet initialized.");
		return;
	}
	if (mStream->transcript) {
		ms_filter_call_method_noarg(mStream->transcript, MS_TRANSCRIPT_PAUSE);
	} else {
		ms_warning("No transcription filter in the audio stream, cannot pause transcription.");
	}
}

Sentence Transcription::initSentence(MSTranscription transcription) {
	Sentence sentence;
	sentence.sentence[0] = '\0';
	strcat(sentence.sentence, transcription.transcribed_word);
	if (!transcription.end_of_sentence) strcat(sentence.sentence, " ");
	sentence.confidences.push_back(transcription.confidence);
	sentence.start = transcription.begining;
	sentence.end = transcription.timestamp;
	sentence.finished = transcription.end_of_sentence;
	sentence.name[0] = '\0';
	strcat(sentence.name, mCurrentName.c_str());
	sentence.words.push_back(transcription);

	return sentence;
}

void Transcription::addWordToSentence(MSTranscription transcription) {
	if (!getNameBySsrc(transcription.ssrc).empty()) mCurrentName = getNameBySsrc(transcription.ssrc);
	if (mSentences.empty()) {
		mLastId++;
		Sentence sentence = Transcription::initSentence(transcription);
		mSentences[mLastId] = sentence;
	} else if (mSentences[mLastId].finished ||
	           strlen(mSentences[mLastId].sentence) + strlen(transcription.transcribed_word) + 1 >
	               mMaxLenghtOfSentence ||
	           mCurrentName != mSentences[mLastId].name) {
		mLastId++;
		Sentence sentence = Transcription::initSentence(transcription);
		mSentences[mLastId] = sentence;
	} else {
		size_t spaceLeft = 500 - strlen(mSentences[mLastId].sentence) - 2;
		strncat(mSentences[mLastId].sentence, transcription.transcribed_word, spaceLeft);
		if (!transcription.end_of_sentence) strcat(mSentences[mLastId].sentence, " ");
		mSentences[mLastId].end = transcription.timestamp;
		mSentences[mLastId].finished = transcription.end_of_sentence;
		mSentences[mLastId].confidences.push_back(transcription.confidence);
		mSentences[mLastId].words.push_back(transcription);
	}
}

bool_t Transcription::testIfCorrection(MSTranscription tr) {
	bool_t modified = false;
	float startCurrent = tr.begining;
	float endCurrent = tr.timestamp;
	for (std::pair<const unsigned int, LinphonePrivate::Sentence> &sentence : mSentences) {
		for (uint32_t i = 0; i < sentence.second.words.size(); i++) {
			MSTranscription transcription = sentence.second.words[i];
			float startOld = transcription.begining;
			float endOld = transcription.timestamp;
			if (startCurrent <= endOld && endCurrent >= startOld) { // ATTENTION AU CAS OU IL Y A QUE TIMESTAMP de fin
				if ((std::min(endCurrent, endOld) - std::max(startCurrent, startOld)) / (endOld - startOld) >= 0.8) {
					if (std::string(transcription.transcribed_word) != std::string(tr.transcribed_word)) {
						sentence.second.words[i] = tr;
						sentence.second.corrected.push_back(i);
						modified = true;
					}
				}
			}
		}
	}
	return modified;
}

void Transcription::processTranscriptionsForApp() {
	while (!mTranscriptions.empty()) {
		MSTranscription tr = mTranscriptions.front();
		mTranscriptions.pop();
		if (!tr.correction) {
			addWordToSentence(tr);
			mModified = true;
		} else {
			if (testIfCorrection(tr)) mModified = true;
		}
	}
}

const char *Transcription::getSentenceById(uint32_t sentenceId) {
	std::string sentence;
	for (MSTranscription word : mSentences[sentenceId].words) {
		sentence += word.transcribed_word;
		sentence += ' ';
	}
	mSentences[sentenceId].sentence[0] = '\0';
	strncat(mSentences[sentenceId].sentence, sentence.c_str(), mMaxLenghtOfSentence);
	return mSentences[sentenceId].sentence;
}

const char *Transcription::getNameById(uint32_t sentenceId) {
	return mSentences[sentenceId].name;
}

// std::vector<uint32_t> Transcription::getCorrectedById(uint32_t sentenceId) {
// 	return mSentences[sentenceId].corrected;
// }

uint32_t Transcription::getLastSentenceId() {
	return mLastId;
}

std::string Transcription::getNameBySsrc(uint32_t ssrc) {
	std::string name;
	mConfCtx = linphone_call_get_conference(linphone_core_get_current_call(getCore()->getCCore()));
	std::shared_ptr<ParticipantDevice> device;
	if (!mConfCtx) return name;
	std::list<std::shared_ptr<ParticipantDevice>> listOfDevices =
	    Conference::toCpp(mConfCtx)->getParticipantDevices(false);
	Conference::toCpp(mConfCtx)->findParticipantDeviceBySsrc(ssrc, LinphoneStreamTypeAudio);
	for (auto deviceFromList : listOfDevices) {
		if (deviceFromList->getSsrc(LinphoneStreamTypeAudio) == ssrc) {
			device = deviceFromList;
		}
	}
	if (!device) return name;
	if (!device->getAddress()->getDisplayName().empty()) {
		name = device->getAddress()->getDisplayName();
	} else {
		name = device->getAddress()->getUsername();
	}

	return name;
}
