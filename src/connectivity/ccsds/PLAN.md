# CCSDS TM Frame Generator Plan

## Current State

The CCSDS module currently has the lower-level building blocks needed for a
basic receive/transmit path:

- `ccsds_cltu` decodes incoming CLTUs into TC frame bytes using BCH error
  correction.
- `ccsds_bch` provides the CLTU BCH primitive.
- `ccsds_rs` generates Reed-Solomon parity for outgoing TM frame coding.
- A generic CCSDS CRC-16 primitive has been added for optional packet CRC use
  and optional TM Frame Error Control Field (FECF) generation.
- `ccsds_space_packet` encodes and decodes CCSDS Space Packets. (*provisional, probably refactor*)
- `ccsds_router` dispatches decoded Space Packets by APID.
- `ccsds_tm_frame` admits complete encoded Space Packets into per-VC queues.
- `ccsds_tm_frame` also has route registration and per-VC route masks for
  dispatching generated TM frames to destination callbacks.
- Kconfig already exposes `CONFIG_AKIRA_CCSDS_RS`, default `y`, to select
  Reed-Solomon coding for TM frame output.

The main missing piece on the transmit side is the frame generator:

```text
queued Space Packets -> TM transfer frame -> configured coding -> route callbacks
```

The plan below is intentionally scoped to that missing middle. It should not
redesign the CCSDS module, update Zephyr, or bind the core CCSDS code directly
to a specific transport.

## Design Goals

- Keep CCSDS transport-neutral.
- Preserve the current per-VC packet admission API.
- Generate complete TM frames from queued packets.
- Make Reed-Solomon, TM frame CRC/FECF, and Space Packet CRC independently
  configurable so low-power builds can avoid redundant error detection work.
- When `CONFIG_AKIRA_CCSDS_RS=y`, support Reed-Solomon parity using the existing
  `ccsds_rs_encode()` primitive.
- When TM frame FECF is enabled, append a CCSDS CRC-16 FECF to the TM transfer
  frame. This should default off when RS is enabled.
- Dispatch complete frames through the existing route callback table.
- Allow example destinations such as log, capture, UDP, UART, or RF to register
  as callbacks without becoming part of the core frame builder.
- Make the first implementation testable on native simulation before wiring to
  hardware-specific device interfaces.

## Non-Goals

- Do not redesign AkiraOS networking or radio abstractions.
- Do not implement full TC frame receive support in this phase.
- Do not change generated build files.
- Do not expose frame generation as an application-driven polling loop; cadence
  should be owned inside the CCSDS TM generator service.
- Do not make the TM generator depend directly on Zephyr UDP, UART, CAN, or RF
  drivers.

## Error Control Configuration

Error detection and correction should be independently configurable because
AkiraOS may run this path on a single low-power CPU, and packet CRC, TM FECF,
and Reed-Solomon coding can overlap in what they detect.

Existing Kconfig:

```text
CONFIG_AKIRA_CCSDS_RS
CONFIG_AKIRA_CCSDS_TM_FECF
CONFIG_AKIRA_CCSDS_TM_MAX_SPACE_PACKET_LEN
CONFIG_AKIRA_CCSDS_TC_MAX_SPACE_PACKET_LEN
```

Proposed Kconfig additions:

```text
CONFIG_AKIRA_CCSDS_SPACE_PACKET_CRC
```

Suggested defaults:

- `CONFIG_AKIRA_CCSDS_RS=y` by default, as it is today.
- `CONFIG_AKIRA_CCSDS_TM_FECF=n` by default.
- `CONFIG_AKIRA_CCSDS_TM_MAX_SPACE_PACKET_LEN=4096` by default.
- `CONFIG_AKIRA_CCSDS_TC_MAX_SPACE_PACKET_LEN=4096` by default.
- `CONFIG_AKIRA_CCSDS_SPACE_PACKET_CRC` should be profile-selected rather than
  always on. It may be omitted for TM packets when RS or TM FECF is enabled,
  but this is a mission/profile decision rather than a core rule.

The CRC primitive should always be generic. Kconfig should control where the CRC
is applied, not whether the shared `ccsds_crc16` helper exists.

## Public Generator API

The generator control API in `ccsds_tm_frame.h` is:

```c
int ccsds_tm_frame_start(k_timeout_t active_delay, k_timeout_t idle_delay);
int ccsds_tm_frame_stop(void);
```

