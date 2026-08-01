; =============================================================================
; Combined-emulator headless Wolf dispatch and clean-exit fixture.
;
; Sends one Wolf3DOrig opcode-0 command through the real eZ80-to-VDP stream,
; then terminates Fab through its documented debug output port. This fixture is
; emulator-only: physical Agon hardware does not implement output port 0 as a
; host-process exit facility.
; =============================================================================

    .assume adl=1
    .org 040000h

    jp start

    .align 64
    .db "MOS",0,1

start:
    ld hl,wolf_hello
    ld bc,wolf_hello_end-wolf_hello
    rst.lil 18h

    ; The eZ80 write returns after queueing the UART bytes. Give the VDP task
    ; half a second to consume the command and emit its diagnostic before the
    ; emulator process is asked to shut down.
    ld a,08h
    rst.lil 08h
    ld hl,(ix+0)
    ld de,60
    add hl,de
    ld (exit_deadline),hl
wait_for_vdp:
    ld a,08h
    rst.lil 08h
    ld hl,(ix+0)
    ld de,(exit_deadline)
    or a
    sbc hl,de
    jr c,wait_for_vdp

    ; Fab debug I/O: writing the desired status to port 0 terminates the
    ; emulator. A=0 records success after the Wolf command has been emitted.
    xor a
    out (0),a
    ld hl,0
    ret

wolf_hello:
    db 23,0,0A0h
    dw 0
    db 04Ah,0
wolf_hello_end:

exit_deadline:
    dl 0
