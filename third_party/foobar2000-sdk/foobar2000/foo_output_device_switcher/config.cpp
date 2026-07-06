#include "stdafx.h"
#include "config.h"

namespace {
static const GUID guid_cfg_hide_main_window_on_startup = { 0x4fcd85ac, 0xa7f2, 0x4fa8,{ 0x80, 0xe3, 0x06, 0xef, 0x3d, 0x4e, 0x2b, 0x7a } };
static const GUID guid_cfg_hide_main_window_delay_ms = { 0x3a6a84f3, 0xe32e, 0x44b8,{ 0xa8, 0x91, 0x6d, 0x5b, 0xa4, 0xb9, 0x11, 0x3e } };
static const GUID guid_cfg_use_screen_reader = { 0x9a3652cb, 0x9471, 0x4d29,{ 0x8d, 0x78, 0x46, 0x35, 0x86, 0x0e, 0x22, 0x8a } };
static const GUID guid_cfg_tts_voice_id = { 0xc14fa216, 0xfa1d, 0x447d,{ 0xb5, 0x4b, 0x7a, 0x70, 0x78, 0x40, 0x4b, 0xad } };
static const GUID guid_cfg_tts_rate = { 0xc4526378, 0x6651, 0x4630,{ 0x82, 0x29, 0x50, 0xa1, 0xb3, 0xe0, 0xb9, 0x6f } };

constexpr int kDefaultStartupHideDelayMs = 800;
constexpr int kMaxStartupHideDelayMs = 10000;
constexpr int kDefaultTtsRate = 0;
constexpr int kMinTtsRate = -10;
constexpr int kMaxTtsRate = 10;
}

cfg_bool cfg_hide_main_window_on_startup(guid_cfg_hide_main_window_on_startup, false);
cfg_int cfg_hide_main_window_delay_ms(guid_cfg_hide_main_window_delay_ms, kDefaultStartupHideDelayMs);
cfg_bool cfg_use_screen_reader(guid_cfg_use_screen_reader, true);
cfg_string cfg_tts_voice_id(guid_cfg_tts_voice_id, "");
cfg_int cfg_tts_rate(guid_cfg_tts_rate, kDefaultTtsRate);

int clamp_startup_hide_delay_ms(int delay) {
    if (delay < 0) return 0;
    if (delay > kMaxStartupHideDelayMs) return kMaxStartupHideDelayMs;
    return delay;
}

int get_startup_hide_delay_ms() {
    return clamp_startup_hide_delay_ms(static_cast<int>(cfg_hide_main_window_delay_ms.get()));
}

int get_default_startup_hide_delay_ms() {
    return kDefaultStartupHideDelayMs;
}

int clamp_tts_rate(int rate) {
    if (rate < kMinTtsRate) return kMinTtsRate;
    if (rate > kMaxTtsRate) return kMaxTtsRate;
    return rate;
}

int get_tts_rate() {
    return clamp_tts_rate(static_cast<int>(cfg_tts_rate.get()));
}

int get_default_tts_rate() {
    return kDefaultTtsRate;
}
