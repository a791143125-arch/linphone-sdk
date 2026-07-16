#include "array"
#include "cstring"
#include "mediastreamer2/msbackgroundformater.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "onnxruntime/onnxruntime_cxx_api.h"
#include "string"
#include <algorithm>
#include <atomic>
#include <bctoolbox/defs.h>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#ifdef HAVE_LIBYUV_H
#include "libyuv/convert_argb.h"
#include "libyuv/planar_functions.h"
#include "libyuv/scale.h"
#endif
#include <chrono>
namespace mediastreamer {

class BackgroundReplacer {
public:

	BackgroundReplacer(MSFilter *f, bool bypass = true) : mFilter(f) {
    	mBypass = bypass;
    	if (!mBypass) ensureModelLoaded();
	}

	void setBypass(int v) {
		bool b = (v != 0);
		if (!b) ensureModelLoaded();
		mBypass = b;
	}

void ensureModelLoaded() {
		if (mModelLoaded) return;
		mModelLoaded = true;
		char *path =
		    ms_strdup_printf("%s/../background_model/model.onnx", ms_factory_get_image_resources_dir(mFilter->factory));
		try {
			std::filesystem::path modelPath(path);
			Ort::SessionOptions opts;
			opts.SetInterOpNumThreads(1);
			#ifdef __ANDROID__
			opts.SetIntraOpNumThreads(1);
			opts.AddConfigEntry("session.intra_op.allow_spinning", "0");
			opts.AppendExecutionProvider("XNNPACK", {{"intra_op_num_threads", "2"}});
			#else
			opts.SetIntraOpNumThreads(2);
			#endif
			mSession = Ort::Session(mEnv, modelPath.c_str(), opts);
			mTimeLastResult = std::chrono::high_resolution_clock::now();
			Ort::AllocatorWithDefaultOptions alloc;
			mInName = mSession.GetInputNameAllocated(0, alloc).get();
			mOutName = mSession.GetOutputNameAllocated(0, alloc).get();
			mWorker = std::thread(&BackgroundReplacer::inference_computer, this);
		} catch (const std::exception &e) {
			ms_error("[BackgroundReplacer] init ORT KO: %s -> bypass", e.what());
			mBypass = true;
		}
		ms_free(path);
	}

	~BackgroundReplacer() {
		{
			std::lock_guard<std::mutex> lk(mMutex);
			mRunning = false;
		}
		mCv.notify_one();
		if (mWorker.joinable()) mWorker.join();
		if (mBgFrame) {
			freemsg(mBgFrame);
			mBgFrame = nullptr;
		}
	}

