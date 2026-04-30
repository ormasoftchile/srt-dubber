#include "core/review_flow.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

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

// Helper: check whether a specific effect type is present in the effects list
template <typename T>
static bool has_effect(const std::vector<core::ReviewEffect>& effects) {
    for (const auto& e : effects) {
        if (std::holds_alternative<T>(e)) return true;
    }
    return false;
}

using namespace core;

// ── Test cases ────────────────────────────────────────────────────────────────

static void test_up_at_zero_stays() {
    ReviewState s{.selected_idx = 0, .total = 3};
    auto t = review_step(s, ReviewCmd::Up, false, {});
    ASSERT_EQ(t.next.selected_idx, 0);
    ASSERT_TRUE(t.effects.empty());
}

static void test_up_from_two_moves_to_one() {
    ReviewState s{.selected_idx = 2, .total = 5};
    auto t = review_step(s, ReviewCmd::Up, false, {});
    ASSERT_EQ(t.next.selected_idx, 1);
    ASSERT_TRUE(t.effects.empty());
}

static void test_down_at_end_stays() {
    ReviewState s{.selected_idx = 4, .total = 5};
    auto t = review_step(s, ReviewCmd::Down, false, {});
    ASSERT_EQ(t.next.selected_idx, 4);
    ASSERT_TRUE(t.effects.empty());
}

static void test_down_from_two_moves_to_three() {
    ReviewState s{.selected_idx = 2, .total = 5};
    auto t = review_step(s, ReviewCmd::Down, false, {});
    ASSERT_EQ(t.next.selected_idx, 3);
    ASSERT_TRUE(t.effects.empty());
}

static void test_play_take_no_take_no_effect() {
    ReviewState s{.selected_idx = 1, .total = 3};
    auto t = review_step(s, ReviewCmd::PlayTake, false, "some/path.wav");
    ASSERT_FALSE(has_effect<PlayReviewTake>(t.effects));
    ASSERT_TRUE(t.effects.empty());
}

static void test_play_take_with_take_emits_effect() {
    std::filesystem::path p{"takes/01.wav"};
    ReviewState s{.selected_idx = 1, .total = 3};
    auto t = review_step(s, ReviewCmd::PlayTake, true, p);
    ASSERT_TRUE(has_effect<PlayReviewTake>(t.effects));
}

static void test_redo_emits_stop_and_navigate() {
    ReviewState s{.selected_idx = 2, .total = 5};
    auto t = review_step(s, ReviewCmd::Redo, false, {});
    ASSERT_TRUE(has_effect<StopReviewPlay>(t.effects));
    ASSERT_TRUE(has_effect<NavigateToRecord>(t.effects));
    ASSERT_FALSE(has_effect<ExitToSession>(t.effects));
}

static void test_back_emits_stop_and_exit() {
    ReviewState s{.selected_idx = 0, .total = 3};
    auto t = review_step(s, ReviewCmd::Back, false, {});
    ASSERT_TRUE(has_effect<StopReviewPlay>(t.effects));
    ASSERT_TRUE(has_effect<ExitToSession>(t.effects));
    ASSERT_FALSE(has_effect<NavigateToRecord>(t.effects));
}

static void test_redo_at_zero_navigates_to_zero() {
    ReviewState s{.selected_idx = 0, .total = 3};
    auto t = review_step(s, ReviewCmd::Redo, false, {});
    ASSERT_TRUE(has_effect<NavigateToRecord>(t.effects));
    for (const auto& e : t.effects) {
        if (std::holds_alternative<NavigateToRecord>(e)) {
            ASSERT_EQ(std::get<NavigateToRecord>(e).idx, 0);
        }
    }
}

static void test_redo_at_three_navigates_to_three() {
    ReviewState s{.selected_idx = 3, .total = 5};
    auto t = review_step(s, ReviewCmd::Redo, false, {});
    ASSERT_TRUE(has_effect<NavigateToRecord>(t.effects));
    for (const auto& e : t.effects) {
        if (std::holds_alternative<NavigateToRecord>(e)) {
            ASSERT_EQ(std::get<NavigateToRecord>(e).idx, 3);
        }
    }
}

