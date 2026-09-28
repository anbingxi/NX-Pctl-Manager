#pragma once
#include <borealis.hpp>
#include <chrono>
#include <memory>
#include "util/ntp_client.hpp"

class TimeSyncActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/time_sync.xml");
    void onContentAvailable() override;
  private:
    void refresh();
    void fetch_time();
    void apply_time();
    void change_server();
    void select_server(const std::string& server);
    std::shared_ptr<int> lifetime = std::make_shared<int>(0);
    bool busy = false;
    std::string server = "ntp.aliyun.com";
    std::string fetched_server, last_report;
    ntp::Reply reply;
    std::chrono::steady_clock::time_point fetched_at;
    BRLS_BIND(brls::Label, clocks, "time_clocks");
    BRLS_BIND(brls::Label, timer, "time_timer");
    BRLS_BIND(brls::Label, result, "time_result");
    BRLS_BIND(brls::DetailCell, item_server, "time_server");
    BRLS_BIND(brls::DetailCell, item_aliyun, "time_aliyun");
    BRLS_BIND(brls::DetailCell, item_cloudflare, "time_cloudflare");
    BRLS_BIND(brls::DetailCell, item_tencent, "time_tencent");
    BRLS_BIND(brls::DetailCell, item_google, "time_google");
    BRLS_BIND(brls::DetailCell, item_pool_cn, "time_pool_cn");
    BRLS_BIND(brls::DetailCell, item_pool_asia, "time_pool_asia");
    BRLS_BIND(brls::DetailCell, item_pool_global, "time_pool_global");
    BRLS_BIND(brls::DetailCell, item_fetch, "time_fetch");
    BRLS_BIND(brls::DetailCell, item_apply, "time_apply");
    BRLS_BIND(brls::DetailCell, item_refresh, "time_refresh");
    BRLS_BIND(brls::DetailCell, item_dump, "time_dump");
};
