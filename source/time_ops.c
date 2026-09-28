#include "time_ops.h"
#include <stdio.h>
#include <string.h>

static Result clock_session(Service *root, Service *clock, u32 command)
{
    return serviceDispatch(root, command, .out_num_objects = 1, .out_objects = clock);
}

static Result read_clock(Service *root, u32 command, u64 *value)
{
    Service clock = {0};
    Result rc = clock_session(root, &clock, command);
    if (R_SUCCEEDED(rc)) rc = serviceDispatchOut(&clock, 0, *value);
    serviceClose(&clock);
    return rc;
}

void time_clock_snapshot(TimeSnapshot *out)
{
    memset(out, 0, sizeof(*out));
    Service root = {0};
    out->service_rc = smGetService(&root, "time:s");
    out->user_rc = out->network_rc = out->local_rc =
        out->automatic_rc = out->accuracy_rc = out->service_rc;
    if (R_SUCCEEDED(out->service_rc)) {
        out->user_rc = read_clock(&root, 0, &out->user_time);
        out->network_rc = read_clock(&root, 1, &out->network_time);
        out->local_rc = read_clock(&root, 4, &out->local_time);
        out->automatic_rc = serviceDispatchOut(&root, 100, out->automatic);
        out->accuracy_rc = serviceDispatchOut(&root, 200, out->accuracy);
    }
    serviceClose(&root);
}

void time_clock_apply(u64 utc_seconds, TimeApply *out)
{
    memset(out, 0, sizeof(*out));
    time_clock_snapshot(&out->before);
#ifdef PCTL_READ_ONLY
    (void)utc_seconds;
    out->open_rc = (Result)0xF001;
#else
    if (R_FAILED(out->before.automatic_rc) || !out->before.automatic) {
        out->refused_automatic = true;
        out->after = out->before;
        return;
    }
    Service root = {0}, network = {0};
    out->open_rc = smGetService(&root, "time:s");
    if (R_SUCCEEDED(out->open_rc)) out->open_rc = clock_session(&root, &network, 1);
    if (R_SUCCEEDED(out->open_rc)) {
        out->write_attempted = true;
        out->write_rc = serviceDispatchIn(&network, 1, utc_seconds);
        if (R_SUCCEEDED(out->write_rc)) {
            out->verify_attempted = true;
            out->verify_rc = serviceDispatchOut(&network, 0, out->readback);
            // Allow the small elapsed interval during IPC, never unsigned underflow.
            out->verified = R_SUCCEEDED(out->verify_rc) && out->readback >= utc_seconds
                && out->readback - utc_seconds <= 5;
        }
    }
    serviceClose(&network);
    serviceClose(&root);
#endif
    time_clock_snapshot(&out->after);
}

void time_clock_dump(char *buf, size_t size)
{
    TimeSnapshot s;
    time_clock_snapshot(&s);
    snprintf(buf, size,
        "=== System clocks (UTC POSIX seconds) ===\n"
        "time:s open: rc=0x%08X\n"
        "User clock: rc=0x%08X value=%llu (use only if rc=0)\n"
        "Network clock: rc=0x%08X value=%llu (use only if rc=0)\n"
        "Local clock: rc=0x%08X value=%llu (use only if rc=0)\n"
        "100 Automatic correction: rc=0x%08X %s\n"
        "200 Network clock accuracy sufficient: rc=0x%08X %s\n"
        "Clock service handles released.\n\n",
        (unsigned)s.service_rc,
        (unsigned)s.user_rc, (unsigned long long)s.user_time,
        (unsigned)s.network_rc, (unsigned long long)s.network_time,
        (unsigned)s.local_rc, (unsigned long long)s.local_time,
        (unsigned)s.automatic_rc, R_FAILED(s.automatic_rc) ? "unavailable" : (s.automatic ? "true" : "false"),
        (unsigned)s.accuracy_rc, R_FAILED(s.accuracy_rc) ? "unavailable" : (s.accuracy ? "true" : "false"));
}
