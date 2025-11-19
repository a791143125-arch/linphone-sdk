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

#ifndef abstract_transcript_h
#define abstract_transcript_h

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msasync.h"
#include <map>
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/mstranscript.h>
#include <vector>

typedef struct MSTranscript {
	void *transcriptionObj;
	std::string modelPath;
	enum transcript_method transcriptionSolution = WHISPER_CPP_OVERLAP;
	MSWorkerThread *wth = NULL;
	MSBufferizer *buf;
	bool_t enable = true;
	int fileDuration = -1;
	float overlap_duration = 1.0f; // For whisper.cpp
	float chunk_duration = 3.0f;   // For whisper.cpp
	AudioStream *audio_stream = NULL;
	uint32_t sizeOfDataSinceBeg = 0;
	uint32_t currentSsrc = 0;
	std::map<float, uint32_t> ssrc_map;
} MSTranscript;

/** Abstract parent class of the different classes implementing transcription */
class AbstractTranscript {
public:
	virtual int init(MSFilter *f) = 0;

	virtual std::vector<MSTranscription> process(MSFilter *f) = 0;

	virtual std::vector<MSTranscription> postProcess(MSFilter *f) = 0;

	virtual void uninit(MSFilter *f) = 0;

	AbstractTranscript();

	/**
	 * @brief Constructor of AbsctractTranscript.
	 * @param modelPath Path to the transcription model.
	 * @param samplerate Samplerate used for the model.
	 */
	AbstractTranscript(std::string modelPath, int samplerate);

	/**
	 * @brief Set the path to the transcription model.
	 * @param path
	 */
	void set_model_path(char *path);

	/**
	 * @brief Get the path to the model stored in the object
	 * @return char* containing the path to the model currently stored in the object
	 */
	const char *get_model_path();

	/**
	 * @brief Set the duration of the file in miliseconds (default value at -1)
	 * @param fileDuration Duration of the file in miliseconds
	 */
	void setFileDuration(int fileDuration);

	virtual ~AbstractTranscript();

protected:
	/**
	 * @brief Read "readSize" audio stored in a MSBufferizer and format it to be exploitable by the transcription model.
	 * @param buffer Buffer from which we read the data (data will be removed from the buffer).
	 * @param readSize Size of the data we want to read from the buffer.
	 * @return Return a vector of float containing formated audio data.
	 * */
	std::vector<float> readAndFormatAudioBufferForTranscription(MSBufferizer *buffer, size_t readSize);

	/**
	 * @brief Make the word lowercase and remove punctuation.
	 * @param word Word we want to clean.
	 * */
	void clean_word(std::string &word);

	std::string mModelPath; /** Path to the transcription model. */
	int mSampleRate;        /** Samplerate used for the transcribed audio.*/
	/** Vector containing the last transcribed words. Sent via the events*/
	std::vector<MSTranscription> mVectorTranscription;
	int mFileDurationMs = -1; /** Duration of the file played (-1 if not set) */
};

#endif