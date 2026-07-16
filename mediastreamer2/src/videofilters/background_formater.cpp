#include "libyuv/scale.h"
#include "mediastreamer2/msbackgroundformater.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include <algorithm>
#include <chrono>
#include <vector>

namespace mediastreamer {

static void boxBlurPlane(
    const uint8_t *src, int srcStride, uint8_t *dst, int dstStride, int w, int h, int r, std::vector<uint8_t> &tmp) {
	if (r < 1 || w < 2 || h < 2) {
		for (int y = 0; y < h; ++y)
			memcpy(dst + y * dstStride, src + y * srcStride, w);
		return;
	}
	tmp.resize((size_t)w * h);
	const int win = 2 * r + 1;
	const uint32_t recip = (1u << 24) / (uint32_t)win;
	for (int y = 0; y < h; ++y) { // horizontal pass src -> tmp
		const uint8_t *s = src + y * srcStride;
		uint8_t *t = tmp.data() + (size_t)y * w;
		int sum = 0;
		for (int i = -r; i <= r; ++i)
			sum += s[std::clamp(i, 0, w - 1)];
		for (int x = 0; x < w; ++x) {
			t[x] = (uint8_t)(((uint32_t)sum * recip) >> 24);
			sum -= s[std::clamp(x - r, 0, w - 1)];
			sum += s[std::clamp(x + r + 1, 0, w - 1)];
		}
	}
	for (int x = 0; x < w; ++x) { // vertical pass tmp -> dst
		int sum = 0;
		for (int i = -r; i <= r; ++i)
			sum += tmp[(size_t)std::clamp(i, 0, h - 1) * w + x];
		for (int y = 0; y < h; ++y) {
			dst[(size_t)y * dstStride + x] = (uint8_t)(((uint32_t)sum * recip) >> 24);
			sum -= tmp[(size_t)std::clamp(y - r, 0, h - 1) * w + x];
			sum += tmp[(size_t)std::clamp(y + r + 1, 0, h - 1) * w + x];
		}
	}
}

class BackgroundFormater {
public:
	BackgroundFormater(MSBackgroundType mode = MSBackgroundSame) : mMode(mode) {
	}

	~BackgroundFormater() {
		if (mBgFrameImage) freemsg(mBgFrameImage);
		if (mBgFrameVideo) freemsg(mBgFrameVideo);
	}

