#include "stdafx.h"
#include "tolk_bridge.h"

namespace {
static const GUID guid_menu_group = { 0xa632c10c, 0xe83f, 0x46d3, { 0x95, 0x46, 0xf9, 0x5b, 0x9d, 0x1a, 0xba, 0xca } };
static const GUID guid_cmd_current = { 0xdebf4392, 0xdad6, 0x42d6, { 0xbe, 0x3f, 0x9d, 0xb8, 0xd6, 0x56, 0xdb, 0x0f } };
static const GUID guid_cmd_previous = { 0xa4f7c3a1, 0x6efb, 0x41c0, { 0x88, 0x9e, 0x0a, 0x7c, 0xbe, 0x4c, 0x2f, 0xbd } };
static const GUID guid_cmd_next = { 0xf4a38e41, 0x50ea, 0x43db, { 0xad, 0x41, 0x3b, 0xdd, 0xc6, 0x10, 0xae, 0x91 } };
static const GUID guid_play_mode_group = { 0xdb6ab541, 0x5206, 0x401a, { 0xab, 0xee, 0x54, 0x21, 0x5a, 0x87, 0x8e, 0x70 } };
static const GUID guid_cmd_play_mode_current = { 0x4124a409, 0x1693, 0x419c, { 0x90, 0x8b, 0x68, 0xdd, 0x8a, 0x9b, 0x49, 0xa5 } };
static const GUID guid_cmd_play_mode_previous = { 0x1e2ebb47, 0x0093, 0x4350, { 0x87, 0x84, 0x54, 0x56, 0x27, 0xf1, 0xec, 0x8d } };
static const GUID guid_cmd_play_mode_next = { 0xd9f972a2, 0xa5c0, 0x4bcf, { 0xa0, 0x10, 0xc4, 0x2b, 0x3d, 0xb8, 0xbe, 0x1c } };
static const GUID guid_playlist_group = { 0x254a3a0f, 0xb47e, 0x4de5, { 0xae, 0xe5, 0xd9, 0x56, 0x67, 0x32, 0x10, 0x0c } };
static const GUID guid_cmd_playlist_current = { 0x0dd69071, 0x97e9, 0x4668, { 0x92, 0xd8, 0x23, 0x8e, 0x3a, 0xb4, 0xcb, 0x86 } };
static const GUID guid_cmd_playlist_previous_play = { 0xa02d6215, 0x9b6e, 0x4907, { 0xbe, 0x5e, 0x9e, 0x85, 0x2e, 0x52, 0xf5, 0x8f } };
static const GUID guid_cmd_playlist_next_play = { 0x680f1592, 0x5bd2, 0x49ba, { 0xab, 0x21, 0x67, 0x10, 0xcb, 0xb4, 0xa3, 0xae } };

struct device_item {
    pfc::string8 name;
    GUID output = {};
    GUID device = {};
};

const char* utf8_from_wide(const wchar_t* text) {
    enum { slots = 16 };
    static thread_local pfc::string8 buffers[slots];
    static thread_local unsigned index = 0;
    index = (index + 1) % slots;
    buffers[index] = pfc::stringcvt::string_utf8_from_wide(text).get_ptr();
    return buffers[index].get_ptr();
}

void set_utf8(pfc::string_base& out, const wchar_t* text) {
    out = pfc::stringcvt::string_utf8_from_wide(text).get_ptr();
}

std::wstring wide_from_utf8(const char* text) {
    return std::wstring(pfc::stringcvt::string_wide_from_utf8(text ? text : "").get_ptr());
}

bool same_device(const device_item& item, const GUID& output, const GUID& device) {
    return item.output == output && item.device == device;
}

std::vector<device_item> enumerate_devices(output_manager_v2* api) {
    std::vector<device_item> devices;
    if (!api) return devices;

    api->listDevices([&](const char* full_name, const GUID& output, const GUID& device) {
        if (output == output_id_null) return;
        for (const auto& item : devices) {
            if (same_device(item, output, device)) return;
        }

        device_item item;
        item.name = (full_name && *full_name) ? full_name : "[unnamed device]";
        item.output = output;
        item.device = device;
        devices.push_back(item);
    });

    return devices;
}

std::vector<device_item> enumerate_devices() {
    auto api = output_manager_v2::tryGet();
    if (!api.is_valid()) return {};
    return enumerate_devices(api.get_ptr());
}

size_t find_current_device(const std::vector<device_item>& devices, const outputCoreConfig_t& config) {
    for (size_t index = 0; index < devices.size(); ++index) {
        if (same_device(devices[index], config.m_output, config.m_device)) return index;
    }
    return SIZE_MAX;
}

void announce(const std::wstring& text) {
    tolk_queue_speak(text.c_str(), true);
}

void show_error(const wchar_t* text) {
    announce(text);
    popup_message::g_show(utf8_from_wide(text), utf8_from_wide(L"foobar2000 \u65E0\u969C\u788D\u589E\u5F3A"));
}

void announce_device(const device_item& item) {
    announce(wide_from_utf8(item.name.get_ptr()));
}

void announce_playback_order(const char* name) {
    std::wstring message = L"\u64AD\u653E\u6A21\u5F0F : ";
    message += wide_from_utf8(name);
    announce(message);
}

void announce_current_playback_order_name(const char* name) {
    std::wstring message = L"\u5F53\u524D : ";
    message += wide_from_utf8(name);
    announce(message);
}

void announce_playlist(const char* name) {
    std::wstring message = L"\u64AD\u653E\u5217\u8868 : ";
    message += wide_from_utf8(name);
    announce(message);
}

void announce_current_playlist_name(const char* name) {
    std::wstring message = L"\u5F53\u524D : ";
    message += wide_from_utf8(name);
    announce(message);
}

void announce_empty_playlist(const char* name) {
    std::wstring message = L"\u64AD\u653E\u5217\u8868\u4E3A\u7A7A : ";
    message += wide_from_utf8(name);
    announce(message);
}

void announce_current_device() {
    auto api = output_manager_v2::tryGet();
    if (!api.is_valid()) {
        show_error(L"当前 foobar2000 版本不支持查看播放设备，请使用 foobar2000 1.5/1.6 或更高版本。");
        return;
    }

    std::vector<device_item> devices = enumerate_devices(api.get_ptr());
    if (devices.empty()) {
        show_error(L"\u6CA1\u6709\u53EF\u7528\u7684\u64AD\u653E\u8BBE\u5907");
        return;
    }

    outputCoreConfig_t config = {};
    api->getCoreConfig(config);

    size_t current = find_current_device(devices, config);
    if (current == SIZE_MAX) {
        show_error(L"\u65E0\u6CD5\u786E\u5B9A\u5F53\u524D\u64AD\u653E\u8BBE\u5907");
        return;
    }

    std::wstring message = L"\u5F53\u524D : ";
    message += wide_from_utf8(devices[current].name.get_ptr());
    announce(message);
}

void switch_device(int delta) {
    auto api = output_manager_v2::tryGet();
    if (!api.is_valid()) {
        show_error(L"当前 foobar2000 版本不支持切换播放设备，请使用 foobar2000 1.5/1.6 或更高版本。");
        return;
    }

    std::vector<device_item> devices = enumerate_devices(api.get_ptr());
    if (devices.empty()) {
        show_error(L"\u6CA1\u6709\u53EF\u7528\u7684\u64AD\u653E\u8BBE\u5907");
        return;
    }

    outputCoreConfig_t config = {};
    api->getCoreConfig(config);

    size_t current = find_current_device(devices, config);
    size_t target = 0;
    if (current == SIZE_MAX) {
        target = delta >= 0 ? 0 : devices.size() - 1;
    } else if (delta >= 0) {
        target = (current + 1) % devices.size();
    } else {
        target = current == 0 ? devices.size() - 1 : current - 1;
    }

    try {
        api->setCoreConfigDevice(devices[target].output, devices[target].device);
        announce_device(devices[target]);
    } catch (const std::exception& e) {
        FB2K_console_formatter() << "foo_output_device_switcher: failed to switch playback device: " << e.what();
        show_error(L"\u5207\u6362\u64AD\u653E\u8BBE\u5907\u5931\u8D25");
    } catch (...) {
        FB2K_console_formatter() << "foo_output_device_switcher: failed to switch playback device.";
        show_error(L"\u5207\u6362\u64AD\u653E\u8BBE\u5907\u5931\u8D25");
    }
}

size_t get_playback_order_count() {
    try {
        return playlist_manager::get()->playback_order_get_count();
    } catch (...) {
        return 0;
    }
}

void announce_current_playback_order() {
    try {
        auto api = playlist_manager::get();
        const t_size count = api->playback_order_get_count();
        if (count == 0) {
            show_error(L"\u6CA1\u6709\u53EF\u7528\u7684\u64AD\u653E\u6A21\u5F0F");
            return;
        }

        const t_size current = api->playback_order_get_active();
        if (current >= count) {
            show_error(L"\u65E0\u6CD5\u786E\u5B9A\u5F53\u524D\u64AD\u653E\u6A21\u5F0F");
            return;
        }

        announce_current_playback_order_name(api->playback_order_get_name(current));
    } catch (const std::exception& e) {
        FB2K_console_formatter() << "foo_output_device_switcher: failed to get current playback mode: " << e.what();
        show_error(L"\u67E5\u770B\u5F53\u524D\u64AD\u653E\u6A21\u5F0F\u5931\u8D25");
    } catch (...) {
        FB2K_console_formatter() << "foo_output_device_switcher: failed to get current playback mode.";
        show_error(L"\u67E5\u770B\u5F53\u524D\u64AD\u653E\u6A21\u5F0F\u5931\u8D25");
    }
}

void switch_playback_order(int delta) {
    try {
        auto api = playlist_manager::get();
        const t_size count = api->playback_order_get_count();
        if (count == 0) {
            show_error(L"\u6CA1\u6709\u53EF\u7528\u7684\u64AD\u653E\u6A21\u5F0F");
            return;
        }

        const t_size current = api->playback_order_get_active();
        t_size target = 0;
        if (current >= count) {
            target = delta >= 0 ? 0 : count - 1;
        } else if (delta >= 0) {
            target = (current + 1) % count;
        } else {
            target = current == 0 ? count - 1 : current - 1;
        }

        api->playback_order_set_active(target);
        announce_playback_order(api->playback_order_get_name(target));
    } catch (const std::exception& e) {
        FB2K_console_formatter() << "foo_output_device_switcher: failed to switch playback mode: " << e.what();
        show_error(L"\u5207\u6362\u64AD\u653E\u6A21\u5F0F\u5931\u8D25");
    } catch (...) {
        FB2K_console_formatter() << "foo_output_device_switcher: failed to switch playback mode.";
        show_error(L"\u5207\u6362\u64AD\u653E\u6A21\u5F0F\u5931\u8D25");
    }
}

size_t get_playlist_count() {
    try {
        return playlist_manager::get()->get_playlist_count();
    } catch (...) {
        return 0;
    }
}

void announce_current_playlist() {
    try {
        auto api = playlist_manager::get();
        const t_size count = api->get_playlist_count();
        if (count == 0) {
            show_error(L"\u6CA1\u6709\u53EF\u7528\u7684\u64AD\u653E\u5217\u8868");
            return;
        }

        const t_size current = api->get_active_playlist();
        if (current >= count) {
            show_error(L"\u65E0\u6CD5\u786E\u5B9A\u5F53\u524D\u64AD\u653E\u5217\u8868");
            return;
        }

        pfc::string8 name;
        if (!api->playlist_get_name(current, name)) {
            show_error(L"\u65E0\u6CD5\u83B7\u53D6\u5F53\u524D\u64AD\u653E\u5217\u8868\u540D\u79F0");
            return;
        }

        announce_current_playlist_name(name.get_ptr());
    } catch (const std::exception& e) {
        FB2K_console_formatter() << "foo_output_device_switcher: failed to get current playlist: " << e.what();
        show_error(L"\u67E5\u770B\u5F53\u524D\u64AD\u653E\u5217\u8868\u5931\u8D25");
    } catch (...) {
        FB2K_console_formatter() << "foo_output_device_switcher: failed to get current playlist.";
        show_error(L"\u67E5\u770B\u5F53\u524D\u64AD\u653E\u5217\u8868\u5931\u8D25");
    }
}

void switch_playlist_and_play(int delta) {
    try {
        auto api = playlist_manager::get();
        const t_size count = api->get_playlist_count();
        if (count == 0) {
            show_error(L"\u6CA1\u6709\u53EF\u7528\u7684\u64AD\u653E\u5217\u8868");
            return;
        }

        const t_size current = api->get_active_playlist();
        t_size target = 0;
        if (current >= count) {
            target = delta >= 0 ? 0 : count - 1;
        } else if (delta >= 0) {
            target = (current + 1) % count;
        } else {
            target = current == 0 ? count - 1 : current - 1;
        }

        pfc::string8 name;
        if (!api->playlist_get_name(target, name)) {
            show_error(L"\u65E0\u6CD5\u83B7\u53D6\u64AD\u653E\u5217\u8868\u540D\u79F0");
            return;
        }

        api->set_active_playlist(target);
        api->set_playing_playlist(target);

        const t_size item_count = api->playlist_get_item_count(target);
        if (item_count == 0) {
            announce_empty_playlist(name.get_ptr());
            return;
        }

        t_size item = api->playlist_get_focus_item(target);
        if (item >= item_count) item = 0;

        api->playlist_set_focus_item(target, item);
        if (!api->playlist_execute_default_action(target, item)) {
            show_error(L"\u64AD\u653E\u64AD\u653E\u5217\u8868\u5931\u8D25");
            return;
        }

        announce_playlist(name.get_ptr());
    } catch (const std::exception& e) {
        FB2K_console_formatter() << "foo_output_device_switcher: failed to switch playlist and play: " << e.what();
        show_error(L"\u5207\u6362\u64AD\u653E\u5217\u8868\u5E76\u64AD\u653E\u5931\u8D25");
    } catch (...) {
        FB2K_console_formatter() << "foo_output_device_switcher: failed to switch playlist and play.";
        show_error(L"\u5207\u6362\u64AD\u653E\u5217\u8868\u5E76\u64AD\u653E\u5931\u8D25");
    }
}

static mainmenu_group_popup_factory g_menu_group(
    guid_menu_group,
    mainmenu_groups::playback,
    mainmenu_commands::sort_priority_base + 500,
    utf8_from_wide(L"\u64AD\u653E\u8BBE\u5907(&D)")
);

static mainmenu_group_popup_factory g_play_mode_group(
    guid_play_mode_group,
    mainmenu_groups::playback,
    mainmenu_commands::sort_priority_base + 510,
    utf8_from_wide(L"\u64AD\u653E\u6A21\u5F0F(&M)")
);

static mainmenu_group_popup_factory g_playlist_group(
    guid_playlist_group,
    mainmenu_groups::playback,
    mainmenu_commands::sort_priority_base + 520,
    utf8_from_wide(L"\u64AD\u653E\u5217\u8868(&L)")
);

class output_device_switcher_menu : public mainmenu_commands {
public:
    enum {
        cmd_current,
        cmd_previous,
        cmd_next,
        cmd_total
    };

