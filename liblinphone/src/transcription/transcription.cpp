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
#include <ostream>
#include <string>
#include <vector>

using namespace LinphonePrivate;

// TEMPORARY, I want to move the definition of this callback in transcription.cpp and link it to the audiostream
// memeber of the class
static void
segment_transcribed_cb(void *data, BCTBX_UNUSED(MSFilter *f), BCTBX_UNUSED(unsigned int event_id), void *event) {
	MSTranscriptEvent *receivedTranscription = NULL;
	receivedTranscription = (MSTranscriptEvent *)event;
	// MSTranscription transcription;
	// transcription.timestamp = receivedTranscription->transcription.timestamp;
	// strncpy(transcription.transcribed_word, receivedTranscription->transcription.transcribed_word,
	//         sizeof(transcription.transcribed_word));
	// transcription.is_final = receivedTranscription->transcription.is_final;
	// transcription.end_of_sentence = receivedTranscription->transcription.end_of_sentence;
	// transcription.ssrc = receivedTranscription->transcription.ssrc;
	// transcription.correction = receivedTranscription->transcription.correction;
	// transcription.beggining = receivedTranscription->transcription.beggining;
	LinphoneTranscription *ptr = static_cast<LinphoneTranscription *>(data);
	linphone_transcription_add(ptr, receivedTranscription->transcription);
}

Transcription::Transcription(std::shared_ptr<Core> core) : CoreAccessor(core) {
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
	return new Transcription(*this);
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
	// mCb();
	processTranscriptionsForApp();
	if (mModified) {
		mCbDisplay(this->toC());
		mModified = false;
		// std::cout << "NB of Corrections for cuurrent sentence : " << mSentences[mLastId].corrected.size() <<
		// std::endl;
	}
}

void Transcription::addTranscriptionCb(LinphoneTranscriptionCb cb) {
	mCb = cb;
}

void Transcription::setDisplayTranscriptionCb(LinphoneTranscriptionDisplayCb cb) {
	mCbDisplay = cb;
}

void Transcription::setAudioStream(AudioStream *stream) {
	mStream = stream;
	audio_stream_set_transcription_callback(stream, segment_transcribed_cb, this->toC());
}

void Transcription::activate(bool_t activate) {
	if (!mStream) {
		ms_warning("The audiostream is not yet initialized, use the transcription activate method during a call.");
		return;
	}
	if (mStream->transcript) {
		ms_filter_call_method(mStream->transcript, MS_TRANSCRIPT_ENABLE, &activate);
	} else {
		ms_warning("No transcription filter in the audio stream, cannot activate/desactivate transcription.");
	}
}

Sentence Transcription::initSentence(MSTranscription transcription) {
	Sentence sentence;
	sentence.sentence[0] = '\0';
	strcat(sentence.sentence, transcription.transcribed_word);
	if (!transcription.end_of_sentence) strcat(sentence.sentence, " ");
	sentence.confidences.push_back(transcription.confidence);
	sentence.start = transcription.beggining;
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
		std::cout << "LAST ID : " << mLastId << std::endl;
		mLastId++;
		Sentence sentence = Transcription::initSentence(transcription);
		mSentences[mLastId] = sentence;
		// printf("IS FINISHED : %i\n", mSentences[mLastId].finished);
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

bool_t Transcription::verifyIfCorrection(MSTranscription tr) {
	bool_t modified = false;
	float startCurrent = tr.beggining;
	float endCurrent = tr.timestamp;
	for (std::pair<const unsigned int, LinphonePrivate::Sentence> &sentence : mSentences) {
		for (uint32_t i = 0; i < sentence.second.words.size(); i++) {
			MSTranscription transcription = sentence.second.words[i];
			float startOld = transcription.beggining;
			float endOld = transcription.timestamp;
			// std::cout << "start current : " << startCurrent << "   end curret : " << endCurrent
			//           << "  start old : " << startOld << "  end old : " << endOld << std::endl;
			if (startCurrent <= endOld && endCurrent >= startOld) { // ATTENTION AU CAS OU IL Y A QUE TIMESTAMP de fin
				if ((std::min(endCurrent, endOld) - std::max(startCurrent, startOld)) / (endOld - startOld) >= 0.8) {
					if (std::string(transcription.transcribed_word) != std::string(tr.transcribed_word)) {
						std::cout << "HEEEEY" << std::endl;
						std::cout << "intervalle : "
						          << (std::min(endCurrent, endOld) - std::max(startCurrent, startOld)) /
						                 (endOld - startOld)
						          << std::endl;
						std::cout << "OLD : " << transcription.transcribed_word << "CORRECTED : " << tr.transcribed_word
						          << std::endl;
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
			if (verifyIfCorrection(tr)) mModified = true;
		}
	}
}

void Transcription::setConf(LinphoneConference *conf) {
	mConfCtx = conf;
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
	// return mSentences[sentenceId].sentence;
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

// Transcription *Transcription::linphone_call_get_transcription(LinphoneCall *call) {
// 	return
// }