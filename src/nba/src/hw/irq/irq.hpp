/*
 * Copyright (C) 2023 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

#include <nba/integer.hpp>
#include <nba/save_state.hpp>

#include "scheduler.hpp"

namespace nba::core {

namespace arm {
struct ARM7TDMI;
};

struct IRQ {
  enum class Source {
    VBlank,
    HBlank,
    VCount,
    Timer,
    Serial,
    DMA,
    Keypad,
    ROM
  };

  IRQ(arm::ARM7TDMI& cpu, Scheduler& scheduler)
      : cpu(cpu)
      , scheduler(scheduler) {
    scheduler.Register(Scheduler::EventClass::IRQ_synchronizer_delay, this, &IRQ::OnIRQDelayPassed);

    Reset();
  }

  void Reset();
  auto ReadByte(int offset) const -> u8;
  auto ReadHalf(int offset) const -> u16;
  void WriteByte(int offset, u8  value);
  void WriteHalf(int offset, u16 value);
  void Raise(IRQ::Source source, int channel = 0);

  bool MasterEnable() const {
    return reg_ime != 0;
  }

  bool HasServableIRQ() const {
    return (reg_ie & reg_if) != 0;
  }

  void LoadState(SaveState const& state);
  void CopyState(SaveState& state);

private:
  enum Registers {
    REG_IE  = 0,
    REG_IF  = 2,
    REG_IME = 4
  };

  void UpdateIRQLine(int event_priority);

  void OnIRQDelayPassed(u64 irq_line);

  int reg_ime;
  u16 reg_ie;
  u16 reg_if;
  arm::ARM7TDMI& cpu;
  Scheduler& scheduler;
  bool irq_line;
};

} // namespace nba::core
