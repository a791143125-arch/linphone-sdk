#include "bctoolbox/tester.h"
#include "mediastreamer2/mstranscript.h"
#include "mediastreamer2/whisper-cpp-overlap-transcript.h"
#include "mediastreamer2_tester.h"
#include "ortp/rtpsession.h"
#include <memory>
#include <random>

std::vector<float> convertUint8ToFloat(const uint8_t *buf, size_t size) {
	std::vector<float> out;
	if (size % 2 != 0) return out; // Invalid size, must be even for int16_t

	out.reserve(size / 2); // Each int16_t is 2 bytes

	for (size_t i = 0; i < size; i += 2) {
		int16_t sample = static_cast<int16_t>(buf[i] | (buf[i + 1] << 8)); // little-endian
		float f = sample / 32768.0f;                                       // Normalize to [-1.0, 1.0]
		out.push_back(f);
	}

	return out;
}

class whisperOverlapTester {

public:
	std::unique_ptr<WhisperCPPOverlapTranscript> mInstance = std::make_unique<WhisperCPPOverlapTranscript>(
	    std::string(MODEL_PATH) + "/ggml-" + std::string(MODEL_NAME) + ".bin", 16000);
	size_t chunkSize = int(mInstance->kChunkDuration * mInstance->mSampleRate);
	size_t overlapSize = int(mInstance->kOverlapDuration * mInstance->mSampleRate);

	void set_durations(float chunkDuration, float overlapDuration) {
		mInstance = std::make_unique<WhisperCPPOverlapTranscript>(std::string(MODEL_PATH) + "/ggml-" +
		                                                              std::string(MODEL_NAME) + ".bin",
		                                                          16000, chunkDuration, overlapDuration);
		chunkSize = int(mInstance->kChunkDuration * mInstance->mSampleRate);
		overlapSize = int(mInstance->kOverlapDuration * mInstance->mSampleRate);
	}

	void setEndofAudio() {
		mInstance->mEndOfAudio = true;
	}

	void cleanWord(std::string &word) {
		mInstance->clean_word(word);
	}

	bool_t test_set_model_path() {
		const char *path_const = "tralala";
		char *path = strdup(path_const);
		mInstance->set_model_path(path);
		free(path);
		return mInstance->mModelPath == std::string("tralala");
	}

	void print_instance_state() {
		ms_message("State of the whisper cpp overlap class members :");
		ms_message("mEndOfAudio : %i", mInstance->mEndOfAudio);
		if (!mInstance->mFinalWords.empty())
			ms_message("mFinalWords[0] : %s", mInstance->mFinalWords[0].transcribed_word);
		if (mInstance->mGlobalOffset) ms_message("mGlobalOffset : %f", mInstance->mGlobalOffset);
		ms_message("mLastDiscardedWord : %s", mInstance->mLastDiscardedWord.c_str());
		if (mInstance->mLastTime) ms_message("mLastTime : %f", mInstance->mLastTime);
		ms_message("mLastValidatedWord : %s", mInstance->mLastValidatedWord.c_str());
		if (mInstance->mPreviousEnd) ms_message("mPreviousEnd : %f", mInstance->mPreviousEnd);
		ms_message("mStartOfTranscription : %i", mInstance->mStartOfTranscription);
	}

	std::string select_words(std::vector<MSTranscription> vectorOfWords) {
		std::string sentence = "";
		mInstance->selectWordsToPrint(vectorOfWords);
		std::vector<MSTranscription> vectorOfTranscriptions = mInstance->mVectorTranscription;
		for (MSTranscription word : vectorOfTranscriptions) {
			sentence += word.transcribed_word;
		}
		mInstance->mVectorTranscription.clear();
		mInstance->mGlobalOffset += (mInstance->kChunkDuration - mInstance->kOverlapDuration);
		return sentence;
	}

	std::vector<float> prepare_chunk_and_overlap() {
		return mInstance->prepareChunkAndOverlap();
	}

	std::vector<float> get_overlap() {
		return mInstance->mOverlap;
	}

