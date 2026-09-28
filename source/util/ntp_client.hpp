// Copyright (C) 2026 Taylor. GPLv3-or-later (see LICENSE).
#pragma once
#include <cstdint>
#include <chrono>
#include <string>

namespace ntp {
struct Reply {
    bool ok = false;
    std::uint64_t unix_seconds = 0;
    std::string error;
    std::chrono::steady_clock::time_point received_at;
};

// Reads a time sample over connected UDP port 123. Socket services must already
// be initialized by the application. This function does not change system time.
Reply fetch(const std::string& host);
}
