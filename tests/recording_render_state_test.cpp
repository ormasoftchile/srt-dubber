// Tests for core::render_state() — pure function, no audio/TUI deps

#include "core/recording_flow.hpp"
#include "core/recording_render_state.hpp"
#include <cassert>
#include <iostream>

static int passed = 0, failed = 0;
#define ASSERT_EQ(a, b) do { if ((a) == (b)) { ++passed; } else { \
    std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " — " << #a << " != " << #b << "\n"; ++failed; } } while(0)
#define ASSERT_TRUE(x) ASSERT_EQ((x), true)
#define ASSERT_FALSE(x) ASSERT_EQ((x), false)

int main() {
    using namespace core;

    // Idle phase → label "idle"
    {
        FlowState s{0, 5, FlowPhase::Idle, false};
        auto rs = render_state(s, "Hello world", 3000, 0);
        ASSERT_EQ(rs.phase_label, std::string("idle"));
        ASSERT_EQ(rs.current_idx, 0);
        ASSERT_EQ(rs.total, 5);
        ASSERT_FALSE(rs.has_take);
        ASSERT_EQ(rs.entry_text, std::string("Hello world"));
        ASSERT_EQ(rs.slot_duration_ms, int64_t(3000));
        ASSERT_EQ(rs.elapsed_ms, int64_t(0));
    }

    // Countdown phase → label "countdown"
    {
        FlowState s{2, 5, FlowPhase::Countdown, true};
        auto rs = render_state(s, "Test", 2000, 0);
        ASSERT_EQ(rs.phase_label, std::string("countdown"));
        ASSERT_TRUE(rs.has_take);
        ASSERT_EQ(rs.current_idx, 2);
    }

    // Recording phase → label "recording", elapsed propagated
    {
        FlowState s{1, 3, FlowPhase::Recording, false};
        auto rs = render_state(s, "Sub", 4000, 1500);
        ASSERT_EQ(rs.phase_label, std::string("recording"));
        ASSERT_EQ(rs.elapsed_ms, int64_t(1500));
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
