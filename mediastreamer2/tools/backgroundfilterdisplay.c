#include "bctoolbox/defs.h"
#include "mediastreamer2/allfilters.h"
#include "mediastreamer2/msbackgroundformater.h"
#include "mediastreamer2/mseventqueue.h"
#include "mediastreamer2/msfactory.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/mswebcam.h"
#include <signal.h>

static int active = 1;

static void stop_handler(BCTBX_UNUSED(int signum)) {
	active = 0;
}

int main(BCTBX_UNUSED(int argc), BCTBX_UNUSED(char *argv[])) {
	MSFactory *factory;
	MSWebCam *cam;
	MSFilter *capture, *display, *pixconv;
	MSTicker *ticker;
	float fps = 30;
	MSVideoSize vsize = MS_VIDEO_SIZE_VGA;
	MSPixFmt fmt = MS_PIX_FMT_UNKNOWN;

	signal(SIGINT, stop_handler);

	factory = ms_factory_new_with_voip();
	ms_factory_create_event_queue(factory);

	cam = ms_web_cam_manager_get_default_cam(ms_factory_get_web_cam_manager(factory));
	capture = ms_web_cam_create_reader(cam);
	display = ms_factory_create_filter(factory, MS_X11VIDEO_ID);

	/* Configure la source comme le recorder, puis relit le format/taille réels */
	ms_filter_call_method(capture, MS_FILTER_SET_FPS, &fps);
	ms_filter_call_method(capture, MS_FILTER_SET_VIDEO_SIZE, &vsize);
	ms_filter_call_method(capture, MS_FILTER_GET_VIDEO_SIZE, &vsize);
	ms_filter_call_method(capture, MS_FILTER_GET_PIX_FMT, &fmt);

	/* Crée le bon convertisseur selon le format réel de la caméra */
	if (fmt == MS_MJPEG) {
		pixconv = ms_factory_create_decoder(factory, "MJPEG");
	} else {
		pixconv = ms_factory_create_filter(factory, MS_PIX_CONV_ID);
		ms_filter_call_method(pixconv, MS_FILTER_SET_PIX_FMT, &fmt);
		ms_filter_call_method(pixconv, MS_FILTER_SET_VIDEO_SIZE, &vsize);
	}

	MSFilter *bgreplacer = ms_factory_create_filter(factory, MS_BACKGROUND_REPLACER_ID);
	MSFilter *bgformater = ms_factory_create_filter(factory, MS_BACKGROUND_FORMATER_ID);
	MSFilter *tee = ms_factory_create_filter(factory, MS_TEE_ID);
	MSFilter *bgimage = ms_factory_create_filter(factory, MS_STATIC_IMAGE_ID);

	const char *imageName = (argc > 1) ? argv[1] : "";
	char *path = ms_strdup_printf("%s/%s", ms_factory_get_image_resources_dir(factory), imageName);

	ms_filter_call_method(bgimage, MS_FILTER_SET_FPS, &fps);
	ms_filter_call_method(bgimage, MS_FILTER_SET_VIDEO_SIZE, &vsize);
	ms_filter_call_method(bgimage, MS_STATIC_IMAGE_SET_IMAGE, path);
	ms_free(path);

	int type = MSBackgroundImage;
	ms_filter_call_method(bgformater, MS_BACKGROUND_FORMATER_SET_TYPE, &type);

	ms_filter_link(capture, 0, pixconv, 0);
	ms_filter_link(pixconv, 0, tee, 0);
	ms_filter_link(tee, 0, bgformater, 0);
	ms_filter_link(tee, 1, bgreplacer, 0);
	ms_filter_link(bgformater, 0, bgreplacer, 1);
	ms_filter_link(bgreplacer, 0, display, 0);
	ms_filter_link(bgimage, 0, bgformater, 1);

	ticker = ms_ticker_new();
	ms_ticker_attach(ticker, capture);

	while (active) {
		ms_event_queue_pump(ms_factory_get_event_queue(factory));
		ms_usleep(50000);
	}

	ms_ticker_detach(ticker, capture);
	ms_filter_unlink(capture, 0, pixconv, 0);
	ms_filter_unlink(pixconv, 0, tee, 0);
	ms_filter_unlink(tee, 0, bgformater, 0);
	ms_filter_unlink(tee, 1, bgreplacer, 0);
	ms_filter_unlink(bgformater, 0, bgreplacer, 1);
	ms_filter_unlink(bgreplacer, 0, display, 0);
	ms_filter_unlink(bgimage, 0, bgformater, 1);
	ms_ticker_destroy(ticker);
	ms_filter_destroy(bgreplacer);
	ms_filter_destroy(bgformater);
	ms_filter_destroy(tee);
	ms_filter_destroy(pixconv);
	ms_filter_destroy(capture);
	ms_filter_destroy(display);
	ms_filter_destroy(bgimage);
	ms_factory_destroy(factory);
	return 0;
}