The normal generator cadence is internal to `ccsds_tm_frame.c` as a Zephyr
delayable work item. Applications enqueue packets and configure routes; they do
not have to drive the frame generator cycle-by-cycle.

The internal generator cycle should:

1. Scan VCs in increasing VCID order, starting at VC 0.
2. Find the lowest-numbered VC with waiting packet data.
3. Generate one coded output frame for that VC.
4. If no VC has waiting packet data, generate one idle output frame.

This gives deterministic priority service: lower-numbered VCs win whenever they
have waiting packets.

Do not implement fairness by rotating VC priority. Instead, the internal
generator service should use different delays after active and idle cycles:

- After packet-bearing output, schedule the next generator cycle quickly.
- After idle output, schedule the next generator cycle with a longer idle delay.

This lets the generator re-check high-priority VCs quickly after servicing them.
If they are empty on the next pass, lower-priority waiting VCs are emitted soon
instead of being delayed by the idle cadence.

The generator decides whether to emit packet-bearing output or idle output from
the queue state it observes. That decision should remain internal to the CCSDS
TM frame generator.

The existing packet admission and route APIs remain unchanged:

```c
int ccsds_tm_frame_add(uint8_t vcid, const uint8_t *packet, size_t packet_len,
                       k_timeout_t timeout);

int ccsds_tm_frame_register_route(ccsds_tm_route_mask_t route_bit,
                                  ccsds_tm_route_fn_t fn, void *user_data);

int ccsds_tm_frame_set_vc_route(uint8_t vcid,
                                ccsds_tm_route_mask_t route_mask);
```

## Frame Generator Responsibilities

The generator should live in `ccsds_tm_frame.c` near the existing VC state and
private `route_frame()` helper.

For each generated packet-bearing output frame:

1. Validate initialization, VCID, and route mask state.
2. Build the TM primary header.
3. Fill the frame data field from the selected VC queue.
4. Track packet continuation state when a packet spans multiple frames.
5. Insert idle/fill data when the queued packet data does not fill the frame.
6. Reserve space for the Operational Control Field (OCF), initially zero-filled
   or otherwise marked unused until CLCW/reporting support exists.
7. Apply the configured frame coding:
   - If `CONFIG_AKIRA_CCSDS_RS=y`, generate Reed-Solomon parity over the TM
     transfer frame with `ccsds_rs_encode()`, prepend the ASM, and append the
     parity symbols to produce a CADU.
   - If `CONFIG_AKIRA_CCSDS_TM_FECF=y`, append a CCSDS CRC-16 FECF to the end
     of the TM transfer frame before any RS parity is computed.
8. Increment master channel and virtual channel frame counters.
9. Dispatch the completed coded output frame through `route_frame()`.

For idle output:

1. Select a configured idle VC, initially VC 7 unless a profile chooses another.
2. Build a TM frame containing only idle/fill data.
3. Apply the same configured coding path as packet-bearing frames: optional TM
   FECF, then optional RS CADU wrapping.
4. Dispatch it through the idle VC's route mask.

### Header Fields

Use `CONFIG_AKIRA_CCSDS_SPACECRAFT_ID` for the spacecraft ID.

Use the selected `vcid` for the virtual channel ID.

Use the existing counters already present in `ccsds_tm_frame.c`:

- `mcfc` for the master channel frame count.
- `vcs[vcid].vcfc` for the per-VC frame count.

The first implementation should keep optional features minimal:

- Reserve the 4-byte OCF location in the TM frame layout, but do not implement
  live OCF/CLCW contents yet.
- No secondary header.
- No security header/trailer.

### TM Frame, CADU, and CRC Sizing

When `CONFIG_AKIRA_CCSDS_RS=y`, the Reed-Solomon primitive uses:

```text
data symbols   = 223 * CONFIG_AKIRA_CCSDS_RS_INTERLEAVE_DEPTH
parity symbols =  32 * CONFIG_AKIRA_CCSDS_RS_INTERLEAVE_DEPTH
```

With the current default interleave depth of 5, this gives:

```text
TM transfer frame / RS data region = 1115 bytes
RS parity                          = 160 bytes
ASM                                = 4 bytes
CADU total                         = 1279 bytes
```

