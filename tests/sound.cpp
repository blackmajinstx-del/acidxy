#define _CRT_SECURE_NO_WARNINGS
#include "engine.cpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace acidxy;
static void require(bool ok, const char *message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
static void packet_render(const char *input, const char *output, bool raw) {
    FILE *in = std::fopen(input, "rb");
    FILE *out = std::fopen(output, "wb");
    require(in && out, "open fixture files");
    Voice v = {};
    Shared shared;
    initialize(v, shared, 1);
    i32 c[8], samples[64];
    u32 pitch[64], event;
    while (std::fread(&event, 4, 1, in) == 1) {
        require(std::fread(c, 4, 8, in) == 8 && std::fread(pitch, 4, 64, in) == 64,
                "complete packet");
        if (event & 1)
            initialize(v, shared, 1);
        if (event & 2)
            start(v, shared, 1);
        if (raw) {
            render_core(v, shared, 1, samples, c, pitch);
            for (auto &x : samples)
                x = mul(x, 1503238554); // core-only reference gain: web / 256
        } else
            render(v, shared, 1, samples, c, pitch);
        require(std::fwrite(samples, 4, 64, out) == 64, "write audio");
    }
    require(!std::ferror(in), "read packets");
    std::fclose(in);
    std::fclose(out);
}
int main(int argc, char **argv) {
    if (argc == 3 || argc == 4) {
        packet_render(argv[1], argv[2], argc == 4 && !std::strcmp(argv[3], "raw"));
        return 0;
    }
    Shared shared;
    Voice v = {}, other = {};
    i32 out[64], copy[64];
    u32 pitch[64];
    i32 c[8] = {1073741824, 601295421, 1288490188, 0, 0, 0, 0, 0};
    for (unsigned j = 0; j < 64; ++j)
        pitch[j] = 6384029;
    initialize(v, shared, 1);
    start(v, shared, 1);
    double energy = 0;
    for (unsigned b = 0; b < 700; ++b) {
        render(v, shared, 1, out, c, pitch);
        for (auto x : out)
            energy += double(x) * x;
    }
    require(energy > 1e12, "audible sustained oscillator");
    require(sizeof(Voice) == 408, "voice size including reciprocal cache");
    require(tuned_increment(6384029, tune_ratio(1073741824)) == 6384029, "neutral tuning exact");
    require(std::abs(double(tuned_increment(6384029, tune_ratio(0))) / 6384029 - .25) < 1e-6,
            "-24 semitones");
    require(std::abs(double(tuned_increment(6384029, tune_ratio(one))) / 6384029 - 4) < 1e-6,
            "+24 semitones");
    // 256 owners: interleaving a second independent voice must not affect output.
    for (u32 owner = 1; owner <= 256; ++owner) {
        Voice reference;
        initialize(v, shared, owner);
        start(v, shared, owner);
        initialize(reference, shared, owner);
        start(reference, shared, owner);
        initialize(other, shared, owner == 256 ? 1 : owner + 1);
        start(other, shared, 2);
        for (unsigned b = 0; b < 4; ++b) {
            render(v, shared, owner, out, c, pitch);
            render(other, shared, 2, copy, c, pitch);
            render(reference, shared, owner, copy, c, pitch);
            require(!std::memcmp(out, copy, sizeof(out)), "voice isolation");
        }
        const u32 phase = v.phase;
        start(v, shared, owner);
        require(v.phase == phase && v.envelope == one,
                "retrigger preserves phase, restarts filter envelope");
        destroy(v, shared, owner);
        initialize(v, shared, owner);
        require(v.phase == 0 && v.ready == 0 && v.low[0] == 0 && v.down[0] == 0,
                "reuse clears state");
    }
    // Every control must affect a patch which exercises envelope and both nonlinear stages.
    i32 base[8] = {1073741824, 858993459, 1073741824, 858993459,
                   1288490188, 429496729, 429496729,  429496729};
    for (unsigned control = 0; control < 8; ++control) {
        Voice a, b;
        initialize(a, shared, 1);
        initialize(b, shared, 2);
        start(a, shared, 1);
        start(b, shared, 2);
        i32 variant[8];
        std::memcpy(variant, base, sizeof(base));
        variant[control] = one;
        bool differs = false;
        for (unsigned j = 0; j < 80; ++j) {
            render(a, shared, 1, out, base, pitch);
            render(b, shared, 2, copy, variant, pitch);
            differs |= std::memcmp(out, copy, sizeof(out)) != 0;
        }
        require(differs, "each control changes sound");
    }
    u32 seed = 71;
    unsigned clips = 0;
    for (unsigned mode = 0; mode < 100; ++mode) {
        for (unsigned j = 0; j < 8; ++j) {
            seed = seed * 1664525u + 1013904223u;
            c[j] = mode < 2 ? (mode ? one : 0) : i32(seed >> 1);
        }
        initialize(v, shared, 1);
        start(v, shared, 1);
        for (unsigned b = 0; b < 512; ++b) {
            if (b == 256)
                for (auto &x : c)
                    x = one - x;
            for (unsigned j = 0; j < 64; ++j)
                pitch[j] = mode < 2 ? 6384029
                                    : u32(180000 + (b * 137 + j * 71 + mode * 99431) % 1000000000);
            render(v, shared, 1, out, c, pitch);
            for (auto x : out) {
                require(x >= -1073741824 && x <= 1073741824, "declared peak");
                clips += x == 1073741824 || x == -1073741824;
            }
            for (unsigned j = 0; j < 2; ++j)
                require(v.low[j] != one && v.low[j] != (-2147483647 - 1) && v.band[j] != one &&
                            v.band[j] != (-2147483647 - 1),
                        "filter headroom");
        }
    }
    require(clips == 0, "test corpus does not depend on final limiter");
    // High resonance, lowest cutoff, a ten-second hold must retain finite periodic audio.
    c[0] = 1073741824;
    c[1] = 0;
    c[2] = one;
    c[3] = 0;
    c[4] = one;
    c[5] = 0;
    c[6] = one;
    c[7] = 0;
    initialize(v, shared, 1);
    start(v, shared, 1);
    for (auto &x : pitch)
        x = 6384029;
    for (unsigned b = 0; b < 7000; ++b)
        render(v, shared, 1, out, c, pitch);
    require(v.low[0] != one && v.low[1] != one, "long-run filter stability");
    std::puts("PASS acid: tuning, all controls, 256 owners, isolation, retrigger, reuse, extremes, "
              "moving pitch/controls, headroom, 10s hold");
}

