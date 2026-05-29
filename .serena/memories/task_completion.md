# Task Completion Checklist

No test target. No lint target. So:

## After Code Changes
1. **Build**: `cmake --build build --parallel` — must succeed clean (no new warnings).
2. **Run**: launch `./build/kinema` (macOS) / `./build/Release/kinema.exe` (Windows) and exercise the changed path. For UI/feature changes: open in actual app, test golden path + edge cases, watch for regressions in other features.
3. **Format**: code must match `.clang-format`. clangd auto-applies if editor configured.
4. **Verify success criteria** stated up-front (bug fix → reproducing test passes; feature → manual exercise works).

## Cleanup
- Remove imports / vars / helpers orphaned by your change.
- Pre-existing dead code: mention to user, do NOT delete unsolicited.
- No reformatting of adjacent untouched code.
- No comments added unless WHY is non-obvious.

## Release Cut (if user requests)
- Bump `project(kinema VERSION X.Y.Z)` in `CMakeLists.txt`.
- Push → CI publishes `v<version>-<shortsha>` GitHub release w/ macOS DMG + Windows NSIS.

## Reporting Back
- If UI/feature change couldn't be tested in browser/app: say so explicitly. Do NOT claim success based on build alone.
- Type checking + test suites verify correctness, not feature correctness.
- End-of-turn summary: 1-2 sentences, what changed + what's next.
