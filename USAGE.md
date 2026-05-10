# Kinema — User Guide

Kinema tracks colored markers via webcam, drives a rigged 3D skeleton in real-time, records the motion, and lets you export results as JSON, video (MP4/AVI), or a GLB that bundles the animation back onto the source rig.

---

## Requirements

- A webcam connected to your computer
- A bright, solid-colored physical marker (sticker, tape, or painted dot)
  - Best colors: **orange, green, yellow, or red**
  - Avoid: white, black, grey, skin tones (too little saturation)
- A rigged GLB/glTF model with a compatible skeleton (Mixamo models work out of the box)
- macOS with GLFW, GLEW, and OpenCV installed

---

## Launch

```bash
./build/kinema
```

On first launch, Kinema auto-loads the bundled Mixamo low-poly rig and seeds three default marker slots (head + both hands) so you can start experimenting immediately.

The window opens with:
- **3D viewport** — grid, coordinate axes, and the loaded rigged model (centered on the origin)
- **Left sidebar** — five collapsible sections: Model, Detector, Markers, Recording, Export
- **Bottom-right** — live camera feed with per-marker overlay circles
- **Top-right** — XYZ orientation gizmo (rotates with camera)
- **Bottom bar** — FPS, recording state, marker detection status
- **Top menu bar** — File → Save JSON / Export Video shortcuts

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

In the **Model** section:

1. The path field defaults to `models/mixamo_lowpoly/mixamo-animated-lowpoly.glb`. Replace it with any GLB/glTF rig path (absolute or relative to the `assets/` folder) if you want a different character.
2. Click **Load Model**. Use **Reset** to return the rig to its bind pose / origin.

Once loaded, the section shows the resolved path and the total number of bones. The bone list becomes available in the Markers section's Bone dropdowns.

> The default rig auto-loads on startup, so you can skip straight to the Detector section the first time.

---

### 2. Tune HSV Ranges

Kinema uses HSV color tracking. In the **Detector** panel, you can see the current tracking mode. Configure the per-marker HSV ranges in the **Markers** panel below.

---

### 3. Configure Marker Slots

In the **Markers** panel, click **+ Add marker** for each physical marker you want to track. Each slot has:

**HSV Range**

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

**Bone binding**

| Field | Description |
|-------|-------------|
| Name | Label shown next to the marker's circle in the camera feed overlay |
| Color | Overlay circle color in the camera feed |
| Binding | How the marker drives the skeleton (see below) |
| Bone | Target bone — a dropdown populated from the loaded rig |

**Binding modes:**

| Mode | Description |
|------|-------------|
| Position | Moves the bone to the unprojected marker position; rotation unchanged |
| IK Target (2-bone) | Marker is the end-effector; analytical 2-bone IK bends root → mid → end toward the marker |
| Look At (rotation only) | Bone rotates to face the marker; position is never written (avoids neck/spine stretching) |

For **IK Target**, three bones must be selected: **IK Root** (e.g. shoulder), **IK Mid** (e.g. elbow), **End** (e.g. hand). The defaults seed `mixamorig:LeftArm / LeftForeArm / LeftHand` (and the right-side equivalents) for Mixamo rigs.

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

After stopping, click **RECONSTRUCT 3D** to replay the recorded motion on the skeleton in the 3D viewport. The recording plays once; press the button again to replay.

---

### 7. Export Results

In the **Export** section:

| Field | Description |
|-------|-------------|
| Output Format | MP4 (recommended) or AVI — only affects the video export |
| FPS | Frames per second for the exported video (1–60) |
| Directory | Destination folder. Defaults to `$HOME`. **Browse…** opens a native folder picker. |
| Filename (no extension) | Base name. Defaults to `kinema_YYYYMMDD`. The appropriate extension is added per format. |

Then, using the buttons inside the Export section:
- **SAVE JSON** — writes `<dir>/<name>.json` with a per-bone local-space keyframe timeline (schema v2).
- **EXPORT VIDEO** — renders each keyframe to an offscreen framebuffer and writes `<dir>/<name>.mp4` or `.avi`.
- **EXPORT GLB** — takes the source GLB you loaded and appends a new animation named `Recorded` built from your keyframes, writing `<dir>/<name>.glb`. Open the result in any glTF viewer (Blender, [glb.ee](https://glb.ee), Three.js) to verify.

**Save JSON** and **Export Video** are also on the **File** menu. **Export GLB** is only available from the Export section.

**JSON schema (v2):**
```json
{
  "version": 2,
  "keyframes": [
    {
      "t": 0.033,
      "bones": {
        "mixamorig:Hips": {"px":0,"py":1,"pz":0,"qw":1,"qx":0,"qy":0,"qz":0,"sx":1,"sy":1,"sz":1},
        "mixamorig:LeftHand": {"px":0.12,"py":1.4,"pz":0.05,"qw":0.97,"qx":0.01,"qy":-0.23,"qz":0.02,"sx":1,"sy":1,"sz":1}
      }
    }
  ]
}
```

Poses are stored in **local space** (relative to each bone's parent), so the rig root can move without rebaking the animation.

---

## Troubleshooting

**Marker not detected / status shows "none"**
- Move the marker directly in front of the camera
- Adjust H min/max to match your marker color (use the table above)
- Increase lighting — poor light reduces saturation
- Raise V min if the room is bright, lower it if dim

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

**GLB export fails or the resulting file won't open**
- The source model must be a **single-buffer GLB**. Multi-buffer glTF-separate (`.gltf` + `.bin`) is not supported.
- Check stderr — any bones the exporter couldn't resolve against the glTF node list are logged and skipped.
- Make sure a recording exists (keyframe count > 0 in the Recording section) and a model has been loaded.

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
