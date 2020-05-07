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

#include <cmath>
#include <iostream>
#include <SDL_events.h>
#include "Sound_Queue.h"

// Hold up to 300 ms of data in the ring buffer
const double Sound_Queue::buftime = 0.300;

Sound_Queue::Sound_Queue():
    samples_buf(0),
    initialized(false)
{}

void Sound_Queue::soundCallback(void* data, uint8_t* stream, int len) {
    reinterpret_cast<Sound_Queue*>(data)->read(reinterpret_cast<sample_t*>(stream), len);
}

bool Sound_Queue::should_wait() {
    return current_rate;
}

std::size_t Sound_Queue::buffer_size() {
    SDL_LockMutex(mutex);
    std::size_t size = samples_buf.used();
    SDL_UnlockMutex(mutex);

    return size;
}

void Sound_Queue::read(sample_t* stream, int length) {
    if (length <= 0)
        return;

    SDL_memset(stream, audio_spec.silence, length);

    // if not initialzed, paused or shutting down, do nothing
    if (!initialized)
        return;

    SDL_LockMutex(mutex);

    samples_buf.read(stream, std::min((std::size_t)(length / 2), samples_buf.used()));

	SDL_CondSignal(buf_ready);
    SDL_UnlockMutex(mutex);
}

void Sound_Queue::write(const sample_t * finalWave, int length) {
    if (!initialized)
        return;

    SDL_LockMutex(mutex);

    unsigned int samples = length / 4;
    std::size_t avail;

    while ((avail = samples_buf.avail() / 2) < samples) {
        samples_buf.write(finalWave, avail * 2);

        finalWave += avail * 2;
        samples -= avail;

		SDL_CondWait(buf_ready, mutex);
    }

    samples_buf.write(finalWave, samples * 2);

    SDL_UnlockMutex(mutex);
}

const char* Sound_Queue::start( long sample_rate, int chan_count )
{
    return init(sample_rate) ? NULL : "";
}


bool Sound_Queue::init(long sampleRate) {
    if (initialized) deinit();

    SDL_AudioSpec audio;
    SDL_memset(&audio, 0, sizeof(audio));

    audio.freq     = sampleRate;

    audio.format   = AUDIO_S16SYS;
    audio.channels = 2;
    audio.samples  = 2048;
    audio.callback = soundCallback;
    audio.userdata = this;

    if (!SDL_WasInit(SDL_INIT_AUDIO)) SDL_Init(SDL_INIT_AUDIO);

	if(SDL_OpenAudio(&audio, &audio_spec) < 0) {
        std::cerr << "Failed to open audio: " << SDL_GetError() << std::endl;
        return false;
    }

    samples_buf.reset(std::ceil(buftime * sampleRate * 2));

    buf_ready       = SDL_CreateCond();
    mutex          = SDL_CreateMutex();

    // turn off audio events because we are not processing them
#if SDL_VERSION_ATLEAST(2, 0, 4)
    SDL_EventState(SDL_AUDIODEVICEADDED,   SDL_IGNORE);
    SDL_EventState(SDL_AUDIODEVICEREMOVED, SDL_IGNORE);
#endif
    SDL_PauseAudio( false );

    return initialized = true;
}

void Sound_Queue::deinit() {
    if (!initialized)
        return;

    initialized = false;

    SDL_DestroyCond(buf_ready);

    SDL_DestroyMutex(mutex);
    mutex = nullptr;

	SDL_PauseAudio( true );
	SDL_CloseAudio();
}

Sound_Queue::~Sound_Queue() {
    deinit();
}

void Sound_Queue::pause() {}
void Sound_Queue::resume() {}

void Sound_Queue::reset() {}
