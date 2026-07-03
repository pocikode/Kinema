# Kinema — User Guide

Kinema tracks colored markers via webcam using chromaticity-based (RGB-ratio) detection, drives a rigged 3D skeleton in real time, records the motion, and exports results as JSON, video (MP4/AVI), or a GLB with the animation baked back onto the source rig.

---

## Requirements

- A webcam connected to your computer
- Bright, solid-colored physical markers (stickers, tape, colored balls, or painted dots)
  - Best colors: **red, orange, yellow, green, cyan, blue, magenta** — saturated colors far apart from each other
  - Avoid: white, black, grey, skin tones (too little chroma to classify)
- A rigged GLB/glTF model with a compatible skeleton (Mixamo models work out of the box)
- A **Release** build of Kinema (see README for build instructions)

---

## Launch

```bash
./build/kinema          # macOS
.\build\Release\kinema.exe   # Windows
```

On startup, Kinema auto-loads the bundled Mixamo low-poly rig and seeds **seven default marker slots** — one head marker plus three markers per arm:

| # | Slot | Default color | Binding | Role |
|---|------|--------------|---------|------|
| 0 | `head` | orange | Look At | Head rotates to face the marker |
| 1 | `upperarm_L` | blue | Hint | Aims the left upper-arm bone |
| 2 | `lowerarm_L` | green | Hint | Left forearm-aim fallback |
| 3 | `palm_L` | red | IK Target | Left-arm end-effector (2-bone IK) |
| 4 | `upperarm_R` | cyan | Hint | Aims the right upper-arm bone |
| 5 | `lowerarm_R` | magenta | Hint | Right forearm-aim fallback |
| 6 | `palm_R` | yellow | IK Target | Right-arm end-effector (2-bone IK) |

The window opens with:
- **3D viewport** — grid, coordinate axes, and the loaded rigged model
- **Left sidebar** — five collapsible sections: Model, Detector, Markers, Recording, Export
- **Bottom-right** — live camera feed with per-marker overlay circles and labels
- **Top-right** — XYZ orientation gizmo (rotates with the camera)
- **Bottom bar** — FPS, recording state, marker detection status
- **Top menu bar** — File → Save JSON / Export Video shortcuts

---

## How RGB-Ratio Detection Works

Instead of thresholding raw pixel colors, Kinema classifies each pixel by its **chromaticity** — the proportion of each channel in the total brightness:

```
r = R / (R+G+B)     g = G / (R+G+B)     b = B / (R+G+B)
```

Because the ratios are normalized, a marker keeps roughly the same `r`/`g`/`b` values whether it is brightly lit or in shadow — this is what makes RGB-ratio mode more robust to lighting changes than raw color thresholding.

A pixel matches a marker slot when:

1. `r`, `g`, and `b` each fall inside the slot's configured windows,
2. the pixel is bright enough (`mean(R,G,B) > Brightness min`), and
3. the pixel is saturated enough (its chromaticity deviates from neutral grey `(⅓,⅓,⅓)` by at least `Saturation min` — this rejects grey/washed-out pixels).

Before classification, the camera frame is **average-pooled**: downsampled by averaging blocks of `Pool size × Pool size` pixels. This suppresses pixel noise and speeds up detection. Enable **Show pooled frame** in the Detector panel to see exactly what the detector sees (a blocky version of the feed) — useful for debugging.

Each slot produces at most one observation: the largest matching blob (see *Marker pairing* below for the second-largest).

---

## Step-by-Step Workflow

### 1. Load a Rigged Model

In the **Model** section:

1. The path field defaults to the bundled Mixamo rig. Replace it with any GLB/glTF rig path if you want a different character.
2. Click **Load Model**. Use **Reset** to return the rig to its bind pose.

Once loaded, the section shows the resolved path and the bone count, and the bone dropdowns in the Markers section become available.

> The default rig auto-loads on startup, so you can usually skip this step.

### 2. Select the RGB Ratio Detector

In the **Detector** panel:

| Control | What it does |
|---------|-------------|
| Mode | Detector backend — **RGB Ratio** (default) |
| Pool size | Average-pool block size (2–32, default 8). Larger = less noise and faster, but small/distant markers may disappear. |
| Show pooled frame | Preview the pooled (blocky) frame in the camera feed instead of the raw image |

The panel also holds the **jitter filter** (applies to all markers, live-tunable):

