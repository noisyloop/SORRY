#pragma once

#include <SDL_mixer.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Assets.h"
#include "ChiptuneSynth.h"

struct MixMusicDeleter { void operator()(Mix_Music* m) const { if (m) Mix_FreeMusic(m); } };
using MusicPtr = std::unique_ptr<Mix_Music, MixMusicDeleter>;

// SDL_mixer setup + SFX + BGM.
//
// BGM is procedural by default: ChiptuneSynth is fed through Mix_HookMusic so
// synth music and file-based SFX share one device, and mood crossfades happen
// inside the synth. If assets/music/<mood>.ogg (or .wav/.mp3/.flac) exists it
// wins over the synth — drop in recorded tracks with no code changes.
// SFX prefer assets/sfx/<name>.wav and fall back to small procedurally
// generated square-wave chunks.
class Audio {
public:
    explicit Audio(Assets& assets);  // throws std::runtime_error on failure
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    void playMood(const std::string& moodName);
    const std::string& currentMood() const { return moodName_; }

    // Built-in names: "blip" (dialogue tick), "pickup", "bump", "confirm".
    void playSfx(const std::string& name);

private:
    Mix_Chunk* proceduralSfx(const std::string& name);
    std::string findMusicFile(const std::string& moodName) const;

    Assets& assets_;
    ChiptuneSynth synth_;
    std::string moodName_;
    MusicPtr music_;
    std::unordered_map<std::string, std::vector<Uint8>> sfxBuffers_;
    std::unordered_map<std::string, ChunkPtr> sfxChunks_;
    bool open_ = false;
    bool hookActive_ = false;
};
