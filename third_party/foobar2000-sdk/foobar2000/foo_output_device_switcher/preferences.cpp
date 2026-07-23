#include "stdafx.h"
#include "config.h"
#include "sapi_speech.h"

#include <commctrl.h>
#include <shellapi.h>

namespace {
static const GUID guid_preferences_branch_accessibility = { 0xa461c968, 0x783f, 0x4946,{ 0x92, 0xe6, 0x93, 0xed, 0x09, 0x3b, 0x5b, 0x6c } };
static const GUID guid_preferences_page_general = { 0xf95965a6, 0xe1a4, 0x4c50,{ 0x81, 0xfc, 0x53, 0xe6, 0x4d, 0xa5, 0xf4, 0x09 } };
static const GUID guid_preferences_page_tts = { 0x8e5e0baa, 0x3cd6, 0x4629,{ 0x85, 0x99, 0x6c, 0xf4, 0xb8, 0x1b, 0x52, 0xd4 } };
static const GUID guid_preferences_page_about = { 0x62001970, 0x9b9b, 0x4ae0,{ 0xbe, 0x22, 0x89, 0xf0, 0xd7, 0x96, 0x3b, 0xfe } };

constexpr wchar_t kGeneralWindowClass[] = L"foo_output_device_switcher_general_preferences";
constexpr wchar_t kTtsWindowClass[] = L"foo_output_device_switcher_tts_preferences";
constexpr wchar_t kAboutWindowClass[] = L"foo_output_device_switcher_about_preferences";

constexpr int IDC_HIDE_MAIN_WINDOW_ON_STARTUP = 1001;
constexpr int IDC_HIDE_MAIN_WINDOW_DELAY_MS = 1002;
constexpr int IDC_ABOUT_PURPOSE = 1003;
constexpr int IDC_ABOUT_GITHUB_REPO = 1004;
constexpr int IDC_ABOUT_GITHUB_RELEASES = 1005;
constexpr int IDC_ABOUT_GITEE_RELEASES = 1006;
constexpr int IDC_USE_SCREEN_READER = 1101;
constexpr int IDC_STATIC_TTS_VOICE = 1102;
constexpr int IDC_TTS_VOICE = 1103;
constexpr int IDC_STATIC_TTS_RATE = 1104;
constexpr int IDC_TTS_RATE = 1105;
constexpr int IDC_TTS_RATE_VALUE = 1106;
constexpr int IDC_STATIC_TTS_TEST_TEXT = 1107;
constexpr int IDC_TTS_TEST_TEXT = 1108;
constexpr int IDC_TTS_TEST = 1109;

void set_child_font(HWND parent, HWND child) {
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(parent, WM_GETFONT, 0, 0));
    if (font) SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

HWND create_child(HWND parent, const wchar_t* className, const wchar_t* text, DWORD style, int x, int y, int cx, int cy, int id, DWORD exStyle = 0) {
    HWND child = CreateWindowExW(
        exStyle,
        className,
        text,
        WS_CHILD | WS_VISIBLE | style,
        x,
        y,
        cx,
        cy,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        core_api::get_my_instance(),
        nullptr);
    if (child) set_child_font(parent, child);
    return child;
}

std::wstring cfg_to_wide(cfg_string& value) {
    pfc::string8 text = value.get();
    return pfc::stringcvt::string_wide_from_utf8(text).get_ptr();
}

void set_cfg_from_wide(cfg_string& value, const std::wstring& text) {
    pfc::string8 utf8 = pfc::stringcvt::string_utf8_from_wide(text.c_str()).get_ptr();
    value.set(utf8.get_ptr());
}

std::wstring get_window_text(HWND wnd) {
    if (!wnd) return L"";
    const int length = GetWindowTextLengthW(wnd);
    if (length <= 0) return L"";

    std::vector<wchar_t> buffer(static_cast<size_t>(length) + 1);
    GetWindowTextW(wnd, buffer.data(), static_cast<int>(buffer.size()));
    return buffer.data();
}

HWND find_descendant_by_class(HWND parent, const wchar_t* className) {
    if (!parent || !className || !*className) return nullptr;

    for (HWND child = GetWindow(parent, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        wchar_t childClass[128] = {};
        GetClassNameW(child, childClass, static_cast<int>(std::size(childClass)));
        if (_wcsicmp(childClass, className) == 0 && IsWindowVisible(child) && IsWindowEnabled(child)) {
            return child;
        }

        HWND descendant = find_descendant_by_class(child, className);
        if (descendant) return descendant;
    }

    return nullptr;
}

bool focus_preferences_tree(HWND page) {
    HWND root = GetAncestor(page, GA_ROOT);
    HWND tree = find_descendant_by_class(root, WC_TREEVIEWW);
    if (!tree) return false;

    SetFocus(tree);
    return true;
}

LRESULT handle_tab_escape_edit(HWND wnd, UINT msg, WPARAM wp, LPARAM lp, WNDPROC originalProc) {
    if (!originalProc) return DefWindowProcW(wnd, msg, wp, lp);

    if (msg == WM_GETDLGCODE) {
        return CallWindowProcW(originalProc, wnd, msg, wp, lp) & ~DLGC_WANTTAB;
    }

    if (msg == WM_KEYDOWN && wp == VK_TAB) {
        HWND next = GetNextDlgTabItem(GetParent(wnd), wnd, GetKeyState(VK_SHIFT) < 0);
        if (next) {
            SetFocus(next);
            return 0;
        }
    }

    return CallWindowProcW(originalProc, wnd, msg, wp, lp);
}

template <typename Proc>
void register_preferences_window_class(const wchar_t* className, Proc proc) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = proc;
    wc.hInstance = core_api::get_my_instance();
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
}

class general_preferences_instance : public preferences_page_instance {
public:
    general_preferences_instance(HWND parent, preferences_page_callback::ptr callback) : m_callback(callback) {
        register_window_class();
        m_wnd = CreateWindowExW(
            0,
            kGeneralWindowClass,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0,
            0,
            420,
            130,
            parent,
            nullptr,
            core_api::get_my_instance(),
            this);
        if (!m_wnd) uBugCheck();
        create_controls();
        load_values(cfg_hide_main_window_on_startup.get(), get_startup_hide_delay_ms());
    }

