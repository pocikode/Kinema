# Kinema Implementation Progress (2026-04-17)

## Completed (3 of 5 phases)

### Phase 1: Engine modifications (external/geni) ✅
All changes committed to geni submodule:
- Fixed GLFW_FALSE → GLFW_RELEASE in key/mouse release callbacks
- Added `virtual void Render()` hook to Application.h, called in Engine.cpp after renderQueue.Draw()
- Added `GetWindow()` and `GetFramebufferSize()` to Engine public API
- Added scroll wheel support: InputManager tracks scroll delta, Engine has scroll callback
- Added `Mesh::CreateLines()` factory with GL_LINES draw mode for grid/axes
- New `OrbitCameraComponent` with arcball orbit (left-drag rotate, right-drag pan, scroll zoom)
- Updated geni CMakeLists.txt: added OrbitCameraComponent.cpp, made glm/json PUBLIC exports

**Build status:** geni compiles cleanly

### Phase 2: Kinema CMakeLists + assets ✅
- Replaced `/Users/agus/work/project/kinema/CMakeLists.txt` with expanded version
- Added OpenCV find_package (requires `brew install opencv` — status: installing)
- Added ImGui via FetchContent (v1.91.0)
- Created assets/shaders/{unlit.vert, unlit.frag}
- Created assets/materials/unlit.mat

### Phase 3: Core modules ✅
Fully implemented:
- `src/modules/MarkerDetector.h/.cpp` — OpenCV VideoCapture, HSV filtering, centroid extraction
- `src/modules/MotionRecorder.h/.cpp` — keyframe recording, JSON export via nlohmann
- `src/modules/VideoExporter.h/.cpp` — offscreen FBO rendering, OpenCV VideoWriter export

## Remaining (2 of 5 phases)

### Phase 4: UIManager and Application rewrite ⏳
**Still needed:**
- `src/ui/UIManager.h/.cpp` — ImGui integration (all panels: menu, marker config, recording control, export settings, status bar, camera feed)
- Rewrite `src/Application.h/.cpp` — complete orchestrator, SetupScene() with grid/axes/camera/light/target box, Update() loop with marker detection and playback

**Key implementation details:**
- UIState struct holds panel state (HSV range, recording flags, export settings, camera texture)
- BuildUI() called each frame before ImGui::Render()
- Render() called after renderQueue.Draw(), before glfwSwapBuffers()
- Camera feed uploaded to OpenGL texture via glTexImage2D from cv::Mat
- 2D→3D marker mapping: worldX/Y = (centroid - 0.5) * 2 * (worldZ / Z_REF); worldZ = Z_REF * sqrt(A_REF / areaPixels)
- Grid: 20×20 squares from -10 to +10 units on XZ plane
- Axes: X=red, Y=green, Z=blue, length 1.5 units

### Phase 5: Documentation (CLAUDE.md) ⏳
- Build instructions
- OpenCV prerequisite note
- Architecture overview

## Critical Files Modified
- `/Users/agus/work/project/kinema/external/geni/src/core/Engine.cpp` — Render hook, scroll callback, GetWindow/GetFramebufferSize impl
- `/Users/agus/work/project/kinema/external/geni/include/core/Application.h` — Render() virtual
- `/Users/agus/work/project/kinema/external/geni/src/render/Mesh.cpp` — CreateLines(), m_drawMode
- `/Users/agus/work/project/kinema/external/geni/CMakeLists.txt` — PUBLIC glm/json, OrbitCameraComponent.cpp

## Next Steps
1. Wait for `brew install opencv` to complete
2. Implement UIManager with all ImGui panels
3. Rewrite Application with complete scene setup and main loop
4. Test build (cmake configure + build)
5. Write CLAUDE.md documentation

## Notes
- OpenCV install is currently running in background (task ID: byt0ks9ll)
- Plan file saved at: /Users/agus/.claude/plans/lest-implement-this-project-nifty-honey.md
- All code follows C++17, uses GLM for math, glm::quat for rotations
- ImGui integration checks `ImGui::GetIO().WantCaptureMouse` to avoid orbit camera input conflicts