	void process(MSFilter *f) {
		mblk_t *m;

		if (f->inputs[1]) {
			mblk_t *bg;
			while ((bg = ms_queue_get(f->inputs[1])) != nullptr) {
				if (mBgFrame) freemsg(mBgFrame);
				mBgFrame = bg;
				mBgDirty = true;
			}
			if (mBgFrame) {
				ms_yuv_buf_init_from_mblk(&mBgPic, mBgFrame);
				mHasBg = true;
			}
		}

		while ((m = ms_queue_get(f->inputs[0])) != nullptr) {
			MSPicture pic;
			ms_yuv_buf_init_from_mblk(&pic, m);

			if (!mBypass) {
				// YUV -> RGBA
				std::vector<uint8_t> argb(pic.w * pic.h * 4);
				libyuv::I420ToARGB(pic.planes[0], pic.strides[0], pic.planes[1], pic.strides[1], pic.planes[2],
				                   pic.strides[2], argb.data(), pic.w * 4, pic.w, pic.h);

				// PRODUCER publish the last image
				{
					std::lock_guard<std::mutex> lk(mMutex);
					mLastImg = std::move(argb);
					mImgW = pic.w;
					mImgH = pic.h;
					mHasNewImg = true;
				}
				mCv.notify_one();

				// CONSUMER : get the last alpha available
				std::vector<uint8_t> alpha;
				int aW = 0, aH = 0;
				{
					std::lock_guard<std::mutex> lk(mMutex);
					if (mHasResult) {
						auto now = std::chrono::high_resolution_clock::now();
						auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - mTimeLastResult).count();
						alpha = mLastResult;
						aW = maskW_;
						aH = maskH_;
						if (age > 200) {
							std::fill(alpha.begin(), alpha.end(), (uint8_t)255); // no foreground
						}
					}
				}

				// Reconstuction of final image with blend
				if (!alpha.empty()) {
					const int W = pic.w, H = pic.h;

					if ((size_t)aW * aH == alpha.size() && (aW != W || aH != H)) {
						std::vector<uint8_t> scaled((size_t)W * H);
						libyuv::ScalePlane(alpha.data(), aW, aW, aH, scaled.data(), W, W, H, libyuv::kFilterBilinear);
						alpha = std::move(scaled);
					}

					if (mHasBg && (mBgDirty || mBgScaledW != W || mBgScaledH != H)) {
						mBgY.resize((size_t)W * H);
						mBgU.resize((size_t)(W / 2) * (H / 2));
						mBgV.resize((size_t)(W / 2) * (H / 2));
						libyuv::I420Scale(mBgPic.planes[0], mBgPic.strides[0], mBgPic.planes[1], mBgPic.strides[1],
						                  mBgPic.planes[2], mBgPic.strides[2], mBgPic.w, mBgPic.h, mBgY.data(), W,
						                  mBgU.data(), W / 2, mBgV.data(), W / 2, W, H, libyuv::kFilterBilinear);
						mBgScaledW = W;
						mBgScaledH = H;
						mBgDirty = false;
					}

					if (!mHasBg && (mBgScaledW != W || mBgScaledH != H)) {
						mBgY.assign((size_t)W * H, 0);
						mBgU.assign((size_t)(W / 2) * (H / 2), 128);
						mBgV.assign((size_t)(W / 2) * (H / 2), 128);
						mBgScaledW = W;
						mBgScaledH = H;
					}

					libyuv::I420Blend(mBgY.data(), W, mBgU.data(), W / 2, mBgV.data(), W / 2,       // src0 = fond
					                  pic.planes[0], pic.strides[0], pic.planes[1], pic.strides[1], // src1 = caméra
					                  pic.planes[2], pic.strides[2], alpha.data(), W, // alpha plein écran
					                  pic.planes[0], pic.strides[0], pic.planes[1], pic.strides[1], // dst = en place
					                  pic.planes[2], pic.strides[2], W, H);
				}
			}
			ms_queue_put(f->outputs[0], m);
		}
	}

	bool getBypass() {
		return mBypass;
	}


private:
	// function designed to resize the given image to match the dimensions
	// and format (RGBA) of the filter
	std::vector<float> preprocesser(const std::vector<uint8_t> &rgba, int srcW, int srcH) const {
		std::vector<uint8_t> redim = redimRGBA(rgba, srcW, srcH, modelW_, modelH_);
		std::vector<float> tenseur(1 * 3 * modelH_ * modelW_);
		const int plan = modelH_ * modelW_;
		for (int y = 0; y < modelH_; ++y)
			for (int x = 0; x < modelW_; ++x) {
				const uint8_t *px = &redim[(y * modelW_ + x) * 4];
				float c0 = px[0] / 255.0f, c1 = px[1] / 255.0f, c2 = px[2] / 255.0f;
				if (swapRB_) std::swap(c0, c2);
				int idx = y * modelW_ + x;
				tenseur[0 * plan + idx] = (c0 - mean_[0]) / std_[0];
				tenseur[1 * plan + idx] = (c1 - mean_[1]) / std_[1];
				tenseur[2 * plan + idx] = (c2 - mean_[2]) / std_[2];
			}
		return tenseur;
	}

