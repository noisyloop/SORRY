#include "ChiptuneSynth.h"

#include <algorithm>
#include <cmath>

namespace {

// Deterministic LCG so patterns are stable across runs and platforms.
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1) {}
    uint32_t next() { s = s * 1664525u + 1013904223u; return s; }
    int range(int n) { return static_cast<int>((next() >> 8) % static_cast<uint32_t>(n)); }
    bool chance(int percent) { return range(100) < percent; }
};

double midiToFreq(int note) {
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

const std::vector<Mood>& moodTable() {
    static const std::vector<Mood> moods = {
        // name                scale (semitones)          root  bpm  seed  duty  bright noise
        {"melancholy_a_minor", {0, 2, 3, 5, 7, 8, 10},      57,  76, 0xA110, 0.50f, 0.85f, 1},
        {"tense_chromatic",    {0, 1, 3, 4, 6, 7, 9, 10},   58, 118, 0xBADD, 0.25f, 0.95f, 3},
        {"calm_pentatonic",    {0, 2, 4, 7, 9},             60,  84, 0xCA1B, 0.50f, 0.80f, 1},
        {"dread_diminished",   {0, 2, 3, 5, 6, 8, 9, 11},   55,  66, 0xD3AD, 0.125f, 0.90f, 2},
        {"playful_major",      {0, 2, 4, 5, 7, 9, 11},      62, 132, 0x91A9, 0.50f, 1.00f, 3},
        {"dream_lydian",       {0, 2, 4, 6, 7, 9, 11},      59,  92, 0xD12A, 0.375f, 0.85f, 2},
    };
    return moods;
}

}  // namespace

const Mood& moodByName(const std::string& name) {
    for (const auto& m : moodTable())
        if (m.name == name) return m;
    return moodTable().front();
}

std::vector<std::string> allMoodNames() {
    std::vector<std::string> names;
    for (const auto& m : moodTable()) names.push_back(m.name);
    return names;
}

// ---------------------------------------------------------------- Voice ----

void ChiptuneSynth::Voice::noteOn(double frequency, double gateSeconds) {
    freq = frequency;
    gate = gateSeconds;
    noteAge = 0;
    // Retrigger the phase so attacks click consistently (very chiptune).
    phase = 0;
}

float ChiptuneSynth::Voice::sample(double dt) {
    noteAge += dt;

    float env;
    if (noteAge < attack)
        env = static_cast<float>(noteAge / attack);
    else if (noteAge < attack + decay)
        env = 1.0f - (1.0f - sustain) * static_cast<float>((noteAge - attack) / decay);
    else
        env = sustain;
    if (noteAge > gate)
        env *= std::max(0.0f, 1.0f - static_cast<float>((noteAge - gate) / release));
    if (env <= 0.0f)
        return 0.0f;

    float raw;
    if (isNoise) {
        // LFSR white noise clocked by `freq` (higher = brighter hit).
        phase += freq * dt;
        while (phase >= 1.0) {
            phase -= 1.0;
            uint32_t bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
            lfsr = (lfsr >> 1) | (bit << 15);
        }
        raw = (lfsr & 1u) ? 1.0f : -1.0f;
    } else {
        phase += freq * dt;
        phase -= std::floor(phase);
        raw = (phase < duty) ? 1.0f : -1.0f;
    }
    return raw * env * gain;
}

// ---------------------------------------------------------------- Track ----

void ChiptuneSynth::Track::start(const Mood& m) {
    mood = m;
    stepTimer = 0;
    stepIndex = 0;
    active = true;

    auto& melody = voices[0];
    melody.duty = mood.melodyDuty;
    melody.gain = 0.22f;
    melody.attack = 0.004f; melody.decay = 0.10f; melody.sustain = 0.55f; melody.release = 0.06f;

    auto& bass = voices[1];
    bass.duty = 0.5f;
    bass.gain = 0.20f;
    bass.attack = 0.006f; bass.decay = 0.05f; bass.sustain = 0.7f; bass.release = 0.04f;

    auto& arp = voices[2];
    arp.duty = 0.25f;
    arp.gain = 0.10f;
    arp.attack = 0.002f; arp.decay = 0.03f; arp.sustain = 0.3f; arp.release = 0.03f;

    auto& noise = voices[3];
    noise.isNoise = true;
    noise.gain = 0.14f;
    noise.attack = 0.001f; noise.decay = 0.04f; noise.sustain = 0.2f; noise.release = 0.05f;

    generatePattern();
}