	/**
	We fill the bufferizer with random data and return a vector containing the generated data for later comparison.
	 */
	std::vector<float> fill_bufferizer(size_t size) {
		std::vector<uint8_t> data(size * 2);

		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> distrib(0, 255);

		for (size_t i = 0; i < size * 2; ++i) {
			data[i] = static_cast<uint8_t>(distrib(gen));
		}

		mblk_t *packet = rtp_create_packet(data.data(), data.size());
		ms_bufferizer_put(mInstance->mBuf, packet);

		return convertUint8ToFloat(data.data(), data.size());
	}

	std::vector<MSTranscription> tokens_into_words(std::vector<Token> tokenList) {
		return mInstance->tokensIntoWords(tokenList);
	}

	void create_bufferizer() {
		mInstance->mBuf = ms_bufferizer_new();
	}

	void destroy_bufferizer() {
		ms_bufferizer_destroy(mInstance->mBuf);
	}
};

// All those tests that does not set a duration are written with the assumption of 3s chunks with 1s of overlap and with
// the time from which we stop accepting words being 1/3 of the lenght of the overlap before the end of the chunk.

// Test no duplication when aligned timestamp
static void test_aligned_timestamps() {

	whisperOverlapTester tester = whisperOverlapTester();
	std::vector<MSTranscription> vectorOfWords;
	std::string endResult = "";
	tester.print_instance_state();

	vectorOfWords.push_back({"1", 1});
	vectorOfWords.push_back({"2", 2.5});
	vectorOfWords.push_back({"3", 3});
	endResult += tester.select_words(vectorOfWords);
	vectorOfWords.clear();
	vectorOfWords.push_back({"2", 2.5});
	vectorOfWords.push_back({"3", 3});
	vectorOfWords.push_back({"4", 3.2});
	endResult += tester.select_words(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "1234", 5);
}

static void test_start_audio() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::string endResult = "";
	std::vector<MSTranscription> vectorOfWords;

	vectorOfWords.push_back({"1", 0});
	vectorOfWords.push_back({"2", 0.1});
	vectorOfWords.push_back({"3", 1});
	endResult += tester.select_words(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "123", 4);
}

// Test misaligned timestamp such as no "2" are within the normal validation range.
static void test_two_same_words_not_validated() {

	whisperOverlapTester tester = whisperOverlapTester();
	std::string endResult = "";
	std::vector<MSTranscription> vectorOfWords;

	vectorOfWords.push_back({"1", 1});
	vectorOfWords.push_back({"2", 2.7});
	endResult += tester.select_words(vectorOfWords);
	vectorOfWords.clear();
	vectorOfWords.push_back({"2", 2.6});
	vectorOfWords.push_back({"3", 3});
	endResult += tester.select_words(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "123", 4);
}

// Test misaligned timestamp such as both "2" are within the normal validation range.
static void test_two_same_words_validated() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::string endResult = "";
	std::vector<MSTranscription> vectorOfWords;

	vectorOfWords.push_back({"1", 1});
	vectorOfWords.push_back({"2", 2.6});
	endResult += tester.select_words(vectorOfWords);
	vectorOfWords.clear();
	vectorOfWords.push_back({"2", 2.7});
	vectorOfWords.push_back({"3", 3});
	endResult += tester.select_words(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "123", 4);
}

// Test end of chunk kept if end of audio.
static void test_end_of_audio() {
	whisperOverlapTester tester = whisperOverlapTester();
	tester.setEndofAudio();
	std::string endResult = "";
	std::vector<MSTranscription> vectorOfWords;
	vectorOfWords.push_back({"1", 1});
	vectorOfWords.push_back({"2", 2.8});
	vectorOfWords.push_back({"3", 3});
	endResult += tester.select_words(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "123", 4);
}

// Verify that we discard the end of the chunk.
static void test_discard_end_of_chunk() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::string endResult = "";
	std::vector<MSTranscription> vectorOfWords;
	vectorOfWords.push_back({"1", 1});
	vectorOfWords.push_back({"2", 2.8});
	vectorOfWords.push_back({"3", 3});
	endResult += tester.select_words(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "1", 2);
}

