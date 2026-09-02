#include "core/recording_flow.hpp"
#include "core/recording_effects.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

// Minimal test framework
static int g_failures = 0;
static int g_tests = 0;

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::fprintf(stderr, "FAIL: %s:%d: %s == %s (%d != %d)\n", __FILE__, __LINE__, #a, #b, (int)(a), (int)(b)); \
        ++g_failures; \
    } \
} while (false)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s:%d: %s is false\n", __FILE__, __LINE__, #cond); \
        ++g_failures; \
    } \
} while (false)

#define ASSERT_FALSE(cond) do { \
    if (cond) { \
        std::fprintf(stderr, "FAIL: %s:%d: %s is true\n", __FILE__, __LINE__, #cond); \
        ++g_failures; \
    } \
} while (false)

static void run_test(const char* name, void (*fn)()) {
    ++g_tests;
    std::printf("Running: %s\n", name);
    fn();
}

// Helper: check whether a specific effect type is present in the effects list
template <typename T>
static bool has_effect(const std::vector<core::RecordingEffect>& effects) {
    for (const auto& e : effects) {
        if (std::holds_alternative<T>(e)) return true;
    }
    return false;
}

using namespace core;

// Test cases

static void test_record_from_idle() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Idle, .has_take = false};
    auto t = recording_step(s, RecordingCmd::Record);
    ASSERT_EQ(t.next.phase, FlowPhase::Countdown);
    ASSERT_TRUE(has_effect<core::StartCountdown>(t.effects));
    ASSERT_FALSE(has_effect<core::CancelCountdown>(t.effects));
}

static void test_record_ignored_in_countdown() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Countdown, .has_take = false};
    auto t = recording_step(s, RecordingCmd::Record);
    ASSERT_EQ(t.next.phase, FlowPhase::Countdown);
    ASSERT_FALSE(has_effect<core::StartCountdown>(t.effects));
}

static void test_record_ignored_in_recording() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Recording, .has_take = false};
    auto t = recording_step(s, RecordingCmd::Record);
    ASSERT_EQ(t.next.phase, FlowPhase::Recording);
    ASSERT_FALSE(has_effect<core::StartCountdown>(t.effects));
}

static void test_stop_cancels_countdown() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Countdown, .has_take = false};
    auto t = recording_step(s, RecordingCmd::Stop);
    ASSERT_EQ(t.next.phase, FlowPhase::Idle);
    ASSERT_TRUE(has_effect<core::CancelCountdown>(t.effects));
    ASSERT_FALSE(has_effect<core::StopRecording>(t.effects));
}

static void test_stop_stops_recording() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Recording, .has_take = false};
    auto t = recording_step(s, RecordingCmd::Stop);
    ASSERT_EQ(t.next.phase, FlowPhase::Idle);
    ASSERT_TRUE(has_effect<core::StopRecording>(t.effects));
    ASSERT_FALSE(has_effect<core::CancelCountdown>(t.effects));
}

static void test_stop_noop_in_idle() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Idle, .has_take = false};
    auto t = recording_step(s, RecordingCmd::Stop);
    ASSERT_EQ(t.next.phase, FlowPhase::Idle);
    ASSERT_FALSE(has_effect<core::CancelCountdown>(t.effects));
    ASSERT_FALSE(has_effect<core::StopRecording>(t.effects));
}

static void test_play_with_take() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Idle, .has_take = true};
    auto t = recording_step(s, RecordingCmd::Play);
    ASSERT_EQ(t.next.phase, FlowPhase::Idle);
    ASSERT_TRUE(has_effect<core::PlayTake>(t.effects));
}

static void test_play_without_take() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Idle, .has_take = false};
    auto t = recording_step(s, RecordingCmd::Play);
    ASSERT_EQ(t.next.phase, FlowPhase::Idle);
    ASSERT_FALSE(has_effect<core::PlayTake>(t.effects));
}

static void test_play_ignored_in_countdown() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Countdown, .has_take = true};
    auto t = recording_step(s, RecordingCmd::Play);
    ASSERT_EQ(t.next.phase, FlowPhase::Countdown);
    ASSERT_FALSE(has_effect<core::PlayTake>(t.effects));
}

static void test_redo_from_idle() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Idle, .has_take = true};
    auto t = recording_step(s, RecordingCmd::Redo);
    ASSERT_EQ(t.next.phase, FlowPhase::Idle);
    ASSERT_TRUE(has_effect<core::ClearTake>(t.effects));
    ASSERT_TRUE(has_effect<core::StopPlayback>(t.effects));
    ASSERT_FALSE(t.next.has_take);
}

static void test_redo_from_countdown() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Countdown, .has_take = false};
    auto t = recording_step(s, RecordingCmd::Redo);
    ASSERT_EQ(t.next.phase, FlowPhase::Idle);
    ASSERT_TRUE(has_effect<core::CancelCountdown>(t.effects));
    ASSERT_TRUE(has_effect<core::ClearTake>(t.effects));
    ASSERT_TRUE(has_effect<core::StopPlayback>(t.effects));
}

static void test_redo_from_recording() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Recording, .has_take = false};
    auto t = recording_step(s, RecordingCmd::Redo);
    ASSERT_EQ(t.next.phase, FlowPhase::Idle);
    ASSERT_TRUE(has_effect<core::StopRecording>(t.effects));
    ASSERT_TRUE(has_effect<core::ClearTake>(t.effects));
    ASSERT_TRUE(has_effect<core::StopPlayback>(t.effects));
}

static void test_next_advances_index() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Idle, .has_take = false};
    auto t = recording_step(s, RecordingCmd::Next);
    ASSERT_EQ(t.next.phase, FlowPhase::Idle);
    ASSERT_EQ(t.next.current_idx, 1);
    ASSERT_TRUE(has_effect<core::StopPlayback>(t.effects));
}

