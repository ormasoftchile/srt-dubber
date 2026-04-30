#!/usr/bin/env bash
# tests/app_harness_test.sh
# Integration tests for app_harness — drives the full AppSession state machine
# via JSON stdin/stdout. Exit 0 = all pass, non-zero = failure.

set -euo pipefail

HARNESS="${1:-$(dirname "$0")/../build/app-harness}"

if [[ ! -x "$HARNESS" ]]; then
  echo "FAIL: app-harness binary not found at $HARNESS"
  echo "      Build with: cmake --build build --parallel"
  exit 1
fi

PASS=0
FAIL=0

check() {
  local label="$1"
  local value="$2"
  local expected="$3"
  if [[ "$value" == "$expected" ]]; then
    echo "  PASS: $label"
    PASS=$((PASS + 1))
  else
    echo "  FAIL: $label"
    echo "        expected: $expected"
    echo "        got:      $value"
    FAIL=$((FAIL + 1))
  fi
}

echo "=== Flow 1: Session → Record → back to Session ==="

# Parse all JSON objects from output
FRAMES=$(printf '%s\n' \
  '{"screen":"nav","input":"GoRecord"}' \
  '{"screen":"recording","input":"r"}' \
  '{"screen":"recording","input":"CountdownComplete"}' \
  '{"screen":"recording","input":"s"}' \
  '{"screen":"recording","input":"n"}' \
  '{"screen":"nav","input":"RecordingDone"}' \
  | "$HARNESS" 2>/dev/null | python3 -c "
import sys, json
text = sys.stdin.read().strip()
# Split into individual JSON objects by counting braces
objects = []
depth = 0
start = 0
for i, ch in enumerate(text):
    if ch == '{': 
        if depth == 0: start = i
        depth += 1
    elif ch == '}':
        depth -= 1
        if depth == 0:
            objects.append(json.loads(text[start:i+1]))
for o in objects:
    print(json.dumps(o))
")

FRAME_COUNT=$(echo "$FRAMES" | wc -l | tr -d ' ')
check "Flow1: frame count (initial+6 turns = 7)" "$FRAME_COUNT" "7"

# Frame 2 (index 1) = after GoRecord → should be on recording screen
F2=$(echo "$FRAMES" | sed -n '2p')
check "Flow1: after GoRecord → screen=recording" \
  "$(echo "$F2" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["nav"]["screen"])')" \
  "recording"

# Frame 3 (index 2) = after 'r' → phase=countdown, effect=StartCountdown
F3=$(echo "$FRAMES" | sed -n '3p')
check "Flow1: after r → phase=countdown" \
  "$(echo "$F3" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["screen_state"]["phase"])')" \
  "countdown"
check "Flow1: after r → StartCountdown effect" \
  "$(echo "$F3" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print("StartCountdown" in d["effects"])')" \
  "True"

# Frame 4 = after CountdownComplete → phase=recording, ActivateCapture
F4=$(echo "$FRAMES" | sed -n '4p')
check "Flow1: after CountdownComplete → phase=recording" \
  "$(echo "$F4" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["screen_state"]["phase"])')" \
  "recording"
check "Flow1: after CountdownComplete → ActivateCapture effect" \
  "$(echo "$F4" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print("ActivateCapture" in d["effects"])')" \
  "True"

# Frame 5 = after 's' (stop) → phase=idle, StopRecording effect
F5=$(echo "$FRAMES" | sed -n '5p')
check "Flow1: after s → phase=idle" \
  "$(echo "$F5" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["screen_state"]["phase"])')" \
  "idle"
check "Flow1: after s → StopRecording effect" \
  "$(echo "$F5" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print("StopRecording" in d["effects"])')" \
  "True"

# Frame 6 = after 'n' (next) → StopPlayback effect, no error
F6=$(echo "$FRAMES" | sed -n '6p')
check "Flow1: after n → no error" \
  "$(echo "$F6" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["error"] is None)')" \
  "True"

# Frame 7 = after RecordingDone → screen=session
F7=$(echo "$FRAMES" | sed -n '7p')
check "Flow1: after RecordingDone → screen=session" \
  "$(echo "$F7" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["nav"]["screen"])')" \
  "session"

echo ""
echo "=== Flow 2: Session → Review → move down → ReviewGoRecord (idx 1) ==="

FRAMES2=$(printf '%s\n' \
  '{"screen":"nav","input":"GoReview"}' \
  '{"screen":"review","input":"j"}' \
  '{"screen":"nav","input":"ReviewGoRecord"}' \
  | "$HARNESS" 2>/dev/null | python3 -c "
import sys, json
text = sys.stdin.read().strip()
objects = []
depth = 0
start = 0
for i, ch in enumerate(text):
    if ch == '{':
        if depth == 0: start = i
        depth += 1
    elif ch == '}':
        depth -= 1
        if depth == 0:
            objects.append(json.loads(text[start:i+1]))
for o in objects:
    print(json.dumps(o))
")

F_REVIEW=$(echo "$FRAMES2" | sed -n '2p')
check "Flow2: after GoReview → screen=review" \
  "$(echo "$F_REVIEW" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["nav"]["screen"])')" \
  "review"

F_DOWN=$(echo "$FRAMES2" | sed -n '3p')
check "Flow2: after j → selected_idx=1" \
  "$(echo "$F_DOWN" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["screen_state"]["selected_idx"])')" \
  "1"

F_GORECORD=$(echo "$FRAMES2" | sed -n '4p')
check "Flow2: after ReviewGoRecord → screen=recording" \
  "$(echo "$F_GORECORD" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["nav"]["screen"])')" \
  "recording"
check "Flow2: after ReviewGoRecord → recording_idx=1" \
  "$(echo "$F_GORECORD" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["nav"]["recording_idx"])')" \
  "1"

echo ""
echo "=== Flow 3: Session → Assemble → Start → AssemblyDone → AssembleDone nav ==="

FRAMES3=$(printf '%s\n' \
  '{"screen":"nav","input":"GoAssemble"}' \
  '{"screen":"assemble","input":"Start"}' \
  '{"screen":"assemble","input":"AssemblyDone"}' \
  '{"screen":"nav","input":"AssembleDone"}' \
  | "$HARNESS" 2>/dev/null | python3 -c "
import sys, json
text = sys.stdin.read().strip()
objects = []
depth = 0
start = 0
for i, ch in enumerate(text):
    if ch == '{':
        if depth == 0: start = i
        depth += 1
    elif ch == '}':
        depth -= 1
        if depth == 0:
            objects.append(json.loads(text[start:i+1]))
for o in objects:
    print(json.dumps(o))
")

F_ASM=$(echo "$FRAMES3" | sed -n '2p')
check "Flow3: after GoAssemble → screen=assemble" \
  "$(echo "$F_ASM" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["nav"]["screen"])')" \
  "assemble"

F_START=$(echo "$FRAMES3" | sed -n '3p')
check "Flow3: after Start → phase=assembling" \
  "$(echo "$F_START" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["screen_state"]["phase_label"])')" \
  "assembling"
check "Flow3: after Start → SpawnAssembly effect" \
  "$(echo "$F_START" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print("SpawnAssembly" in d["effects"])')" \
  "True"

F_DONE=$(echo "$FRAMES3" | sed -n '4p')
check "Flow3: after AssemblyDone → phase=complete" \
  "$(echo "$F_DONE" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["screen_state"]["phase_label"])')" \
  "complete"

F_NAVDONE=$(echo "$FRAMES3" | sed -n '5p')
check "Flow3: after AssembleDone nav → screen=session" \
  "$(echo "$F_NAVDONE" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["nav"]["screen"])')" \
  "session"

echo ""
echo "=== Flow 4: Invalid commands ==="

# Wrong screen: send recording command while on session screen
WRONG_SCREEN=$(printf '%s\n' \
  '{"screen":"recording","input":"r"}' \
  | "$HARNESS" 2>/dev/null | python3 -c "
import sys, json
text = sys.stdin.read().strip()
objects = []
depth = 0
start = 0
for i, ch in enumerate(text):
    if ch == '{':
        if depth == 0: start = i
        depth += 1
    elif ch == '}':
        depth -= 1
        if depth == 0:
            objects.append(json.loads(text[start:i+1]))
for o in objects:
    print(json.dumps(o))
" | tail -1)

check "Flow4: wrong screen → error not null" \
  "$(echo "$WRONG_SCREEN" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["error"] is not None)')" \
  "True"
check "Flow4: wrong screen → state unchanged (still session)" \
  "$(echo "$WRONG_SCREEN" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["nav"]["screen"])')" \
  "session"

# Unknown nav command
UNKNOWN_CMD=$(printf '%s\n' \
  '{"screen":"nav","input":"FlyToMoon"}' \
  | "$HARNESS" 2>/dev/null | python3 -c "
import sys, json
text = sys.stdin.read().strip()
objects = []
depth = 0
start = 0
for i, ch in enumerate(text):
    if ch == '{':
        if depth == 0: start = i
        depth += 1
    elif ch == '}':
        depth -= 1
        if depth == 0:
            objects.append(json.loads(text[start:i+1]))
for o in objects:
    print(json.dumps(o))
" | tail -1)

check "Flow4: unknown nav command → error not null" \
  "$(echo "$UNKNOWN_CMD" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["error"] is not None)')" \
  "True"
check "Flow4: unknown nav command → state unchanged" \
  "$(echo "$UNKNOWN_CMD" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["nav"]["screen"])')" \
  "session"

# JSON parse error
BAD_JSON=$(printf '%s\n' \
  'not valid json {{{' \
  | "$HARNESS" 2>/dev/null | python3 -c "
import sys, json
text = sys.stdin.read().strip()
objects = []
depth = 0
start = 0
for i, ch in enumerate(text):
    if ch == '{':
        if depth == 0: start = i
        depth += 1
    elif ch == '}':
        depth -= 1
        if depth == 0:
            objects.append(json.loads(text[start:i+1]))
for o in objects:
    print(json.dumps(o))
" | tail -1)

check "Flow4: bad JSON → error contains 'JSON parse error'" \
  "$(echo "$BAD_JSON" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print("JSON parse error" in (d["error"] or ""))')" \
  "True"

echo ""
echo "=== Flow 5: Quit ==="

QUIT_FRAMES=$(printf '%s\n' \
  '{"screen":"nav","input":"Quit"}' \
  | "$HARNESS" 2>/dev/null | python3 -c "
import sys, json
text = sys.stdin.read().strip()
objects = []
depth = 0
start = 0
for i, ch in enumerate(text):
    if ch == '{':
        if depth == 0: start = i
        depth += 1
    elif ch == '}':
        depth -= 1
        if depth == 0:
            objects.append(json.loads(text[start:i+1]))
for o in objects:
    print(json.dumps(o))
")

QUIT_FRAME=$(echo "$QUIT_FRAMES" | tail -1)
check "Flow5: Quit → running=false" \
  "$(echo "$QUIT_FRAME" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d["nav"]["running"])')" \
  "False"
check "Flow5: Quit → exit=true" \
  "$(echo "$QUIT_FRAME" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d.get("exit", False))')" \
  "True"

echo ""
echo "=== Results ==="
echo "PASS: $PASS  FAIL: $FAIL"

if [[ "$FAIL" -gt 0 ]]; then
  exit 1
fi
exit 0