The generator should treat `CONFIG_AKIRA_CCSDS_MAX_FRAME_LEN` as the maximum TM
transfer frame length. For the default RS profile, this is also the RS data
region passed into `ccsds_rs_encode()`. The object sent to physical-layer style
routes is a CADU:

```text
CADU = ASM || TM transfer frame || RS parity
```

This means the implementation should not use `CONFIG_AKIRA_CCSDS_MAX_FRAME_LEN`
as the total transmitted CADU buffer size. It is correct as the TM transfer frame
size, but the generated/routed CADU needs room for the attached sync marker and
Reed-Solomon parity as well.

The first implementation should make that distinction explicit with local helper
constants, for example:

```c
#define CCSDS_TM_ASM_LEN 4u
#define CCSDS_TM_OCF_LEN 4u
#define CCSDS_TM_FECF_LEN 2u
#define CCSDS_TM_FRAME_LEN CCSDS_RS_INTERLEAVED_DATA_LEN
#define CCSDS_TM_CADU_LEN \
    (CCSDS_TM_ASM_LEN + CCSDS_TM_FRAME_LEN + CCSDS_RS_INTERLEAVED_PARITY_LEN)
```

The packet/fill data area must be sized after reserving any enabled trailing TM
fields:

```text
packet/fill capacity = TM frame length - primary header - OCF reserve - FECF if enabled
```

For the first implementation, reserve `CCSDS_TM_OCF_LEN` even though the OCF
contents are not yet meaningful. This keeps the frame layout compatible with
future operational control reporting.

When `CONFIG_AKIRA_CCSDS_TM_FECF=y`, append the CCSDS CRC-16 FECF to the end of
the TM transfer frame:

```text
TM frame with FECF = TM transfer frame body || CRC-16 FECF
```

In that mode, the packet/fill data area must reserve two bytes for the FECF so
the TM transfer frame does not exceed `CONFIG_AKIRA_CCSDS_MAX_FRAME_LEN`. The
CRC should cover the TM transfer frame bytes before the FECF, using the CCSDS
CRC-16 parameters for the selected profile.

If both TM FECF and RS are enabled, compute and append the FECF first, then
compute RS parity over the complete TM transfer frame including the FECF. The
default should avoid this double protection at frame level by leaving TM FECF
off when RS is enabled.

The CRC implementation should be generic, not TM-specific, because the same
CCSDS CRC-16 is used by packets as well as the TM FECF path. Add it as a shared
primitive, for example:

```text
src/connectivity/ccsds/ccsds_crc16.h
src/connectivity/ccsds/ccsds_crc16.c
```

Suggested API:

```c
uint16_t ccsds_crc16_compute(const uint8_t *data, size_t len);
uint16_t ccsds_crc16_update(uint16_t crc, const uint8_t *data, size_t len);
```

The implementation can be bitwise first or table-driven. If a verified lookup
table is available, use it to keep the implementation small and deterministic.

### Idle/Fill Data

`ccsds_space_packet_encode()` currently requires a non-empty payload, so idle
fill should not be forced through that API.

For packet-bearing frames, fill unused frame data bytes while formatting the
emitted frame, not while admitting packets. The current implementation writes an
APID 2047 idle Space Packet when enough room remains for a valid packet header
and payload byte; if fewer bytes remain, it zero-fills the tail.

If no VC contains waiting packet data during a generator cycle, the generator
should emit one coded idle output frame rather than returning without output.
This keeps the downlink cadence continuous. The first pass should make the idle
VC explicit with a local constant or Kconfig option rather than hiding it in
route logic.

## Packet Queue Handling

The existing admission function validates complete encoded Space Packets before
writing them into a per-VC `k_pipe`.

The generator needs to read from that pipe without corrupting packet boundaries.
The current VC state already includes:

```c
size_t packet_rem;
bool packet_is_idle;
```

`packet_rem` tracks whether the generator is in the middle of a packet that did
not fit in the previous frame.

Suggested behavior:

- At the start of each generator cycle, inspect VCs in increasing VCID order
  starting from VC 0.
- Select the lowest-numbered VC with queued packet data or an unfinished packet
  continuation.
- Generate at most one coded output frame for that VC.
- If no VC has queued packet data or an unfinished continuation, generate one
  coded idle output frame.
