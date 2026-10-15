# Checkpoint 1 – Graphics Algorithms

**Project:** Sign Language Gesture Visualizer
**Checkpoint 1 (Technical Foundation):** the graphics algorithms covered so far, implemented in pure C++ and used to draw our 2D hand.

Every algorithm here is our own code in `src/`. None of them call a graphics library.

| # | Topic | Algorithm(s) | File | Where it is used in the hand |
|---|---|---|---|---|
| 1 | Pixels | Framebuffer, point clipping, BMP writer | `Framebuffer.cpp` | Everything is drawn into it |
| 2 | Lines | DDA, Bresenham | `RasterLines.cpp` | Skeleton bones, outlines, sampled curves |
| 3 | Line operations | Dashed/dotted lines, thick lines, polylines | `RasterLines.cpp` | Detail-view window marker, motion trails |
| 4 | Circles | Midpoint circle (outline and filled) | `RasterCircles.cpp` | Knuckles, fingertips, joint markers |
| 5 | Ellipses | Midpoint ellipse (outline and filled) | `RasterEllipses.cpp` | Palm pads, fingernails |
| 6 | Polygons | Outline, scan-line fill (Edge Table + Active Edge Table) | `RasterPolygons.cpp` | Forearm, palm, finger bones |
| 7 | Polygon operations | Shoelace area, convexity test, point-in-polygon, centroid | `Geometry.cpp` | Palm analysis, tests |
| 8 | Clipping | Cohen–Sutherland (lines), Sutherland–Hodgman (polygons) | `Clipping.cpp`, `Canvas.cpp` | Everything is clipped to the screen; the detail view |
| 9 | Curves | Bézier (de Casteljau and Bernstein form) | `Curves.cpp` | Palm crease lines |
| 10 | Splines | Catmull–Rom, uniform cubic B-spline | `Curves.cpp` | Smooth palm outline, fingertip motion trails |

---

## 1. Our approach: a framebuffer we control

The `Framebuffer` is a block of memory holding one RGB colour per pixel. Every algorithm writes into it one pixel at a time through `setPixel(x, y, colour)`. The finished image is saved as a `.bmp` file by our own BMP writer: a 54-byte header, then rows of blue/green/red bytes, each row padded to a multiple of 4 bytes. So the whole pipeline is pure C++.

- `(0, 0)` is the **bottom-left** pixel. BMP files also store the bottom row first, so the buffer is written out as it is.
- **Point clipping:** `setPixel` ignores any pixel outside the *scissor rectangle*, which is the whole buffer by default. Shapes can therefore run off the edge safely.
- `saveBMP(path, scale)` can enlarge every pixel into a `scale × scale` block. This produces the zoomed images that show each algorithm's staircase.

---

## 2. Line drawing (`RasterLines.cpp`)

A line from `(x0, y0)` to `(x1, y1)` has `dx = x1 − x0` and `dy = y1 − y0`. The screen is a grid, so we must choose which pixels best approximate the line.

### 2.1 DDA (Digital Differential Analyzer)

1. `steps = max(|dx|, |dy|)`. We step along the longer axis, one pixel per step, so a steep line has no gaps.
2. `xInc = dx / steps` and `yInc = dy / steps`. One of these is ±1 and the other is a fraction.
3. From `(x0, y0)`, repeat `steps + 1` times: plot `(round(x), round(y))`, then add the increments.

**Cost:** two floating-point additions and two roundings per pixel. Floating-point error also builds up along long lines.

### 2.2 Bresenham

Bresenham chooses pixels using **integers only**. For a gentle slope (`|dx| ≥ |dy|`), x always steps by one, and y either stays on the same row or moves one row. The decision parameter `p` tells us which pixel is closer to the true line:

```
p0 = 2|dy| − |dx|
p < 0  : stay on this row     p = p + 2|dy|
p >= 0 : step y by one        p = p + 2|dy| − 2|dx|
```

