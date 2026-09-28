# Clock host tests

Compile the real `source/time_ops.c` against this minimal libnx service stub.
The stub checks handle ownership, parent/child close order, command direction,
write attempts, and independent clock and accuracy results.

Run both variants from the repository root in Ubuntu CI:

```sh
gcc -std=c11 -Wall -Wextra -Werror -Itests/time_ops -Isource \
  source/time_ops.c tests/time_ops/test.c -o /tmp/time-ops-write
/tmp/time-ops-write
gcc -std=c11 -Wall -Wextra -Werror -DPCTL_READ_ONLY=1 \
  -Itests/time_ops -Isource \
  source/time_ops.c tests/time_ops/test.c -o /tmp/time-ops-read-only
/tmp/time-ops-read-only
rm -f /tmp/time-ops-write /tmp/time-ops-read-only
```

Coverage includes root opening failures, each clock child opening failure,
each clock reading failure, failed automatic/accuracy queries, failed network
write, failed readback, and release of every acquired handle on all paths.
Automatic correction false or unavailable prevents writes. Successful write
readback accepts a delta of 0 through 5 seconds and rejects negative deltas,
6 seconds, and unsigned wraparound. An accuracy flag that is false or failed
stays false even when write verification succeeds. Read-only builds never send
a write. Repeated snapshots and dumps must leave no idle handles.

These tests do not emulate IPC descriptors, official time synchronization,
Horizon clock permissions, NTP responses, game restrictions, or sleep/wake.
At authoring time GCC was unavailable locally; run both variants in CI before
reporting them as passed.
