# CCSDS TC Receive Plan

## Current State

The CCSDS module already has several pieces needed for a telecommand receive
path, but the TC path is not complete:

- `ccsds_cltu_decode_message()` can decode one complete CLTU into TC transfer
  frame bytes using the BCH primitive. It validates the fixed 64-bit CLTU tail
  sequence `c5 c5 c5 c5 c5 c5 c5 79` and excludes that final codeblock from
  BCH decode, but CLTU fill handling still needs to be implemented and tested.
- `ccsds_cltu_rx_push()` is still a streaming-acquisition stub.
- `ccsds_tc_frame_decode()` is still a stub.
- `ccsds_tc_frame_extract_packet()` currently assumes the decoded TC frame data
  contains one complete Space Packet.
- `ccsds_router` can dispatch decoded Space Packets by APID.
- The TM generator already reserves a 4-byte OCF field in every generated TM
  transfer frame, but currently zero-fills it.
- The TM side has a UDP route when `CONFIG_NETWORKING=y`.

This plan covers TC receive work that complements the existing TM generator.
The exact TC frame fields, COP-1/FARM behavior, control-command decoding, and
CLCW bit layout must be verified against the relevant CCSDS standards and the
available reference implementations before implementation. Do not infer those
details from memory.

## Design Goals

- Keep TC decoding independent of any transport.
- Accept complete CLTUs as the first TC input unit.
- Decode CLTUs into TC transfer frames using verified CLTU start, BCH block,
  tail-sequence, and fill handling.
- Decode TC transfer frames enough to reject malformed input, reject wrong
  spacecraft IDs, identify control frames, and expose the frame data field.
- Log every rejected CLTU or TC frame with a concise reason.
- Keep UDP TC input as a network-gated integration adapter when networking is
  active.
- Include the small spacecraft-side COP-1/FARM acceptance state with TC frame
  decode unless the implementation grows large enough to justify a split.
- Generate a CLCW from TC/COP-1 state and inject it into the TM OCF field.
- Reassemble TC Space Packets after frame acceptance, not inside the frame
  parser.
- Route complete decoded TC Space Packets through the existing APID router.
- Add a small number of harmless core-platform test TC packets for end-to-end
  validation.

## Non-Goals

- Do not redesign AkiraOS networking or radio abstractions.
- Do not bind core TC decode to UDP, UART, RF, or any other device interface.
- Do not update Zephyr.
- Do not edit generated build files.
- Do not scan the Zephyr tree unless the local API documentation/code requires
  it.
- Do not implement packet reassembly before TC frame acceptance and CLCW
  reporting are testable.
- Do not implement command side effects that can reset, flash, transmit, or
  mutate persistent configuration in the first test-packet slice.

## Receive Pipeline

The core pipeline should be transport-neutral:

```text
complete CLTU
  -> ccsds_cltu_decode_message()
  -> ccsds_tc_frame_decode()
  -> spacecraft ID check
  -> control-frame detection
  -> COP-1/FARM acceptance and CLCW state update
  -> TC Space Packet reassembly
  -> APID router
```

UDP is a network-gated input adapter for integration testing:

```text
UDP datagram containing one complete CLTU
  -> TC receive core
```

The UDP adapter should not parse TC frame fields and should not own COP-1/FARM
state. It only receives complete CLTU datagrams and passes them into the
transport-neutral TC receive path.

Every CLTU or TC frame reject should produce a concise log message that names
the failing layer and reason, for example CLTU decode failure, malformed TC
frame, wrong spacecraft ID, unsupported control command, COP-1/FARM rejection,
packet reassembly overflow, or APID dispatch failure. Logs should include small
metadata such as input length, spacecraft ID, VCID, control/bypass state, and
frame sequence number when those fields were decoded. Do not dump full CLTU,
frame, or command contents by default.

## CLTU Acquisition and Decode

The complete-CLTU decoder should validate the CLTU boundary before TC frame
decode:

1. Validate the CLTU start sequence.
2. Validate the fixed CLTU tail sequence `c5 c5 c5 c5 c5 c5 c5 79` as the
   final 8-byte codeblock.
3. Exclude the tail sequence from BCH data-block decode.
4. Decode only complete BCH(63,56) codeblocks before the tail sequence.
5. Accept permitted CLTU codeblock fill only according to verified rules.
6. Reject trailing data after the tail sequence unless a verified profile rule
   permits it.