For steep lines the roles of x and y swap. `sx` and `sy` (each +1 or −1) cover lines going left or down, so all eight octants work.

**Worked example:** (20, 10) → (30, 18). `dx = 10`, `dy = 8`, `p0 = 6`, `2dy = 16`, `2dy − 2dx = −4`.

| k | p_k | pixel |
|---|---|---|
| 0 | 6 | (20, 10) |
| 1 | 2 | (21, 11) |
| 2 | −2 | (22, 12) |
| 3 | 14 | (23, 12) |
| 4 | 10 | (24, 13) |
| 5 | 6 | (25, 14) |
| 6 | 2 | (26, 15) |
| 7 | −2 | (27, 16) |
| 8 | 14 | (28, 16) |
| 9 | 10 | (29, 17) |
| 10 | – | (30, 18) |

### 2.3 DDA vs Bresenham

The lines page draws a starburst of 24 lines in all eight octants with **both** algorithms into two separate buffers. It then colours every pixel by which algorithm chose it: yellow for both, red for DDA only, green for Bresenham only. At 480×360 they disagree on **8 pixels**.

Every difference is a near-tie, where the true line passes almost exactly halfway between two pixels. There, the floating-point error that DDA builds up tips its rounding the other way. The two algorithms choose essentially the same pixels; the real difference is **cost and exactness**. Bresenham uses only integer additions and comparisons and never builds up error.

### 2.4 Line operations (line attributes)

Both share Bresenham's loop (`bresenhamWalk`), which reports each pixel and its step number:

- **Line style (dashed, dotted):** plot pixel *i* only when bit `(i mod 16)` of a 16-bit pattern is 1. `0x0F0F` gives 4 on, 4 off (dashed). `0x3333` gives 2 on, 2 off (dotted). `0x19FF` gives dash-dot.
- **Line width (pixel replication):** for every pixel Bresenham chooses, plot a short run of pixels across the line: a vertical run for a gentle line, a horizontal run for a steep one. A run of *n* pixels is only `n·cos θ` thick at right angles to the line, so we stretch the run to `width × length / max(|dx|, |dy|)`. A 3-pixel-wide 45° line therefore uses runs of `round(3√2) = 4` pixels.
- **Polyline:** consecutive points joined with lines. This is how every curve is drawn.

---

## 3. Circles: the midpoint circle algorithm (`RasterCircles.cpp`)

A circle is symmetric in **eight ways**. If `(x, y)` is on it, so are `(±x, ±y)` and `(±y, ±x)`. We compute only the arc from `(0, r)` to where `x = y`, then mirror each point. The circles page shows each octant in its own colour.

On that arc x always increases by one, and y either stays or decreases by one. We test the **midpoint** between the two candidate pixels with `f(x, y) = x² + y² − r²` (negative inside the circle):

```
p0 = 1 − r                   (integer form of 5/4 − r)
x = x + 1
p < 0  : midpoint inside  -> keep y      p = p + 2x + 1
p >= 0 : midpoint outside -> y = y − 1    p = p + 2x + 1 − 2y
```

Here x and y are the new values after the step.

**Worked example:** r = 10.

| k | p_k | next point |
|---|---|---|
| 0 | −9 | (1, 10) |
| 1 | −6 | (2, 10) |
| 2 | −1 | (3, 10) |
| 3 | 6 | (4, 9) |
| 4 | −3 | (5, 9) |
| 5 | 8 | (6, 8) |
| 6 | 5 | (7, 7) |

**Filled circle:** uses the same loop, but joins the mirrored points with 4 horizontal spans. A filled circle covers about π(r + ½)² pixels, not πr², because the boundary pixels are included.

---

## 4. Ellipses: the midpoint ellipse algorithm (`RasterEllipses.cpp`)

An axis-aligned ellipse with radii `rx` and `ry` is symmetric in only **four** ways: x and y can no longer be swapped. We compute the quarter from `(0, ry)` to `(rx, 0)` and mirror each point into the other three quadrants.

