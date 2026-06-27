#include "stdafx.h"
#include "tolk_bridge.h"

namespace {
static const GUID guid_menu_group = { 0xa632c10c, 0xe83f, 0x46d3, { 0x95, 0x46, 0xf9, 0x5b, 0x9d, 0x1a, 0xba, 0xca } };
static const GUID guid_cmd_previous = { 0xa4f7c3a1, 0x6efb, 0x41c0, { 0x88, 0x9e, 0x0a, 0x7c, 0xbe, 0x4c, 0x2f, 0xbd } };
static const GUID guid_cmd_next = { 0xf4a38e41, 0x50ea, 0x43db, { 0xad, 0x41, 0x3b, 0xdd, 0xc6, 0x10, 0xae, 0x91 } };

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
    popup_message::g_show(utf8_from_wide(text), utf8_from_wide(L"\u64AD\u653E\u8BBE\u5907\u5207\u6362"));
}

void announce_device(const device_item& item) {
    announce(wide_from_utf8(item.name.get_ptr()));
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

static mainmenu_group_popup_factory g_menu_group(
    guid_menu_group,
    mainmenu_groups::playback,
    mainmenu_commands::sort_priority_base + 500,
    utf8_from_wide(L"\u64AD\u653E\u8BBE\u5907(&D)")
);

class output_device_switcher_menu : public mainmenu_commands {
public:
    enum {
        cmd_previous,
        cmd_next,
        cmd_total
    };

    t_uint32 get_command_count() override { return cmd_total; }

    GUID get_command(t_uint32 index) override {
        switch (index) {
        case cmd_previous: return guid_cmd_previous;
        case cmd_next: return guid_cmd_next;
        default: uBugCheck();
        }
    }

    void get_name(t_uint32 index, pfc::string_base& out) override {
        switch (index) {
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
        if (enumerate_devices().size() < 2) flags |= flag_disabled;
        return true;
    }

    void execute(t_uint32 index, service_ptr_t<service_base>) override {
        switch (index) {
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
}
