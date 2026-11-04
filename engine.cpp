// DT303-derived DSP: srtuss and ACID-XY contributors; see LICENSE and README.md.
#include "core.h"
#include "tables.h"
#include <opxy/q31.hpp>
namespace acidxy {
using opxy::i32;
using opxy::u32;
const i32 one = 2147483647;
const i32 audio_one = 8388608;              // internal Q9.23: +/-256, NOT host Q1.31
const u32 max_increment = 2104533975u;      // 0.49 cycles/sample
const i32 maximum_output_gain = 1503238554; // web's original .7
const i32 level_ceiling = 3984589;          // .475 host full scale, below .5 ABI limit
const u32 level_hold_blocks = 70;           // 101.6 ms at 44.1 kHz / 64

inline i32 add(i32 a, i32 b) {
    i32 result;
    if (__builtin_add_overflow(a, b, &result))
        return a < 0 ? (-2147483647 - 1) : one;
    return result;
}
inline i32 subtract(i32 a, i32 b) {
    i32 result;
    if (__builtin_sub_overflow(a, b, &result))
        return a < 0 ? (-2147483647 - 1) : one;
    return result;
}
inline i32 mul(i32 a, i32 b) {
    if (b < 0) return opxy::q31::multiply(a,b);
    const u32 coefficient=u32(b)>>15;
    const i32 high=a>>16;
    const u32 low=u32(a)&65535u;
    return high*i32(coefficient)+i32((low*coefficient)>>16);
}
inline i32 clamp(i32 x, i32 limit) { return x < -limit ? -limit : x > limit ? limit : x; }
inline i32 interpolate(i32 first, i32 next, u32 fraction) {
    const i32 delta = next - first;
    return first + opxy::q31::multiply(delta, i32(fraction << 15));
}
inline i32 interpolate(const i32 *table, u32 index, u32 fraction) {
    return interpolate(table[index], table[index + 1], fraction);
}
inline i32 exp2_value(u32 index) {
    const u32 at = index * 3u;
    const u32 value = tables::exp2_filter[at] |
                      (u32(tables::exp2_filter[at + 1]) << 8) |
                      (u32(tables::exp2_filter[at + 2]) << 16);
    return i32(value << 7);
}
inline i32 lookup_exp2(u32 position) {
    const u32 index = position >> 16;
    return index >= 1440u ? exp2_value(1440u)
                          : interpolate(exp2_value(index), exp2_value(index + 1), position & 65535u);
}
// position in Q16, endpoints included; last is the final table index.
inline i32 lookup(const i32 *table, u32 position, u32 last) {
    const u32 index = position >> 16;
    return index >= last ? table[last] : interpolate(table, index, position & 65535u);
}
inline i32 normalized_table(const i32 *table, i32 amount, u32 last) {
    const u32 value = u32(amount);
    const u32 position = (value >> 15) * last + (((value & 32767u) * last) >> 15);
    return amount == one ? table[last] : lookup(table, position, last);
}
inline i32 tune_ratio(i32 amount) {
    const u32 a = u32(amount);
    const u32 position = a <= 1610612735u
        ? (128u << 16) + (a / 128u) * 5u + ((a % 128u) * 5u) / 128u
        : (1088u << 16) + ((a - 1610612735u) / 256u) * 11u +
          (((a - 1610612735u) % 256u) * 11u) / 256u;
    return lookup_exp2(position); // Q3.29
}
// Unsigned full precision 32x32 product, split into 16-bit limbs; no 64-bit
// runtime arithmetic or compiler division helpers in contributor audio code.
struct PitchStep {
    u32 whole, fractional;
};
inline PitchStep prepare_pitch(u32 increment, i32 ratio) {
    const u32 a0 = increment & 65535u, a1 = increment >> 16;
    const u32 b0 = u32(ratio) & 65535u, b1 = u32(ratio) >> 16;
    const u32 p0 = a0 * b0, p1 = a0 * b1, p2 = a1 * b0;
    u32 high = a1 * b1, low = p0;
    u32 old = low;
    low += p1 << 16;
    high += (p1 >> 16) + (low < old);
    old = low;
    low += p2 << 16;
    high += (p2 >> 16) + (low < old);
    if (high >= (max_increment >> 3))
        return PitchStep{max_increment, 0};
    return PitchStep{(high << 3) | (low >> 29), low & 0x1fffffffu};
}
inline u32 advance_pitch(const PitchStep &step, u32 &residual) {
    const u32 sum = residual + step.fractional;
    residual = sum & 0x1fffffffu;
    return step.whole + (sum >> 29);
}
inline u32 tuned_increment(u32 increment, i32 ratio, u32 *residual = 0) {
    const PitchStep step = prepare_pitch(increment, ratio);
    return residual ? advance_pitch(step, *residual) : step.whole;
}

inline i32 fraction(u32 n, u32 d) {
    if (!d)
        return 0;
    u32 f = 0;
    for (unsigned j = 0; j < 24; ++j) {
        n <<= 1;
        f <<= 1;
        if (n >= d) {
            n -= d;
            f |= 1;
        }
    }
    return i32(f << 7);
}
inline i32 blep(u32 phase, u32 dt) {
    if (!dt)
        return 0;
    if (phase < dt) {
        const i32 x = one - fraction(phase, dt);
        return -mul(x, x);
    }
    if (phase > 0u - dt) {
        const i32 x = one - fraction(0u - phase, dt);
        return mul(x, x);
    }
    return 0;
}
inline i32 oscillator(u32 phase, u32 dt, i32 wave) {
    const i32 edge = blep(phase, dt);
    const i32 raw = phase < 0x80000000u ? i32(phase) - 2147483647 - 1 : i32(phase - 0x80000000u);
    const i32 saw = subtract(raw, edge);
    if (!wave)
        return saw;
    const i32 square =
        subtract(add(phase > 0x80000000u ? one : -one, blep(phase - 0x80000000u, dt)), edge);
    return add(saw,mul(square-saw,wave));
}
struct BlepReciprocal { i32 value; u32 shift; };
inline BlepReciprocal prepare_blep(u32 dt) {
    if (!dt) return BlepReciprocal{0,0};
    u32 shift=0;
    while (dt < 0x40000000u) { dt <<= 1; ++shift; }
    return BlepReciprocal{dt == 0x40000000u ? one : fraction(0x40000000u,dt),shift};
}
inline BlepReciprocal cached_blep(Voice &v,u32 dt) {
    if(v.blep_increment!=dt){
        const BlepReciprocal result=prepare_blep(dt);
        v.blep_increment=dt;v.blep_shift=result.shift;v.blep_reciprocal=result.value;
    }
    return BlepReciprocal{v.blep_reciprocal,v.blep_shift};
}
inline i32 blep_prepared(u32 phase,u32 dt,const BlepReciprocal &reciprocal) {
    if (!dt) return 0;
    if (phase < dt) {
        const i32 x=one-mul(i32(phase << reciprocal.shift),reciprocal.value)*2;
        return -mul(x,x);
    }
    if (phase > 0u-dt) {
        const i32 x=one-mul(i32((0u-phase) << reciprocal.shift),reciprocal.value)*2;
        return mul(x,x);
    }
    return 0;
}
inline i32 oscillator_prepared(u32 phase,u32 dt,i32 wave,const BlepReciprocal &reciprocal) {
    const i32 edge=blep_prepared(phase,dt,reciprocal);
    const i32 raw=phase < 0x80000000u ? i32(phase)-2147483647-1 : i32(phase-0x80000000u);
    const i32 saw=subtract(raw,edge);
    if (!wave) return saw;
    const i32 square=subtract(add(phase>0x80000000u ? one : -one,
        blep_prepared(phase-0x80000000u,dt,reciprocal)),edge);
    return add(saw,mul(square-saw,wave));
}
inline i32 coefficient(i32 cutoff, i32 depth, i32 envelope) {
    // exponent = min(0, 7*cutoff - 7 + 5*envelope*depth), Q8.24.
    i32 exponent = (cutoff / 128) * 7 - 117440512 + (mul(envelope, depth) / 128) * 5;
    if (exponent >= 0)
        return one;
    if (exponent < -117440512)
        exponent = -117440512;
    const u32 amount = u32(exponent + 117440512);
    const u32 position = amount <= 50331648u ? amount / 6u
        : amount <= 100663296u
            ? (128u << 16) + ((amount - 50331648u) * 5u) / 4u
            : (1088u << 16) + ((amount - 100663296u) * 11u) / 8u;
    return lookup_exp2(position);
}
inline i32 section(i32 input, i32 kf, i32 kq, i32 &low, i32 &band) {
    const i32 lp = add(low, mul(band, kf));
    const i32 hp = subtract(subtract(input, lp), mul(band, kq));
    band = add(band, mul(hp, kf));
    low = lp;
    return lp;
}
inline i32 filter(Voice &v, i32 input, i32 kf, i32 kq) {
    const i32 first = section(input, kf, kq, v.low[0], v.band[0]);
    return section(first, kf, kq, v.low[1], v.band[1]);
}
inline i32 filter_multiply(i32 sample,i32 coefficient) {
    if(u32(sample)+0x01000000u<0x02000000u)
        return ((sample>>8)*(coefficient>>16))>>7;
    return mul(sample,coefficient);
}
inline i32 section_bounded(i32 input,i32 kf,i32 kq,i32 &low,i32 &band) {
    const i32 lp=low+filter_multiply(band,kf);
    const i32 hp=input-lp-filter_multiply(band,kq);
    band+=filter_multiply(hp,kf); low=lp;
    return lp;
}
inline i32 filter_bounded(Voice &v,i32 input,i32 kf,i32 kq) {
    return section_bounded(section_bounded(input,kf,kq,v.low[0],v.band[0]),
        kf,kq,v.low[1],v.band[1]);
}
inline i32 fir(i32 (&history)[7], i32 sample) {
    for (unsigned j = 6; j > 0; --j)
        history[j] = history[j - 1];
    history[0] = sample;
    i32 result = 0;
    for (unsigned j = 0; j < 7; ++j)
        result = add(result, mul(history[j], tables::drive_kernel[j]));
    return result;
}
// Exact polyphase evaluation of the original zero-stuffed interpolation FIR.
// After each input's four phases, up[3] is the sole nonzero history sample.
// Decimation only emits the fourth convolution; retain its seven taps in the
// original order so both arithmetic rounding and persistent state stay exact.
// Accepted fast014 release uses 1x Drive; the historical 4x/2x routines remain
// for comparison. This is not an exposed user parameter.
const unsigned drive_oversampling = 1;
inline i32 drive_clip(i32 sample, i32 amount) {
    if (sample >= audio_one) return audio_one;
    if (sample <= -audio_one) return -audio_one;
    return clamp(sample + mul(sample, amount) * 8, audio_one);
}
inline i32 drive_stage(Voice &v, i32 sample, i32 amount) {
    if (drive_oversampling == 1)
        return drive_clip(sample, amount);
    if (drive_oversampling == 2) {
        // Seven-tap half-band FIR: [-1,0,9,16,9,0,-1]/32.
        // Zero taps and polyphase evaluation avoid four full convolutions.
        const i32 k0 = -67108864, k2 = 603979776, k3 = 1073741824;
        const i32 input = add(sample, sample);
        const i32 even = add(add(mul(input, k0), mul(v.up[0], k2)),
                             add(mul(v.up[1], k2), mul(v.up[2], k0)));
        const i32 odd = mul(v.up[0], k3);
        v.up[2] = v.up[1]; v.up[1] = v.up[0]; v.up[0] = input;
        v.down[6] = v.down[4]; v.down[5] = v.down[3];
        v.down[4] = v.down[2]; v.down[3] = v.down[1];
        v.down[2] = v.down[0];
        v.down[1] = drive_clip(even, amount);
        v.down[0] = drive_clip(odd, amount);
        return add(add(mul(v.down[0], k0), mul(v.down[2], k2)),
                   add(mul(v.down[3], k3), add(mul(v.down[4], k2), mul(v.down[6], k0))));
    }
    const i32 input = add(add(sample, sample), add(sample, sample));
    const i32 previous = v.up[3];
    i32 phases[4];
    phases[0] = mul(input, tables::drive_kernel[0]) + mul(previous, tables::drive_kernel[4]);
    phases[1] = mul(input, tables::drive_kernel[1]) + mul(previous, tables::drive_kernel[5]);
    phases[2] = mul(input, tables::drive_kernel[2]) + mul(previous, tables::drive_kernel[6]);
    phases[3] = mul(input, tables::drive_kernel[3]);
    v.up[3] = input;
    // The interpolation phase weights sum to less than 0.307, even if
    // input/previous are at the Q9.23 rails: their additions cannot overflow.
    for (unsigned j = 0; j < 4; ++j) {
        const i32 up = phases[j];
        if (up >= audio_one)
            phases[j] = audio_one;
        else if (up <= -audio_one)
            phases[j] = -audio_one;
        else {
            // Here |up| < 1, so |up + 8*up*amount| < 9 in Q9.23.
            // Preserve the original multiply truncation before the x8 gain.
            phases[j] = clamp(up + mul(up, amount) * 8, audio_one);
        }
    }
    v.down[6] = v.down[2];
    v.down[5] = v.down[1];
    v.down[4] = v.down[0];
    v.down[3] = phases[0];
    v.down[2] = phases[1];
    v.down[1] = phases[2];
    v.down[0] = phases[3];
    i32 result = 0;
    for (unsigned j = 0; j < 7; ++j)
        // Downsample inputs are clipped to +/-1; unit-sum positive taps
        // leave ample headroom for every partial sum.
        result += mul(v.down[j], tables::drive_kernel[j]);
    return result;
}
inline i32 saturation_prepared(i32 sample, i32 amount, i32 compensation) {
    if (!amount)
        return sample;
    const i32 a = mul(sample, amount);
    i32 x = sample;
    if (sample >= -0x10000000 && sample <= 0x10000000) {
        // |sample| <= 32 implies |sample + 5*a| <= 192, inside Q9.23.
        x = sample + a * 5;
    } else {
        for (unsigned j = 0; j < 5; ++j)
            x = add(x, a);
    }
    // tanh table extends to 16; beyond that its Q23 result is exactly 1.
    const u32 magnitude = x < 0 ? 0u - u32(x) : u32(x);
    const i32 curved =
        magnitude >= 8u * audio_one ? audio_one : lookup(tables::tanh_fast, magnitude >> 2, 256);
    const i32 result = mul(curved, compensation);
    return x < 0 ? -result : result;
}
inline void drive_block(Voice &v,opxy::Output output,i32 amount) {
    i32 input[64],phases[4][64];
    for (unsigned i=0;i<64;++i)
        input[i]=add(add(output[i],output[i]),add(output[i],output[i]));
    for (unsigned phase=0;phase<4;++phase) {
        const i32 a=tables::drive_kernel[phase];
        const i32 b=phase<3 ? tables::drive_kernel[phase+4] : 0;
        for (unsigned i=0;i<64;++i) {
            const i32 previous=i ? input[i-1] : v.up[3];
            const i32 x=mul(input[i],a)+mul(previous,b);
            phases[phase][i]=drive_clip(x,amount);
        }
    }
    for (unsigned i=0;i<64;++i) {
        i32 result=mul(phases[3][i],tables::drive_kernel[0]);
        result+=mul(phases[2][i],tables::drive_kernel[1]);
        result+=mul(phases[1][i],tables::drive_kernel[2]);
        result+=mul(phases[0][i],tables::drive_kernel[3]);
        result+=mul(i ? phases[3][i-1] : v.down[0],tables::drive_kernel[4]);
        result+=mul(i ? phases[2][i-1] : v.down[1],tables::drive_kernel[5]);
        result+=mul(i ? phases[1][i-1] : v.down[2],tables::drive_kernel[6]);
        output[i]=result;
    }
    v.up[3]=input[63];
    v.down[0]=phases[3][63];v.down[1]=phases[2][63];v.down[2]=phases[1][63];v.down[3]=phases[0][63];
    v.down[4]=phases[3][62];v.down[5]=phases[2][62];v.down[6]=phases[1][62];
}
inline i32 saturation(i32 sample, i32 amount) {
    return saturation_prepared(sample, amount, normalized_table(tables::sat_compensation, amount, 128));
}
// TD-3-MO-style oscillator overdrive before the filter. Unlike the old
// post-filter saturation this deliberately changes how resonance responds.
inline i32 overdrive(i32 sample, i32 amount) {
    if (!amount)
        return sample;
    const i32 a = mul(sample, amount);
    i32 x = sample;
    // The oscillator is divided by 256 before this stage, so its production
    // input fits this exact fast path. Preserve saturation for wider callers.
    if (sample >= -0x07ffffff && sample <= 0x07ffffff)
        x = sample + a * 15;
    else
        for (unsigned j = 0; j < 15; ++j)
            x = add(x, a);
    const u32 magnitude = x < 0 ? 0u - u32(x) : u32(x);
    const i32 curved =
        magnitude >= 8u * audio_one ? audio_one : lookup(tables::tanh_fast, magnitude >> 2, 256);
    return x < 0 ? -curved : curved;
}
inline i32 boost_prepared(i32 sample,i32 gain_q24) {
    // |sample| <= 1 in Q23, gain <= 16: signed product remains in i32.
    return ((sample >> 8)*(gain_q24 >> 12))/16;
}
inline i32 overdrive_prepared(i32 sample,i32 gain_q24) {
    const i32 x=boost_prepared(sample,gain_q24);
    const u32 magnitude=x<0 ? 0u-u32(x) : u32(x);
    const i32 curved=magnitude>=8u*audio_one ? audio_one : lookup(tables::tanh_fast, magnitude >> 2, 256);
    return x<0 ? -curved : curved;
}
inline i32 drive_clip_prepared(i32 sample,i32 gain_q24) {
    if(sample>=audio_one) return audio_one;
    if(sample<=-audio_one) return -audio_one;
    return clamp(boost_prepared(sample,gain_q24),audio_one);
}

inline i32 wave_mix(i32 control) {
    // Physical control caption is -50..+50. Left half morphs square -> saw;
    // right half keeps saw and increases distortion.
    const i32 middle = 1073741824;
    return control < middle ? one - control * 2 : 0;
}
inline i32 wave_distortion(i32 control) {
    const i32 middle = 1073741824;
    return control > middle ? (control - middle) * 2 : 0;
}

inline void initialize(Voice &v, Shared &, u32) {
    v.phase = 0;
    v.phase_fraction = 0;
    v.envelope = 0;
    v.ready = 0;
    v.drive_mix = 0;
    v.output_gain = maximum_output_gain;
    v.pending_peak = 0;
    v.gain_hold = 0;
    v.held_peak = 0;
    v.blep_increment=0;v.blep_shift=0;v.blep_reciprocal=0;
    for (unsigned j = 0; j < 64; ++j)
        v.pending[j] = 0;
    for (unsigned j = 0; j < 2; ++j)
        v.low[j] = v.band[j] = 0;
    for (unsigned j = 0; j < 7; ++j)
        v.up[j] = v.down[j] = 0;
    for (unsigned j = 0; j < 8; ++j)
        v.controls[j] = 0;
}
inline void start(Voice &v, Shared &, u32) { v.envelope = one; }
inline void destroy(Voice &, Shared &, u32) {}
template <bool Stable, bool ConstantPitch>
inline void render_block(Voice &v, opxy::Output output, opxy::Parameters params,
                         opxy::Pitch pitch) {
    // Tune is immediate and block-constant. The other mappings can be prepared
    // once when controls have reached their targets; the moving path retains
    // the original per-sample smoothing and exact state update order.
    const i32 ratio = tune_ratio(v.controls[0]);
    const PitchStep prepared_pitch = prepare_pitch(pitch[0], ratio);
    const BlepReciprocal prepared_blep = prepare_blep(prepared_pitch.whole);
    const i32 prepared_keep = normalized_table(tables::envelope_keep, v.controls[4], 128);
    const i32 prepared_kq = one - mul(v.controls[2], 1932735282);
    const i32 prepared_wave = wave_mix(v.controls[6]);
    const i32 prepared_distortion = wave_distortion(v.controls[6]);
    const i32 prepared_compensation = Stable && prepared_distortion
        ? normalized_table(tables::sat_compensation, prepared_distortion, 128) : one;
    const i32 mix_target = params[7] > 0 ? one : 0;
    const i32 prepared_overdrive_gain=(v.controls[5]>>7)*15+16777216;
    const i32 prepared_drive_gain=(v.controls[7]>>7)*8+16777216;
    i32 filter_coefficients[64];
    if (Stable) {
        const i32 keep2=opxy::q31::multiply(prepared_keep,prepared_keep);
        const i32 keep4=opxy::q31::multiply(keep2,keep2);
        const i32 keep8=opxy::q31::multiply(keep4,keep4);
        i32 envelope=v.envelope;
        i32 previous=coefficient(v.controls[1],v.controls[3],envelope);
        for(unsigned base=0;base<64;base+=8){
            if(envelope && v.controls[4]!=one)
                envelope=opxy::q31::multiply(envelope,keep8);
            const i32 next=coefficient(v.controls[1],v.controls[3],envelope);
            const i32 delta=(next-previous)/8;
            i32 value=previous;
            for(unsigned j=0;j<7;++j){value+=delta;filter_coefficients[base+j]=value;}
            filter_coefficients[base+7]=next;
            previous=next;
        }
        v.envelope=envelope;
    }
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        // ~1.45ms one-pole smoothing. Tune follows host pitch/control ownership;
        // the remaining parameters reach their exact destination, including zero.
        if (!Stable)
            for (unsigned j = 1; j < 8; ++j) {
                const i32 target = params[j] < 0 ? 0 : params[j];
                const i32 delta = target - v.controls[j];
                v.controls[j] += (delta < 64 && delta > -64) ? delta : delta / 64;
            }
        const i32 *c = v.controls;
        const u32 dt = ConstantPitch ? advance_pitch(prepared_pitch, v.phase_fraction)
                                     : tuned_increment(pitch[i], ratio, &v.phase_fraction);
        const i32 wave = Stable ? prepared_wave : wave_mix(c[6]);
        const i32 source = (ConstantPitch
            ? oscillator_prepared(v.phase,dt,wave,prepared_blep)
            : oscillator(v.phase,dt,wave)) / 256;
        const i32 sample = c[5] ? overdrive_prepared(source,Stable ? prepared_overdrive_gain
            : (c[5]>>7)*15+16777216) : source;
        v.phase += dt;
        if (!Stable && v.envelope && c[4] != one)
            v.envelope = mul(v.envelope, Stable ? prepared_keep
                                                : normalized_table(tables::envelope_keep, c[4], 128));
        const i32 kf = Stable ? filter_coefficients[i] : coefficient(c[1], c[3], v.envelope);
        const i32 kq = Stable ? prepared_kq : one - mul(c[2], 1932735282); // 1 - .9*reso
        // This is two time updates of the SAME pair of SVFs with the SAME
        // oscillator input, not four cascaded sections (the V23 port changed it).
        // With input and four states bounded by S=2^26, two updates produce
        // intermediates no larger than 27*S < INT_MAX. Keep saturated fallback
        // for deliberately extreme resonant states.
        const u32 bounds=(u32(v.low[0])+0x04000000u) | (u32(v.low[1])+0x04000000u) |
            (u32(v.band[0])+0x04000000u) | (u32(v.band[1])+0x04000000u) |
            (u32(sample)+0x04000000u);
        i32 value;
        if (bounds < 0x08000000u) {
            filter_bounded(v,sample,kf,kq);
            value=filter_bounded(v,sample,kf,kq)/2;
        } else {
            filter(v,sample,kf,kq);
            value=filter(v,sample,kf,kq)/2;
        }
        if (!v.drive_mix && mix_target) {
            for (unsigned j = 0; j < 7; ++j)
                v.up[j] = v.down[j] = 0;
        }
        const i32 mix_delta = mix_target - v.drive_mix;
        v.drive_mix += mix_delta < 64 && mix_delta > -64 ? mix_delta : mix_delta / 64;
        if (v.drive_mix) {
            const i32 wet = drive_oversampling==1 ? drive_clip_prepared(value,Stable ? prepared_drive_gain
                : (c[7]>>7)*8+16777216) : drive_stage(v, value, c[7]);
            value = v.drive_mix == one ? wet : add(value, mul(subtract(wet, value), v.drive_mix));
        }
        const i32 distortion = Stable ? prepared_distortion : wave_distortion(c[6]);
        if (distortion)
            value = saturation_prepared(value, distortion, Stable ? prepared_compensation
                : normalized_table(tables::sat_compensation, distortion, 128));
        output[i] = value; // raw Q9.23; output conditioning owns format conversion
    }
}
inline void render_stable_constant(Voice &v,opxy::Output output,const i32 *c,opxy::Pitch pitch) {
    const PitchStep step=prepare_pitch(pitch[0],tune_ratio(c[0]));
    const BlepReciprocal reciprocal=prepare_blep(step.whole);
    const i32 wave=wave_mix(c[6]);
    for(unsigned i=0;i<64;++i){
        const u32 dt=advance_pitch(step,v.phase_fraction);
        output[i]=oscillator_prepared(v.phase,dt,wave,reciprocal)/256;
        v.phase+=dt;
    }
    if(c[5]) for(unsigned i=0;i<64;++i) output[i]=overdrive(output[i],c[5]);
    const i32 keep=normalized_table(tables::envelope_keep,c[4],128);
    const i32 kq=one-mul(c[2],1932735282);
    const i32 base_kf=coefficient(c[1],0,0);
    for(unsigned i=0;i<64;++i){
        if(v.envelope && c[4]!=one) v.envelope=mul(v.envelope,keep);
        const i32 kf=c[3] && v.envelope ? coefficient(c[1],c[3],v.envelope) : base_kf;
        const i32 sample=output[i];
        const u32 bounds=(u32(v.low[0])+0x04000000u) | (u32(v.low[1])+0x04000000u) |
            (u32(v.band[0])+0x04000000u) | (u32(v.band[1])+0x04000000u) | (u32(sample)+0x04000000u);
        if(bounds<0x08000000u){filter_bounded(v,sample,kf,kq);output[i]=filter_bounded(v,sample,kf,kq)/2;}
        else {filter(v,sample,kf,kq);output[i]=filter(v,sample,kf,kq)/2;}
    }
    if(c[7]) drive_block(v,output,c[7]);
    const i32 distortion=wave_distortion(c[6]);
    if(distortion){
        const i32 compensation=normalized_table(tables::sat_compensation,distortion,128);
        for(unsigned i=0;i<64;++i) output[i]=saturation_prepared(output[i],distortion,compensation);
    }
}
inline void render_core(Voice &v, Shared &, u32, opxy::Output output, opxy::Parameters params,
                        opxy::Pitch pitch) {
    // UI order: decay, cutoff, reso, drive, overdrive, env mod, wav/dist, tune.
    // Keep the proven DSP/state order unchanged: tune, cutoff, reso, env mod,
    // decay, overdrive, wav/dist, drive.
    const i32 dsp_controls[8] = {params[7], params[1], params[2], params[5],
                                 params[0], params[4], params[6], params[3]};
    if (!v.ready) {
        for (unsigned j = 0; j < 8; ++j)
            v.controls[j] = dsp_controls[j] < 0 ? 0 : dsp_controls[j];
        v.drive_mix = v.controls[7] > 0 ? one : 0;
        v.ready = 1;
    }
    v.controls[0] = dsp_controls[0] < 0 ? 0 : dsp_controls[0];
    bool stable = true;
    for (unsigned j = 1; j < 8; ++j) {
        if (v.controls[j] != (dsp_controls[j] < 0 ? 0 : dsp_controls[j]))
            stable = false;
    }
    bool constant_pitch = true;
    for (unsigned j = 1; j < 64; ++j)
        if (pitch[j] != pitch[0]) {
            constant_pitch = false;
            break;
        }
    if (stable) {
        if (constant_pitch && false && drive_oversampling==4 &&
            ((dsp_controls[7]>0 && v.drive_mix==one) || (!dsp_controls[7] && !v.drive_mix)))
            render_stable_constant(v,output,dsp_controls,pitch);
        else if (constant_pitch)
            render_block<true, true>(v, output, dsp_controls, pitch);
        else
            render_block<true, false>(v, output, dsp_controls, pitch);
    } else {
        if (constant_pitch)
            render_block<false, true>(v, output, dsp_controls, pitch);
        else
            render_block<false, false>(v, output, dsp_controls, pitch);
    }
}

