#include "engine.cpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace acidxy;
static void require(bool ok, const char *why) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", why);
        std::exit(1);
    }
}
int main() {
    Voice v;
    Shared shared;
    i32 audio[64];
    initialize(v, shared, 1);
    for (auto &x : audio)
        x = audio_one / 2;
    level_output(v, audio);
    for (auto x : audio)
        require(x == 0, "exact one-block lookahead latency");
    for (auto &x : audio)
        x = audio_one / 2;
    level_output(v, audio);
    for (auto x : audio)
        require(x == mul(audio_one / 2, maximum_output_gain) * 256,
                "unlimited path restores original web gain");
    // Sudden high peak must be anticipated in the preceding output block.
    for (auto &x : audio)
        x = 4 * audio_one;
    level_output(v, audio);
    require(v.output_gain < maximum_output_gain, "peak reduces gain ahead of signal");
    for (auto &x : audio)
        x = 4 * audio_one;
    level_output(v, audio);
    for (auto x : audio)
        require(magnitude(x) <= u32((level_ceiling + 2) * 256), "no attack overshoot");
    // Periodic waveform: once gain settles, protection is a linear scale,
    // including when the unattenuated waveform would greatly exceed the ceiling.
    initialize(v, shared, 1);
    i32 gain = 0;
    for (unsigned block = 0; block < 200; ++block) {
        for (unsigned i = 0; i < 64; ++i)
            audio[i] = i32(std::sin((block * 64 + i) * 6.283185307179586 / 256) * audio_one * 4);
        level_output(v, audio);
        for (auto x : audio)
            require(magnitude(x) <= u32((level_ceiling + 2) * 256), "bounded periodic output");
        if (block == 100)
            gain = v.output_gain;
        if (block > 100) {
            require(gain == v.output_gain, "no gain ripple on steady periodic input");
            for (unsigned i = 0; i < 64; ++i) {
                const i32 raw =
                    i32(std::sin(((block - 1) * 64 + i) * 6.283185307179586 / 256) * audio_one * 4);
                const double ideal=double(raw)*double(gain)/8388608.;
                require(std::abs(double(audio[i])-ideal)<=65536.,
                        "steady output scale error stays below one 15-bit output step");
            }
        }
    }
    // Arbitrary full-range inputs and alternating blocks exercise the lookahead
    // proof independently of oscillator/filter behavior, without a final clamp.
    u32 seed = 714;
    for (unsigned block = 0; block < 20000; ++block) {
        for (unsigned i = 0; i < 64; ++i) {
            seed = seed * 1664525u + 1013904223u;
            audio[i] = block % 7 == 0       ? 0
                       : seed < 0x80000000u ? i32(seed)
                                            : -1 - i32(0xffffffffu - seed);
        }
        level_output(v, audio);
        for (auto x : audio)
            require(magnitude(x) <= u32((level_ceiling + 2) * 256),
                    "no clipping/overflow at any input level");
        require(v.output_gain >= 0 && v.output_gain <= maximum_output_gain,
                "gain has fixed maximum");
    }
    i32 prior = v.output_gain;
    for (unsigned block = 0; block < 3000; ++block) {
        for (auto &x : audio)
            x = 0;
        level_output(v, audio);
        if (block > 1)
            require(v.output_gain >= prior, "monotonic release to original level");
        prior = v.output_gain;
    }
    require(v.output_gain == maximum_output_gain, "full gain returns after peak passes");
    initialize(v, shared, 1);
    for (auto x : v.pending)
        require(x == 0, "voice reuse clears delayed audio");
    std::puts("PASS acid level: original gain, fixed latency, anticipated peaks, clean steady "
              "waveform, full-range stress, hold/release, reuse");
}