// Verify that don't validate a word that has a timestamp before the last word validated
static void test_no_smaller_timestamp() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::string endResult = "";
	std::vector<MSTranscription> vectorOfWords;

	vectorOfWords.push_back({"1", 1});
	vectorOfWords.push_back({"2", 2.6});
	endResult += tester.select_words(vectorOfWords);
	vectorOfWords.clear();
	vectorOfWords.push_back({"3", 3});
	vectorOfWords.push_back({"3bis", 3});
	vectorOfWords.push_back({"2bis", 2.5});
	endResult += tester.select_words(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "123", 4);
}

static void test_negative_timestamp() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::string endResult = "";
	std::vector<MSTranscription> vectorOfWords;

	vectorOfWords.push_back({"1", -61});
	vectorOfWords.push_back({"2", 2.6});
	endResult += tester.select_words(vectorOfWords);
	vectorOfWords.clear();
	vectorOfWords.push_back({"2bis", -2});
	vectorOfWords.push_back({"3", 3});
	endResult += tester.select_words(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "23", 3);
}

// high timestamp does not stop word validation
static void test_high_timestamp() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::string endResult = "";
	std::vector<MSTranscription> vectorOfWords;

	vectorOfWords.push_back({"1", 1});
	vectorOfWords.push_back({"2", 2.6});
	endResult += tester.select_words(vectorOfWords);
	vectorOfWords.clear();
	vectorOfWords.push_back({"2bis", 1800});
	vectorOfWords.push_back({"3", 3});
	endResult += tester.select_words(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "123", 4);
}

static void test_empty_first_chunk() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::string endResult = "";
	std::vector<MSTranscription> vectorOfWords;

	endResult += tester.select_words(vectorOfWords); // empty chunk
	vectorOfWords.clear();
	vectorOfWords.push_back({"1", 3});
	vectorOfWords.push_back({"2", 4});
	endResult += tester.select_words(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "12", 3);
}

static void test_empty_middle_chunk() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::string endResult = "";
	std::vector<MSTranscription> vectorOfWords;

	vectorOfWords.push_back({"1", 1});
	vectorOfWords.push_back({"2", 2});
	endResult += tester.select_words(vectorOfWords);
	vectorOfWords.clear();
	endResult += tester.select_words(vectorOfWords); // empty chunk
	vectorOfWords.clear();
	vectorOfWords.push_back({"3", 5});
	vectorOfWords.push_back({"4", 6});
	endResult += tester.select_words(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "1234", 5);
}

static void test_large_overlap() {
	whisperOverlapTester tester = whisperOverlapTester();
	tester.set_durations(3.0f, 2.0f);
	std::string endResult = "";
	std::vector<MSTranscription> vectorOfWords;

	vectorOfWords.push_back({"1", 1});
	vectorOfWords.push_back({"2", 2.3});
	vectorOfWords.push_back({"3", 2.4});
	endResult += tester.select_words(vectorOfWords);
	vectorOfWords.clear();
	vectorOfWords.push_back({"2", 2.3});
	vectorOfWords.push_back({"3", 2.4});
	vectorOfWords.push_back({"4", 3.1});
	vectorOfWords.push_back({"5", 3.3});
	vectorOfWords.push_back({"6", 3.4});
	endResult += tester.select_words(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "12345", 6);
}

static void test_large_chunk() {
	whisperOverlapTester tester = whisperOverlapTester();
	tester.set_durations(10.0f, 1.0f);
	std::string endResult = "";
	std::vector<MSTranscription> vectorOfWords;

	vectorOfWords.push_back({"1", 0.1});
	vectorOfWords.push_back({"2", 2});
	vectorOfWords.push_back({"3", 5});
	vectorOfWords.push_back({"4", 7});
	vectorOfWords.push_back({"5", 9.6});
	vectorOfWords.push_back({"6", 9.7});
	endResult += tester.select_words(vectorOfWords);
	vectorOfWords.clear();
	vectorOfWords.push_back({"5", 9.6});
	vectorOfWords.push_back({"6", 9.7});
	vectorOfWords.push_back({"7", 10});
	vectorOfWords.push_back({"8", 18.3});
	endResult += tester.select_words(vectorOfWords);
	BC_ASSERT_NSTRING_EQUAL(endResult.c_str(), "12345678", 9);
}

