// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "app.hpp"
#include "util/pctl_ops_c.hpp"

namespace app
{

static bool     s_ok          = false;
static uint32_t s_init_result = 0;

bool init()
{
    Result rc     = pctl_ops_init();
    s_init_result = (uint32_t)rc;
    s_ok          = R_SUCCEEDED(rc);
    // Probe availability without keeping the single privileged session occupied.
    // Each service operation opens and releases its own reference.
    if (s_ok) pctl_ops_exit();
    return s_ok;
}

void shutdown()
{
    if (s_ok) pctl_ops_exit();
    s_ok = false;
}

bool     pctl_available()  { return s_ok; }
uint32_t pctl_init_result(){ return s_init_result; }

}   // namespace app
