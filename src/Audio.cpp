#include "Audio.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void musicHook(void* userdata, Uint8* stream, int len) {
    auto* synth = static_cast<ChiptuneSynth*>(userdata);
    synth->render(reinterpret_cast<int16_t*>(stream), len / 2);
}

// Renders a short square-wave sweep into a 16-bit mono buffer.
std::vector<Uint8> renderSweep(double startHz, double endHz, double seconds,
                               float duty, float gain) {
    const int rate = ChiptuneSynth::kSampleRate;
    const int frames = static_cast<int>(seconds * rate);
    std::vector<Uint8> buf(static_cast<size_t>(frames) * 2);
    auto* out = reinterpret_cast<int16_t*>(buf.data());
    double phase = 0;
    for (int i = 0; i < frames; ++i) {
        double t = static_cast<double>(i) / frames;
        double freq = startHz + (endHz - startHz) * t;
        phase += freq / rate;
        phase -= std::floor(phase);
        float env = static_cast<float>(1.0 - t);  // simple linear decay
        float s = (phase < duty ? 1.0f : -1.0f) * env * env * gain;
        out[i] = static_cast<int16_t>(s * 20000.0f);
    }
    return buf;
}

}  // namespace

Audio::Audio(Assets& assets) : assets_(assets) {
    // Mono S16 at the synth's rate so the music hook can write samples directly.
    if (Mix_OpenAudioDevice(ChiptuneSynth::kSampleRate, AUDIO_S16SYS, 1, 1024,
                            nullptr, 0) != 0)
        throw std::runtime_error(std::string("Mix_OpenAudioDevice failed: ") + Mix_GetError());
    open_ = true;
    Mix_AllocateChannels(8);
    Mix_HookMusic(musicHook, &synth_);
    hookActive_ = true;
}

Audio::~Audio() {
    if (open_) {
        Mix_HookMusic(nullptr, nullptr);
        // Free music and chunks before closing the device.
        music_.reset();
        sfxChunks_.clear();
        Mix_CloseAudio();
    }
}

std::string Audio::findMusicFile(const std::string& moodName) const {
    if (assets_.rootDir().empty())
        return "";
    for (const char* ext : {".ogg", ".wav", ".mp3", ".flac"}) {
        std::string path = assets_.rootDir() + "/music/" + moodName + ext;
        if (std::ifstream(path).good())
            return path;
    }
    return "";
}

void Audio::playMood(const std::string& moodName) {
    moodName_ = moodName;

    if (std::string file = findMusicFile(moodName); !file.empty()) {
        MusicPtr m(Mix_LoadMUS(file.c_str()));
        if (m) {
            if (hookActive_) {
                Mix_HookMusic(nullptr, nullptr);
                hookActive_ = false;
            }
            music_ = std::move(m);
            Mix_FadeInMusic(music_.get(), -1,
                            static_cast<int>(ChiptuneSynth::kCrossfadeSeconds * 1000));
            return;
        }
        std::cerr << "[audio] " << file << " exists but failed to load ("
                  << Mix_GetError() << "); using the synth instead\n";
    }

    if (!hookActive_) {
        Mix_HaltMusic();
        music_.reset();
        Mix_HookMusic(musicHook, &synth_);
        hookActive_ = true;
    }
    synth_.setMood(moodName);
}

void Audio::playSfx(const std::string& name) {
    Mix_Chunk* chunk = assets_.sfx(name);  // real file wins if present
    if (!chunk)
        chunk = proceduralSfx(name);
    if (chunk)
        Mix_PlayChannel(-1, chunk, 0);
}

Mix_Chunk* Audio::proceduralSfx(const std::string& name) {
    if (auto it = sfxChunks_.find(name); it != sfxChunks_.end())
        return it->second.get();

    std::vector<Uint8> buf;
    if (name == "blip")
        buf = renderSweep(880, 880, 0.03, 0.5f, 0.35f);
    else if (name == "pickup")
        buf = renderSweep(523, 1568, 0.18, 0.25f, 0.5f);
    else if (name == "confirm")
        buf = renderSweep(659, 988, 0.09, 0.5f, 0.4f);
    else if (name == "bump")
        buf = renderSweep(160, 90, 0.08, 0.5f, 0.5f);
    else
        return nullptr;

    // Mix_QuickLoad_RAW does not copy, so keep the buffer alive alongside it.
    auto [it, inserted] = sfxBuffers_.emplace(name, std::move(buf));
    Mix_Chunk* chunk = Mix_QuickLoad_RAW(it->second.data(),
                                         static_cast<Uint32>(it->second.size()));
    if (!chunk) {
        std::cerr << "[audio] Mix_QuickLoad_RAW failed for " << name << "\n";
        return nullptr;
    }
    sfxChunks_[name] = ChunkPtr(chunk);
    return chunk;
}