static void test_tokens_simple() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::vector<Token> vectorOfTokens;
	std::vector<MSTranscription> vectorOfWords;

	vectorOfTokens.push_back({" foo", 1});
	vectorOfTokens.push_back({"bar", 2});
	vectorOfTokens.push_back({"[end]", 3});
	vectorOfWords = tester.tokens_into_words(vectorOfTokens);
	BC_ASSERT_EQUAL(vectorOfWords.size(), 1, int, "%i");
	if (vectorOfWords.size() == 1) {
		BC_ASSERT_NSTRING_EQUAL(vectorOfWords[0].transcribed_word, "foobar", 8);
		BC_ASSERT_EQUAL(vectorOfWords[0].timestamp, 2, float, "%f");
	}
}

static void test_tokens_two_words() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::vector<Token> vectorOfTokens;
	std::vector<MSTranscription> vectorOfWords;

	vectorOfTokens.push_back({" foo", 1});
	vectorOfTokens.push_back({"bar", 2});
	vectorOfTokens.push_back({" he", 3});
	vectorOfTokens.push_back({"llo", 4});
	vectorOfTokens.push_back({"[end]", 5});
	vectorOfWords = tester.tokens_into_words(vectorOfTokens);
	BC_ASSERT_EQUAL(vectorOfWords.size(), 2, int, "%i");
	if (vectorOfWords.size() == 2) {
		BC_ASSERT_NSTRING_EQUAL(vectorOfWords[0].transcribed_word, "foobar", 8);
		BC_ASSERT_EQUAL(vectorOfWords[0].timestamp, 2, float, "%f");

		BC_ASSERT_NSTRING_EQUAL(vectorOfWords[1].transcribed_word, "hello", 7);
		BC_ASSERT_EQUAL(vectorOfWords[1].timestamp, 4, float, "%f");
	}
}

static void test_tokens_two_special_tokens() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::vector<Token> vectorOfTokens;
	std::vector<MSTranscription> vectorOfWords;

	vectorOfTokens.push_back({" foo", 1});
	vectorOfTokens.push_back({"bar", 2});
	vectorOfTokens.push_back({"[end]", 3});
	vectorOfTokens.push_back({" he", 4});
	vectorOfTokens.push_back({"llo", 5});
	vectorOfTokens.push_back({"[end]", 6});
	vectorOfWords = tester.tokens_into_words(vectorOfTokens);
	BC_ASSERT_EQUAL(vectorOfWords.size(), 2, int, "%i");
	if (vectorOfWords.size() == 2) {
		BC_ASSERT_NSTRING_EQUAL(vectorOfWords[0].transcribed_word, "foobar", 8);
		BC_ASSERT_EQUAL(vectorOfWords[0].timestamp, 2, float, "%f");
		BC_ASSERT_NSTRING_EQUAL(vectorOfWords[1].transcribed_word, "hello", 7);
		BC_ASSERT_EQUAL(vectorOfWords[1].timestamp, 5, float, "%f");
	}
}

static void test_tokens_underscore_special_token() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::vector<Token> vectorOfTokens;
	std::vector<MSTranscription> vectorOfWords;

	vectorOfTokens.push_back({" foo", 1});
	vectorOfTokens.push_back({"bar", 2});
	vectorOfTokens.push_back({"_end", 3});
	vectorOfWords = tester.tokens_into_words(vectorOfTokens);
	BC_ASSERT_EQUAL(vectorOfWords.size(), 1, int, "%i");
	if (vectorOfWords.size() == 1) {
		BC_ASSERT_NSTRING_EQUAL(vectorOfWords[0].transcribed_word, "foobar", 8);
		BC_ASSERT_EQUAL(vectorOfWords[0].timestamp, 2, float, "%f");
	}
}

