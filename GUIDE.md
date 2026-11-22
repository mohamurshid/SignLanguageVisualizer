# Benson Gitonga – your part of Checkpoint 3 and the final project

**Push order:** **5th (last)**. Your push completes the project, so you also check that it builds.
**Issue you close:** #31

## Your files

| File | What it is |
|---|---|
| `src/Handshapes.h`, `src/Handshapes.cpp` | **New.** Every handshape: A–Z, 0–10 and the extra shapes, with how each letter is turned; the thumb contacts via the search; the Build-mode "looks most like" hint. |
| `src/Signs3D.h`, `src/Signs3D.cpp` | **New.** The dictionary: 26 letters (J and Z move), 11 numbers, 32 conversation signs written against the body, with descriptions, locations and aliases. |
| `tests/final_tests.cpp`, `tests/render_test.cpp` | **New / changed.** 52 new checks (146 in total). |
| `build.bat`, `CMakeLists.txt`, `.gitignore` | **Changed.** Build `sign_studio` (and `sign_studio_glut` with CMake + freeglut); ignore `output_final`. |
| `README.md`, `docs/` | **Changed / new.** The final README, the write-up `docs/final-3d-opengl.md`, the screenshots PDF, the images and GIFs, and the roadmap update. |

## Before you push: make sure you can explain it

Read your files and the whole of `docs/final-3d-opengl.md` and the README. Then check that you can explain:

- How a sign is written against the body, e.g. THANK YOU: "middle fingertip on the chin, then forward and down, palm up".
- Why numbers 1–5 face the signer, why C and O are turned, and what J and Z do.
- The 32 signs by group, and which are two-handed.
- What the new checks test, especially that the 3D hand seen from the front equals the CP2 hand.
- Where every topic is explained (README and write-up): you are the person the team asks.

The best preparation is to run `bin\sign_studio.exe` and find your part on screen.

## How to push

You already have the repository from Checkpoints 1 and 2.

1. **Get the latest work first**, so you start from everyone's pushes:
   ```bat
   cd /d C:\path\to\your\clone
   git pull
   ```
2. **Copy your files in.** Unzip this package, then copy its folders over the repository, replacing old versions:
   ```bat
   robocopy "C:\path\to\unzipped\5-Benson" . /E /XF GUIDE.md
   ```
3. **Commit and push**, one commit per step:
   ```bat
   git add src\Handshapes.h src\Handshapes.cpp
   git commit -m "Add the handshapes: A-Z, 0-10 and the extra shapes (#31)"
   git add src\Signs3D.h src\Signs3D.cpp
   git commit -m "Add the sign dictionary: letters, numbers and 32 conversation signs (#31)"
   git add tests\final_tests.cpp tests\render_test.cpp
   git commit -m "Add 52 checks for the 3D version (#31)"
   git add build.bat CMakeLists.txt .gitignore
   git commit -m "Build the 3D application"
   git add README.md docs
   git commit -m "Document the final project: write-up, screenshots PDF, images and GIFs (closes #31)"
   git push
   ```
   If `git push` is refused because someone pushed before you, run `git pull --rebase` and then `git push` again.
4. Check on GitHub that the commits show **your** name, and that your issue closed.

## After your push: check the whole project

Everyone's files are now in. In the repository folder, open **Developer Command Prompt for VS 2022** and run:
```bat
build.bat
bin\render_test.exe
bin\sign_studio.exe
```
- `render_test` should end with `146 passed, 0 failed` and `ALL CHECKS PASSED`.
- In `sign_studio`: click the window, press `H` for help, `Enter` to type a sentence, `B` for Build mode.
- `bin\sign_studio.exe --capture` writes 18 images and GIFs to `output_final`.

If anything fails to build, copy the error and send it to the team.
