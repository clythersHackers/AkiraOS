# CCSDS Shell User Manual

This document covers the CCSDS shell commands currently available in AkiraOS.
The commands are development-oriented and manually controlled: booting the board
does not start CCSDS telemetry or telecommand input, and `ccsds tm init` does
not select any output route by default.

## What The TM Commands Do

The CCSDS TM path builds Transfer Frames from queued CCSDS Space Packets. With
the current default profile, emitted output is a coded CADU:

```text
ASM || TM transfer frame || Reed-Solomon parity
```

The shell controls three separate things:

- TM state: initialize, start, stop, and inspect the generator.
- Destinations: callbacks that can receive already-coded TM output.
- Routes: per-VC masks that choose which destinations receive each VC's frames.

Destinations must be available before a route can use them. Routes are per
virtual channel, so VC 0 and VC 7 can be sent to different destinations or to
multiple destinations at the same time.

## Basic Workflow

Initialize CCSDS TM state:

```text
ccsds tm init
```

Inspect available destinations:

```text
ccsds tm route info
```

Select where generated frames should go:

```text
ccsds tm route set 0 log
ccsds tm route set 7 log
```

Start TM generation:

```text
ccsds tm start
```

Stop TM generation:

```text
ccsds tm stop
```

`ccsds tm init` registers destinations but leaves all VC route masks set to
`none`. This keeps telemetry quiet until a route is explicitly selected.

The shell owns the manual control commands and its concise `log` destination.
The `udp` destination is implemented as a CCSDS route module, so it can be
registered by non-shell code as well.

## Commands

### `ccsds tm init`

Initializes TM frame state and route registration state. It also stops any
running APID 0 time packet producer before resetting TM state.

Registered destinations:

- `log`
- `udp`, only when `CONFIG_NETWORKING=y`

Default VC routes after init:

```text
VC 0: none
VC 1: none
VC 2: none
VC 3: none
VC 4: none
VC 5: none
VC 6: none
VC 7: none
```

### `ccsds tm start`

Starts the internal TM generator and starts APID 0 time packets on VC 0.

If no VC has queued packet data, the generator emits idle output on VC 7. Output
is only delivered to destinations selected by the current VC route mask.

### `ccsds tm stop`

Stops APID 0 time packets and stops the internal TM generator.

### `ccsds tm status`

Shows manual activation state, recent log-route metadata, and the current route
mask for each VC.

The route lines use stable destination names:

```text
route vc=0 mask=0x00000000 routes=none
route vc=7 mask=0x00000001 routes=log
```

When multiple destinations are selected, the names are comma-separated:

```text
route vc=0 mask=0x00000009 routes=log,udp
```

### `ccsds tm route info`

Shows the destinations supported by the current build.

Example with networking disabled:

```text
dest log: available=1 route=log
dest udp: available=0 route=udp reason=networking-disabled
```

Example with networking enabled:

```text
dest log: available=1 route=log
dest udp: available=1 route=udp peer=192.168.1.255:5005
```

The UDP peer comes from:

```text
CONFIG_AKIRA_CCSDS_TM_UDP_DEST_IP
CONFIG_AKIRA_CCSDS_TM_UDP_DEST_PORT
```

### `ccsds tm route list`

Lists the current route mask for each VC. This requires `ccsds tm init` first.

### `ccsds tm route set <vcid> <routes>`

Replaces the route mask for one VC.

Examples:

```text
ccsds tm route set 0 log
ccsds tm route set 7 udp
ccsds tm route set 0 log,udp
```

### `ccsds tm route add <vcid> <routes>`

Adds one or more destinations to the existing route mask for one VC.

Example:

```text
ccsds tm route set 0 log
ccsds tm route add 0 udp
```

If both destinations are available, VC 0 is routed to both `log` and `udp`.

### `ccsds tm route del <vcid> <routes>`

Removes one or more destinations from the existing route mask for one VC.

Example:

```text
ccsds tm route del 0 log
```

### `ccsds tm route clear <vcid>`

Sets one VC route mask to `none`.

Example:

```text
ccsds tm route clear 7
```

## TC Commands

The CCSDS TC shell commands control a development UDP input for complete CLTU
datagrams. The UDP input is available only when `CONFIG_NETWORKING=y`.

Each received UDP datagram is treated as one complete CLTU and is passed into
the shared TC receive path. The UDP adapter does not own TC decode state, APID
routing policy, COP-1/FARM state, or packet reassembly. Decode and dispatch
counters are shared across complete-CLTU input sources so future UART or RF
inputs can report through the same status path.