7. Report malformed start, missing tail, invalid tail, non-block-aligned BCH
   body, invalid BCH parity, and invalid fill distinctly enough for tests and
   counters.

UDP and future transport adapters still pass one bounded CLTU unit into the
TC receive core. They should not parse tail sequences themselves.

## TC Frame Decode

The first `ccsds_tc_frame_decode()` implementation should be intentionally
small:

1. Validate input pointers and minimum size.
2. Decode the TC transfer-frame fields required by the selected profile.
3. Reject frames whose spacecraft ID does not match
   `CONFIG_AKIRA_CCSDS_SPACECRAFT_ID`.
4. Decode the virtual channel ID used by the TC frame.
5. Decode whether the frame is bypass, sequence-controlled, or a control frame,
   using the exact verified field definitions.
6. Decode the frame sequence number when present.
7. Apply the small spacecraft-side COP-1/FARM acceptance rules for the selected
   profile, using per-received-VC state.
8. Update the CLCW-producing state after accepted/rejected TC frames.
9. Expose the frame data field as a view into the provided buffer when the
   frame carries packet data.
10. Consume decoded TC transfer-frame bytes after CLTU tail and fill handling
   has already been completed by the CLTU layer.

The TC frame path should not:

- Dispatch APIDs.
- Reassemble Space Packets.
- Depend on UDP or any other transport.

## COP-1/FARM State

For the AkiraOS spacecraft-side receive profile, COP-1/FARM should initially
live with TC frame decoding if the reference implementation remains small and
clear. The goal is to keep the acceptance path obvious rather than creating
module boundaries for a small state update.

```text
src/connectivity/ccsds/ccsds_tc_frame.h
src/connectivity/ccsds/ccsds_tc_frame.c
```

If later control-command handling, multiple TC links, richer diagnostics, or
test setup make the file unwieldy, split the state into `ccsds_cop1.h` and
`ccsds_cop1.c` then. Do not split it preemptively.

Exact state names, transitions, and control-command effects must still come
from the standards or reference implementations.

Initial responsibilities:

- Initialize per-received-VC TC receive state.
- Accept a decoded TC frame view.
- Reject wrong-state or unsupported frame classes cleanly.
- Detect control frames and route them through a control-command handler.
- Track the next expected frame sequence number per received TC VC.
- Track enough per-VC state to generate a CLCW for any reportable TC VC.
- Expose counters/status useful for shell diagnostics and tests.

The first implementation may return `-ENOTSUP` for unsupported control commands
after detecting that a frame is a control frame. That is preferable to inventing
control-command behavior.

## CLCW Provider And TM OCF Injection

The TM generator should not know COP-1 internals. Add a CLCW provider API to
the TM frame module, for example:

```c
typedef int (*ccsds_tm_clcw_provider_t)(uint32_t *clcw, void *user_data);

int ccsds_tm_frame_set_clcw_provider(ccsds_tm_clcw_provider_t fn,
                                     void *user_data);
```

The provider does not need a TM VCID argument. For this profile, the virtual
channel reported in the CLCW is derived from TC receive/COP-1 state, not from
the TM virtual channel that happens to be carrying the OCF. COP-1/FARM must
still store the next expected frame sequence number per received TC VC so the
CLCW report value field can be generated for any reportable TC VC.

TM frame formatting should:

1. Build the TM primary header.
2. Fill the TM data field.
3. Ask the registered CLCW provider for the current CLCW.
4. Write the 4-byte CLCW into the reserved OCF field.
5. Append optional TM FECF.
6. Apply optional RS coding.
7. Route the generated output as before.

If no CLCW provider is registered, the current zero-filled OCF behavior should
remain the default.

## UDP TC Input Adapter

The UDP input adapter already exists and is compiled only when networking is
available:

```text
src/connectivity/ccsds/ccsds_tc_udp_input.h
src/connectivity/ccsds/ccsds_tc_udp_input.c
```

Current intended behavior:

- Compile only when `CONFIG_NETWORKING=y`.
- Bind to a Kconfig-configured local UDP port.
- Treat each UDP datagram as one complete CLTU.
- Reject datagrams larger than `CONFIG_AKIRA_CCSDS_MAX_CLTU_LEN` and log the
  reject.
- Pass accepted datagrams to the TC receive core.
- Maintain simple counters: datagrams received, CLTU decode failures, TC frame
  rejects, control frames seen, packets dispatched.

