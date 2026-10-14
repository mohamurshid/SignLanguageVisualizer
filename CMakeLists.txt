cmake_minimum_required(VERSION 3.16)
project(SignLanguageVisualizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# All graphics algorithms and the hand model. Pure C++: no libraries.
add_library(graphics_core STATIC
    src/Geometry.cpp
    src/Framebuffer.cpp
    src/RasterLines.cpp
    src/RasterCircles.cpp
    src/RasterEllipses.cpp
    src/RasterPolygons.cpp
    src/Clipping.cpp
    src/Curves.cpp
    src/Canvas.cpp
    src/Hand.cpp
    src/Poses.cpp
    src/HandRenderer.cpp
    src/TestScene.cpp
)
target_include_directories(graphics_core PUBLIC src)

# Checkpoint 1 program: renders every component to .bmp images (pure C++).
add_executable(cp1_render apps/cp1_render.cpp)
target_link_libraries(cp1_render PRIVATE graphics_core)

# Automated checks for every algorithm (pure C++).
add_executable(render_test tests/render_test.cpp)
target_link_libraries(render_test PRIVATE graphics_core)

# Interactive viewer for Windows: uses only Windows' own window functions.
if(WIN32)
    add_executable(sign_viewer_win32 apps/viewer_win32.cpp)
    target_link_libraries(sign_viewer_win32 PRIVATE graphics_core user32 gdi32)
endif()

# Optional OpenGL viewer: only built if OpenGL and freeglut are installed.
find_package(OpenGL QUIET)
find_package(GLUT QUIET)
if(OpenGL_FOUND AND GLUT_FOUND)
    add_executable(sign_viewer apps/viewer_glut.cpp)
    target_link_libraries(sign_viewer PRIVATE graphics_core OpenGL::GL GLUT::GLUT)
else()
    message(STATUS "OpenGL/freeglut not found: skipping the optional viewer (sign_viewer)")
endif()