| Control | What it does |
|---------|-------------|
| Smoothing | EMA factor 0–1. Higher = snappier response, lower = smoother but laggier. |
| Deadzone | Radius (normalized) within which small centroid movement is ignored — kills stationary jitter |
| Occlusion hold | When a marker vanishes, keep extrapolating its motion for this many seconds (0 = off) |
| Arm forward | Constant forward lean applied to IK arms (world units) |

and the **depth calibration** (see *Depth Estimation* below).

### 3. Configure Marker Slots

In the **Markers** panel, use **+ Add marker** / **- Remove last** to manage slots. Each slot shows:

- **Name** — label drawn next to the marker's circle in the camera feed.
- **Color swatch** — derived automatically from the configured detection range, so it always reflects what the detector is looking for.
- **Status** — `detected` (green) when the color is found this frame, `held` (yellow) while occlusion-hold is extrapolating, `—` (grey) otherwise.

#### Sampling a color (recommended)

Click **Pick color (eyedropper)**, then click your physical marker in the *Camera Feed* window. Kinema averages a small patch around the click, computes its chromaticity, and sets the slot's `r`/`g` windows to ±0.06 around it. The `b` window, brightness floor, and saturation floor are left as configured.

Repeat per slot, holding each marker in front of the camera under the same lighting you will record in.

#### Manual tuning (optional)

| Field | Meaning |
|-------|---------|
| r min / r max | Red chromaticity window (0–1) |
| g min / g max | Green chromaticity window (0–1) |
| b min / b max | Blue chromaticity window (0–1). Default 0–1 = unconstrained; needed to separate colors that share r/g but differ in blue, e.g. red (b≈0) vs magenta (b≈0.4). |
| Saturation min | Rejects grey/washed-out pixels (0–1, default 0.12). Raise if background objects false-trigger; lower if a pale marker is missed. |
| Brightness min | Rejects dark pixels (0–255, default 30). Raise in bright rooms to ignore shadows. |

Approximate chromaticity windows for common marker colors (starting points — the eyedropper is more accurate for your lighting):

| Color | r | g | b |
|-------|------|------|------|
| Red | 0.50–1.00 | 0.00–0.30 | 0.00–0.30 |
| Orange | 0.45–0.65 | 0.25–0.40 | 0.00–0.25 |
| Yellow | 0.38–0.52 | 0.38–0.52 | 0.00–0.20 |
| Green | 0.00–0.30 | 0.45–1.00 | 0.00–0.35 |
| Cyan | 0.00–0.25 | 0.33–0.50 | 0.35–0.60 |
| Blue | 0.00–0.30 | 0.10–0.35 | 0.45–1.00 |
| Magenta | 0.35–0.55 | 0.00–0.25 | 0.35–0.60 |

#### Marker pairing (one color, two markers)

Set **Pair with (shares color)** on a slot to reference another slot. The paired (follower) slot then has no color range of its own — it is resolved as the **second-largest blob** of the source slot's color, and left/right identity is disambiguated automatically. Useful when you only have two markers of the same color (e.g. two red gloves).

#### Binding modes

| Mode | Description |
|------|-------------|
| Position | Moves the bone to the unprojected marker position; rotation unchanged |
| IK Target (2-bone) | Marker is the end-effector; analytical 2-bone IK bends root → mid → end toward it |
| Look At (rotation only) | Bone rotates to face the marker; position is never written (no neck/spine stretching) |
| Hint (detect only) | Marker is tracked but drives no bone directly; referenced by an IK Target slot as an arm-direction hint |

For **IK Target**, select three bones — **IK Root** (e.g. shoulder), **IK Mid** (elbow), **End** (hand) — and optionally reference an **Upper-arm marker** and **Lower-arm marker** (Hint slots). The upper-arm hint aims the upper-arm bone; the lower-arm hint acts as a forearm-aim fallback when the palm marker is occluded. The default slots come pre-wired this way for Mixamo rigs.

#### Saving the setup

**Export config...** / **Import config...** write and read the entire marker setup (RGB-ratio ranges, bindings, IK wiring, pairing) as a JSON file, so a tuned configuration can be reused across sessions.

### 4. Calibrate Depth (optional, recommended)

Kinema estimates marker depth from apparent blob size:

```
depth ≈ Ref distance × sqrt(Ref area / blob area in pixels)
```

To calibrate, in the **Detector → Depth** group:

