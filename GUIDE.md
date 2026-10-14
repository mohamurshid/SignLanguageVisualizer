# Joy Wanjohi – your part of Checkpoint 1

**Push order:** **4th** – after Elvis.
**Issues you close:** #11, #12, #13

## Your files

The files in this package are already in the right folders.

| File | What it does |
|---|---|
| `src/RasterPolygons.cpp` | Polygon outline and scan-line fill with an Edge Table and an Active Edge Table. |
| `src/Clipping.h`, `src/Clipping.cpp` | Cohen–Sutherland line clipping (outcodes) and Sutherland–Hodgman polygon clipping. |
| `src/Canvas.h`, `src/Canvas.cpp` | Clip-then-draw for every shape: lines and outlines through Cohen–Sutherland, filled shapes through Sutherland–Hodgman, circles and ellipses through point clipping. |
| `apps/viewer_win32.cpp` | The interactive window (`sign_viewer.exe`), using only Windows' built-in window functions: keys, timer, copying our pixels to the screen. |

## Before you push: make sure you can explain it

You will be asked about your part. Read your files and Sections 5.1–5.2 and 6 of `docs/algorithms.md`. Then check that you can answer:

- Scan-line fill: what the Edge Table and Active Edge Table hold, the even-odd rule, why the top row of each edge is left out, and why horizontal edges are skipped.
- Cohen–Sutherland: the 4-bit outcodes, trivial accept and trivial reject, and the worked example (0,20) → (60,50).
- Sutherland–Hodgman: the four inside/outside cases, and its limitation with concave polygons.
- Why Canvas uses geometric clipping for lines and polygons but point clipping for circles and ellipses.
- How the viewer gets our framebuffer onto the screen (`StretchDIBits`) and handles keys.

## How to push

You only do steps 1–3 once.

1. **Install Git** from https://git-scm.com/download/win if you don't have it.
2. **Tell Git who you are.** Use the email on *your own* GitHub account; that is how your commits are counted as yours:
   ```bat
   git config --global user.name "Joy Wanjohi"
   git config --global user.email "your-github-email@example.com"
   ```
3. **Get the repository.** Accept Abdalla's collaborator invite (check your email or github.com/notifications), then:
   ```bat
   cd /d C:\projects
   git clone https://github.com/ABDALLA-USERNAME/sign-language-visualizer.git
   cd sign-language-visualizer
   ```
4. **Copy your files in.** Unzip this package, then copy its folders into the repository folder, keeping the same structure. This command does it, skipping this guide:
   ```bat
   robocopy "C:\path\to\unzipped\4-Joy" . /E /XF GUIDE.md
   ```
5. **Commit and push.** Get the latest work first, then make one commit per step below:
   ```bat
   git pull
   git add src\RasterPolygons.cpp
   git commit -m "Add scan-line polygon fill with edge tables (closes #11)"
   git add src\Clipping.h src\Clipping.cpp src\Canvas.h src\Canvas.cpp
   git commit -m "Add Cohen-Sutherland and Sutherland-Hodgman clipping and Canvas (closes #12)"
   git add apps\viewer_win32.cpp
   git commit -m "Add interactive Windows viewer (closes #13)"
   git push
   ```
   If `git push` is refused because someone pushed before you, run `git pull --rebase` and then `git push` again.
6. Check on GitHub that your commits show **your** name, and that the issues closed.

## Round 2: one change of your own

Once all five have pushed, make one real change of your own, test it, and push it as a separate commit. This shows genuine development in the history and gives you something of your own to talk about.

Suggestion: Add a key to `viewer_win32.cpp`, for example `R` to go back to sign 5 with everything reset, then rebuild and try it.
