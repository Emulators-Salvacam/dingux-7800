/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include "global.h"
#include "psp_sdl.h"
#include "psp_sound.h"

  static u16 *psp_buffer = 0;
  static u16 *psp_mix_buffer = 0;

         int psp_buffer_length = 0;

  static volatile int psp_buffer_read_index   = 0;
  static volatile int psp_buffer_write_index  = 0;
  static volatile int psp_buffer_full = 0;

void
psp_sound_send_buffer(unsigned char *sample, int length)
{
  int index = 0;
  while (length-- > 0) {

    int sample_value = ((signed char)sample[ index++ ]) << 8;
    if (sample_value > 44100) sample_value = 44100;
    if (sample_value < -44100) sample_value = -44100;

    psp_buffer[ psp_buffer_write_index++ ] = sample_value;
    psp_buffer[ psp_buffer_write_index++ ] = sample_value;

    if (psp_buffer_write_index >= psp_buffer_length) {
      psp_buffer_write_index = 0;
    }
  }
}

static void 
psp_sound_callback(void *udata, u8 *stream, int length)
{
  if (ATARI.atari_snd_enable) {

    int index = 0;
    u16* src = (u16 *)(psp_buffer + psp_buffer_read_index);
    u16* tgt = (u16 *)psp_mix_buffer;
    int length_w = length / 2;

    while (length_w-- > 0) {
      /* Too fast ? */
      *tgt++ = *src++;
      //psp_buffer[ psp_buffer_read_index++ ] = 0;
      if (++psp_buffer_read_index >= psp_buffer_length) {
        psp_buffer_read_index = 0;
        src = (u16 *)psp_buffer;
      }
    }
    long volume = (SDL_MIX_MAXVOLUME * gp2xGetSoundVolume()) / 100;
    SDL_MixAudio(stream, (unsigned char *)psp_mix_buffer, length, volume);
  } else {
    memset(stream, 0, length);
  }
}

int
psp_sound_init()
{
 SDL_AudioSpec wanted;
 SDL_AudioSpec obtained;

  memset(&wanted, 0, sizeof(wanted));
  memset(&obtained, 0, sizeof(obtained));

  wanted.freq     = 44100;
  wanted.format   = AUDIO_S16;
  wanted.channels = 1;
  wanted.samples  = 1024;
  wanted.callback = psp_sound_callback;
  wanted.userdata = NULL;

  /* Open the sound device, forcing the desired format */
  if ( SDL_OpenAudio(&wanted, &obtained) < 0 ) {
    fprintf(stderr, "Couldn't open sound: %s\n", SDL_GetError());
    psp_buffer_length = wanted.samples;
    psp_buffer = (u16 *)malloc(psp_buffer_length * sizeof(u16)); 
    return(0);
  }
  psp_buffer_length = 1 * obtained.samples;
  psp_buffer = (u16 *)malloc(psp_buffer_length * sizeof(u16)); 
  memset(psp_buffer, 0, sizeof(u16) * psp_buffer_length);
  psp_mix_buffer = (u16 *)malloc(2 * psp_buffer_length * sizeof(u16)); 
  memset(psp_mix_buffer, 0, 2 * sizeof(u16) * psp_buffer_length);

  psp_buffer_read_index  = 0;
  psp_buffer_write_index = 0;

  SDL_Delay(1000);        // Give sound some time to init
  SDL_PauseAudio(0);


  return(1);
}

void 
psp_sound_pause(void)
{
  SDL_PauseAudio(1);
}

void 
psp_sound_resume(void)
{
  if (ATARI.atari_snd_enable) {
    SDL_PauseAudio(0);
  }
}

void 
psp_sound_shutdown(void)
{
  SDL_CloseAudio();
}

