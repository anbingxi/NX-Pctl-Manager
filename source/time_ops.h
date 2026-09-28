#pragma once
#include <switch.h>

typedef struct {
    Result service_rc, user_rc, network_rc, local_rc, automatic_rc, accuracy_rc;
    u64 user_time, network_time, local_time;
    bool automatic, accuracy;
} TimeSnapshot;

typedef struct {
    TimeSnapshot before, after;
    bool refused_automatic, write_attempted, verify_attempted, verified;
    Result open_rc, write_rc, verify_rc;
    u64 readback;
} TimeApply;

// All handles are acquired and released within each operation.
void time_clock_snapshot(TimeSnapshot *out);
void time_clock_apply(u64 utc_seconds, TimeApply *out);
void time_clock_dump(char *buf, size_t size);
