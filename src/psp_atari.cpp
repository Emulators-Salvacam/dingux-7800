/*
   Ludovic Jacomme <Ludovic.Jacomme@gmail.com>
*/
#include <stdlib.h>
#include <stdio.h>

#include <zlib.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "global.h"
#include "psp_fmgr.h"
#include "psp_kbd.h"
#include "psp_danzeff.h"
#include "psp_sdl.h"

#include "Cartridge.h"
#include "ProSystem.h"
#include "Database.h"
#include "Sound.h"

extern "C" {

static word display_palette16[256];
static uint display_palette32[65535];

void
psp_atari_reset()
{
  prosystem_Reset();
}

int
psp_atari_load_state(char *filename)
{
  int error;

  std::string std_filename(filename);
  error = prosystem_Load(filename) ? 0 : 1;

  return error;
}

int
psp_atari_save_state(char *filename)
{
  int error;

  std::string std_filename(filename);
  error = prosystem_Save(std_filename, true) ? 0 : 1;

  return error;
}

int 
psp_atari_load_rom(char *filename) 
{
  std::string std_filename(filename);
  if(! cartridge_Load(std_filename)) return 1;
  database_Load(cartridge_digest);
  prosystem_Reset();

  return 0;
}

int 
psp_atari_load_rom_buffer(char *rom_buffer, int rom_size) 
{
  if(! cartridge_Load_buffer(rom_buffer, rom_size)) return 1;
  database_Load(cartridge_digest);
  prosystem_Reset();

  return 0;
}

static void
atari_synchronize()
{
  static u32 nextclock = 1;
  static u32 next_sec_clock = 0;
  static u32 cur_num_frame = 0;

  u32 curclock = SDL_GetTicks();

  if (ATARI.atari_speed_limiter) {
    while (curclock < nextclock) {
     curclock = SDL_GetTicks();
    }
    u32 f_period = 1000 / ATARI.atari_speed_limiter;
    nextclock += f_period;
    if (nextclock < curclock) nextclock = curclock + f_period;
  }

  if (ATARI.atari_view_fps) {
    cur_num_frame++;
    if (curclock > next_sec_clock) {
      next_sec_clock = curclock + 1000;
      ATARI.atari_current_fps = cur_num_frame * (1 + ATARI.psp_skip_max_frame);
      cur_num_frame = 0;
    }
  }
}

void
psp_atari_init_palette16()
{
  for(uint index = 0; index < 256; index++) {
    word r = palette_data[(index * 3) + 0];
    word g = palette_data[(index * 3) + 1];
    word b = palette_data[(index * 3) + 2];
    display_palette16[index] = psp_sdl_rgb(r, g, b);
  }
}

static inline void
psp_atari_put_image_normal()
{
  /* 320 x 223 */
  uint height = maria_visibleArea.GetHeight( );
  uint length = maria_visibleArea.GetLength( );

  const byte* buffer = maria_surface + 
      ((maria_visibleArea.top - maria_displayArea.top) * length);

  uint pitch = PSP_LINE_SIZE;
  u16* surface = (u16*)back_surface->pixels + 8*pitch;
  uint indexY = height;
  while (indexY-- > 0) {
    uint indexX = length;
    while (indexX > 0) {
      *surface++ = display_palette16[*buffer++];
      *surface++ = display_palette16[*buffer++];
      *surface++ = display_palette16[*buffer++];
      *surface++ = display_palette16[*buffer++];
      indexX -= 4;
    }
    surface += pitch - length;
  }
}

void
psp_atari_blit_image()
{
  /* 320 x 223 */
  uint height = maria_visibleArea.GetHeight( );
  uint length = maria_visibleArea.GetLength( );

  const byte* buffer = maria_surface + 
      ((maria_visibleArea.top - maria_displayArea.top) * maria_visibleArea.GetLength( ));

  ushort* surface = (ushort*)blit_surface->pixels;
  uint pitch = blit_surface->w;
  uint indexY = height;
  while (indexY-- > 0) {
    uint indexX = length;
    const byte* src = buffer;
    while (indexX-- > 0) {
      *surface++ = display_palette16[*buffer++];
    }
    surface += pitch - length;
  }
}

void
psp_atari_refresh_screen()
{
  if (ATARI.psp_skip_cur_frame <= 0) {

    ATARI.psp_skip_cur_frame = ATARI.psp_skip_max_frame;

    psp_atari_put_image_normal(); 
    atari_synchronize();

    if (psp_kbd_is_danzeff_mode()) {
      danzeff_moveTo(-10, -65);
      danzeff_render( ATARI.danzeff_trans );
    }

    if (ATARI.atari_view_fps) {
      char buffer[32];
      sprintf(buffer, "%03d %3d", ATARI.atari_current_clock, (int)ATARI.atari_current_fps );
      psp_sdl_fill_print(0, 0, buffer, 0xffffff, 0 );
    }

    psp_sdl_flip();
  
    if (psp_screenshot_mode) {
      psp_screenshot_mode--;
      if (psp_screenshot_mode <= 0) {
        psp_sdl_save_screenshot();
        psp_screenshot_mode = 0;
      }
    }
  } else if (ATARI.psp_skip_max_frame) {
    ATARI.psp_skip_cur_frame--;
  }
}

void
psp_atari_main_loop()
{
  if (bios_Load("./7800.rom")) {
    bios_enabled = true;
  }
  /* Open default rom */
  if (psp_atari_load_rom("./default.a78")) {
    psp_sdl_exit(1);
  }

  psp_atari_init_palette16();
  psp_sdl_black_screen();

  while (1) {
    psp_update_keys();

    if(prosystem_active && !prosystem_paused) {
      prosystem_ExecuteFrame(ATARI.keyboard_data);
      sound_Store();
      psp_atari_refresh_screen();
    }
  }
}

} // extern "C"
