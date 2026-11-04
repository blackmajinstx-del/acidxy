#include "display.h"

namespace acidxy {
using opxy::i32;
using opxy::u32;
using opxy::draw::unit;

static const u32 strobe_period_q10 = 41; // two fast flashes restart on every note-on
static const u32 strobe_total_q10 = 246;

static inline bool notes_active(const opxy::draw::Input &input) {
    // The public SDK supplies note activity, not transport state.
    // Animate for held notes; do not add a private firmware dependency.
    return input.held_notes > 0 && input.held_notes <= 128;
}

static inline i32 control_q10(i32 value) {
    if (value < 0)
        value = 0;
    return value >> 21; // Q31 [0,1] -> Q10 [0,1023]
}

static void header_tune(char *out, u32 capacity, i32 value) {
    // One decimal semitone matches the continuous +/-24-semitone mapping.
    const i32 tenths =
        -240 +
        static_cast<i32>(((static_cast<u32>(value < 0 ? 0 : value) >> 12) * 480u + 262144u) >> 19);
    char number[8] = {};
    u32 at = 0;
    number[at++] = tenths < 0 ? '-' : '+';
    const u32 magnitude = static_cast<u32>(tenths < 0 ? -tenths : tenths);
    if (magnitude >= 100)
        number[at++] = static_cast<char>('0' + magnitude / 100);
    number[at++] = static_cast<char>('0' + magnitude / 10 % 10);
    number[at++] = '.';
    number[at++] = static_cast<char>('0' + magnitude % 10);
    opxy::draw::copy(out, capacity, number);
}

static void header_wave_dist(char *out, u32 capacity, i32 value) {
    const u32 normalized = value < 0 ? 0 : static_cast<u32>(value);
    const i32 amount = static_cast<i32>(((normalized >> 12) * 100u + 262144u) >> 19) - 50;
    char number[5] = {};
    u32 at = 0;
    if (amount > 0)
        number[at++] = '+';
    else if (amount < 0)
        number[at++] = '-';
    const u32 magnitude = static_cast<u32>(amount < 0 ? -amount : amount);
    if (magnitude >= 10)
        number[at++] = static_cast<char>('0' + magnitude / 10);
    number[at++] = static_cast<char>('0' + magnitude % 10);
    opxy::draw::copy(out, capacity, number);
}

inline void display_reset(DisplayState &state) {
    state.phase = 0;
    state.header.reset();
    state.notes.reset();
    state.last_note_serial = 0;
    state.strobe_elapsed = strobe_total_q10;
    state.play_elapsed = 0;
}

inline void display(DisplayState &state, const opxy::draw::Input &input, Frame &frame) {
    using namespace opxy::draw;
    const char *const titles[8] = {"decay", "cutoff", "reso", "drive",
                                   "overdrv", "env mod", "wav dist", "tune"};
    char values[8][header_capacity] = {};
    for (u32 i = 0; i < 8; ++i)
        header_amount(values[i], header_capacity, input.slots[i]);
    header_tune(values[7], header_capacity, input.slots[7]);
    header_wave_dist(values[6], header_capacity, input.slots[6]);
    const opxy::u8 slots[2][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}};
    page_header(state.header, input, frame, titles, values, 2, slots);

    const i32 dt = clamp(input.elapsed, 0, 103);
    const bool audio_active = notes_active(input) || input.held_notes > 0;
    if (audio_active && !input.reduced_motion)
        state.phase += static_cast<u32>(dt) * 32u;
    else
        state.phase = 0;
    const bool track_changed = !state.notes.initialized || state.notes.track != input.track;
    if (track_changed) {
        state.phase = 0;
        state.play_elapsed = 0;
        state.strobe_elapsed = strobe_total_q10;
    }
    const bool same_track =
        state.notes.initialized && state.notes.track == input.track && input.held_notes <= 128;
    if (same_track && input.note_serial != state.last_note_serial) {
        ++state.play_elapsed;
        state.strobe_elapsed = 0;
        state.phase += 8192u; // snap the visual oscillator to each new note
    }
    state.last_note_serial = input.note_serial;
    state.notes.update(input);
    // The pose counter above follows note onsets, independent of CUTOFF.

