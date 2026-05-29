# Code Style & Conventions

## Formatting
- `.clang-format` is authoritative — read before reformatting.
- Indent: 4 spaces (no tabs). `tab_width=4`.
- Encoding: UTF-8, LF line endings, final newline required (`.editorconfig`).
- AccessModifierOffset: -2.
- AlignAfterOpenBracket: BlockIndent.
- No lint target. No test target.

## Naming
- Classes: `PascalCase` (`Application`, `SkeletonDriver`, `HSVMarkerDetector`).
- Members: `m_` prefix (`m_detector`, `m_depthRefDist`, `m_depthRefArea`).
- Methods: `PascalCase` (`Detect`, `Apply`, `BuildUI`, `RebuildBindingsFromState`).
- Free fns / locals: `camelCase`.
- Enums / nested types: `PascalCase` (`BindingKind`, `MarkerObservation`, `MarkerSlot`).

## Header Style
- `#pragma once` (not include guards).
- Forward-declare where possible; Geni via `<geni.h>` umbrella.
- Public interface in `.h`, impl in `.cpp`.

## Conventions (from CLAUDE.md)
- Touch only what the task requires. No reformatting adjacent code. No "improve" comments. Match existing style even when you'd write it differently.
- State assumptions before coding. Two interpretations exist → surface, don't pick silently.
- Changes orphan import/var/helper → remove. Pre-existing dead code → mention, don't delete.
- Minimum code that solves problem. No speculative abstractions. No error handling for impossible scenarios. No flexibility not asked for.
- Define success criteria up-front. Bug → write reproducing test, then fix. Multi-step task → brief plan w/ verify step per item.

## Comments
- Default: no comments. Only when WHY is non-obvious (hidden constraint, subtle invariant, workaround for specific bug).
- Don't explain WHAT (well-named identifiers do that).
- Don't reference current task/fix/callers (belongs in PR description; rots fast).
