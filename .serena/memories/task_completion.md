# Kinema — What to Do When a Task is Completed

## After Code Changes
1. **Format**: run `clang-format -i` on all modified `.cpp` / `.h` files
2. **Build**: `cmake --build build --parallel` — fix any compile errors before considering done
3. **Manual test**: verify the relevant scenario from the checklist in CLAUDE.md:
   - Window opens with Tokyo Night theme
   - Default Mixamo rig loads (bone count > 0)
   - Orbit camera works
   - Relevant feature (detection / recording / export) functions correctly
   - No regressions in other panels

## No Automated Tests
There is no test suite. Verification is entirely manual via the running application.

## Exports to Verify
- **JSON**: check file contains `"version": 2` and per-bone pose data
- **Video**: confirm MP4/AVI written with correct frame count
- **GLB**: open in a glTF viewer (e.g., Babylon.js sandbox) — new `Recorded` animation should appear

## Common Build Pitfalls
- `pkg-config --cflags --libs opencv4` must succeed; if not, `brew install opencv`
- Missing GLFW/GLEW: `brew install glfw glew`
- Stale build: `rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel`