Ellipse function: `f(x, y) = ry²x² + rx²y² − rx²ry²`. The quarter is split into **two regions** at the point where the slope is −1, which is where `2ry²x = 2rx²y`.

**Region 1** (slope between 0 and −1): x always steps by one. Test the midpoint `(x + 1, y − ½)`:

```
p1_0 = ry² − rx²·ry + rx²/4
p1 < 0  : keep y          p1 = p1 + 2ry²x + ry²
p1 >= 0 : y = y − 1       p1 = p1 + 2ry²x − 2rx²y + ry²
continue while 2ry²x < 2rx²y
```

**Region 2** (steeper than −1): y always steps down by one. Test the midpoint `(x + ½, y − 1)`:

```
p2_0 = ry²(x + ½)² + rx²(y − 1)² − rx²ry²
p2 > 0  : keep x          p2 = p2 − 2rx²y + rx²
p2 <= 0 : x = x + 1       p2 = p2 + 2ry²x − 2rx²y + rx²
continue while y > 0
```

`2ry²x` and `2rx²y` are updated by adding constants (`2ry²` and `−2rx²`), so every step is an integer addition. The two starting values contain quarters, so they are rounded.

**Worked example:** rx = 8, ry = 6, so `ry² = 36`, `rx² = 64`.

| Region | k | p_k | next point | 2ry²x | 2rx²y |
|---|---|---|---|---|---|
| 1 | 0 | −332 | (1, 6) | 72 | 768 |
| 1 | 1 | −224 | (2, 6) | 144 | 768 |
| 1 | 2 | −44 | (3, 6) | 216 | 768 |
| 1 | 3 | 208 | (4, 5) | 288 | 640 |
| 1 | 4 | −108 | (5, 5) | 360 | 640 |
| 1 | 5 | 288 | (6, 4) | 432 | 512 |
| 1 | 6 | 244 | (7, 3) | 504 | 384 |
| 2 | 0 | −23 | (8, 2) | 576 | 256 |
| 2 | 1 | 361 | (8, 1) | 576 | 128 |
| 2 | 2 | 297 | (8, 0) | 576 | 0 |

Region 1 stops once `2ry²x ≥ 2rx²y` (504 ≥ 384). The circles page draws region 1 in green and region 2 in orange.

**Special cases:** an ellipse with `ry = 0` or `rx = 0` is a straight line, and is drawn as one.

---

## 5. Polygons (`RasterPolygons.cpp`, `Geometry.cpp`)

### 5.1 Representation and outline

A polygon is an ordered list of vertices `v0 … vN`. The outline is the line `vi → vi+1` for each vertex, plus the closing edge `vN → v0`.

### 5.2 Scan-line fill with Edge Table and Active Edge Table

Sweep a horizontal line from the bottom of the polygon to the top. On each row, find where the row crosses the edges, sort the crossings left to right, and fill between pairs (1st–2nd, 3rd–4th, …). Every crossing switches between outside and inside: the **even–odd rule**. That is why concave shapes fill correctly. A self-intersecting star keeps its centre empty, because that region is crossed twice.

- **Edge Table:** every non-horizontal edge, filed under its lowest row. Each entry stores `yMax`, the x at the bottom row, and `1/m = dx/dy`.
- **Active Edge Table:** the edges crossing the current row. For each row: add the edges that start there, remove those with `yMax ≤ y`, sort by x, fill from `ceil(x_left)` to `floor(x_right)`, then add `1/m` to every x.
- **Shared vertices:** each edge covers the rows `yMin ≤ y < yMax`, with the top row excluded. A vertex shared by two edges is then counted once, so crossings always come in pairs.
- **Horizontal edges** are skipped. The edges on either side of them already mark the span.

### 5.3 Polygon operations

