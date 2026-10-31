#pragma once
#include <opxy/page_header.hpp>

namespace acidxy {
// No mutable wave paths; the retired slots retain stable artwork IDs.
using Frame = opxy::draw::Frame<0, 0, 31, 16, 16>;
struct DisplayState {
    opxy::u32 phase;
    opxy::draw::PageHeader header;
    opxy::draw::NoteTracker notes;
    opxy::u32 last_note_serial;
    opxy::u32 strobe_elapsed;
    opxy::u32 play_elapsed;
};
static_assert(sizeof(opxy::draw::PageHeader) == 20, "Unexpected Open-XY PageHeader layout");
static_assert(sizeof(opxy::draw::NoteTracker) == 16, "Unexpected Open-XY NoteTracker layout");
static_assert(sizeof(DisplayState) == 52, "Keep resources.display.stateBytes in sync");
} // namespace acidxy