static void test_next_at_end_stays() {
    FlowState s{.current_idx = 2, .total = 3, .phase = FlowPhase::Idle, .has_take = false};
    auto t = recording_step(s, RecordingCmd::Next);
    ASSERT_EQ(t.next.current_idx, 2);
}

static void test_back_retreats_index() {
    FlowState s{.current_idx = 1, .total = 3, .phase = FlowPhase::Idle, .has_take = false};
    auto t = recording_step(s, RecordingCmd::Back);
    ASSERT_EQ(t.next.phase, FlowPhase::Idle);
    ASSERT_EQ(t.next.current_idx, 0);
    ASSERT_TRUE(has_effect<core::StopPlayback>(t.effects));
}

static void test_back_at_start_stays() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Idle, .has_take = false};
    auto t = recording_step(s, RecordingCmd::Back);
    ASSERT_EQ(t.next.current_idx, 0);
}

static void test_next_stops_recording() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Recording, .has_take = false};
    auto t = recording_step(s, RecordingCmd::Next);
    ASSERT_EQ(t.next.phase, FlowPhase::Idle);
    ASSERT_TRUE(has_effect<core::StopRecording>(t.effects));
    ASSERT_EQ(t.next.current_idx, 1);
}

static void test_quit_from_recording() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Recording, .has_take = false};
    auto t = recording_step(s, RecordingCmd::Quit);
    ASSERT_EQ(t.next.phase, FlowPhase::Idle);
    ASSERT_TRUE(has_effect<core::StopRecording>(t.effects));
    ASSERT_TRUE(has_effect<core::ExitToSession>(t.effects));
}

static void test_countdown_complete() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Countdown, .has_take = false};
    auto t = recording_step(s, RecordingCmd::CountdownComplete);
    ASSERT_EQ(t.next.phase, FlowPhase::Recording);
    ASSERT_TRUE(has_effect<core::ActivateCapture>(t.effects));
}

static void test_countdown_complete_ignored_in_idle() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Idle, .has_take = false};
    auto t = recording_step(s, RecordingCmd::CountdownComplete);
    ASSERT_EQ(t.next.phase, FlowPhase::Idle);
    ASSERT_FALSE(has_effect<core::ActivateCapture>(t.effects));
}

static void test_countdown_failed_returns_idle_and_cancels() {
    FlowState s{.current_idx = 0, .total = 3, .phase = FlowPhase::Countdown, .has_take = false};
    auto t = recording_step(s, RecordingCmd::CountdownFailed);
    ASSERT_EQ(t.next.phase, FlowPhase::Idle);
    ASSERT_TRUE(has_effect<core::CancelCountdown>(t.effects));
    ASSERT_FALSE(has_effect<core::ActivateCapture>(t.effects));
}

static void test_unknown_key() {
    auto cmd = parse_recording_cmd("z");
    ASSERT_FALSE(cmd.has_value());
}

static void test_valid_keys_parse() {
    ASSERT_EQ(parse_recording_cmd("r").value(), RecordingCmd::Record);
    ASSERT_EQ(parse_recording_cmd("s").value(), RecordingCmd::Stop);
    ASSERT_EQ(parse_recording_cmd("p").value(), RecordingCmd::Play);
    ASSERT_EQ(parse_recording_cmd("x").value(), RecordingCmd::Redo);
    ASSERT_EQ(parse_recording_cmd("n").value(), RecordingCmd::Next);
    ASSERT_EQ(parse_recording_cmd("b").value(), RecordingCmd::Back);
    ASSERT_EQ(parse_recording_cmd("q").value(), RecordingCmd::Quit);
}

int main() {
    std::printf("=== Recording Flow Tests ===\n");
    
    run_test("record_from_idle", test_record_from_idle);
    run_test("record_ignored_in_countdown", test_record_ignored_in_countdown);
    run_test("record_ignored_in_recording", test_record_ignored_in_recording);
    run_test("stop_cancels_countdown", test_stop_cancels_countdown);
    run_test("stop_stops_recording", test_stop_stops_recording);
    run_test("stop_noop_in_idle", test_stop_noop_in_idle);
    run_test("play_with_take", test_play_with_take);
    run_test("play_without_take", test_play_without_take);
    run_test("play_ignored_in_countdown", test_play_ignored_in_countdown);
    run_test("redo_from_idle", test_redo_from_idle);
    run_test("redo_from_countdown", test_redo_from_countdown);
    run_test("redo_from_recording", test_redo_from_recording);
    run_test("next_advances_index", test_next_advances_index);
    run_test("next_at_end_stays", test_next_at_end_stays);
    run_test("back_retreats_index", test_back_retreats_index);
    run_test("back_at_start_stays", test_back_at_start_stays);
    run_test("next_stops_recording", test_next_stops_recording);
    run_test("quit_from_recording", test_quit_from_recording);
    run_test("countdown_complete", test_countdown_complete);
    run_test("countdown_complete_ignored_in_idle", test_countdown_complete_ignored_in_idle);
    run_test("countdown_failed_returns_idle_and_cancels", test_countdown_failed_returns_idle_and_cancels);
    run_test("unknown_key", test_unknown_key);
    run_test("valid_keys_parse", test_valid_keys_parse);
    
    std::printf("\n=== Summary ===\n");
    std::printf("Tests run:    %d\n", g_tests);
    std::printf("Failures:     %d\n", g_failures);
    
    if (g_failures == 0) {
        std::printf("\nAll tests passed!\n");
        return 0;
    } else {
        std::printf("\nSome tests failed.\n");
        return 1;
    }
}