| Operation | Method | Used for |
|---|---|---|
| Area and orientation | Shoelace: `A = ½ Σ (xᵢ yᵢ₊₁ − xᵢ₊₁ yᵢ)`. Positive means anticlockwise. | Palm area; checking clipping results |
| Convex or concave | Sign of the cross product at every vertex. Mixed signs mean concave. | Confirms the palm is concave at the web of the thumb |
| Point-in-polygon | Cast a ray to the right and count edge crossings. An odd count means inside: the same even–odd rule as the fill. | The grid of dots on the polygons page |
| Centroid | `Cx = Σ (xᵢ + xᵢ₊₁)(xᵢyᵢ₊₁ − xᵢ₊₁yᵢ) / 6A`, and the same for y | The dot in the star |

---

## 6. Clipping (`Clipping.cpp`, `Canvas.cpp`)

Clipping keeps only the part of a shape inside a rectangular **clip window**, and it happens **before** scan conversion. The line and fill algorithms then never produce pixels outside the window.

### 6.1 Cohen–Sutherland line clipping

Each end point gets a 4-bit **outcode**, with one bit per window side it lies beyond:

```
1001 | 1000 | 1010        TOP = 8, BOTTOM = 4
-----+------+-----        RIGHT = 2, LEFT = 1
0001 | 0000 | 0010
-----+------+-----
0101 | 0100 | 0110
```

1. Both codes are `0000`: **accept** the whole line.
2. `code_a AND code_b ≠ 0000`: both ends are beyond the same side, so **reject** the whole line.
3. Otherwise, take an end point that is outside, move it to where the line meets the window edge named by one of its bits, recompute its code, and repeat.
   - With `x = xmin` or `xmax`: `y = y0 + (y1 − y0)(x − x0)/(x1 − x0)`
   - With `y = ymin` or `ymax`: `x = x0 + (x1 − x0)(y − y0)/(y1 − y0)`

**Worked example:** window `x 10..50, y 10..40`, line (0, 20) → (60, 50).

| Step | a | code a | b | code b | Action |
|---|---|---|---|---|---|
| 1 | (0, 20) | 0001 | (60, 50) | 1010 | Clip a against LEFT: y = 20 + 30·10/60 = 25 |
| 2 | (10, 25) | 0000 | (60, 50) | 1010 | Clip b against TOP: x = 10 + 50·15/25 = 40 |
| 3 | (10, 25) | 0000 | (40, 40) | 0000 | Accept |

### 6.2 Sutherland–Hodgman polygon clipping

Clip the whole polygon against **one** window edge at a time (left, right, bottom, top), feeding each result into the next pass. For each polygon edge S → P:

| S | P | Output |
|---|---|---|
| inside | inside | P |
| inside | outside | intersection I |
| outside | inside | I, then P |
| outside | outside | nothing |

On the clipping page, a 14-vertex concave star is clipped to an **11-vertex** polygon, which is then scan-line filled.

**Limitation:** when the window cuts a concave polygon into separate pieces, Sutherland–Hodgman keeps them joined by edges running along the window border.

### 6.3 How clipping is used in the hand

All drawing goes through a `Canvas`, which owns a clip rectangle:

| Shape | How it is clipped |
|---|---|
| Lines, polylines, outlines | Cohen–Sutherland, then DDA or Bresenham. Outlines are clipped edge by edge, so no false edges appear along the border. |
| Filled polygons | Sutherland–Hodgman, then scan-line fill |
| Circles, ellipses, thick lines | Point clipping (the scissor test), because the clipping algorithms above work on straight edges only |

**The detail view** (key `C` in the viewer, `hand_detail_view.bmp`) shows clipping doing a real job. A square **window** around the fingers is marked with a dashed rectangle and drawn magnified into a **viewport** in the corner. The window-to-viewport mapping is

```
x_v = V.xmin + (x_w − W.xmin) · k,    k = V.width / W.width
```

and the same for y. Applied to our hand view, this gives a new scale and origin, so the hand is drawn again 2× larger. Without clipping, the magnified hand would spill across the main picture. An automated test checks that no pixel outside the viewport changes.

