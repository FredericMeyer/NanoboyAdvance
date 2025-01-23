/*
 * Copyright (C) 2024 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <nba/log.hpp>
#include <cstdio>

#include "arm/arm7tdmi.hpp"
#include "bus/bus.hpp"
#include "bus/io.hpp"

#define REG(address) ((address) >> 2)

namespace nba::core {

static constexpr bool AddressUsesNewIO(u32 address) {
  return address < 0x04000058u;
}

static constexpr int GetAccessSize(u32 mask) {
  int size = 0;
  u32 ff = 0xFFu;

  for(int i = 0; i < 4; i++) {
    if(mask & ff) size += 8;

    ff <<= 8;
  }

  return size;
}

static constexpr u32 GetAccessAddressOffset(u32 mask) {
  int offset = 0;

  for(int i = 0; i < 4; i++) {
    if(mask & 0xFFu) break;

    offset++;
    mask >>= 8;
  }

  return offset;
}

u8 Bus::Hardware::ReadByteNew(u32 address) {
  switch(address & 3u) {
    case 0u: return ReadWord<0x000000FFu>(address);;
    case 1u: return ReadWord<0x0000FF00u>(address & ~3u) >>  8;
    case 2u: return ReadWord<0x00FF0000u>(address & ~3u) >> 16;
    case 3u: return ReadWord<0xFF000000u>(address & ~3u) >> 24;
  }
  return 0u;
}

u16 Bus::Hardware::ReadHalfNew(u32 address) {
  switch(address & 2u) {
    case 0u: return ReadWord<0x0000FFFFu>(address);
    case 2u: return ReadWord<0xFFFF0000u>(address & ~2u) >> 16;
  }
  return 0u;
}

u32 Bus::Hardware::ReadWordNew(u32 address) {
  return ReadWord<0xFFFFFFFFu>(address);
}

void Bus::Hardware::WriteByteNew(u32 address, u8  value) {
  const u32 word = value * 0x01010101u;

  switch(address & 3u) {
    case 0u: WriteWord<0x000000FFu>(address      , word); break;
    case 1u: WriteWord<0x0000FF00u>(address & ~3u, word); break;
    case 2u: WriteWord<0x00FF0000u>(address & ~3u, word); break;
    case 3u: WriteWord<0xFF000000u>(address & ~3u, word); break;
  }
}

void Bus::Hardware::WriteHalfNew(u32 address, u16 value) {
  const u32 word = value * 0x00010001u;

  switch(address & 2u) {
    case 0u: WriteWord<0x0000FFFFu>(address      , word); break;
    case 2u: WriteWord<0xFFFF0000u>(address & ~2u, word); break;
  }
}

void Bus::Hardware::WriteWordNew(u32 address, u32 value) {
  WriteWord<0xFFFFFFFFu>(address, value);
}

template<u32 mask> u32 Bus::Hardware::ReadWord(u32 address) {
  u32 value = 0u;

  const auto Unhandled = [&]() {
    const int access_size = GetAccessSize(mask);
    const u32 access_address = address + GetAccessAddressOffset(mask);

    Log<Warn>("IO: unhandled {}-bit read from 0x{:08X}", access_size, access_address);
  }; // @todo: does this *really* need to be in a lambda function?

  auto& ppu_io = ppu.mmio;

  switch(REG(address)) {
    case REG(0x04000000u): {
      if constexpr(mask & 0x000000FFu) value |= ppu_io.dispcnt.Read(0);
      if constexpr(mask & 0x0000FF00u) value |= ppu_io.dispcnt.Read(1) << 8;
      if constexpr(mask & 0x00FF0000u) value |= ppu_io.greenswap << 16;
      return value;
    }
    case REG(0x04000004u): {
      if constexpr(mask & 0x000000FFu) value |= ppu_io.dispstat.Read(0);
      if constexpr(mask & 0x0000FF00u) value |= ppu_io.dispstat.Read(1) << 8;
      if constexpr(mask & 0x00FF0000u) value |= ppu_io.vcount << 16;
      return value;
    }
    case REG(0x04000008u): {
      if constexpr(mask & 0x000000FFu) value |= ppu_io.bgcnt[0].Read(0);
      if constexpr(mask & 0x0000FF00u) value |= ppu_io.bgcnt[0].Read(1) <<  8;
      if constexpr(mask & 0x00FF0000u) value |= ppu_io.bgcnt[1].Read(0) << 16;
      if constexpr(mask & 0xFF000000u) value |= ppu_io.bgcnt[1].Read(1) << 24;
      return value;
    }
    case REG(0x0400000Cu): {
      if constexpr(mask & 0x000000FFu) value |= ppu_io.bgcnt[2].Read(0);
      if constexpr(mask & 0x0000FF00u) value |= ppu_io.bgcnt[2].Read(1) <<  8;
      if constexpr(mask & 0x00FF0000u) value |= ppu_io.bgcnt[3].Read(0) << 16;
      if constexpr(mask & 0xFF000000u) value |= ppu_io.bgcnt[3].Read(1) << 24;
      return value;
    }
    case REG(0x04000048u): {
      if constexpr(mask & 0x000000FFu) value |=  ppu_io.winin.Read(0) <<  0;
      if constexpr(mask & 0x0000FF00u) value |=  ppu_io.winin.Read(1) <<  8;
      if constexpr(mask & 0x00FF0000u) value |= ppu_io.winout.Read(0) << 16;
      if constexpr(mask & 0xFF000000u) value |= ppu_io.winout.Read(1) << 24;
      return value;
    }
  case REG(0x04000050u): {
      if constexpr(mask & 0x000000FFu) value |= ppu_io.bldcnt.Read(0) << 0;
      if constexpr(mask & 0x0000FF00u) value |= ppu_io.bldcnt.Read(1) << 8;
      if constexpr(mask & 0x00FF0000u) value |= ppu_io.eva << 16;
      if constexpr(mask & 0xFF000000u) value |= ppu_io.evb << 24;
      return value;
    }
    default: {
      Unhandled();
    }
  }

  return bus->ReadOpenBus(address);
}

template<u32 mask> void Bus::Hardware::WriteWord(u32 address, u32 value) {
  const auto Unhandled = [&]() {
    const int access_size = GetAccessSize(mask);
    const u32 access_address = address + GetAccessAddressOffset(mask);
    const u32 access_value = (value & mask) >> (GetAccessAddressOffset(mask) * 8);

    Log<Warn>("IO: unhandled {}-bit write to 0x{:08X} = 0x{:08X}", access_size, access_address, access_value);
  }; // @todo: does this *really* need to be in a lambda function?

  auto& ppu_io = ppu.mmio;

  switch(REG(address)) {
    case REG(0x04000000u): {
      if constexpr(mask & 0x000000FFu) ppu_io.dispcnt.Write(0, (u8)(value >> 0));
      if constexpr(mask & 0x0000FF00u) ppu_io.dispcnt.Write(1, (u8)(value >> 8));
      if constexpr(mask & 0x00FF0000u) ppu_io.greenswap = (value >> 16) & 1;
      break;
    }
    case REG(0x04000004u): {
      if constexpr(mask & 0x000000FFu) ppu_io.dispstat.Write(0, (u8)(value >> 0));
      if constexpr(mask & 0x0000FF00u) ppu_io.dispstat.Write(1, (u8)(value >> 8));
      break;
    }
    case REG(0x04000008u): {
      if constexpr(mask & 0x000000FFu) ppu_io.bgcnt[0].Write(0, (u8)(value >>  0));
      if constexpr(mask & 0x0000FF00u) ppu_io.bgcnt[0].Write(1, (u8)(value >>  8));
      if constexpr(mask & 0x00FF0000u) ppu_io.bgcnt[1].Write(0, (u8)(value >> 16));
      if constexpr(mask & 0xFF000000u) ppu_io.bgcnt[1].Write(1, (u8)(value >> 24));
      break;
    }
    case REG(0x0400000Cu): {
      if constexpr(mask & 0x000000FFu) ppu_io.bgcnt[2].Write(0, (u8)(value >>  0));
      if constexpr(mask & 0x0000FF00u) ppu_io.bgcnt[2].Write(1, (u8)(value >>  8));
      if constexpr(mask & 0x00FF0000u) ppu_io.bgcnt[3].Write(0, (u8)(value >> 16));
      if constexpr(mask & 0xFF000000u) ppu_io.bgcnt[3].Write(1, (u8)(value >> 24));
      break;
    }
    case REG(0x04000010u): {
      constexpr u32 lsw_mask = (mask >>  0) & 0x1FFu;
      constexpr u32 msw_mask = (mask >> 16) & 0x1FFu;
      
      if constexpr(mask & 0x0000FFFFu) ppu_io.bghofs[0] = ((value >>  0) & lsw_mask) | (ppu_io.bghofs[0] & ~lsw_mask);
      if constexpr(mask & 0xFFFF0000u) ppu_io.bgvofs[0] = ((value >> 16) & msw_mask) | (ppu_io.bgvofs[0] & ~msw_mask);
      break;
    } 
    case REG(0x04000014u): {
      constexpr u32 lsw_mask = (mask >>  0) & 0x1FFu;
      constexpr u32 msw_mask = (mask >> 16) & 0x1FFu;
      
      if constexpr(mask & 0x0000FFFFu) ppu_io.bghofs[1] = ((value >>  0) & lsw_mask) | (ppu_io.bghofs[1] & ~lsw_mask);
      if constexpr(mask & 0xFFFF0000u) ppu_io.bgvofs[1] = ((value >> 16) & msw_mask) | (ppu_io.bgvofs[1] & ~msw_mask);
      break;
    }
    case REG(0x04000018u): {
      constexpr u32 lsw_mask = (mask >>  0) & 0x1FFu;
      constexpr u32 msw_mask = (mask >> 16) & 0x1FFu;
      
      if constexpr(mask & 0x0000FFFFu) ppu_io.bghofs[2] = ((value >>  0) & lsw_mask) | (ppu_io.bghofs[2] & ~lsw_mask);
      if constexpr(mask & 0xFFFF0000u) ppu_io.bgvofs[2] = ((value >> 16) & msw_mask) | (ppu_io.bgvofs[2] & ~msw_mask);
      break;
    }
    case REG(0x0400001Cu): {
      constexpr u32 lsw_mask = (mask >>  0) & 0x1FFu;
      constexpr u32 msw_mask = (mask >> 16) & 0x1FFu;
      
      if constexpr(mask & 0x0000FFFFu) ppu_io.bghofs[3] = ((value >>  0) & lsw_mask) | (ppu_io.bghofs[3] & ~lsw_mask);
      if constexpr(mask & 0xFFFF0000u) ppu_io.bgvofs[3] = ((value >> 16) & msw_mask) | (ppu_io.bgvofs[3] & ~msw_mask);
      break;
    }
    case REG(0x04000020u): {
      if constexpr(mask & 0x0000FFFFu) ppu_io.bgpa[0] = (value & (u16)(mask >>  0)) | (ppu_io.bgpa[0] & ~(u16)(mask >>  0));
      if constexpr(mask & 0xFFFF0000u) ppu_io.bgpb[0] = (value & (u16)(mask >> 16)) | (ppu_io.bgpb[0] & ~(u16)(mask >> 16));
      break;
    }
    case REG(0x04000024u): {
      if constexpr(mask & 0x0000FFFFu) ppu_io.bgpc[0] = (value & (u16)(mask >>  0)) | (ppu_io.bgpc[0] & ~(u16)(mask >>  0));
      if constexpr(mask & 0xFFFF0000u) ppu_io.bgpd[0] = (value & (u16)(mask >> 16)) | (ppu_io.bgpd[0] & ~(u16)(mask >> 16));
      break;
    }
    case REG(0x04000028u): {
      if constexpr(mask & 0x000000FFu) ppu_io.bgx[0].Write(0, (u8)(value >>  0));
      if constexpr(mask & 0x0000FF00u) ppu_io.bgx[0].Write(1, (u8)(value >>  8));
      if constexpr(mask & 0x00FF0000u) ppu_io.bgx[0].Write(2, (u8)(value >> 16));
      if constexpr(mask & 0xFF000000u) ppu_io.bgx[0].Write(3, (u8)(value >> 24));
      break;
    }
    case REG(0x0400002Cu): {
      if constexpr(mask & 0x000000FFu) ppu_io.bgy[0].Write(0, (u8)(value >>  0));
      if constexpr(mask & 0x0000FF00u) ppu_io.bgy[0].Write(1, (u8)(value >>  8));
      if constexpr(mask & 0x00FF0000u) ppu_io.bgy[0].Write(2, (u8)(value >> 16));
      if constexpr(mask & 0xFF000000u) ppu_io.bgy[0].Write(3, (u8)(value >> 24));
      break;
    }
    case REG(0x04000030u): {
      if constexpr(mask & 0x0000FFFFu) ppu_io.bgpa[1] = (value & (u16)(mask >>  0)) | (ppu_io.bgpa[1] & ~(u16)(mask >>  0));
      if constexpr(mask & 0xFFFF0000u) ppu_io.bgpb[1] = (value & (u16)(mask >> 16)) | (ppu_io.bgpb[1] & ~(u16)(mask >> 16));
      break;
    }
    case REG(0x04000034u): {
      if constexpr(mask & 0x0000FFFFu) ppu_io.bgpc[1] = (value & (u16)(mask >>  0)) | (ppu_io.bgpc[1] & ~(u16)(mask >>  0));
      if constexpr(mask & 0xFFFF0000u) ppu_io.bgpd[1] = (value & (u16)(mask >> 16)) | (ppu_io.bgpd[1] & ~(u16)(mask >> 16));
      break;
    }
    case REG(0x04000038u): {
      if constexpr(mask & 0x000000FFu) ppu_io.bgx[1].Write(0, (u8)(value >>  0));
      if constexpr(mask & 0x0000FF00u) ppu_io.bgx[1].Write(1, (u8)(value >>  8));
      if constexpr(mask & 0x00FF0000u) ppu_io.bgx[1].Write(2, (u8)(value >> 16));
      if constexpr(mask & 0xFF000000u) ppu_io.bgx[1].Write(3, (u8)(value >> 24));
      break;
    }
    case REG(0x0400003Cu): {
      if constexpr(mask & 0x000000FFu) ppu_io.bgy[1].Write(0, (u8)(value >>  0));
      if constexpr(mask & 0x0000FF00u) ppu_io.bgy[1].Write(1, (u8)(value >>  8));
      if constexpr(mask & 0x00FF0000u) ppu_io.bgy[1].Write(2, (u8)(value >> 16));
      if constexpr(mask & 0xFF000000u) ppu_io.bgy[1].Write(3, (u8)(value >> 24));
      break;
    }
    case REG(0x04000040u): {
      if constexpr(mask & 0x000000FFu) ppu_io.winh[0].Write(0, (u8)(value >>  0));
      if constexpr(mask & 0x0000FF00u) ppu_io.winh[0].Write(1, (u8)(value >>  8));
      if constexpr(mask & 0x00FF0000u) ppu_io.winh[1].Write(0, (u8)(value >> 16));
      if constexpr(mask & 0xFF000000u) ppu_io.winh[1].Write(1, (u8)(value >> 24));
      break;
    }
    case REG(0x04000044u): {
      if constexpr(mask & 0x000000FFu) ppu_io.winv[0].Write(0, (u8)(value >>  0));
      if constexpr(mask & 0x0000FF00u) ppu_io.winv[0].Write(1, (u8)(value >>  8));
      if constexpr(mask & 0x00FF0000u) ppu_io.winv[1].Write(0, (u8)(value >> 16));
      if constexpr(mask & 0xFF000000u) ppu_io.winv[1].Write(1, (u8)(value >> 24));
      break;
    }
    case REG(0x04000048u): {
      if constexpr(mask & 0x000000FFu)  ppu_io.winin.Write(0, (u8)(value >>  0));
      if constexpr(mask & 0x0000FF00u)  ppu_io.winin.Write(1, (u8)(value >>  8));
      if constexpr(mask & 0x00FF0000u) ppu_io.winout.Write(0, (u8)(value >> 16));
      if constexpr(mask & 0xFF000000u) ppu_io.winout.Write(1, (u8)(value >> 24));
      break;
    }
    case REG(0x0400004Cu): {
      if constexpr(mask & 0x000000FFu) ppu_io.mosaic.Write(0, (u8)(value >> 0));
      if constexpr(mask & 0x0000FF00u) ppu_io.mosaic.Write(1, (u8)(value >> 8));
      break;
    }
    case REG(0x04000050u): {
      if constexpr(mask & 0x000000FFu) ppu_io.bldcnt.Write(0, (u8)(value >> 0));
      if constexpr(mask & 0x0000FF00u) ppu_io.bldcnt.Write(1, (u8)(value >> 8));
      if constexpr(mask & 0x00FF0000u) ppu_io.eva = (u8)(value >> 16) & 0x1Fu;
      if constexpr(mask & 0xFF000000u) ppu_io.evb = (u8)(value >> 24) & 0x1Fu;
      break;
    }
    case REG(0x04000054u): {
      if constexpr(mask & 0x000000FFu) ppu_io.evy = value & 0x1Fu;
      break;
    }

    default: {
      Unhandled();
    }
  }
}

// LEGACY I/O CODE BELOW:

auto Bus::Hardware::ReadByte(u32 address) ->  u8 {
  if(AddressUsesNewIO(address)) {
    return ReadByteNew(address);
  }

  auto& apu_io = apu.mmio;
  auto& ppu_io = ppu.mmio;

  switch(address) {
    // PPU
    // case DISPCNT+0:  return ppu_io.dispcnt.Read(0);
    // case DISPCNT+1:  return ppu_io.dispcnt.Read(1);
    // case GREENSWAP+0: return ppu_io.greenswap;
    // case GREENSWAP+1: return 0;
    // case DISPSTAT+0: return ppu_io.dispstat.Read(0);
    // case DISPSTAT+1: return ppu_io.dispstat.Read(1);
    // case VCOUNT+0:   return ppu_io.vcount & 0xFF;
    // case VCOUNT+1:   return 0;
    // case BG0CNT+0:   return ppu_io.bgcnt[0].Read(0);
    // case BG0CNT+1:   return ppu_io.bgcnt[0].Read(1);
    // case BG1CNT+0:   return ppu_io.bgcnt[1].Read(0);
    // case BG1CNT+1:   return ppu_io.bgcnt[1].Read(1);
    // case BG2CNT+0:   return ppu_io.bgcnt[2].Read(0);
    // case BG2CNT+1:   return ppu_io.bgcnt[2].Read(1);
    // case BG3CNT+0:   return ppu_io.bgcnt[3].Read(0);
    // case BG3CNT+1:   return ppu_io.bgcnt[3].Read(1);
    // case WININ+0:    return ppu_io.winin.Read(0);
    // case WININ+1:    return ppu_io.winin.Read(1);
    // case WINOUT+0:   return ppu_io.winout.Read(0);
    // case WINOUT+1:   return ppu_io.winout.Read(1);
    // case BLDCNT+0:   return ppu_io.bldcnt.Read(0);
    // case BLDCNT+1:   return ppu_io.bldcnt.Read(1);
    // case BLDALPHA+0: return ppu_io.eva;
    // case BLDALPHA+1: return ppu_io.evb;

    // DMA 0 - 3
    case DMA0CNT_L:
    case DMA0CNT_L+1: return 0;
    case DMA0CNT_H:   return dma.Read(0, 10);
    case DMA0CNT_H+1: return dma.Read(0, 11);
    case DMA1CNT_L:
    case DMA1CNT_L+1: return 0;
    case DMA1CNT_H:   return dma.Read(1, 10);
    case DMA1CNT_H+1: return dma.Read(1, 11);
    case DMA2CNT_L:
    case DMA2CNT_L+1: return 0;
    case DMA2CNT_H:   return dma.Read(2, 10);
    case DMA2CNT_H+1: return dma.Read(2, 11);
    case DMA3CNT_L:
    case DMA3CNT_L+1: return 0;
    case DMA3CNT_H:   return dma.Read(3, 10);
    case DMA3CNT_H+1: return dma.Read(3, 11);

    // Sound
    case SOUND1CNT_L:   return apu_io.psg1.Read(0);
    case SOUND1CNT_L+1: return apu_io.psg1.Read(1);
    case SOUND1CNT_H:   return apu_io.psg1.Read(2);
    case SOUND1CNT_H+1: return apu_io.psg1.Read(3);
    case SOUND1CNT_X:   return apu_io.psg1.Read(4);
    case SOUND1CNT_X+1: return apu_io.psg1.Read(5);
    case SOUND1CNT_X+2:
    case SOUND1CNT_X+3: return 0;
    case SOUND2CNT_L:   return apu_io.psg2.Read(2);
    case SOUND2CNT_L+1: return apu_io.psg2.Read(3);
    case SOUND2CNT_L+2:
    case SOUND2CNT_L+3: return 0;
    case SOUND2CNT_H:   return apu_io.psg2.Read(4);
    case SOUND2CNT_H+1: return apu_io.psg2.Read(5);
    case SOUND2CNT_H+2:
    case SOUND2CNT_H+3: return 0;
    case SOUND3CNT_L:   return apu_io.psg3.Read(0);
    case SOUND3CNT_L+1: return apu_io.psg3.Read(1);
    case SOUND3CNT_H:   return apu_io.psg3.Read(2);
    case SOUND3CNT_H+1: return apu_io.psg3.Read(3);
    case SOUND3CNT_X:   return apu_io.psg3.Read(4);
    case SOUND3CNT_X+1: return apu_io.psg3.Read(5);
    case SOUND3CNT_X+2:
    case SOUND3CNT_X+3: return 0;
    case SOUND4CNT_L:   return apu_io.psg4.Read(0);
    case SOUND4CNT_L+1: return apu_io.psg4.Read(1);
    case SOUND4CNT_L+2:
    case SOUND4CNT_L+3: return 0;
    case SOUND4CNT_H:   return apu_io.psg4.Read(4);
    case SOUND4CNT_H+1: return apu_io.psg4.Read(5);
    case SOUND4CNT_H+2:
    case SOUND4CNT_H+3: return 0;
    case WAVE_RAM + 0:
    case WAVE_RAM + 1:
    case WAVE_RAM + 2:
    case WAVE_RAM + 3:
    case WAVE_RAM + 4:
    case WAVE_RAM + 5:
    case WAVE_RAM + 6:
    case WAVE_RAM + 7:
    case WAVE_RAM + 8:
    case WAVE_RAM + 9:
    case WAVE_RAM + 10:
    case WAVE_RAM + 11:
    case WAVE_RAM + 12:
    case WAVE_RAM + 13:
    case WAVE_RAM + 14:
    case WAVE_RAM + 15: {
      return apu_io.psg3.ReadSample(address & 0xF);
    }
    case SOUNDCNT_L:   return apu_io.soundcnt.Read(0);
    case SOUNDCNT_L+1: return apu_io.soundcnt.Read(1);
    case SOUNDCNT_H:   return apu_io.soundcnt.Read(2);
    case SOUNDCNT_H+1: return apu_io.soundcnt.Read(3);
    case SOUNDCNT_X:   return apu_io.soundcnt.Read(4);
    case SOUNDCNT_X+1:
    case SOUNDCNT_X+2:
    case SOUNDCNT_X+3: return 0;
    case SOUNDBIAS:    return apu_io.bias.Read(0);
    case SOUNDBIAS+1:  return apu_io.bias.Read(1);
    case SOUNDBIAS+2:
    case SOUNDBIAS+3:  return 0;

    // Timer 0 - 3
    case TM0CNT_L:   return timer.ReadByte(0, 0);
    case TM0CNT_L+1: return timer.ReadByte(0, 1);
    case TM0CNT_H:   return timer.ReadByte(0, 2);
    case TM0CNT_H+1: return 0;
    case TM1CNT_L:   return timer.ReadByte(1, 0);
    case TM1CNT_L+1: return timer.ReadByte(1, 1);
    case TM1CNT_H:   return timer.ReadByte(1, 2);
    case TM1CNT_H+1: return 0;
    case TM2CNT_L:   return timer.ReadByte(2, 0);
    case TM2CNT_L+1: return timer.ReadByte(2, 1);
    case TM2CNT_H:   return timer.ReadByte(2, 2);
    case TM2CNT_H+1: return 0;
    case TM3CNT_L:   return timer.ReadByte(3, 0);
    case TM3CNT_L+1: return timer.ReadByte(3, 1);
    case TM3CNT_H:   return timer.ReadByte(3, 2);
    case TM3CNT_H+1: return 0;

    // Serial communication
    // @todo: implement sensible read/write behaviour for the remaining registers.
    case SIOCNT+0: return (u8)(siocnt >> 0);
    case SIOCNT+1: return (u8)(siocnt >> 8);
    case RCNT+0: return rcnt[0];
    case RCNT+1: return rcnt[1];
    case RCNT+2:
    case RCNT+3: return 0;
    case 0x04000142:
    case 0x04000143: return 0;
    case 0x0400015A:
    case 0x0400015B: return 0;

    // Keypad
    case KEYINPUT+0: return keypad.input.ReadByte(0);
    case KEYINPUT+1: return keypad.input.ReadByte(1);
    case KEYCNT:     return keypad.control.ReadByte(0);
    case KEYCNT+1:   return keypad.control.ReadByte(1);

    // IRQ controller
    case IE+0:  return irq.ReadByte(0);
    case IE+1:  return irq.ReadByte(1);
    case IF+0:  return irq.ReadByte(2);
    case IF+1:  return irq.ReadByte(3);
    case IME+0: return irq.ReadByte(4);
    case IME+1:
    case IME+2:
    case IME+3: return 0;

    // System control
    case WAITCNT+0: {
      return waitcnt.sram |
            (waitcnt.ws0[0] << 2) |
            (waitcnt.ws0[1] << 4) |
            (waitcnt.ws1[0] << 5) |
            (waitcnt.ws1[1] << 7);
    }
    case WAITCNT+1: {
      return waitcnt.ws2[0] |
            (waitcnt.ws2[1] << 2) |
            (waitcnt.phi << 3) |
            (waitcnt.prefetch ? 64 : 0) |
            (waitcnt.cgb ? 128 : 0);
    }
    case WAITCNT+2:
    case WAITCNT+3: return 0;
    case POSTFLG:   return postflg;
    case HALTCNT:   return 0;
    case 0x04000302: 
    case 0x04000303: return 0;

    case MGBA_LOG_ENABLE+0: return (u8)(mgba_log.enable >> 0);
    case MGBA_LOG_ENABLE+1: return (u8)(mgba_log.enable >> 8);
  }

  return bus->ReadOpenBus(address);
}

auto Bus::Hardware::ReadHalf(u32 address) -> u16 {
  if(AddressUsesNewIO(address)) {
    return ReadHalfNew(address);
  }

  switch(address) {
    // Timer 0 - 3
    case TM0CNT_L: return timer.ReadHalf(0, 0);
    case TM0CNT_H: return timer.ReadHalf(0, 2);
    case TM1CNT_L: return timer.ReadHalf(1, 0);
    case TM1CNT_H: return timer.ReadHalf(1, 2);
    case TM2CNT_L: return timer.ReadHalf(2, 0);
    case TM2CNT_H: return timer.ReadHalf(2, 2);
    case TM3CNT_L: return timer.ReadHalf(3, 0);
    case TM3CNT_H: return timer.ReadHalf(3, 2);

    // IRQ controller
    case IE:  return irq.ReadHalf(0);
    case IF:  return irq.ReadHalf(2);
    case IME: return irq.ReadHalf(4);
  }

  return ReadByte(address) | (ReadByte(address + 1) << 8);
}

auto Bus::Hardware::ReadWord(u32 address) -> u32 {
  if(AddressUsesNewIO(address)) {
    return ReadWordNew(address);
  }

  switch(address) {
    case TM0CNT_L: return timer.ReadWord(0);
    case TM1CNT_L: return timer.ReadWord(1);
    case TM2CNT_L: return timer.ReadWord(2);
    case TM3CNT_L: return timer.ReadWord(3);
  }

  return ReadHalf(address) | (ReadHalf(address + 2) << 16);
}

void Bus::Hardware::WriteByte(u32 address,  u8 value) {
  if(AddressUsesNewIO(address)) {
    WriteByteNew(address, value);
    return;
  }

  auto& apu_io = apu.mmio;
  auto& ppu_io = ppu.mmio;

  const bool apu_enable = apu_io.soundcnt.master_enable;

  switch(address) {
    // PPU
    // case DISPCNT+0:  ppu_io.dispcnt.Write(0, value); break;
    // case DISPCNT+1:  ppu_io.dispcnt.Write(1, value); break;
    // case GREENSWAP+0: ppu_io.greenswap = value & 1; break;
    // case GREENSWAP+1: break;
    // case DISPSTAT+0: ppu_io.dispstat.Write(0, value); break;
    // case DISPSTAT+1: ppu_io.dispstat.Write(1, value); break;
    // case BG0CNT+0:   ppu_io.bgcnt[0].Write(0, value); break;
    // case BG0CNT+1:   ppu_io.bgcnt[0].Write(1, value); break;
    // case BG1CNT+0:   ppu_io.bgcnt[1].Write(0, value); break;
    // case BG1CNT+1:   ppu_io.bgcnt[1].Write(1, value); break;
    // case BG2CNT+0:   ppu_io.bgcnt[2].Write(0, value); break;
    // case BG2CNT+1:   ppu_io.bgcnt[2].Write(1, value); break;
    // case BG3CNT+0:   ppu_io.bgcnt[3].Write(0, value); break;
    // case BG3CNT+1:   ppu_io.bgcnt[3].Write(1, value); break;
    // case BG0HOFS+0: {
    //   ppu_io.bghofs[0] &= 0xFF00;
    //   ppu_io.bghofs[0] |= value;
    //   break;
    // }
    // case BG0HOFS+1: {
    //   ppu_io.bghofs[0] &= 0x00FF;
    //   ppu_io.bghofs[0] |= (value & 1) << 8;
    //   break;
    // }
    // case BG0VOFS+0: {
    //   ppu_io.bgvofs[0] &= 0xFF00;
    //   ppu_io.bgvofs[0] |= value;
    //   break;
    // }
    // case BG0VOFS+1: {
    //   ppu_io.bgvofs[0] &= 0x00FF;
    //   ppu_io.bgvofs[0] |= (value & 1) << 8;
    //   break;
    // }
    // case BG1HOFS+0: {
    //   ppu_io.bghofs[1] &= 0xFF00;
    //   ppu_io.bghofs[1] |= value;
    //   break;
    // }
    // case BG1HOFS+1: {
    //   ppu_io.bghofs[1] &= 0x00FF;
    //   ppu_io.bghofs[1] |= (value & 1) << 8;
    //   break;
    // }
    // case BG1VOFS+0: {
    //   ppu_io.bgvofs[1] &= 0xFF00;
    //   ppu_io.bgvofs[1] |= value;
    //   break;
    // }
    // case BG1VOFS+1: {
    //   ppu_io.bgvofs[1] &= 0x00FF;
    //   ppu_io.bgvofs[1] |= (value & 1) << 8;
    //   break;
    // }
    // case BG2HOFS+0: {
    //   ppu_io.bghofs[2] &= 0xFF00;
    //   ppu_io.bghofs[2] |= value;
    //   break;
    // }
    // case BG2HOFS+1: {
    //   ppu_io.bghofs[2] &= 0x00FF;
    //   ppu_io.bghofs[2] |= (value & 1) << 8;
    //   break;
    // }
    // case BG2VOFS+0: {
    //   ppu_io.bgvofs[2] &= 0xFF00;
    //   ppu_io.bgvofs[2] |= value;
    //   break;
    // }
    // case BG2VOFS+1: {
    //   ppu_io.bgvofs[2] &= 0x00FF;
    //   ppu_io.bgvofs[2] |= (value & 1) << 8;
    //   break;
    // }
    // case BG3HOFS+0: {
    //   ppu_io.bghofs[3] &= 0xFF00;
    //   ppu_io.bghofs[3] |= value;
    //   break;
    // }
    // case BG3HOFS+1: {
    //   ppu_io.bghofs[3] &= 0x00FF;
    //   ppu_io.bghofs[3] |= (value & 1) << 8;
    //   break;
    // }
    // case BG3VOFS+0: {
    //   ppu_io.bgvofs[3] &= 0xFF00;
    //   ppu_io.bgvofs[3] |= value;
    //   break;
    // }
    // case BG3VOFS+1: {
    //   ppu_io.bgvofs[3] &= 0x00FF;
    //   ppu_io.bgvofs[3] |= (value & 1) << 8;
    //   break;
    // }
    // case BG2PA:   ppu_io.bgpa[0] = (ppu_io.bgpa[0] & 0xFF00) | (value << 0); break;
    // case BG2PA+1: ppu_io.bgpa[0] = (ppu_io.bgpa[0] & 0x00FF) | (value << 8); break;
    // case BG2PB:   ppu_io.bgpb[0] = (ppu_io.bgpb[0] & 0xFF00) | (value << 0); break;
    // case BG2PB+1: ppu_io.bgpb[0] = (ppu_io.bgpb[0] & 0x00FF) | (value << 8); break;
    // case BG2PC:   ppu_io.bgpc[0] = (ppu_io.bgpc[0] & 0xFF00) | (value << 0); break;
    // case BG2PC+1: ppu_io.bgpc[0] = (ppu_io.bgpc[0] & 0x00FF) | (value << 8); break;
    // case BG2PD:   ppu_io.bgpd[0] = (ppu_io.bgpd[0] & 0xFF00) | (value << 0); break;
    // case BG2PD+1: ppu_io.bgpd[0] = (ppu_io.bgpd[0] & 0x00FF) | (value << 8); break;
    // case BG2X:    ppu_io.bgx[0].Write(0, value); break;
    // case BG2X+1:  ppu_io.bgx[0].Write(1, value); break;
    // case BG2X+2:  ppu_io.bgx[0].Write(2, value); break;
    // case BG2X+3:  ppu_io.bgx[0].Write(3, value); break;
    // case BG2Y:    ppu_io.bgy[0].Write(0, value); break;
    // case BG2Y+1:  ppu_io.bgy[0].Write(1, value); break;
    // case BG2Y+2:  ppu_io.bgy[0].Write(2, value); break;
    // case BG2Y+3:  ppu_io.bgy[0].Write(3, value); break;
    // case BG3PA:   ppu_io.bgpa[1] = (ppu_io.bgpa[1] & 0xFF00) | (value << 0); break;
    // case BG3PA+1: ppu_io.bgpa[1] = (ppu_io.bgpa[1] & 0x00FF) | (value << 8); break;
    // case BG3PB:   ppu_io.bgpb[1] = (ppu_io.bgpb[1] & 0xFF00) | (value << 0); break;
    // case BG3PB+1: ppu_io.bgpb[1] = (ppu_io.bgpb[1] & 0x00FF) | (value << 8); break;
    // case BG3PC:   ppu_io.bgpc[1] = (ppu_io.bgpc[1] & 0xFF00) | (value << 0); break;
    // case BG3PC+1: ppu_io.bgpc[1] = (ppu_io.bgpc[1] & 0x00FF) | (value << 8); break;
    // case BG3PD:   ppu_io.bgpd[1] = (ppu_io.bgpd[1] & 0xFF00) | (value << 0); break;
    // case BG3PD+1: ppu_io.bgpd[1] = (ppu_io.bgpd[1] & 0x00FF) | (value << 8); break;
    // case BG3X:    ppu_io.bgx[1].Write(0, value); break;
    // case BG3X+1:  ppu_io.bgx[1].Write(1, value); break;
    // case BG3X+2:  ppu_io.bgx[1].Write(2, value); break;
    // case BG3X+3:  ppu_io.bgx[1].Write(3, value); break;
    // case BG3Y:    ppu_io.bgy[1].Write(0, value); break;
    // case BG3Y+1:  ppu_io.bgy[1].Write(1, value); break;
    // case BG3Y+2:  ppu_io.bgy[1].Write(2, value); break;
    // case BG3Y+3:  ppu_io.bgy[1].Write(3, value); break;
    // case WIN0H+0: ppu_io.winh[0].Write(0, value); break;
    // case WIN0H+1: ppu_io.winh[0].Write(1, value); break;
    // case WIN1H+0: ppu_io.winh[1].Write(0, value); break;
    // case WIN1H+1: ppu_io.winh[1].Write(1, value); break;
    // case WIN0V+0: ppu_io.winv[0].Write(0, value); break;
    // case WIN0V+1: ppu_io.winv[0].Write(1, value); break;
    // case WIN1V+0: ppu_io.winv[1].Write(0, value); break;
    // case WIN1V+1: ppu_io.winv[1].Write(1, value); break;
    // case WININ+0: ppu_io.winin.Write(0, value); break;
    // case WININ+1: ppu_io.winin.Write(1, value); break;
    // case WINOUT+0: ppu_io.winout.Write(0, value); break;
    // case WINOUT+1: ppu_io.winout.Write(1, value); break;
    // case MOSAIC+0: ppu_io.mosaic.Write(0, value); break;
    // case MOSAIC+1: ppu_io.mosaic.Write(1, value); break;
    // case BLDCNT+0: ppu_io.bldcnt.Write(0, value); break;
    // case BLDCNT+1: ppu_io.bldcnt.Write(1, value); break;
    // case BLDALPHA+0: ppu_io.eva = value & 0x1F; break;
    // case BLDALPHA+1: ppu_io.evb = value & 0x1F; break;
    // case BLDY: ppu_io.evy = value & 0x1F; break;

    // DMA 0 - 3
    case DMA0SAD:     dma.Write(0, 0, value); break;
    case DMA0SAD+1:   dma.Write(0, 1, value); break;
    case DMA0SAD+2:   dma.Write(0, 2, value); break;
    case DMA0SAD+3:   dma.Write(0, 3, value); break;
    case DMA0DAD:     dma.Write(0, 4, value); break;
    case DMA0DAD+1:   dma.Write(0, 5, value); break;
    case DMA0DAD+2:   dma.Write(0, 6, value); break;
    case DMA0DAD+3:   dma.Write(0, 7, value); break;
    case DMA0CNT_L:   dma.Write(0, 8, value); break;
    case DMA0CNT_L+1: dma.Write(0, 9, value); break;
    case DMA0CNT_H:   dma.Write(0, 10, value); break;
    case DMA0CNT_H+1: dma.Write(0, 11, value); break;
    case DMA1SAD:     dma.Write(1, 0, value); break;
    case DMA1SAD+1:   dma.Write(1, 1, value); break;
    case DMA1SAD+2:   dma.Write(1, 2, value); break;
    case DMA1SAD+3:   dma.Write(1, 3, value); break;
    case DMA1DAD:     dma.Write(1, 4, value); break;
    case DMA1DAD+1:   dma.Write(1, 5, value); break;
    case DMA1DAD+2:   dma.Write(1, 6, value); break;
    case DMA1DAD+3:   dma.Write(1, 7, value); break;
    case DMA1CNT_L:   dma.Write(1, 8, value); break;
    case DMA1CNT_L+1: dma.Write(1, 9, value); break;
    case DMA1CNT_H:   dma.Write(1, 10, value); break;
    case DMA1CNT_H+1: dma.Write(1, 11, value); break;
    case DMA2SAD:     dma.Write(2, 0, value); break;
    case DMA2SAD+1:   dma.Write(2, 1, value); break;
    case DMA2SAD+2:   dma.Write(2, 2, value); break;
    case DMA2SAD+3:   dma.Write(2, 3, value); break;
    case DMA2DAD:     dma.Write(2, 4, value); break;
    case DMA2DAD+1:   dma.Write(2, 5, value); break;
    case DMA2DAD+2:   dma.Write(2, 6, value); break;
    case DMA2DAD+3:   dma.Write(2, 7, value); break;
    case DMA2CNT_L:   dma.Write(2, 8, value); break;
    case DMA2CNT_L+1: dma.Write(2, 9, value); break;
    case DMA2CNT_H:   dma.Write(2, 10, value); break;
    case DMA2CNT_H+1: dma.Write(2, 11, value); break;
    case DMA3SAD:     dma.Write(3, 0, value); break;
    case DMA3SAD+1:   dma.Write(3, 1, value); break;
    case DMA3SAD+2:   dma.Write(3, 2, value); break;
    case DMA3SAD+3:   dma.Write(3, 3, value); break;
    case DMA3DAD:     dma.Write(3, 4, value); break;
    case DMA3DAD+1:   dma.Write(3, 5, value); break;
    case DMA3DAD+2:   dma.Write(3, 6, value); break;
    case DMA3DAD+3:   dma.Write(3, 7, value); break;
    case DMA3CNT_L:   dma.Write(3, 8, value); break;
    case DMA3CNT_L+1: dma.Write(3, 9, value); break;
    case DMA3CNT_H:   dma.Write(3, 10, value); break;
    case DMA3CNT_H+1: dma.Write(3, 11, value); break;

    // Sound
    case SOUND1CNT_L:   if(apu_enable) apu_io.psg1.Write(0, value); break;
    case SOUND1CNT_L+1: if(apu_enable) apu_io.psg1.Write(1, value); break;
    case SOUND1CNT_H:   if(apu_enable) apu_io.psg1.Write(2, value); break;
    case SOUND1CNT_H+1: if(apu_enable) apu_io.psg1.Write(3, value); break;
    case SOUND1CNT_X:   if(apu_enable) apu_io.psg1.Write(4, value); break;
    case SOUND1CNT_X+1: if(apu_enable) apu_io.psg1.Write(5, value); break;
    case SOUND2CNT_L:   if(apu_enable) apu_io.psg2.Write(2, value); break;
    case SOUND2CNT_L+1: if(apu_enable) apu_io.psg2.Write(3, value); break;
    case SOUND2CNT_H:   if(apu_enable) apu_io.psg2.Write(4, value); break;
    case SOUND2CNT_H+1: if(apu_enable) apu_io.psg2.Write(5, value); break;
    case SOUND3CNT_L:   if(apu_enable) apu_io.psg3.Write(0, value); break;
    case SOUND3CNT_L+1: if(apu_enable) apu_io.psg3.Write(1, value); break;
    case SOUND3CNT_H:   if(apu_enable) apu_io.psg3.Write(2, value); break;
    case SOUND3CNT_H+1: if(apu_enable) apu_io.psg3.Write(3, value); break;
    case SOUND3CNT_X:   if(apu_enable) apu_io.psg3.Write(4, value); break;
    case SOUND3CNT_X+1: if(apu_enable) apu_io.psg3.Write(5, value); break;
    case SOUND4CNT_L:   if(apu_enable) apu_io.psg4.Write(0, value); break;
    case SOUND4CNT_L+1: if(apu_enable) apu_io.psg4.Write(1, value); break;
    case SOUND4CNT_H:   if(apu_enable) apu_io.psg4.Write(4, value); break;
    case SOUND4CNT_H+1: if(apu_enable) apu_io.psg4.Write(5, value); break;
    case WAVE_RAM+0:
    case WAVE_RAM+1:
    case WAVE_RAM+2:
    case WAVE_RAM+3:
    case WAVE_RAM+4:
    case WAVE_RAM+5:
    case WAVE_RAM+6:
    case WAVE_RAM+7:
    case WAVE_RAM+8:
    case WAVE_RAM+9:
    case WAVE_RAM+10:
    case WAVE_RAM+11:
    case WAVE_RAM+12:
    case WAVE_RAM+13:
    case WAVE_RAM+14:
    case WAVE_RAM+15: {
      apu_io.psg3.WriteSample(address & 0xF, value);
      break;
    }
    case FIFO_A:   if(apu_enable) apu_io.fifo[0].WriteByte(0, value); break;
    case FIFO_A+1: if(apu_enable) apu_io.fifo[0].WriteByte(1, value); break;
    case FIFO_A+2: if(apu_enable) apu_io.fifo[0].WriteByte(2, value); break;
    case FIFO_A+3: if(apu_enable) apu_io.fifo[0].WriteByte(3, value); break;
    case FIFO_B:   if(apu_enable) apu_io.fifo[1].WriteByte(0, value); break;
    case FIFO_B+1: if(apu_enable) apu_io.fifo[1].WriteByte(1, value); break;
    case FIFO_B+2: if(apu_enable) apu_io.fifo[1].WriteByte(2, value); break;
    case FIFO_B+3: if(apu_enable) apu_io.fifo[1].WriteByte(3, value); break;
    case SOUNDCNT_L:   if(apu_enable) apu_io.soundcnt.Write(0, value); break;
    case SOUNDCNT_L+1: if(apu_enable) apu_io.soundcnt.Write(1, value); break;
    case SOUNDCNT_H:   apu_io.soundcnt.Write(2, value); break;
    case SOUNDCNT_H+1: apu_io.soundcnt.Write(3, value); break;
    case SOUNDCNT_X:   apu_io.soundcnt.Write(4, value); break;
    case SOUNDBIAS:    apu_io.bias.Write(0, value); break;
    case SOUNDBIAS+1:  apu_io.bias.Write(1, value); break;

    // Timers 0 - 3
    case TM0CNT_L:   timer.WriteByte(0, 0, value); break;
    case TM0CNT_L+1: timer.WriteByte(0, 1, value); break;
    case TM0CNT_H:   timer.WriteByte(0, 2, value); break;
    case TM1CNT_L:   timer.WriteByte(1, 0, value); break;
    case TM1CNT_L+1: timer.WriteByte(1, 1, value); break;
    case TM1CNT_H:   timer.WriteByte(1, 2, value); break;
    case TM2CNT_L:   timer.WriteByte(2, 0, value); break;
    case TM2CNT_L+1: timer.WriteByte(2, 1, value); break;
    case TM2CNT_H:   timer.WriteByte(2, 2, value); break;
    case TM3CNT_L:   timer.WriteByte(3, 0, value); break;
    case TM3CNT_L+1: timer.WriteByte(3, 1, value); break;
    case TM3CNT_H:   timer.WriteByte(3, 2, value); break;

    // Serial communication
    case SIOCNT+0: WriteHalf(SIOCNT, (siocnt & 0xFF00) | (value << 0)); break;
    case SIOCNT+1: WriteHalf(SIOCNT, (siocnt & 0x00FF) | (value << 8)); break;
    case RCNT+0: rcnt[0] = value; break;
    case RCNT+1: rcnt[1] = value; break;

    // Keypad
    case KEYCNT:   keypad.control.WriteByte(0, value); break;
    case KEYCNT+1: keypad.control.WriteByte(1, value); break;

    // IRQ controller
    case IE+0: irq.WriteByte(0, value); break;
    case IE+1: irq.WriteByte(1, value); break;
    case IF+0: irq.WriteByte(2, value); break;
    case IF+1: irq.WriteByte(3, value); break;
    case IME:  irq.WriteByte(4, value); break;

    // System control
    case WAITCNT+0: {
      waitcnt.sram  = (value >> 0) & 3;
      waitcnt.ws0[0] = (value >> 2) & 3;
      waitcnt.ws0[1] = (value >> 4) & 1;
      waitcnt.ws1[0] = (value >> 5) & 3;
      waitcnt.ws1[1] = (value >> 7) & 1;
      bus->UpdateWaitStateTable();
      break;
    }
    case WAITCNT+1: {
      const bool prefetch_old = waitcnt.prefetch;
      waitcnt.ws2[0] = (value >> 0) & 3;
      waitcnt.ws2[1] = (value >> 2) & 1;
      waitcnt.phi = (value >> 3) & 3;
      waitcnt.prefetch = (value >> 6) & 1;
      if(prefetch_old && !waitcnt.prefetch) {
        prefetch_buffer_was_disabled = true;
      }
      bus->UpdateWaitStateTable();
      break;
    }
    case POSTFLG: {
      if(cpu.state.r15 <= 0x3FFF) {
        postflg |= value & 1;
      }
      break;
    }
    case HALTCNT: {
      if(cpu.state.r15 <= 0x3FFF) {
        if(value & 0x80) {
          // haltcnt = HaltControl::Stop;
        } else {
          haltcnt = HaltControl::Halt;
          bus->Step(1);
        }
      }
      break;
    }

    default: {
      if(address >= MGBA_LOG_STRING_LO && address < MGBA_LOG_STRING_HI) {
        mgba_log.message[address & 0xFF] = value;
      }
      break;
    }
  }
}

void Bus::Hardware::WriteHalf(u32 address, u16 value) {
  if(AddressUsesNewIO(address)) {
    WriteHalfNew(address, value);
    return;
  }

  auto& apu_io = apu.mmio;

  const bool apu_enable = apu_io.soundcnt.master_enable;

  switch(address) {
    case FIFO_A+0: if(apu_enable) apu_io.fifo[0].WriteHalf(0, value); break;
    case FIFO_A+2: if(apu_enable) apu_io.fifo[0].WriteHalf(2, value); break;
    case FIFO_B+0: if(apu_enable) apu_io.fifo[1].WriteHalf(0, value); break;
    case FIFO_B+2: if(apu_enable) apu_io.fifo[1].WriteHalf(2, value); break;

    // Timers 0 - 3
    case TM0CNT_L: timer.WriteHalf(0, 0, value); break;
    case TM0CNT_H: timer.WriteHalf(0, 2, value); break;
    case TM1CNT_L: timer.WriteHalf(1, 0, value); break;
    case TM1CNT_H: timer.WriteHalf(1, 2, value); break;
    case TM2CNT_L: timer.WriteHalf(2, 0, value); break;
    case TM2CNT_H: timer.WriteHalf(2, 2, value); break;
    case TM3CNT_L: timer.WriteHalf(3, 0, value); break;
    case TM3CNT_H: timer.WriteHalf(3, 2, value); break;

    /* Do not invoke Keypad::UpdateIRQ() twice for a single 16-bit write.
     * See https://github.com/fleroviux/NanoBoyAdvance/issues/152 for details.
     */
    case KEYCNT: {
      keypad.control.WriteHalf(value);
      break;
    }

    // IRQ controller
    case IE:  irq.WriteHalf(0, value); break;
    case IF:  irq.WriteHalf(2, value); break;
    case IME: irq.WriteByte(4, value); break;

    case SIOCNT: {
      siocnt = (siocnt & 0x80u) | (value & ~0x80u);

      if(!(siocnt & 0x80u) && value & 0x80u) {
        // bit 0 (from bit  1): internal shift clock (0 = 256 KHz, 1 = 2 MHz)
        // bit 1 (from bit 12): transfer length (0 = 8-bit, 1 = 32-bit)
        static const int table[4] {
          512,
          64,
          2048,
          256
        };

        siocnt |= 0x80u;

        const int cycles = table[((siocnt >> 1) & 1u) | ((siocnt >> 11) & 2u)];

        bus->scheduler.Add(cycles, Scheduler::EventClass::SIO_transfer_done);
      }

      break;
    }

    case MGBA_LOG_SEND: {
      if(mgba_log.enable && (value & 0x100) != 0) {
        fmt::print("mGBA log: {}\n", mgba_log.message.data());
        std::fflush(stdout);
        mgba_log.message.fill(0);
      }
      break;
    }
    case MGBA_LOG_ENABLE: {
      if(value == 0xC0DE) {
        mgba_log.enable = 0x1DEA;
      }
      break;
    }

    default: {
      WriteByte(address + 0, u8(value >> 0));
      WriteByte(address + 1, u8(value >> 8));
      break;
    }
  }
}

void Bus::Hardware::WriteWord(u32 address, u32 value) {
  if(AddressUsesNewIO(address)) {
    WriteWordNew(address, value);
    return;
  }

  auto& apu_io = apu.mmio;

  const bool apu_enable = apu_io.soundcnt.master_enable;

  switch(address) {
    case FIFO_A: if(apu_enable) apu_io.fifo[0].WriteWord(value); break;
    case FIFO_B: if(apu_enable) apu_io.fifo[1].WriteWord(value); break;

    // Timers 0 - 3
    case TM0CNT_L: timer.WriteWord(0, value); break;
    case TM1CNT_L: timer.WriteWord(1, value); break;
    case TM2CNT_L: timer.WriteWord(2, value); break;
    case TM3CNT_L: timer.WriteWord(3, value); break;

    default: {
      WriteHalf(address + 0, u16(value >> 0));
      WriteHalf(address + 2, u16(value >> 16));
      break;
    }
  }
}

void Bus::SIOTransferDone() {
  if(hw.siocnt & 0x4000u) {
    hw.irq.Raise(IRQ::Source::Serial);
  }

  hw.siocnt &= ~0x80u;
}

} // namespace nba::core
