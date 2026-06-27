#include "stdafx.h"
#include "tolk_bridge.h"

class output_device_switcher_initquit : public initquit {
public:
    void on_init() override {}
    void on_quit() override { tolk_shutdown(); }
};

static initquit_factory_t<output_device_switcher_initquit> g_initquit;

