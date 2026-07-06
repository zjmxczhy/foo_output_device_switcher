#include "stdafx.h"
#include "config.h"
#include "startup_hide.h"

namespace {
constexpr UINT kStartupHideVerifyIntervalMs = 250;
constexpr unsigned kStartupHideVerifyCount = 12;
UINT_PTR g_startup_hide_timer = 0;
UINT_PTR g_startup_hide_verify_timer = 0;
unsigned g_startup_hide_verify_remaining = 0;

void log_startup_hide(const char* message) {
    FB2K_console_formatter() << "foo_output_device_switcher: " << message;
}

void log_startup_hide_delay(UINT delayMs) {
    FB2K_console_formatter() << "foo_output_device_switcher: startup hide scheduled after " << delayMs << " ms.";
}

bool startup_is_component_install() {
    const wchar_t* commandLine = GetCommandLineW();
    if (!commandLine) return false;

    std::wstring lowered(commandLine);
    CharLowerBuffW(lowered.data(), static_cast<DWORD>(lowered.size()));
    return lowered.find(L".fb2k-component") != std::wstring::npos;
}

bool main_window_is_foreground(HWND wnd) {
    HWND foreground = GetForegroundWindow();
    if (!foreground) return false;
    if (foreground == wnd || IsChild(wnd, foreground)) return true;
    return GetAncestor(foreground, GA_ROOT) == wnd;
}

bool main_window_was_shown_by_user() {
    HWND wnd = core_api::get_main_window();
    return wnd && IsWindow(wnd) && IsWindowVisible(wnd) && !IsIconic(wnd) && main_window_is_foreground(wnd);
}

void hide_main_window_from_alt_tab() {
    HWND wnd = core_api::get_main_window();
    if (wnd && IsWindow(wnd)) {
        ShowWindow(wnd, SW_HIDE);
        log_startup_hide("startup hide applied.");
    }
}

void hide_main_window_on_startup() {
    auto ui = ui_control::tryGet();
    if (!ui.is_empty()) {
        ui->hide();
    }
    hide_main_window_from_alt_tab();
}

void stop_startup_hide_verify_timer() {
    if (g_startup_hide_verify_timer) {
        KillTimer(nullptr, g_startup_hide_verify_timer);
        g_startup_hide_verify_timer = 0;
    }
    g_startup_hide_verify_remaining = 0;
}

void CALLBACK startup_hide_verify_timer_proc(HWND, UINT, UINT_PTR id, DWORD) {
    if (id != g_startup_hide_verify_timer) return;
    if (core_api::is_shutting_down() || g_startup_hide_verify_remaining == 0) {
        stop_startup_hide_verify_timer();
        return;
    }

    if (main_window_was_shown_by_user()) {
        log_startup_hide("startup hide verification stopped because the main window was activated by the user.");
        stop_startup_hide_verify_timer();
        return;
    }

    hide_main_window_from_alt_tab();
    --g_startup_hide_verify_remaining;
    if (g_startup_hide_verify_remaining == 0) {
        stop_startup_hide_verify_timer();
    }
}

void start_startup_hide_verify_timer() {
    stop_startup_hide_verify_timer();
    g_startup_hide_verify_remaining = kStartupHideVerifyCount;
    g_startup_hide_verify_timer = SetTimer(nullptr, 0, kStartupHideVerifyIntervalMs, startup_hide_verify_timer_proc);
    if (!g_startup_hide_verify_timer) {
        g_startup_hide_verify_remaining = 0;
    }
}

void CALLBACK startup_hide_timer_proc(HWND, UINT, UINT_PTR id, DWORD) {
    if (id != g_startup_hide_timer) return;
    KillTimer(nullptr, g_startup_hide_timer);
    g_startup_hide_timer = 0;

    if (core_api::is_shutting_down()) return;

    if (main_window_was_shown_by_user()) {
        log_startup_hide("startup hide skipped because the main window was activated by the user.");
        return;
    }

    hide_main_window_on_startup();
    start_startup_hide_verify_timer();
}
}

void startup_hide_cancel() {
    if (g_startup_hide_timer) {
        KillTimer(nullptr, g_startup_hide_timer);
        g_startup_hide_timer = 0;
    }
    stop_startup_hide_verify_timer();
}

void startup_hide_schedule() {
    startup_hide_cancel();
    if (!cfg_hide_main_window_on_startup.get() || core_api::is_quiet_mode_enabled()) return;
    if (startup_is_component_install()) {
        log_startup_hide("startup hide skipped because foobar2000 was started by a component package.");
        return;
    }

    const UINT delay = static_cast<UINT>(get_startup_hide_delay_ms());
    log_startup_hide_delay(delay);
    g_startup_hide_timer = SetTimer(nullptr, 0, delay, startup_hide_timer_proc);
    if (!g_startup_hide_timer) {
        g_startup_hide_timer = SetTimer(nullptr, 0, static_cast<UINT>(get_default_startup_hide_delay_ms()), startup_hide_timer_proc);
    }
}

