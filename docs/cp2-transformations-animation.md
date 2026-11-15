# Checkpoint 2 – Transformations and Animation

**Project:** Sign Language Gesture Visualizer
**Checkpoint 2:** build on the CP1 components by implementing transformations and animation in pure C++.

Everything here is our own C++ code in `src/` and uses only the standard library: the matrices, the matrix stack, the keyframe animation, and the GIF writer that saves the animations.

| # | Topic | What we implemented | File | Where it is used |
|---|---|---|---|---|
| 1 | Homogeneous coordinates | 3×3 matrices for 2D points and directions | `Transform.*` | Every point drawn |
| 2 | Basic transformations | Translation, rotation, scaling, reflection, shear | `Transform.*` | Moving, turning, scaling and mirroring the hand |
| 3 | Fixed-point transformations | Rotation about a pivot, scaling about a fixed point | `Transform.*` | The hand turns about the **wrist**; each finger turns about its **knuckle** |
| 4 | Composite transformations | Matrix multiplication; the order matters | `Transform.*`, `TransformScene.*` | View × Forearm × Wrist × Joints |
| 5 | Inverse transformation | Affine inverse | `Transform.*` | Build mode: mouse position → joint angle |
| 6 | Hierarchical modelling | Matrix stack (push / pop) | `Transform.*`, `Hand.cpp` | Forward kinematics of all 15 finger bones |
| 7 | Window-to-viewport | As a matrix | `HandRenderer.cpp`, `cp2_animate.cpp` | Detail view, frame grids |
| 8 | Transformed primitives | Rotated ellipses as transformed polygons | `TransformedShapes.*` | Palm pads and nails when the hand is rotated |
| 9 | Keyframe animation | Keyframes, in-betweening, easing | `Animation.*` | 6 animated signs (Watch mode) |
| 10 | Motion paths | Catmull–Rom spline through keyframe positions | `Animation.*` | The wrist's path in each moving sign |
| 11 | Time-based playback | Elapsed time × speed, looping | `Animation.*` | Viewer playback speed (`[` `]`) |
| 12 | Animation output | Animated GIF writer with LZW compression | `GifWriter.*` | `anim_*.gif` |

---

## 1. Homogeneous coordinates

A 2D point `(x, y)` is written as the column `(x, y, 1)`, and every transformation becomes a 3×3 matrix `M` with `p' = M p`:

```
| x' |   | a  b  c | | x |        x' = a x + b y + c
| y' | = | d  e  f | | y |        y' = d x + e y + f
| 1  |   | 0  0  1 | | 1 |
```

The third coordinate (1) is what lets **translation** be a matrix too, through `c` and `f`. Then *any* sequence of translations, rotations, scalings, reflections and shears multiplies into **one** matrix.

A **direction** (a vector) is written with 0 instead of 1, so translation does not affect it (`applyVector`). We use this for the bone directions in Build mode and for the arrow heads.

## 2. The basic transformations (`Transform.cpp`)

| Transformation | Matrix | Example |
|---|---|---|
| Translation `T(tx, ty)` | `[[1, 0, tx], [0, 1, ty], [0, 0, 1]]` | (1, 1) by (3, −2) → (4, −1) |
| Rotation `R(θ)`, anticlockwise about the origin | `[[cos θ, −sin θ, 0], [sin θ, cos θ, 0], [0, 0, 1]]` | (2, 0) by 30° → (1.732, 1) |
| Scaling `S(sx, sy)` about the origin | `[[sx, 0, 0], [0, sy, 0], [0, 0, 1]]` | det = sx·sy = how much areas grow |
| Reflection in the y-axis | `S(−1, 1)` | (2, 5) → (−2, 5); det < 0 |
| Reflection in the x-axis | `S(1, −1)` | (x, y) → (x, −y) |
| Shear | `[[1, shx, 0], [shy, 1, 0], [0, 0, 1]]` | shx = 0.5: (0, 2) → (1, 2) |

The transformations page (`transform_1_basics.bmp`, viewer key `T`) shows each of these applied to a finger shape. Dashed is before, filled is after, and the yellow dot is the fixed point.

## 3. Rotation about a pivot and scaling about a fixed point

`R(θ)` rotates about the **origin**. To rotate about any point `p`, move `p` to the origin, rotate, and move it back:

```
R_p(θ) = T(p) · R(θ) · T(−p)        (read right to left: T(−p) happens first)
```

**Worked example:** rotate (3, 1) by 90° about p = (2, 1).

```
T(−p):  (3, 1) → (1, 0)
R(90):  (1, 0) → (0, 1)
T(p):   (0, 1) → (2, 2)

Composite matrix:  | 0 −1  3 |      check: x' = −1·1 + 3 = 2
                   | 1  0 −1 |             y' =  1·3 − 1 = 2   ✓
                   | 0  0  1 |
```

The pivot itself never moves, and `render_test` checks this. Scaling about a fixed point works the same way: `S_p = T(p) · S(sx, sy) · T(−p)`.

**In the hand:**

