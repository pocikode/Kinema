# Kinema

**Kinema** is a real-time marker-based motion capture desktop application. It tracks colored markers through a webcam using chromaticity-based (RGB-ratio) color detection and maps their motion onto the bones of a rigged 3D model in real time. Captured motion can be replayed, and exported as JSON keyframes, video, or a GLB with the animation embedded.

## Key Features

- **RGB-Ratio (Chromaticity) Marker Detection**: Each pixel is normalized into ratios `r = R/(R+G+B)`, `g = G/(R+G+B)`, `b = B/(R+G+B)`, making detection far more robust to lighting and brightness changes than raw color thresholding. The frame is average-pooled before classification to suppress noise.
- **Eyedropper Color Picker**: Click your marker in the live camera feed and the detection range is derived automatically — no manual slider tuning required.
- **Real-time Skeleton Driving**: Bind each marker to a bone as *Position*, *IK Target* (analytical 2-bone IK), *Look At* (rotation only), or *Hint* (arm-direction reference for an IK chain).
- **Marker Pairing**: Two markers sharing one color (e.g. left and right palms) are resolved as the largest and second-largest blob, with left/right disambiguation.
- **Stabilization & Occlusion Hold**: Anti-jitter filtering (deadzone + EMA) plus short extrapolation when a marker is briefly occluded.
- **Depth Estimation with One-Click Calibration**: Marker depth (Z) is estimated from blob area; calibrate directly from the UI.
- **Recording & Playback**: Record motion sequences and preview them immediately on the 3D rig (*Reconstruct 3D*).
- **Flexible Export**:
  - **JSON** — per-bone local-space keyframes (schema v2).
  - **Video** — re-render the motion as MP4 or AVI.
  - **GLB** — bake the recorded animation back into the source GLB file.
- **Marker Config Save/Load**: Export and re-import the full marker setup (color ranges + bone bindings) as JSON.

---

## Building

### Prerequisites

- **CMake** 3.15+
- **C++17 compiler** (Clang on macOS, MSVC on Windows)
- **Webcam**
- The repository must be cloned **with submodules** (the in-tree Geni engine lives in `external/geni`):

```bash
git clone --recurse-submodules <repo-url>
# or, if already cloned without submodules:
git submodule update --init --recursive
```

### macOS

1. Install dependencies:
   ```bash
   brew install opencv glfw glew cmake
   ```
2. Configure and build:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --parallel
   ```
3. Run:
   ```bash
   ./build/kinema
   ```
   > On first launch macOS asks for camera permission (System Settings → Privacy & Security → Camera). Once granted, the app retries camera initialization automatically.

### Windows

Dependencies are managed with **vcpkg** (see `vcpkg.json`: glfw3, glew, opencv4).

1. Install vcpkg:
   ```powershell
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   ```
2. Configure and build (from the Kinema root directory):
   ```powershell
   cmake -B build -S . "-DCMAKE_TOOLCHAIN_FILE=[path/to/vcpkg]/scripts/buildsystems/vcpkg.cmake"
   cmake --build build --config Release
   ```
3. Run:
   ```powershell
   .\build\Release\kinema.exe
   ```

> Always use a **Release** build — Debug builds significantly reduce detection FPS.

---

## Quick Start

1. **Launch the app.** The bundled Mixamo rig loads automatically, along with 7 default marker slots (head + upper-arm/lower-arm/palm for each arm).
2. **Sample your marker colors.** In the **Markers** panel, click **Pick color (eyedropper)** on a slot, then click your physical marker in the *Camera Feed* window. The chromaticity range fills in automatically.
3. **Record.** Open the **Recording** panel, click **START RECORDING**, perform your motion, then **STOP RECORDING**.
4. **Preview.** Click **RECONSTRUCT 3D** to replay the recorded motion on the model.
5. **Export.** Save the result as JSON, video (MP4/AVI), or GLB from the **Export** panel.

For a detailed walkthrough of every panel, the RGB-ratio detection model, depth calibration, and troubleshooting, see the **[User Guide (USAGE.md)](USAGE.md)**.

---

## Troubleshooting (Quick)

- **No camera?** Make sure the webcam isn't in use by another app. On macOS, check camera permission in System Settings.
- **Marker not detected?** Use **Pick color (eyedropper)** and click the marker directly in the camera feed. Ensure adequate lighting.
- **Low FPS?** Use a **Release** build and raise *Pool size* in the Detector panel.

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.
