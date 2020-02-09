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
//#include "ConfigManager.h"
//#include "../gba/Globals.h"
//#include "../gba/Sound.h"

//extern int emulating;

// Hold up to 300 ms of data in the ring buffer
const double Sound_Queue::buftime = 0.300;

Sound_Queue::Sound_Queue():
    samples_buf(0),
    initialized(false)
{}

void Sound_Queue::soundCallback(void* data, uint8_t* stream, int len) {
    reinterpret_cast<Sound_Queue*>(data)->read(reinterpret_cast<uint16_t*>(stream), len);
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

void Sound_Queue::read(uint16_t* stream, int length) {
    if (length <= 0)
        return;

    SDL_memset(stream, audio_spec.silence, length);

    // if not initialzed, paused or shutting down, do nothing
    if (!initialized)
        return;

    if (!buffer_size()) {
        if (should_wait())
            SDL_SemWait(data_available);
        else
            return;
    }

    SDL_LockMutex(mutex);

    samples_buf.read(stream, std::min((std::size_t)(length / 2), samples_buf.used()));

    SDL_UnlockMutex(mutex);

    SDL_SemPost(data_read);
}

void Sound_Queue::write(uint16_t * finalWave, int length) {
    if (!initialized)
        return;

    SDL_LockMutex(mutex);

    //if (SDL_GetAudioDeviceStatus(sound_device) != SDL_AUDIO_PLAYING)
	//SDL_PauseAudioDevice(sound_device, 0);
	SDL_PauseAudio( false );

    unsigned int samples = length / 4;
    std::size_t avail;

    while ((avail = samples_buf.avail() / 2) < samples) {
	samples_buf.write(finalWave, avail * 2);

	finalWave += avail * 2;
	samples -= avail;

	SDL_UnlockMutex(mutex);

	SDL_SemPost(data_available);

	if (should_wait())
	    SDL_SemWait(data_read);
	else
	    // Drop the remainder of the audio data
	    return;

	SDL_LockMutex(mutex);
    }

    samples_buf.write(finalWave, samples * 2);

    SDL_UnlockMutex(mutex);
}


bool Sound_Queue::init(long sampleRate) {
    if (initialized) deinit();

    SDL_AudioSpec audio;
    SDL_memset(&audio, 0, sizeof(audio));

    // for "no throttle" use regular rate, audio is just dropped
    //audio.freq     = current_rate ? sampleRate * (current_rate / 100.0) : sampleRate;
    audio.freq     = sampleRate * 0.5;

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
    //sound_device = SDL_OpenAudioDevice(NULL, 0, &audio, &audio_spec, SDL_AUDIO_ALLOW_ANY_CHANGE);

    /*if(sound_device == 0) {
        std::cerr << "Failed to open audio: " << SDL_GetError() << std::endl;
        return false;
    }*/

    samples_buf.reset(std::ceil(buftime * sampleRate * 2));

    mutex          = SDL_CreateMutex();
    data_available = SDL_CreateSemaphore(0);
    data_read      = SDL_CreateSemaphore(1);

    // turn off audio events because we are not processing them
#if SDL_VERSION_ATLEAST(2, 0, 4)
    SDL_EventState(SDL_AUDIODEVICEADDED,   SDL_IGNORE);
    SDL_EventState(SDL_AUDIODEVICEREMOVED, SDL_IGNORE);
#endif

    return initialized = true;
}

void Sound_Queue::deinit() {
    if (!initialized)
        return;

    initialized = false;

    SDL_LockMutex(mutex);
    /*int is_emulating = emulating;
    emulating = 0;*/
    SDL_SemPost(data_available);
    SDL_SemPost(data_read);
    SDL_UnlockMutex(mutex);

    SDL_Delay(100);

    SDL_DestroySemaphore(data_available);
    data_available = nullptr;
    SDL_DestroySemaphore(data_read);
    data_read      = nullptr;

    SDL_DestroyMutex(mutex);
    mutex = nullptr;

   // SDL_CloseAudioDevice(sound_device);
	SDL_PauseAudio( true );
	SDL_CloseAudio();

    //emulating = is_emulating;
}

Sound_Queue::~Sound_Queue() {
    deinit();
}

void Sound_Queue::pause() {}
void Sound_Queue::resume() {}

void Sound_Queue::reset() {
 //   init(soundGetSampleRate());
}

void Sound_Queue::setThrottle(unsigned short throttle_) {
//    current_rate = throttle_;
//    reset();
}
