---
layout: page
title: The Emulator
---

**YAKC** means 'Yet Another KC emulator'. I might come up with a better name some day.

The [github repo is here](https://github.com/floooh/yakc)

It is written in a simple C++11 (meaning no exceptions, no RTTI, minimal STL 
usage), and aims to be fast, simple and portable. The most important 
deployment platform is the web browser via emscripten, but it is also 
possible to build native versions for OSX, Linux, Windows, Android and iOS.

Extra care has been taken to keep the client size small for the 
HTML5 platform. The compressed size for the complete emulator
with debugging UI is under 400 kBytes (compressed), without the debugging UI
the compressed size is under 200 kBytes.

The emulator consists of 4 main components:

- **the 'core'**: this is the actual emulator code as a set of low-level classes
in header files, these have no dependencies apart from the standard C and 
C++ headers
- **debugging UI**: the debugger UI overlay has been implemented with
[Ocornut's imgui](https://github.com/ocornut/imgui)
- **application wrapper**: my 'weekend engine' [Oryol](https://github.com/floooh/oryol) is used
as portable application wrapper
- **build system**: another of my side project, [the fips buildsystem](https://github.com/floooh/fips) is used to build the whole thing, fips is essentially a
layer above cmake to simplify the setup of multi-platform projects

Python is used for code-generation to create the Z80 instruction decoder and
dump binary files like ROMs and tape files into C headers.

### Further dependencies:

Oryol depends on a few other 3rd party libs that are used indirectly:

- **[GLFW](https://github.com/glfw/glfw)**: GLFW is used as window system and
  input wrapper on OSX, Linux and Windows if the OpenGL rendering backend is
  used
- **[flextgl](https://github.com/ginkgo/flextGL)**: is used as OpenGL extension
wrangler
- **Unittest++**: is used for unit tests (there doesn't seem to be an official
github repo, so here's only the link to my [fipsified version](https://github.com/floooh/fips-unittestpp))

I also have to credit [MAME](https://github.com/mamedev/mame) 
and Alexander Lang's Javascript [KC emulator](http://lanale.de/kc85_emu/KC85_Emu.html)
for providing hints when I was stuck.

### Emulated Features:

The emulator is fairly complete, but not perfect:

- emulates a KC85/3 or (soon) KC85/4 with switchable CAOS operating system ROMs
- fast and nearly complete documented Z80 emulation passing ZEXDOC, instructions
are emulated with their proper cycle counts
- expansion slot system with RAM and ROM modules
- internal RAM/ROM bank switching
- properly emulates the CTC and PIO registers and their interaction with the
rest of the system (with one exception: the sound volume in PIO-B)
- emulates the look of an old-school CRT color or black-and-white TV
- KCC file format loading

Unsupported features:

- floppy disk device and the CP/M OS
- cassette loading/saving (the audio emulation is not precise enough for
saving, and the hardware emualation for loading has not been implemented)
- hardware quirks like the 'display snowing/needling'
- the serial keyboard input logic is not emulated, instead the key codes
are patched directly into the right operating system locations
- the Z80 emulation doesn't emulate the following features: non-maskable
interrupts, interrupt modes 0 and 1, extra memory wait-states


### The Emulator Core

The core classes are under **yakc_core**.

#### Low-level hardware components:

- **clock**: this provides a central system clock and counters
- **memory**: the memory class maps the 64kByte Z80 address range to 
host memory in 8 KByte pages. It also implements a simple bank-switching 
logic to map memory pages in and out of the visible address range.

#### Z80 emulation:

- **z80**: the z80 class is the core of the emulator, it provides a
very fast and (nearly) complete emulation of the Z80 (enough to run the
ZEXDOC tests by Frank D. Cringle), a large part of the z80 class is
code-generated via python 
- **z80ctc**: a simple and incomplete emulation of the Z80 CTC chip, 
currently just enough to run the KC85 emulator
- **z80pio**: likewise, a simple and incomplete emulation of the Z80 PIO chip,
just enough for the KC85
- **z80int**: this implements the interrupt controller device logic in a 
'daisy chain', used to prioritize interrupt requests from the CTC and PIO
- **z80dbg**: this class bundles some low-level debugging functions, for 
instance a single breakpoint, and a history ring-buffer for the PC register

#### KC85 emulation:

- **kc85_video**: the kc85_video class emulates the video memory scan-out logic,
mainly a method to decode the KC85 video memory into a linear
RGBA8 image (which is then usually copied into an OpenGL texture)
- **kc85_audio**: this is a simple interface class to the KC85 sound generation
hardware (which is just a CTC-driven oscillator connected to an internal speaker),
it provides callbacks to control start/stop/frequency and volume of sound
- **kc85\_expansion and kc85\_module**: these 2 classes emulate the expansion
slot system of the KC85, only RAM and ROM expansion modules are currently
implemented
- **kc85**: this is the emulators main class which embeds, connects and 
controls all previously described components

### How the Z80 emulator works:

The core of the Z80 emulation is the **z80::step()** method, which executes
the next instruction and returns the number of CPU cycles taken. The
method's body is generated by a python script and is essentially a huge,
nested switch-case statement on one or more instruction-bytes, where 
each case-branch implements a single instruction (with very few 
special-case exceptions).

The register file is a simple struct, 8-bit register pairs and their
16-bit combinations are grouped into unions. Memory access is wrapped
by an embedded object of the 'memory' class. Instructions change
the state of the registers or memory, and potentially update the
flags register. 

Some instructions use lookup tables for the resulting flags, some compute
the flags (I tried out different combinations of flag lookup and 
combinations until I ended up with the fastest execution of the ZEXDOC
test).

The R register is incremented once per fetched instruction-byte 
and wraps around at 0x7F.

The increment/decrement-and-repeat instructions (e.g. LDIR, CPIR) implement
their loops by resetting the PC to the start of the instruction as long
as the instruction isn't done yet.

The IN and OUT instructions simply invoke externally provided callbacks
(these are implemented in the kc85 class).

For interrupt handling, a simple daisy-chain protocol is implemented, which
prioritizes interrupts between different interrupt controllers (in the
KC85 emulation, only the CTC and PIO act as interrupt controllers).

Non-maskable interrupts, and interrupt modes 0 and 1 are currently not
implemented (these are not needed for the KC85).

The Z80 emulation is quite fast. On a mid-2014 MacBookPro with 2.8 GHz i5 
CPU, a Z80 running at 1.2 GHz can be emulated, which results in around
150 MIPS (the fastest Z80 instructions take 4 cycles, the slowest 23 cycles).

The emulator is running the ZEXDOC conformance test without errors, which means
that the documented CPU flags are computed correctly, and most undocumented
instructions have been implemented.

#### How the emulator core works:

The most important methods of the kc85 class are **poweron()**, 
**reset()** and **onframe()**.

The poweron() method performs power-on initialization, reset() performs 
a 'soft-reset', and onframe() runs the emulator for a single 16.6ms frame. 

What happens during an emulator-frame:

- input is handled once per frame, this is a hack, the proper
  input handling (which is not implemented) is described 
  [here](https://github.com/floooh/yakc/blob/master/scribble/kc85_3_kbdint.md)
- the clock computes how many CPU cycles to run for this frame (at 60fps and
1.75MHz Z80 speed, this results in around 29.2k CPU cycles)
- the instruction-loop is executed until the computed number of cycles
is reached:
    - the z80::step() function is called, this executes the next instruction
      and returns the number of cycles taken
    - the clock-timers and CTC are updated, this may cause to trigger hardware-
      lines (like the vsync), and interrupts to be triggered
    - the z80::handle_irq() method is called to handle resulting interrupt
      requests
    - if the emulated CPU executes an IN/OUT instruction, the kc85::in_cb() 
      and kc85::out_cb() callbacks are invoked, these callbacks
      read or write PIO/CTC registers, control expansion modules and
      memory bank switching

The emulator does not directly interact with the host system, platform-specific
things like keyboard input, video and audio output need to be handled outside
in the wrapper application.

### The Oryol Wrapper Application

The wrapper application code is in the **yakc_app** directory:

- **YakcApp in Main.cc**: this is the Oryol application subclass which 
initializes the required Oryol modules:
    - **Gfx**: required to render the emulator video output to screen as a 
      fullscreen rectangle
    - **IO and HTTP**: are needed to load KCC file from a web server
    - **Input**: is required to get keyboard and mouse input
    - **Synth**: a software audio synthesizer module to generate audio via OpenAL
- **Draw**: the Draw class creates all the required rendering resources and
  per-frame decodes the KC85 video memory into a texture, and render a fullscreen
  rectangle using a CRT-effect shader
- **Audio**: the Audio class is triggered by callbacks from the kc85_audio
  class and forwards sound generation to the Oryol Synth module using 
  a simple square wave generator

### The Debugging UI

The debugger UI is implemented with Ocornut's imgui and can be toggled
by pressing TAB or double-tapping (on touch-controlled platforms).

The UI currently has the following features:

- cold- and warm-start
- load and start KCC files
- boot the emulator into different configurations
- step-debugger, disassembler and memory editor windows
- inspect the PIO and CTC registers
- visualize the current memory map
- add and remove modules from expansion slots
- control various settings



