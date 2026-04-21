# Kinema — User Guide

Kinema tracks colored marker stickers or ArUco fiducial tags via webcam, drives a rigged 3D skeleton in real-time, records the motion, and lets you export results as JSON or video.

---

## Requirements

- A webcam connected to your computer
- **HSV mode** — a bright, solid-colored physical marker (sticker, tape, or painted dot)
  - Best colors: **orange, green, yellow, or red**
  - Avoid: white, black, grey, skin tones (too little saturation)
- **ArUco mode** — printed ArUco fiducial tags (any dictionary; 5 cm default)
- A rigged GLB/glTF model with a compatible skeleton (Mixamo models work out of the box)
- macOS with GLFW, GLEW, and OpenCV installed

---

## Launch

```bash
./build/kinema
```

The window opens with:
- **3D viewport** — grid, coordinate axes, and the loaded rigged model (once loaded)
- **Left sidebar** — five collapsible panels: Model, Detector, Markers, Recording, Export
- **Bottom-right** — live camera feed with per-marker overlay circles
- **Top-right** — XYZ orientation gizmo (rotates with camera)
- **Bottom bar** — FPS, recording state, marker detection status

---

## Interface Overview

```
┌──────────────────────────────────────────────────┐
│  File  [menu bar]                          [gizmo]│
├──────────────┬───────────────────────────────────┤
│ ▼ Model      │                                   │
│──────────────│         3D Viewport               │
│ ▼ Detector   │      (rigged model here)          │
│──────────────│                                   │
│ ▼ Markers    │                                   │
│──────────────│                    ┌─────────────┐│
│ ▶ Recording  │                    │ Camera Feed ││
│──────────────│                    │ + overlays  ││
│ ▶ Export     │                    └─────────────┘│
├──────────────┴───────────────────────────────────┤
│ FPS: 60.0 | Idle | Markers: none       [status] │
└──────────────────────────────────────────────────┘
```

---

## Step-by-Step Workflow

### 1. Load a Rigged Model

In the **Model** panel:

1. Enter the path to a GLB/glTF file with a skeleton (e.g. `models/mixamo_lowpoly/mixamo-animated-lowpoly.glb`)
2. Click **Load Model**

Once loaded, the panel shows the model path and the total number of detected bones. The bone list becomes available for binding in the Markers panel.

> If you skip this step, marker detection still works but no skeleton is driven.

---

### 2. Choose a Detector

In the **Detector** panel, select either:

| Mode | Best for | Notes |
|------|----------|-------|
| **HSV** | Colored stickers/tape | Fast; no orientation |
| **ArUco** | Printed fiducial tags | Provides 6-DOF pose (position + rotation) |

**ArUco-specific settings:**

| Setting | Description |
|---------|-------------|
| Dictionary | Tag family printed on your tags (e.g. `DICT_4X4_50`) |
| Marker (m) | Physical side length of the printed tag in meters (default 0.05 m = 5 cm) |

---

### 3. Configure Marker Slots

In the **Markers** panel, click **+ Add marker** for each physical marker you want to track. Each slot has:

**Identification (HSV mode)**

| Slider | What it does |
|--------|-------------|
| H min / H max | Select the hue range (see table below) |
| S min / S max | Filter out dull/grey areas — raise Min to ~80 |
| V min / V max | Filter out very dark areas — raise Min to ~50 |

**Hue reference (OpenCV HSV, range 0–179):**

| Color | Hue Min | Hue Max |
|-------|---------|---------|
| Red   | 0       | 10 (and 165–179) |
| Orange | 5      | 25      |
| Yellow | 22     | 38      |
| Green  | 35     | 85      |
| Blue   | 90     | 130     |
| Purple | 130    | 160     |

**Identification (ArUco mode)**

| Field | Description |
|-------|-------------|
| Tag id | Numeric ID printed on the ArUco tag |

**Bone binding**

| Field | Description |
|-------|-------------|
| Name | Label shown in camera feed overlay |
| Color | Overlay circle color in camera feed |
| Binding | How the marker drives the skeleton (see below) |
| Bone | Target bone (dropdown populated after model load) |

**Binding modes:**

| Mode | Description |
|------|-------------|
| Position | Moves the bone to the unprojected marker position; rotation unchanged |
| Position + Rotation | Moves and rotates the bone (ArUco only — requires orientation data) |
| IK Target (2-bone) | Marker is the end-effector; two-bone analytical IK poses root → mid → end chain |

