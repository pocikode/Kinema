# Kinema — Project Overview

## Purpose
Kinema is a desktop C++ application for 3D motion reconstruction. It tracks physical markers via webcam, maps them to bones of a rigged 3D character skeleton, records motion as keyframes, and exports the animation as JSON, MP4/AVI video, or GLB.

## Tech Stack
- **Language**: C++17
- **Build**: CMake 3.15+
- **3D Engine**: Custom `Geni` engine (`external/geni/`) — OpenGL 3.3 Core, GLFW 3, GLEW, GLM, Bullet3, cgltf
- **UI**: Dear ImGui 1.91.0 (fetched via CMake FetchContent), Tokyo Night theme
- **Computer Vision**: OpenCV 4.7+ (`core`, `imgproc`, `videoio`, `highgui`, `objdetect`, `calib3d`)
- **File dialogs**: portable-file-dialogs (`external/pfd/`)
- **Platform**: macOS-first (Darwin), portable C++17

## Source Layout
```
src/
  Application.cpp / Application.h   # Main orchestrator (derives Geni::Application)
  main.cpp
  modules/
    MarkerDetection.h/cpp            # IMarkerDetector base class; owns cv::VideoCapture
    MarkerDetector.h                 # MarkerObservation struct
    HSVMarkerDetector.h/cpp          # HSV color blob detection
    ArucoMarkerDetector.h/cpp        # OpenCV ArUco tag detection + solvePnP
    SkeletonDriver.h/cpp             # Applies observations to Geni::Skeleton (Position/PositionRotation/LookAt/IK)
    MotionRecorder.h/cpp             # Per-bone keyframe recording & JSON v2 export
    VideoExporter.h/cpp              # Offscreen FBO → MP4/AVI via cv::VideoWriter
    GlbExporter.h/cpp                # Rewrites source GLB with new animation track
  ui/
    UIManager.h/cpp                  # ImGui sidebar, camera feed window, axis gizmo
external/
  geni/                              # Vendored 3D engine
  pfd/                               # Portable file dialogs (single-header)
assets/
  shaders/   (unlit.vert/frag, skinned.vert/frag)
  materials/ (unlit.mat, skinned.mat)
  models/    (mixamo_lowpoly/mixamo-animated-lowpoly.glb — default rig)
thesis/      # Academic thesis documents (unrelated to runtime code)
```

## Key Abstractions
- **IMarkerDetector** — shared camera backbone; subclasses implement `Detect()` → `vector<MarkerObservation>`
- **SkeletonDriver** — maps marker observations to bones via `MarkerBinding` (Position, PositionRotation, LookAt, IKTarget two-bone chain)
- **MotionRecorder** — records local-space bone poses as timestamped keyframes; JSON schema v2
- **UIManager / UIState** — ImGui UI; dirty flags (`detectorDirty`, `markersDirty`) are the UI→Application contract
- **Application** — orchestrator: detect → drive skeleton → record → handle export requests each frame
