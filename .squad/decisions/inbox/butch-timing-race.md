### 2026-04-14: Capture timing race + warmup margin
**By:** Butch  
**What:** set_capture_active(true) now called before CountdownState::Go. kWarmupSamples increased to 2500ms.
**Why:** Race between Go! display and capture activation caused first-word loss. 1.5s warmup insufficient for AirPods Max HFP on Mac mini.