// test timestamp validated not of punctuation
static void test_tokens_punctuation_no_timestamp() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::vector<Token> vectorOfTokens;
	std::vector<MSTranscription> vectorOfWords;

	vectorOfTokens.push_back({" foo", 1});
	vectorOfTokens.push_back({"bar", 2});
	vectorOfTokens.push_back({".", 3});
	vectorOfTokens.push_back({" foo", 4});
	vectorOfTokens.push_back({"bar", 5});
	vectorOfTokens.push_back({"!", 6});
	vectorOfTokens.push_back({" foo", 7});
	vectorOfTokens.push_back({"bar", 8});
	vectorOfTokens.push_back({"?", 9});
	vectorOfTokens.push_back({"[end]", 10});
	vectorOfWords = tester.tokens_into_words(vectorOfTokens);
	BC_ASSERT_EQUAL(vectorOfWords.size(), 3, int, "%i");
	if (vectorOfWords.size() == 3) {
		BC_ASSERT_NSTRING_EQUAL(vectorOfWords[0].transcribed_word, "foobar.", 9);
		BC_ASSERT_EQUAL(vectorOfWords[0].timestamp, 2, float, "%f");
		BC_ASSERT_NSTRING_EQUAL(vectorOfWords[1].transcribed_word, "foobar!", 9);
		BC_ASSERT_EQUAL(vectorOfWords[1].timestamp, 5, float, "%f");
		BC_ASSERT_NSTRING_EQUAL(vectorOfWords[2].transcribed_word, "foobar?", 9);
		BC_ASSERT_EQUAL(vectorOfWords[2].timestamp, 8, float, "%f");
	}
}

static void test_tokens_two_special_tokens_in_a_row() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::vector<Token> vectorOfTokens;
	std::vector<MSTranscription> vectorOfWords;

	vectorOfTokens.push_back({" foo", 1});
	vectorOfTokens.push_back({"bar", 2});
	vectorOfTokens.push_back({"[end]", 3});
	vectorOfTokens.push_back({"[end]", 4});
	vectorOfTokens.push_back({" he", 5});
	vectorOfTokens.push_back({"llo", 6});
	vectorOfTokens.push_back({"[end]", 7});
	vectorOfWords = tester.tokens_into_words(vectorOfTokens);
	BC_ASSERT_EQUAL(vectorOfWords.size(), 2, int, "%i");
	if (vectorOfWords.size() == 2) {
		BC_ASSERT_NSTRING_EQUAL(vectorOfWords[0].transcribed_word, "foobar", 8);
		BC_ASSERT_EQUAL(vectorOfWords[0].timestamp, 2, float, "%f");
		BC_ASSERT_NSTRING_EQUAL(vectorOfWords[1].transcribed_word, "hello", 7);
		BC_ASSERT_EQUAL(vectorOfWords[1].timestamp, 6, float, "%f");
	}
}

static void test_tokens_long_word() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::vector<Token> vectorOfTokens;
	std::vector<MSTranscription> vectorOfWords;

	vectorOfTokens.push_back({" abc", 1});
	vectorOfTokens.push_back({"def", 2});
	vectorOfTokens.push_back({"ghi", 3});
	vectorOfTokens.push_back({"jkl", 4});
	vectorOfTokens.push_back({"mno", 5});
	vectorOfTokens.push_back({"pqr", 6});
	vectorOfTokens.push_back({"stu", 7});
	vectorOfTokens.push_back({"vwx", 8});
	vectorOfTokens.push_back({"yz", 9});
	vectorOfTokens.push_back({"[end]", 10});
	vectorOfWords = tester.tokens_into_words(vectorOfTokens);
	BC_ASSERT_EQUAL(vectorOfWords.size(), 1, int, "%i");
	if (vectorOfWords.size() == 1) {
		BC_ASSERT_NSTRING_EQUAL(vectorOfWords[0].transcribed_word, "abcdefghijklmnopqrstuvwxyz", 28);
		BC_ASSERT_EQUAL(vectorOfWords[0].timestamp, 9, float, "%f");
	}
}

static void test_tokens_two_short_words() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::vector<Token> vectorOfTokens;
	std::vector<MSTranscription> vectorOfWords;

	vectorOfTokens.push_back({" foo", 1});
	vectorOfTokens.push_back({" bar", 2});
	vectorOfTokens.push_back({"[end]", 3});
	vectorOfWords = tester.tokens_into_words(vectorOfTokens);
	BC_ASSERT_EQUAL(vectorOfWords.size(), 2, int, "%i");
	if (vectorOfWords.size() == 2) {
		BC_ASSERT_NSTRING_EQUAL(vectorOfWords[0].transcribed_word, "foo", 5);
		BC_ASSERT_EQUAL(vectorOfWords[0].timestamp, 1, float, "%f");
		BC_ASSERT_NSTRING_EQUAL(vectorOfWords[1].transcribed_word, "bar", 5);
		BC_ASSERT_EQUAL(vectorOfWords[1].timestamp, 2, float, "%f");
	}
}

