# Steven — History

## Core Context
- **Project:** srt-dubber — a local C++20 TUI tool for recording voice-over takes per subtitle slot and assembling them into a dubbed video track
- **Owner:** ormasoftchile
- **Tech stack:** C++20, FTXUI (TUI), miniaudio (single-header audio), ffmpeg (CLI invocation), std::filesystem
- **Steven's role:** TUI Developer — owns all FTXUI screens and key handling; calm, minimal, keyboard-driven design
- **Key collaborators:** Alan (architecture), Butch (audio), Richard (build)
- **Design philosophy:** dark background, soft contrast, generous spacing, no clutter, no animations

## Learnings

### FTXUI v5 Component model
- Each screen creates its own `ScreenInteractive::Fullscreen()`, runs `screen.Loop(component)`, then returns a `ScreenAction` enum. This avoids complex state sharing across a single global screen.
- `CatchEvent(renderer, fn)` wraps a `Renderer` to intercept keyboard events before FTXUI handles them.
- `screen.ExitLoopClosure()()` is the idiomatic way to exit the loop from within an event handler.
- `std::jthread` (C++20) is ideal for the recording timer thread — its destructor automatically requests stop and joins, so no manual cleanup is needed.
- `screen.PostEvent(Event::Custom)` is safe to call from background threads; it queues a redraw without blocking.

### Layout conventions used
- `border(vbox({...}))` as the root element for each screen — clean single-line border.
- `separator()` between logical sections (top bar / body / hints).
- `filler()` in `hbox` to right-align items or push content to edges.
- `paragraphAlignCenter(text) | bold` for the subtitle display in Screen 2.
- `yframe(vbox(rows))` + `focus(row)` for scrollable lists (Screen 3) — FTXUI auto-scrolls to keep the focused element visible.
- `inverted(row)` for selected row highlight — no color dependency, works on any terminal.

### Data model integration
- `core::Project::entries()` is the single source of truth; all screen state reads from it.
- `raw_take_path` being a `std::string` (not `std::filesystem::path`) is intentional for JSON serialisation — implicit `path` construction from `string` handles playback calls.
- Added `display_name()`, `assemble_log`, `assemble_complete`, and `output_path` to `core::Project` as the minimum TUI contract. These are marked as transient (not persisted).
- Recording status stays `pending` after a raw take — only ffmpeg processing (Butch) upgrades it to `ok`/`stretched`/`overflow`.

### Screen 2 (Recording) timer design
- A background `std::jthread` posts `Event::Custom` every 100 ms.
- The renderer queries `recorder.elapsed_ms()` and `recorder.is_recording()` directly on each redraw — no shared atomic state to manage.
- The REC indicator (`●REC`) and elapsed counter only render when `is_recording()` is true.

## Wave 1 Complete — 2025-04-14

**TUI Delivered.** All four screens (session, recording, review, assembly) completed and integrated with core project model. ADR merged. ~3900 lines FTXUI component code + 3 transient Project fields.

**Wave 2 in progress:**
- **Alan** is auditing integration points: module boundaries, data flow (SRT → project → TUI), state persistence (TUI changes → project.json).
- **Richard** is attempting the first macOS native build; will resolve any linker/header issues and smoke-test.

**Next sprint:** Integration validation, then first cross-platform builds (Linux, Windows). Assembly triggering logic TBD.

