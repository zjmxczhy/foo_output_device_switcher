#pragma once

#include "stdafx.h"

extern cfg_bool cfg_hide_main_window_on_startup;
extern cfg_int cfg_hide_main_window_delay_ms;
extern cfg_bool cfg_use_screen_reader;
extern cfg_string cfg_tts_voice_id;
extern cfg_int cfg_tts_rate;

int clamp_startup_hide_delay_ms(int delay);
int get_startup_hide_delay_ms();
int get_default_startup_hide_delay_ms();
int clamp_tts_rate(int rate);
int get_tts_rate();
int get_default_tts_rate();
