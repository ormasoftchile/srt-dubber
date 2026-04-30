#include "core/app_session.hpp"
#include <cstdio>
#include <cstdlib>

// Minimal test framework
static int g_failures = 0;
static int g_tests    = 0;

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::fprintf(stderr, "FAIL: %s:%d: %s == %s (%d != %d)\n", \
            __FILE__, __LINE__, #a, #b, (int)(a), (int)(b)); \
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

// ---------- Tests ----------

static void test_initial_state() {
    core::NavState s;
    ASSERT_EQ(s.screen, core::AppScreen::Session);
    ASSERT_TRUE(s.running);
    ASSERT_EQ(s.recording_idx, 0);
    ASSERT_EQ(s.review_idx, 0);
}

static void test_go_record_from_session() {
    core::NavState s;
    auto t = core::nav_step(s, core::NavCmd::GoRecord, 0);
    ASSERT_EQ(t.next.screen, core::AppScreen::Recording);
    ASSERT_EQ(t.next.recording_idx, 0);
    ASSERT_TRUE(t.next.running);
}

static void test_go_review_from_session() {
    core::NavState s;
    auto t = core::nav_step(s, core::NavCmd::GoReview);
    ASSERT_EQ(t.next.screen, core::AppScreen::Review);
    ASSERT_TRUE(t.next.running);
}

static void test_go_assemble_from_session() {
    core::NavState s;
    auto t = core::nav_step(s, core::NavCmd::GoAssemble);
    ASSERT_EQ(t.next.screen, core::AppScreen::Assemble);
    ASSERT_TRUE(t.next.running);
}

static void test_recording_done_returns_to_session() {
    core::NavState s;
    s.screen = core::AppScreen::Recording;
    auto t = core::nav_step(s, core::NavCmd::RecordingDone);
    ASSERT_EQ(t.next.screen, core::AppScreen::Session);
    ASSERT_TRUE(t.next.running);
}

static void test_review_go_record_with_payload() {
    core::NavState s;
    s.screen = core::AppScreen::Review;
    auto t = core::nav_step(s, core::NavCmd::ReviewGoRecord, 3);
    ASSERT_EQ(t.next.screen, core::AppScreen::Recording);
    ASSERT_EQ(t.next.recording_idx, 3);
    ASSERT_TRUE(t.next.running);
}

static void test_review_go_session() {
    core::NavState s;
    s.screen = core::AppScreen::Review;
    auto t = core::nav_step(s, core::NavCmd::ReviewGoSession);
    ASSERT_EQ(t.next.screen, core::AppScreen::Session);
    ASSERT_TRUE(t.next.running);
}

static void test_quit_stops_running() {
    core::NavState s;
    auto t = core::nav_step(s, core::NavCmd::Quit);
    ASSERT_FALSE(t.next.running);
}

static void test_assemble_done_returns_to_session() {
    core::NavState s;
    s.screen = core::AppScreen::Assemble;
    auto t = core::nav_step(s, core::NavCmd::AssembleDone);
    ASSERT_EQ(t.next.screen, core::AppScreen::Session);
    ASSERT_TRUE(t.next.running);
}

static void test_review_idx_preserved_across_transitions() {
    core::NavState s;
    s.review_idx = 7;
    // GoReview preserves review_idx
    auto t = core::nav_step(s, core::NavCmd::GoReview);
    ASSERT_EQ(t.next.review_idx, 7);
    ASSERT_EQ(t.next.screen, core::AppScreen::Review);
}

int main() {
    run_test("initial state is Session, running=true",       test_initial_state);
    run_test("GoRecord → Recording, recording_idx=0",        test_go_record_from_session);
    run_test("GoReview → Review",                            test_go_review_from_session);
    run_test("GoAssemble → Assemble",                        test_go_assemble_from_session);
    run_test("RecordingDone → Session",                      test_recording_done_returns_to_session);
    run_test("ReviewGoRecord payload=3 → Recording idx=3",   test_review_go_record_with_payload);
    run_test("ReviewGoSession → Session",                    test_review_go_session);
    run_test("Quit → running=false",                         test_quit_stops_running);
    run_test("AssembleDone → Session",                       test_assemble_done_returns_to_session);
    run_test("review_idx preserved across GoReview",         test_review_idx_preserved_across_transitions);

    std::printf("\n%d/%d tests passed\n", g_tests - g_failures, g_tests);
    return g_failures == 0 ? 0 : 1;
}
