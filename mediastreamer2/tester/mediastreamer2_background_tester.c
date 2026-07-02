#include "mediastreamer2/msbackgroundformater.h"
#include "mediastreamer2/msfactory.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"

static MSFactory *_factory = NULL;

static int tester_before_all(void) {
	_factory = ms_tester_factory_new();
	return 0;
}

static int tester_after_all(void) {
	ms_factory_destroy(_factory);
	_factory = NULL;
	return 0;
}
#ifdef VIDEO_BACKGROUND_ENABLED
static void background_formater_creation(void) {
	MSFilter *f = ms_factory_create_filter(_factory, MS_BACKGROUND_FORMATER_ID);
	BC_ASSERT_PTR_NOT_NULL(f);
	if (f) {
		BC_ASSERT_EQUAL(f->desc->ninputs, 3, int, "%d");
		BC_ASSERT_EQUAL(f->desc->noutputs, 1, int, "%d");
		ms_filter_destroy(f);
	}
}

static void background_formater_type_change(void) {
	MSFilter *f = ms_factory_create_filter(_factory, MS_BACKGROUND_FORMATER_ID);
	const MSBackgroundType types[] = {MSBackgroundSame, MSBackgroundImage, MSBackgroundVideo, MSBackgroundBlur};
	int i;
	BC_ASSERT_PTR_NOT_NULL(f);
	if (!f) return;
	for (i = 0; i < 4; i++) {
		int in = (int)types[i];
		int out = -1;
		ms_filter_call_method(f, MS_BACKGROUND_FORMATER_SET_TYPE, &in);
		ms_filter_call_method(f, MS_BACKGROUND_FORMATER_GET_TYPE, &out);
		BC_ASSERT_EQUAL(out, in, int, "%d");
	}
	ms_filter_destroy(f);
}

static void background_formater_default_type(void) {
	MSFilter *f = ms_factory_create_filter(_factory, MS_BACKGROUND_FORMATER_ID);
	int t = -1;
	BC_ASSERT_PTR_NOT_NULL(f);
	if (!f) return;
	ms_filter_call_method(f, MS_BACKGROUND_FORMATER_GET_TYPE, &t);
	BC_ASSERT_EQUAL(t, MSBackgroundSame, int, "%d"); /* passthrough by default*/
	ms_filter_destroy(f);
}

static void background_replacer_bypass_change(void) {
	MSFilter *f = ms_factory_create_filter(_factory, MS_BACKGROUND_REPLACER_ID);
	int v;
	BC_ASSERT_PTR_NOT_NULL(f);
	if (!f) return;

	v = 0;
	ms_filter_call_method(f, MS_BACKGROUND_REPLACER_SET_BYPASS, &v);
	v = -1;
	ms_filter_call_method(f, MS_BACKGROUND_REPLACER_GET_BYPASS, &v);
	BC_ASSERT_EQUAL(v, 0, int, "%d");

	v = 1;
	ms_filter_call_method(f, MS_BACKGROUND_REPLACER_SET_BYPASS, &v);
	v = -1;
	ms_filter_call_method(f, MS_BACKGROUND_REPLACER_GET_BYPASS, &v);
	BC_ASSERT_EQUAL(v, 1, int, "%d");

	ms_filter_destroy(f);
}
static void background_replacer_creation(void) {
	MSFilter *f = ms_factory_create_filter(_factory, MS_BACKGROUND_REPLACER_ID);
	int bypass = -1;
	BC_ASSERT_PTR_NOT_NULL(f);
	if (!f) return;
	BC_ASSERT_EQUAL(f->desc->ninputs, 2, int, "%d");
	BC_ASSERT_EQUAL(f->desc->noutputs, 1, int, "%d");
	ms_filter_call_method(f, MS_BACKGROUND_REPLACER_GET_BYPASS, &bypass);
	BC_ASSERT_EQUAL(bypass, 1, int, "%d"); /* bypass is active by default */
	ms_filter_destroy(f);
}

/* feeds the formatter's 3 inputs and cycles through the 4 modes via a ticker */
static void background_formater_process_all_modes(void) {
	MSFilter *cam = NULL, *image = NULL, *video = NULL, *formater = NULL, *sink = NULL;
	MSTicker *ticker = NULL;
	MSVideoSize vsize = {320, 240};
	float fps = 15.0f;
	const MSBackgroundType modes[] = {MSBackgroundSame, MSBackgroundImage, MSBackgroundVideo, MSBackgroundBlur};
	int i;

	cam = ms_factory_create_filter(_factory, MS_STATIC_IMAGE_ID);
	image = ms_factory_create_filter(_factory, MS_STATIC_IMAGE_ID);
	video = ms_factory_create_filter(_factory, MS_STATIC_IMAGE_ID);
	formater = ms_factory_create_filter(_factory, MS_BACKGROUND_FORMATER_ID);
	sink = ms_factory_create_filter(_factory, MS_VOID_SINK_ID);
	BC_ASSERT_PTR_NOT_NULL(formater);
	if (!cam || !image || !video || !formater || !sink) goto end;

	{
		MSFilter *srcs[3] = {cam, image, video};
		for (i = 0; i < 3; i++) {
			ms_filter_call_method(srcs[i], MS_FILTER_SET_VIDEO_SIZE, &vsize);
			ms_filter_call_method(srcs[i], MS_FILTER_SET_FPS, &fps);
		}
	}

	ms_filter_link(cam, 0, formater, 0);
	ms_filter_link(image, 0, formater, 1);
	ms_filter_link(video, 0, formater, 2);
	ms_filter_link(formater, 0, sink, 0);

	ticker = ms_ticker_new();
	ms_ticker_attach(ticker, cam);

	for (i = 0; i < 4; i++) {
		int t = (int)modes[i];
		ms_filter_call_method(formater, MS_BACKGROUND_FORMATER_SET_TYPE, &t);
		ms_usleep(300000);
	}

	ms_ticker_detach(ticker, cam);
	ms_filter_unlink(cam, 0, formater, 0);
	ms_filter_unlink(image, 0, formater, 1);
	ms_filter_unlink(video, 0, formater, 2);
	ms_filter_unlink(formater, 0, sink, 0);
	BC_ASSERT_TRUE(TRUE);
end:
	if (ticker) ms_ticker_destroy(ticker);
	if (cam) ms_filter_destroy(cam);
	if (image) ms_filter_destroy(image);
	if (video) ms_filter_destroy(video);
	if (formater) ms_filter_destroy(formater);
	if (sink) ms_filter_destroy(sink);
}

#endif

static test_t tests[] = {
#ifdef VIDEO_BACKGROUND_ENABLED
    TEST_NO_TAG("Background formater creation", background_formater_creation),
    TEST_NO_TAG("Background formater type change", background_formater_type_change),
    TEST_NO_TAG("Background formater default type", background_formater_default_type),

    TEST_NO_TAG("Background replacer bypass change", background_replacer_bypass_change),
    TEST_NO_TAG("Background replacer creation", background_replacer_creation),
    TEST_NO_TAG("Background formater process all modes", background_formater_process_all_modes),
#endif
};

test_suite_t background_test_suite = {
    "Background", tester_before_all, tester_after_all, NULL, NULL, sizeof(tests) / sizeof(tests[0]), tests, 0};