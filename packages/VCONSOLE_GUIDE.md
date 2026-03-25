# Aria Virtual Console — Package Guide

The Aria virtual console is a PICO-8-style fantasy console built from five cooperating packages. Together they simulate an 8-bit hardware platform where Aria programs run against a fixed memory map, SNES-style input, 4-channel audio, and terminal-based display.

## Architecture Overview

```
┌────────────────────────────────────────────────────────┐
│                    Game / Cart                         │
│                  (user Aria code)                      │
├──────────┬──────────┬──────────┬──────────┬────────────┤
│ aria-    │ aria-    │ aria-    │ aria-    │ aria-      │
│ input    │ display  │ audio   │ color   │ console    │
│ (POSIX)  │ (ANSI)   │ (ALSA)  │ (pure)  │ (bus mem)  │
├──────────┴──────────┴──────────┴──────────┴────────────┤
│              Linux / POSIX terminal                    │
└────────────────────────────────────────────────────────┘
```

## Packages

### aria-console — Memory Map & Bus

**Type**: C shim + pure Aria  
**Purpose**: Defines the 64KB virtual address space and provides read/write access to an in-memory byte array.

| Zone | Address Range | Size | Purpose |
|------|--------------|------|---------|
| RAM  | 0x0000–0x1FFF | 8 KB | Work RAM |
| DISP | 0x2000–0x27FF | 2 KB | Display registers |
| INP  | 0x2800–0x2FFF | 2 KB | Input registers |
| SND  | 0x3000–0x37FF | 2 KB | Audio registers |
| SYS  | 0x3800–0x3FFF | 2 KB | System (IRQ, timers) |
| VRAM | 0x4000–0x7FFF | 16 KB | Tile data + tile map |
| ROM  | 0x8000–0xFFFF | 32 KB | Cartridge ROM |

**Key functions**:
- `bus_init()` / `bus_reset()` — initialize/clear memory
- `bus_read(addr)` / `bus_write(addr, val)` — byte access
- `bus_read16(addr)` / `bus_write16(addr, val)` — 16-bit LE access
- `bus_fill(start, len, val)` — fill range
- `bus_zone(addr)` — decode address to zone ID
- `con_ms_to_frame(ms)` / `con_is_vblank(ms)` — frame timing at 60fps

---

### aria-input — Keyboard Input

**Type**: C shim (POSIX termios)  
**Purpose**: Raw keyboard input with SNES-style button mapping, frame-based state tracking, and edge detection.

**Button layout** (12 buttons, SNES-style):
```
D-pad:  RIGHT(1) LEFT(2) DOWN(4) UP(8)
Face:   A(16) B(32) X(64) Y(128)
Shoulder: L(256) R(512)
System: START(1024) SELECT(2048)
```

**Default key bindings**: WASD → D-pad, JKLS → face buttons, QE → shoulder, Enter → START, `[` → SELECT. Arrow keys also map to D-pad.

**Key functions**:
- `inp_enter_raw()` / `inp_exit_raw()` — toggle terminal raw mode
- `inp_update()` — call once per frame; drains pending keys, updates button state
- `inp_btn(mask)` — 1 if button held this frame
- `inp_btnp(mask)` — 1 if button **just pressed** this frame (edge detect)
- `inp_btnr(mask)` — 1 if button **just released** this frame (edge detect)
- `inp_bind(ascii, mask)` — remap a key to a button

---

### aria-display — Terminal Graphics

**Type**: C shim (ANSI escape codes)  
**Purpose**: Terminal-based rendering via ANSI escape sequences. Cursor control, color, line/box drawing.

**Key functions**:
- `disp_raw_enter()` / `disp_raw_exit()` — terminal raw mode
- `disp_clear()` / `disp_move(col, row)` / `disp_home()` — screen control
- `disp_set_fg(color)` / `disp_set_bg(color)` — 16-color palette (0-15)
- `disp_hline(...)` / `disp_vline(...)` / `disp_box(...)` — drawing primitives
- `disp_print_at(col, row, fg, bg, text)` — positioned text output
- `disp_size()` → packed int64 (use `disp_cols`/`disp_rows` to unpack)
- `disp_flush()` — flush output buffer

---

### aria-audio — Sound Synthesis

**Type**: C shim (ALSA on Linux)  
**Purpose**: 4-channel phase-accumulator synthesis with ADSR envelopes and real audio output.

**Channels**: 0–3, independent. **Waveforms**: square (0), triangle (1), sawtooth (2), noise/LFSR (3).

**Key functions**:
- `audio_init(dry)` — dry=1 for testing (silent), dry=0 for ALSA output
- `audio_play_tone(ch, freq, wf, dur)` — start a tone (dur=0 means sustain)
- `audio_stop_channel(ch)` / `audio_stop_all()` — silence
- `audio_set_volume(ch, vol)` — volume 0–15
- `audio_set_envelope(ch, atk, dec, sus, rel)` — ADSR envelope (ms/ms/level/ms)
- `audio_envelope_level(ch, elapsed)` — compute current amplitude
- `audio_tick()` — render 735 samples (one 60fps frame) and write to ALSA
- `audio_note_freq(midi)` — MIDI note → Hz lookup

---

### aria-color — Color Utilities

**Type**: Pure Aria (no FFI)  
**Purpose**: RGBA color packing/unpacking, transforms, and an indexed 16-color palette.

**Color layout**: `R[31:24] G[23:16] B[15:8] A[7:0]` packed in uint64.

**Key functions**:
- `color_pack(r, g, b, a)` / `color_r` / `color_g` / `color_b` / `color_a` — pack/unpack
- `color_invert(packed)` / `color_gray(packed)` — transforms
- `pal_get(idx)` — 16-color PICO-8 palette lookup (returns packed RGBA)
- `pal_to_ansi(idx)` — map palette index to nearest ANSI terminal color (0-15)
- `PAL_SIZE()` — 16

## Typical Game Loop

```
use "../aria-input/src/aria_input.aria".*;
use "../aria-display/src/aria_display.aria".*;
use "../aria-audio/src/aria_audio.aria".*;

func:main = int32() {
    drop(inp_enter_raw());
    drop(disp_raw_enter());
    drop(audio_init(0i32));

    // game loop
    for (int32:running = 1i32; running == 1i32; running = running) {
        drop(inp_update());

        if (raw(inp_btnp(raw(BTN_A()))) == 1i32) {
            // A button just pressed this frame
        }
        if (raw(inp_btn(raw(BTN_UP()))) == 1i32) {
            // UP held
        }

        drop(disp_clear());
        // ... draw ...
        drop(disp_flush());
        drop(audio_tick());
    }

    drop(audio_shutdown());
    drop(disp_raw_exit());
    drop(inp_exit_raw());
    pass(0i32);
};
```