    t_uint32 get_command_count() override { return cmd_total; }

    GUID get_command(t_uint32 index) override {
        switch (index) {
        case cmd_current: return guid_cmd_current;
        case cmd_previous: return guid_cmd_previous;
        case cmd_next: return guid_cmd_next;
        default: uBugCheck();
        }
    }

    void get_name(t_uint32 index, pfc::string_base& out) override {
        switch (index) {
        case cmd_current:
            set_utf8(out, L"\u67E5\u770B\u5F53\u524D\u64AD\u653E\u8BBE\u5907");
            break;
        case cmd_previous:
            set_utf8(out, L"\u5207\u6362\u5230\u4E0A\u4E00\u4E2A\u64AD\u653E\u8BBE\u5907");
            break;
        case cmd_next:
            set_utf8(out, L"\u5207\u6362\u5230\u4E0B\u4E00\u4E2A\u64AD\u653E\u8BBE\u5907");
            break;
        default:
            uBugCheck();
        }
    }

    bool get_description(t_uint32 index, pfc::string_base& out) override {
        switch (index) {
        case cmd_current:
            set_utf8(out, L"\u7528 Tolk \u64AD\u62A5 foobar2000 \u5F53\u524D\u4F7F\u7528\u7684\u64AD\u653E\u8BBE\u5907\u3002");
            return true;
        case cmd_previous:
            set_utf8(out, L"\u5C06 foobar2000 \u5207\u6362\u5230\u4E0A\u4E00\u4E2A\u64AD\u653E\u8BBE\u5907\uFF0C\u5E76\u7528 Tolk \u64AD\u62A5\u65B0\u8BBE\u5907\u3002");
            return true;
        case cmd_next:
            set_utf8(out, L"\u5C06 foobar2000 \u5207\u6362\u5230\u4E0B\u4E00\u4E2A\u64AD\u653E\u8BBE\u5907\uFF0C\u5E76\u7528 Tolk \u64AD\u62A5\u65B0\u8BBE\u5907\u3002");
            return true;
        default:
            return false;
        }
    }

