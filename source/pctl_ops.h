// NX-Pctl-Manager — thin wrappers around the system parental-control (`pctl`)
// service. All user-facing strings live in the UI layer; this layer is data only.
// Copyright (C) 2026 Taylor.  This program is free software under the GNU
// General Public License v3 or later; it comes with NO WARRANTY. See the
// LICENSE file or <https://www.gnu.org/licenses/gpl-3.0.html> for details.
#pragma once
#include <switch.h>

typedef enum {
    PctlSafetyLevel_None       = 0,
    PctlSafetyLevel_Custom     = 1,
    PctlSafetyLevel_YoungChild = 2,
    PctlSafetyLevel_Child      = 3,
    PctlSafetyLevel_Teen       = 4,
} PctlSafetyLevel;

// Best-effort snapshot of the current parental-control state. A field's `*_ok`
// flag is false when that particular query failed (e.g. on a firmware where the
// command is unavailable) — callers should show "unavailable" rather than 0.
typedef struct {
    bool safety_level_ok;
    u32  safety_level;            // a PctlSafetyLevel value when safety_level_ok

    bool pin_length_ok;
    u32  pin_length;              // number of PIN digits; 0 == no PIN registered

    bool restriction_enabled_ok;
    bool restriction_enabled;
} PctlStatus;

Result pctl_ops_init(void);   // open the `pctl` service (needs CFW for the privileged calls)
void   pctl_ops_exit(void);
Result pctl_ops_reinit(void); // close + reopen the session (recovers if a bad command closed it)

void pctl_status_fetch(PctlStatus *out);

Result pctl_set_pin(void);                   // opens the OS PIN screen (registers or changes the PIN)
Result pctl_delete_parental_controls(void);  // wipes the PIN and every restriction — CANNOT BE UNDONE
Result pctl_delete_pairing(void);            // unlinks the companion mobile app from this console

// Deprecated entry point. Always returns an error; a parent must use the system
// PIN interface to temporarily unlock restrictions.
Result pctl_unlock_restriction_temporarily(void);

const char *pctl_safety_level_name(u32 level);

// ---- play timer ----
// Writes a multi-line read-only diagnostic report into buf: 1453/1455/1458/1454, the
// raw 0x44-byte GetPlayTimerSettings (145601) + its u16[34]/decoded view, 1459
// GetPlayTimerRemainingTimeDisplayInfo, 1031 IsRestrictionEnabled, 1006
// IsRestrictionTemporaryUnlocked, 1206 GetPinCodeLength, and 1208 GetPinCode.
// The 1208 read is only a compatibility probe; PIN content is never reported.
void pctl_play_timer_dump(char *buf, size_t bufsz);

// Writes 0x44 bytes of zeros via SetPlayTimerSettingsForDebug — clears any
// play-time limit. pctl_play_timer_set_uniform(0) configures a 0-minute limit.
Result pctl_play_timer_clear(void);

// Per-day value sentinel: this day has no configured limit at all (unrestricted).
// (0 means a configured 0-minute limit. Actual game behavior must be tested.)
#define PT_DAY_NOLIMIT 0xFFFFu

// Snapshot of the play-timer state.
typedef struct {
    bool session_valid;  // reinitialization succeeded; session_rc is always set
    Result session_rc;
    bool valid;          // GetPlayTimerSettings (145601) succeeded
    bool config_attempted;
    Result config_rc;    // meaningful only when config_attempted
    bool enabled_attempted;
    bool enabled_valid;
    Result enabled_rc;   // meaningful only when enabled_attempted
    bool enabled;        // IsPlayTimerEnabled (1453); meaningful only when enabled_valid
    bool restricted_attempted;
    bool restricted_valid;
    Result restricted_rc; // meaningful only when restricted_attempted
    bool restricted;     // IsRestrictedByPlayTimer (1455); meaningful only when restricted_valid
    bool remaining_attempted;
    bool remaining_valid;
    Result remaining_rc; // meaningful only when remaining_attempted
    u64  remaining_ns;   // GetPlayTimerRemainingTime (1454), ns; meaningful only when remaining_valid
    u16  day_min[7];     // per-day, Sun..Sat; meaningful only when valid
} PtState;

void pctl_play_timer_query(PtState *out);

// Sets per-day play-time limits (days_min[0]=Sunday .. [6]=Saturday): each is minutes
// (0 == a configured 0-minute limit), or PT_DAY_NOLIMIT to leave that day unrestricted.
// If every day is PT_DAY_NOLIMIT the play timer is turned off entirely.
Result pctl_play_timer_set_days(const u16 days_min[7]);

// Convenience: the same minute limit on every day (minutes == 0 blocks every day —
// to turn the timer OFF instead, use pctl_play_timer_clear).
Result pctl_play_timer_set_uniform(u16 minutes);
