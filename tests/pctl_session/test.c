#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pctl_ops.h"

enum { MOCK_ERROR = 0x701 };
static Service service;
static struct {
    unsigned refs, init_calls, exit_calls, ipc_calls, failed_ipcs, writes, applets;
    unsigned fail_init_call;
    u32 fail_command;
    bool enabled, restricted, unlocked;
    u8 last_write[0x44];
} model;

static void reset(void)
{
    /* A test must release its references before resetting the fake service. */
    assert(model.refs == 0);
    memset(&model, 0, sizeof(model));
}

Result pctlInitialize(void)
{
    model.init_calls++;
    if (model.init_calls == model.fail_init_call) return MOCK_ERROR;
    assert(model.refs == 0); /* A duplicate acquisition is a lifecycle defect. */
    model.refs++;
    return 0;
}

void pctlExit(void)
{
    assert(model.refs == 1); /* Detect exit without ownership or double release. */
    model.refs--;
    model.exit_calls++;
}

Service *pctlGetServiceSession_Service(void)
{
    assert(model.refs == 1);
    return &service;
}

Result pctlauthRegisterPasscode(void)
{
    assert(model.refs == 0); /* The OS applet needs the privileged slot free. */
    model.applets++;
    return MOCK_ERROR; /* Even cancellation/failure must leave no idle session. */
}

Result mock_dispatch(Service *srv, u32 command, void *out, size_t out_size,
                     const void *in, size_t in_size)
{
    assert(srv == &service);
    assert(model.refs == 1);
    model.ipc_calls++;
    if (in != NULL || command == 1043 || command == 1941 || command == 1201)
        model.writes++;
    if (command == model.fail_command) {
        model.failed_ipcs++;
        return MOCK_ERROR;
    }
    if (in != NULL) {
        assert(command == 195101);
        assert(in_size == sizeof(model.last_write));
        memcpy(model.last_write, in, in_size);
    }
    if (out != NULL) {
        memset(out, 0, out_size);
        bool value = false;
        switch (command) {
            case 1453: value = model.enabled; break;
            case 1455: value = model.restricted; break;
            case 1006: value = model.unlocked; break;
            case 1031: value = true; break;
            case 1206: case 1208: {
                u32 length = 6;
                assert(out_size == sizeof(length));
                memcpy(out, &length, sizeof(length));
                return 0;
            }
            default: return 0;
        }
        assert(out_size == sizeof(value));
        memcpy(out, &value, sizeof(value));
    }
    return 0;
}

static void test_ownership(void)
{
    reset();
    model.fail_init_call = 1;
    assert(pctl_ops_init() == MOCK_ERROR);
    pctl_ops_exit();
    assert(model.exit_calls == 0);
    assert(model.refs == 0);

    reset();
    assert(pctl_ops_init() == 0);
    assert(pctl_ops_init() == 0);
    assert(model.init_calls == 1 && model.refs == 1);
    assert(pctl_ops_reinit() == 0);
    assert(model.init_calls == 2 && model.exit_calls == 1 && model.refs == 1);
    pctl_ops_exit();
    pctl_ops_exit();
    assert(model.exit_calls == 2 && model.refs == 0);
}

static void test_reads(void)
{
    PtState state;
    PctlStatus status;
    char report[8192];
    reset();
    for (unsigned i = 0; i < 20; ++i) {
        pctl_play_timer_query(&state);
        assert(state.session_valid && state.enabled_valid && state.restricted_valid);
        assert(state.temporary_unlocked_valid && state.remaining_valid && state.valid);
        assert(model.refs == 0);
        pctl_status_fetch(&status);
        assert(status.restriction_enabled_ok && status.pin_length_ok && status.safety_level_ok);
        assert(model.refs == 0);
        pctl_play_timer_dump(report, sizeof(report));
        assert(strstr(report, "content=not recorded") != NULL);
        assert(model.refs == 0 && model.writes == 0);
    }
    assert(model.init_calls == model.exit_calls);

    /* Fail every reconnect in the real diagnostic function in turn. */
    reset();
    pctl_play_timer_dump(report, sizeof(report));
    const unsigned reconnect_count = model.init_calls;
    assert(reconnect_count > 0 && model.refs == 0);
    for (unsigned fail_at = 1; fail_at <= reconnect_count; ++fail_at) {
        reset();
        model.fail_init_call = fail_at;
        pctl_play_timer_dump(report, sizeof(report));
        assert(strstr(report, "failed") != NULL);
        assert(model.init_calls == fail_at);
        assert(model.exit_calls == fail_at - 1 && model.refs == 0);
    }

    const u32 commands[] = {1453, 1455, 1006, 1454, 145601};
    for (unsigned i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        reset();
        model.fail_command = commands[i];
        pctl_play_timer_query(&state);
        assert(model.refs == 0 && model.init_calls == model.exit_calls);
        switch (commands[i]) {
            case 1453: assert(!state.enabled_valid && state.enabled_rc == MOCK_ERROR); break;
            case 1455: assert(!state.restricted_valid && state.restricted_rc == MOCK_ERROR); break;
            case 1006: assert(!state.temporary_unlocked_valid && state.temporary_unlocked_rc == MOCK_ERROR); break;
            case 1454: assert(!state.remaining_valid && state.remaining_rc == MOCK_ERROR); break;
            case 145601: assert(!state.valid && state.config_rc == MOCK_ERROR); break;
        }
        pctl_status_fetch(&status);
        pctl_play_timer_dump(report, sizeof(report));
        assert(model.refs == 0 && model.writes == 0);
    }

    reset();
    model.fail_init_call = 1;
    pctl_play_timer_query(&state);
    assert(!state.session_valid && !state.enabled_attempted && !state.config_attempted);
    assert(model.ipc_calls == 0 && model.exit_calls == 0 && model.refs == 0);
    reset();
    model.fail_init_call = 1;
    pctl_status_fetch(&status);
    assert(!status.restriction_enabled_ok && model.ipc_calls == 0 && model.refs == 0);

    const u32 status_commands[] = {1032, 1206, 1031};
    for (unsigned i = 0; i < sizeof(status_commands) / sizeof(status_commands[0]); ++i) {
        reset();
        model.fail_command = status_commands[i];
        pctl_status_fetch(&status);
        if (status_commands[i] == 1032) assert(!status.safety_level_ok);
        if (status_commands[i] == 1206) assert(!status.pin_length_ok);
        if (status_commands[i] == 1031) assert(!status.restriction_enabled_ok);
        assert(model.refs == 0 && model.init_calls == model.exit_calls);
    }
    reset();
    model.fail_command = 1208;
    pctl_play_timer_dump(report, sizeof(report));
    assert(strstr(report, "content=not recorded") != NULL);
    assert(model.failed_ipcs == 1);
    assert(model.refs == 0 && model.init_calls == model.exit_calls && model.writes == 0);
    reset();
    model.fail_command = 1952;
    pctl_play_timer_dump(report, sizeof(report));
    assert(model.failed_ipcs == 1);
    assert(model.refs == 0 && model.init_calls == model.exit_calls && model.writes == 0);

    reset();
    assert(pctl_ops_init() == 0);
    pctl_play_timer_dump(report, 0);
    assert(model.refs == 0 && model.ipc_calls == 0);
}

