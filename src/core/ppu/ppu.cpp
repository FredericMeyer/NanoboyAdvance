/*
 * Copyright (C) 2018 Frederic Meyer. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <algorithm>
#include <cstring>

#include "../cpu.hpp"
#include "ppu.hpp"

using namespace NanoboyAdvance::GBA;

constexpr std::uint16_t PPU::s_color_transparent;
constexpr int PPU::s_wait_cycles[3];

PPU::PPU(CPU* cpu) 
    : cpu(cpu)
    , config(cpu->config)
    , pram(cpu->memory.pram)
    , vram(cpu->memory.vram)
    , oam(cpu->memory.oam)
{
    InitBlendTable();
    Reset();
}

void PPU::Reset() {
    mmio.dispcnt.Reset();
    mmio.dispstat.Reset();
    mmio.vcount = 0;

    for (int i = 0; i < 4; i++) {
        mmio.bgcnt[i].Reset();
        mmio.bghofs[i] = 0;
        mmio.bgvofs[i] = 0;
    }

    mmio.eva = 0;
    mmio.evb = 0;
    mmio.evy = 0;
    mmio.bldcnt.Reset();
    
    wait_cycles = 0;
    Next(Phase::SCANLINE);
    //RenderScanline();
}

void PPU::InitBlendTable() {
    for (int color0 = 0; color0 <= 31; color0++)
    for (int color1 = 0; color1 <= 31; color1++)
    for (int factor0 = 0; factor0 <= 16; factor0++)
    for (int factor1 = 0; factor1 <= 16; factor1++) {
        int result = (color0 * factor0 + color1 * factor1) >> 4;

        blend_table[factor0][factor1][color0][color1] = std::min<std::uint8_t>(result, 31);
    }
}

void PPU::Next(Phase phase) {
    this->phase  = phase;
    wait_cycles += s_wait_cycles[static_cast<int>(phase)];
}

auto PPU::ConvertColor(std::uint16_t color) -> std::uint32_t {
    int r = (color >>  0) & 0x1F;
    int g = (color >>  5) & 0x1F;
    int b = (color >> 10) & 0x1F;

    return r << 19 |
           g << 11 |
           b <<  3 |
           0xFF000000;
}

void PPU::Tick() {
    auto& vcount = mmio.vcount;
    auto& dispstat = mmio.dispstat;

    switch (phase) {
        case Phase::SCANLINE: {
            Next(Phase::HBLANK);
            dispstat.hblank_flag = 1;

            if (dispstat.hblank_irq_enable) {
                cpu->mmio.irq_if |= CPU::INT_HBLANK;
            }
            break;
        }
        case Phase::HBLANK: {
            dispstat.hblank_flag = 0;
            dispstat.vcount_flag = ++vcount == dispstat.vcount_setting;

            if (dispstat.vcount_flag && dispstat.vcount_irq_enable) {
                cpu->mmio.irq_if |= CPU::INT_VCOUNT;
            }

            if (vcount == 160) {
                dispstat.vblank_flag = 1;
                Next(Phase::VBLANK);

                if (dispstat.vblank_irq_enable) {
                    cpu->mmio.irq_if |= CPU::INT_VBLANK;
                }
            } else {
                Next(Phase::SCANLINE);
                RenderScanline();
            }
            break;
        }
        case Phase::VBLANK: {
            if (vcount == 227) {
                Next(Phase::SCANLINE);

                /* Update vertical counter. */
                vcount = 0;
                dispstat.vcount_flag = dispstat.vcount_setting == 0;

                RenderScanline();
            } else {
                Next(Phase::VBLANK);

                if (vcount == 226)
                    dispstat.vblank_flag = 0;
                
                /* Update vertical counter. */
                dispstat.vcount_flag = ++vcount == dispstat.vcount_setting;
            }

            if (dispstat.vcount_flag && dispstat.vcount_irq_enable) {
                cpu->mmio.irq_if |= CPU::INT_VCOUNT;
            }
            break;
        }
    }
}

