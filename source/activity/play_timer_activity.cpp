// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "activity/play_timer_activity.hpp"

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <fmt/format.h>

#include "action/pt_flow.hpp"
#include "activity/play_timer_perday_activity.hpp"
#include "util/numpad.hpp"
#include "util/pctl_ops_c.hpp"

using namespace brls::literals;

namespace
{
#ifdef PCTL_PROBE
std::string save_diagnostic(const char* report)
{
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
        size_t length = std::strlen(report);
        bool ok = std::fwrite(report, 1, length, file) == length;
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
        return fmt::format("Diagnostic saved: {}", path);
    }
    return "Could not find a free diagnostic filename.";
}
#endif

// If the seven days share one value, return that minute count; otherwise return
// 60 as a sensible default for the numpad to land on. PT_DAY_NOLIMIT counts as
// "no useful starting value" → fall back to 60.
uint16_t guess_uniform_seed()
{
    PtState pt;
    pctl_play_timer_query(&pt);
    if (!pt.valid) return 60;
    for (int i = 1; i < 7; i++)
        if (pt.day_min[i] != pt.day_min[0]) return 60;
    return (pt.day_min[0] == PT_DAY_NOLIMIT) ? 60 : pt.day_min[0];
}

// Pop the "Parental controls are temporarily off — return to the main menu;
// the new limit takes effect once parental controls are active again." dialog.
// Called only after a write that went through the gate's unlock branch.
void show_did_unlock_notice()
{
    auto* d = new brls::Dialog("nx_pctl/play_timer/did_unlock_notice"_i18n);
    d->addButton("hints/ok"_i18n, [] {});
    d->open();
}
}   // namespace

void PlayTimerActivity::onContentAvailable()
{
#ifdef PCTL_READ_ONLY
    this->pt_set_all->setVisibility(brls::Visibility::GONE);
    this->pt_per_day->setVisibility(brls::Visibility::GONE);
    this->pt_remove->setVisibility(brls::Visibility::GONE);
#else
    // Set daily limit (all days) — numpad → write-gate → cmd 195101.
    this->pt_set_all->registerClickAction([this](brls::View*) {
        auto v = numpad::prompt_minutes(
            "Daily play-time limit for ALL days  (0 = zero minutes)",
            guess_uniform_seed());
        if (!v.has_value()) return true;   // user cancelled
        uint16_t value = *v;

        pt_flow::ready_to_write([this, value](bool ok, bool did_unlock) {
            if (!ok) return;   // declined / failed — gate already toasted
            Result rc = pctl_play_timer_set_uniform(value);
            this->state_header->refresh();
            if (R_FAILED(rc)) {
                brls::Application::notify(fmt::format(
                    "Failed to write the play-time limit (error 0x{:08X}).",
                    (unsigned)rc));
                return;
            }
            if (value)
                brls::Application::notify(fmt::format(
                    "Play-time limit written: {} minute(s)/day.", value));
            else
                brls::Application::notify(
                    "0 minutes/day written. Test game behavior after restoring restrictions.");
            if (did_unlock) show_did_unlock_notice();
        });
        return true;
    });

    // Per-day limits — push the staging sub-Activity.
    this->pt_per_day->registerClickAction([](brls::View*) {
        brls::Application::pushActivity(new PlayTimerPerDayActivity());
        return true;
    });
#endif

#ifdef PCTL_PROBE
    // PROBE build: export a report under /switch/nx_pctl_manager/logs/.
    // The compatibility probe never stores PIN contents in the report.
    this->pt_diag->setVisibility(brls::Visibility::VISIBLE);
    this->pt_diag->registerClickAction([](brls::View*) {
        static char buf[16384];
        pctl_play_timer_dump(buf, sizeof(buf));
        if (std::strlen(buf) >= sizeof(buf) - 1) {
            brls::Application::notify("Diagnostic report exceeded its buffer.");
            return true;
        }
        brls::Application::notify(save_diagnostic(buf));
        return true;
    });
#endif

#ifndef PCTL_READ_ONLY
    // Remove play-time limit after confirmation and the shared state check.
    this->pt_remove->registerClickAction([this](brls::View*) {
        auto* dialog = new brls::Dialog("nx_pctl/play_timer/dialog/remove/body"_i18n);
        dialog->addButton("hints/cancel"_i18n, [] {});
        dialog->addButton("nx_pctl/play_timer/dialog/remove/confirm"_i18n, [this]() {
          pt_flow::ready_to_write([this](bool ok, bool) {
            if (!ok) return;
            Result rc = pctl_play_timer_clear();
            this->state_header->refresh();
            if (R_SUCCEEDED(rc))
                brls::Application::notify("Play timer turned off.");
            else
                brls::Application::notify(fmt::format(
                    "Could not turn off the play timer (error 0x{:08X}).",
                    (unsigned)rc));
          });
        });
        dialog->open();
        return true;
    });
#endif

    this->state_header->refresh();
}

void PlayTimerActivity::onResume()
{
    brls::Activity::onResume();
    this->state_header->refresh();
}
