#include "../display.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace acid;
using namespace opxy::draw;
static void require(bool ok, const char *why) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", why);
        std::exit(1);
    }
}
static void draw(DisplayState &s, Input &in, acid::Frame &f) {
    std::memset(&f, 0, sizeof(f));
    display(s, in, f);
}
int main() {
    DisplayState s = {};
    Input in = {};
    acid::Frame f = {};
    display_reset(s);
    in.elapsed = 17;
    in.track = 1;
    for (unsigned j = 0; j < 4; ++j)
        in.shown[j] = j;
    in.slots[7] = 1073741824;
    draw(s, in, f);
    require(!std::strcmp(f.texts[15].chars, "+0.0"), "neutral tune caption on second row");
    require(!std::strcmp(f.texts[0].chars, "decay") && !std::strcmp(f.texts[8].chars, "overdrv"),
            "two fixed rows");
    in.reduced_motion = 1;
    for (unsigned j = 0; j < 4; ++j)
        in.shown[j] = j + 4;
    draw(s, in, f);
    require(s.header.page == 1 && s.header.position == unit, "second page reduced-motion jump");
    require(!std::strcmp(f.texts[0].chars, "overdrv") && !std::strcmp(f.texts[8].chars, "decay"),
            "active captions swap emphasis");
    require(f.nodes[2].y == 18 * unit, "bar on edited second row");
    for (unsigned j = 0; j < 4; ++j)
        in.shown[j] = j;
    in.reduced_motion = 0;
    in.held_notes = 1;
    in.slots[4] = 2147483647;
    in.slots[2] = 2147483647;
    in.slots[3] = 2147483647;
    in.note_serial = 1;
    draw(s, in, f);
    require(s.phase > 0, "held notes animate wave");
    require(acid::Frame::path_count == 0 && f.nodes[0].x == hidden_x * unit &&
            f.nodes[1].x == hidden_x * unit, "visual waveform removed");
    require(f.nodes[18].paint == keep && f.nodes[21].paint == keep,
            "sprite keeps its authored white body and black underlay");
    require((f.nodes[16].x == 0) != (f.nodes[17].x == 0), "alternating note strobe");
    require(f.nodes[18].x == 0 && f.nodes[21].x == 0, "first note selects new left pose and mask");
    in.slots[2] = 2147483; // 0.1% resonance: both beam layers must already differ.
    draw(s, in, f);
    require(f.nodes[24].x == 0 && f.nodes[25].x == 0 &&
            f.nodes[24].shade >= 2 && f.nodes[25].shade > f.nodes[24].shade,
            "both resonance beam layers visible from bottom of range");
    in.slots[4] = 0;
    draw(s, in, f);
    require(f.nodes[14].shade == 0 && f.nodes[15].shade == 0,
            "overdrive zero hides eyebrows even at full resonance");
    in.slots[4] = 2147483647;
    in.slots[2] = 0;
    draw(s, in, f);
    require(f.nodes[14].shade == 7 && f.nodes[15].shade == 7 &&
            f.nodes[24].x == hidden_x * unit && f.nodes[25].x == hidden_x * unit,
            "overdrive lights brows while resonance zero disables stage lights");
    in.slots[2] = 2147483647;
    const unsigned pose_order[4] = {1, 2, 1, 0};
    for (unsigned expected : pose_order) {
        ++in.note_serial;
        in.slots[1] = in.slots[1] == 0 ? 2147483647 : 0;
        draw(s, in, f);
        require(f.nodes[18 + expected].x == 0 && f.nodes[21 + expected].x == 0,
                "note onsets select left-center-right-center independently of cutoff");
    }
    const auto prior = s.phase;
    draw(s, in, f);
    require(s.phase > prior, "continuous motion");
    in.reduced_motion = 1;
    const auto stopped_pose_count = s.play_elapsed;
    draw(s, in, f);
    const auto stopped = s.phase;
    draw(s, in, f);
    require(s.phase == stopped && s.play_elapsed == stopped_pose_count,
            "reduced motion freezes animation");
    require(f.nodes[19].x == 0 && f.nodes[16].x == hidden_x * unit, "center pose and no flashing");
    in.reduced_motion = 0;
    in.track = 2;
    draw(s, in, f);
    require(s.strobe_elapsed == 246 && s.header.elapsed == 154,
            "track change resets event history and header");
    in.held_notes = 0;
    draw(s, in, f);
    acid::Frame idle = f;
    for (unsigned j = 0; j < 20; ++j)
        draw(s, in, f);
    require(!std::memcmp(&idle, &f, sizeof(f)), "idle scene remains still");
    for(unsigned configuration=0;configuration<2;++configuration){
        DisplayState routing={}; Input controls={}; acid::Frame scene={};
        display_reset(routing);controls.track=1;controls.elapsed=1;controls.held_notes=1;
        controls.slots[2]=configuration ? 0 : 2147483647;
        controls.slots[3]=configuration ? 2147483647 : 0;
        draw(routing,controls,scene);controls.note_serial=1;draw(routing,controls,scene);
        const bool strobe=scene.nodes[16].x==0 || scene.nodes[17].x==0;
        const bool stage=scene.nodes[24].x==0 || scene.nodes[26].x==0;
        require(strobe==bool(configuration) && stage!=bool(configuration),
                "drive and resonance independently own strobes and stage lights");
    }
    opxy::u32 seed = 37;
    for (unsigned frame = 0; frame < 80000; ++frame) {
        seed = seed * 1664525u + 1013904223u;
        in.elapsed = opxy::i32(seed % 500) - 100;
        in.track = frame / 257;
        in.reduced_motion = (seed >> 14) & 1;
        in.held_notes = frame % 9 == 0 ? notes_unavailable : (seed >> 16) % 9;
        in.note_serial = seed;
        in.note_off_serial = seed >> 3;
        for (unsigned j = 0; j < 8; ++j) {
            seed = seed * 1664525u + 1013904223u;
            in.slots[j] = opxy::i32(seed >> 1);
        }
        for (unsigned j = 0; j < 4; ++j)
            in.shown[j] = j + ((frame & 1) ? 4 : 0);
        draw(s, in, f);
        unsigned poses = 0;
        for (unsigned j = 18; j < 21; ++j)
            poses += f.nodes[j].x == 0;
        require(poses == 1, "exactly one dancer pose");
        for (const auto &node : f.nodes) {
            require(node.x >= node_left * unit && node.x <= node_right * unit &&
                        node.y >= node_top * unit && node.y <= node_bottom * unit,
                    "node coordinates");
            require(node.scale > 0 && node.shade < 16, "node scale and palette");
        }
        for (const auto &path : f.paths)
            for (const auto &point : path.points)
                require(point.x >= -64 * unit && point.x <= 64 * unit && point.y >= -64 * unit &&
                            point.y <= 64 * unit,
                        "path extent");
        for (const auto &text : f.texts)
            require(text.chars[text_capacity - 1] == 0, "terminated text");
    }
    std::puts("PASS acid drawing: header pages, tune, notes, strobes, reduced motion, track "
              "resets, idle, 80000 frames");
}
