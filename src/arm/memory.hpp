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

#ifdef ARMIGO_INCLUDE

inline u32 read_hword(u32 offset) {
    if (offset & 1) {
        u32 value = bus_read_hword(offset & ~1);

        return (value >> 8) | (value << 24);
    }

    return bus_read_hword(offset);
}

inline u32 read_hword_signed(u32 offset) {
    u32 value = 0;

    if (offset & 1) {
        value = bus_read_byte(offset);

        if (value & 0x80) {
            value |= 0xFFFFFF00;
        }
    } else {
        value = bus_read_hword(offset);

        if (value & 0x8000) {
            value |= 0xFFFF0000;
        }
    }

    return value;
}

inline u32 read_word(u32 offset) {
    return bus_read_word(offset & ~3);
}

inline u32 read_word_rotated(u32 offset) {
    u32 value = read_word(offset);
    int amount = (offset & 3) * 8;

    return amount == 0 ? value : ((value >> amount) | (value << (32 - amount)));
}

inline void write_hword(u32 offset, u16 value) {
    bus_write_hword(offset & ~1, value);
}

inline void write_word(u32 offset, u32 value) {
    bus_write_word(offset & ~3, value);
}

inline void refill_pipeline() {
    if (m_cpsr & MASK_THUMB) {
        m_opcode[0] = read_hword(m_reg[15]);
        m_opcode[1] = read_hword(m_reg[15] + 2);
        m_reg[15] += 4;
    } else {
        m_opcode[0] = read_word(m_reg[15]);
        m_opcode[1] = read_word(m_reg[15] + 4);
        m_reg[15] += 8;
    }
    
    m_index = 0;
    m_flush = false;
}

#endif
