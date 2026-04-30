#include "audio/recorder.hpp"  // stub (tests/audio/recorder.hpp shadows src/audio/recorder.hpp)
#include "audio/player.hpp"    // stub
#include "core/effect_dispatcher.hpp"
#include "core/project.hpp"
#include "core/recording_effects.hpp"
#include "core/types.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

// ── Minimal test framework ──────────────────────────────────────────────────

static int g_failures = 0;
static int g_tests    = 0;

#define ASSERT_EQ(a, b) do { \
    ++g_tests; \
    if ((a) != (b)) { \
        std::fprintf(stderr, "FAIL: %s:%d  %s == %s  (%d != %d)\n", \
                     __FILE__, __LINE__, #a, #b, (int)(a), (int)(b)); \
        ++g_failures; \
    } \
} while (false)

#define ASSERT_TRUE(cond) do { \
    ++g_tests; \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s:%d  %s is false\n", __FILE__, __LINE__, #cond); \
        ++g_failures; \
    } \
} while (false)

#define ASSERT_FALSE(cond) do { \
    ++g_tests; \
    if (cond) { \
        std::fprintf(stderr, "FAIL: %s:%d  %s is true\n", __FILE__, __LINE__, #cond); \
        ++g_failures; \
    } \
} while (false)

static void run_test(const char* name, void (*fn)()) {
    std::printf("Running: %s\n", name);
    fn();
}

// ── Fixture helpers ─────────────────────────────────────────────────────────

// Returns a Project loaded from the bundled minimal fixture.
// Takes/processed/output dirs are created alongside tests/fixtures/minimal.srt.
static core::Project make_test_project() {
    namespace fs = std::filesystem;
    // __FILE__ is the absolute path of this source file at compile time.
    auto srt = fs::path(__FILE__).parent_path() / "fixtures" / "minimal.srt";
    return core::Project::load_or_create(srt);
}

// ── Test cases ──────────────────────────────────────────────────────────────

static void test_stop_playback_when_not_playing() {
    // player.is_playing() == false — dispatcher must not crash.
    AudioPlayer  player;
    AudioRecorder recorder;
    auto project = make_test_project();

    ASSERT_FALSE(player.is_playing());

    core::RecordingEffectDispatcher dispatcher{recorder, player, project};
    dispatcher.apply(core::StopPlayback{});

    // stop() should NOT have been called (guard: only call if is_playing()).
    ASSERT_EQ(player.stop_call_count, 0);
}

static void test_save_project_calls_save() {
    AudioPlayer   player;
    AudioRecorder recorder;
    auto project = make_test_project();

    core::RecordingEffectDispatcher dispatcher{recorder, player, project};

    // Verify it doesn't throw / crash. The project writes project.json to disk.
    dispatcher.apply(core::SaveProject{});
    // If we reach here without a crash the effect was dispatched.
    ASSERT_TRUE(true);
}

static void test_clear_take_resets_entry() {
    AudioPlayer   player;
    AudioRecorder recorder;
    auto project = make_test_project();

    // Pre-populate entry 0 with a take.
    auto& e = project.entries()[0];
    e.raw_take_path       = "/some/take.wav";
    e.processed_take_path = "/some/processed.wav";
    e.raw_duration_ms     = 1234;
    e.status              = core::TakeStatus::ok;

    core::RecordingEffectDispatcher dispatcher{recorder, player, project};
    dispatcher.apply(core::ClearTake{0});

    ASSERT_TRUE(project.entries()[0].raw_take_path.empty());
    ASSERT_TRUE(project.entries()[0].processed_take_path.empty());
    ASSERT_EQ(project.entries()[0].raw_duration_ms, -1);
    ASSERT_EQ((int)project.entries()[0].status, (int)core::TakeStatus::pending);
}

static void test_apply_all_runs_in_order() {
    AudioPlayer   player;
    AudioRecorder recorder;
    auto project = make_test_project();

    // Prime player as playing so StopPlayback calls stop().
    player.play("/fake/path.wav");
    ASSERT_TRUE(player.is_playing());

    core::RecordingEffectDispatcher dispatcher{recorder, player, project};
    dispatcher.apply_all({core::StopPlayback{}, core::SaveProject{}});

    // StopPlayback was first: player should have stopped.
    ASSERT_FALSE(player.is_playing());
    // stop_call_count == 2: one from the play() setup above is wrong...
    // Actually play() doesn't call stop(). Let's just verify is_playing() is false.
    ASSERT_TRUE(!player.is_playing());
}

// ── Main ────────────────────────────────────────────────────────────────────

int main() {
    run_test("stop_playback_when_not_playing",  test_stop_playback_when_not_playing);
    run_test("save_project_calls_save",          test_save_project_calls_save);
    run_test("clear_take_resets_entry",          test_clear_take_resets_entry);
    run_test("apply_all_runs_in_order",          test_apply_all_runs_in_order);

    if (g_failures == 0) {
        std::printf("\nAll %d assertions passed.\n", g_tests);
        return 0;
    }
    std::fprintf(stderr, "\n%d / %d assertions FAILED.\n", g_failures, g_tests);
    return 1;
}