    GUID get_parent() override { return guid_menu_group; }

    t_uint32 get_sort_priority() override {
        return mainmenu_commands::sort_priority_base;
    }

    bool get_display(t_uint32 index, pfc::string_base& text, t_uint32& flags) override {
        get_name(index, text);
        flags = 0;
        const size_t device_count = enumerate_devices().size();
        if (index == cmd_current) {
            if (device_count < 1) flags |= flag_disabled;
        } else if (device_count < 2) {
            flags |= flag_disabled;
        }
        return true;
    }

    void execute(t_uint32 index, service_ptr_t<service_base>) override {
        switch (index) {
        case cmd_current:
            announce_current_device();
            break;
        case cmd_previous:
            switch_device(-1);
            break;
        case cmd_next:
            switch_device(1);
            break;
        default:
            uBugCheck();
        }
    }
};

static mainmenu_commands_factory_t<output_device_switcher_menu> g_menu_factory;

class playback_mode_menu : public mainmenu_commands {
public:
    enum {
        cmd_current,
        cmd_previous,
        cmd_next,
        cmd_total
    };

    t_uint32 get_command_count() override { return cmd_total; }

    GUID get_command(t_uint32 index) override {
        switch (index) {
        case cmd_current: return guid_cmd_play_mode_current;
        case cmd_previous: return guid_cmd_play_mode_previous;
        case cmd_next: return guid_cmd_play_mode_next;
        default: uBugCheck();
        }
    }

