# Roadmap: what we are yet to do (CP3 and final)

**Project:** Sign Language Gesture Visualizer
**Status when written:** Checkpoint 2 done. Next: Checkpoint 3 (week 12, OpenGL prototype), then the final project (week 13).

> **Update – final project delivered.** Everything planned here is now built: the 3D hand in OpenGL with lighting, shading and an orbit camera; A–Z and 0–10; type-a-word and sentence mode; the 32-sign conversation set with a head-and-torso reference and a second hand. Differences from this plan:
> - textures were replaced by skin colour and shading;
> - the arm uses a small two-bone inverse kinematics so the elbow follows the hand;
> - the signer's view looks from behind the signer, with the head and torso hidden.
>
> See [final-3d-opengl.md](final-3d-opengl.md).

This file records the scope decisions agreed after Checkpoint 2. **Everything we build in OpenGL should support it.**

---

## 1. Scope decisions

| Decision | What it means |
|---|---|
| **A basic conversation set** of about 30 everyday signs, not "all of ASL" | A clear, finishable goal (section 2) |
| **Full fingerspelling:** A–Z and 0–10 | Any word, name or number can be shown, even without its own sign |
| **Type a word** | The hand fingerspells whatever the user types (section 4) |
| **Sentence mode** | Known words are signed; unknown words are fingerspelled. Real signers handle words they lack a sign for the same way. |
| **3D in OpenGL** | Needed for palm orientation and for letters such as M, N, T, C, O (section 5) |
| **A simple head and torso** as a reference | Many signs are made at the chin, mouth, forehead or chest |
| **A second, mostly still "base" hand** | Needed for two-handed signs such as NAME, HELP, MORE |

**Out of scope** (we will say so in the presentation):
- facial expressions, which ASL uses as grammar (for example raised eyebrows for yes/no questions);
- recognising signs from a camera;
- the full ASL vocabulary and regional variants;
- realistic hand anatomy and finger collisions.

## 2. How many signs is "basic conversation"?

There is no official number. Beginner ASL courses typically teach a first set of about **100–150 signs** (for example Lifeprint's "First 100 Signs" and Start ASL's "Top 150"). Their very first lessons focus on about **25** signs: greetings, manners, names, simple questions and answers.

**Our target: a core set of about 30 signs**, plus fingerspelling (A–Z, 0–10) for everything else. Together they are enough for a short introductory conversation, for example:

> HELLO. MY NAME A-B-D-A-L-L-A. YOU DEAF? NICE MEET YOU. THANK YOU. GOODBYE.

### The conversation set

Done = already animated in Checkpoint 2.
1H = one hand, 2H = two hands.
"Neutral" = in front of the body; otherwise the body location.

| # | Sign | Group | Hands | Location | Status |
|---|---|---|---|---|---|
| 1 | HELLO | Greeting | 1H | Forehead | **Done** |
| 2 | GOODBYE | Greeting | 1H | Neutral | **Done** |
| 3 | THANK YOU | Manners | 1H | Chin | **Done** |
| 4 | YES | Answer | 1H | Neutral | **Done** |
| 5 | NO | Answer | 1H | Neutral | **Done** |
| 6 | I-LOVE-YOU | Greeting | 1H | Neutral | Handshape done |
| 7 | PLEASE | Manners | 1H | Chest (circling) | To do |
| 8 | SORRY | Manners | 1H | Chest (circling) | To do |
| 9 | GOOD | Answer | 1H | Chin | To do |
| 10 | BAD | Answer | 1H | Chin | To do |
| 11 | FINE | Answer | 1H | Chest | To do |
| 12 | I / ME | People | 1H | Chest (point) | To do |
| 13 | YOU | People | 1H | Neutral (point) | To do |
| 14 | MY | People | 1H | Chest | To do |
| 15 | YOUR | People | 1H | Neutral | To do |
| 16 | WHERE | Question | 1H | Neutral | To do |
| 17 | WHO | Question | 1H | Mouth / chin | To do |
| 18 | UNDERSTAND | Understanding | 1H | Forehead | To do |
| 19 | DEAF | Identity | 1H | Ear / mouth | To do |
| 20 | HEARING | Identity | 1H | Mouth | To do |
| 21 | NEED | Needs | 1H | Neutral | To do |
| 22 | WATER | Needs | 1H | Chin | To do |
| 23 | BATHROOM | Needs | 1H | Neutral | To do |
| 24 | NAME | People | 2H | Neutral | To do |
| 25 | NICE | Greeting | 2H | Neutral | To do |
| 26 | MEET | Greeting | 2H | Neutral | To do |
| 27 | WHAT | Question | 2H | Neutral | To do |
| 28 | HOW | Question | 2H | Neutral | To do |
| 29 | AGAIN | Understanding | 2H | Neutral | To do |
| 30 | HELP | Needs | 2H | Neutral | To do |
| 31 | WANT | Needs | 2H | Neutral | To do |
| 32 | MORE | Needs | 2H | Neutral | To do |

**Summary:** 32 signs.
- 6 are done.
- 9 are one-handed in neutral space and can be built straight away.
- 14 are one-handed at a body location and need the head and torso reference.
- 9 are two-handed and need the base hand.

**Order of work:** first the one-handed signs, then the body-location signs, then the two-handed signs. If time runs out, we cut from the end of the list.

