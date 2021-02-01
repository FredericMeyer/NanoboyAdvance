/*
 * Copyright (C) 2020 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

#include <cstdint>

#include "channel/fifo.hpp"
#include "channel/sequencer.hpp"

namespace nba::core {

enum Side {
  SIDE_LEFT  = 0,
  SIDE_RIGHT = 1
};
  
enum DMANumber {
  DMA_A = 0,
  DMA_B = 1
};
  
struct SoundControl {
  SoundControl(
    FIFO* fifos,
    BaseChannel& psg1,
    BaseChannel& psg2,
    BaseChannel& psg3,
    BaseChannel& psg4)
      : fifos(fifos)
      , psg1(psg1)
      , psg2(psg2)
      , psg3(psg3)
      , psg4(psg4) {
  }
  
  bool master_enable;

  struct PSG {
    int  volume;
    int  master[2];
    bool enable[2][4];
  } psg;

  struct DMA {
    int  volume;
    bool enable[2];
    int  timer_id;
  } dma[2];

  void Reset();
  auto Read(int address) -> std::uint8_t;
  void Write(int address, std::uint8_t value);

private:
  FIFO* fifos;

  BaseChannel& psg1;
  BaseChannel& psg2;
  BaseChannel& psg3;
  BaseChannel& psg4;
};

struct BIAS {
  int level;
  int resolution;

  void Reset();
  auto Read(int address) -> std::uint8_t;
  void Write(int address, std::uint8_t value);
  
  auto GetSampleInterval() -> int { return 512 >> resolution; }
  auto GetSampleRate() -> int { return 32768 << resolution; }
};

} // namespace nba::core
