/*
 * Copyright (C) 2024 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

#include <array>
#include <nba/rom/rom.hpp>
#include <nba/integer.hpp>
#include <nba/save_state.hpp>
#include <vector>

#include "hw/apu/apu.hpp"
#include "hw/ppu/ppu.hpp"
#include "hw/dma/dma.hpp"
#include "hw/irq/irq.hpp"
#include "hw/keypad/keypad.hpp"
#include "hw/timer/timer.hpp"

namespace nba::core {

namespace {
struct ARM7TDMI;
} // namespace nba::core::arm

struct Bus {
  enum Access {
    Nonsequential = 0,
    Sequential = 1,
    Code = 2,
    Dma = 4,
    Lock = 8
  };

  void Reset();
  void Attach(std::vector<u8> const& bios);
  void Attach(ROM&& rom);

  auto ReadByte(u32 address, int access) ->  u8;
  auto ReadHalf(u32 address, int access) -> u16;
  auto ReadWord(u32 address, int access) -> u32;

  void WriteByte(u32 address, u8  value, int access);
  void WriteHalf(u32 address, u16 value, int access);
  void WriteWord(u32 address, u32 value, int access);

  void Idle();

//private:
  Scheduler& scheduler;

  struct Memory {
    std::array<u8, 0x04000> bios;
    std::array<u8, 0x40000> wram;
    std::array<u8, 0x08000> iram;
    struct Latch {
      u32 bios = 0;
    } latch;
    ROM rom;
  } memory;

  struct Hardware {
    arm::ARM7TDMI& cpu;
    IRQ& irq;
    DMA& dma;
    APU& apu;
    PPU& ppu;
    Timer& timer;
    KeyPad& keypad;
    Bus* bus = nullptr;

    struct WaitstateControl {
      int sram = 0;
      int ws0[2] { 0, 0 };
      int ws1[2] { 0, 0 };
      int ws2[2] { 0, 0 };
      int phi = 0;
      bool prefetch = false;
      bool cgb = false;
    } waitcnt;

    bool prefetch_buffer_was_disabled = false;

    enum class HaltControl {
      Run = 0,
      Stop = 1,
      Halt = 2
    } haltcnt = HaltControl::Run;

    u16 siocnt = 0;
    u8 rcnt[2] { 0, 0 };
    u8 postflg = 0;

    struct MGBALog {
      u16 enable = 0;
      std::array<char, 257> message;
    } mgba_log = {};

    auto ReadByte(u32 address) ->  u8;
    auto ReadHalf(u32 address) -> u16;
    auto ReadWord(u32 address) -> u32;

    void WriteByte(u32 address,  u8 value);
    void WriteHalf(u32 address, u16 value);
    void WriteWord(u32 address, u32 value);

    u8  ReadByteNew(u32 address);
    u16 ReadHalfNew(u32 address);
    u32 ReadWordNew(u32 address);

    void WriteByteNew(u32 address, u8  value);
    void WriteHalfNew(u32 address, u16 value);
    void WriteWordNew(u32 address, u32 value);

    template<u32 mask> u32   ReadWord(u32 address);
    template<u32 mask> void WriteWord(u32 address, u32 value);
  } hw;

  struct Prefetch {
    bool active = false;
    u32 head_address;
    u32 last_address;
    int count = 0;
    int capacity = 8;
    int opcode_width = 4;
    int countdown;
    int duty;
    bool thumb;
  } prefetch;

  int last_access;
  int parallel_internal_cpu_cycle_limit;

  template<typename T>
  auto Read(u32 address, int access) -> T;
  
  template<typename T>
  void Write(u32 address, int access, T value);

  template<typename T>
  auto Align(u32 address) -> u32 {
    return address & ~(sizeof(T) - 1);
  }

  template<typename T>
  auto ALWAYS_INLINE ReadPRAM(u32 address) noexcept -> T {
    constexpr int cycles = std::is_same_v<T, u32> ? 2 : 1;

    for(int i = 0; i < cycles; i++) {
      do {
        Step(1);
        hw.ppu.Sync();
      } while(hw.ppu.DidAccessPRAM());
    }

    return hw.ppu.ReadPRAM<T>(address);
  }

  template<typename T>
  void ALWAYS_INLINE WritePRAM(u32 address, T value) noexcept {
    if constexpr (!std::is_same_v<T, u32>) {
      do {
        Step(1);
        hw.ppu.Sync();
      } while(hw.ppu.DidAccessPRAM());

      hw.ppu.WritePRAM<T>(address, value);
    } else {
      WritePRAM(address + 0, (u16)(value >>  0));
      WritePRAM(address + 2, (u16)(value >> 16));
    }
  }

  template<typename T>
  auto ALWAYS_INLINE ReadVRAM(u32 address) noexcept -> T {
    constexpr int cycles = std::is_same_v<T, u32> ? 2 : 1;

    u32 boundary = hw.ppu.GetSpriteVRAMBoundary();

    address &= 0x1FFFF;

    if(address >= boundary) {
      for(int i = 0; i < cycles; i++) {
        do {
          Step(1);
          hw.ppu.Sync();
        } while(hw.ppu.DidAccessVRAM_OBJ());
      }

      return hw.ppu.ReadVRAM_OBJ<T>(address, boundary);
    } else {
      for(int i = 0; i < cycles; i++) {
        do {
          Step(1);
          hw.ppu.Sync();
        } while(hw.ppu.DidAccessVRAM_BG());
      }

      return hw.ppu.ReadVRAM_BG<T>(address);
    }
  }

  template<typename T>
  void ALWAYS_INLINE WriteVRAM(u32 address, T value) noexcept {
    if constexpr (!std::is_same_v<T, u32>) {
      u32 boundary = hw.ppu.GetSpriteVRAMBoundary();

      address &= 0x1FFFF;

      if(address >= boundary) {
        // TODO: de-duplicate this code (see ReadVRAM):
        do {
          Step(1);
          hw.ppu.Sync();
        } while(hw.ppu.DidAccessVRAM_OBJ());

        hw.ppu.WriteVRAM_OBJ<T>(address, value, boundary);
      } else {
        // TODO: de-duplicate this code (see ReadVRAM):
        do {
          Step(1);
          hw.ppu.Sync();
        } while(hw.ppu.DidAccessVRAM_BG());

        hw.ppu.WriteVRAM_BG<T>(address, value);
      }
    } else {
      WriteVRAM<u16>(address + 0, (u16)(value >>  0));
      WriteVRAM<u16>(address + 2, (u16)(value >> 16));
    }
  }

  template<typename T>
  auto ALWAYS_INLINE ReadOAM(u32 address) noexcept -> T {
    do {
      Step(1);
      hw.ppu.Sync();
    } while(hw.ppu.DidAccessOAM());

    return hw.ppu.ReadOAM<T>(address);
  }

  template<typename T>
  void ALWAYS_INLINE WriteOAM(u32 address, T value) noexcept {
    do {
      Step(1);
      hw.ppu.Sync();
    } while(hw.ppu.DidAccessOAM());

    hw.ppu.WriteOAM<T>(address, value);
  }

  auto ReadBIOS(u32 address) -> u32;
  auto ReadOpenBus(u32 address) -> u32;

  void SIOTransferDone();

  void Prefetch(u32 address, bool code, int cycles);
  void StopPrefetch();
  void Step(int cycles);
  void UpdateWaitStateTable();

  void LoadState(SaveState const& state);
  void CopyState(SaveState& state);
 
  int wait16[2][16] {
    { 1, 1, 3, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 1, 3, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
  };

  int wait32[2][16] {
    { 1, 1, 6, 1, 1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 1, 6, 1, 1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 1 }
  };

public:
  Bus(Scheduler& scheduler, Hardware&& hw);

  auto GetHostAddress(u32 address, size_t size) -> u8*;

  template<typename T>
  auto GetHostAddress(u32 address, size_t count = 1) -> T* {
    return (T*)GetHostAddress(address, sizeof(T) * count);
  }
};

} // namespace nba::core