For **IK Target**, three bones must be selected: **IK Root** (e.g. shoulder), **IK Mid** (e.g. elbow), **End** (e.g. hand).

When a marker is detected, its slot shows a green **detected** indicator. The camera feed shows a colored overlay circle at the marker's position.

> **Tip:** Start with a narrow Hue range and widen if detection is unstable.

---

### 4. Navigate the 3D Viewport

| Action | Control |
|--------|---------|
| Rotate | Left mouse button + drag |
| Pan | Right mouse button + drag |
| Zoom | Scroll wheel |

The **XYZ gizmo** (top-right) reflects the current camera orientation:
- **Red** = X axis
- **Green** = Y axis
- **Blue** = Z axis

---

### 5. Record Motion

1. Ensure at least one marker shows **detected**
2. Open the **Recording** panel and click **START RECORDING**
3. Move the markers in front of the camera
4. Click **STOP RECORDING**

The panel shows elapsed time and keyframe count in real time.

---

### 6. Reconstruct & Preview

After stopping, click **RECONSTRUCT 3D** to replay the recorded motion on the skeleton in the 3D viewport.

---

### 7. Export Results

In the **Export** panel:

| Field | Description |
|-------|-------------|
| Output Format | MP4 (recommended) or AVI |
| FPS | Frames per second for the exported video (1–60) |
| Output Path | File name without extension (e.g. `output`, `my_recording`) |

Then:
- **SAVE JSON** — writes `<path>.json` with all keyframes (position + rotation + timestamp)
- **EXPORT VIDEO** — renders each keyframe and writes `<path>.mp4` or `<path>.avi`

These actions are also accessible via **File → Save JSON** and **File → Export Video** in the menu bar.

**JSON format example:**
```json
{
  "keyframes": [
    { "t": 0.000, "px": 0.12, "py": 0.05, "pz": 1.98, "qw": 1, "qx": 0, "qy": 0, "qz": 0 },
    { "t": 0.033, "px": 0.15, "py": 0.06, "pz": 1.95, "qw": 1, "qx": 0, "qy": 0, "qz": 0 }
  ]
}
```

---

## Troubleshooting

**Marker not detected / status shows "none"**
- Move the marker directly in front of the camera
- Adjust H min/max to match your marker color (use the table above)
- Increase lighting — poor light reduces saturation
- Raise V min if the room is bright, lower it if dim

**ArUco marker not detected**
- Ensure the printed tag matches the selected dictionary
- Check the **Marker (m)** field matches your printed tag's physical size
- Avoid glare and motion blur — ArUco decoding needs clear corners

**Bone dropdown is empty**
- Load a model first via the **Model** panel; bones are unavailable until a model is loaded

**Skeleton doesn't move**
- Confirm the bone name in the slot matches the skeleton's bone name exactly
- For IK Target, all three bones (root, mid, end) must be set

**Camera feed shows "Camera not available"**
- Check that your webcam is connected and not in use by another app
- On macOS, grant camera permission in System Preferences → Privacy → Camera
- The app tries camera index 0 by default; multiple cameras may require a code change

**Target position moves erratically (HSV mode)**
- The depth estimate (Z axis) uses marker area — keep the marker flat and face-on to the camera
- Large depth jumps mean the marker is partially occluded; try a larger, brighter sticker

**Low FPS**
- Close other GPU-heavy applications
- Use Release build: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel`

**Export video is empty or won't open**
- Ensure OpenCV was built with video codec support (`brew install opencv` includes this on macOS)
- Try AVI format instead of MP4 if MP4 fails

---

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| (none yet) | All actions are mouse-driven via the UI panels |

---

## Depth Estimation Notes (HSV mode)

Kinema uses **single-camera depth estimation** based on the apparent size of the marker:

```
depth ≈ referenceDistance × sqrt(referenceArea / markerAreaInPixels)
```

Default calibration constants (in `Application.cpp`):
- `depthRefDist = 2.0` meters
- `depthRefArea = 10000` px²

For best accuracy, hold your marker at exactly 2 meters from the camera, note its pixel area from the **Markers** panel, and update `depthRefArea` to that value before recompiling.

ArUco mode uses `solvePnP` with the marker's known physical size, giving more accurate depth without manual calibration.
