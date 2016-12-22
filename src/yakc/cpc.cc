//------------------------------------------------------------------------------
//  cpc.cc
//
//  TODO:
//  - audio!
//  - improve CRTC emulation so that demos work (implement CRTC closer
//    to the real thing, a cascade of counters, and decode video memory
//    per CRTC cycle, not per line)
//  - subtle differences between different CRTC types
//  - ROM module switching
//  - KC Compact differences (color ROM, CIO instead of gate array, ...)
//  - support more emulator file formats
//
//------------------------------------------------------------------------------
#include "cpc.h"

#include <stdio.h>

namespace YAKC {

//------------------------------------------------------------------------------
void
cpc::init_key_mask(ubyte ascii, int col, int bit, int shift) {
    YAKC_ASSERT((col >= 0) && (col < 10));
    YAKC_ASSERT((bit >= 0) && (bit < 8));
    YAKC_ASSERT((shift >= 0) && (shift < 2));
    this->key_map[ascii] = key_mask();
    this->key_map[ascii].col[col] = (1<<bit);
    if (shift != 0) {
        this->key_map[ascii].col[2] = (1<<5);
    }
}

//------------------------------------------------------------------------------
void
cpc::init_keymap() {
    // http://cpctech.cpc-live.com/docs/keyboard.html
    clear(this->key_map, sizeof(this->key_map));
    const char* kbd =
        // no shift
        "   ^08641 "
        "  [-97532 "
        "   @oure  "
        "  ]piytwq "
        "   ;lhgs  "
        "   :kjfda "
        "  \\/mnbc  "
        "   ., vxz "

        // shift
        "    _(&$! "
        "  {=)'%#\" "
        "   |OURE  "
        "  }PIYTWQ "
        "   +LHGS  "
        "   *KJFDA "
        "  `?MNBC  "
        "   >< VXZ ";
    int l = strlen(kbd);
    YAKC_ASSERT(l == 160);
    for (int shift = 0; shift < 2; shift++) {
        for (int col = 0; col < 10; col++) {
            for (int bit = 0; bit < 8; bit++) {
                ubyte ascii = kbd[shift*80 + bit*10 + col];
                this->init_key_mask(ascii, col, bit, shift);
            }
        }
    }

    // special keys
    this->init_key_mask(' ',  5, 7, 0);     // Space
    this->init_key_mask(0x02, 2, 5, 0);     // Shift
    this->init_key_mask(0x08, 1, 0, 0);     // Cursor Left
    this->init_key_mask(0x09, 0, 1, 0);     // Cursor Right
    this->init_key_mask(0x0A, 0, 2, 0);     // Cursor Down
    this->init_key_mask(0x0B, 0, 0, 0);     // Cursor Up
    this->init_key_mask(0x01, 9, 7, 0);     // Delete
    this->init_key_mask(0x0C, 2, 0, 0);     // Clr
    this->init_key_mask(0x0D, 2, 2, 0);     // Return
    this->init_key_mask(0x03, 8, 2, 0);     // Escape

    // joystick (just use some unused upper ascii codes)
    this->init_key_mask(0xF0, 9, 2, 0);     // joystick left
    this->init_key_mask(0xF1, 9, 3, 0);     // joystick right
    this->init_key_mask(0xF2, 9, 0, 0);     // joystick down
    this->init_key_mask(0xF3, 9, 1, 0);     // joystick up
    this->init_key_mask(0xF4, 9, 5, 0);     // joystick fire0
    this->init_key_mask(0xF5, 9, 4, 0);     // joystick fire1
}

//------------------------------------------------------------------------------
void
cpc::init(breadboard* b, rom_images* r) {
    YAKC_ASSERT(b && r);
    this->board = b;
    this->roms = r;
    this->init_keymap();
}

//------------------------------------------------------------------------------
bool
cpc::check_roms(const rom_images& roms, device model, os_rom os) {
    if (device::cpc464 == model) {
        return roms.has(rom_images::cpc464_os) && roms.has(rom_images::cpc464_basic);
    }
    else if (device::cpc6128 == model) {
        return roms.has(rom_images::cpc6128_os) && roms.has(rom_images::cpc6128_basic);
    }
    else if (device::kccompact == model) {
        return roms.has(rom_images::kcc_os) && roms.has(rom_images::kcc_basic);
    }
    else {
        return false;
    }
}

//------------------------------------------------------------------------------
void
cpc::init_memory_map() {
    z80& cpu = this->board->cpu;
    cpu.mem.unmap_all();
    YAKC_ASSERT(check_roms(*this->roms, this->cur_model, os_rom::none));
    this->ga_config = 0x00;     // enable both ROMs
    this->ram_config = 0x00;    // standard RAM bank config (0,1,2,3)
    this->update_memory_mapping();
}

//------------------------------------------------------------------------------
void
cpc::on_context_switched() {
    // FIXME!
}

//------------------------------------------------------------------------------
void
cpc::poweron(device m) {
    YAKC_ASSERT(this->board);
    YAKC_ASSERT(int(device::any_cpc) & int(m));
    YAKC_ASSERT(!this->on);

    this->cur_model = m;
    this->on = true;
    this->video.init(this->board);
    this->ga_config = 0;
    this->ram_config = 0;
    this->pio_c = 0;
    this->next_key_mask = key_mask();
    this->next_joy_mask = key_mask();
    this->cur_keyboard_mask = key_mask();

    // map memory
    clear(this->board->ram, sizeof(this->board->ram));
    this->init_memory_map();

    // initialize clock to 4 MHz
    this->board->clck.init(4000);

    // CPU start state
    this->board->cpu.init();
    this->board->cpu.PC = 0x0000;
}

//------------------------------------------------------------------------------
void
cpc::poweroff() {
    YAKC_ASSERT(this->on);
    this->board->cpu.mem.unmap_all();
    this->on = false;
}

//------------------------------------------------------------------------------
void
cpc::reset() {
    this->video.reset();
    this->ga_config = 0;
    this->ram_config = 0;
    this->pio_c = 0;
    this->next_key_mask = key_mask();
    this->next_joy_mask = key_mask();
    this->cur_keyboard_mask = key_mask();
    this->board->cpu.reset();
    this->board->cpu.PC = 0x0000;
    this->init_memory_map();
}

//------------------------------------------------------------------------------
uint64_t
cpc::step(uint64_t start_tick, uint64_t end_tick) {
    // step the system for given number of cycles, return actually
    // executed number of cycles
    z80& cpu = this->board->cpu;
    z80dbg& dbg = this->board->dbg;
    uint64_t cur_tick = start_tick;
    while (cur_tick < end_tick) {
        if (dbg.check_break(cpu)) {
            dbg.paused = true;
            return end_tick;
        }
        dbg.store_pc_history(cpu); // FIXME: only if debug window open?
        int ticks_step = cpu.step(this);
        // need to round up ticks to 4, this is a CPC specialty
        ticks_step += cpu.handle_irq(this);
        ticks_step = (ticks_step + 3) & ~3;
        this->board->clck.update(this, ticks_step);
        this->video.update(this, ticks_step);
        cur_tick += ticks_step;
    }
    return cur_tick;
}

//------------------------------------------------------------------------------
void
cpc::cpu_out(uword port, ubyte val) {
    // http://cpcwiki.eu/index.php/Default_I/O_Port_Summary
    if (0 == (port & (1<<14))) {
        // CRTC function
        const uword crtc_func = port & 0x0300;
        if (crtc_func == 0x0000) {
            // 0xBCxx: select CRTC register
            this->video.select_crtc(val);
        }
        else if (crtc_func == 0x0100) {
            // 0xBDxx: write CRTC register
            this->video.write_crtc(val);
        }
        else {
            printf("OUT: unknown CRTC function!\n");
        }
        return;
    }
    else if (0x4000 == (port & 0xC000)) {
        //
        // Gate Array function
        //
        // http://www.grimware.org/doku.php/documentations/devices/gatearray
        // http://www.cpcwiki.eu/index.php/Gate_Array
        //
        //  bits 7 and 6 (on CPC plus also 5) define the command
        //
        const ubyte reg = val & 0xC0;
        if (reg == 0x00) {
            // if bit 4 is set, this means that the border color should be set
            this->video.select_pen(val);
        }
        else if (reg == 0x40) {
            this->video.assign_color(val);
        }
        else if (reg == 0x80) {
            //
            //  select screen mode, rom config and int ctrl
            //
            //  - bits 0 and 1 select the screen mode
            //      00: Mode 0 (160x200 @ 16 colors)
            //      01: Mode 1 (320x200 @ 4 colors)
            //      02: Mode 2 (640x200 @ 2 colors)
            //      11: Mode 3 (160x200 @ 2 colors, undocumented)
            //
            //  - bit 2: disable/enable lower ROM
            //  - bit 3: disable/enable upper ROM
            //
            //  - bit 4: interrupt generation control
            //
            this->ga_config = val;
            this->video.set_video_mode(val & 3);
            if (val & (1<<4)) {
                this->video.interrupt_control();
            }
            this->update_memory_mapping();
        }
        else if (reg == 0xC0) {
            // CPC6128 RAM banking
            if (device::cpc6128 == this->cur_model) {
                this->ram_config = val;
                this->update_memory_mapping();
            }
        }
        else {
            // FIXME: unknown Gate Array function
            printf("OUT Unknown Gate Array func: %02x", val);
        }
    }
    else if ((port & 0xFF00) == 0xF400) {
        // FIXME: 8255 PIO Port A (PSG Data)
        // printf("OUT PIO Port A: %02x\n", val);
    }
    else if ((port & 0xFF00) == 0xF600) {
        // FIXME: 8255 PIO Port C (KeybRow, Tape, PSG Control)
        //printf("OUT PIO Port C: %02x\n", val);
        this->pio_c = val;
    }
    else if ((port & 0xFF00) == 0xF700) {
        // FIXME: 8255 PIO Control Register
        // printf("OUT PIO Control: %02x\n", val);
    }
    else {
        printf("OUT %04x %02x\n", port, val);
    }
}

//------------------------------------------------------------------------------
// CPC6128 RAM block indices (see cpu_out())
int ram_table[8][4] = {
    { 0, 1, 2, 3 },
    { 0, 1, 2, 7 },
    { 4, 5, 6, 7 },
    { 0, 3, 2, 7 },
    { 0, 4, 2, 3 },
    { 0, 5, 2, 3 },
    { 0, 6, 2, 3 },
    { 0, 7, 2, 3 },
};

void
cpc::update_memory_mapping() {
    // index into RAM config array
    int ram_table_index;
    ubyte* rom0_ptr,*rom1_ptr;
    if (device::kccompact == this->cur_model) {
        ram_table_index = 0;
        rom0_ptr = this->roms->ptr(rom_images::kcc_os);
        rom1_ptr = this->roms->ptr(rom_images::kcc_basic);
    }
    else if (device::cpc6128 == this->cur_model) {
        ram_table_index = this->ram_config & 0x07;
        rom0_ptr = this->roms->ptr(rom_images::cpc6128_os);
        rom1_ptr = this->roms->ptr(rom_images::cpc6128_basic);
    }
    else {
        ram_table_index = 0;
        rom0_ptr = this->roms->ptr(rom_images::cpc464_os);
        rom1_ptr = this->roms->ptr(rom_images::cpc464_basic);
    }
    auto& cpu = this->board->cpu;
    const int i0 = ram_table[ram_table_index][0];
    const int i1 = ram_table[ram_table_index][1];
    const int i2 = ram_table[ram_table_index][2];
    const int i3 = ram_table[ram_table_index][3];
    // 0x0000..0x3FFF
    if (this->ga_config & (1<<2)) {
        // read/write from and to RAM bank
        cpu.mem.map(0, 0x0000, 0x4000, this->board->ram[i0], true);
    }
    else {
        // read from ROM, write to RAM
        cpu.mem.map_rw(0, 0x0000, 0x4000, rom0_ptr, this->board->ram[i0]);
    }
    // 0x4000..0x7FFF
    cpu.mem.map(0, 0x4000, 0x4000, this->board->ram[i1], true);
    // 0x8000..0xBFFF
    cpu.mem.map(0, 0x8000, 0x4000, this->board->ram[i2], true);
    // 0xC000..0xFFFF
    if (this->ga_config & (1<<3)) {
        // read/write from and to RAM bank
        cpu.mem.map(0, 0xC000, 0x4000, this->board->ram[i3], true);
    }
    else {
        // read from ROM, write to RAM
        cpu.mem.map_rw(0, 0xC000, 0x4000, rom1_ptr, this->board->ram[i3]);
    }
}

//------------------------------------------------------------------------------
void
cpc::put_input(ubyte ascii, ubyte joy0_mask) {
    // ascii=0 means no key pressed, joystick input mutes keyboard input
    this->next_joy_mask = key_mask();
    if (0 == joy0_mask) {
        this->next_key_mask = this->key_map[ascii];
    }
    else {
        this->next_key_mask = key_mask();
        if (joy0_mask & joystick::left) {
            this->next_joy_mask.combine(this->key_map[0xF0]);
        }
        if (joy0_mask & joystick::right) {
            this->next_joy_mask.combine(this->key_map[0xF1]);
        }
        if (joy0_mask & joystick::up) {
            this->next_joy_mask.combine(this->key_map[0xF2]);
        }
        if (joy0_mask & joystick::down) {
            this->next_joy_mask.combine(this->key_map[0xF3]);
        }
        if (joy0_mask & joystick::btn0) {
            this->next_joy_mask.combine(this->key_map[0xF5]);
        }
        if (joy0_mask & joystick::btn1) {
            this->next_joy_mask.combine(this->key_map[0xF4]);
        }
    }
}

//------------------------------------------------------------------------------
ubyte
cpc::cpu_in(uword port) {

    if (0 == (port & (1<<14))) {
        // CRTC function
        // FIXME: this doesn't seem to be called from the BASIC prompt
        const uword crtc_func = port & 0x0300;
        if (crtc_func == 0x0300) {
            // 0xBFxx: read from selected CRTC register
            return this->video.read_crtc();
        }
        else {
            printf("IN: CRTC unknown function!\n");
            return 0xFF;
        }
    }
    else if ((port & 0xFF00) == 0xF400) {
        // 8255 PIO Port A (PSG Data)
        if ((port & 0x000F) < 10) {
            // NOTE: this is a quick-n-dirty hack to get keyboard input working!
            return ~(this->cur_keyboard_mask.col[port & 0x000F]);
        }
        else {
            printf("IN: PIO Port A unknown function!\n");
            return 0xFF;
        }
    }
    else if ((port & 0xFF00) == 0xF500) {
        // http://cpcwiki.eu/index.php/8255
        //
        // PIO Port B:
        //  Bit 7: cassette data input
        //  Bit 6: printer port ready (1=not ready, 0=ready)
        //  Bit 5: expansion port /EXP pin
        //  Bit 4: screen refresh rate (1=50Hz, 0=60Hz)
        //  Bit 3..1: distributor id (shown in start screen)
        //      0: Isp
        //      1: Triumph
        //      2: Saisho
        //      3: Solavox
        //      4: Awa
        //      5: Schneider
        //      6: Orion
        //      7: Amstrad
        //  Bit 0: vsync
        //
        ubyte val = (1<<4) | (7<<1);    // 50Hz refresh rate, Amstrad
        if (this->video.vsync_bit()) {
            val |= (1<<0);
        }
        return val;
    }
    else if ((port & 0xFF00) == 0xF600) {
        // PIO-C, FIXME: why is this read back?
        return this->pio_c;
    }
    else {
        printf("IN %04x\n", port);
        return 0x00;
    }
}

//------------------------------------------------------------------------------
void
cpc::irq() {
    // forward interrupt request to CPU
    this->board->cpu.irq();
}

//------------------------------------------------------------------------------
void
cpc::iack() {
    this->video.interrupt_acknowledge();
}

//------------------------------------------------------------------------------
void
cpc::vblank() {
    // fetch next key mask
    this->cur_keyboard_mask = this->next_key_mask;
    this->cur_keyboard_mask.combine(this->next_joy_mask);
}

//------------------------------------------------------------------------------
const char*
cpc::system_info() const {
    return "FIXME!";
}

} // namespace YAKC
