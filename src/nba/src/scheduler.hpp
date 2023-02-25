/*
 * Copyright (C) 2023 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

#include <nba/log.hpp>
#include <nba/common/compiler.hpp>
#include <nba/integer.hpp>
#include <nba/save_state.hpp>
#include <functional>
#include <limits>

namespace nba::core {

struct Scheduler {
  template<class T>
  using EventMethod = void (T::*)(int);

  template<class T>
  using EventMethod2 = void (T::*)();

  template<class T>
  using EventMethod3 = void (T::*)(u64);

  enum class EventClass : u16 {
    Unknown,

    // PPU
    PPU_hdraw_vdraw,
    PPU_hblank_vdraw,
    PPU_hblank_irq_vdraw,
    PPU_hdraw_vblank,
    PPU_hblank_vblank,
    PPU_hblank_irq_vblank,
    PPU_begin_sprite_fetch,
    PPU_video_dma,
    PPU_hblank_dma,
    PPU_latch_dispcnt,

    // APU
    APU_mixer,
    APU_sequencer,
    APU_PSG1_generate,
    APU_PSG2_generate,
    APU_PSG3_generate,
    APU_PSG4_generate,

    // IRQ controller
    IRQ_synchronizer_delay,

    // Timers
    TM_overflow,
    TM_write_reload,
    TM_write_control,

    // DMA
    DMA_activated,

    Count
  };

  struct Event {
    std::function<void(int)> callback;
    u64 timestamp; 

    u64 UID() const { return uid; }
  
  private:
    friend class Scheduler;
    int handle;
    u64 key;
    u64 uid;
    u64 user_data;
    EventClass event_class;
  };

  Scheduler() {
    for(int i = 0; i < kMaxEvents; i++) {
      heap[i] = new Event();
      heap[i]->handle = i;
    }

    for(int i = 0; i < (int)EventClass::Count; i++) {
      callbacks[i] = [i](u64 user_data) {
        Assert(false, "Scheduler: unhandled event class: {}", i);
      };
    }

    Reset();
  }

 ~Scheduler() {
    for (int i = 0; i < kMaxEvents; i++) {
      delete heap[i];
    }
  }

  void Reset() {
    heap_size = 0;
    timestamp_now = 0;
    next_uid = 1;
    Add(std::numeric_limits<u64>::max(), [](int) {
      Assert(false, "Scheduler: reached end of the event queue.");
    });
  }

  auto GetTimestampNow() const -> u64 {
    return timestamp_now;
  }

  void SetTimestampNow(u64 timestamp) {
    timestamp_now = timestamp;
  }

  auto GetTimestampTarget() const -> u64 {
    return heap[0]->timestamp;
  }

  auto GetRemainingCycleCount() const -> int {
    return int(GetTimestampTarget() - GetTimestampNow());
  }

  void AddCycles(int cycles) {
    auto timestamp_next = timestamp_now + cycles;
    Step(timestamp_next);
    timestamp_now = timestamp_next;
  }

  /*void Register(EventClass event_class, std::function<void()> callback) {
    callbacks[(int)event_class] = std::move(callback);
  }*/

  template<class T>
  void Register(EventClass event_class, T* object, EventMethod2<T> method, uint priority = 0) {
    callbacks[(int)event_class] = [object, method](u64 user_data) {
      (object->*method)();
    };
  }

  template<class T>
  void Register(EventClass event_class, T* object, EventMethod3<T> method, uint priority = 0) {
    callbacks[(int)event_class] = [object, method](u64 user_data) {
      (object->*method)(user_data);
    };
  }

  auto Add(u64 delay, EventClass event_class, uint priority = 0, u64 user_data = 0) -> Event* {
    int n = heap_size++;
    int p = Parent(n);

    Assert(
      heap_size <= kMaxEvents,
      "Scheduler: reached maximum number of events."
    );

    Assert(priority <= 3, "Scheduler: priority must be between 0 and 3.");

    auto event = heap[n];
    event->timestamp = GetTimestampNow() + delay;
    event->key = (event->timestamp << 2) | priority;
    event->uid = next_uid++;
    event->user_data = user_data;
    event->event_class = event_class;

    while (n != 0 && heap[p]->key > heap[n]->key) {
      Swap(n, p);
      n = p;
      p = Parent(n);
    }

    return event;
  }

  // OBSOLETE
  auto Add(u64 delay, std::function<void(int)> callback, uint priority = 0) -> Event* {
    int n = heap_size++;
    int p = Parent(n);

    Assert(
      heap_size <= kMaxEvents,
      "Scheduler: reached maximum number of events."
    );

    Assert(priority <= 3, "Scheduler: priority must be between 0 and 3.");

    auto event = heap[n];
    event->timestamp = GetTimestampNow() + delay;
    event->key = (event->timestamp << 2) | priority;
    event->uid = next_uid++;
    event->callback = callback;
    event->event_class = EventClass::Unknown;

    while (n != 0 && heap[p]->key > heap[n]->key) {
      Swap(n, p);
      n = p;
      p = Parent(n);
    }

    return event;
  }

  template<class T>
  auto Add(u64 delay, T* object, EventMethod<T> method, uint priority = 0) -> Event* {
    return Add(delay, [object, method](int cycles_late) {
      (object->*method)(cycles_late);
    }, priority);
  }

  void Cancel(Event* event) {
    Remove(event->handle);
  }

  auto GetEventByUID(u64 uid) -> Event* {
    for(int i = 0; i < heap_size; i++) {
      auto event = heap[i];

      if(event->uid == uid) {
        return event;
      }
    }

    return nullptr;
  }

  void LoadState(SaveState const& state) {
    auto& ss_scheduler = state.scheduler;

    for(int i = 0; i < ss_scheduler.event_count; i++) {
      auto& event = ss_scheduler.events[i];

      u64 timestamp = event.key >> 2;
      int priority = event.key & 3;
      u64 uid = event.uid;
      u64 user_data = event.user_data;
      EventClass event_class = (EventClass)event.event_class;

      if(event_class != EventClass::Unknown) {
        Add(timestamp - state.timestamp, event_class, priority, user_data)->uid = uid;
      }
    }

    // This must happen after deserializing all events, because calling Add() modifies `next_uid`.
    next_uid = ss_scheduler.next_uid;
  }

  void CopyState(SaveState& state) {
    auto& ss_scheduler = state.scheduler;

    for(int i = 0; i < heap_size; i++) {
      auto event = heap[i];

      ss_scheduler.events[i] = { event->key, event->uid, event->user_data, (u16)event->event_class };
    }

    ss_scheduler.event_count = heap_size;
    ss_scheduler.next_uid = next_uid;
  }