void PPU::RenderScanline() {
    std::uint16_t vcount = mmio.vcount;
    std::uint32_t* line = &config->video.output[vcount * 240];

    if (mmio.dispcnt.forced_blank) {
        for (int x = 0; x < 240; x++)
            line[x] = ConvertColor(0x7FFF);
    } else {
        std::uint16_t backdrop = ReadPalette(0, 0);
        std::uint32_t fill = backdrop * 0x00010001;

        /* Reset scanline buffers. */
        for (int i = 0; i < 240; i++) {
            ((std::uint32_t*)pixel)[i] = fill;
        }
        for (int i = 0; i < 60; i++) {
            ((std::uint32_t*)obj_attr)[i] = 0;
            ((std::uint32_t*)priority)[i] = 0x04040404;
        }
        for (int i = 0; i < 120; i++) {
            ((std::uint32_t*)layer)[i] = 0x05050505;
        }

        /* Render window masks. */
        if (mmio.dispcnt.enable[6])
        	RenderWindow(0);
        if (mmio.dispcnt.enable[7])
        	RenderWindow(1);
        
        /* TODO: how does HW behave when we select mode 6 or 7? */
        switch (mmio.dispcnt.mode) {
            case 0: {
                for (int i = 3; i >= 0; i--) {
                    if (mmio.dispcnt.enable[i])
                        RenderLayerText(i);
                }
                if (mmio.dispcnt.enable[4])
                    RenderLayerOAM();
                break;
            }
            case 1:
                break;
            case 2:
                break;
            case 3:
                break;
            case 4: {
                int frame = mmio.dispcnt.frame * 0xA000;
                int offset = frame + vcount * 240;
                for (int x = 0; x < 240; x++) {
                    line[x] = ConvertColor(ReadPalette(0, cpu->memory.vram[offset + x]));
                }
                break;
            }
            case 5:
                break;
        }

        auto& bldcnt = mmio.bldcnt;
                
        for (int x = 0; x < 240; x++) {
            auto sfx = bldcnt.sfx;
            int layer1 = layer[0][x];
            int layer2 = layer[1][x];

            bool is_alpha_obj = (layer[0][x] == 4) && (obj_attr[x] & OBJ_IS_ALPHA);               
            bool is_sfx_1 = bldcnt.targets[0][layer1] || is_alpha_obj;
            bool is_sfx_2 = bldcnt.targets[1][layer2];

            if (is_alpha_obj && is_sfx_2) {
                sfx = BlendControl::Effect::SFX_BLEND;
            }

            if (sfx != BlendControl::Effect::SFX_NONE && is_sfx_1 &&
                    (is_sfx_2 || sfx != BlendControl::Effect::SFX_BLEND)) {
                Blend(&pixel[0][x], pixel[1][x], sfx);
            }
        }

        for (int x = 0; x < 240; x++) {
            line[x] = ConvertColor(pixel[0][x]);
        }
    }
}

void PPU::RenderWindow(int id) {
	int line = mmio.vcount;
	auto& winv = mmio.winv[id];

	if ((winv.min <= winv.max && (line < winv.min || line >= winv.max)) ||
	    (winv.min >  winv.max && (line < winv.min && line >= winv.max)) )
	{
		win_active[id] = false;
	} else {
		auto& winh = mmio.winh[id];

		win_active[id] = true;

		if (winh._changed) {
			if (winh.min <= winh.max) {
				for (int x = 0; x < 240; x++) {
					win_mask[id][x] = x >= winh.min && x < winh.max;
				}
			} else {
				for (int x = 0; x < 240; x++) {
					win_mask[id][x] = x >= winh.min || x < winh.max;
				}
			}
		}
	}
}

void PPU::Blend(std::uint16_t* _target1, std::uint16_t target2, BlendControl::Effect sfx) {
    std::uint16_t target1 = *_target1;

    int r1 = (target1 >>  0) & 0x1F;
    int g1 = (target1 >>  5) & 0x1F;
    int b1 = (target1 >> 10) & 0x1F;

    switch (sfx) {
        case BlendControl::Effect::SFX_BLEND: {
            int eva = std::min<int>(16, mmio.eva);
            int evb = std::min<int>(16, mmio.evb);

            int r2 = (target2 >>  0) & 0x1F;
            int g2 = (target2 >>  5) & 0x1F;
            int b2 = (target2 >> 10) & 0x1F;

            r1 = blend_table[eva][evb][r1][r2];
            g1 = blend_table[eva][evb][g1][g2];
            b1 = blend_table[eva][evb][b1][b2];
            break;
        }
        case BlendControl::Effect::SFX_BRIGHTEN: {
            int evy = std::min<int>(16, mmio.evy);

            r1 = blend_table[16 - evy][evy][r1][31];
            g1 = blend_table[16 - evy][evy][g1][31];
            b1 = blend_table[16 - evy][evy][b1][31];
            break;
        }
        case BlendControl::Effect::SFX_DARKEN: {
            int evy = std::min<int>(16, mmio.evy);

            r1 = blend_table[16 - evy][evy][r1][0];
            g1 = blend_table[16 - evy][evy][g1][0];
            b1 = blend_table[16 - evy][evy][b1][0];
            break;
        }
    }

    *_target1 = (r1<<0) | (g1<<5) | (b1<<10);
}