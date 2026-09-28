# NTP packet tests

Compile the real pure C parser from the repository root:

```sh
gcc -std=c11 -Wall -Wextra -Werror -Isource/util \
  source/util/ntp_packet.c tests/ntp_packet/test.c -o /tmp/ntp-packet-test
/tmp/ntp-packet-test
rm -f /tmp/ntp-packet-test
```

Assertions cover truncated packets, every LI/version/mode value, accepted and
rejected strata, each mismatched origin byte, unknown transmit timestamps,
date limits, and the 2036 NTP era boundary using explicit wire values.

The network module uses connected UDP port 123, tries at most three resolved
addresses, and sets a three-second receive timeout for each. The response origin
must echo the random request cookie. The caller receives the server transmit
time, rounded down to seconds; the function does not modify the system clock.
The accepted date interval is 2020-01-01 through the end of 2099. It selects the
unique era 0 or era 1 representation without relying on an incorrect local clock.

Protocol source: [RFC 5905](https://www.rfc-editor.org/rfc/rfc5905.html), sections
6 and 7.3 for timestamp format, era rollover and packet fields, and section 8 for
origin/transmit matching. Reference Timestamp is the server's last clock update;
Transmit Timestamp is the time returned by this implementation.

This module checks protocol consistency and source matching. It does not provide
authenticated NTP or compensate for network delay. Tests do not perform network
requests or validate Switch networking. At authoring time the tests have not
been run locally; run the command in Ubuntu CI.