static void test_clean_word() {
	whisperOverlapTester tester = whisperOverlapTester();

	std::string word = "HaDe!";
	tester.cleanWord(word);
	BC_ASSERT_NSTRING_EQUAL(word.c_str(), "hade", 5);

	word = "FOOBAR!!";
	tester.cleanWord(word);
	BC_ASSERT_NSTRING_EQUAL(word.c_str(), "foobar", 7);

	word = "FOO..BAR!!";
	tester.cleanWord(word);
	BC_ASSERT_NSTRING_EQUAL(word.c_str(), "foobar", 7);

	word = "FOO-BAR!!";
	tester.cleanWord(word);
	BC_ASSERT_NSTRING_EQUAL(word.c_str(), "foo-bar", 8);
}

static void test_set_model_path() {
	whisperOverlapTester tester = whisperOverlapTester();
	BC_ASSERT_TRUE(tester.test_set_model_path());
}

// test chunk 3s, overlap 1s for the first and second iteration
static void test_buffer_handling_basic() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::vector<float> overlap;
	std::vector<float> output;
	std::vector<float> secondIteration;
	std::vector<float> input;
	tester.create_bufferizer();

	input = tester.fill_bufferizer(tester.chunkSize);
	tester.prepare_chunk_and_overlap();
	overlap = tester.get_overlap();
	output = tester.prepare_chunk_and_overlap();
	BC_ASSERT_TRUE(input == output);
	BC_ASSERT_TRUE(std::equal(overlap.begin(), overlap.end(), input.begin()));
	secondIteration.insert(secondIteration.begin(), input.end() - tester.overlapSize, input.end());
	input = tester.fill_bufferizer(tester.chunkSize - tester.overlapSize);
	secondIteration.insert(secondIteration.end(), input.begin(), input.end());
	output = tester.prepare_chunk_and_overlap();
	overlap = tester.get_overlap();
	BC_ASSERT_TRUE(secondIteration == output);
	BC_ASSERT_TRUE(std::equal(overlap.begin(), overlap.end(), secondIteration.end() - tester.overlapSize));

	tester.destroy_bufferizer();
}

// test chunk 5s, overlap 1s for the first and second iteration
static void test_buffer_handling_long_chunk() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::vector<float> overlap;
	std::vector<float> output;
	std::vector<float> secondIteration;
	std::vector<float> input;
	tester.set_durations(5.0f, 1.0f);

	tester.create_bufferizer();
	input = tester.fill_bufferizer(tester.chunkSize);
	tester.prepare_chunk_and_overlap();
	overlap = tester.get_overlap();
	output = tester.prepare_chunk_and_overlap();
	BC_ASSERT_TRUE(input == output);
	BC_ASSERT_TRUE(std::equal(overlap.begin(), overlap.end(), input.begin()));
	secondIteration.insert(secondIteration.begin(), input.end() - tester.overlapSize, input.end());
	input = tester.fill_bufferizer(tester.chunkSize - tester.overlapSize);
	secondIteration.insert(secondIteration.end(), input.begin(), input.end());
	output = tester.prepare_chunk_and_overlap();
	overlap = tester.get_overlap();
	BC_ASSERT_TRUE(secondIteration == output);
	BC_ASSERT_TRUE(std::equal(overlap.begin(), overlap.end(), secondIteration.end() - tester.overlapSize));

	tester.destroy_bufferizer();
}

// test non integer overlap
static void test_buffer_handling_non_standard_overlap_size() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::vector<float> overlap;
	std::vector<float> output;
	std::vector<float> secondIteration;
	std::vector<float> input;
	tester.set_durations(3.0f, 1.0164f);
	tester.create_bufferizer();

	input = tester.fill_bufferizer(tester.chunkSize);
	tester.prepare_chunk_and_overlap();
	overlap = tester.get_overlap();
	output = tester.prepare_chunk_and_overlap();
	BC_ASSERT_TRUE(input == output);
	BC_ASSERT_TRUE(std::equal(overlap.begin(), overlap.end(), input.begin()));
	secondIteration.insert(secondIteration.begin(), input.end() - tester.overlapSize, input.end());
	input = tester.fill_bufferizer(tester.chunkSize - tester.overlapSize);
	secondIteration.insert(secondIteration.end(), input.begin(), input.end());
	output = tester.prepare_chunk_and_overlap();
	overlap = tester.get_overlap();
	BC_ASSERT_TRUE(secondIteration == output);
	BC_ASSERT_TRUE(std::equal(overlap.begin(), overlap.end(), secondIteration.end() - tester.overlapSize));

	tester.destroy_bufferizer();
}