- If `packet_rem == 0`, read the next Space Packet primary header first.
- Decode the packet length from the primary header.
- Copy as much of the packet as fits in the remaining frame data area.
- If the full packet does not fit, store the remaining byte count in
  `packet_rem`.
- On the next generated frame for that VC, continue copying packet bytes before
  trying to read a new packet.
- When a continuation consumes the whole frame data area and no packet starts in
  that frame, set the first header pointer to `0x7ff`.
- When a continuation is followed by another queued packet start in the same
  frame, set the first header pointer to that packet start offset.

The current `k_pipe` byte stream is workable for this, but it means the
generator must be careful when peeking or reading headers. If Zephyr pipe peek
support is awkward for the target version, a minimal internal staging buffer may
be cleaner than changing the public admission API.

## Route Callback Examples

Example callbacks should be added outside the core builder, likely as:

```text
src/connectivity/ccsds/ccsds_tm_routes.h
src/connectivity/ccsds/ccsds_tm_routes.c
```

These callbacks demonstrate destination registration without hard-coding
transport dependencies into the TM generator.

### Log Route

Purpose: development visibility.

Route bit: `CCSDS_TM_ROUTE_LOG`

Behavior:

- Log VCID.
- Log frame length.
- Log a short header summary such as MCFC and VCFC.
- Do not dump full frame contents by default.

### Capture Route

Purpose: tests and simple integration demos.

Route bit: `CCSDS_TM_ROUTE_ARCHIVE`

Behavior:

- Copy the last routed frame into a caller-provided buffer.
- Store the last VCID and frame length.
- Return `-ENOSPC` if the capture buffer is too small.

This route is useful for native tests because it verifies generation and routing
without a real network or device.

### Generic Send Route

Purpose: common adapter shape for UDP, UART, RF, or other transports.

Route bits: transport-specific, such as `CCSDS_TM_ROUTE_UDP` or
`CCSDS_TM_ROUTE_UART`

Suggested shape:

```c
typedef int (*ccsds_tm_send_fn_t)(const uint8_t *frame, size_t frame_len,
                                  void *user_data);

struct ccsds_tm_send_route {
    ccsds_tm_send_fn_t send;
    void *user_data;
};

int ccsds_tm_route_send(uint8_t vcid, const uint8_t *frame,
                        size_t frame_len, void *user_data);
```

`ccsds_tm_route_send()` would adapt the CCSDS route callback signature to a
transport-specific send function. UDP, UART, and RF can each provide their own
`send` function and context.

## Suggested First Implementation Slice

Implement the generator in small, testable slices:

1. [x] Add a generic CCSDS CRC-16 primitive in `ccsds_crc16.h` and
   `ccsds_crc16.c`.
2. [x] Add `ccsds_tm_frame_start()` and `ccsds_tm_frame_stop()`.
3. [x] Add an internal delayable-work generator service in
   `ccsds_tm_frame.c`.
4. [x] Keep the first generator service minimal: start/stop state, queued-byte
   detection, lowest-numbered active VC selection, active/idle delay selection,
   and scheduling behavior only.
5. [x] Add focused native tests for start/stop behavior and active/idle delay
   selection.
6. [x] Build one minimal idle TM transfer-frame candidate when no VC has queued
   packet bytes.
7. [x] Use VC 7 as the initial idle VC.
8. [x] Fill the idle frame data area with deterministic idle/fill bytes while
   reserving space for the OCF.
9. [x] Reserve and zero-fill the initial OCF field.
10. [x] Apply configured coding to idle output:
   - RS enabled: prepend ASM and append RS parity around the TM transfer frame.
   - TM FECF enabled: append CCSDS CRC-16 FECF inside the TM transfer frame
     before RS parity is computed.
11. [x] Dispatch the coded idle output through the existing route masks and route
    callbacks.
12. [x] Add a capture route or test-local callback for idle frame tests.
13. [x] Build packet-bearing output frames for the lowest-numbered VC with
    waiting packet bytes or unfinished continuation.
14. [x] Apply the configured coding to packet-bearing output:
   - TM FECF enabled: append CCSDS CRC-16 FECF inside the TM transfer frame.
   - RS enabled: prepend ASM and append RS parity around the TM transfer frame.
15. [x] Add native tests for the CRC primitive and generator.
    - [x] CRC primitive tests.
    - [x] Generator start/stop and cadence tests.
    - [x] Idle frame generation, configured coding, and route dispatch tests.
    - [x] Packet-bearing generated frame tests.

