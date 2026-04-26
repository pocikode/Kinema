# Kinema — Code Style & Conventions

## Formatting (enforced via `.clang-format`)
- **Indent**: 4 spaces, no tabs
- **Column limit**: 120
- **Brace style**: Custom Allman-like — braces on their own line for classes, functions, control flow (`BreakBeforeBraces: Custom`, `AfterFunction: true`, `AfterClass: true`, `AfterControlStatement: Always`)
- **Pointer/reference alignment**: Right (`int* p`, `int& r`)
- **Access modifier offset**: -2 (dedented from class body)
- Short functions, enums, if-statements NOT collapsed to single line
- Trailing comments aligned

## Naming Conventions (inferred from codebase)
- **Classes/Structs**: PascalCase (e.g., `HSVMarkerDetector`, `MarkerObservation`, `UIState`)
- **Methods/Functions**: PascalCase (e.g., `GetLastFrame()`, `AddSkeletonKeyframe()`)
- **Member variables**: `m_` prefix + camelCase (e.g., `m_detector`, `m_skeleton`)
- **Local variables**: camelCase
- **Constants / enums**: PascalCase enum values (e.g., `DetectorKind::HSV`, `BindingMode::LookAt`)
- **File pairs**: `ClassName.h` / `ClassName.cpp` matching the class name

## C++ Style
- C++17 standard
- No RTTI / exceptions style (engine layer is low-level)
- Use `override` on virtual method overrides
- Prefer `const` references for read-only parameters
- Smart pointers (`std::unique_ptr`) for ownership; raw pointers for non-owning refs
- No docstrings/comments by default; add only for non-obvious invariants or workarounds

## Project-Specific Patterns
- **Dirty flags**: `UIState` uses `*Dirty` bool fields as the UI→Application contract; Application consumes and clears them each `Update()`
- **Module init/destroy**: modules use `Init()` / `Destroy()` (not constructors/destructors) to match the engine lifecycle
- **Lazy caching**: `SkeletonDriver` caches bind-pose geometry on first use to avoid drift
- **Local-space poses**: `MotionRecorder` always records local-space bone transforms for re-parenting safety
