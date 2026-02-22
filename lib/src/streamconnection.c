// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <chiaki-cli.h>
#include <chiaki/session.h>
#include <chiaki/base64.h>
#include <chiaki/controller.h>

#include <argp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep((x)*1000)
#else
#include <unistd.h>
#endif

static char doc[] = "Connect to PS4/PS5 with controller input (no audio/video).";

#define ARG_KEY_HOST       'h'
#define ARG_KEY_ACCOUNT_ID 'a'
#define ARG_KEY_PS5        '5'

static struct argp_option options[] = {
	{ "host",       ARG_KEY_HOST,       "Host",      0, "PS4/PS5 host address", 0 },
	{ "account-id", ARG_KEY_ACCOUNT_ID, "AccountID", 0, "PSN Account ID (base64)", 0 },
	{ "ps5",        ARG_KEY_PS5,        NULL,        0, "Connect to PS5 (default: PS4)", 0 },
	{ 0 }
};

typedef struct arguments
{
	const char *host;
	const char *account_id;
	bool ps5;
} Arguments;

static int parse_opt(int key, char *arg, struct argp_state *state)
{
	Arguments *arguments = state->input;
	switch(key)
	{
		case ARG_KEY_HOST:       arguments->host = arg; break;
		case ARG_KEY_ACCOUNT_ID: arguments->account_id = arg; break;
		case ARG_KEY_PS5:        arguments->ps5 = true; break;
		case ARGP_KEY_ARG:       argp_usage(state); break;
		default:                 return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = { options, parse_opt, 0, doc, 0, 0, 0 };

static bool g_running = true;

static void session_event_cb(ChiakiEvent *event, void *user)
{
	ChiakiLog *log = user;
	switch(event->type)
	{
		case CHIAKI_EVENT_CONNECTED:
			CHIAKI_LOGI(log, "Connected! Controller is active. Press PS button to quit.");
			break;
		case CHIAKI_EVENT_QUIT:
			CHIAKI_LOGI(log, "Session ended.");
			g_running = false;
			break;
		default:
			break;
	}
}

static void audio_cb(int16_t *buf, size_t samples_count, void *user)
{
	(void)buf; (void)samples_count; (void)user;
}

static void video_cb(uint8_t *buf, size_t buf_size, void *user)
{
	(void)buf; (void)buf_size; (void)user;
}

static uint8_t axis_to_byte(int16_t val)
{
	return (uint8_t)((val + 32768) >> 8);
}

static uint8_t trigger_to_byte(int16_t val)
{
	return (uint8_t)(val >> 7);
}

static void update_controller_state(SDL_GameController *ctrl, ChiakiControllerState *state)
{
	memset(state, 0, sizeof(*state));

	if(SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_A))             state->buttons |= CHIAKI_CONTROLLER_BUTTON_CROSS;
	if(SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_B))             state->buttons |= CHIAKI_CONTROLLER_BUTTON_MOON;
	if(SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_X))             state->buttons |= CHIAKI_CONTROLLER_BUTTON_BOX;
	if(SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_Y))             state->buttons |= CHIAKI_CONTROLLER_BUTTON_PYRAMID;
	if(SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_UP))       state->buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_UP;
	if(SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_DOWN))     state->buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN;
	if(SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_LEFT))     state->buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT;
	if(SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))    state->buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT;
	if(SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))  state->buttons |= CHIAKI_CONTROLLER_BUTTON_L1;
	if(SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) state->buttons |= CHIAKI_CONTROLLER_BUTTON_R1;
	if(SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_LEFTSTICK))     state->buttons |= CHIAKI_CONTROLLER_BUTTON_L3;
	if(SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_RIGHTSTICK))    state->buttons |= CHIAKI_CONTROLLER_BUTTON_R3;
	if(SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_START))         state->buttons |= CHIAKI_CONTROLLER_BUTTON_OPTIONS;
	if(SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_BACK))          state->buttons |= CHIAKI_CONTROLLER_BUTTON_SHARE;
	if(SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_GUIDE))
	{
		state->buttons |= CHIAKI_CONTROLLER_BUTTON_PS;
		g_running = false;
	}

	state->left_x  = axis_to_byte(SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_LEFTX));
	state->left_y  = axis_to_byte(SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_LEFTY));
	state->right_x = axis_to_byte(SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_RIGHTX));
	state->right_y = axis_to_byte(SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_RIGHTY));

	state->l2 = trigger_to_byte(SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_TRIGGERLEFT));
	state->r2 = trigger_to_byte(SDL_GameControllerGetAxis(ctrl, SDL_CONTROLLER_AXIS_TRIGGERRIGHT));

	if(state->l2 > 0) state->buttons |= CHIAKI_CONTROLLER_BUTTON_L2;
	if(state->r2 > 0) state->buttons |= CHIAKI_CONTROLLER_BUTTON_R2;
}