// test non integer chunk size
static void test_buffer_handling_non_standard_chunk_size() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::vector<float> overlap;
	std::vector<float> output;
	std::vector<float> secondIteration;
	std::vector<float> input;
	tester.set_durations(3.05856f, 1.0f);
	tester.create_bufferizer();

	input = tester.fill_bufferizer(tester.chunkSize);
	tester.prepare_chunk_and_overlap();
	overlap = tester.get_overlap();
	output = tester.prepare_chunk_and_overlap();
	BC_ASSERT_TRUE(input == output);
	BC_ASSERT_TRUE(std::equal(overlap.begin(), overlap.end(), input.begin()));
	secondIteration.insert(secondIteration.begin(), input.end() - tester.overlapSize, input.end());
	input = tester.fill_bufferizer(tester.chunkSize - tester.overlapSize);
	secondIteration.insert(secondIteration.end(), input.begin(), input.end());
	output = tester.prepare_chunk_and_overlap();
	overlap = tester.get_overlap();
	BC_ASSERT_TRUE(secondIteration == output);
	BC_ASSERT_TRUE(std::equal(overlap.begin(), overlap.end(), secondIteration.end() - tester.overlapSize));

	tester.destroy_bufferizer();
}

// test non integer chunk size and overlap
static void test_buffer_handling_non_standard_overlap_and_chunk_size() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::vector<float> overlap;
	std::vector<float> output;
	std::vector<float> secondIteration;
	std::vector<float> input;
	tester.set_durations(3.54468f, 1.4687f);
	tester.create_bufferizer();

	input = tester.fill_bufferizer(tester.chunkSize);
	tester.prepare_chunk_and_overlap();
	overlap = tester.get_overlap();
	output = tester.prepare_chunk_and_overlap();
	BC_ASSERT_TRUE(input == output);
	BC_ASSERT_TRUE(std::equal(overlap.begin(), overlap.end(), input.begin()));
	secondIteration.insert(secondIteration.begin(), input.end() - tester.overlapSize, input.end());
	input = tester.fill_bufferizer(tester.chunkSize - tester.overlapSize);
	secondIteration.insert(secondIteration.end(), input.begin(), input.end());
	output = tester.prepare_chunk_and_overlap();
	overlap = tester.get_overlap();
	BC_ASSERT_TRUE(secondIteration == output);
	BC_ASSERT_TRUE(std::equal(overlap.begin(), overlap.end(), secondIteration.end() - tester.overlapSize));

	tester.destroy_bufferizer();
}

// we test if at the end if the audio, we get the remaining audio in the buffer even if it was not enough to fill an
// entire chunk
static void test_buffer_end_of_audio() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::vector<float> overlap;
	std::vector<float> output;
	std::vector<float> secondIteration;
	std::vector<float> input;
	tester.create_bufferizer();

	input = tester.fill_bufferizer(tester.chunkSize);
	tester.prepare_chunk_and_overlap();
	overlap = tester.get_overlap();
	output = tester.prepare_chunk_and_overlap();
	BC_ASSERT_TRUE(input == output);
	BC_ASSERT_TRUE(std::equal(overlap.begin(), overlap.end(), input.begin()));
	tester.setEndofAudio();
	secondIteration.insert(secondIteration.begin(), input.end() - tester.overlapSize, input.end());
	input = tester.fill_bufferizer(16538);
	secondIteration.insert(secondIteration.end(), input.begin(), input.end());
	output = tester.prepare_chunk_and_overlap();
	BC_ASSERT_TRUE(secondIteration == output);

	tester.destroy_bufferizer();
}

