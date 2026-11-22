# Checkpoint 3 and the final project: the 3D version in OpenGL

This document explains how the 3D application (`sign_studio`) works: every technique, where it is used, and a worked example where one helps. The screenshots with captions are in [FINAL-graphical-output.pdf](FINAL-graphical-output.pdf).

**In one sentence:** the Checkpoint 1/2 rig was moved into 3D with 4×4 matrices, given surfaces built from our own meshes, drawn by OpenGL with lighting and a camera, and extended into a learning tool: the full alphabet, numbers, 32 conversation signs on a body with two hands, and a type-a-sentence mode.

## Contents

1. [What carried over from Checkpoints 1 and 2](#1-what-carried-over-from-checkpoints-1-and-2)
2. [4×4 matrices](#2-44-matrices)
3. [The hand in 3D](#3-the-hand-in-3d)
4. [Placing the hand: the hierarchy above the fingers](#4-placing-the-hand-the-hierarchy-above-the-fingers)
5. [The camera: view and projection](#5-the-camera-view-and-projection)
6. [Meshes](#6-meshes)
7. [Normals under transformation](#7-normals-under-transformation)
8. [OpenGL rendering: depth, lighting and shading](#8-opengl-rendering-depth-lighting-and-shading)
9. [The arm: two-bone inverse kinematics](#9-the-arm-two-bone-inverse-kinematics)
10. [The body reference and placing signs against it](#10-the-body-reference-and-placing-signs-against-it)
11. [Handshapes and the thumb contact search](#11-handshapes-and-the-thumb-contact-search)
12. [The sign dictionary](#12-the-sign-dictionary)
13. [Two-hand animation](#13-two-hand-animation)
14. [Type a word or a sentence](#14-type-a-word-or-a-sentence)
15. [The application: modes, picking and the overlay](#15-the-application-modes-picking-and-the-overlay)
16. [Screenshots and GIFs: back into our own framebuffer](#16-screenshots-and-gifs-back-into-our-own-framebuffer)
17. [Testing](#17-testing)
18. [Known limitations](#18-known-limitations)

---

## 1. What carried over from Checkpoints 1 and 2

The 3D version is built **on** the earlier work rather than beside it:

| From | Reused as |
|---|---|
| CP1/CP2 hand (`Hand.h`) | The finger sizes, joint positions and every joint angle. A sign written for Checkpoint 2 means the same in 3D. |
| CP1 Catmull–Rom palm outline | The 3D palm is that outline, extruded (§6) |
| CP1 Framebuffer + BMP writer | Screenshots: OpenGL's picture is read back into our framebuffer and saved by our writer (§16) |
| CP2 matrix stack | The same push / multiply / pop structure, now 4×4 (§2, §3) |
| CP2 keyframes, easing, Catmull–Rom paths, holds | The same in-betweening, now for two hands in 3D (§13) |
| CP2 GIF writer (LZW) | Animations of the 3D signs, with median-cut colours added (§16) |
| CP2 Watch / Build modes | Learn / Type / Build modes (§15) |

The CP1 and CP2 programs (`cp1_render`, `cp2_animate`, `sign_viewer`) still build, run, and pass their tests.

## 2. 4×4 matrices

`src/Math3D.*`. A point is the column (x, y, z, 1) and a direction is (x, y, z, 0). Every transformation is a 4×4 matrix, and, as in 2D, **A·B means "B first, then A"**.

| Transformation | Matrix (rows) |
|---|---|
| Translate (tx, ty, tz) | `[1 0 0 tx; 0 1 0 ty; 0 0 1 tz; 0 0 0 1]` |
| Scale (sx, sy, sz) | `diag(sx, sy, sz, 1)` |
| Rotate about x by θ | `[1 0 0 0; 0 c −s 0; 0 s c 0; 0 0 0 1]` |
| Rotate about y by θ | `[c 0 s 0; 0 1 0 0; −s 0 c 0; 0 0 0 1]` |
| Rotate about z by θ | `[c −s 0 0; s c 0 0; 0 0 1 0; 0 0 0 1]` |
| Reflect in x | `diag(−1, 1, 1, 1)`: turns a right hand into a left hand |
| Rotate about any axis k (Rodrigues) | `R = cos θ·I + sin θ·[k]× + (1 − cos θ)·k kᵀ` |

**Worked example.** Rotating (0, 1, 0) by 90° about x: y' = c·1 − s·0 = 0 and z' = s·1 + c·0 = 1, giving (0, 0, 1). The finger that pointed up now points at the viewer.

**Inverse.** `Mat4::inverse` uses the general formula: inverse = adjugate / determinant, where the adjugate is the transposed matrix of cofactors. A negative determinant means the matrix contains a mirror.

**Handing matrices to OpenGL.** We store `m[row][column]`; OpenGL reads 16 numbers column by column. `toColumnMajor()` reorders them for `glLoadMatrixd`. A test checks that a translation lands in elements 12–14.

**The 3D matrix stack** (`MatrixStack4`) works exactly like the 2D one: `push()` saves the current matrix, each operation multiplies onto the right, and `pop()` restores the saved matrix.

## 3. The hand in 3D

`src/Hand3D.*`. Hand coordinates: the wrist is the origin, **x** points to the thumb side, **y** along the fingers, and **z** out of the palm. For each finger, the matrix stack does:

```
push
  translate(base)                  the knuckle
  rotateZ(restDirection)           the finger's direction in the palm
  for each of the 3 bones:
      jointFrame[b] = top
      rotateZ(bend[b])             spread the finger, in the palm plane
      rotateY(−flex[b])            curl the finger towards the palm (+z)
      boneFrame[b] = top           the bone runs along this frame's x-axis
      translate(length[b], 0, 0)   to the next joint
      point[b+1] = top · (0,0,0)
pop
```

This is the Checkpoint 2 chain with **one more rotation**. In 2D, flex only shortened a bone on the screen (length × cos(total flex)). In 3D, flex really turns the bone towards the palm.

**Worked example: the 3D hand seen from the front equals the 2D hand.** Take the index finger, with its knuckle at (0.33, 0.98), lengths 0.46, 0.27, 0.21, and flex 30°, 30°, 0°:

| Bone | Total flex | Direction (y, z) | Point reached |
|---|---|---|---|
| 1 | 30° | (cos 30°, sin 30°) = (0.866, 0.5) | (0.33, 1.378, 0.230) |
| 2 | 60° | (0.5, 0.866) | (0.33, 1.513, 0.464) |
| 3 | 60° | (0.5, 0.866) | (0.33, 1.618, 0.646) |

The 2D formula gives the tip's height as 0.98 + 0.46·cos 30° + 0.27·cos 60° + 0.21·cos 60° = **1.618**, the same y. The 3D version also knows the tip is 0.646 in front of the palm. A test checks this equality for 200+ joints of the whole CP2 library.

**The thumb** sits on the fleshy pad, 0.06 in front of the palm. Its frame is turned 35° about its own length (`kThumbRoll`), so that its curl crosses the palm towards the little finger instead of going straight out.

## 4. Placing the hand: the hierarchy above the fingers

`Placement3D` puts the hand in front of the body:

```
hand matrix = T(position) · S(scale) · [mirror] · Rz(turn) · Rx(tip) · Ry(roll) · Rz(wristBend) · Rx(wristFlex)
```

| Angle | Meaning | Examples |
|---|---|---|
| roll (about the arm) | which way the palm faces | 0 = to the viewer, 180 = to the signer (numbers 1–5), 90 = towards the midline (FINE, WATER) |
| tip (about x) | fingers towards the viewer | 90 with roll 180 = palm up, fingers forward (WHAT, NICE's base hand) |
| turn (about z) | tilts the hand in the picture | −90 with roll 180 = fingers across the body (G, H, NAME) |
| wristBend / wristFlex | the wrist alone | GOODBYE's waves, YES's nods |

**The left hand** uses the same numbers with `left = true`, which adds the reflection `diag(−1, 1, 1, 1)`. Because the mirror is applied **after** the rotations, "palm towards the midline" means the same for both hands, so two-handed signs are written once and mirrored. The mirror has determinant −1, so it also turns anticlockwise triangles clockwise. The renderer checks the determinant of every model-view matrix and tells OpenGL which winding is the front (`glFrontFace`). Without this, the left hand would be drawn inside out.

## 5. The camera: view and projection

`src/Camera.*`, `src/Math3D.cpp`. An **orbit camera** sits on a sphere around a target point:

```
eye = target + distance · (sin(yaw)·cos(pitch), sin(pitch), cos(yaw)·cos(pitch))
```

Dragging the mouse changes yaw and pitch; the wheel changes the distance.

- **View matrix: `lookAt(eye, target, up)`.** It moves the world so the camera sits at the origin looking down −z. Its rows are the camera's right, up and backward axes. *Example:* with the eye at (0, 0, 10) and the target at the origin, the eye maps to (0, 0, 0) and the target to (0, 0, −10).
- **Perspective projection.** With f = 1 / tan(fovY / 2), the matrix maps x by f/aspect and y by f. It puts −z into w, so the later divide by w makes far things smaller. Depths from zNear to zFar map to −1 … +1 for the depth buffer. A test checks both ends.
- **Projecting a point to the window** (used for picking): clip = P·V·p, then divide by w, then x_window = (x + 1)/2 · width and y_window = (1 − y)/2 · height. The target of the camera lands exactly in the centre of the window, which is also tested.

**Presets** (`C`):

| Preset | View |
|---|---|
| Front | as a learner sitting opposite sees the signer |
| Three-quarter | turned to the signer's right |
| Side | shows depth: curled fingers, M / N / T |
| Signer's view | from behind the signer with the head and torso hidden, so the hands look like the learner's own, with the same left and right |

`Z` gives a close-up whose target glides after the palm each frame. The gliding is a smoothed move towards the goal: target += (goal − target) · min(1, 6·dt).

## 6. Meshes

`src/Mesh.*`, `src/HandMesh.*`. Every surface is a list of vertices (position + normal) and triangles (three indices, anticlockwise seen from outside). OpenGL draws each mesh with one `glDrawElements` call.

| Shape | How it is built | Used for |
|---|---|---|
| Sphere | latitude/longitude grid; the normal of a unit sphere is the point itself | joints, fingertips, nails (squashed), head, elbows |
| Frustum | a cone with its top cut off, along +x; the normal leans by the slope (r0 − r1)/length | finger bones (each bone tapers from one joint's width to the next) |
| Profile tube | elliptical rings (y, rx, rz) joined by triangles, smooth normals, flat caps | forearm, neck, torso |
| Slab | a 2D outline extruded along z with a **rounded edge**: rings at angles −90° … 90°, each inset by bevel·(1 − cos a) and lifted to (thickness/2)·sin a | the palm |

**The palm** is the CP1 palm outline: 14 control points through a closed Catmull–Rom spline, 6 samples per span. The outline is **concave** at the web of the thumb, so the flat front and back faces are triangulated with **ear clipping**:

1. A vertex is an "ear" if it turns the right way (convex) and the triangle it forms with its two neighbours contains no other vertex.
2. Cut the ear off and repeat until three vertices remain.

*Example:* an L-shaped polygon with 6 vertices gives 6 − 2 = 4 triangles whose areas add up to 3, the area of the L (tested).

**The meshes never change while signing.** Each piece is built once in its own coordinates, and only the matrices change: a bone is drawn with `hand matrix · boneFrame[b]`, and a joint with `hand matrix · T(point) · S(radius)`. This is the matrix-stack idea of Checkpoint 2, feeding OpenGL.

## 7. Normals under transformation

A normal must stay **perpendicular** to the surface. Under non-uniform scaling (a sphere squashed into a nail or an ellipsoid head), simply transforming the normal like a direction tilts it the wrong way. The correct matrix is the **inverse transpose** of the model matrix.

*Example:* scale a sphere by (2, 1, 1). At the point (1/√2, 1/√2, 0) the true normal of the stretched surface is proportional to (0.5, 1, 0), not (2, 1, 0). The inverse transpose diag(0.5, 1, 1) gives exactly (0.5, 1, 0).

Where this is used:
- `Mesh::append` uses the inverse transpose when it bakes a transformed copy.
- At draw time OpenGL does the same automatically with the modelview matrix, and `GL_NORMALIZE` brings the normals back to unit length.
- A mirror also reverses the triangle order, so mirrored copies still face outwards (tested).

## 8. OpenGL rendering: depth, lighting and shading

`src/GLRenderer.*`. Only **OpenGL 1.1** is used, the version every Windows PC has in `opengl32.dll`, so nothing needs installing. Each frame:

1. **Background:** a vertical colour gradient drawn as one quad.
2. **Projection and view:** `glLoadMatrixd` with *our* perspective and lookAt matrices. No GLU is used.
3. **Hidden surfaces:**
   - the depth buffer (`GL_DEPTH_TEST`) keeps, for every pixel, the nearest surface drawn so far, so fingers in front hide fingers behind in any order. This replaces the painter's algorithm of the 2D version;
   - back-face culling skips triangles facing away.
4. **Lighting:** the **Phong reflection model**, evaluated by OpenGL for three directional lights:

   colour = ambient + Σ lights [ diffuse · max(N·L, 0) + specular · max(R·V, 0)^shininess ]

   | Light | From | Role |
   |---|---|---|
   | Key | front-left, above | main warm light |
   | Fill | right | soft cool light into the shadows |
   | Rim | behind, above | outlines the hand against the background |

   `GL_LIGHT_MODEL_LOCAL_VIEWER` uses the true direction to the eye for the highlights. Light positions are set after loading the view matrix, so they stay fixed in the world when the camera orbits.

   *Example:* for the palm facing the viewer, N = (0, 0, 1). The key light direction (−5, 8, 10) normalised is (−0.36, 0.58, 0.73), so N·L = 0.73: the palm receives 73% of the key light's diffuse colour.
5. **Materials:**

   | Material | Diffuse colour | Highlight |
   |---|---|---|
   | Skin | warm | weak |
   | Nails | lighter | strong and sharp (shininess 70) |
   | Clothing | blue-grey | almost none |

   `GL_COLOR_MATERIAL` lets `glColor` set the diffuse colour.
6. **Shading modes** (`S`):

   | Mode | How it shades |
   |---|---|
   | Smooth (**Gouraud**) | colours computed at the vertices with the averaged normals, then interpolated across each triangle: the hand looks round |
   | Flat | one colour per triangle: the facets show |
   | Wireframe | `glPolygonMode(GL_LINE)`: only the edges of every triangle, showing how each surface is built |

   `L` turns lighting off to compare.
7. **Overlays:** the skeleton (`K`) and each bone's local axes (`G`, red x, green y, blue z) are drawn as lines and points with depth testing off.
8. **Interface:** an orthographic projection in pixels, semi-transparent panels (alpha blending: colour = α·new + (1 − α)·old), and bitmap text.

## 9. The arm: two-bone inverse kinematics

`solveElbow` in `src/HandMesh.cpp`. The signs say where the **hand** is; the arm has to follow. The shoulder S is fixed on the body, the wrist W is wherever the sign puts it, the upper arm has length l₁ = 3.4 and the forearm l₂ = 2.6. The elbow E must satisfy |E − S| = l₁ and |W − E| = l₂. Those two spheres meet in a circle:

```
d = |W − S|                         (clamped so the arm cannot over-stretch)
a = (l₁² − l₂² + d²) / (2d)         distance from S to the circle's centre along S→W  (law of cosines)
h = √(l₁² − a²)                     the circle's radius
E = S + a·u + h·p                   u = unit vector S→W, p = the "pole" direction made perpendicular to u
```

The pole points down, outwards and slightly back, which is where a real elbow goes. The higher the hand, the further out it swings, so the arm opens naturally for signs at the head.

**Worked example:** fingerspelling A.

| Quantity | Value |
|---|---|
| Shoulder S | (−1.82, 1.80, −0.58) |
| Wrist W | (−1.15, 1.37, 1.85) |
| d | 2.558 |
| a | (3.4² − 2.6² + 2.558²) / (2 · 2.558) = 2.217 |
| h | √(3.4² − 2.217²) = 2.578 |
| Elbow E | (−2.03, −1.02, 1.31): below the shoulder and out to the side |

A test checks that the elbow keeps both lengths exactly. The forearm mesh is then drawn from E to W, twisted to follow the hand's thumb side.

This is a small, analytic inverse kinematics for the arm. The full IK stretch goal of the proposal (dragging a fingertip and solving all finger joints) remained out of scope.

## 10. The body reference and placing signs against it

`src/Body.*`. The head and torso are built from ellipsoids and profile tubes, in **body space**:

- the signer faces the viewer (+z);
- the signer's right side is at −x, so the dominant right hand appears on the viewer's left;
- y = 0 is the middle of the chest, and one unit is about 9 cm.

It is a **reference for location**, not an avatar: it has no facial expressions and does not move.

**Named anchors** give every sign a place to start from: forehead, temple, eye, nose, mouth, chin, cheek, ear, chest, shoulder, neutral space, and the fingerspelling position.

**Placing a hand point exactly on an anchor** (`placeSiteAt`). Each hand has named sites:

| Group | Sites |
|---|---|
| Fingertips | index, middle, thumb and little-finger tips |
| Palm | palm front, palm back, front of the fingers |
| Other | knuckles, wrist |

For a chosen handshape and orientation:

```
position = target − (hand matrix with the wrist at the origin) · site
```

*Example:* THANK YOU starts with "middle fingertip on the chin". The code works out where the wrist must be for the fingertip to touch the chin, so the sign stays correct if the hand's size or the chin's position changes. A test checks the fingertip lands exactly on the target.

## 11. Handshapes and the thumb contact search

`src/Handshapes.*`. A handshape is still a list of joint angles. There are:
- **26 letters**;
- **11 numbers**;
- **10 extra shapes** used inside words: flat, bent, claw, flat-O, relaxed, and others.

Some shapes are also turned when shown, which is stored with the shape:

| Shapes | Turned how |
|---|---|
| C, O, 0 | turned to show the curve |
| G, H | point across the body |
| P, Q | point down |
| 1–5 | face the signer (ASL counts with the palm in) |
| 10 | a sideways thumbs-up |

**Thumb contact search.** In O, D, F, flat-O and 6–9 the thumb tip must touch a fingertip. Typing angles by hand never quite meets. `touchThumbTo` searches for them:

```
error = (|thumbTip − fingerTip| − (r_thumb + r_finger))²  +  a tiny pull towards the starting angles
repeat with step = 24°, 12°, 6°, … 0.05°:
    for each variable (thumb: 3 bends + 3 curls; the other finger: 3 curls):
        try +step and −step; keep the change if the error goes down
start from 4 different thumb positions; keep the best result
```

This is **coordinate descent**. Letting the other finger move a little as well matters: on its own the thumb could not reach the index tip of an O. Every contact shape now touches to within a hundredth of a unit. The O case is tested.

**Build mode's hint** compares the pose being built with every letter and number, using the average difference of all 30 joint angles, and shows the closest ("Looks most like: L").

## 12. The sign dictionary

`src/Signs3D.*`. There are **69 signs** in total:
- **26 letters**: J and Z trace their shape in the air;
- **11 numbers**: 10 is shaken with a twist of the forearm;
- **32 conversation signs**: 10 of them are two-handed.

Every sign is a list of keyframes for both hands, written against the body with `placeSiteAt`.

| Group | Signs |
|---|---|
| Greetings and manners | HELLO, GOODBYE, THANK YOU, PLEASE, SORRY, NICE, MEET |
| Answers | YES, NO, GOOD, BAD, FINE |
| People | I (me), YOU, MY, YOUR, NAME |
| Questions | WHAT, WHERE, WHO, HOW |
| Understanding | UNDERSTAND, AGAIN, DEAF, HEARING |
| Needs | NEED, WANT, HELP, MORE, WATER, BATHROOM |
| Feelings | I LOVE YOU |

Each sign carries:
- a short description of how it is made;
- where it is made (chin, chest, neutral space, …);
- whether it uses two hands;
- English aliases ("thanks" → THANK YOU, "toilet" → BATHROOM, "seven" → 7).

**Motion types used:**

| Motion | Signs |
|---|---|
| Path | HELLO, THANK YOU, YOU |
| Circle | PLEASE, SORRY, HEARING |
| Taps | FINE, WATER, NAME, MORE |
| Handshape change | GOODBYE, NO, UNDERSTAND, WANT |
| Wrist nod or wag | YES, WHERE, NEED |
| Twist | BATHROOM, 10 |
| Two hands together | MEET, NICE, HELP, AGAIN, HOW |

The Z is drawn as the signer writes it, so the viewer sees it mirrored. Repeated keyframes at its corners keep the corners sharp: the spline treats them as holds.

**Accuracy:** the signs follow common citation forms from beginner references. Many signs have variants, and the dictionary should be checked by fluent signers before the tool is used as a reference (see §18).

## 13. Two-hand animation

`src/Timeline.*`. Checkpoint 2's animation, extended: a keyframe now holds **both hands**, each with its handshape and 3D placement. Between keys:

| Quantity | How it is blended |
|---|---|
| Joint and arm angles | a + (b − a)·e, with the eased time e = u²(3 − 2u) |
| Wrist positions | a **Catmull–Rom spline** through the neighbouring keys, so the hand moves along a curve |
| Two identical keys | a **hold**: the hand stays exactly still (no spline overshoot) |

**Joining signs:** `appendKeys` adds the next sign's keys after a short blend. A sentence is one long timeline that starts and ends with both hands at rest, lowered outside the picture.

**Playback** is time-based: time += real seconds × speed, with speed 0.125× to 4×. In Learn mode the sign loops after a short pause.

## 14. Type a word or a sentence

`src/Speller.*`.

1. **Split** the text into lower-case words: "Nice to meet YOU!" → nice, to, meet, you. Apostrophes join words, and common contractions are expanded ("I'm" → I).
2. **Look up** the longest phrase first: three words, then two, then one ("thank you", "i love you").
3. **Skip** words ASL does not sign separately: a, an, the, is, am, are, be, to, of, do, does, did, it.
4. **Sign** every word that has a sign. **Fingerspell** every other word letter by letter, which is what signers do for names and for words with no sign:
   - a **double letter** slides 0.22 units to the side (the LL in ABDALLA);
   - numbers 0–10 have their own signs, and larger numbers are shown digit by digit ("25" → 2-5);
   - a single letter on its own ("a") is that letter.
5. **Join** everything into one timeline: 0.34 s between signs, 0.18 s between spelled letters, 0.5 s from rest, 0.6 s back to rest. Each part's start and end times are kept for the subtitles.

**Worked example:** "nice to meet you" gives NICE (0.50–1.40 s), MEET (1.74–2.54 s) and YOU (2.88–3.53 s), with a total of 4.53 s including the return to rest. "to" is left out.

"hello my name abdalla" gives HELLO, MY, NAME, then A-B-D-A-L-L-A: 10 parts in 9.1 s.

## 15. The application: modes, picking and the overlay

`apps/StudioCore.*` holds the whole application. The window files only open a window, pass on input and time, and draw text:
- `apps/studio_win32.cpp` uses Windows + WGL, with nothing to install;
- `apps/studio_glut.cpp` uses freeglut.

| Mode | What the learner does |
|---|---|
| **Learn** | Browse letters, numbers or words (`Tab`, `←` `→`). The sign loops, with its name, description and location. |
| **Type** | `Enter`, type, `Enter`. The sentence is signed with subtitles: the sign being made is shown in orange; for a spelled word, its letters with the current one lit. |
| **Build** | Drag any joint. Down curls it, sideways spreads it. Arrow keys adjust the selected joint, or turn the hand if none is selected. The closest letter or number is shown. |

**Picking in 3D:**
1. Every joint is projected to the window with the same projection × view × hand matrix the renderer uses (§5).
2. The nearest one within 24 pixels of the mouse is chosen; on a tie, the one nearer the camera wins.
3. Dragging changes that joint's flex and bend.
4. Dragging anywhere else orbits the camera.

**Left-handed signers (`M`):** the whole scene is mirrored with diag(−1, 1, 1, 1) in front of the view. The winding check from §4 keeps everything drawn correctly.

**Text:**

| Window | Method | Normal font | Large font |
|---|---|---|---|
| Windows | `wglUseFontBitmaps` turns two Windows fonts into OpenGL display lists | Consolas | Segoe UI |
| GLUT | GLUT's bitmap fonts | `GLUT_BITMAP_9_BY_15` | `GLUT_BITMAP_HELVETICA_18` |

The core asks the window code to measure text, so the panels fit their contents.

## 16. Screenshots and GIFs: back into our own framebuffer

`sign_studio --capture` (`apps/StudioCapture.*`) drives the application as a user would: choose a sign, a camera, a shading mode and a moment in time. Then it:

1. reads the finished picture with **`glReadPixels`** into our **Checkpoint 1 Framebuffer** (both store the bottom row first);
2. composes charts tile by tile in that framebuffer;
3. saves with our **BMP writer**, and animations with our **Checkpoint 2 GIF writer** at 15 frames per second.

**Median-cut colour quantization.** A shaded 3D frame has tens of thousands of colours; a GIF frame allows 256. The Checkpoint 2 fallback (a fixed 6×7×6 colour cube) looked posterised, so the GIF writer now chooses 256 colours per frame by **median cut**:

1. Count every colour, reduced to 5 bits per channel (32,768 possible values).
2. Start with one box holding all the colours.
3. Repeatedly take the box with the widest spread in red, green or blue, and cut it at the pixel-count median along that channel.
4. Each of the 256 boxes becomes one palette colour: its average.
5. Each 5-bit colour is matched to its nearest palette entry once, in a lookup table, weighting green most because the eye is most sensitive to it.

A test quantizes a 4,096-colour gradient with a worst-case error under 20 levels out of 255.

## 17. Testing

`tests/final_tests.cpp` adds **52 checks** to `render_test`, for **146 in total**. None of the new checks need a window or OpenGL:

| Area | What is checked |
|---|---|
| 3D maths | rotations about x, y and z; Rodrigues equals rotateZ; M·M⁻¹ = I; reflection determinant; lookAt; the perspective depth range; projecting to the window centre; the matrix stack; OpenGL's column order |
| 3D hand | the front view equals the 2D hand on 200+ joints; curling goes towards the palm; placeSiteAt lands exactly; a left hand has a negative determinant; the O's thumb touches; the arm keeps its lengths; the elbow hangs below the shoulder |
| Meshes | unit normals; outward winding of spheres and bones; ear clipping of a concave L; the palm's flat front; mirrored meshes still face out |
| Dictionary | A–Z and 0–10 in order; 32 word signs; every sign in time order; aliases; THANK YOU starts on the chin; 10 two-handed signs; the Build-mode hint |
| Timeline | clamping; in-between values; holds stay still; joining signs |
| Speller | tokenizing; skipped words; fingerspelling a name; parts in order; the double-letter slide; rest at both ends; contractions; single letters; numbers; phrases; partAt |
| GIF colours | median cut on a gradient |

The Windows build was compiled with MinGW, and the tests and the capture also ran through Wine with Windows' OpenGL interface: all 146 checks passed and all 18 files were written. The application was **not** compiled with Microsoft's own compiler (MSVC) during development; `build.bat` uses it when run from the Developer Command Prompt.

## 18. Known limitations

- **Hands only:** ASL's facial grammar (eyebrows, mouth shapes) is not shown. The head is only a location reference.
- **English word order:** sentence mode signs words in the order typed and drops function words. It is not a translator and does not produce ASL grammar.
- **One form per sign:** regional and personal variants exist. Every sign should be reviewed by fluent signers before the tool is used as a reference, which is the first recommendation of our final presentation.
- **No collision handling:** fingers can briefly pass through each other between very different handshapes, and contact with the body is approximate.
- **Stylised geometry:** tubes and spheres, no skin deformation, no textures (replaced by skin colour and shading).
- **Per-vertex lighting:** OpenGL 1.1 computes the Phong model at the vertices (Gouraud shading), so highlights are softer than per-pixel Phong.
- **Fixed shoulders:** the arm follows the hand by two-bone IK, but the body does not lean or turn.
