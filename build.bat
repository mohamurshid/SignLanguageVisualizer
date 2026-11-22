@echo off
rem ===========================================================================
rem  build.bat - builds every program of the project (no libraries needed)
rem    bin\cp1_render.exe   Checkpoint 1: every component to .bmp images
rem    bin\cp2_animate.exe  Checkpoint 2: transformations + animations
rem    bin\render_test.exe  automated checks for every algorithm (2D and 3D)
rem    bin\sign_viewer.exe  the 2D interactive viewer (Checkpoint 2)
rem    bin\sign_studio.exe  the 3D OpenGL application (Checkpoint 3 / final)
rem                         uses only the OpenGL that comes with Windows
rem
rem  Run it from "Developer Command Prompt for VS 2022" (uses cl.exe),
rem  or from any prompt where g++ is available.
rem ===========================================================================
setlocal
cd /d "%~dp0"

set SRC=src\Geometry.cpp src\Framebuffer.cpp src\RasterLines.cpp src\RasterCircles.cpp src\RasterEllipses.cpp src\RasterPolygons.cpp src\Clipping.cpp src\Curves.cpp src\Canvas.cpp src\Hand.cpp src\Poses.cpp src\HandRenderer.cpp src\TestScene.cpp src\Transform.cpp src\TransformedShapes.cpp src\TransformScene.cpp src\Animation.cpp src\GifWriter.cpp
set SRC3D=src\Math3D.cpp src\Hand3D.cpp src\Mesh.cpp src\HandMesh.cpp src\Body.cpp src\Camera.cpp src\Handshapes.cpp src\Signs3D.cpp src\Timeline.cpp src\Speller.cpp
set STUDIO=src\GLRenderer.cpp apps\StudioCore.cpp apps\StudioCapture.cpp apps\studio_win32.cpp

if not exist bin mkdir bin
if not exist bin\obj mkdir bin\obj

where cl >nul 2>nul
if %errorlevel%==0 goto msvc
where g++ >nul 2>nul
if %errorlevel%==0 goto gcc

echo No C++ compiler found.
echo Open "Developer Command Prompt for VS 2022" from the Start menu,
echo go to this folder and run build.bat again.
exit /b 1

:msvc
echo Building with Microsoft C++ (cl)...
cl /nologo /std:c++17 /EHsc /O2 /W3 /Isrc /Fobin\obj\ %SRC% apps\cp1_render.cpp /Fe:bin\cp1_render.exe
if errorlevel 1 goto failed
cl /nologo /std:c++17 /EHsc /O2 /W3 /Isrc /Fobin\obj\ %SRC% apps\cp2_animate.cpp /Fe:bin\cp2_animate.exe
if errorlevel 1 goto failed
cl /nologo /std:c++17 /EHsc /O2 /W3 /Isrc /Fobin\obj\ %SRC% %SRC3D% tests\render_test.cpp tests\final_tests.cpp /Fe:bin\render_test.exe
if errorlevel 1 goto failed
cl /nologo /std:c++17 /EHsc /O2 /W3 /Isrc /Fobin\obj\ /Iapps %SRC% apps\ViewerCore.cpp apps\viewer_win32.cpp /Fe:bin\sign_viewer.exe user32.lib gdi32.lib
if errorlevel 1 goto failed
cl /nologo /std:c++17 /EHsc /O2 /W3 /Isrc /Fobin\obj\ /Iapps %SRC% %SRC3D% %STUDIO% /Fe:bin\sign_studio.exe opengl32.lib user32.lib gdi32.lib
if errorlevel 1 goto failed
goto done

:gcc
echo Building with g++...
g++ -std=c++17 -O2 -Wall -Isrc %SRC% apps\cp1_render.cpp -o bin\cp1_render.exe
if errorlevel 1 goto failed
g++ -std=c++17 -O2 -Wall -Isrc %SRC% apps\cp2_animate.cpp -o bin\cp2_animate.exe
if errorlevel 1 goto failed
g++ -std=c++17 -O2 -Wall -Isrc %SRC% %SRC3D% tests\render_test.cpp tests\final_tests.cpp -o bin\render_test.exe
if errorlevel 1 goto failed
g++ -std=c++17 -O2 -Wall -Isrc -Iapps %SRC% apps\ViewerCore.cpp apps\viewer_win32.cpp -o bin\sign_viewer.exe -lgdi32 -luser32
if errorlevel 1 echo WARNING: the 2D viewer did not build with this g++ - the other programs are fine.
g++ -std=c++17 -O2 -Wall -Isrc -Iapps %SRC% %SRC3D% %STUDIO% -o bin\sign_studio.exe -lopengl32 -lgdi32 -luser32
if errorlevel 1 echo WARNING: the 3D studio did not build with this g++ - the other programs are fine.
goto done

:failed
echo.
echo BUILD FAILED - see the errors above.
exit /b 1

:done
echo.
echo Build finished. Now run:
echo     bin\render_test.exe              (automated checks)
echo     bin\sign_studio.exe              (the 3D application - press H for help)
echo     bin\sign_studio.exe --capture    (renders the final-project images to output_final)
echo     bin\cp1_render.exe, bin\cp2_animate.exe, bin\sign_viewer.exe  (Checkpoints 1 and 2)
