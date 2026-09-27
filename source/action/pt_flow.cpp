// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "action/pt_flow.hpp"

#include <borealis.hpp>

#include "util/pctl_ops_c.hpp"

namespace pt_flow
{

void ready_to_write(std::function<void(bool, bool)> on_ready)
{
    PtState pt;
    pctl_play_timer_query(&pt);

    if (!pt.enabled_valid || !pt.valid || !pt.restricted_valid) {
        brls::Application::notify("Play timer state is unknown; could not verify that changing it is safe.");
        on_ready(false, false);
        return;
    }

    if (pt.enabled) {
        brls::Application::notify("Use the system parental-control screen and PIN to temporarily unlock, then try again.");
        on_ready(false, false);
        return;
    }

    on_ready(true, false);
}

}   // namespace pt_flow
