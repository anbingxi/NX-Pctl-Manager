#include "activity/time_sync_activity.hpp"
#include "util/diagnostics.hpp"
#include "util/pctl_ops_c.hpp"
extern "C" {
#include "time_ops.h"
}
#include <ctime>
#include <fmt/format.h>

namespace {
std::string utc(uint64_t value)
{
    std::time_t seconds = (std::time_t)value;
    std::tm* tm = std::gmtime(&seconds);
    char text[40];
    if (!tm || !std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S UTC", tm))
        return fmt::format("{} seconds", value);
    return text;
}
std::string clock_text(Result rc, uint64_t value)
{
    return R_SUCCEEDED(rc) ? utc(value) : fmt::format("无法读取 (0x{:08X})", (unsigned)rc);
}
std::string flag_text(Result rc, bool value)
{
    return R_SUCCEEDED(rc) ? (value ? "true" : "false")
        : fmt::format("未知 (0x{:08X})", (unsigned)rc);
}
}

void TimeSyncActivity::refresh()
{
    TimeSnapshot clock;
    time_clock_snapshot(&clock);
    this->clocks->setText(fmt::format(
        "用户时钟：{}\n网络时钟：{}\n自动校时：{}\n网络时钟精度足够：{}",
        clock_text(clock.user_rc, clock.user_time), clock_text(clock.network_rc, clock.network_time),
        flag_text(clock.automatic_rc, clock.automatic), flag_text(clock.accuracy_rc, clock.accuracy)));
    PtState state;
    pctl_play_timer_query(&state);
    const Result session_error = state.session_valid ? 0 : state.session_rc;
    this->timer->setText(fmt::format(
        "限时已启用：{}\n当前受到时间限制：{}\n系统临时解锁：{}\n剩余时间：{}\n实际游戏阻止效果需退出工具后测试。",
        flag_text(state.enabled_valid ? 0 : (session_error ? session_error : state.enabled_rc), state.enabled),
        flag_text(state.restricted_valid ? 0 : (session_error ? session_error : state.restricted_rc), state.restricted),
        flag_text(state.temporary_unlocked_valid ? 0 : (session_error ? session_error : state.temporary_unlocked_rc), state.temporary_unlocked),
        state.remaining_valid ? fmt::format("{} ns", state.remaining_ns) : "未知"));
}

void TimeSyncActivity::select_server(const std::string& host)
{
    if (busy) return;
    server = host;
    reply = {};
    this->item_server->setText("NTP 服务器：" + host + "（点击修改）");
    this->result->setText("先读取服务器时间，再应用时间并检查限时。");
}

void TimeSyncActivity::change_server()
{
    if (busy) return;
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0))) return;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, "NTP 服务器域名或 IP");
    swkbdConfigSetGuideText(&kbd, "仅填写主机名，使用 UDP 123；不要填写 http://");
    swkbdConfigSetInitialText(&kbd, server.c_str());
    swkbdConfigSetStringLenMin(&kbd, 1);
    swkbdConfigSetStringLenMax(&kbd, 253);
    char host[256] = {0};
    Result rc = swkbdShow(&kbd, host, sizeof(host));
    swkbdClose(&kbd);
    if (R_SUCCEEDED(rc)) select_server(host);
}

void TimeSyncActivity::fetch_time()
{
    if (busy) return;
    busy = true;
    reply = {};
    const std::string host = server;
    this->result->setText("正在读取 " + host + "，请稍候……");
    std::weak_ptr<int> weak = lifetime;
    brls::async([this, weak, host] {
        ntp::Reply response = ntp::fetch(host);
        brls::sync([this, weak, host, response] {
            if (weak.expired()) return;
            busy = false;
            reply = response;
            fetched_server = host;
            fetched_at = response.received_at;
            this->result->setText(response.ok
                ? "服务器时间：" + utc(response.unix_seconds) + "\n可选择“应用时间并检查限时”。"
                : "读取失败：" + response.error);
        });
    });
}

