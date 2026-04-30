#include "core/assemble_flow.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

// ─── Minimal test framework ─────────────────────────────────────────────────

static int g_failures = 0;
static int g_tests    = 0;

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

#define ASSERT_STR_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::fprintf(stderr, "FAIL: %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a).c_str(), (b).c_str()); \
        ++g_failures; \
    } \
} while (false)

#define ASSERT_STR_CONTAINS(haystack, needle) do { \
    if ((haystack).find(needle) == std::string::npos) { \
        std::fprintf(stderr, "FAIL: %s:%d: \"%s\" not found in \"%s\"\n", __FILE__, __LINE__, needle, (haystack).c_str()); \
        ++g_failures; \
    } \
} while (false)

static void run_test(const char* name, void (*fn)()) {
    ++g_tests;
    std::printf("Running: %s\n", name);
    fn();
}

// ─── Effect helper ──────────────────────────────────────────────────────────

template <typename T>
static bool has_effect(const std::vector<core::AssembleEffect>& effects) {
    for (const auto& e : effects) {
        if (std::holds_alternative<T>(e)) return true;
    }
    return false;
}

using namespace core;

// ─── Test cases ─────────────────────────────────────────────────────────────

// 1. Start from Idle → phase = Assembling, emits SpawnAssembly
static void test_start_from_idle() {
    AssembleState s{};
    auto t = assemble_step(s, AssembleCmd::Start);
    ASSERT_EQ(t.next.phase, AssemblePhase::Assembling);
    ASSERT_TRUE(has_effect<SpawnAssembly>(t.effects));
    ASSERT_EQ((int)t.effects.size(), 1);
}

// 2. Start from Assembling → no-op (guard re-trigger)
static void test_start_from_assembling_noop() {
    AssembleState s{.phase = AssemblePhase::Assembling};
    auto t = assemble_step(s, AssembleCmd::Start);
    ASSERT_EQ(t.next.phase, AssemblePhase::Assembling);
    ASSERT_TRUE(t.effects.empty());
}

// 3. ProgressLine from Assembling → emits AppendLog, stays Assembling
static void test_progress_line_appends_log() {
    AssembleState s{.phase = AssemblePhase::Assembling};
    auto t = assemble_step(s, AssembleCmd::ProgressLine, "Processing...");
    ASSERT_EQ(t.next.phase, AssemblePhase::Assembling);
    ASSERT_TRUE(has_effect<AppendLog>(t.effects));
    // Verify the log line payload
    bool found = false;
    for (const auto& e : t.effects) {
        if (auto* al = std::get_if<AppendLog>(&e)) {
            if (al->line == "Processing...") found = true;
        }
    }
    ASSERT_TRUE(found);
}

// 4. Multiple ProgressLine calls → log_lines accumulate
static void test_progress_lines_accumulate() {
    AssembleState s{.phase = AssemblePhase::Assembling};
    auto t1 = assemble_step(s,        AssembleCmd::ProgressLine, "Line 1");
    auto t2 = assemble_step(t1.next,  AssembleCmd::ProgressLine, "Line 2");
    auto t3 = assemble_step(t2.next,  AssembleCmd::ProgressLine, "Line 3");
    ASSERT_EQ((int)t3.next.log_lines.size(), 3);
    ASSERT_STR_EQ(t3.next.log_lines[0], std::string("Line 1"));
    ASSERT_STR_EQ(t3.next.log_lines[1], std::string("Line 2"));
    ASSERT_STR_EQ(t3.next.log_lines[2], std::string("Line 3"));
}

// 5. AssemblyDone from Assembling → Complete, output_path set, emits SaveProject
static void test_assembly_done() {
    AssembleState s{.phase = AssemblePhase::Assembling};
    auto t = assemble_step(s, AssembleCmd::AssemblyDone, "/out/dub.wav");
    ASSERT_EQ(t.next.phase, AssemblePhase::Complete);
    ASSERT_STR_EQ(t.next.output_path, std::string("/out/dub.wav"));
    ASSERT_TRUE(has_effect<SaveProject>(t.effects));
}

// 6. AssemblyFailed from Assembling → Failed, error_msg set
static void test_assembly_failed() {
    AssembleState s{.phase = AssemblePhase::Assembling};
    auto t = assemble_step(s, AssembleCmd::AssemblyFailed, "ffmpeg error");
    ASSERT_EQ(t.next.phase, AssemblePhase::Failed);
    ASSERT_STR_EQ(t.next.error_msg, std::string("ffmpeg error"));
    ASSERT_FALSE(has_effect<SaveProject>(t.effects));
}

// 7. Back from Idle → emits ExitToSession
static void test_back_from_idle() {
    AssembleState s{};
    auto t = assemble_step(s, AssembleCmd::Back);
    ASSERT_TRUE(has_effect<ExitToSession>(t.effects));
}

// 8. Back from Assembling → emits ExitToSession
static void test_back_from_assembling() {
    AssembleState s{.phase = AssemblePhase::Assembling};
    auto t = assemble_step(s, AssembleCmd::Back);
    ASSERT_TRUE(has_effect<ExitToSession>(t.effects));
}

// 9. Back from Complete → emits ExitToSession
static void test_back_from_complete() {
    AssembleState s{.phase = AssemblePhase::Complete, .output_path = "/out/dub.wav"};
    auto t = assemble_step(s, AssembleCmd::Back);
    ASSERT_TRUE(has_effect<ExitToSession>(t.effects));
}

// 10. assemble_render_state for Complete → correct phase_label and footer
static void test_render_state_complete() {
    AssembleState s{.phase = AssemblePhase::Complete, .output_path = "/out/dub.wav"};
    auto rs = assemble_render_state(s);
    ASSERT_STR_EQ(rs.phase_label, std::string("complete"));
    ASSERT_STR_CONTAINS(rs.footer, "/out/dub.wav");
    ASSERT_TRUE(rs.allow_back);
}

// ─── main ───────────────────────────────────────────────────────────────────

int main() {
    run_test("start_from_idle",             test_start_from_idle);
    run_test("start_from_assembling_noop",  test_start_from_assembling_noop);
    run_test("progress_line_appends_log",   test_progress_line_appends_log);
    run_test("progress_lines_accumulate",   test_progress_lines_accumulate);
    run_test("assembly_done",               test_assembly_done);
    run_test("assembly_failed",             test_assembly_failed);
    run_test("back_from_idle",              test_back_from_idle);
    run_test("back_from_assembling",        test_back_from_assembling);
    run_test("back_from_complete",          test_back_from_complete);
    run_test("render_state_complete",       test_render_state_complete);

    std::printf("\n%d/%d tests passed.\n", g_tests - g_failures, g_tests);
    return g_failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