    void get_name(t_uint32 index, pfc::string_base& out) override {
        switch (index) {
        case cmd_current:
            set_utf8(out, L"\u67E5\u770B\u5F53\u524D\u64AD\u653E\u6A21\u5F0F");
            break;
        case cmd_previous:
            set_utf8(out, L"\u5207\u6362\u5230\u4E0A\u4E00\u4E2A\u64AD\u653E\u6A21\u5F0F");
            break;
        case cmd_next:
            set_utf8(out, L"\u5207\u6362\u5230\u4E0B\u4E00\u4E2A\u64AD\u653E\u6A21\u5F0F");
            break;
        default:
            uBugCheck();
        }
    }

    bool get_description(t_uint32 index, pfc::string_base& out) override {
        switch (index) {
        case cmd_current:
            set_utf8(out, L"\u7528 Tolk \u64AD\u62A5 foobar2000 \u5F53\u524D\u7684\u64AD\u653E\u6A21\u5F0F\u3002");
            return true;
        case cmd_previous:
            set_utf8(out, L"\u5C06 foobar2000 \u5207\u6362\u5230\u4E0A\u4E00\u4E2A\u64AD\u653E\u6A21\u5F0F\uFF0C\u5E76\u7528 Tolk \u64AD\u62A5\u65B0\u6A21\u5F0F\u3002");
            return true;
        case cmd_next:
            set_utf8(out, L"\u5C06 foobar2000 \u5207\u6362\u5230\u4E0B\u4E00\u4E2A\u64AD\u653E\u6A21\u5F0F\uFF0C\u5E76\u7528 Tolk \u64AD\u62A5\u65B0\u6A21\u5F0F\u3002");
            return true;
        default:
            return false;
        }
    }