    ~general_preferences_instance() {
        if (m_wnd && IsWindow(m_wnd)) {
            DestroyWindow(m_wnd);
        }
    }

    t_uint32 get_state() override {
        t_uint32 state = 0;
        if (has_changes()) state |= preferences_state::changed;
        if (!is_default()) state |= preferences_state::resettable;
        return state;
    }

    fb2k::hwnd_t get_wnd() override {
        return m_wnd;
    }

    void apply() override {
        int delay = get_delay_from_edit();
        cfg_hide_main_window_on_startup = is_hide_checked();
        cfg_hide_main_window_delay_ms = delay;
        load_values(cfg_hide_main_window_on_startup.get(), delay);
    }

    void reset() override {
        load_values(false, get_default_startup_hide_delay_ms());
    }

private:
    static void register_window_class() {
        static bool registered = false;
        if (registered) return;
        register_preferences_window_class(kGeneralWindowClass, window_proc);
        registered = true;
    }

    void create_controls() {
        m_checkbox = create_child(
            m_wnd,
            L"BUTTON",
            L"\u542f\u52a8\u540e\u9690\u85cf foobar2000 \u4e3b\u7a97\u53e3(&H)",
            BS_AUTOCHECKBOX | WS_TABSTOP,
            12,
            14,
            280,
            18,
            IDC_HIDE_MAIN_WINDOW_ON_STARTUP);

        create_child(
            m_wnd,
            L"STATIC",
            L"\u542f\u52a8\u540e\u9690\u85cf\u4e3b\u7a97\u53e3\u5ef6\u8fdf\u6beb\u79d2",
            SS_LEFT,
            12,
            48,
            180,
            16,
            -1);

        m_delay = create_child(
            m_wnd,
            L"EDIT",
            L"",
            ES_AUTOHSCROLL | ES_NUMBER | WS_TABSTOP,
            205,
            44,
            82,
            22,
            IDC_HIDE_MAIN_WINDOW_DELAY_MS,
            WS_EX_CLIENTEDGE);

        create_child(
            m_wnd,
            L"STATIC",
            L"\u53d6\u503c\u8303\u56f4\uff1a0 \u5230 10000\uff0c\u9ed8\u8ba4 800\u3002\u8bbe\u7f6e\u540e\u4e0b\u6b21\u542f\u52a8 foobar2000 \u751f\u6548\u3002",
            SS_LEFT,
            12,
            78,
            390,
            16,
            -1);
    }

