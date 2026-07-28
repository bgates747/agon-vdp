# Pingo render-completion notification

Pingo render completion is an opt-in compatibility hack for stock MOS. It uses
the existing MOS keyboard-packet callback rather than adding a second UART
driver or requiring a custom MOS build.

## Enable or disable

The new Pingo subcommand is:

```text
VDU 23,0,&A0,sid; &49,41, mode, token;
```

- `mode = 0`: disable notifications.
- `mode = 1`: send a completion record through `PACKET_KEYCODE`.
- `token`: caller-selected 16-bit value, little-endian on the wire.
- Any unsupported mode disables notifications.

The setting belongs to one Pingo control structure. It is disabled by default
and returns to disabled whenever that control is initialized again. Existing
applications therefore receive no new traffic.

## Completion record

After a successful `render to bitmap` subcommand 38, mode 1 emits this ordinary
VDP protocol packet:

```text
81 0A 50 33 44 52 01 01 tt tt ss ss
```

All multibyte fields are little-endian:

| Payload byte | Meaning |
| --- | --- |
| 0–3 | ASCII magic `P3DR` |
| 4 | protocol version, currently 1 |
| 5 | event type, 1 = render complete |
| 6–7 | caller token |
| 8–9 | low 16 bits of the render sequence |

The leading `81 0A` is the stock VDP framing for packet type 1 with a ten-byte
payload, making a twelve-byte UART frame. This deliberately matches the size
of the established stock mouse event and stays below the eZ80's sixteen-byte
receive FIFO. An earlier experimental record used all sixteen payload bytes
accepted by MOS, but repeated fields that a one-render-in-flight client already
owns. Hardware testing did not establish that the larger record was being
lost; the observed timeout was ultimately traced to the client's setup queue.
The compact record is retained as the version-1 ABI because it is sufficient,
has more UART margin, and is now qualified on hardware.

The notification is sent only after `rendererRender()` returns, any RGBA8888
compatibility expansion has completed, and Pingo has restored its private frame
pointer. A completion is emitted only after a successful render; an invalid
output bitmap does not produce one. The client already owns the one render in
flight and therefore knows its target bitmap, so repeating that bitmap ID and
a success status in the record would add no correlation value.

## MOS callback

An application installs a receiver with MOS API `mos_setkbvector` (`0x1D`).
MOS calls it with `DEU` pointing at its 16-byte protocol-data buffer. The
callback must remain short and register-safe because it runs from the UART
interrupt path.

The callback must first check the `P3DR` magic. Normal keyboard events use the
same packet type and still reach the callback, so non-matching four-byte
keyboard records must be handled according to the application's keyboard
policy.

MOS resumes its ordinary keyboard handling after the callback returns. A
recognized completion record would otherwise be interpreted as a fabricated
key event (`P`, modifier byte `3`, virtual key `D`, and down byte `R`). A stock
MOS callback should therefore:

1. copy all 10 completion bytes into an application-owned mailbox;
2. set its one-byte ready flag only after that copy is complete;
3. overwrite source payload bytes 0–3 with zero; and
4. return with an ordinary `RET`.

MOS will still increment its key-event counter for that neutralized record,
but it will see a null key-up event and will not leave a fabricated key held.
Do not move the magic beyond byte 3 as a workaround: ordinary four-byte
keyboard records do not clear bytes 4–9 of MOS's shared protocol buffer, so a
tail signature could survive from an earlier completion.

The callback runs inside MOS's UART interrupt path. It should perform bounded
memory work only—no MOS calls, VDU writes, rendering, file I/O, or console
output. MOS preserves the application's `AF`, `BC`, `DE`, and `HL` around its
UART interrupt handler, but not `IX` or `IY`; a callback must preserve either
index register it uses. `DEU` contains the payload pointer, and the callback
returns with `RET`, not `RETI`.

## Control lifetime and correlation

The render sequence belongs to the Pingo control structure, not to an
application invocation. Only its low sixteen bits are transmitted. An eZ80 or
MOS reset does not necessarily reset VDP state, and creating an already-existing
control ID is rejected rather than replacing it. Applications must therefore
not assume that their first completion sequence is zero.

For one-render-in-flight clients, correlate a completion using the magic,
protocol version, event type, and caller token. The client already knows which
bitmap its sole outstanding render targets. Accept the first returned sequence
as the baseline and require modulo-65536 monotonic increments thereafter if
sequence validation is desired.

## First-render queue barrier

VDU writes are asynchronous. Texture upload, scene construction, display
allocation, and the first render command can all be waiting in VDP's input
queue while the eZ80 continues running. A client that starts a first-render
timeout when it merely transmits command 38 can therefore measure the preceding
setup backlog as if it were render latency.

Before timing the first render, send a stock general-poll marker:

```text
VDU 23,0,&80,&A5
```

Wait until MOS `sysvar_gp` (`IX+&37`) becomes `&A5`, then start the render
timeout and submit command 38. Subsequent renders in the one-in-flight state
machine naturally follow a received completion, so they do not require another
setup barrier.

The hardware fixture also leaves the active video mode and final framebuffer
intact when it exits. An earlier version restored mode 0 immediately after
receiving the delayed completion, hiding the valid rendered image before the
physical monitor could resynchronize. That was a display-lifetime problem, not
a notification failure.

## Asynchronous client pattern

The intended application structure is a single-render-in-flight state machine:

1. install the MOS callback;
2. enable notifications;
3. submit an absolute object/camera state and one render;
4. continue updating world state and consuming input in the foreground;
5. consume the committed completion mailbox;
6. display/flip the completed bitmap; and
7. submit the newest absolute state when no render is in flight.

This keeps world-state ownership on the eZ80 and naturally coalesces obsolete
intermediate visual states. On exit, first disable Pingo notifications, then
clear `mos_setkbvector`; otherwise a later keyboard packet could jump into
application memory that MOS has already reused.

## Known boundary

The implementation deliberately uses `VDUStreamProcessor::send_packet()`, the
normal VDP response path. Consequently, explicit buffered-command output
redirection also redirects this packet. Direct application-issued Pingo
commands use the normal MOS UART stream.

Pingo rendering itself currently blocks VDP's core-0 command loop. The eZ80
can keep simulating while a frame renders, but VDP-originated keyboard packets
queued during that interval are not transmitted until `rendererRender()`
returns. Removing that frame-length input latency would require a later,
broader change to the VDP task/transport architecture; it is not part of this
minimal notification hack.