    GUID get_parent() override { return guid_play_mode_group; }

    t_uint32 get_sort_priority() override {
        return mainmenu_commands::sort_priority_base;
    }

    bool get_display(t_uint32 index, pfc::string_base& text, t_uint32& flags) override {
        get_name(index, text);
        flags = 0;
        const size_t mode_count = get_playback_order_count();
        if (index == cmd_current) {
            if (mode_count < 1) flags |= flag_disabled;
        } else if (mode_count < 2) {
            flags |= flag_disabled;
        }
        return true;
    }

    void execute(t_uint32 index, service_ptr_t<service_base>) override {
        switch (index) {
        case cmd_current:
            announce_current_playback_order();
            break;
        case cmd_previous:
            switch_playback_order(-1);
            break;
        case cmd_next:
            switch_playback_order(1);
            break;
        default:
            uBugCheck();
        }
    }
};

static mainmenu_commands_factory_t<playback_mode_menu> g_playback_mode_menu_factory;

class playlist_menu : public mainmenu_commands {
public:
    enum {
        cmd_current,
        cmd_previous_play,
        cmd_next_play,
        cmd_total
    };

    t_uint32 get_command_count() override { return cmd_total; }

    GUID get_command(t_uint32 index) override {
        switch (index) {
        case cmd_current: return guid_cmd_playlist_current;
        case cmd_previous_play: return guid_cmd_playlist_previous_play;
        case cmd_next_play: return guid_cmd_playlist_next_play;
        default: uBugCheck();
        }
    }