When networking is disabled, shell/status commands should report TC UDP input
as unavailable rather than failing ambiguously.

## TC Packet Reassembly

After frame decode and COP-1/FARM acceptance are testable, add a TC packet
reassembly module, for example:

```text
src/connectivity/ccsds/ccsds_tc_reassembly.h
src/connectivity/ccsds/ccsds_tc_reassembly.c
```

Responsibilities:

- Maintain partial packet state across accepted TC frames.
- Decode Space Packet primary headers only after enough bytes are available.
- Enforce `CONFIG_AKIRA_CCSDS_TC_MAX_SPACE_PACKET_LEN`.
- Emit complete encoded Space Packet bytes or decoded packet views.
- Reset cleanly after malformed length, overflow, or explicit resync.
- Dispatch complete packets through `ccsds_router`.

The existing `ccsds_tc_frame_extract_packet()` can remain as a narrow helper for
single-packet tests, but it should not be the long-term reassembly boundary.

## Core Platform Test Packets

Add a small, harmless set of TC packet handlers for end-to-end validation after
CLTU decode, TC frame decode, CLCW injection, and basic dispatch are working.

Initial candidates:

- No-op or ping command.
- Request one APID 0 time telemetry packet.
- Query TC/COP-1 counters.

These handlers should live outside the CCSDS parser/decoder modules. They are
platform behavior selected by AkiraOS, not CCSDS protocol mechanics.

## Suggested First Implementation Slice

1. [x] Rename the existing TM plan to `TM_PLAN.md`.
2. [x] Add this `TC_PLAN.md`.
3. [ ] Add focused tests using supplied complete CLTU vectors.
4. [x] Add CLTU tail-sequence detection, validation, stripping, and tests.
5. [x] Implement TC frame decode enough to reject malformed input and wrong
   spacecraft IDs.
6. [x] Detect control frames and return a clear unsupported-control result until
   exact handling is implemented from references.
7. [ ] Add minimal COP-1/FARM acceptance and CLCW-producing state in the TC
   frame path.
8. [ ] Add a CLCW provider and inject its value into TM OCF.
9. [ ] Add tests proving generated TM output carries the expected CLCW.
10. [x] Add UDP input for complete CLTU datagrams when
    `CONFIG_NETWORKING=y`.
11. [ ] Add tests/status polish for UDP input.
12. [ ] Add TC packet reassembly.
13. [ ] Add harmless core-platform test APIDs.

## Suggested Tests

- Complete supplied CLTU decodes into TC frame bytes.
- CLTU decode rejects missing or malformed tail sequences.
- CLTU decode strips the tail sequence before BCH codeblock decode.
- CLTU decode rejects non-block-aligned BCH body before the tail sequence.
- CLTU decode handles permitted codeblock fill according to verified vectors.
- Rejected CLTUs and TC frames emit concise reject logs.
- TC frame decode rejects invalid lengths.
- TC frame decode rejects wrong spacecraft ID.
- TC frame decode identifies control frames from supplied vectors.
- Unsupported control frame returns a clear error without corrupting state.
- TC frame COP-1/FARM state tracks next expected FSN independently per received
  TC VC.
- TC frame COP-1/FARM state produces the expected CLCW for supplied vectors.
- TM idle output carries the provider CLCW in OCF.
- TM packet-bearing output carries the provider CLCW in OCF.
- With TM FECF enabled, FECF covers the TM frame including the OCF/CLCW.
- With RS enabled, RS parity is computed after CLCW insertion.
- UDP input is unavailable when `CONFIG_NETWORKING=n`.
- UDP input accepts one complete CLTU datagram when `CONFIG_NETWORKING=y`.
- TC packet reassembly emits one complete Space Packet from an accepted frame.
- TC packet reassembly emits one complete Space Packet split across accepted
  frames.
- Core test APID receives a no-op or ping packet through the router.

## Open Questions

- Which reference implementation should be treated as authoritative for TC
  frame field layout and COP-1/FARM edge cases?
- Which control commands are required for the first AkiraOS profile?
- Should the CLCW provider report the last received TC VC by default, or a
  profile-selected TC VC, when multiple TC VCs are active?
- Should UDP TC input be activated from the existing CCSDS shell, from Kconfig,
  or both?
- Which UDP local port should be the default for complete CLTU injection?
- Should TC receive counters be exposed through shell only, telemetry only, or
  both?