private:
  static constexpr int kMaxEvents = 64;

  static constexpr int Parent(int n) { return (n - 1) / 2; }
  static constexpr int LeftChild(int n) { return n * 2 + 1; }
  static constexpr int RightChild(int n) { return n * 2 + 2; }

  void Step(u64 timestamp_next) {
    while (heap[0]->timestamp <= timestamp_next && heap_size > 0) {
      auto event = heap[0];
      timestamp_now = event->timestamp;
      // @todo: cache the callback function? maybe copying it is slow though.
      if(event->event_class != EventClass::Unknown) {
        callbacks[(int)event->event_class](event->user_data);
      } else {
        event->callback(0);
      }
      Remove(event->handle);
    }
  }

  void Remove(int n) {
    Swap(n, --heap_size);

    int p = Parent(n);
    if (n != 0 && heap[p]->key > heap[n]->key) {
      do {
        Swap(n, p);
        n = p;
        p = Parent(n);
      } while (n != 0 && heap[p]->key > heap[n]->key);
    } else {
      Heapify(n);
    }
  }

  void Swap(int i, int j) {
    auto tmp = heap[i];
    heap[i] = heap[j];
    heap[j] = tmp;
    heap[i]->handle = i;
    heap[j]->handle = j;
  }

  void Heapify(int n) {
    int l = LeftChild(n);
    int r = RightChild(n);

    if (l < heap_size && heap[l]->key < heap[n]->key) {
      Swap(l, n);
      Heapify(l);
    }

    if (r < heap_size && heap[r]->key < heap[n]->key) {
      Swap(r, n);
      Heapify(r);
    }
  }

  Event* heap[kMaxEvents];
  int heap_size;
  u64 timestamp_now;
  u64 next_uid;

  std::function<void(u64)> callbacks[(int)EventClass::Count];
};

inline u64 GetEventUID(Scheduler::Event* event) {
  return event ? event->UID() : 0;
}

} // namespace nba::core