**Before keyframing any sign**, check it against a reference video (Lifeprint, Signing Savvy or Start ASL) and note which variant we used. Many signs have more than one accepted form.

## 3. Handshapes needed

| Set | Have now | Still needed |
|---|---|---|
| Letters | A, B, E, I, L, R, S, U, V, W, Y (11) | C, D, F, G, H, K, M, N, O, P, Q, T, X (13 still), and J, Z (traced in the air, so animations) |
| Numbers | 1, 3, 4, 5 | 0 (= O), 2 (= V), 6 (= W), 9 (= F) reuse letter shapes. 7 and 8 are new. 10 is a movement (thumbs-up shake). |
| Word signs | 5, B (flat), S (fist), ILY | Flat-O, bent-B, claw, X; most other word signs reuse letter shapes |

A handshape is only a list of joint angles (`Poses.cpp`), so each one is quick to add. Letters that point sideways or down (G, H, P, Q) need wrist and forearm rotation. Letters with the thumb tucked under fingers (M, N, T) only look right in 3D.

## 4. Type a word: design

This is already almost built. The **B-A-B-Y** animation in `Animation.cpp` is made exactly this way, with a hard-coded word. Type-a-word turns that into a function: `spellWord(text) → SignAnimation`.

**Rules:**
- **Each letter:** hold its handshape for about 0.45 s, then blend to the next in about 0.25 s, drifting slightly sideways as real fingerspelling does.
- **Double letters** (BOOK, ABDALLA): repeat with a small sideways bounce.
- **J and Z:** short motion keyframes (the little finger traces J; the index finger traces Z).
- **Spaces:** a short pause. **Digits:** number handshapes. **Any other character:** skipped.
- **Display:** the typed word is shown on screen with the current letter highlighted, and the existing speed keys `[` and `]` work.

**Sentence mode** extends this:
1. Split the text into words.
2. If a word is in the sign dictionary, play its sign.
3. Otherwise, fingerspell it.
4. Join everything with short blends.

**Example:** `hello my name abdalla` → HELLO, MY, NAME, A-B-D-A-L-L-A.

## 5. Requirements for the OpenGL version (CP3)

1. **Keep the data, replace the drawing.**
   - Joint angles, sign library, keyframes and timing (`Hand`, `Poses`, `Animation`) stay as they are.
   - The OpenGL renderer replaces `HandRenderer`. It does not replace the data.
   - The CP1/CP2 pure C++ pages remain as the documented foundation.
2. **3D hand.**
   - Bones become 3D shapes (for example cylinders or boxes) and joints become spheres.
   - The matrices go from 3×3 to 4×4, using a matrix stack (ours or `glPushMatrix`/`glPopMatrix`).
3. **Wrist and forearm rotation in 3D.** Add forearm **roll** (turning the palm up, down or sideways) and wrist **pitch** to `Placement`. Most conversation signs depend on which way the palm faces.
4. **Body reference.**
   - A simple head and torso with named anchor points: forehead, eyes, ear, mouth, chin and chest.
   - Keyframe positions should be given relative to an anchor (for example "chin, a little forward"), not as raw coordinates.
5. **Second hand.**
   - Two `Hand` instances: the dominant hand and a base hand (mirrored, using the existing `Placement.mirror`).
   - Keyframes can hold a pose and a placement for each hand.
6. **Camera.** It must be able to orbit the hand. A side view makes M, N, T, C, O and X readable.
7. **Lighting and shading.** Needed so curled fingers read as 3D. This also covers the course's lighting topic.
8. **Interface.**
   - Watch mode and Build mode, as in CP2.
   - A **type-a-word / sentence box**, with the sign or letter being played shown on screen.
9. **Optional:** load sign definitions from a text file, so the team can add the 30+ signs without recompiling.

## 6. Suggested owners (CP3 issues #27–#31)

| Issue | Owner | Includes |
|---|---|---|
| #27 OpenGL hand geometry | Abdalla | 3D bones and joints, 4×4 matrix stack, forearm roll, second hand |
| #28 Animation in OpenGL | Elvis | Keyframes in 3D, `spellWord` and sentence mode, J and Z |
| #29 Lighting, shading, camera | Ilhan | Lights, materials, orbit camera, head and torso reference |
| #30 Watch/Build interface | Joy | Both modes in OpenGL, text input box, on-screen word display |
| #31 Sign library and testing | Benson | Remaining letters and numbers, the conversation set, checks against reference videos, tests |

## 7. For the presentation: "What we are yet to do"

- **Move to OpenGL (week 12):** a 3D hand with lighting, shading and a camera you can turn, using the same joint hierarchy and animation data.
- **Complete fingerspelling:** all 26 letters (J and Z animated) and the numbers 0–10. We have 11 letters and 4 numbers now.
- **Type a word:** the hand fingerspells any word the user types.
- **A basic conversation set:** about 30 everyday signs for greetings, manners, people, questions and needs. 6 are already animated.
- **Sentence mode:** known words are signed and unknown words are spelled, for example "HELLO, MY NAME A-B-D-A-L-L-A".
- **A body reference and a second hand,** needed for signs at the chin or chest and for two-handed signs.
- **Not planned:** facial expressions, sign recognition from a camera, the full ASL vocabulary.
