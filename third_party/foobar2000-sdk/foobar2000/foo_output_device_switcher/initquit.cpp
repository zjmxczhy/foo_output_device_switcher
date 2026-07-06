#include "stdafx.h"
#include "speech_engine.h"
#include "startup_hide.h"

class output_device_switcher_initquit : public initquit {
public:
    void on_init() override { startup_hide_schedule(); }
    void on_quit() override {
        startup_hide_cancel();
        speech_shutdown();
    }
};

static initquit_factory_t<output_device_switcher_initquit> g_initquit;