static Result write_variant(unsigned variant)
{
    u16 days[7] = {0, 10, 20, 30, 40, 50, 60};
    switch (variant) {
        case 0: return pctl_play_timer_set_days(days);
        case 1: return pctl_play_timer_set_uniform(0);
        default: return pctl_play_timer_clear();
    }
}

static void test_writes_and_applet(void)
{
#ifdef PCTL_READ_ONLY
    for (unsigned variant = 0; variant < 3; ++variant) {
        reset();
        assert(pctl_ops_init() == 0);
        assert(R_FAILED(write_variant(variant)));
        assert(model.refs == 0 && model.ipc_calls == 0 && model.writes == 0);
    }
    reset();
    assert(R_FAILED(pctl_delete_parental_controls()));
    assert(R_FAILED(pctl_delete_pairing()));
    assert(R_FAILED(pctl_set_pin()));
    assert(model.init_calls == 0 && model.applets == 0 && model.writes == 0);
#else
    /* All timer entry points must apply the service gate, including clear. */
    const bool permitted_states[8] = {true, false, false, false, true, true, true, true};
    for (unsigned variant = 0; variant < 3; ++variant) {
        for (unsigned states = 0; states < 8; ++states) {
            reset();
            model.enabled = (states & 1) != 0;
            model.restricted = (states & 2) != 0;
            model.unlocked = (states & 4) != 0;
            const bool permitted = permitted_states[states];
            Result rc = write_variant(variant);
            assert(R_SUCCEEDED(rc) == permitted);
            assert(model.writes == (permitted ? 1u : 0u));
            assert(model.refs == 0 && model.init_calls == model.exit_calls);
            if (permitted && variant == 2)
                for (unsigned i = 0; i < sizeof(model.last_write); ++i)
                    assert(model.last_write[i] == 0);
        }
        const u32 fail_commands[] = {1453, 1455, 1006, 195101};
        for (unsigned i = 0; i < sizeof(fail_commands) / sizeof(fail_commands[0]); ++i) {
            reset();
            model.fail_command = fail_commands[i];
            assert(write_variant(variant) == MOCK_ERROR);
            assert(model.writes == (fail_commands[i] == 195101 ? 1u : 0u));
            assert(model.refs == 0 && model.init_calls == 1 && model.exit_calls == 1);
        }
        reset();
        model.fail_init_call = 1;
        assert(write_variant(variant) == MOCK_ERROR);
        assert(model.ipc_calls == 0 && model.writes == 0 && model.exit_calls == 0 && model.refs == 0);
    }

    reset();
    assert(pctl_ops_init() == 0);
    assert(pctl_set_pin() == MOCK_ERROR);
    assert(model.applets == 1 && model.refs == 0 && model.init_calls == 1);
    assert(pctl_ops_init() == 0); /* Applet return did not leave an idle reference. */
    pctl_ops_exit();
    assert(model.init_calls == 2 && model.exit_calls == 2);

    const u32 deletes[] = {1043, 1941};
    for (unsigned i = 0; i < 2; ++i) {
        reset();
        model.fail_command = deletes[i];
        Result rc = i == 0 ? pctl_delete_parental_controls() : pctl_delete_pairing();
        assert(rc == MOCK_ERROR && model.writes == 1 && model.refs == 0);
        assert(model.init_calls == 1 && model.exit_calls == 1);
    }
#endif
    reset();
    assert(pctl_ops_init() == 0);
    assert(R_FAILED(pctl_unlock_restriction_temporarily()));
    assert(model.refs == 0 && model.ipc_calls == 0 && model.writes == 0);
}

int main(void)
{
    test_ownership();
    test_reads();
    test_writes_and_applet();
    puts("pctl lifecycle and write-gate assertions passed");
    return 0;
}
