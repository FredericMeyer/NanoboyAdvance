/*
 * Copyright (C) 2020 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

namespace nba::core {

class Envelope {
public:
  void Reset() {
    direction = Direction::Decrement;
    initial_volume = 0;
    divider = 0;
    Restart();
  }

  void Restart() {
    step = divider;
    current_volume = initial_volume;
    active = enabled;
  }

  void Tick() {
    if (--step == 0) {
      step = divider;
    
      if (active && divider != 0) {
        if (direction == Direction::Increment) {
          if (current_volume != 15) {
            current_volume++;
          } else {
            active = false;
          }
        } else {
          if (current_volume != 0) {
            current_volume--;
          } else {
            active = false;
          }
        }
      }
    }
  }

  bool active = false;
  bool enabled = false;

  enum Direction {
    Increment = 1,
    Decrement = 0
  } direction;

  int initial_volume;
  int current_volume;
  int divider;

private:
  int step;
};

class Sweep {
public:
  void Reset() {
    direction = Direction::Increment;
    initial_freq = 0;
    divider = 0;
    shift = 0;
    Restart();
  }

  void Restart() {
    channel_disabled = false;

    /* TODO: If the sweep shift is non-zero, frequency calculation and the
     * overflow check are performed immediately.
     */
    if (enabled) {
      current_freq = initial_freq;
      shadow_freq = initial_freq;
      step = divider;
      active = shift != 0 || divider != 0;
    }
  }

  void Tick() {
    if (active && --step == 0) {
      int new_freq;
      int offset = shadow_freq >> shift;

      if (direction == Direction::Increment) {
        new_freq = shadow_freq + offset;
      } else {
        new_freq = shadow_freq - offset;
      }

      if (new_freq >= 2048) {
        channel_disabled = true;
      } else if (shift != 0) {
        shadow_freq  = new_freq;
        current_freq = new_freq;
      }

      /* TODO: then frequency calculation and overflow check are run AGAIN immediately
       * using this new value, but this second new frequency is not written back.
       */
      step = divider;
    }
  }

  bool active = false;
  bool enabled = false;
  bool channel_disabled = false;

  enum Direction {
    Increment = 0,
    Decrement = 1
  } direction;

  int initial_freq;
  int current_freq;
  int shadow_freq;
  int divider;
  int shift;

private:
  int step;
};

class BaseChannel {
public:
  static constexpr int s_cycles_per_step = 16777216 / 512;

  BaseChannel(bool enable_envelope, bool enable_sweep) {
    envelope.enabled = enable_envelope;
    sweep.enabled = enable_sweep;
    Reset();
  }

  void Reset() {
    length = 0;
    envelope.Reset();
    sweep.Reset();
    step = 0;
  }

  void Tick() {
    // http://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware#Frame_Sequencer
    switch (step) {
      case 0: length--; break;
      case 1: break;
      case 2: length--; sweep.Tick(); break;
      case 3: break;
      case 4: length--; break;
      case 5: break;
      case 6: length--; sweep.Tick(); break;
      case 7: envelope.Tick(); break;
    }

    step = (step + 1) % 8;
  }

protected:
  void Restart() {
    if (length == 0) {
      length = length_default;
    }
    sweep.Restart();
    envelope.Restart();
    step = 0;
  }

  int length;
  int length_default = 64;
  Envelope envelope;
  Sweep sweep;

private:
  int step;
};

} // namespace nba::core
