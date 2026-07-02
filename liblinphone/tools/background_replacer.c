#include <poll.h>
#include <signal.h>

#include "linphone/core.h"
#include <X11/Xlib.h>
#include <bctoolbox/defs.h>
#include <bctoolbox/port.h>
#include <string.h>

static bool_t running = TRUE;
static void stop(BCTBX_UNUSED(int signum)) {
	running = FALSE;
}

static void force_vp8(LinphoneCore *lc) {
	bctbx_list_t *codecs = linphone_core_get_video_payload_types(lc);
	for (bctbx_list_t *e = codecs; e != NULL; e = e->next) {
		LinphonePayloadType *pt = (LinphonePayloadType *)e->data;
		bool_t keep = (strcasecmp(linphone_payload_type_get_mime_type(pt), "VP8") == 0);
		linphone_payload_type_enable(pt, keep);
	}
	bctbx_list_free_with_data(codecs, (bctbx_list_free_func)linphone_payload_type_unref);
}

static void setup(LinphoneCore *lc) {
	const char **devs = linphone_core_get_video_devices(lc);
	for (int i = 0; devs && devs[i]; i++) {
		if (strstr(devs[i], "Mire")) {
			linphone_core_set_video_device(lc, devs[i]);
			ms_message("[bgtest] using camera '%s'", devs[i]);
			return;
		}
	}
	ms_error("[bgtest] Mire camera not found (DEBUG=1 missing ?)");
}

static void
call_state_changed(LinphoneCore *lc, LinphoneCall *call, LinphoneCallState cstate, BCTBX_UNUSED(const char *msg)) {
	switch (cstate) {
		case LinphoneCallIncomingReceived: {
			LinphoneCallParams *p = linphone_core_create_call_params(lc, call);
			linphone_call_params_enable_video(p, TRUE);
			linphone_call_params_set_video_direction(p, LinphoneMediaDirectionRecvOnly); /* pauline REÇOIT+affiche */
			linphone_core_accept_call_with_params(lc, call, p);
			linphone_call_params_unref(p);
			break;
		}

		case LinphoneCallStreamsRunning:
			ms_message("[bgtest] call StreamsRunning (video=%d)",
			           linphone_call_params_video_enabled(linphone_call_get_current_params(call)));
			break;
		default:
			break;
	}
}

static LinphoneCore *make_core(const char *name, int udp_port, bool_t display) {
	/* dossiers isolés par core pour ne PAS toucher à la vraie config/DB de l'utilisateur */
	char *dir = bctbx_strdup_printf("/tmp/bgtest_%s", name);
	bctbx_mkdir(dir);

	LinphoneFactory *factory = linphone_factory_get();
	linphone_factory_set_data_dir(factory, dir); /* main.db, zrtp, etc. -> temp */
	linphone_factory_set_config_dir(factory, dir);
	linphone_factory_set_cache_dir(factory, dir);

	LpConfig *cfg = linphone_config_new(NULL);
	linphone_config_set_string(cfg, "sip", "bind_address", "127.0.0.1");
	linphone_config_set_string(cfg, "rtp", "bind_address", "127.0.0.1");

	LinphoneCoreVTable vtable = {0};
	vtable.call_state_changed = call_state_changed;
	LinphoneCore *lc = linphone_core_new_with_config(&vtable, cfg, NULL);
	linphone_config_unref(cfg);
	bctbx_free(dir);

	linphone_core_set_user_agent(lc, name, NULL);
	linphone_core_enable_video_capture(lc, TRUE);
	linphone_core_enable_video_display(lc, display);
	linphone_core_set_use_files(lc, TRUE);
	linphone_core_enable_echo_cancellation(lc, FALSE);

	LinphoneVideoActivationPolicy *vpol = linphone_factory_create_video_activation_policy(factory);
	linphone_video_activation_policy_set_automatically_accept(vpol, TRUE);
	linphone_video_activation_policy_set_automatically_initiate(vpol, TRUE);
	linphone_core_set_video_activation_policy(lc, vpol);
	linphone_video_activation_policy_unref(vpol);

	LCSipTransports tp = {0};
	tp.udp_port = udp_port;
	linphone_core_set_sip_transports(lc, &tp);
	linphone_core_set_audio_port_range(lc, 1024, 65000);
	linphone_core_set_video_port_range(lc, 1024, 65000);

	setup(lc);

	force_vp8(lc);
	return lc;
}

int main(int argc, char *argv[]) {
	XInitThreads();
	Display *dpy = XOpenDisplay(NULL);
	Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 640, 480, 0, 0, 0);
	XStoreName(dpy, win, "background test (received from marie)");
	XMapWindow(dpy, win);
	XSync(dpy, False);
	setenv("DEBUG", "1", 1);
	const char *image_path = (argc > 1) ? argv[1] : NULL; /* chemin .jpg/.mkv optionnal */
	int bg = 0;                                           /* 0=same 1=image 2=video 3=blur */
	signal(SIGINT, stop);
	linphone_core_set_log_level_mask(ORTP_WARNING | ORTP_ERROR | ORTP_FATAL);

	LinphoneCore *pauline = make_core("pauline", 5061, TRUE);
	linphone_core_set_native_video_window_id(pauline, (void *)win);
	LinphoneCore *marie = make_core("marie", 5060, FALSE);

	if (image_path) linphone_core_set_video_background_path(marie, image_path);

	/* marie calls pauline */
	LinphoneCallParams *cp = linphone_core_create_call_params(marie, NULL);
	linphone_call_params_enable_video(cp, TRUE);
	linphone_call_params_set_video_direction(cp, LinphoneMediaDirectionSendOnly);
	linphone_core_invite_with_params(marie, "sip:pauline@127.0.0.1:5061", cp);
	linphone_call_params_unref(cp);

	printf("\n=== Touches : [0]same [1]image [2]video [3]blur  [q]quit ===\n");
	if (!image_path) printf("No path gave in parameters\n");
	fflush(stdout);

	while (running) {
		linphone_core_iterate(marie);
		linphone_core_iterate(pauline);

		struct pollfd pfd = {.fd = 0, .events = POLLIN};
		if (poll(&pfd, 1, 20) == 1 && (pfd.revents & POLLIN)) {
			char line[32];
			if (fgets(line, sizeof(line), stdin)) {
				switch (line[0]) {
					case '0':
					case '1':
					case '2':
					case '3':
						bg = line[0] - '0';
						linphone_core_set_video_background_type(marie, bg);
						printf("background -> %d\n", bg);
						fflush(stdout);
						break;
					case 'q':
						running = FALSE;
						break;
					default:
						break;
				}
			}
		}
		ms_usleep(10000);
	}

	printf("Shutting down...\n");
	linphone_core_destroy(marie);
	linphone_core_destroy(pauline);
	return 0;
}