Do not add real UDP/UART/RF device code in this first slice.

The first internal generator service should use two cadences:

- Active cadence: short delay after the generator finds waiting packet data.
- Idle cadence: longer delay after the generator emits idle output.

An optional later improvement is a bounded burst mode where the internal service
runs several immediate generator cycles after packet-bearing output, stopping
when it emits idle output or reaches a configured burst limit.

### Current Status

The current implementation can emit coded idle TM output and packet-bearing TM
output through the existing route callback table.

Implemented and tested:

- `ccsds_tm_frame_start()` and `ccsds_tm_frame_stop()` exist in the public API.
- `ccsds_tm_frame.c` owns an internal Zephyr delayable-work item.
- The service records start/stop state.
- The service can scan VC queues in increasing VCID order.
- The service can choose active cadence when any VC has queued bytes.
- The service can choose idle cadence when all VC queues are empty.
- Native tests cover start/stop state and active/idle cadence selection.
- The idle generator builds a deterministic TM transfer frame on VC 7 when no
  VC has queued packet bytes.
- The idle frame uses the configured spacecraft ID, master channel frame count,
  VC frame count, `first_header_pointer = 0x7fe`, zero-filled idle data, and a
  reserved zero-filled 4-byte OCF.
- `CONFIG_AKIRA_CCSDS_TM_FECF` exists and appends a CCSDS CRC-16 FECF before
  RS parity when enabled.
- `CONFIG_AKIRA_CCSDS_TM_MAX_SPACE_PACKET_LEN` defaults to 2048 bytes to keep
  target DRAM use bounded while still allowing packet continuation on the
  default RS frame profile.
- `CONFIG_AKIRA_CCSDS_TC_MAX_SPACE_PACKET_LEN` defaults to 4096 bytes.
- With `CONFIG_AKIRA_CCSDS_RS=y`, idle output is routed as a CADU:
  `ASM || TM transfer frame || RS parity`.
- Native tests cover idle route dispatch, VC 7 selection, OCF zero-fill,
  counter increments, RS CADU wrapping, and FECF placement/checking.

Manual activation status:

- `CONFIG_AKIRA_CCSDS_SHELL` gates the development shell commands and defaults
  to `y` when Zephyr shell support is enabled.
- `ccsds tm init` initializes TM frame state and registers available
  destinations. It leaves all VC route masks set to `none` until a route is
  explicitly selected.
- `ccsds tm start` starts the TM generator and starts APID 0 time packets on
  VC 0.
- `ccsds tm stop` stops APID 0 time packets and then stops the TM generator.
- `ccsds tm status` reports manual activation state and the latest concise
  route metadata, plus current per-VC route masks after initialization.
- `ccsds tm route info` reports destinations available in the current build.
- The development log route reports VCID, output length, MCFC, VCFC, FHP, and
  whether the output begins with the CADU ASM. It does not dump full frames by
  default.
- No boot/init path calls `ccsds_tm_frame_init()`.
- No boot/init path calls `ccsds_tm_frame_start()`.
- Booting the ESP32-S3 will not start the generator or transmit CCSDS TM.
- Native tests build and run the CCSDS shell helper coverage.
- `./build.sh -b akiraconsole -ccsds` links successfully with the bounded
  2048-byte default TM packet/queue profile.

The packet-bearing implementation produces one output frame per active generator
cycle for the lowest-numbered VC with waiting packet bytes or an unfinished
packet continuation. It uses the same TM primary-header builder, OCF
reservation, optional FECF, RS CADU coding, and route callback table already
proven by the idle path. The frame formatter carries packet continuation across
frame boundaries, packs additional queued packet starts when space remains, and
only inserts APID 2047 idle fill while formatting the emitted frame.

APID 0 spacecraft time telemetry is also available as an explicit producer:

- `ccsds_time_packet_build()` builds an APID 0 Space Packet with a 6-byte
  big-endian CUC-style payload: `uint32 seconds`, then `uint16 fine time`
  in units of 1/65536 second.
- `ccsds_time_packet_build_now()` derives the timestamp from Zephyr kernel
  ticks.
- `ccsds_time_packet_start()` starts a 10-second delayable producer that queues
  time packets onto the selected VC. It is not auto-started at boot.