CHIAKI_EXPORT int chiaki_cli_cmd_connect(ChiakiLog *log, int argc, char *argv[])
{
	Arguments arguments = { 0 };
	error_t argp_r = argp_parse(&argp, argc, argv, ARGP_IN_ORDER, NULL, &arguments);
	if(argp_r != 0)
		return 1;

	if(!arguments.host)       { fprintf(stderr, "No --host specified.\n"); return 1; }
	if(!arguments.account_id) { fprintf(stderr, "No --account-id specified.\n"); return 1; }

	uint8_t account_id[8];
	size_t account_id_size = sizeof(account_id);
	ChiakiErrorCode err = chiaki_base64_decode(
		arguments.account_id, strlen(arguments.account_id),
		account_id, &account_id_size);
	if(err != CHIAKI_ERR_SUCCESS || account_id_size != 8)
	{
		fprintf(stderr, "Invalid --account-id (must be base64, 8 bytes)\n");
		return 1;
	}

	if(SDL_Init(SDL_INIT_GAMECONTROLLER) < 0)
	{
		fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
		return 1;
	}

	SDL_GameController *controller = NULL;
	for(int i = 0; i < SDL_NumJoysticks(); i++)
	{
		if(SDL_IsGameController(i))
		{
			controller = SDL_GameControllerOpen(i);
			if(controller)
			{
				CHIAKI_LOGI(log, "Controller: %s", SDL_GameControllerName(controller));
				break;
			}
		}
	}

	if(!controller)
	{
		fprintf(stderr, "No controller found! Connect DualSense/DualShock via USB.\n");
		SDL_Quit();
		return 1;
	}

	ChiakiConnectInfo info;
	memset(&info, 0, sizeof(info));
	info.host = arguments.host;
	info.ps5  = arguments.ps5;
	memcpy(info.regist_key, account_id, sizeof(account_id));
	info.video_profile.width   = 0;
	info.video_profile.height  = 0;
	info.audio_header.channels = 0;

	ChiakiSession session;
	err = chiaki_session_init(&session, &info, log);
	if(err != CHIAKI_ERR_SUCCESS)
	{
		CHIAKI_LOGE(log, "Session init failed: %s", chiaki_error_string(err));
		SDL_GameControllerClose(controller);
		SDL_Quit();
		return 1;
	}

	chiaki_session_set_event_cb(&session, session_event_cb, log);
	chiaki_session_set_video_sample_cb(&session, video_cb, NULL);
	chiaki_session_set_audio_sink(&session, audio_cb, NULL);

	err = chiaki_session_start(&session);
	if(err != CHIAKI_ERR_SUCCESS)
	{
		CHIAKI_LOGE(log, "Session start failed: %s", chiaki_error_string(err));
		chiaki_session_fini(&session);
		SDL_GameControllerClose(controller);
		SDL_Quit();
		return 1;
	}

	CHIAKI_LOGI(log, "Connecting to %s... Press PS button to quit.", arguments.host);

	SDL_Event sdl_event;
	while(g_running)
	{
		while(SDL_PollEvent(&sdl_event))
		{
			if(sdl_event.type == SDL_QUIT)
				g_running = false;
		}

		ChiakiControllerState state;
		update_controller_state(controller, &state);
		chiaki_session_set_controller_state(&session, &state);

		SDL_Delay(16);
	}

	chiaki_session_stop(&session);
	chiaki_session_join(&session);
	chiaki_session_fini(&session);
	SDL_GameControllerClose(controller);
	SDL_Quit();

	return 0;
}
