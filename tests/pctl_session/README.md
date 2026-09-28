# Host lifecycle tests

These tests compile the real `source/pctl_ops.c` against a minimal `switch.h`.
The fake service counts active references, initialization, release, applet calls,
and mutation attempts. Assertions detect leaks, duplicate acquisition, invalid
release, IPC without a session, unauthorized timer writes, and replay after errors.

Run from the repository root with GCC:

```sh
gcc -std=c11 -Wall -Wextra -Werror -Itests/pctl_session -Isource \
  source/pctl_ops.c tests/pctl_session/test.c -o /tmp/pctl-session-write
/tmp/pctl-session-write
gcc -std=c11 -Wall -Wextra -Werror -DPCTL_READ_ONLY=1 \
  -Itests/pctl_session -Isource \
  source/pctl_ops.c tests/pctl_session/test.c -o /tmp/pctl-session-read-only
/tmp/pctl-session-read-only
rm -f /tmp/pctl-session-write /tmp/pctl-session-read-only
```

The writable tests cover all enabled/restricted/temporary-unlock combinations,
all three timer write entry points (including clear), failed initialization and
IPC, and session release before and after a failed system PIN applet.
The read-only tests ensure mutations and the applet are blocked in the service
layer. Both builds exercise repeated query/status/dump calls, every diagnostic
reconnect failure, query validity on IPC errors, and idempotent initialization.

The stub does not emulate Horizon IPC descriptors, OS service internals, game
restriction behavior, sleep/wake behavior, or a physical console. Passing these
tests establishes host-level ownership and write-gate behavior only.

At authoring time these tests have not been executed locally; the default WSL
environment did not expose GCC. Run both commands in Ubuntu CI before accepting
their result.