---

## 7. Curves (`Curves.cpp`)

A curve is a formula **P(t)** that gives a point for each `t` from 0 to 1. To draw one, we evaluate it at many values of t and join the points with lines. The number of segments grows with the curve's length on screen (`segmentsForLength`), so each segment is about 3–4 pixels long.

### 7.1 Bézier curves

For control points `P0 … Pn`, **de Casteljau's algorithm** at parameter t repeatedly interpolates between neighbouring points, `lerp(A, B, t) = (1 − t)A + tB`, until a single point is left. That point is P(t).

The same curve in **Bernstein form** (cubic) is:

```
P(t) = (1−t)³ P0 + 3t(1−t)² P1 + 3t²(1−t) P2 + t³ P3
```

The weights are never negative and always sum to 1, so the curve stays inside the convex hull of its control points. It passes through P0 and P3 but generally not through P1 and P2. Our tests check that both forms give identical points.

**Worked example:** P0 = (0, 0), P1 = (10, 30), P2 = (40, 30), P3 = (50, 0), t = 0.5.

| Level | Points |
|---|---|
| 0 (control points) | (0, 0), (10, 30), (40, 30), (50, 0) |
| 1 | (5, 15), (25, 30), (45, 15) |
| 2 | (15, 22.5), (35, 22.5) |
| 3 = P(0.5) | **(25, 22.5)** |

The curves page draws these levels for a moving t.

**In the hand:** the three palm creases (heart, head and life lines) are cubic Bézier curves.

### 7.2 Splines

A spline is built from cubic pieces (spans). Each span uses four neighbouring control points and a basis matrix: `P(t) = [t³ t² t 1] · M · [p0 p1 p2 p3]ᵀ`.

| | Catmull–Rom | Uniform cubic B-spline |
|---|---|---|
| Matrix M | ½ · [[−1, 3, −3, 1], [2, −5, 4, −1], [−1, 0, 1, 0], [0, 2, 0, 0]] | ⅙ · [[−1, 3, −3, 1], [3, −6, 3, 0], [−3, 0, 3, 0], [1, 4, 1, 0]] |
| Passes through the control points? | **Yes**: P(0) = p1, P(1) = p2 | **No**: P(0) = (p0 + 4p1 + p2)/6 |
| Smoothness | C1 (smooth direction) | C2 (smooth curvature) |
| Stays in convex hull | No, it can overshoot | Yes |

**Example:** p0 = (0, 0), p1 = (10, 10), p2 = (20, 0), p3 = (30, 10). At t = 0, Catmull–Rom gives (10, 10) = p1, while the B-spline gives (10, 6.67).

**Open and closed splines:** a closed spline wraps around the point list, which gives an outline. An open spline repeats its end points (once for Catmull–Rom, twice for the B-spline) so that it starts at the first point and ends at the last.

**In the hand:**

- **Palm outline:** a *closed* Catmull–Rom spline through 14 control points. It is sampled into an 84-sided polygon, which is then scan-line filled. Catmull–Rom is used because the outline must pass through the points that define the hand's shape; the control points are visible in wireframe mode.
- **Motion trails:** going from one sign to another, each fingertip's position is recorded at 5 keyframes (t = 0, ¼, ½, ¾, 1). An *open* Catmull–Rom spline through those keyframes draws the path (key `M`, `hand_motion_trail.bmp`). This prepares for Checkpoint 2, where motion will be driven by keyframes and curves.

---

## 8. The hand: what is drawn with what (`HandRenderer.cpp`)

