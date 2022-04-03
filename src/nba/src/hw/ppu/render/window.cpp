/*
 * Copyright (C) 2022 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include "hw/ppu/ppu.hpp"

namespace nba::core {

void PPU::RenderWindow(int vcount, int id) {
  auto& mmio = mmio_copy[vcount];
  auto& winv = mmio.winv[id];
  auto& winh = mmio.winh[id];

  if (vcount == winv.min) {
    window_scanline_enable[id] = true;
  }

  if (vcount == winv.max) {
    window_scanline_enable[id] = false;
  }

  if (window_scanline_enable[id] && winh._changed) {
    if (winh.min <= winh.max) {
      for (int x = 0; x < 240; x++) {
        buffer_win[id][x] = x >= winh.min && x < winh.max;
      }
    } else {
      for (int x = 0; x < 240; x++) {
        buffer_win[id][x] = x >= winh.min || x < winh.max;
      }
    }
    
    winh._changed = false;
  }
}

} // namespace nba::core