    const i32 resonance = control_q10(input.slots[2]);
    const i32 overdrive_amount = control_q10(input.slots[4]);
    // Retire the old waveform slots; no geometry is generated or uploaded.
    hide(frame, 0);
    hide(frame, 1);

    // CUTOFF controls faces; cap them at half brightness so they stay behind the dancer.
    const i32 cutoff = control_q10(input.slots[1]);
    const u32 face_shade = static_cast<u32>(clamp((cutoff * 7 + 511) / 1023, 0, 7));
    place(frame, 10, 0, 0, unit, face_shade, stroke); // Ring remains outline-only.
    place(frame, 11, 0, 0, unit, face_shade, fill_and_stroke);
    place(frame, 12, 0, 0, unit, face_shade, fill_and_stroke);
    place(frame, 13, 0, 0, unit, face_shade,
          stroke); // Smooth open smile from original web preview.

    // OVERDRIVE controls eyebrows, still capped at half brightness.
    const u32 brow_shade = static_cast<u32>(clamp((overdrive_amount * 7 + 511) / 1023, 0, 7));
    place(frame, 14, 0, 0, unit, brow_shade, fill_and_stroke);
    place(frame, 15, 0, 0, unit, brow_shade, fill_and_stroke);

    // DRIVE controls the alternating note strobes.
    const i32 drive = control_q10(input.slots[3]);
    const bool strobe_enabled = drive > 0;
    const bool strobe_visible =
        strobe_enabled && !input.reduced_motion && state.strobe_elapsed < strobe_total_q10 &&
        (state.strobe_elapsed % strobe_period_q10) < (strobe_period_q10 / 2);
    if (strobe_visible) {
        const u32 strobe_shade = static_cast<u32>(clamp(2 + (drive * 12) / 1023, 2, 14));
        const u32 side = state.play_elapsed & 1u;
        place(frame, 16 + side, 0, -18 * unit, unit, strobe_shade, fill_and_stroke);
        hide(frame, 17 - side);
    } else {
        hide(frame, 16);
        hide(frame, 17);
    }
    if (state.strobe_elapsed < strobe_total_q10) {
        state.strobe_elapsed += static_cast<u32>(dt);
        if (state.strobe_elapsed > strobe_total_q10)
            state.strobe_elapsed = strobe_total_q10;
    }

    const u32 sequence[4] = {0, 1, 2, 1};
    u32 dancer_frame = 1;
    if (!input.reduced_motion && state.play_elapsed > 0 &&
        (notes_active(input) || state.strobe_elapsed < strobe_total_q10))
        dancer_frame = sequence[(state.play_elapsed - 1u) & 3u];
    for (u32 i = 0; i < 3; ++i) {
        const u32 node = 18 + i;
        if (i == dancer_frame) {
            place(frame, node, 0, 18 * unit, unit, 15, keep);
            place(frame, 21 + i, 0, 18 * unit, unit, 0, keep);
        } else {
            hide(frame, node);
            hide(frame, 21 + i);
        }
    }
    // One combined club-light effect: every second note alternates the two
    // overhead beams, while three filled floor ellipses flash with it. RESONANCE
    // controls all shades; there are only seven simple nodes to keep drawing cheap.
    const bool stage_visible = resonance > 0 && !input.reduced_motion && state.play_elapsed > 0 &&
                               ((state.play_elapsed - 1u) & 1u) == 0 && state.strobe_elapsed < 82u;
    if (stage_visible) {
        const u32 side = ((state.play_elapsed - 1u) >> 1) & 1u;
        for (u32 i = 0; i < 4; ++i)
            if (i / 2 == side)
                place(frame, 24 + i, 0, 0, unit,
                      static_cast<u32>(i % 2 ? 4 + (resonance * 3) / 1023
                                               : 2 + (resonance * 2) / 1023), fill_and_stroke);
            else
                hide(frame, 24 + i);
        for (u32 i = 0; i < 3; ++i)
            place(frame, 28 + i, 0, 0, unit,
                  static_cast<u32>(clamp(2 + (resonance * (1 + i * 2)) / 1023, 2, 7)), fill_and_stroke);
    } else {
        for (u32 i = 24; i < 31; ++i)
            hide(frame, i);
    }
}
} // namespace acidxy


