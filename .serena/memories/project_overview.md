# Kinema — Project Overview

Real-time markerless-style motion capture desktop app, C++17.

## Pipeline
Webcam → OpenCV HSV color-blob detection → 2D blobs unprojected to 3D → drive bones of rigged GLB (Mixamo-compatible) loaded into in-tree **Geni** engine (OpenGL + GLFW + GLEW + ImGui).

Captured motion: replayable, exportable as JSON keyframes, video (MP4/AVI), or re-baked into GLB.

## Entry Point
`src/main.cpp` constructs `Application` (subclass of `Geni::Application`) → `Geni::Engine` singleton owns GLFW window + render loop.

## Per-Frame Coordinator (`Application::Update`)
1. `m_detector->Detect()` — `HSVMarkerDetector` grabs webcam frame, HSV-thresholds each `HSVRange`, emits one `MarkerObservation` per slot (`id = slot index`).
2. `SkeletonDriver::Apply` — walks `MarkerBinding`s (Position/LookAt) + `IKChain`s (analytical 2-bone IK), mutates skeleton joint GameObjects. Bind-pose geometry sampled lazily on first apply, cached on chain.
3. If recording: `MotionRecorder::AddSkeletonKeyframe` samples local-space bone poses (re-parenting-safe).
4. `UIManager::BuildUI` renders ImGui sidebar against `UIState`. Flags on `UIState` (`loadModelRequested`, `markersDirty`, `startRecordRequested`, `exportGlbRequested`...) are the contract — UI sets flags, `Application` drains + calls module.

## Important Seams
- **UIManager ↔ SkeletonDriver translation**: UI uses `MarkerSlot` w/ `BindingKind` (Position/IKTarget/LookAt). `Application::RebuildBindingsFromState` translates to parallel `MarkerBinding` + `IKChain` lists. Slot identity = vector index = `MarkerObservation::id`. Reordering reshuffles identities.
- **2D → 3D unprojection**: `Application::Unproject2DtoWorld` uses normalized centroid + blob `areaPixels` against `m_depthRefDist`/`m_depthRefArea` as depth proxy. Driver receives this as `std::function<glm::vec3(MarkerObservation&)>` callback — math stays in Application.
- **Geni boundary**: everything Geni (Scene, GameObject, Skeleton, Material, Mesh, AnimationComponent) via single `<geni.h>` umbrella header. Bones are GameObjects under `Geni::Skeleton`; drive bone = write transform component → Geni propagates skinning matrices.
- **Exporters**: `MotionRecorder::SaveToJson` writes v2 schema (per-bone local pose map). `VideoExporter` re-renders viewport frames into OpenCV `VideoWriter`. `GlbExporter` bakes recorded keyframes back into source GLB.
