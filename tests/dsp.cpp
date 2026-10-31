#include "engine.cpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace acidxy;
static void require(bool ok, const char *why) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", why);
        std::exit(1);
    }
}
static i32 q(double x) { return x >= 1 ? one : i32(std::round(x * 2147483647.)); }
static double correction(double phase, double dt) {
    if (!dt)
        return 0;
    if (phase < dt) {
        double t = phase / dt;
        return t + t - t * t - 1;
    }
    if (phase > 1 - dt) {
        double t = (phase - 1) / dt;
        return t * t + t + t + 1;
    }
    return 0;
}
// Full zero-stuffed implementation retained only as an independent regression
// oracle for the polyphase path, including every tap and Q31 truncation.
static i32 naive_drive(Voice &v, i32 sample, i32 amount) {
    i32 result = 0;
    for (unsigned j = 0; j < 4; ++j) {
        const i32 up = fir(v.up, j == 0 ? add(add(sample, sample), add(sample, sample)) : 0);
        const i32 boost = mul(up, amount);
        const i32 doubled = add(boost, boost), four = add(doubled, doubled);
        result = fir(v.down, clamp(add(up, add(four, four)), audio_one));
    }
    return result;
}
int main() {
    u32 seed = 67;
    for (unsigned j = 0; j < 100000; ++j) {
        seed = seed * 1664525u + 1013904223u;
        u32 phase = seed;
        seed = seed * 1664525u + 1013904223u;
        u32 dt = seed % max_increment + 1;
        const double p = double(phase) / 4294967296., step = double(dt) / 4294967296.;
        const double b = correction(p, step), saw = p * 2 - 1 - b;
        const double square = (p > .5 ? 1 : -1) + correction(std::fmod(p + .5, 1.), step) - b;
        require(std::abs(double(oscillator(phase, dt, 0)) / 2147483648. - saw) < 6e-5,
                "pBLEP saw reference");
        require(std::abs(double(oscillator(phase, dt, one)) / 2147483648. - square) < 6e-5,
                "pBLEP square reference");
    }
    for (unsigned j = 0; j <= 1000; ++j) {
        const double amount = j / 1000.;
        require(std::abs(double(tune_ratio(q(amount))) / 536870912. -
                         std::pow(2., -2 + 4 * amount)) < 2e-6,
                "continuous tune ratios");
        const double ideal = std::exp(-.004 * (1 - amount));
        require(std::abs(double(normalized_table(tables::envelope_keep, q(amount))) / 2147483648. -
                         ideal) < 1e-8,
                "web decay mapping");
        const double k = std::pow(2., -7 + 7 * amount);
        require(std::abs(double(coefficient(q(amount), 0, one)) / 2147483648. - k) < 3e-6,
                "seven-octave filter coefficient");
        for (int sample = -150; sample <= 150; sample += 3) {
            const double x = sample / 100.;
            const double expected =
                amount ? std::tanh(x * (1 + 5 * amount)) / (1 + 1.5 * amount) : x;
            const double actual = double(saturation(i32(x * audio_one), q(amount))) / audio_one;
            require(std::abs(expected - actual) < 1.6e-4, "bounded fast saturation vs analytic tanh");
        }
    }
    require(normalized_table(tables::envelope_keep, one) == one, "infinite decay endpoint");
    Voice naive, optimized;
    Shared drive_shared;
    initialize(naive, drive_shared, 1);
    initialize(optimized, drive_shared, 2);
    for (unsigned i = 0; drive_oversampling==4 && i < 100000; ++i) {
        seed = seed * 1664525u + 1013904223u;
        const i32 sample = seed < 0x80000000u ? i32(seed) : -1 - i32(0xffffffffu - seed);
        seed = seed * 1664525u + 1013904223u;
        const i32 amount = i32(seed >> 1);
        require(naive_drive(naive, sample, amount) == drive_stage(optimized, sample, amount),
                "polyphase drive bit-exact");
        require(!std::memcmp(naive.up, optimized.up, sizeof(naive.up)) &&
                    !std::memcmp(naive.down, optimized.down, sizeof(naive.down)),
                "polyphase FIR histories bit-exact");
    }
    // Independent double FIR validates coefficient order, phase delay and gain.
    Shared shared;
    Voice v;
    initialize(v, shared, 1);
    double up[7] = {}, down[7] = {};
    const double kernel[7] = {.013475, .097598, .235621, .306612, .235621, .097598, .013475};
    double error = 0, energy = 0;
    for (unsigned i = 0; drive_oversampling==4 && i < 16000; ++i) {
        const double x = .6 * std::sin(i * .13) + .8 * std::sin(i * .71);
        const double amount = .63;
        double expected = 0;
        for (unsigned phase = 0; phase < 4; ++phase) {
            for (unsigned j = 6; j > 0; --j)
                up[j] = up[j - 1];
            up[0] = phase == 0 ? x * 4 : 0;
            double interpolated = 0;
            for (unsigned j = 0; j < 7; ++j)
                interpolated += up[j] * kernel[j];
            const double clipped = std::fmax(-1., std::fmin(1., interpolated * (1 + 8 * amount)));
            for (unsigned j = 6; j > 0; --j)
                down[j] = down[j - 1];
            down[0] = clipped;
            expected = 0;
            for (unsigned j = 0; j < 7; ++j)
                expected += down[j] * kernel[j];
        }
        const double actual = double(drive_stage(v, i32(x * audio_one), q(amount))) / audio_one;
        error += (expected - actual) * (expected - actual);
        energy += expected * expected;
    }
    if(drive_oversampling==4)
        require(10 * std::log10(error / energy) < -85, "bounded coefficient FIR drive reference");
    else {
        require(drive_oversampling==1,"explicit fast drive mode");
        for(unsigned i=0;i<16000;++i){
            const double sample=1.5*std::sin(i*.13), amount=(i%1000)/999.;
            const double expected=std::fmax(-1.,std::fmin(1.,sample*(1+8*amount)));
            const double actual=double(drive_clip_prepared(i32(sample*audio_one),(q(amount)>>7)*8+16777216))/audio_one;
            require(std::abs(expected-actual)<.0003,"1x drive bounded error against independent hard clip");
        }
    }
    // Recursive filter impulse response settles rather than latching to a rail.
    for (unsigned level = 0; level <= 10; ++level) {
        initialize(v, shared, 1);
        const i32 kf = coefficient(q(level / 10.), 0, 0);
        for (unsigned i = 0; i < 441000; ++i)
            filter(v, i == 0 ? audio_one / 4 : 0, kf, q(.1));
        require(std::abs(v.low[0]) < 5000 && std::abs(v.low[1]) < 5000,
                "filter impulse tail settles");
    }
    // Tune the maximum-resonance filter to the fundamental, not merely to
    // arbitrary knob corners. These notes used to hit the Q6.26 state rails.
    double largest = 0;
    for (unsigned note = 45; note <= 117; note += 12) {
        const double hz = 440 * std::pow(2., (int(note) - 69) / 12.);
        const double target = 2 * std::sin(3.141592653589793 * hz / (2 * 44100));
        const double cutoff = (std::log(target) / std::log(2.) + 7) / 7;
        opxy::u32 pitches[64];
        for (auto &x : pitches)
            x = opxy::u32(hz / 44100 * 4294967296.);
        i32 params[8] = {0, q(cutoff), one, 0, 0, 0, 0, q(.5)}, output[64];
        for (unsigned wave = 0; wave < 2; ++wave) {
            params[6] = wave ? one : 0;
            initialize(v, shared, 1);
            start(v, shared, 1);
            for (unsigned block = 0; block < 500; ++block) {
                render_core(v, shared, 1, output, params, pitches);
                for (auto x : output)
                    require(x != 1073741823 && x != -1073741824,
                            "no internal filter clipping at resonant fundamentals");
                for (unsigned j = 0; j < 2; ++j) {
                    require(v.low[j] != one && v.low[j] != (-2147483647 - 1) && v.band[j] != one &&
                                v.band[j] != (-2147483647 - 1),
                            "resonant state headroom");
                    const double physical = std::abs(double(v.low[j])) / audio_one;
                    if (physical > largest)
                        largest = physical;
                }
            }
        }
    }
    require(largest > 32, "resonance fixture exceeds the former numeric range");
    std::puts(
        "PASS acid DSP: analytic oscillator, tune, decay, filter, saturation, independent "
        "FIR, bit-exact polyphase and histories, impulse tails, resonant fundamental headroom");
}