1. Stand with the marker at a known distance from the camera (default reference: 2 m). Adjust **Ref distance** if you use a different distance.
2. Select which slot to calibrate against in **Calib marker**.
3. Click **Calibrate from marker** — the marker's current blob area is captured as **Ref area**.

Depth accuracy depends on the marker staying flat and face-on to the camera; partial occlusion shrinks the blob and reads as "further away".

### 5. Navigate the 3D Viewport

| Action | Control |
|--------|---------|
| Rotate | Left mouse button + drag |
| Pan | Right mouse button + drag |
| Zoom | Scroll wheel |

The XYZ gizmo (top-right) mirrors the camera orientation: **red** = X, **green** = Y, **blue** = Z.

### 6. Record Motion

1. Ensure the markers you need show **detected**.
2. Open the **Recording** panel and click **START RECORDING**.
3. Perform the motion in front of the camera.
4. Click **STOP RECORDING**.

The panel shows elapsed time and keyframe count live. Keyframes store **local-space** bone poses.

### 7. Reconstruct & Preview

After stopping, click **RECONSTRUCT 3D** to replay the recording on the skeleton in the viewport (with interpolation between keyframes). Press it again to replay.

### 8. Export Results

In the **Export** section:

| Field | Description |
|-------|-------------|
| Output Format | MP4 (recommended) or AVI — affects the video export only |
| FPS | Frames per second for the exported video (1–60) |
| Directory | Destination folder (defaults to `$HOME`); **Browse…** opens a native folder picker |
| Filename (no extension) | Base name, defaults to `kinema_YYYYMMDD`; the extension is added per format |

Then:

- **SAVE JSON** — writes `<dir>/<name>.json` with a per-bone local-space keyframe timeline (schema v2).
- **EXPORT VIDEO** — renders each keyframe offscreen and writes `<dir>/<name>.mp4` or `.avi`.
- **EXPORT GLB** — appends a new animation named `Recorded` to the source GLB and writes `<dir>/<name>.glb`. Open it in any glTF viewer (Blender, [glb.ee](https://glb.ee), Three.js) to verify.

**Save JSON** and **Export Video** are also on the **File** menu.

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

**Marker not detected / status shows "—"**
- Use the **eyedropper**: click the marker in the camera feed under recording lighting — this beats manual tuning every time.
- Enable **Show pooled frame** to see the detector's actual input; if the marker vanishes into the background there, lower **Pool size** or use a bigger marker.
- Lower **Saturation min** for pale markers; lower **Brightness min** in dim rooms.
- If two slots fight over one marker, their chromaticity windows overlap — tighten the `b` window (e.g. red vs magenta) or pick more distant colors.

**Wrong marker picked up (background object)**
- Raise **Saturation min** — most background surfaces are near-grey.
- Raise **Brightness min** to reject shadows.
- Narrow the offending slot's `r`/`g`/`b` windows.

**Bone dropdown is empty**
- Load a model first via the **Model** panel.

**Skeleton doesn't move**
- Confirm each slot's bone selection matches a real bone in the loaded rig.
- For IK Target, all three bones (root, mid, end) must be set.
- Hint slots never move bones by themselves — they must be referenced from an IK Target slot.

**Camera feed shows "Camera not available"**
- Check the webcam is connected and not used by another app.
- On macOS, grant camera permission (System Settings → Privacy & Security → Camera); the app retries automatically once granted.
- The app uses camera index 0; multiple cameras may require a code change.

**Marker position jumps around**
- Lower **Smoothing** and/or raise **Deadzone** in the Detector panel.
- Raise **Occlusion hold** so brief dropouts are bridged by extrapolation (slot shows `held`).

**Depth (Z) is erratic**
- Recalibrate: stand at **Ref distance** and click **Calibrate from marker**.
- Keep the marker flat and face-on; occlusion shrinks the blob and skews depth.

**Low FPS**
- Use a Release build: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel`
- Raise **Pool size** — detection cost drops quadratically with pool size.

**Export video is empty or won't open**
- Ensure OpenCV was built with video codec support (`brew install opencv` includes this on macOS).
- Try AVI if MP4 fails.

**GLB export fails or the file won't open**
- The source model must be a **single-buffer GLB**; multi-file glTF (`.gltf` + `.bin`) is not supported.
- Check stderr — bones that couldn't be resolved against the glTF node list are logged and skipped.
- A recording must exist (keyframe count > 0) and a model must be loaded.