	std::vector<uint8_t> redimRGBA(const std::vector<uint8_t> &img, int srcW, int srcH, int dstW, int dstH) const {
		std::vector<uint8_t> out((size_t)dstW * dstH * 4);
		float sx = (float)srcW / dstW, sy = (float)srcH / dstH;
		for (int y = 0; y < dstH; ++y) {
			int ys = (int)(y * sy);
			for (int x = 0; x < dstW; ++x) {
				int xs = (int)(x * sx);
				memcpy(&out[(y * dstW + x) * 4], &img[(ys * srcW + xs) * 4], 4);
			}
		}
		return out;
	}

	std::vector<float> redimMasque(const std::vector<float> &s, int sw, int sh, int dw, int dh) {
		std::vector<float> out((size_t)dw * dh);
		for (int y = 0; y < dh; ++y) {
			float fy = (y + 0.5f) * sh / dh - 0.5f;
			int y0 = (int)std::floor(fy);
			float wy = fy - y0;
			int y0c = std::clamp(y0, 0, sh - 1);
			int y1c = std::clamp(y0 + 1, 0, sh - 1);
			for (int x = 0; x < dw; ++x) {
				float fx = (x + 0.5f) * sw / dw - 0.5f;
				int x0 = (int)std::floor(fx);
				float wx = fx - x0;
				int x0c = std::clamp(x0, 0, sw - 1);
				int x1c = std::clamp(x0 + 1, 0, sw - 1);
				float a = s[y0c * sw + x0c], b = s[y0c * sw + x1c];
				float c = s[y1c * sw + x0c], d = s[y1c * sw + x1c];
				out[y * dw + x] = (a * (1 - wx) + b * wx) * (1 - wy) + (c * (1 - wx) + d * wx) * wy;
			}
		}
		return out;
	}

	// Function on wich the thread is processing foreground
	void inference_computer() {

		while (mRunning) {

			std::vector<uint8_t> img;
			int w, h;

			{
				std::unique_lock<std::mutex> lk(mMutex);
				mCv.wait(lk, [this] { return mHasNewImg || !mRunning; });
				if (!mRunning) break;
				img = std::move(mLastImg);
				w = mImgW;
				h = mImgH;
				mHasNewImg = false;
			}

			std::vector<float> input = preprocesser(img, w, h);
			std::array<int64_t, 4> inShape = {1, 3, modelH_, modelW_};

			// Model setup for the frame
			Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
			Ort::Value inT =
			    Ort::Value::CreateTensor<float>(memInfo, input.data(), input.size(), inShape.data(), inShape.size());

			// Inference processing
			const char *insN[] = {mInName.c_str()};
			const char *outsN[] = {mOutName.c_str()};
			auto out = mSession.Run(Ort::RunOptions{nullptr}, insN, &inT, 1, outsN, 1);

			// sortie [1,2,H,W]
			auto shape = out[0].GetTensorTypeAndShapeInfo().GetShape(); // {1, 2, H, W}
			int H = (int)shape[2], W = (int)shape[3];
			int plane = H * W;
			const float *o = out[0].GetTensorMutableData<float>();

			// mask
			std::vector<float> mask(plane);
			for (int i = 0; i < plane; ++i)
				mask[i] = o[HUMAN_CH * plane + i];

			if (lastImgBuffer.size() != mask.size()) lastImgBuffer = mask;
			else
				for (size_t i = 0; i < mask.size(); ++i)
					lastImgBuffer[i] = alpha_ * mask[i] + (1.0f - alpha_) * lastImgBuffer[i];

			std::vector<float> outMask = redimMasque(lastImgBuffer, W, H, w, h);

			std::vector<uint8_t> alpha(outMask.size());
			for (size_t i = 0; i < outMask.size(); ++i) {
				float person = 1.0f - outMask[i];
				float a = std::clamp((person - mThreshold) / (1.0f - mThreshold), 0.0f, 1.0f);
				a = a * a * (3.0f - 2.0f * a);
				alpha[i] = (uint8_t)((1.0f - a) * 255.0f);
			}

			{
				std::lock_guard<std::mutex> lk(mMutex);
				mLastResult = std::move(alpha); // uint8 maintenant
				mTimeLastResult = std::chrono::high_resolution_clock::now();
				maskW_ = w;
				maskH_ = h;
				mHasResult = true;
			}
		}
	}

