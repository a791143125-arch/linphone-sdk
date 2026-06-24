// Filtre prenant une image Yuv420 en entrée et une background (optionnel) aussi en Yuv420
// Et sort l'image avec l'arrière plan remplacé par le background
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

// Classe principal du filtre
// Contient le process et toute les fonctions nécessaires pour fonctionner
class BackgroundReplacer {
public:
	BackgroundReplacer(MSFilter *f, bool bypass = true) {

		mBypass = bypass;

		// SETUP du model à faire une seule fois
		char *path = ms_strdup_printf("%s/model.onnx", ms_factory_get_image_resources_dir(f->factory));
		mOpts.SetInterOpNumThreads(1);
		mOpts.SetIntraOpNumThreads(1);
		mSession = Ort::Session(mEnv, path, mOpts);
		ms_free(path);
		mTimeLastResult = std::chrono::high_resolution_clock::now();

		// noms d'entrée/sortie récupérés une fois
		Ort::AllocatorWithDefaultOptions alloc;
		mInName = mSession.GetInputNameAllocated(0, alloc).get();
		mOutName = mSession.GetOutputNameAllocated(0, alloc).get();

		mWorker = std::thread(&BackgroundReplacer::inference_computer, this);
	}

	// Destruction du filtre en détruisant aussi le thread
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
		auto tmp_trait = std::chrono::high_resolution_clock::now();
		// Lit si il y a une deuxième entrée pour le fond
		// et l'assigne à mBgFrame
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

		// Lit l'entrée de laquelle il faut séparer la personne
		while ((m = ms_queue_get(f->inputs[0])) != nullptr) {
			MSPicture pic;
			ms_yuv_buf_init_from_mblk(&pic, m);

			if (!mBypass) {
				// YUV -> RGBA
				std::vector<uint8_t> argb(pic.w * pic.h * 4);
				libyuv::I420ToARGB(pic.planes[0], pic.strides[0], pic.planes[1], pic.strides[1], pic.planes[2],
				                   pic.strides[2], argb.data(), pic.w * 4, pic.w, pic.h);

				// PRODUCTEUR publie la dernière image
				{
					std::lock_guard<std::mutex> lk(mMutex);
					mLastImg = std::move(argb);
					mImgW = pic.w;
					mImgH = pic.h;
					mHasNewImg = true;
				}
				mCv.notify_one();

				// CONSOMMATEUR : récupère le dernier alpha dispo (uint8, déjà pleine résolution)
				std::vector<uint8_t> alpha;
				{
					std::lock_guard<std::mutex> lk(mMutex);
					if (mHasResult) {
						auto now = std::chrono::high_resolution_clock::now();
						auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - mTimeLastResult).count();
						alpha = mLastResult;
						if (age > 200) {
							std::cout << "temps trop long depuis dernier mask\n";
							std::fill(alpha.begin(), alpha.end(), (uint8_t)255); // fond complet (sûr)
						}
					}
				}

				// Reconstruction via blend SIMD (libyuv)
				if (!alpha.empty()) {
					const int W = pic.w, H = pic.h;

					// (re)scale le fond vers la taille image, uniquement si nécessaire
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

					// pas de fond -> fond noir (Y=0, U/V=128), comme l'ancien comportement
					if (!mHasBg && (mBgScaledW != W || mBgScaledH != H)) {
						mBgY.assign((size_t)W * H, 0);
						mBgU.assign((size_t)(W / 2) * (H / 2), 128);
						mBgV.assign((size_t)(W / 2) * (H / 2), 128);
						mBgScaledW = W;
						mBgScaledH = H;
					}

					// dst = fond·α + caméra·(1-α), en place dans pic, en SIMD
					libyuv::I420Blend(mBgY.data(), W, mBgU.data(), W / 2, mBgV.data(), W / 2,       // src0 = fond
					                  pic.planes[0], pic.strides[0], pic.planes[1], pic.strides[1], // src1 = caméra
					                  pic.planes[2], pic.strides[2], alpha.data(), W, // alpha plein écran
					                  pic.planes[0], pic.strides[0], pic.planes[1], pic.strides[1], // dst = en place
					                  pic.planes[2], pic.strides[2], W, H);
				}

				auto tmp_trait2 = std::chrono::high_resolution_clock::now();
				if (verbosePerf) std::cout << "temps du traitement : "
				          << std::chrono::duration_cast<std::chrono::milliseconds>(tmp_trait2 - tmp_trait).count()
				          << "ms\n";
			}
			ms_queue_put(f->outputs[0], m);
		}
	}

	bool getBypass() {
		return mBypass;
	}

	void setBypass(bool bypass) {
		mBypass = bypass;
	}

