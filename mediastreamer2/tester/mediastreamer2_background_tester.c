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

#ifdef VIDEO_BACKGROUND_ENABLED
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
#endif

static test_t tests[] = {
    TEST_NO_TAG("Background formater creation", background_formater_creation),
    TEST_NO_TAG("Background formater type change", background_formater_type_change),
#ifdef VIDEO_BACKGROUND_ENABLED
    TEST_NO_TAG("Background replacer bypass change", background_replacer_bypass_change),
#endif
};

test_suite_t background_test_suite = {"Background",
                                      tester_before_all,
                                      tester_after_all,
                                      NULL,
                                      NULL,
                                      sizeof(tests) / sizeof(tests[0]),
                                      tests,
                                      0};