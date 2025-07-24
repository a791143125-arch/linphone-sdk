/*
 * Copyright (c) 2010-2024 Belledonne Communications SARL.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mediastreamer2/msfilter.h"
#include "mswebrtc_noise_suppressor.h"

static void webrtc_noise_suppressor_init(MSFilter *f) {
	mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *NSInst =
	    new mswebrtc_noise_suppressor::mswebrtc_noise_suppressor(f);
	f->data = NSInst;
}

static void webrtc_noise_suppressor_uninit(MSFilter *f) {
	mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *NSInst =
	    static_cast<mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *>(f->data);
	NSInst->uninit();
	delete NSInst;
	f->data = nullptr;
}

static void webrtc_noise_suppressor_preprocess(MSFilter *f) {
	mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *NSInst =
	    static_cast<mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *>(f->data);
	NSInst->preprocess();
}

static void webrtc_noise_suppressor_process(MSFilter *f) {
	mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *NSInst =
	    static_cast<mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *>(f->data);
	// mblk_t *im;
	// while ((im = ms_queue_get(f->inputs[0]))) {
	// 	ms_queue_put(f->outputs[0], im);
	// }
	NSInst->process(f);
}

static void webrtc_noise_suppressor_postprocess(MSFilter *f) {
	mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *NSInst =
	    static_cast<mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *>(f->data);
	NSInst->postprocess();
}

static int webrtc_noise_suppressor_set_sample_rate(MSFilter *f, void *arg) {
	mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *NSInst =
	    static_cast<mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *>(f->data);
	return NSInst->setRate(*static_cast<int *>(arg));
}

static int webrtc_noise_suppressor_get_sample_rate(MSFilter *f, void *arg) {
	// mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *NSInst =
	// static_cast<mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *>(f->data); *static_cast<int *>(arg) =
	// NSInst->getSampleRate();
	return 0;
}

static int webrtc_noise_suppressor_set_bypass_mode(MSFilter *f, void *arg) {
	// mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *NSInst =
	// static_cast<mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *>(f->data); bool_t *bypass = static_cast<bool_t
	// *>(arg); NSInst->setBypassMode(*bypass);
	return 0;
}

static int webrtc_noise_suppressor_get_bypass_mode(MSFilter *f, void *arg) {
	// mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *NSInst =
	// static_cast<mswebrtc_noise_suppressor::mswebrtc_noise_suppressor *>(f->data); *static_cast<bool_t *>(arg) =
	// NSInst->getBypassMode();
	return 0;
}

static MSFilterMethod webrtc_noise_suppressor_methods[] = {
    {MS_FILTER_SET_SAMPLE_RATE, webrtc_noise_suppressor_set_sample_rate},
    {MS_FILTER_GET_SAMPLE_RATE, webrtc_noise_suppressor_get_sample_rate},
    // {MS_NOISE_SUPPRESSOR_SET_BYPASS_MODE, webrtc_noise_suppressor_set_bypass_mode},
    // {MS_NOISE_SUPPRESSOR_GET_BYPASS_MODE, webrtc_noise_suppressor_get_bypass_mode},
    {0, NULL}};

extern "C" MSFilterDesc ms_webrtc_noise_suppressor_desc = {MS_FILTER_PLUGIN_ID,
                                                           "MSWebRTCNoiseSuppressor",
                                                           "Noise suppresion using WebRTC library.",
                                                           MS_FILTER_OTHER,
                                                           NULL,
                                                           1,
                                                           1,
                                                           webrtc_noise_suppressor_init,
                                                           webrtc_noise_suppressor_preprocess,
                                                           webrtc_noise_suppressor_process,
                                                           webrtc_noise_suppressor_postprocess,
                                                           webrtc_noise_suppressor_uninit,
                                                           webrtc_noise_suppressor_methods,
                                                           0};