    void get_name(t_uint32 index, pfc::string_base& out) override {
        switch (index) {
        case cmd_current:
            set_utf8(out, L"\u67E5\u770B\u5F53\u524D\u64AD\u653E\u5217\u8868");
            break;
        case cmd_previous_play:
            set_utf8(out, L"\u5207\u6362\u5230\u4E0A\u4E00\u4E2A\u64AD\u653E\u5217\u8868\u5E76\u64AD\u653E");
            break;
        case cmd_next_play:
            set_utf8(out, L"\u5207\u6362\u5230\u4E0B\u4E00\u4E2A\u64AD\u653E\u5217\u8868\u5E76\u64AD\u653E");
            break;
        default:
            uBugCheck();
        }
    }

    bool get_description(t_uint32 index, pfc::string_base& out) override {
        switch (index) {
        case cmd_current:
            set_utf8(out, L"\u7528 Tolk \u64AD\u62A5 foobar2000 \u5F53\u524D\u6D3B\u52A8\u7684\u64AD\u653E\u5217\u8868\u3002");
            return true;
        case cmd_previous_play:
            set_utf8(out, L"\u5207\u6362\u5230\u4E0A\u4E00\u4E2A\u64AD\u653E\u5217\u8868\u5E76\u64AD\u653E\u5176\u4E2D\u7684\u7126\u70B9\u6B4C\u66F2\u6216\u7B2C\u4E00\u9996\u6B4C\u66F2\uFF0C\u7136\u540E\u7528 Tolk \u64AD\u62A5\u64AD\u653E\u5217\u8868\u540D\u79F0\u3002");
            return true;
        case cmd_next_play:
            set_utf8(out, L"\u5207\u6362\u5230\u4E0B\u4E00\u4E2A\u64AD\u653E\u5217\u8868\u5E76\u64AD\u653E\u5176\u4E2D\u7684\u7126\u70B9\u6B4C\u66F2\u6216\u7B2C\u4E00\u9996\u6B4C\u66F2\uFF0C\u7136\u540E\u7528 Tolk \u64AD\u62A5\u64AD\u653E\u5217\u8868\u540D\u79F0\u3002");
            return true;
        default:
            return false;
        }
    }

    GUID get_parent() override { return guid_playlist_group; }

    t_uint32 get_sort_priority() override {
        return mainmenu_commands::sort_priority_base;
    }

    bool get_display(t_uint32 index, pfc::string_base& text, t_uint32& flags) override {
        get_name(index, text);
        flags = 0;
        const size_t playlist_count = get_playlist_count();
        if (index == cmd_current) {
            if (playlist_count < 1) flags |= flag_disabled;
        } else if (playlist_count < 2) {
            flags |= flag_disabled;
        }
        return true;
    }

    void execute(t_uint32 index, service_ptr_t<service_base>) override {
        switch (index) {
        case cmd_current:
            announce_current_playlist();
            break;
        case cmd_previous_play:
            switch_playlist_and_play(-1);
            break;
        case cmd_next_play:
            switch_playlist_and_play(1);
            break;
        default:
            uBugCheck();
        }
    }
};

static mainmenu_commands_factory_t<playlist_menu> g_playlist_menu_factory;
}
