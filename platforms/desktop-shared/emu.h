/*
 * Gearboy - Nintendo Game Boy Emulator
 * Copyright (C) 2012  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#ifndef EMU_H
#define	EMU_H

#include "../../src/gearboy.h"

#ifdef EMU_IMPORT
    #define EXTERN
#else
    #define EXTERN extern
#endif

EXTERN GB_Color* emu_frame_buffer;
EXTERN bool emu_audio_sync;

EXTERN void emu_init(const char* save_path);
EXTERN void emu_destroy(void);
EXTERN void emu_run_to_vblank(void);
EXTERN void emu_load_rom(const char* file_path, bool force_dmg, bool save_in_rom_dir, Cartridge::CartridgeTypes mbc);
EXTERN void emu_key_pressed(Gameboy_Keys key);
EXTERN void emu_key_released(Gameboy_Keys key);
EXTERN void emu_pause(void);
EXTERN void emu_resume(void);
EXTERN bool emu_is_paused(void);
EXTERN bool emu_is_empty(void);
EXTERN void emu_reset(bool force_dmg, bool save_in_rom_dir, Cartridge::CartridgeTypes mbc);
EXTERN void emu_memory_dump(void);
EXTERN void emu_audio_volume(float volume);
EXTERN void emu_audio_reset(void);
EXTERN bool emu_is_audio_enabled(void);
EXTERN void emu_dmg_palette(GB_Color& color1, GB_Color& color2, GB_Color& color3, GB_Color& color4);
EXTERN void emu_dmg_predefined_palette(int palette);
EXTERN bool emu_is_cgb(void);
EXTERN void emu_save_ram(const char* file_path);
EXTERN void emu_load_ram(const char* file_path, bool force_dmg, bool save_in_rom_dir, Cartridge::CartridgeTypes mbc);
EXTERN void emu_save_state_slot(int index);
EXTERN void emu_load_state_slot(int index);
EXTERN void emu_save_state_file(const char* file_path);
EXTERN void emu_load_state_file(const char* file_path);
EXTERN void emu_add_cheat(const char* cheat);
EXTERN void emu_clear_cheats();
EXTERN void emu_get_info(char* info);

#undef EMU_IMPORT
#undef EXTERN
#endif	/* EMU_H */