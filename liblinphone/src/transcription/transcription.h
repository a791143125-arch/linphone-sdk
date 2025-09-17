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

#ifndef _L_TRANSCRIPTION_H_
#define _L_TRANSCRIPTION_H_

#include "bctoolbox/list.h"
#include "belle-sip/object++.hh"
#include "core/core-accessor.h"
#include "linphone/api/c-types.h"

#include "linphone/api/c-callbacks.h"
#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/mstranscript.h"
#include <cstdint>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include <memory>

#include "c-wrapper/c-wrapper.h"
#include "linphone/api/c-types.h"

#include "linphone/utils/general.h"

#include "core/core.h"

LINPHONE_BEGIN_NAMESPACE

class TranscriptionCbs;

struct Word {
	std::string word;
	float start;
	float end;
	bool_t final;
};

struct Sentence {
	std::vector<MSTranscription> words;
	char sentence[500];
	float start;
	float end;
	std::vector<float> confidences;
	std::vector<uint32_t> corrected;
	bool_t finished;
	char name[50];
};

class LINPHONE_PUBLIC Transcription : public bellesip::HybridObject<LinphoneTranscription, Transcription>,
                                      public UserDataAccessor,
                                      public CallbacksHolder<TranscriptionCbs>,
                                      public CoreAccessor {

private:
	void processTranscriptionsForApp();
	bool_t verifyIfCorrection(MSTranscription tr);
	void addWordToSentence(MSTranscription transcription);

public:
	Transcription(const std::shared_ptr<Core> &core);
	Transcription(const Transcription &other);

	Transcription *clone() const override;
	Transcription &operator=(const Transcription &other);

	void addTranscription(MSTranscription transcription);
	Sentence initSentence(MSTranscription transcription);
	const char *getSentenceById(uint32_t sentenceId);
	const char *getNameById(uint32_t sentenceId);
	// std::vector<uint32_t> getCorrectedById(uint32_t sentenceId);
	uint32_t getLastSentenceId();
	void setAudioStream(AudioStream *stream);
	void activate(bool_t activate);
	std::string getNameBySsrc(uint32_t ssrc);

	std::string mTest;
	std::queue<MSTranscription> mTranscriptions;
	AudioStream *mStream;
	LinphoneConference *mConfCtx;
	std::map<uint32_t, Sentence> mSentences;
	uint32_t mLastId;
	uint32_t mMaxLenghtOfSentence;
	float mMaxDurationOfSentence;
	std::string mCurrentName;
	bool_t mModified;
};

class TranscriptionCbs : public bellesip::HybridObject<LinphoneTranscriptionCbs, TranscriptionCbs>, public Callbacks {
public:
	LinphoneTranscriptionCbsDisplayCb getTranscriptionDisplay() const {
		return mTranscriptionDisplayCb;
	}
	void setTranscriptionDisplay(LinphoneTranscriptionCbsDisplayCb cb) {
		mTranscriptionDisplayCb = cb;
	}

private:
	LinphoneTranscriptionCbsDisplayCb mTranscriptionDisplayCb = nullptr;
};

LINPHONE_END_NAMESPACE

#endif /* _L_TRANSCRIPTION_H_ */
