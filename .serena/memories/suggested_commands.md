# Kinema — Suggested Commands

## Build
```bash
# Configure (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build (parallel)
cmake --build build --parallel

# Clean rebuild
rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel
```

## Run
```bash
./build/kinema
```
Assets are auto-copied to `build/assets/` by CMake POST_BUILD.

## Format (clang-format)
```bash
# Format a single file
clang-format -i src/Application.cpp

# Format all source files
find src -name '*.cpp' -o -name '*.h' | xargs clang-format -i
```
Config: `.clang-format` at repo root (120-col limit, 4-space indent, custom brace style).

## Dependencies (macOS)
```bash
brew install opencv      # OpenCV 4.7+
# GLFW and GLEW via pkg-config (brew install glfw glew)
```

## Linting / Static Analysis
No automated linter configured. Use `clangd` (`.clangd` config at repo root) for IDE-based diagnostics.

## Testing
No automated test suite. Manual testing per the checklist in CLAUDE.md (window opens, rig loads, orbit camera, detection, recording, export).

## Useful Git / System Commands
```bash
git status / git log / git diff
ls / find / grep
pkg-config --cflags --libs opencv4   # verify OpenCV linkage
```