| Part | Primitive | Algorithm |
|---|---|---|
| Forearm | Polygon | Sutherland–Hodgman → scan-line fill; Cohen–Sutherland → Bresenham outline |
| Palm | Closed spline → polygon | Catmull–Rom → scan-line fill |
| Palm pads (thumb and little-finger base) | Ellipse | Filled midpoint ellipse |
| Palm creases | Curve | Cubic Bézier → polyline |
| Finger bones (skin) | 4-sided polygon ("capsule") | Scan-line fill (wireframe: outline) |
| Rounded knuckles and fingertips | Circle | Filled midpoint circle |
| Fingernails (curled fingers only) | Ellipse | Filled midpoint ellipse + outline |
| Skeleton bones | Line | DDA or Bresenham (key `D`) |
| Skeleton joints, wrist (root) | Circle | Filled circle + midpoint ring |
| Detail view | Clipping | Window-to-viewport + Cohen–Sutherland + Sutherland–Hodgman |
| Motion trails | Open spline | Catmull–Rom → thick lines |

**Bone shape:** for a bone from joint A to B, the direction `(dx, dy)` is rotated by 90° to `(−dy, dx)` and normalised. A and B are then moved half the skin width each way, giving a 4-sided polygon. Circles on both ends round it off.

**Painter's algorithm:** forearm, palm, pads, creases, then the fingers from pinky to index, and the thumb last. Nearer parts paint over farther ones.

---

## 9. The rig and forward kinematics (`Hand.cpp`)

The palm/wrist is the root of the joint hierarchy. Each finger has **three joints and a fingertip**:

- Fingers: MCP (knuckle), PIP (middle joint), DIP (end joint), then the tip
- Thumb: CMC, MCP, IP, then the tip

Each joint has a **bend** angle (spreading the fingers, in the picture plane) and a **flex** angle (curling towards the viewer). Forward kinematics walks outwards along each finger:

```
direction += bend[i];      flex += flex[i]
projected length = length[i] × cos(flex)           (orthographic projection)
next joint = previous joint + projected length × (cos(direction), sin(direction))
```

Because the angles add up, bending a knuckle carries every joint after it. A bone flexed past 90° folds back over the palm and is drawn in a darker tone, and its fingernail becomes visible.

The 9 signs (5, A, B, L, V, W, Y, I-Love-You, I) in `Poses.cpp` are just lists of angles. Moving between signs blends every angle with smoothstep easing `t²(3 − 2t)`. In Checkpoint 2 this becomes proper transformation matrices and keyframe animation.

---

## 10. Testing (`tests/render_test.cpp`)

The test program runs without a window and performs **62 checks**, all passing:

- Lines: all directions, zero length, clipping at the edges, and the Bresenham worked example pixel by pixel; dashed patterns; thick-line widths.
- Circles and ellipses: the r = 10 circle and rx = 8, ry = 6 ellipse worked examples, symmetry, filled areas, and zero radii.
- Polygons: exact fill counts, a concave U, a zero-height polygon, oversized polygons; shoelace area and orientation, convexity, point-in-polygon, and centroid.
- Clipping: outcodes; accept, reject, horizontal, diagonal and corner cases; Sutherland–Hodgman inside, outside, overlapping and covering cases. Also: 40 random lines and 40 triangles drawn through a Canvas, with no pixel landing outside the viewport.
- Curves: Bézier end points, de Casteljau = Bernstein, the t = 0.5 worked example, Catmull–Rom passing through all 14 palm points, and B-spline start points.
- Scenes: every sign renders; the detail view never draws outside its viewport.

## 11. Known limitations

- **Depth** comes only from drawing order (painter's algorithm). 3D comes later.
- **Edges are aliased** (jagged), as expected from basic scan conversion. There is no anti-aliasing.
- **Ellipses are axis-aligned.** A rotated ellipse needs transformations (Checkpoint 2). The nails are only drawn on curled fingers, which point roughly straight down.
- **Sutherland–Hodgman** keeps the separate pieces of a concave polygon joined along the window border (see 6.2).
- **Thick lines** have square ends, and small gaps can appear where thick segments meet at sharp angles.
- **Motion trails** follow the spline through 5 keyframes, which approximates the true fingertip path between those keyframes.
