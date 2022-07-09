/*
 * Copyright (C) 2021 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

#include <memory>
#include <nba/config.hpp>
#include <nba/integer.hpp>
#include <nba/rom/rom.hpp>
#include <vector>

namespace nba {

struct CoreBase {
  static constexpr int kCyclesPerFrame = 280896;

  virtual ~CoreBase() = default;

  virtual void Reset() = 0;
  virtual void Attach(std::vector<u8> const& bios) = 0;
  virtual void Attach(ROM&& rom) = 0;
  virtual auto CreateRTC() -> std::unique_ptr<GPIO> = 0;
  virtual auto CreateSolarSensor() -> std::unique_ptr<GPIO> = 0;
  virtual void Run(int cycles) = 0;

  void RunForOneFrame() {
    Run(kCyclesPerFrame);
  }
};

auto CreateCore(
  std::shared_ptr<Config> config
) -> std::unique_ptr<CoreBase>;

} // namespace nba