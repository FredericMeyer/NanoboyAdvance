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

#include "enums.hpp"
#include "util/integer.hpp"
#define ARMIGO_INCLUDE

namespace ARMigo {
    class ARM {
    public:
        /// Constructor
        ARM();

        /// Resets the CPU state.
        virtual void reset();

        /// Executes exactly one instruction.
        inline void step() {
            bool thumb = m_cpsr & MASK_THUMB;

            if (thumb) {
                m_reg[15] &= ~1;

                if (m_index == 0) {
                    m_opcode[2] = read_hword(m_reg[15]);
                } else {
                    m_opcode[m_index - 1] = read_hword(m_reg[15]);
                }

                thumb_execute(m_opcode[m_index]);
            } else {
                m_reg[15] &= ~3;

                if (m_index == 0) {
                    m_opcode[2] = read_word(m_reg[15]);
                } else {
                    m_opcode[m_index - 1] = read_word(m_reg[15]);
                }

                arm_execute(m_opcode[m_index]);
            }

            if (m_flush) {
                refill_pipeline();
                return;
            }

            // update pipeline status
            m_index = (m_index + 1) % 3;

            // update instruction pointer
            m_reg[15] += thumb ? 2 : 4;
        }

        /// Tries to raise an IRQ exception.
        void raise_interrupt();

        /// HLE-flag getter
        bool get_hle() { return m_hle; }

        /// HLE-flag setter
        /// @param  hle  HLE-flag
        void set_hle(bool hle) { m_hle = hle; }

    protected:
        u32 m_reg[16];

        u32 m_bank[BANK_COUNT][7];

        u32 m_cpsr;
        u32 m_spsr[SPSR_COUNT];
        u32* m_spsr_ptr;

        int m_index;
        bool m_flush;
        u32 m_opcode[3];

        bool m_hle;

        // memory bus methods
        virtual u8  bus_read_byte(u32 address)  { return 0; }
        virtual u16 bus_read_hword(u32 address) { return 0; }
        virtual u32 bus_read_word(u32 address)  { return 0; }
        virtual void bus_write_byte(u32 address, u8 value) {}
        virtual void bus_write_hword(u32 address, u16 value) {}
        virtual void bus_write_word (u32 address, u32 value) {}

        // swi #nn HLE-handler
        virtual void software_interrupt(int number) {}

        // memory access helpers
        #include "memory.hpp"

    private:
        static Bank mode_to_bank(Mode mode);

        void switch_mode(Mode new_mode);

        // conditional helpers
        #include "flags.hpp"

        // shifting helpers
        #include "shifting.hpp"

        // ARM and THUMB interpreter cores
        #include "arm/arm_emu.hpp"
        #include "thumb/thumb_emu.hpp"
    };
}

#undef ARMIGO_INCLUDE