// test chunk 3s, overlap 2.5s for the first and second iteration
static void test_large_overlap_buffer() {
	whisperOverlapTester tester = whisperOverlapTester();
	std::vector<float> overlap;
	std::vector<float> output;
	std::vector<float> secondIteration;
	std::vector<float> input;
	tester.set_durations(3.f, 2.8f);

	tester.create_bufferizer();

	input = tester.fill_bufferizer(tester.chunkSize);
	tester.prepare_chunk_and_overlap();
	overlap = tester.get_overlap();
	output = tester.prepare_chunk_and_overlap();
	BC_ASSERT_TRUE(input == output);
	BC_ASSERT_TRUE(std::equal(overlap.begin(), overlap.end(), input.begin()));
	secondIteration.insert(secondIteration.begin(), input.end() - tester.overlapSize, input.end());
	input = tester.fill_bufferizer(tester.chunkSize - tester.overlapSize);
	secondIteration.insert(secondIteration.end(), input.begin(), input.end());
	output = tester.prepare_chunk_and_overlap();
	overlap = tester.get_overlap();
	BC_ASSERT_TRUE(secondIteration == output);
	BC_ASSERT_TRUE(std::equal(overlap.begin(), overlap.end(), secondIteration.end() - tester.overlapSize));

	tester.destroy_bufferizer();
}

static int tester_before_all(void) {
	return 0;
}

static int tester_after_all(void) {
	return 0;
}

static test_t tests[] = {
    TEST_NO_TAG("aligned timestamps", test_aligned_timestamps),
    TEST_NO_TAG("start audio", test_start_audio),
    TEST_NO_TAG("two same words not validated", test_two_same_words_not_validated),
    TEST_NO_TAG("two same words validated", test_two_same_words_validated),
    TEST_NO_TAG("end of audio", test_end_of_audio),
    TEST_NO_TAG("discard end of chunk", test_discard_end_of_chunk),
    TEST_NO_TAG("no smaller timestamp", test_no_smaller_timestamp),
    TEST_NO_TAG("negative timestamp", test_negative_timestamp),
    TEST_NO_TAG("high timestamp", test_high_timestamp),
    TEST_NO_TAG("empty first chunk", test_empty_first_chunk),
    TEST_NO_TAG("empty middle chunk", test_empty_middle_chunk),
    TEST_NO_TAG("large overlap", test_large_overlap),
    TEST_NO_TAG("large chunk", test_large_chunk),
    TEST_NO_TAG("tokens simple", test_tokens_simple),
    TEST_NO_TAG("tokens two words", test_tokens_two_words),
    TEST_NO_TAG("two special tokens", test_tokens_two_special_tokens),
    TEST_NO_TAG("underscore special token", test_tokens_underscore_special_token),
    TEST_NO_TAG("punctuation no timestamp", test_tokens_punctuation_no_timestamp),
    TEST_NO_TAG("two special tokens in a row", test_tokens_two_special_tokens_in_a_row),
    TEST_NO_TAG("tokens long word", test_tokens_long_word),
    TEST_NO_TAG("wo short words", test_tokens_two_short_words),
    TEST_NO_TAG("basic", test_buffer_handling_basic),
    TEST_NO_TAG("long chunk", test_buffer_handling_long_chunk),
    TEST_NO_TAG("non standard overlap size", test_buffer_handling_non_standard_overlap_size),
    TEST_NO_TAG("non standard chunk size", test_buffer_handling_non_standard_chunk_size),
    TEST_NO_TAG("non standard overlap and chunk size", test_buffer_handling_non_standard_overlap_and_chunk_size),
    TEST_NO_TAG("clean word", test_clean_word),
    TEST_NO_TAG("set model path", test_set_model_path),
    TEST_NO_TAG("audio end buffer", test_buffer_end_of_audio),
    TEST_NO_TAG("large overlap buffer", test_large_overlap_buffer),
};
extern "C" {
test_suite_t whispercpp_overlap_unit_test_suite = {"Unit Whispercpp Overlap",
                                                   tester_before_all,
                                                   tester_after_all,
                                                   NULL,
                                                   NULL,
                                                   sizeof(tests) / sizeof(tests[0]),
                                                   tests,
                                                   0};
}