void TimeSyncActivity::apply_time()
{
    if (busy) return;
    if (!reply.ok) {
        this->result->setText("请先成功读取服务器时间。");
        return;
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - fetched_at).count();
    if (elapsed < 0 || elapsed > 120) {
        reply = {};
        this->result->setText("服务器时间样本已过期，请重新读取。");
        return;
    }
    const std::string before = diagnostic::current_report();
    bool saved = false;
    const std::string before_save = diagnostic::save("=== Before third-party clock apply ===\n" + before, &saved);
    if (!saved) {
        this->result->setText("未修改时间；写入前的日志保存失败：" + before_save);
        return;
    }
    elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - fetched_at).count();
    if (elapsed < 0 || elapsed > 120) {
        reply = {};
        this->result->setText("服务器时间样本已过期，请重新读取。");
        return;
    }
    const u64 target = reply.unix_seconds + (u64)elapsed;
    TimeApply applied;
    time_clock_apply(target, &applied);
    // A time write does not establish pctl enforcement. Query real state again.
    refresh();
    std::string message;
    if (applied.refused_automatic && R_FAILED(applied.before.automatic_rc))
        message = fmt::format("未写入：无法确认系统自动校时状态 (0x{:08X})。", (unsigned)applied.before.automatic_rc);
    else if (applied.refused_automatic)
        message = "未写入：请在系统设置的日期与时间中开启“通过互联网同步时间”，再重试。此工具使用所选 NTP 服务器。";
    else if (!applied.write_attempted)
        message = fmt::format("无法打开可写网络时钟 (0x{:08X})。", (unsigned)applied.open_rc);
    else if (R_FAILED(applied.write_rc))
        message = fmt::format("写入网络时钟失败 (0x{:08X})。", (unsigned)applied.write_rc);
    else if (!applied.verified)
        message = fmt::format("写入接口成功，但读回核验失败 (0x{:08X})。请查看日志。", (unsigned)applied.verify_rc);
    else if (R_FAILED(applied.after.accuracy_rc) || !applied.after.accuracy)
        message = "网络时间写入并读回一致；系统尚未确认网络时钟精度足够。请查看限时状态并导出日志。";
    else
        message = "网络时间写入并读回一致，系统确认时钟精度足够。下方限时读数仍需结合实际游戏测试。";
    last_report = fmt::format(
        "=== Third-party NTP clock apply ===\nserver={}\ntarget_utc={}\n"
        "open_rc=0x{:08X} write_attempted={} write_rc=0x{:08X}\n"
        "verify_attempted={} verify_rc=0x{:08X} verified={} readback={}\n"
        "summary={}\n\n--- Before ---\n{}\n--- After ---\n{}",
        fetched_server, target, (unsigned)applied.open_rc, applied.write_attempted, (unsigned)applied.write_rc,
        applied.verify_attempted, (unsigned)applied.verify_rc, applied.verified, applied.readback,
        message, before, diagnostic::current_report());
    reply = {};
    this->result->setText(message);
    brls::Application::notify(diagnostic::save(last_report));
}

void TimeSyncActivity::onContentAvailable()
{
    clocks->setSingleLine(false);
    timer->setSingleLine(false);
    result->setSingleLine(false);
    item_server->registerClickAction([this](brls::View*) { change_server(); return true; });
    item_aliyun->registerClickAction([this](brls::View*) { select_server("ntp.aliyun.com"); return true; });
    item_cloudflare->registerClickAction([this](brls::View*) { select_server("time.cloudflare.com"); return true; });
    item_tencent->registerClickAction([this](brls::View*) { select_server("ntp1.tencent.com"); return true; });
    item_google->registerClickAction([this](brls::View*) { select_server("time.google.com"); return true; });
    item_pool_cn->registerClickAction([this](brls::View*) { select_server("cn.pool.ntp.org"); return true; });
    item_pool_asia->registerClickAction([this](brls::View*) { select_server("asia.pool.ntp.org"); return true; });
    item_pool_global->registerClickAction([this](brls::View*) { select_server("pool.ntp.org"); return true; });
    item_fetch->registerClickAction([this](brls::View*) { fetch_time(); return true; });
#ifdef PCTL_READ_ONLY
    item_apply->setVisibility(brls::Visibility::GONE);
#else
    item_apply->registerClickAction([this](brls::View*) { apply_time(); return true; });
#endif
    item_refresh->registerClickAction([this](brls::View*) { refresh(); return true; });
    item_dump->registerClickAction([this](brls::View*) {
        brls::Application::notify(diagnostic::save(last_report.empty() ? diagnostic::current_report()
            : last_report + "\n--- Current ---\n" + diagnostic::current_report()));
        return true;
    });
    select_server(server);
    refresh();
}