For the current default profile, `CONFIG_AKIRA_CCSDS_RS=y`, generated output
routes should receive a CADU:

```text
ASM || TM transfer frame || RS parity
```

This is now runnable on ESP32-S3 as a manually activated development TM source:
the board can boot normally, then a developer can run `ccsds tm init`, select
routes with `ccsds tm route set` or `ccsds tm route add`, and run
`ccsds tm start` from the shell to observe VC 7 idle output and APID 0 time
packets on VC 0 through the selected destinations.

## Suggested Tests

Add a new test file:

```text
tests/src/test_ccsds_crc16.c
tests/src/test_ccsds_tm_frame.c
```

Update `tests/CMakeLists.txt` to compile:

- `ccsds_crc16.c`
- `ccsds_space_packet.c`
- `ccsds_tm_frame.c`
- the new test files

Initial tests:

- Generic CCSDS CRC-16 matches known vectors.
- Incremental `ccsds_crc16_update()` matches one-shot
  `ccsds_crc16_compute()`.
- `ccsds_tm_frame_init()` clears counters, routes, and VC state.
- Invalid route bit registrations are rejected.
- Invalid VC route masks are rejected.
- A valid route callback receives a generated frame.
- Multiple route bits call multiple callbacks for the same generated frame.
- VC route masks isolate frames by virtual channel.
- Generator cycles select the lowest-numbered waiting VC.
- The internal generator service uses a shorter delay after packet-bearing
  output than after idle output.
- A high-priority VC that becomes empty lets lower-priority waiting VCs emit on
  the next quick cycle.
- A generator cycle emits idle output when no VC has waiting packet data.
- Generated TM frames reserve the 4-byte OCF field before optional FECF and
  coding.
- With `CONFIG_AKIRA_CCSDS_RS=y`, generated output includes ASM and RS parity of
  the RS data region.
- With `CONFIG_AKIRA_CCSDS_TM_FECF=y`, generated output includes a CCSDS
  CRC-16 FECF at the end of the TM transfer frame.
- With both RS and TM FECF enabled, the RS parity is computed after the FECF is
  appended.
- Oversize packet admission returns `-EMSGSIZE`.
- A packet larger than one frame data area continues across generated frames.

## Completed Phase: Manual Target Activation

This phase makes the TM generator observable on a target board without starting
it automatically at boot and without committing CCSDS to a concrete transport
such as UDP, UART, or a radio frontend.

The Kconfig-gated development shell surface is:

```text
CONFIG_AKIRA_CCSDS_SHELL
```

The shell provides explicit manual activation commands:

```text
ccsds tm init
ccsds tm start
ccsds tm stop
ccsds tm status
```

Route control commands are documented in the completed route-control phase
below.

`ccsds tm start` starts the TM generator and the APID 0 time packet producer on
VC 0. `ccsds tm stop` stops both. Separate time-packet start/stop shell commands
are intentionally omitted for this first target activation slice.

The implementation keeps activation manual:

- Do not call `ccsds_tm_frame_init()` from boot/init code.
- Do not call `ccsds_tm_frame_start()` from boot/init code.
- Do not start APID 0 time packets at boot.
- Do not add UDP/UART/RF adapters in this phase.

For first visibility, the shell can register a development log route. The route
registers through the existing TM route callback table and logs concise frame
metadata when explicitly selected:

- VCID.
- Routed output length.
- Master channel frame count and VC frame count.
- First header pointer.
- Whether output is a CADU with ASM or an uncoded TM transfer frame.

It does not dump full frames by default. If a hexdump is useful later, gate it
behind a separate debug option and limit it to the first few bytes.

This phase lets a developer boot the board normally, run shell commands, select
a route, and observe idle VC 7 frames and APID 0 time packets on VC 0 through
the chosen destination.

## Completed Phase: Configurable Log/UDP TM Routes

The first route-control slice is implemented for the development log route and
a minimal UDP development route. The current route table already uses a bitmask,
so shell control exposes both replacement and additive operations:

```text
ccsds tm route info
ccsds tm route list
ccsds tm route set <vcid> <routes>
ccsds tm route add <vcid> <routes>
ccsds tm route del <vcid> <routes>
ccsds tm route clear <vcid>
```

Supported route names in this slice:

```text
log
udp
```

Implemented behavior:

- `set` replaces the VC route mask.
- `add` ORs one or more route bits into the VC route mask.
- `del` clears one or more route bits from the VC route mask.
- `clear` sets the VC route mask to `CCSDS_TM_ROUTE_NONE`.
- `list` shows each VC route mask using stable names such as `log` and `udp`.
- Commands should reject unsupported route bits and invalid VCIDs before
  changing state.
- `ccsds tm status` also reports the current per-VC route masks after
  initialization.
- `ccsds tm route info` reports destinations available in the current build.
- `ccsds tm init` initializes TM frame state, registers the shell development
  log route, registers the UDP development route module when
  `CONFIG_NETWORKING=y`, and leaves all VC route masks set to `none`.
- When `CONFIG_NETWORKING=n`, log route control still works and selecting UDP
  returns a clear unsupported-route error.

This keeps the shell similar in spirit to CSP route manipulation while matching
the existing CCSDS TM callback bitmask model.

## Later Work

The route-control and UDP development slice is complete. Remaining TM-focused
work should add other example destinations and recorded/replay support without
tying the CCSDS core to any one transport.

### Remaining Example Destination Routes

Keep adding example route handlers outside the core TM generator:

- Development log route: already implemented for concise metadata.
- UDP route: implemented as a development-only route module when
  `CONFIG_NETWORKING=y`. It sends already-coded TM output bytes to the
  Kconfig-configured destination IPv4 address and UDP port.
- Serial device route: still to do. It should write coded TM output to a
  configured byte-stream device such as a UART, USB CDC ACM endpoint, or radio
  frontend that presents a serial interface.

Keep the adapter shape generic:

```c
typedef int (*ccsds_tm_send_fn_t)(const uint8_t *frame, size_t frame_len,
                                  void *user_data);

struct ccsds_tm_send_route {
    ccsds_tm_send_fn_t send;
    void *user_data;
};
```

Adapters should register through `ccsds_tm_frame_register_route()` and receive
already-coded output frames. They should not call the generator directly.

### Recorded TM / Archive Route

Some missions downlink live telemetry and recorded telemetry on different VCs
during a pass. A useful first profile would be:

- VC 0: live housekeeping / APID 0 time packets.
- VC 1: recorded telemetry replay.
- VC 7: idle output.

Add an archive route that can persist either:

- coded TM output frames/CADUs as emitted by the route callback, or
- decoded/admitted Space Packets before TM framing.

The first implementation should choose one storage format explicitly. Coded
frames are simplest for route capture and bit-exact replay, while packet storage
is better if replay should regenerate fresh MCFC/VCFC/FHP and coding. For a
spacecraft-style recorded TM path, packet storage is usually the cleaner
long-term model because replay can inject packets into VC 1 and let the normal
TM generator produce current transfer frames.

Potential shell commands:

```text
ccsds tm record start <vcid> <path>
ccsds tm record stop <vcid>
ccsds tm record status
ccsds tm replay <vcid> <path>
ccsds tm replay stop <vcid>
```

Storage targets may include SD files or a bounded NVRAM/flash-backed file. The
route should handle full storage gracefully and report drops/counters in status.

Replay should enqueue stored packets into the selected VC using the existing
`ccsds_tm_frame_add()` path rather than bypassing routing or frame generation.
This lets recorded TM on VC 1 share the same coding, counters, and destination
route masks as live TM.

### Other TM Follow-Ups

1. Decide whether to add bounded burst generation on top of the existing
   workqueue-driven service.
2. Clarify Kconfig naming for RS data length versus total coded frame length.
3. Add stricter idle packet support if required by the selected mission profile.
4. Consider per-VC queue sizing or active-VC masks so target builds do not
   reserve maximum packet depth for every VC.
5. Continue TC frame decode support after the TM path is stable.

## Open Questions

- Should idle-only VC 7 frames eventually contain an encoded idle Space Packet,
  or remain zero-filled with `first_header_pointer = 0x7fe`?
- Should route callback failures be aggregated and returned by the generator, or
  should generation succeed once the frame is built even if one destination
  fails?
- Which VC should carry idle output by default if a profile does not configure
  one?
- Which VC-to-route defaults should the Akira console profile eventually use?
- Should recorded telemetry be persisted as Space Packets, coded TM frames, or
  both behind separate route/storage modes?
- Should route shell commands accept numeric masks, route names, or both?
