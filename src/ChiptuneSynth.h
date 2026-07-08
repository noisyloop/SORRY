#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// A mood is everything needed to deterministically generate a looping track:
// same seed + scale + tempo => same music every run.
struct Mood {
    std::string name;
    std::vector<int> scale;   // semitone offsets from the root, e.g. {0,2,3,5,7,8,10}
    int rootNote = 57;        // MIDI note number (57 = A3)
    double bpm = 100.0;
    uint32_t seed = 1;
    float melodyDuty = 0.5f;  // square-wave duty cycle for the melody voice
    float brightness = 1.0f;  // scales overall level a touch per mood
    int noiseDensity = 2;     // 0..4, how busy the percussion is
};

// Registry of the built-in moods. Unknown names fall back to the first entry.
const Mood& moodByName(const std::string& name);
std::vector<std::string> allMoodNames();

// Procedural 4-voice chiptune generator: melody, bass, arpeggio, noise.
// render() is called from the audio callback thread; setMood() from the main
// thread. Crossfades between moods over kCrossfadeSeconds.
class ChiptuneSynth {
public:
    static constexpr int kSampleRate = 44100;
    static constexpr double kCrossfadeSeconds = 1.5;

    void setMood(const std::string& name);

    // Fills `frames` mono int16 samples. Audio-thread only.
    void render(int16_t* out, int frames);

private:
    // One playing pattern: 4 voices stepping through a 64-step (4 bar) loop.
    struct Voice {
        double phase = 0;
        double freq = 0;
        double noteAge = 1e9;   // seconds since note-on
        double gate = 0;        // seconds the note stays held
        bool isNoise = false;
        uint32_t lfsr = 0xACE1u;
        float duty = 0.5f;
        // ADSR (seconds, sustain is a level)
        float attack = 0.005f, decay = 0.08f, sustain = 0.5f, release = 0.05f;
        float gain = 0.25f;

        float sample(double dt);
        void noteOn(double frequency, double gateSeconds);
    };

    struct Track {
        Mood mood;
        std::array<Voice, 4> voices;  // melody, bass, arp, noise
        // Pattern data, one entry per 16th-note step over 4 bars.
        // melody/bass/arpChord hold precomputed MIDI notes, -1 = rest.
        struct Step { int melody = -1; int bass = -1; int arpChord = -1; bool noise = false; bool accent = false; };
        std::vector<Step> steps;
        double stepTimer = 0;
        int stepIndex = 0;
        bool active = false;

        void start(const Mood& m);
        float tick(double dt);        // advance one sample, return mixed sample [-1,1]
    private:
        void generatePattern();
        void advanceStep();
    };

    Track a_, b_;                 // crossfade pair; one fades out, the other in
    Track* current_ = &a_;
    Track* next_ = nullptr;
    double fade_ = 0;             // 0..1 progress of crossfade

    std::mutex pendingMutex_;
    std::string pendingMood_;
    bool hasPending_ = false;
};
