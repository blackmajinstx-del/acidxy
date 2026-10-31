#pragma once
#include <opxy/audio.hpp>
namespace acidxy {
struct Voice {
    opxy::u32 phase;
    opxy::u32 phase_fraction;  // low 29 bits of the tuned phase step
    opxy::i32 low[2], band[2]; // Q9.23: +/-256 headroom in recursive states
    opxy::i32 envelope;        // Q1.31
    opxy::i32 up[7], down[7];  // Q9.23, continuous across callback blocks
    opxy::i32 controls[8];     // smoothed normalized Q1.31
    opxy::u32 ready;
    opxy::i32 drive_mix;   // short crossfade when entering/leaving the FIR path
    opxy::i32 pending[64]; // one block of Q9.23 audio for transparent peak lookahead
    opxy::i32 output_gain; // Q1.31, includes the web's original 0.7 output gain
    opxy::u32 pending_peak;
    opxy::u32 gain_hold;
    opxy::u32 held_peak;
    opxy::u32 blep_increment, blep_shift;
    opxy::i32 blep_reciprocal;
};
struct Shared {};
static_assert(sizeof(Voice) == 408, "Update definition.json with Voice size");
} // namespace acidxy