private:
	// fonction destiné à redimensionner l'image donnée pour correspondre
	// à la dimension et au format (RGBA) du filtre
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

	// redimension bilinéaire pour un résultat fluide
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

	// Fonction sur laquelle le thread du calcul de l'inférence va tourner
	void inference_computer() {

		std::cout << "lancement inference worker\n";
		while (mRunning) {

			std::vector<uint8_t> img;
			int w, h;

			// Lit la dernière image disponible (lock pour éviter accès concurrent)
			{
				std::unique_lock<std::mutex> lk(mMutex);
				mCv.wait(lk, [this] { return mHasNewImg || !mRunning; });
				if (!mRunning) break;
				img = std::move(mLastImg);
				w = mImgW;
				h = mImgH;
				mHasNewImg = false;
			}

			auto tmp_infwork = std::chrono::high_resolution_clock::now();
			// preprocess redimension de l'image à la bonne taille
			std::vector<float> input = preprocesser(img, w, h);
			std::array<int64_t, 4> inShape = {1, 3, modelH_, modelW_};

			// Setup le model pour acceuillir notre image
			Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
			Ort::Value inT =
			    Ort::Value::CreateTensor<float>(memInfo, input.data(), input.size(), inShape.data(), inShape.size());

			// inférence
			const char *insN[] = {mInName.c_str()};
			const char *outsN[] = {mOutName.c_str()};
			auto out = mSession.Run(Ort::RunOptions{nullptr}, insN, &inT, 1, outsN, 1);

			// sortie [1,2,H,W]
			auto shape = out[0].GetTensorTypeAndShapeInfo().GetShape(); // {1, 2, H, W}
			int H = (int)shape[2], W = (int)shape[3];
			int plane = H * W;
			const float *o = out[0].GetTensorMutableData<float>();

			// masque
			std::vector<float> mask(plane);
			for (int i = 0; i < plane; ++i)
				mask[i] = o[HUMAN_CH * plane + i];

			if (lastImgBuffer.size() != mask.size()) lastImgBuffer = mask;
			else
				for (size_t i = 0; i < mask.size(); ++i)
					lastImgBuffer[i] = alpha_ * mask[i] + (1.0f - alpha_) * lastImgBuffer[i];

			std::vector<float> outMask = redimMasque(lastImgBuffer, W, H, w, h);

			// float [0..1] -> uint8 [0..255] pour libyuv
			std::vector<uint8_t> alpha(outMask.size());
			for (size_t i = 0; i < outMask.size(); ++i)
				alpha[i] = (uint8_t)std::clamp(outMask[i] * 255.0f, 0.0f, 255.0f);

			{
				std::lock_guard<std::mutex> lk(mMutex);
				mLastResult = std::move(alpha); // uint8 maintenant
				mTimeLastResult = std::chrono::high_resolution_clock::now();
				maskW_ = w;
				maskH_ = h;
				mHasResult = true;
			}

			auto tmp_infwork2 = std::chrono::high_resolution_clock::now();
			
			if (verbosePerf) std::cout << "Nouveau mask calculé en "
			          << std::chrono::duration_cast<std::chrono::milliseconds>(tmp_infwork2 - tmp_infwork).count()
			          << "ms\n";
		}
	}

	// variables pour le thread
	std::thread mWorker;
	std::mutex mMutex;
	std::condition_variable mCv;
	std::atomic<bool> mRunning{true};

	// variables globales
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
	bool verbosePerf = false;

	// etats ONNX Runtime
	Ort::Env mEnv{ORT_LOGGING_LEVEL_WARNING, "pphumanseg"};
	Ort::SessionOptions mOpts;
	Ort::Session mSession{nullptr};
	Ort::AllocatorWithDefaultOptions mAlloc;
	std::string mInName, mOutName;

	// params du modèle
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
    .id = MS_BACKGROUND_REPLACER_ID,
    .name = "MSBackgroundReplacer",
    .text = "YUV420P background replacer",
    .category = MS_FILTER_OTHER,
    .ninputs = 2,
    .noutputs = 1,
    .init = ms_background_replacer_init,
    .process = ms_background_replacer_process,
    .uninit = ms_background_replacer_uninit,
    .methods = ms_background_replacer_methods,
};
}

MS_FILTER_DESC_EXPORT(ms_BackgroundReplacer_desc)
