# NX-Pctl-Manager

[![build](https://github.com/tailiang2008/NX-Pctl-Manager/actions/workflows/build.yml/badge.svg)](https://github.com/tailiang2008/NX-Pctl-Manager/actions/workflows/build.yml)
[![license: GPLv3](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE)
[![latest release](https://img.shields.io/github/v/release/tailiang2008/NX-Pctl-Manager)](https://github.com/tailiang2008/NX-Pctl-Manager/releases/latest)

A Nintendo Switch parental-controls tool — **no phone app, no Nintendo account, no internet** — configure the daily play-time limit directly on the console, and reset / delete the PIN or unlink the companion app while you're at it.

> ⚠️ **Requires custom firmware (CFW; Atmosphère recommended).** It calls the restricted system `pctl` service directly, so it only runs on a hacked console (a stock retail console can't use it). It doesn't bypass any account or online verification — it just brings the parental-controls settings that are otherwise buried in the phone app / deep in System Settings (plus the forgot-PIN recovery options) onto the console, for CFW users to do offline.
>
> It talks to restricted / `*ForDebug` `pctl` commands directly — usually fine, but in a few states (notably *removing* the play-time limit while the "time's up" lock screen is up) it can destabilise Atmosphère. **Tested on firmware 22.1.0 / Atmosphère 1.11.1; other versions are unverified. Use at your own risk.**

---

## Features

### Configure the play timer (daily limit) offline — the main point of this tool

Nintendo **only lets you set the daily play-time limit through the "Nintendo Switch Parental Controls" phone app** and sync it to the console; there's no option for it in the console's System Settings. This tool talks to the console's `pctl` service directly, so **on the console, fully offline** (no phone app, no Nintendo account, no internet) you can:

- **Set one limit for every day** — one minute value applied to Sunday–Saturday.
- **Set each day separately** — Sunday, Monday … Saturday each get their own value (edited day-by-day in a sub-menu, staged, then saved all at once).
- **Turn the limit off** — remove the play timer entirely.

`0` configures a zero-minute limit. On firmware 22.5 / Atmosphère 1.11.2, the reported configuration has not yet been shown to block games. Use *Remove play-time limit* to clear the configuration.

If the timer is enabled or reports a restriction, first enter the parent's PIN in the system parental-control screen to temporarily unlock it. The manager checks command 1006 before allowing the write. Unknown status blocks timer changes. Each operation releases its pctl session before returning to the UI, including failed operations.

### Other (parental-controls recovery / maintenance)

- **Set / change the PIN** — opens the system's parental-controls passcode screen.
- **Delete all parental controls** — wipes the PIN and every restriction (**irreversible**; confirmed before it runs). Your recovery option when you've forgotten the PIN.
- **Unlink the companion app** — breaks the link between the "Nintendo Switch Parental Controls" phone app and this console.
- **View status** — current safety level, whether a PIN is set (and its length), whether restrictions are enabled, and play-timer status (active or not / the configured daily limit).

## How to use

Controls: ↑ / ↓ move the cursor, (A) confirms, (B) goes back one level / (on the main menu) exits. Delete all parental controls, unlink companion app, and remove the limit show a confirmation screen first.

### Pick the flow that matches your console

**A. Parental controls haven't been set up on this console yet.** No companion-app pairing exists.

1. On the Switch: *System Settings* → *Parental Controls* → set a PIN. (Don't bother with the companion-app pairing — that's the whole point of this homebrew: you skip it.)
2. Launch this app from hbmenu and continue at *Set the play timer* below.

**B. Parental controls were already set up via the Nintendo Switch Parental Controls phone app.** The phone app is currently paired with this console.

1. Launch this app from hbmenu, pick *Unlink companion app* — otherwise the phone app's next sync will overwrite whatever limit you set here.
2. Continue at *Set the play timer* below.

### Set the play timer

Main menu → *Play timer (daily limit)*:

- *Set daily limit (all days)*: pops a number pad for the minutes (0–1440), applies it to every day, writes it after you confirm.
- *Per-day limits*: opens a sub-menu; press (A) on a day to type its value (it's **staged** — edited days are marked `(*)`); when you're done, pick *Save per-day limits* to write all 7 at once; press (B) to leave without saving.
- *Remove play-time limit*: turns the whole timer off (with confirmation).
- If the timer is enabled or restricted, use the system PIN screen for temporary unlock, then return to write. The manager checks status again immediately before sending the write.

### Test the session-release build on 22.5

Build with `make PROBE=1 READ_ONLY=0 dist` for the complete manager menu plus *Dump current config*. `READ_ONLY=1` builds the reduced diagnostic interface.

1. Replace the old `/switch/nx_pctl_manager.nro` with the new artifact. Verify the main menu contains PIN, daily limit, delete, and unlink actions; verify the daily-limit page contains uniform, per-day, remove, and dump actions.
2. With parental controls enabled, refresh and export one dump. The report records the actual enabled, restricted, and temporary-unlock results and confirms the tool released its session before returning. Exit with B from the main menu.
3. Use the HOME parental-control icon and enter the parent's PIN. Check whether the crash recurs. If it does, preserve the new Atmosphère reports and stop that test.
4. If HOME unlock works, reopen the manager. Temporary unlock should read `yes`. Export another dump; timer changes should pass the status gate. A failed query must show unavailable and block timer changes.
5. For game-lock acceptance, configure zero minutes for every day, restore restrictions, and test both a low-age-rating game and a previously suspended game after sleep/wake. Actual playable content is a failure even when remaining time is zero. Export a dump at each changed parental-control state or failed game-lock observation; routine navigation does not need a dump.

Build success and session-release tests do not establish that all-game blocking or sleep/wake restoration works on real hardware. `IsRestrictedByPlayTimer=false` remains the system's result until the underlying enforcement behavior is identified.

### Third-party time synchronization and timer checks

Open **时间同步与限时检查** from the main menu. Select a preset or enter an NTP hostname/IP, choose **读取服务器时间**, then **应用时间并检查限时**. Presets include:

| Provider | Host |
| --- | --- |
| Alibaba Cloud | `ntp.aliyun.com` |
| Tencent Cloud | `ntp1.tencent.com` |
| Cloudflare | `time.cloudflare.com` |
| Google | `time.google.com` |
| NTP Pool China | `cn.pool.ntp.org` |
| NTP Pool Asia | `asia.pool.ntp.org` |
| NTP Pool global | `pool.ntp.org` |

This action sends a user-initiated NTP query over UDP 123 to the selected provider and writes the returned UTC time to the system network clock using a temporary `time:s` session. Enable **Synchronize Clock via Internet** in System Settings first; the tool checks that choice and uses the selected NTP server for its own request. It does not install a replacement for the system's background synchronization. If Nintendo's servers are blocked, an accessible third-party NTP server can still supply this sample. Network reachability is checked by the request itself.

The page shows user/network clocks, the actual system automatic-correction and network-clock-accuracy results, and the actual pctl enabled/restricted/temporary-unlock results. A successful clock write, readback, or accuracy check does not establish that games are blocked. Before writing, the tool must successfully save the original clock and pctl report; afterwards it automatically saves the combined comparison under `/switch/nx_pctl_manager/logs/`. Each clock and pctl operation releases its service handles. Failed or unavailable reads remain distinct from false.

Time samples expire after 120 seconds. Requests validate the NTP response mode, version, leap status, stratum, request cookie and transmit timestamp; malformed responses are never applied. The screen shows UTC explicitly and leaves the console's timezone selection in place.

For hardware testing on 22.5:

1. Keep the zero-minute configuration and parental-control state unchanged for the time comparison. Apply a valid NTP sample and record whether **网络时钟精度足够** and **当前受到时间限制** change.
2. Exit the manager with B. Test playable content in a low-age-rating game; then test a previously suspended game after sleep/wake. Reopen the time page and export another report for each failed observation.
3. Restart into the same system environment and reopen the page to check whether the corrected clock and restriction behavior persist. Persistence and the all-game lock remain unverified until this physical-console test is completed.
4. If time is written and accuracy is sufficient but restrictions remain false, time synchronization alone has not resolved the fault. Keep the saved before/after report for the next investigation.

Reference implementations: [SwitchTime](https://github.com/3096/switch-time/blob/master/source/main.c), [libnx clock IPC](https://github.com/switchbrew/libnx/blob/master/nx/source/services/time.c). Provider references: [Alibaba Cloud](https://help.aliyun.com/zh/eci/user-guide/configure-the-ntp-service), [Tencent Cloud](https://intl.cloud.tencent.com/zh/document/product/213/32379), [Cloudflare](https://developers.cloudflare.com/time-services/ntp/usage/), [Google](https://developers.google.com/time), [NTP Pool](https://www.ntppool.org/en/zone/asia).

### Other actions

- *Set / change parental control PIN* switches to the system applet to set a passcode and returns automatically when done.
- *Delete all parental controls* wipes the PIN and every restriction (with confirmation, **irreversible**). Also your recovery option when you've forgotten the PIN.
- The main menu always shows a status panel; *Refresh status* re-reads it.

## Install

Grab `nx_pctl_manager.nro` from the [**Releases**](../../releases) page, drop it in `/switch/` on the SD card, and run it from hbmenu. (Or download `nx_pctl_manager.zip` from the same release and unzip it onto the SD card root — it puts the `.nro` in `/switch/` for you.)

## Build from source

Needs [devkitPro](https://devkitpro.org/) and the `switch-dev` toolchain (libnx included):

```sh
export DEVKITPRO=/opt/devkitpro
make            # produces ./nx_pctl_manager.nro
make dist       # packs nx_pctl_manager.zip — unzip it straight onto the SD card
make nxlink     # build and push over nxlink to a Switch running hbmenu on the LAN
make clean
```

Without a native toolchain, the repo's `./run.sh` builds via the official Docker image `devkitpro/devkita64`: `./run.sh` just builds; `./run.sh <switch-ip>` builds then pushes over nxlink (press Y in hbmenu first).

## License

GPLv3 (see [`LICENSE`](LICENSE)).

### Third-party

The graphical UI (v3.0.0+) is built on **[borealis](https://github.com/xfangfang/borealis)** — a Horizon-system-style UI library for Switch homebrew — under the Apache License 2.0. The pinned commit lives at `extern/borealis/`; see its `LICENSE` and `NOTICE`. The text-console v2 line did not depend on borealis.