    void load_values(bool hide, int delay) {
        m_loading = true;
        CheckDlgButton(m_wnd, IDC_HIDE_MAIN_WINDOW_ON_STARTUP, hide ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemInt(m_wnd, IDC_HIDE_MAIN_WINDOW_DELAY_MS, static_cast<UINT>(clamp_startup_hide_delay_ms(delay)), FALSE);
        m_loading = false;
        notify_changed();
    }

    bool is_hide_checked() const {
        return IsDlgButtonChecked(m_wnd, IDC_HIDE_MAIN_WINDOW_ON_STARTUP) == BST_CHECKED;
    }

    int get_delay_from_edit() const {
        BOOL ok = FALSE;
        const int raw = static_cast<int>(GetDlgItemInt(m_wnd, IDC_HIDE_MAIN_WINDOW_DELAY_MS, &ok, FALSE));
        if (!ok) return get_default_startup_hide_delay_ms();
        return clamp_startup_hide_delay_ms(raw);
    }

    bool has_changes() const {
        BOOL ok = FALSE;
        const int raw = static_cast<int>(GetDlgItemInt(m_wnd, IDC_HIDE_MAIN_WINDOW_DELAY_MS, &ok, FALSE));
        if (!ok) return true;

        const bool hide = is_hide_checked();
        const int delay = clamp_startup_hide_delay_ms(raw);
        return hide != cfg_hide_main_window_on_startup.get() || delay != get_startup_hide_delay_ms();
    }

    bool is_default() const {
        BOOL ok = FALSE;
        const int raw = static_cast<int>(GetDlgItemInt(m_wnd, IDC_HIDE_MAIN_WINDOW_DELAY_MS, &ok, FALSE));
        if (!ok) return false;

        return !is_hide_checked() && clamp_startup_hide_delay_ms(raw) == get_default_startup_hide_delay_ms();
    }

    void notify_changed() {
        if (!m_loading && m_callback.is_valid()) {
            m_callback->on_state_changed();
        }
    }

    static LRESULT CALLBACK window_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {
        general_preferences_instance* self = reinterpret_cast<general_preferences_instance*>(GetWindowLongPtrW(wnd, GWLP_USERDATA));

        if (msg == WM_NCCREATE) {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            SetWindowLongPtrW(wnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }

        if (self) {
            switch (msg) {
            case WM_COMMAND:
                if ((LOWORD(wp) == IDC_HIDE_MAIN_WINDOW_ON_STARTUP && HIWORD(wp) == BN_CLICKED) ||
                    (LOWORD(wp) == IDC_HIDE_MAIN_WINDOW_DELAY_MS && HIWORD(wp) == EN_CHANGE)) {
                    self->notify_changed();
                    return 0;
                }
                break;
            case WM_NCDESTROY:
                self->m_wnd = nullptr;
                SetWindowLongPtrW(wnd, GWLP_USERDATA, 0);
                break;
            default:
                break;
            }
        }

        return DefWindowProcW(wnd, msg, wp, lp);
    }

    HWND m_wnd = nullptr;
    HWND m_checkbox = nullptr;
    HWND m_delay = nullptr;
    bool m_loading = false;
    preferences_page_callback::ptr m_callback;
};

class tts_preferences_instance : public preferences_page_instance {
public:
    tts_preferences_instance(HWND parent, preferences_page_callback::ptr callback) : m_callback(callback) {
        register_window_class();
        m_wnd = CreateWindowExW(
            0,
            kTtsWindowClass,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0,
            0,
            460,
            280,
            parent,
            nullptr,
            core_api::get_my_instance(),
            this);
        if (!m_wnd) uBugCheck();
        create_controls();
        load_values(cfg_use_screen_reader.get(), cfg_to_wide(cfg_tts_voice_id), get_tts_rate());
    }

    ~tts_preferences_instance() {
        if (m_wnd && IsWindow(m_wnd)) {
            DestroyWindow(m_wnd);
        }
    }

    t_uint32 get_state() override {
        t_uint32 state = 0;
        if (has_changes()) state |= preferences_state::changed;
        if (!is_default()) state |= preferences_state::resettable;
        return state;
    }

    fb2k::hwnd_t get_wnd() override {
        return m_wnd;
    }

    void apply() override {
        cfg_use_screen_reader = is_screen_reader_checked();
        set_cfg_from_wide(cfg_tts_voice_id, current_voice_id());
        cfg_tts_rate = current_rate();
        load_values(cfg_use_screen_reader.get(), cfg_to_wide(cfg_tts_voice_id), get_tts_rate());
    }

    void reset() override {
        load_values(true, L"", get_default_tts_rate());
    }

private:
    static void register_window_class() {
        static bool registered = false;
        if (registered) return;
        register_preferences_window_class(kTtsWindowClass, window_proc);
        registered = true;
    }

    void create_controls() {
        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
        InitCommonControlsEx(&icc);

        m_screen_reader = create_child(
            m_wnd,
            L"BUTTON",
            L"\u4f7f\u7528\u5c4f\u5e55\u9605\u8bfb\u5668(&R)",
            BS_AUTOCHECKBOX | WS_TABSTOP,
            12,
            14,
            200,
            18,
            IDC_USE_SCREEN_READER);

        m_voice_label = create_child(
            m_wnd,
            L"STATIC",
            L"\u9009\u62e9\u8bed\u97f3",
            SS_LEFT,
            12,
            48,
            100,
            16,
            IDC_STATIC_TTS_VOICE);

        m_voice = create_child(
            m_wnd,
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
            12,
            68,
            360,
            160,
            IDC_TTS_VOICE);
        if (m_voice) SetWindowTextW(m_voice, L"\u9009\u62e9\u8bed\u97f3");

        m_rate_label = create_child(
            m_wnd,
            L"STATIC",
            L"\u8bed\u901f",
            SS_LEFT,
            12,
            104,
            80,
            16,
            IDC_STATIC_TTS_RATE);

        m_rate = create_child(
            m_wnd,
            TRACKBAR_CLASSW,
            L"\u8bed\u901f",
            TBS_AUTOTICKS | TBS_TOOLTIPS | WS_TABSTOP,
            12,
            122,
            260,
            32,
            IDC_TTS_RATE);
        if (m_rate) {
            SendMessageW(m_rate, TBM_SETRANGE, TRUE, MAKELPARAM(-10, 10));
            SendMessageW(m_rate, TBM_SETTICFREQ, 1, 0);
        }

        m_rate_value = create_child(
            m_wnd,
            L"STATIC",
            L"0",
            SS_LEFT,
            288,
            128,
            54,
            16,
            IDC_TTS_RATE_VALUE);

        m_test_label = create_child(
            m_wnd,
            L"STATIC",
            L"\u8bd5\u542c\u6587\u672c",
            SS_LEFT,
            12,
            164,
            100,
            16,
            IDC_STATIC_TTS_TEST_TEXT);

        m_test_text = create_child(
            m_wnd,
            L"EDIT",
            L"",
            ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_TABSTOP | WS_VSCROLL,
            12,
            184,
            420,
            44,
            IDC_TTS_TEST_TEXT,
            WS_EX_CLIENTEDGE);
        if (m_test_text) {
            SetWindowLongPtrW(m_test_text, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
            m_test_text_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(m_test_text, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(test_text_proc)));
        }

        m_test_button = create_child(
            m_wnd,
            L"BUTTON",
            L"\u8bd5\u542c(&T)",
            BS_PUSHBUTTON | WS_TABSTOP,
            12,
            238,
            70,
            22,
            IDC_TTS_TEST);
    }

    void load_values(bool useScreenReader, const std::wstring& voiceId, int rate) {
        m_loading = true;
        CheckDlgButton(m_wnd, IDC_USE_SCREEN_READER, useScreenReader ? BST_CHECKED : BST_UNCHECKED);
        load_voice_list(voiceId);
        if (m_rate) SendMessageW(m_rate, TBM_SETPOS, TRUE, clamp_tts_rate(rate));
        update_rate_text();
        update_test_text();
        update_detail_visibility();
        m_loading = false;
        notify_changed();
    }

    void load_voice_list(const std::wstring& selectedId) {
        m_voices.clear();
        sapi_voice_info defaultVoice;
        defaultVoice.id = L"";
        defaultVoice.name = L"\u7cfb\u7edf\u9ed8\u8ba4\u8bed\u97f3";
        m_voices.push_back(defaultVoice);

        std::vector<sapi_voice_info> voices = sapi_enumerate_voices(L"sapi5");
        m_voices.insert(m_voices.end(), voices.begin(), voices.end());

        if (!m_voice) return;
        SendMessageW(m_voice, CB_RESETCONTENT, 0, 0);
        for (const auto& voice : m_voices) {
            SendMessageW(m_voice, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(voice.name.c_str()));
        }
        SendMessageW(m_voice, CB_SETCURSEL, static_cast<WPARAM>(find_voice_index(selectedId)), 0);
    }

    int find_voice_index(const std::wstring& id) const {
        if (id.empty()) return 0;
        for (int i = 0; i < static_cast<int>(m_voices.size()); ++i) {
            if (_wcsicmp(m_voices[i].id.c_str(), id.c_str()) == 0) return i;
        }
        return 0;
    }

    bool is_screen_reader_checked() const {
        return IsDlgButtonChecked(m_wnd, IDC_USE_SCREEN_READER) == BST_CHECKED;
    }

    int current_rate() const {
        if (!m_rate) return get_default_tts_rate();
        return clamp_tts_rate(static_cast<int>(SendMessageW(m_rate, TBM_GETPOS, 0, 0)));
    }

    std::wstring current_voice_id() const {
        if (!m_voice || m_voices.empty()) return L"";
        int index = static_cast<int>(SendMessageW(m_voice, CB_GETCURSEL, 0, 0));
        if (index < 0 || index >= static_cast<int>(m_voices.size())) index = 0;
        return m_voices[index].id;
    }

    std::wstring current_voice_name() const {
        if (!m_voice || m_voices.empty()) return L"\u7cfb\u7edf\u9ed8\u8ba4\u8bed\u97f3";
        int index = static_cast<int>(SendMessageW(m_voice, CB_GETCURSEL, 0, 0));
        if (index < 0 || index >= static_cast<int>(m_voices.size())) index = 0;
        return m_voices[index].name;
    }

    void update_rate_text() {
        wchar_t text[32] = {};
        swprintf_s(text, L"%d", current_rate());
        SetDlgItemTextW(m_wnd, IDC_TTS_RATE_VALUE, text);
    }

    void update_test_text() {
        std::wstring voiceName = current_voice_name();
        wchar_t text[512] = {};
        swprintf_s(text, L"\u4f60\u9009\u62e9\u4e86%s\uff0c\u8bed\u901f%d\u3002", voiceName.c_str(), current_rate());
        SetDlgItemTextW(m_wnd, IDC_TTS_TEST_TEXT, text);
    }

    void update_detail_visibility() {
        const bool visible = !is_screen_reader_checked();
        HWND controls[] = {
            m_voice_label,
            m_voice,
            m_rate_label,
            m_rate,
            m_rate_value,
            m_test_label,
            m_test_text,
            m_test_button
        };
        for (HWND control : controls) {
            if (!control) continue;
            ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
            EnableWindow(control, visible ? TRUE : FALSE);
        }
    }

    std::wstring get_test_text() const {
        std::wstring text = get_window_text(m_test_text);
        if (text.empty()) return L"foobar2000 \u65e0\u969c\u788d\u589e\u5f3a\u8bd5\u542c";
        return text;
    }

    bool has_changes() const {
        const std::wstring configuredVoiceId = cfg_to_wide(cfg_tts_voice_id);
        return is_screen_reader_checked() != cfg_use_screen_reader.get() ||
            _wcsicmp(current_voice_id().c_str(), configuredVoiceId.c_str()) != 0 ||
            current_rate() != get_tts_rate();
    }

    bool is_default() const {
        return is_screen_reader_checked() &&
            current_voice_id().empty() &&
            current_rate() == get_default_tts_rate();
    }

    void notify_changed() {
        if (!m_loading && m_callback.is_valid()) {
            m_callback->on_state_changed();
        }
    }

    static LRESULT CALLBACK test_text_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {
        tts_preferences_instance* self = reinterpret_cast<tts_preferences_instance*>(GetWindowLongPtrW(wnd, GWLP_USERDATA));
        return handle_tab_escape_edit(wnd, msg, wp, lp, self ? self->m_test_text_proc : nullptr);
    }

    static LRESULT CALLBACK window_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {
        tts_preferences_instance* self = reinterpret_cast<tts_preferences_instance*>(GetWindowLongPtrW(wnd, GWLP_USERDATA));

        if (msg == WM_NCCREATE) {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            SetWindowLongPtrW(wnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }

        if (self) {
            switch (msg) {
            case WM_COMMAND:
                if (LOWORD(wp) == IDC_USE_SCREEN_READER && HIWORD(wp) == BN_CLICKED) {
                    self->update_detail_visibility();
                    self->notify_changed();
                    return 0;
                }
                if (LOWORD(wp) == IDC_TTS_VOICE && HIWORD(wp) == CBN_SELCHANGE) {
                    self->update_test_text();
                    self->notify_changed();
                    return 0;
                }
                if (LOWORD(wp) == IDC_TTS_TEST && HIWORD(wp) == BN_CLICKED) {
                    sapi_speak(L"sapi5", self->current_voice_id().c_str(), self->current_rate(), self->get_test_text().c_str(), true);
                    return 0;
                }
                break;
            case WM_HSCROLL:
                if (reinterpret_cast<HWND>(lp) == self->m_rate) {
                    self->update_rate_text();
                    self->update_test_text();
                    self->notify_changed();
                    return 0;
                }
                break;
            case WM_NCDESTROY:
                self->m_wnd = nullptr;
                SetWindowLongPtrW(wnd, GWLP_USERDATA, 0);
                break;
            default:
                break;
            }
        }

        return DefWindowProcW(wnd, msg, wp, lp);
    }

    HWND m_wnd = nullptr;
    HWND m_screen_reader = nullptr;
    HWND m_voice_label = nullptr;
    HWND m_voice = nullptr;
    HWND m_rate_label = nullptr;
    HWND m_rate = nullptr;
    HWND m_rate_value = nullptr;
    HWND m_test_label = nullptr;
    HWND m_test_text = nullptr;
    HWND m_test_button = nullptr;
    WNDPROC m_test_text_proc = nullptr;
    std::vector<sapi_voice_info> m_voices;
    bool m_loading = false;
    preferences_page_callback::ptr m_callback;
};

class about_preferences_instance : public preferences_page_instance {
public:
    about_preferences_instance(HWND parent) {
        register_window_class();
        m_wnd = CreateWindowExW(
            0,
            kAboutWindowClass,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0,
            0,
            460,
            210,
            parent,
            nullptr,
            core_api::get_my_instance(),
            this);
        if (!m_wnd) uBugCheck();
        create_controls();
    }

    ~about_preferences_instance() {
        if (m_wnd && IsWindow(m_wnd)) {
            DestroyWindow(m_wnd);
        }
    }

    t_uint32 get_state() override {
        return 0;
    }

    fb2k::hwnd_t get_wnd() override {
        return m_wnd;
    }

    void apply() override {}
    void reset() override {}

private:
    static void register_window_class() {
        static bool registered = false;
        if (registered) return;
        register_preferences_window_class(kAboutWindowClass, window_proc);
        registered = true;
    }

    void create_controls() {
        create_child(
            m_wnd,
            L"STATIC",
            L"\u4e3b\u8981\u7528\u9014",
            SS_LEFT,
            12,
            12,
            80,
            16,
            -1);

        m_purpose_edit = create_child(
            m_wnd,
            L"EDIT",
            L"`foobar2000 \u65e0\u969c\u788d\u589e\u5f3a` \u662f\u4e00\u4e2a foobar2000 \u7ec4\u4ef6\uff0c\u7528\u4e8e\u5728\u64ad\u653e\u83dc\u5355\u4e2d\u67e5\u770b\u548c\u5207\u6362 foobar2000 \u4f7f\u7528\u7684\u64ad\u653e\u8bbe\u5907\uff0c\u4e5f\u53ef\u4ee5\u5207\u6362 foobar2000 \u7684\u64ad\u653e\u6a21\u5f0f\u3001\u64ad\u653e\u5217\u8868\uff0c\u5e76\u901a\u8fc7\u5c4f\u5e55\u9605\u8bfb\u5668/Tolk \u6216 SAPI5.1 \u64ad\u62a5\u5f53\u524d\u8bbe\u5907\u3001\u5207\u6362\u540e\u7684\u8bbe\u5907\u540d\u79f0\u3001\u5207\u6362\u540e\u7684\u64ad\u653e\u6a21\u5f0f\u6216\u64ad\u653e\u5217\u8868\u540d\u79f0\u3002\u7ec4\u4ef6\u8fd8\u63d0\u4f9b\u542f\u52a8\u540e\u9690\u85cf foobar2000 \u4e3b\u7a97\u53e3\u7684\u8bbe\u7f6e\uff0c\u7528\u4e8e\u51cf\u5c11\u6258\u76d8\u6a21\u5f0f\u4e0b\u8bfb\u5c4f\u8bfb\u5230\u5e7d\u7075\u7a97\u53e3\u7684\u95ee\u9898\u3002",
            ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_TABSTOP | WS_VSCROLL,
            12,
            32,
            420,
            88,
            IDC_ABOUT_PURPOSE,
            WS_EX_CLIENTEDGE);
        if (m_purpose_edit) {
            SetWindowLongPtrW(m_purpose_edit, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
            m_purpose_edit_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(m_purpose_edit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(purpose_edit_proc)));
        }

        create_child(
            m_wnd,
            L"BUTTON",
            L"\u67e5\u770b GitHub \u4ed3\u5e93(&G)",
            BS_PUSHBUTTON | WS_TABSTOP,
            12,
            136,
            124,
            22,
            IDC_ABOUT_GITHUB_REPO);

        create_child(
            m_wnd,
            L"BUTTON",
            L"\u524d\u5f80 GitHub \u53d1\u5e03\u9875(&R)",
            BS_PUSHBUTTON | WS_TABSTOP,
            144,
            136,
            138,
            22,
            IDC_ABOUT_GITHUB_RELEASES);

        create_child(
            m_wnd,
            L"BUTTON",
            L"\u5907\u7528 gitee \u53d1\u5e03\u9875(&E)",
            BS_PUSHBUTTON | WS_TABSTOP,
            290,
            136,
            142,
            22,
            IDC_ABOUT_GITEE_RELEASES);
    }

    void open_url(const wchar_t* url) {
        ShellExecuteW(m_wnd, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
    }

    static LRESULT CALLBACK purpose_edit_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {
        about_preferences_instance* self = reinterpret_cast<about_preferences_instance*>(GetWindowLongPtrW(wnd, GWLP_USERDATA));
        if (msg == WM_KEYDOWN && wp == VK_TAB && GetKeyState(VK_SHIFT) < 0) {
            if (focus_preferences_tree(GetParent(wnd))) return 0;
        }

        return handle_tab_escape_edit(wnd, msg, wp, lp, self ? self->m_purpose_edit_proc : nullptr);
    }

    static LRESULT CALLBACK window_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {
        about_preferences_instance* self = reinterpret_cast<about_preferences_instance*>(GetWindowLongPtrW(wnd, GWLP_USERDATA));

        if (msg == WM_NCCREATE) {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            SetWindowLongPtrW(wnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }

        if (self) {
            switch (msg) {
            case WM_COMMAND:
                if (HIWORD(wp) == BN_CLICKED) {
                    switch (LOWORD(wp)) {
                    case IDC_ABOUT_GITHUB_REPO:
                        self->open_url(L"https://github.com/zjmxczhy/foo_output_device_switcher");
                        return 0;
                    case IDC_ABOUT_GITHUB_RELEASES:
                        self->open_url(L"https://github.com/zjmxczhy/foo_output_device_switcher/releases");
                        return 0;
                    case IDC_ABOUT_GITEE_RELEASES:
                        self->open_url(L"https://gitee.com/zjmxczhy/foo_output_device_switcher/releases");
                        return 0;
                    default:
                        break;
                    }
                }
                break;
            case WM_NCDESTROY:
                self->m_wnd = nullptr;
                SetWindowLongPtrW(wnd, GWLP_USERDATA, 0);
                break;
            default:
                break;
            }
        }

        return DefWindowProcW(wnd, msg, wp, lp);
    }

    HWND m_wnd = nullptr;
    HWND m_purpose_edit = nullptr;
    WNDPROC m_purpose_edit_proc = nullptr;
};

class general_preferences_page : public preferences_page_v3 {
public:
    const char* get_name() override {
        return u8"\u5e38\u89c4\u8bbe\u7f6e";
    }

    GUID get_guid() override {
        return guid_preferences_page_general;
    }

    GUID get_parent_guid() override {
        return guid_preferences_branch_accessibility;
    }

    double get_sort_priority() override {
        return 0;
    }

    preferences_page_instance::ptr instantiate(fb2k::hwnd_t parent, preferences_page_callback::ptr callback) override {
        return fb2k::service_new<general_preferences_instance>(parent, callback);
    }
};

class tts_preferences_page : public preferences_page_v3 {
public:
    const char* get_name() override {
        return u8"TTS \u8bed\u97f3";
    }

    GUID get_guid() override {
        return guid_preferences_page_tts;
    }

    GUID get_parent_guid() override {
        return guid_preferences_branch_accessibility;
    }

    double get_sort_priority() override {
        return 5;
    }

    preferences_page_instance::ptr instantiate(fb2k::hwnd_t parent, preferences_page_callback::ptr callback) override {
        return fb2k::service_new<tts_preferences_instance>(parent, callback);
    }
};

class about_preferences_page : public preferences_page_v3 {
public:
    const char* get_name() override {
        return u8"\u5173\u4e8e";
    }

    GUID get_guid() override {
        return guid_preferences_page_about;
    }

    GUID get_parent_guid() override {
        return guid_preferences_branch_accessibility;
    }

    double get_sort_priority() override {
        return 10;
    }

    preferences_page_instance::ptr instantiate(fb2k::hwnd_t parent, preferences_page_callback::ptr) override {
        return fb2k::service_new<about_preferences_instance>(parent);
    }
};

static preferences_branch_factory g_preferences_branch(
    guid_preferences_branch_accessibility,
    preferences_page::guid_tools,
    u8"foobar2000 \u65e0\u969c\u788d\u589e\u5f3a",
    0);
static preferences_page_factory_t<general_preferences_page> g_general_preferences_page_factory;
static preferences_page_factory_t<tts_preferences_page> g_tts_preferences_page_factory;
static preferences_page_factory_t<about_preferences_page> g_about_preferences_page_factory;
}
