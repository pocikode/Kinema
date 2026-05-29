# Suggested Commands (Darwin / macOS)

## Build & Run

### macOS
```bash
brew install opencv glfw glew cmake
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/kinema                   # also produces build/Kinema.app
```

### Windows (PowerShell, vcpkg)
```powershell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
./build/Release/kinema.exe
```

## Clone (w/ Geni submodule)
```bash
git clone --recurse-submodules <repo>
# or after plain clone:
git submodule update --init --recursive
```

## Packaging
```bash
./package.sh                     # macOS → DMG
.\package.bat                    # Windows → NSIS exe
# both wrap: cpack against configured build tree
```

## Version Bump (cuts release on push)
Edit `CMakeLists.txt` line `project(kinema VERSION X.Y.Z)`.

## Lint / Test
**None.** No test target. No lint target. Style enforced via `.clang-format` (clangd handles via `compile_commands.json`).

## Darwin Utilities
- File listing: `ls -la`, `ls -G` (color on macOS)
- Find: `find . -name 'pattern'` (BSD find, no `-printf`)
- Grep: `grep -R --include='*.cpp'` or prefer `rg` (ripgrep)
- Sed: BSD sed — `sed -i '' 's/x/y/' file` (empty backup arg required)
- Open file/app: `open path` / `open -a AppName`
- Clipboard: `pbcopy` / `pbpaste`
- Process: `ps aux`, `lsof -i :PORT`

## Git (project-relevant)
```bash
git status
git log --oneline -20
git submodule status
git submodule update --init --recursive
```

## RTK (token-optimized proxy)
Commands auto-rewritten by hook (transparent). Manual:
```bash
rtk gain                  # token savings analytics
rtk gain --history        # command history w/ savings
rtk proxy <cmd>           # raw exec, no filter (debug)
```