- The **wrist** is the origin of hand coordinates, so `R(wristAngle)` turns the hand about the wrist while the forearm stays still. This is the GOODBYE wave.
- Each **finger** turns about its knuckle, because the matrix stack first moves to the knuckle and then rotates (§6).

## 4. Composite transformations – the order matters

Matrix multiplication is not commutative. Rotating then translating is different from translating then rotating.

**Example** with T = T(1, 0), R = R(90°), applied to the point (1, 0):

- `T · R` (rotate first, then translate): (1, 0) → (0, 1) → **(1, 1)**
- `R · T` (translate first, then rotate): (1, 0) → (2, 0) → **(0, 2)**

`transform_2_composite.bmp` shows this on the finger: green is `T·R` and orange is `R·T`. Next to it is the three-step rotation about a pivot.

**Every point of the hand** goes through this composite on its way to the screen:

```
screen = View · Forearm · R(wrist) · Joint₁ · Joint₂ · … · point

View    = T(wrist position on screen) · S(pixels per hand unit)
Forearm = T(position) · S(scale) · [reflection for a left hand]
```

## 5. The inverse matrix (Build mode)

For an affine matrix with 2×2 part `A` and translation `t`:

```
| A  t |⁻¹   | A⁻¹   −A⁻¹ t |          A⁻¹ = 1/(ae − bd) · |  e  −b |
| 0  1 |   = | 0       1    |                              | −d   a |
```

`M · M⁻¹ = I` is checked by the tests.

**Build mode uses this to turn the mouse into a joint angle.** A joint's frame maps local coordinates to the screen: `screen = M · local`. Therefore `local = M⁻¹ · mouse`. In the joint's own frame, the bone points along the local x-axis when its bend is 0, so the bend that makes the bone point at the mouse is just:

```
bend = atan2(local.y, local.x)
```

The same idea in forearm coordinates turns the wrist when you drag empty space. Because it uses the inverse matrix, it still works when the hand is moved, scaled or mirrored.

## 6. Hierarchical modelling with a matrix stack (`Hand.cpp`)

In CP1 the joints were placed with hand-written trigonometry. In CP2 they are placed with a **matrix stack**: `push()` saves the current matrix, `pop()` restores it, and each `translate`/`rotate` multiplies onto the right of the current matrix.

```
for each finger:
    push()                                  // save the hand frame
    translate(base)                         // move to the knuckle
    rotate(rest direction)                  // face along the finger
    for each of its 3 bones:
        jointFrame[i] = top()               // the frame before this joint's bend
        rotate(bend[i])                     // turn at the joint
        translate(length[i] · cos(flex), 0) // move along the bone to the next joint
        point[i+1] = top() · (0, 0)
    pop()                                   // back to the hand frame for the next finger
```

Because each bone's matrix is multiplied onto its parent's, bending the knuckle carries the middle joint, end joint and fingertip with it. `pop()` makes sure the next finger starts from the hand again, not from the end of the previous finger.

The tests check that the matrix stack gives **exactly** the same joint positions as the CP1 trigonometry for all 16 signs (to 10⁻¹²). The CP1 images are pixel-identical.

**The full hierarchy:** forearm → wrist rotation → palm → knuckle → middle joint → end joint → fingertip. `hand_frames.bmp` (viewer key `G`) draws every joint's local frame, with the red arrow for local x and the green for local y.

## 7. Window-to-viewport as a matrix

The CP1 detail view mapped a window W to a viewport V with formulas. In CP2 it is a single matrix:

```
WV = T(V.xmin, V.ymin) · S(k, k) · T(−W.xmin, −W.ymin),    k = V.width / W.width
zoomed view = WV · View
```

The animation frame grids (`anim_*_frames.bmp`) use the same idea. Each cell is a viewport, and the window is the area of hand units where the hand moves. It is scaled by the same `k` in x and y, so the hand keeps its shape.

## 8. Transformed primitives: rotated ellipses (`TransformedShapes.cpp`)

Lines and polygons survive any affine transformation: we transform their vertices and draw as before. **Circles and ellipses are different.** The midpoint ellipse algorithm can only draw *axis-aligned* ellipses, and a rotated ellipse is still an ellipse but tilted. So an ellipse given in local coordinates is drawn like this:

- **Matrix has no rotation or shear** (`b = d = 0`): use the midpoint ellipse with radii `rx·|a|` and `ry·|e|`. The absolute value handles mirroring.
- **Otherwise:** sample points `(cx + rx cos t, cy + ry sin t)`, transform them, and scan-line fill the resulting polygon. The number of points grows with the ellipse's size on screen.

The palm pads and fingernails therefore stay correct when the hand is turned (Build mode, GOODBYE wave). Midpoint circles stay circles under rotation and uniform scaling, and their radius is scaled by `√|det|`.

## 9. Keyframe animation (`Animation.cpp`)

A moving sign is a list of **keyframes**. Each says, at one moment, what the hand looks like: its **pose** (all 30 joint angles) and its **placement** (position, wrist angle, scale). The frames in between are computed:

```
u = (t − t₀) / (t₁ − t₀)          how far between the two keyframes (0 → 1)
e = u² (3 − 2u)                   ease-in-out: slow start, fast middle, slow stop
angle = a₀ + (a₁ − a₀) · e        every joint angle, the wrist angle and the scale
position = Catmull-Rom(p₋₁, p₀, p₁, p₂, e)   a smooth motion path (§10)
```

**Worked example (GOODBYE):** keyframes +22° at t = 0.3 s and −22° at t = 0.7 s. At t = 0.4 s:

```
u = (0.4 − 0.3) / 0.4 = 0.25
e = 0.25² × (3 − 0.5) = 0.15625
wrist = 22 + (−22 − 22) × 0.15625 = 15.125°
```

Without easing (linear interpolation) it would be 11°. Easing keeps the hand near the turning point for longer, as a real wave does.

**The 6 animated signs:**

| Sign | Keys | Length | What moves | Transformations used |
|---|---|---|---|---|
| HELLO | 5 | 1.8 s | Flat hand moves out from the forehead and tilts | Translation along a curved path, wrist rotation |
| GOODBYE | 6 | 1.8 s | Open hand waves | Rotation about the wrist (fixed point) |
| YES | 5 | 1.2 s | Fist nods down and towards the viewer, twice | Translation, scaling (closer looks bigger) |
| NO | 5 | 1.1 s | Index and middle fingers snap onto the thumb, twice | Joint-angle interpolation (pose keyframes) |
| THANK YOU | 5 | 1.9 s | Flat hand from the chin forwards and down | Translation, scaling, wrist rotation |
| B-A-B-Y | 8 | 3.1 s | Fingerspelling: hold each letter, blend between them | Pose keyframes with holds |

A **hold** (two keyframes with the same position) keeps the hand perfectly still. The interpolation detects it instead of letting the spline drift.

## 10. Motion paths

The wrist positions of the keyframes are joined by a **Catmull–Rom spline** (from CP1), so the hand travels along a smooth curve instead of a zig-zag. The spline passes through every keyframe position, so the hand is exactly where each keyframe says. `anim_*_path.bmp` (and viewer key `M` while watching) draws the path with its keyframes. The start pose is drawn dim and the end pose filled.

## 11. Time-based playback

The frame shown depends on **elapsed time**, not on how fast the computer draws:

```
time = time + realSeconds × speed      (speed 0.125× to 4×, keys [ and ])
after the end, pause 0.6 s and loop
```

At 2× speed, 0.25 s of real time advances the animation by 0.5 s, and the tests check this. The GIFs are made the same way, sampling the animation every 1/20 s.

## 12. Saving animations: our GIF writer (`GifWriter.cpp`)

Each animation is saved as an animated GIF written by hand, like the CP1 BMP writer:

1. **Header:** `GIF89a`, plus the screen size.
2. **NETSCAPE2.0 block:** loop forever.
3. **For each frame:**
   - a graphic control block with the display time in hundredths of a second;
   - a colour table, giving each different colour in the frame an index (our frames use a few dozen colours, so they are stored exactly);
   - the pixel indices compressed with **LZW**. Codes 0–255 are single colours, 256 = CLEAR and 257 = END. Each new sequence (a known sequence plus one more pixel) gets the next code, so long runs of one colour shrink to a few codes. Codes start at 9 bits, grow to 12, and the dictionary restarts at 4095 entries.
4. **Trailer:** `;`

We checked the GIFs with an independent decoder: every frame matches our framebuffer pixel for pixel, including a stress test that fills the LZW dictionary and a frame with more than 256 colours.

---

## Testing

`render_test` now runs **94 checks**: the 62 from CP1, plus 32 for CP2.

- **Transformations:** every basic transformation; the worked examples; the pivot staying fixed; the order of matrices; `M·M⁻¹ = I`; determinants; push/pop.
- **The hierarchy:** matrix-stack kinematics equals the CP1 trigonometry for all 16 signs; joint frames sit on the joints; the wrist stays put when it rotates; mirroring is a reflection.
- **Build mode:** picking with the inverse matrix makes a bone point at the target.
- **Ellipses:** a rotated ellipse lies along its rotated axis.
- **Animation:** it passes through every keyframe; easing; clamping; motion paths; holds; speed; looping.
- **GIF writer:** LZW compression; the file structure.

## Known limitations

- The hand is still drawn in 2D (front view). Flexing towards the viewer is shown by foreshortening (`cos(flex)`), and coming closer by scaling. Real depth comes with 3D in OpenGL (Checkpoint 3).
- Rotated ellipses are drawn as polygons (accurate to about 3 pixels per edge), not with the midpoint algorithm.
- Interpolating joint angles linearly can make fingers pass through each other briefly between very different handshapes. There is no collision detection.
- Joint limits in Build mode are simple ranges (bend ±90°, flex −20° to 120°), not real anatomy.
- Several signs are front-view approximations of their handshapes (for example R, E, S).