	void process(MSFilter *f) {
		mblk_t *m;
		// static image
		if (f->inputs[1]) {
			mblk_t *bg;
			while ((bg = ms_queue_get(f->inputs[1])) != nullptr) {
				if (mMode == MSBackgroundImage) {
					if (mBgFrameImage) freemsg(mBgFrameImage);
					mBgFrameImage = bg;
				} else {
					freemsg(bg);
				}
			}
		}
		// video
		if (f->inputs[2]) {
			mblk_t *bg;
			while ((bg = ms_queue_get(f->inputs[2])) != nullptr) {
				if (mMode == MSBackgroundVideo) {
					if (mBgFrameVideo) freemsg(mBgFrameVideo);
					mBgFrameVideo = bg;
				} else {
					freemsg(bg);
				}
			}
		}
		if (mMode == MSBackgroundImage) {
			mBgFrame = mBgFrameImage;
		} else if (mMode == MSBackgroundVideo) {
			mBgFrame = mBgFrameVideo;
		}

		while ((m = ms_queue_get(f->inputs[0])) != nullptr) {

			if ((mMode == MSBackgroundImage || mMode == MSBackgroundVideo) && mBgFrame) {
				ms_queue_put(f->outputs[0], dupmsg(mBgFrame));
				freemsg(m);
				continue;
			} else if (mMode == MSBackgroundBlur) {
				MSPicture src, dst;
				ms_yuv_buf_init_from_mblk(&src, m);
				mblk_t *out = ms_yuv_buf_alloc(&dst, src.w, src.h);

				// Y Plan : downscale ÷2 -> blur -> upscale to gain ressources
				int sw = src.w / 2, sh = src.h / 2;
				mSmall.resize((size_t)sw * sh);
				mSmallBlur.resize((size_t)sw * sh);
				libyuv::ScalePlane(src.planes[0], src.strides[0], src.w, src.h, mSmall.data(), sw, sw, sh,
				                   libyuv::kFilterBilinear);
				boxBlurPlane(mSmall.data(), sw, mSmallBlur.data(), sw, sw, sh, mRadius / 2, mTmp);
				libyuv::ScalePlane(mSmallBlur.data(), sw, sw, sh, dst.planes[0], dst.strides[0], src.w, src.h,
				                   libyuv::kFilterBilinear);

				// U/V chroma plan : already at semi resolution, directly blurred
				int cw = src.w / 2, ch = src.h / 2;
				boxBlurPlane(src.planes[1], src.strides[1], dst.planes[1], dst.strides[1], cw, ch, mRadius / 2, mTmp);
				boxBlurPlane(src.planes[2], src.strides[2], dst.planes[2], dst.strides[2], cw, ch, mRadius / 2, mTmp);

				freemsg(m);
				ms_queue_put(f->outputs[0], out);
				continue;
			} else if (mMode == MSBackgroundVideo) {
				MSPicture src, dst;
				ms_yuv_buf_init_from_mblk(&src, m);
				mblk_t *out = ms_yuv_buf_alloc(&dst, src.w, src.h);

				freemsg(m);
				ms_queue_put(f->outputs[0], out);
				continue;
			}

			MSPicture src, dst;
			ms_yuv_buf_init_from_mblk(&src, m);
			mblk_t *out = ms_yuv_buf_alloc(&dst, src.w, src.h);

			for (int y = 0; y < src.h; ++y)
				memcpy(dst.planes[0] + y * dst.strides[0], src.planes[0] + y * src.strides[0], src.w);
			for (int y = 0; y < src.h / 2; ++y) {
				memcpy(dst.planes[1] + y * dst.strides[1], src.planes[1] + y * src.strides[1], src.w / 2);
				memcpy(dst.planes[2] + y * dst.strides[2], src.planes[2] + y * src.strides[2], src.w / 2);
			}

			freemsg(m);
			ms_queue_put(f->outputs[0], out);
		}
	}
	void setMode(int m) {
		mMode = (MSBackgroundType)m;
	}
	int getMode() const {
		return (int)mMode;
	}

private:
	MSBackgroundType mMode;
	mblk_t *mBgFrame = nullptr;
	mblk_t *mBgFrameImage = nullptr;
	mblk_t *mBgFrameVideo = nullptr;
	int mRadius = 25;
	std::vector<uint8_t> mTmp;
	std::vector<uint8_t> mSmall, mSmallBlur;
};
} // namespace mediastreamer

using namespace mediastreamer;

static void ms_background_formater_init(MSFilter *f) {
	f->data = new BackgroundFormater();
}

static void ms_background_formater_uninit(MSFilter *f) {
	delete reinterpret_cast<BackgroundFormater *>(f->data);
}

static void ms_background_formater_process(MSFilter *f) {
	reinterpret_cast<BackgroundFormater *>(f->data)->process(f);
}

static int ms_background_formater_get_type(MSFilter *f, void *data) {
	BackgroundFormater *s = (BackgroundFormater *)f->data;
	*(int *)data = s->getMode();
	return 0;
}

static int ms_background_formater_set_type(MSFilter *f, void *data) {
	BackgroundFormater *s = (BackgroundFormater *)f->data;
	s->setMode(*(int *)data);
	return 0;
}

static MSFilterMethod ms_background_formater_methods[] = {
    {MS_BACKGROUND_FORMATER_SET_TYPE, ms_background_formater_set_type},
    {MS_BACKGROUND_FORMATER_GET_TYPE, ms_background_formater_get_type},
    {0, nullptr}};

extern "C" {

MSFilterDesc ms_BackgroundFormater_desc = {
    MS_BACKGROUND_FORMATER_ID,      /* id */
    "MSBackgroundFormater",         /* name */
    "YUV420P background formater",  /* text */
    MS_FILTER_OTHER,                /* category */
    nullptr,                        /* enc_fmt */
    3,                              /* ninputs */
    1,                              /* noutputs */
    ms_background_formater_init,    /* init */
    nullptr,                        /* preprocess */
    ms_background_formater_process, /* process */
    nullptr,                        /* postprocess */
    ms_background_formater_uninit,  /* uninit */
    ms_background_formater_methods, /* methods */
    0                               /* flags */
};
}

MS_FILTER_DESC_EXPORT(ms_BackgroundFormater_desc)
