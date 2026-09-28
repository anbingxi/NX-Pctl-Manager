#include "util/diagnostics.hpp"
#include "util/pctl_ops_c.hpp"
extern "C" {
#include "time_ops.h"
}
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>
#include <fmt/format.h>

namespace diagnostic {
std::string current_report()
{
    char clock[2048], pctl[16384];
    time_clock_dump(clock, sizeof(clock));
    pctl_play_timer_dump(pctl, sizeof(pctl));
    if (std::strlen(pctl) >= sizeof(pctl) - 1)
        return std::string(clock) + "ERROR: pctl diagnostic report truncated.\n";
    return std::string(clock) + pctl;
}

std::string save(const std::string& report, bool* saved)
{
    if (saved) *saved = false;
    const char* base = "/switch/nx_pctl_manager";
    const std::string dir = std::string(base) + "/logs";
    if (mkdir(base, 0777) != 0 && errno != EEXIST)
        return fmt::format("Could not create diagnostic directory (error {}).", errno);
    if (mkdir(dir.c_str(), 0777) != 0 && errno != EEXIST)
        return fmt::format("Could not create diagnostic directory (error {}).", errno);
    std::time_t now = std::time(nullptr);
    std::tm* clock = std::localtime(&now);
    char stamp[32];
    if (!clock || !std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", clock))
        return "Could not read the clock for the diagnostic filename.";
    for (unsigned index = 1; index <= 9999; ++index) {
        std::string path = fmt::format("{}/{}_{}_{:04}.txt", dir, stamp,
            (unsigned long long)svcGetSystemTick(), index);
        if (access(path.c_str(), F_OK) == 0) continue;
        std::string pending = path + ".tmp";
        if (access(pending.c_str(), F_OK) == 0) continue;
        FILE* file = std::fopen(pending.c_str(), "wb");
        if (!file) return fmt::format("Could not open diagnostic file (error {}).", errno);
        bool ok = std::fwrite(report.data(), 1, report.size(), file) == report.size();
        int write_error = ok ? 0 : errno;
        if (ok && std::fflush(file) != 0) { ok = false; write_error = errno; }
        if (std::fclose(file) != 0) { ok = false; write_error = errno; }
        if (!ok) {
            std::remove(pending.c_str());
            return fmt::format("Could not finish diagnostic file (error {}).", write_error);
        }
        if (std::rename(pending.c_str(), path.c_str()) != 0) {
            int rename_error = errno;
            std::remove(pending.c_str());
            return fmt::format("Could not rename diagnostic file (error {}).", rename_error);
        }
        if (saved) *saved = true;
        return fmt::format("Diagnostic saved: {}", path);
    }
    return "Could not find a free diagnostic filename.";
}
}