static void test_consecutive_up_down() {
    ReviewState s{.selected_idx = 2, .total = 5};
    auto t1 = review_step(s, ReviewCmd::Up, false, {});
    ASSERT_EQ(t1.next.selected_idx, 1);
    auto t2 = review_step(t1.next, ReviewCmd::Down, false, {});
    ASSERT_EQ(t2.next.selected_idx, 2);
    auto t3 = review_step(t2.next, ReviewCmd::Down, false, {});
    ASSERT_EQ(t3.next.selected_idx, 3);
}

static void test_play_take_path_threaded_into_effect() {
    std::filesystem::path p{"takes/042.wav"};
    ReviewState s{.selected_idx = 0, .total = 1};
    auto t = review_step(s, ReviewCmd::PlayTake, true, p);
    ASSERT_TRUE(has_effect<PlayReviewTake>(t.effects));
    for (const auto& e : t.effects) {
        if (std::holds_alternative<PlayReviewTake>(e)) {
            ASSERT_TRUE(std::get<PlayReviewTake>(e).path == p);
        }
    }
}

static void test_parse_up_keys() {
    ASSERT_TRUE(parse_review_cmd("k").has_value());
    ASSERT_EQ(parse_review_cmd("k").value(), ReviewCmd::Up);
    ASSERT_EQ(parse_review_cmd("\x1B[A").value(), ReviewCmd::Up);
}

static void test_parse_down_keys() {
    ASSERT_EQ(parse_review_cmd("j").value(), ReviewCmd::Down);
    ASSERT_EQ(parse_review_cmd("\x1B[B").value(), ReviewCmd::Down);
}

static void test_parse_play_key() {
    ASSERT_EQ(parse_review_cmd("p").value(), ReviewCmd::PlayTake);
}

static void test_parse_redo_keys() {
    ASSERT_EQ(parse_review_cmd("\r").value(), ReviewCmd::Redo);
    ASSERT_EQ(parse_review_cmd("r").value(), ReviewCmd::Redo);
}

static void test_parse_back_keys() {
    ASSERT_EQ(parse_review_cmd("q").value(), ReviewCmd::Back);
    ASSERT_EQ(parse_review_cmd("b").value(), ReviewCmd::Back);
}

static void test_parse_unknown_key() {
    ASSERT_FALSE(parse_review_cmd("z").has_value());
    ASSERT_FALSE(parse_review_cmd("x").has_value());
    ASSERT_FALSE(parse_review_cmd("").has_value());
}

int main() {
    std::printf("=== Review Flow Tests ===\n");

    run_test("up_at_zero_stays",              test_up_at_zero_stays);
    run_test("up_from_two_moves_to_one",      test_up_from_two_moves_to_one);
    run_test("down_at_end_stays",             test_down_at_end_stays);
    run_test("down_from_two_moves_to_three",  test_down_from_two_moves_to_three);
    run_test("play_take_no_take_no_effect",   test_play_take_no_take_no_effect);
    run_test("play_take_with_take_emits",     test_play_take_with_take_emits_effect);
    run_test("redo_emits_stop_and_navigate",  test_redo_emits_stop_and_navigate);
    run_test("back_emits_stop_and_exit",      test_back_emits_stop_and_exit);
    run_test("redo_at_zero_navigates_to_zero",test_redo_at_zero_navigates_to_zero);
    run_test("redo_at_three_navigates_to_three", test_redo_at_three_navigates_to_three);
    run_test("consecutive_up_down",           test_consecutive_up_down);
    run_test("play_take_path_threaded",        test_play_take_path_threaded_into_effect);
    run_test("parse_up_keys",                 test_parse_up_keys);
    run_test("parse_down_keys",               test_parse_down_keys);
    run_test("parse_play_key",                test_parse_play_key);
    run_test("parse_redo_keys",               test_parse_redo_keys);
    run_test("parse_back_keys",               test_parse_back_keys);
    run_test("parse_unknown_key",             test_parse_unknown_key);

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