Decoded TC transfer frames must use version 0, reserved bits set to zero, a
matching configured spacecraft ID, and a frame length field matching the
received frame size. Control frames are identified and rejected as unsupported
commands by the current receive path.

### `ccsds tc start udp`

Starts the UDP listener for complete TC CLTU datagrams.

When networking is enabled, the listener binds to:

```text
CONFIG_AKIRA_CCSDS_UDP_LOCAL_IP
CONFIG_AKIRA_CCSDS_UDP_LOCAL_PORT
```

Bounded output units use the configured peer endpoint:

```text
CONFIG_AKIRA_CCSDS_UDP_PEER_IP
CONFIG_AKIRA_CCSDS_UDP_PEER_PORT
```

With frame support enabled, each input datagram remains one complete TC CLTU.
With frame support disabled, each input datagram is one encoded Space Packet.

When networking is disabled, this command reports that TC UDP input is
unavailable.

### `ccsds tc stop udp`

Stops the UDP TC input listener.

When networking is disabled, this command reports that TC UDP input is
unavailable.

### `ccsds tc status`

Shows shared TC receive counters for complete-CLTU inputs.

```text
cltu_rx=<count> oversize=<count> cltu_fail=<count> frame_reject=<count> control=<count>
dispatch_ok=<count> dispatch_fail=<count> last_error=<errno>
last_cltu_len=<bytes> last_tc_frame_len=<bytes>
```

### `ccsds tc status udp`

Shows UDP listener state and UDP transport counters.

With networking enabled, status includes:

```text
ccsds tc udp available=1 running=<0|1>
local_port=5005 peer=<address>:5005
udp_rx=<datagrams> udp_tx=<datagrams> udp_last_error=<errno>
```

With networking disabled, status reports:

```text
ccsds tc udp available=0 running=0
```

### Basic TC UDP Smoke Test

Build and flash with CCSDS enabled, then start the TC UDP listener from the
device shell:

```text
ccsds tc start udp
ccsds tc status udp
```

Confirm that the listener is running and note the local port:

```text
ccsds tc udp available=1 running=1
local_port=5005
```

From a host on the same network, send this CLTU test vector to the device IP
address and TC UDP local port:

```sh
echo 'EB900123456789ABCD90C5C5C5C5C5C5C579' | xxd -r -p | nc -u -w1 <device-ip> 5005
```

This vector is a minimal AkiraOS decoder test input: CLTU start sequence
`EB90`, one valid BCH(63,56) block, and the fixed CLTU tail sequence
`C5C5C5C5C5C5C579`.

Check UDP transport status:

```text
ccsds tc status udp
```

Expected UDP status after one received datagram:

```text
udp_rx=1 udp_last_error=<negative ENOTSUP value>
```

Check shared TC receive status:

```text
ccsds tc status
```

Expected shared TC status changes:

```text
cltu_rx=1 oversize=0 cltu_fail=0 frame_reject=1 control=0
dispatch_ok=0 dispatch_fail=0 last_error=<negative ENOTSUP value>
last_cltu_len=18 last_tc_frame_len=7
```

For this UDP smoke test, `ENOTSUP` means the datagram reached the listener and
the CLTU passed into the TC receive path. The numeric errno can differ between
native simulator and MCU targets. A result with `udp_rx=0` means the UDP
datagram did not reach the listener; first check the device IP address and the
printed `local_port`.

## Route Names

Supported route names in the current shell:

```text
log
udp
```

Routes can be separated by commas, plus signs, pipes, or separate shell
arguments:

```text
log,udp
log+udp
log|udp
log udp
```

## Destination Notes

### Log

The `log` destination records concise frame metadata:

- VCID.
- Routed output length.
- Master channel frame count.
- Virtual channel frame count.
- First header pointer.
- Whether output begins with the CADU ASM.

It does not dump full frame contents.

### UDP

The `udp` destination sends the already-coded TM output bytes in one UDP
datagram. It is development-only and is available only when Zephyr networking is
enabled.

The default peer is `192.168.1.255:5005`, a directed broadcast address for a
typical `192.168.1.0/24` LAN. The route enables socket broadcast support before
sending.

With the current default Reed-Solomon interleave depth of 5, the CADU is 1279
bytes, which fits under a typical 1500-byte Ethernet/WiFi MTU as a UDP payload.
Higher interleave depths or smaller path MTUs may require a payload guard or a
different transport strategy.