// Output is limited by gain, not by clipping individual samples. The preceding
// call already anticipates the pending block's peak, so both ends of its gain
// ramp are safe. Gain never exceeds 0.7. Peak hold prevents waveform-period
// ripple; release restores level after a resonant transient has passed.
inline u32 magnitude(i32 value) { return value < 0 ? 0u - u32(value) : u32(value); }
inline i32 scale_output(i32 sample,i32 gain) {
    // Q23 audio -> Q31 output. The lookahead proves the resulting signed
    // 16x16 product is in range; truncation toward zero retains peak safety.
    return (sample/256)*(gain>>15);
}
inline void level_output(Voice &v, opxy::Output output) {
    u32 current_peak = 0;
    for (unsigned i = 0; i < 64; ++i) {
        const u32 absolute = magnitude(output[i]);
        if (absolute > current_peak)
            current_peak = absolute;
    }
    const u32 window_peak = current_peak > v.pending_peak ? current_peak : v.pending_peak;
    // Peaks within 3.125% of the held maximum refresh the hold. This
    // recognizes repeated waveform crests without letting block phase cause
    // periodic gain release/reduction on a steady note.
    if (window_peak >= v.held_peak) {
        v.held_peak = window_peak;
        v.gain_hold = level_hold_blocks;
    } else if (window_peak >= v.held_peak - (v.held_peak >> 5)) {
        v.gain_hold = level_hold_blocks;
    } else if (v.gain_hold)
        --v.gain_hold;
    else {
        const u32 difference = v.held_peak - window_peak;
        v.held_peak -= difference < 128 ? difference : difference / 128;
    }
    i32 allowed = maximum_output_gain;
    // peak > ceiling/.7: below this point the original .7 gain is already safe.
    if (v.held_peak > 5692269u)
        allowed = fraction(u32(level_ceiling), v.held_peak);
    if (allowed > maximum_output_gain)
        allowed = maximum_output_gain;
    const i32 start = v.output_gain, end = allowed;
    const i32 delta = end - start, step = delta / 64, remainder = delta % 64;
    const i32 sign = remainder < 0 ? -1 : 1;
    const u32 rest = u32(remainder < 0 ? -remainder : remainder);
    if (!delta) {
        // Most sustained blocks have settled gain: omit the ramp bookkeeping.
        for (unsigned i = 0; i < 64; ++i) {
            const i32 raw = output[i];
            output[i] = mul(v.pending[i],start)*256;
            v.pending[i] = raw;
        }
    } else {
        u32 error = 0;
        i32 gain = start;
        for (unsigned i = 0; i < 64; ++i) {
            gain += step;
            error += rest;
            if (error >= 64) {
                gain += sign;
                error -= 64;
            }
            const i32 raw = output[i];
            // Both ramp endpoints <= the pending block's safe gain: this multiply
            // stays below the ceiling before Q9.23 -> Q1.31 conversion (x256).
            output[i] = mul(v.pending[i],gain)*256;
            v.pending[i] = raw;
        }
    }
    v.output_gain = end;
    v.pending_peak = current_peak;
}
inline void render(Voice &v, Shared &shared, u32 owner, opxy::Output output,
                   opxy::Parameters params, opxy::Pitch pitch) {
    render_core(v, shared, owner, output, params, pitch);
    level_output(v, output);
}
} // namespace acidxy

