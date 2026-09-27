// Verify the play timer state before changing its configuration. An active or
// unknown timer must be handled through the system parental-control PIN flow.
// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>

namespace pt_flow
{

// The second callback value is retained for existing callers and is always
// false. A false first value means the caller must not write.
void ready_to_write(std::function<void(bool ok, bool did_unlock)> on_ready);

}   // namespace pt_flow
