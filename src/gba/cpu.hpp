/**
  * Copyright (C) 2017 flerovium^-^ (Frederic Meyer)
  *
  * This file is part of NanoboyAdvance.
  *
  * NanoboyAdvance is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  * 
  * NanoboyAdvance is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  * GNU General Public License for more details.
  * 
  * You should have received a copy of the GNU General Public License
  * along with NanoboyAdvance. If not, see <http://www.gnu.org/licenses/>.
  */

#pragma once

#include <string>
#include "arm/arm.hpp"
#include "ppu/ppu.hpp"
#include "enums.hpp"
#include "interrupt.hpp"
#include "cart_backup.hpp"
#define CPU_INCLUDE

using namespace ARMigo;

namespace GameBoyAdvance {
    
    class CPU : public ARM {
    private:
        typedef u8 (CPU::*read_func)(u32 address);
        typedef void (CPU::*write_func)(u32 address, u8 value);

        u8* m_rom;
        size_t m_rom_size;
        CartBackup* m_backup {nullptr};

        u8 m_bios[0x4000];
        u8 m_wram[0x40000];
        u8 m_iram[0x8000];
        u8 m_pal[0x400];
        u8 m_oam[0x400];
        u8 m_vram[0x18000];

        u8 m_mmio[0x800];

        #include "io.hpp"

        PPU m_ppu;
        Interrupt m_interrupt;

        int m_cycles;
        bool m_dma_active;
        int  m_current_dma;

        u8 read_bios(u32 address);
        u8 read_wram(u32 address);
        u8 read_iram(u32 address);
        u8 read_mmio(u32 address);
        u8 read_pal(u32 address);
        u8 read_vram(u32 address);
        u8 read_oam(u32 address);
        u8 read_rom(u32 address);
        u8 read_save(u32 address);
        u8 read_invalid(u32 address);

        void write_wram(u32 address, u8 value);
        void write_iram(u32 address, u8 value);
        void write_mmio(u32 address, u8 value);
        void write_pal(u32 address, u8 value);
        void write_vram(u32 address, u8 value);
        void write_oam(u32 address, u8 value);
        void write_save(u32 address, u8 value);
        void write_invalid(u32 address, u8 value);

        void run_for(int cycles);

        void timer_step(int cycles);
        void timer_increment(struct IO::Timer& timer, bool& overflow);

        void dma_hblank();
        void dma_vblank();
        void dma_transfer();

        static constexpr int m_mem_cycles8_16[16] = {
            1, 1, 3, 1, 1, 1, 1, 1, 5, 5, 1, 1, 1, 1, 5, 1
        };

        static constexpr int m_mem_cycles32[16] = {
            1, 1, 6, 1, 1, 2, 2, 1, 8, 8, 1, 1, 1, 1, 5, 1
        };

        static constexpr read_func m_read_table[16] = {
            &CPU::read_bios,
            &CPU::read_invalid,
            &CPU::read_wram,
            &CPU::read_iram,
            &CPU::read_mmio,
            &CPU::read_pal,
            &CPU::read_vram,
            &CPU::read_oam,
            &CPU::read_rom,
            &CPU::read_rom,
            &CPU::read_invalid,
            &CPU::read_invalid,
            &CPU::read_invalid,
            &CPU::read_invalid,
            &CPU::read_save,
            &CPU::read_invalid
        };

        static constexpr write_func m_write_table[16] = {
            &CPU::write_invalid,
            &CPU::write_invalid,
            &CPU::write_wram,
            &CPU::write_iram,
            &CPU::write_mmio,
            &CPU::write_pal,
            &CPU::write_vram,
            &CPU::write_oam,
            &CPU::write_invalid,
            &CPU::write_invalid,
            &CPU::write_invalid,
            &CPU::write_invalid,
            &CPU::write_invalid,
            &CPU::write_invalid,
            &CPU::write_save,
            &CPU::write_invalid
        };

        static constexpr int m_timer_ticks[4] = { 1, 64, 256, 1024 };

    public:
        CPU();
        ~CPU();

        void reset();

        PPU& get_ppu();
        u16& get_keypad();
        void set_bios(u8* data, size_t size);
        void set_game(u8* data, size_t size, std::string save_file);

        void frame();
    protected:

        #include "memory.hpp"

        void software_interrupt(int number) final;
    };
}

#undef CPU_INCLUDE