	// variables for the thread gestion
	std::thread mWorker;
	std::mutex mMutex;
	std::condition_variable mCv;
	std::atomic<bool> mRunning{true};

	// globales variables
	std::vector<uint8_t> mLastImg;
	int mImgW = 0, mImgH = 0;
	bool mHasNewImg = false;
	std::chrono::time_point<std::chrono::high_resolution_clock> mTimeLastResult;
	std::vector<uint8_t> mLastResult;
	std::vector<float> lastImgBuffer;
	bool mHasResult = false;
	mblk_t *mBgFrame = nullptr;
	MSPicture mBgPic{};
	bool mHasBg = false;
	std::vector<uint8_t> mBgY, mBgU, mBgV;
	int mBgScaledW = 0, mBgScaledH = 0;
	bool mBgDirty = false;
	bool mBypass;
	bool mModelLoaded = false;
	MSFilter *mFilter = nullptr;

	// ONNX Runtime states
	Ort::Env mEnv{ORT_LOGGING_LEVEL_WARNING, "pphumanseg"};
	Ort::Session mSession{nullptr};
	Ort::AllocatorWithDefaultOptions mAlloc;
	std::string mInName, mOutName;

	// model parameters
	float mThreshold = 0.5f; // Min probability to keep a pixel as a person
	float alpha_ = 0.5f;
	int modelW_ = 192, modelH_ = 192;
	int maskW_ = 0, maskH_ = 0;
	bool swapRB_ = false;
	float mean_[3] = {0.5f, 0.5f, 0.5f};
	float std_[3] = {0.5f, 0.5f, 0.5f};
	static constexpr int HUMAN_CH = 0;
};

} // namespace mediastreamer

using namespace mediastreamer;

static void ms_background_replacer_init(MSFilter *f) {
	f->data = new BackgroundReplacer(f);
}

static void ms_background_replacer_uninit(MSFilter *f) {
	delete reinterpret_cast<BackgroundReplacer *>(f->data);
}

static void ms_background_replacer_process(MSFilter *f) {
	reinterpret_cast<BackgroundReplacer *>(f->data)->process(f);
}

static int ms_background_replacer_get_bypass(MSFilter *f, void *data) {
	BackgroundReplacer *s = (BackgroundReplacer *)f->data;
	*(int *)data = s->getBypass();
	return 0;
}

static int ms_background_replacer_set_bypass(MSFilter *f, void *data) {
	BackgroundReplacer *s = (BackgroundReplacer *)f->data;
	s->setBypass(*(int *)data);
	return 0;
}

static MSFilterMethod ms_background_replacer_methods[] = {
    {MS_BACKGROUND_REPLACER_SET_BYPASS, ms_background_replacer_set_bypass},
    {MS_BACKGROUND_REPLACER_GET_BYPASS, ms_background_replacer_get_bypass},
    {0, nullptr}};

extern "C" {

MSFilterDesc ms_BackgroundReplacer_desc = {
    MS_BACKGROUND_REPLACER_ID,      /* id */
    "MSBackgroundReplacer",         /* name */
    "YUV420P background replacer",  /* text */
    MS_FILTER_OTHER,                /* category */
    nullptr,                        /* enc_fmt */
    2,                              /* ninputs */
    1,                              /* noutputs */
    ms_background_replacer_init,    /* init */
    nullptr,                        /* preprocess */
    ms_background_replacer_process, /* process */
    nullptr,                        /* postprocess */
    ms_background_replacer_uninit,  /* uninit */
    ms_background_replacer_methods, /* methods */
    0                               /* flags */
};
}

MS_FILTER_DESC_EXPORT(ms_BackgroundReplacer_desc)
