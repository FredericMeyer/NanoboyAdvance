/**
  * Copyright (C) 2019 fleroviux (Frederic Meyer)
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

#include <cstdint>

namespace GameBoyAdvance {

struct DisplayControl {
  int mode;
  int cgb_mode;
  int frame;
  int hblank_oam_access;
  int oam_mapping_1d;
  int forced_blank;
  int enable[8];

  void Reset();
  auto Read(int address) -> std::uint8_t;
  void Write(int address, std::uint8_t value);
};

struct DisplayStatus {
  int vblank_flag;
  int hblank_flag;
  int vcount_flag;
  int vblank_irq_enable;
  int hblank_irq_enable;
  int vcount_irq_enable;
  int vcount_setting;

  void Reset();
  auto Read(int address) -> std::uint8_t;
  void Write(int address, std::uint8_t value);
};

struct BackgroundControl {
  int priority;
  int tile_block;
  int mosaic_enable;
  int full_palette;
  int map_block;
  int wraparound;
  int size;

  void Reset();
  auto Read(int address) -> std::uint8_t;
  void Write(int address, std::uint8_t value);
};

struct ReferencePoint {
  std::int32_t initial;
  std::int32_t _current;
  
  void Reset();
  void Write(int address, std::uint8_t value);
};
  
struct BlendControl {
  enum Effect {
    SFX_NONE,
    SFX_BLEND,
    SFX_BRIGHTEN,
    SFX_DARKEN
  } sfx;
  
  int targets[2][6];

  void Reset();
  auto Read(int address) -> std::uint8_t;
  void Write(int address, std::uint8_t value);
};

struct WindowRange {
  int min;
  int max;
  bool _changed;

  void Reset();
  void Write(int address, std::uint8_t value);
};

struct WindowLayerSelect {
  int enable[2][6];

  void Reset();
  auto Read(int offset) -> std::uint8_t;
  void Write(int offset, std::uint8_t value);
};

struct Mosaic {
  struct {
    int horizontal;
    int vertical;
  } bg, obj;
  
  void Reset();
  void Write(int address, std::uint8_t value);
};

}
