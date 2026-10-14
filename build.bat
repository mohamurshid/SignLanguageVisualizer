@echo off
rem ===========================================================================
rem  build.bat - builds the PURE C++ programs (no libraries needed)
rem    bin\cp1_render.exe   renders every component to .bmp images
rem    bin\render_test.exe  automated checks for every algorithm
rem    bin\sign_viewer.exe  interactive window (uses only Windows' own
rem                         window functions, no graphics library)
rem
rem  Run it from "Developer Command Prompt for VS 2022" (uses cl.exe),
rem  or from any prompt where g++ is available.
rem ===========================================================================
setlocal
cd /d "%~dp0"

set SRC=src\Geometry.cpp src\Framebuffer.cpp src\RasterLines.cpp src\RasterCircles.cpp src\RasterEllipses.cpp src\RasterPolygons.cpp src\Clipping.cpp src\Curves.cpp src\Canvas.cpp src\Hand.cpp src\Poses.cpp src\HandRenderer.cpp src\TestScene.cpp

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
cl /nologo /std:c++17 /EHsc /O2 /W3 /Isrc /Fobin\obj\ %SRC% tests\render_test.cpp /Fe:bin\render_test.exe
if errorlevel 1 goto failed
cl /nologo /std:c++17 /EHsc /O2 /W3 /Isrc /Fobin\obj\ %SRC% apps\viewer_win32.cpp /Fe:bin\sign_viewer.exe user32.lib gdi32.lib
if errorlevel 1 goto failed
goto done

:gcc
echo Building with g++...
g++ -std=c++17 -O2 -Wall -Isrc %SRC% apps\cp1_render.cpp -o bin\cp1_render.exe
if errorlevel 1 goto failed
g++ -std=c++17 -O2 -Wall -Isrc %SRC% tests\render_test.cpp -o bin\render_test.exe
if errorlevel 1 goto failed
g++ -std=c++17 -O2 -Wall -Isrc %SRC% apps\viewer_win32.cpp -o bin\sign_viewer.exe -lgdi32 -luser32
if errorlevel 1 echo WARNING: the interactive viewer did not build with this g++ - the other programs are fine.
goto done

:failed
echo.
echo BUILD FAILED - see the errors above.
exit /b 1

:done
echo.
echo Build finished. Now run:
echo     bin\render_test.exe
echo     bin\cp1_render.exe
echo     bin\sign_viewer.exe    (interactive window)
echo The images are written to the "output" folder.