void ChiptuneSynth::Track::generatePattern() {
    constexpr int kBars = 4;
    constexpr int kStepsPerBar = 16;
    steps.assign(kBars * kStepsPerBar, {});

    Rng rng(mood.seed);
    const auto& scale = mood.scale;
    const int degrees = static_cast<int>(scale.size());

    // A tiny chord progression: one scale degree per bar, always coming home.
    std::array<int, kBars> chordDegree{};
    chordDegree[0] = 0;
    for (int bar = 1; bar < kBars; ++bar)
        chordDegree[bar] = rng.range(degrees);
    chordDegree[kBars - 1] = rng.chance(50) ? 0 : chordDegree[kBars - 1];

    auto scaleNote = [&](int degree, int octave) {
        int wrapped = ((degree % degrees) + degrees) % degrees;
        int octShift = (degree - wrapped) / degrees;
        return mood.rootNote + scale[wrapped] + 12 * (octave + octShift);
    };

    int melodyDegree = degrees;  // start an octave up, on the root
    for (int bar = 0; bar < kBars; ++bar) {
        int chord = chordDegree[bar];
        for (int i = 0; i < kStepsPerBar; ++i) {
            auto& st = steps[bar * kStepsPerBar + i];

            // Melody: a lazy random walk with rests; denser on faster moods.
            int noteChance = mood.bpm > 110 ? 55 : 40;
            if (i % 2 == 0 && rng.chance(noteChance)) {
                melodyDegree += rng.range(5) - 2;  // -2..+2 steps
                melodyDegree = std::clamp(melodyDegree, degrees - 2, 2 * degrees + 3);
                st.melody = scaleNote(melodyDegree, 1);
                st.accent = (i % 8 == 0);
            }

            // Bass: root of the bar's chord on strong beats, fifth as a pickup.
            if (i % 4 == 0)
                st.bass = scaleNote(chord, -1);
            else if (i % 4 == 2 && rng.chance(60))
                st.bass = scaleNote(chord + 4, -1);

            // Arpeggio: cycle chord tones (1-3-5) every step.
            int tone = i % 3;
            st.arpChord = scaleNote(chord + tone * 2, 0);

            // Percussion: kick on the downbeats, hats/snare by density.
            bool kick = (i % 8 == 0);
            bool snare = (i % 8 == 4) && mood.noiseDensity >= 2;
            bool hat = (i % 2 == 0) && mood.noiseDensity >= 3 && rng.chance(70);
            st.noise = kick || snare || hat;
        }
    }
}

void ChiptuneSynth::Track::advanceStep() {
    const double stepSeconds = 60.0 / mood.bpm / 4.0;
    const auto& st = steps[stepIndex];

    if (st.melody >= 0)
        voices[0].noteOn(midiToFreq(st.melody), stepSeconds * (st.accent ? 1.8 : 0.9));
    if (st.bass >= 0)
        voices[1].noteOn(midiToFreq(st.bass), stepSeconds * 0.95);
    if (st.arpChord >= 0)
        voices[2].noteOn(midiToFreq(st.arpChord), stepSeconds * 0.5);
    if (st.noise) {
        bool onBeat = (stepIndex % 8 == 0);
        voices[3].noteOn(onBeat ? 900.0 : 5500.0, stepSeconds * 0.3);
    }

    stepIndex = (stepIndex + 1) % static_cast<int>(steps.size());
}

float ChiptuneSynth::Track::tick(double dt) {
    const double stepSeconds = 60.0 / mood.bpm / 4.0;
    stepTimer += dt;
    if (stepTimer >= stepSeconds) {
        stepTimer -= stepSeconds;
        advanceStep();
    }
    float mix = 0;
    for (auto& v : voices)
        mix += v.sample(dt);
    // Gentle soft clip keeps chord stacks from cracking.
    return std::tanh(mix * 1.2f) * mood.brightness;
}

// ---------------------------------------------------------------- Synth ----

void ChiptuneSynth::setMood(const std::string& name) {
    std::lock_guard lock(pendingMutex_);
    pendingMood_ = name;
    hasPending_ = true;
}

void ChiptuneSynth::render(int16_t* out, int frames) {
    {
        std::lock_guard lock(pendingMutex_);
        if (hasPending_) {
            const Mood& m = moodByName(pendingMood_);
            hasPending_ = false;
            bool alreadyPlaying = current_->active && current_->mood.name == m.name && !next_;
            if (!alreadyPlaying) {
                if (!current_->active) {
                    current_->start(m);  // first mood: no fade, just play
                } else {
                    Track* other = (current_ == &a_) ? &b_ : &a_;
                    other->start(m);
                    next_ = other;
                    fade_ = 0;
                }
            }
        }
    }

    const double dt = 1.0 / kSampleRate;
    for (int i = 0; i < frames; ++i) {
        float s = current_->active ? current_->tick(dt) : 0.0f;
        if (next_) {
            fade_ = std::min(1.0, fade_ + dt / kCrossfadeSeconds);
            float t = static_cast<float>(fade_);
            // Equal-power crossfade.
            s = s * std::cos(t * static_cast<float>(M_PI_2))
              + next_->tick(dt) * std::sin(t * static_cast<float>(M_PI_2));
            if (fade_ >= 1.0) {
                current_->active = false;
                current_ = next_;
                next_ = nullptr;
            }
        }
        int v = static_cast<int>(s * 22000.0f);
        out[i] = static_cast<int16_t>(std::clamp(v, -32768, 32767));
    }
}
