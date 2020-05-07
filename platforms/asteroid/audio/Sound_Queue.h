// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 2015 VBA-M development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef __VBA_SOUND_SDL_H__
#define __VBA_SOUND_SDL_H__

#include "ringbuffer.h"
#include "SoundDriver.h"

#include "SDL.h"

class Sound_Queue : public SoundDriver {
public:
        Sound_Queue();
        virtual ~Sound_Queue();

        virtual bool init(long sampleRate);
	const char* start( long sample_rate, int chan_count = 1 );
        virtual void pause();
        virtual void reset();
        virtual void resume();
        virtual void write(const sample_t *finalWave, int length);

protected:
        static void soundCallback(void* data, uint8_t* stream, int length);
        virtual void read(sample_t* stream, int length);
        virtual bool should_wait();
        virtual std::size_t buffer_size();
        virtual void deinit();

private:
        RingBuffer<sample_t> samples_buf;

        SDL_mutex* mutex;
        SDL_cond* buf_ready;
        SDL_AudioSpec audio_spec;

        unsigned short current_rate;

        bool initialized = false;

        // Defines what delay in seconds we keep in the sound buffer
        static const double buftime;
};

#endif // __VBA_SOUND_SDL_H__
