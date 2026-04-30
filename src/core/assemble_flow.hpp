#pragma once
#include "effects.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace core {

// ─── Commands ──────────────────────────────────────────────────────────────

enum class AssembleCmd {
    Start,           // user-triggered or auto: begin assembly
    ProgressLine,    // assembly thread posted a log line
    AssemblyDone,    // assembly thread completed successfully
    AssemblyFailed,  // assembly thread reported an error
    Back,            // 'q' — return to session (allowed at any phase)
};

std::optional<AssembleCmd> parse_assemble_cmd(const std::string& key);

// ─── State ─────────────────────────────────────────────────────────────────

enum class AssemblePhase { Idle, Assembling, Complete, Failed };

struct AssembleState {
    AssemblePhase            phase      = AssemblePhase::Idle;
    std::vector<std::string> log_lines; ///< accumulated progress lines
    std::string              output_path;
    std::string              error_msg;
};

// ─── Effects ───────────────────────────────────────────────────────────────

/// Caller must spawn the assembly background thread.
struct SpawnAssembly {};

/// Append a line to the log display.
struct AppendLog { std::string line; };

using AssembleEffect = std::variant<
    SpawnAssembly,
    AppendLog,
    SaveProject,     // from effects.hpp — on successful completion
    ExitToSession    // from effects.hpp
>;

// ─── Transition ────────────────────────────────────────────────────────────

struct AssembleTransition {
    AssembleState               next;
    std::vector<AssembleEffect> effects;
};

/// Pure assemble flow state machine.
/// \param state    current assemble state
/// \param cmd      command (Start / ProgressLine / AssemblyDone / AssemblyFailed / Back)
/// \param payload  extra data for ProgressLine (the log string) / AssemblyDone (output path) / AssemblyFailed (error msg)
AssembleTransition assemble_step(AssembleState       state,
                                  AssembleCmd         cmd,
                                  const std::string&  payload = "");

// ─── Render state ──────────────────────────────────────────────────────────

struct AssembleRenderState {
    std::string              phase_label;   // "idle" | "assembling" | "complete" | "failed"
    std::vector<std::string> log_lines;
    std::string              footer;        // status message for the bottom bar
    bool                     allow_back;    // 'q' hint visible
};

AssembleRenderState assemble_render_state(const AssembleState& state);

} // namespace core
