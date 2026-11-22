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
    src/Transform.cpp
    src/TransformedShapes.cpp
    src/TransformScene.cpp
    src/Animation.cpp
    src/GifWriter.cpp
)
target_include_directories(graphics_core PUBLIC src)

# The 3D version (Checkpoint 3 and the final project): maths, hand, meshes,
# body, camera, sign dictionary, timeline, speller. Pure C++ - no OpenGL here.
add_library(graphics_3d STATIC
    src/Math3D.cpp
    src/Hand3D.cpp
    src/Mesh.cpp
    src/HandMesh.cpp
    src/Body.cpp
    src/Camera.cpp
    src/Handshapes.cpp
    src/Signs3D.cpp
    src/Timeline.cpp
    src/Speller.cpp
)
target_link_libraries(graphics_3d PUBLIC graphics_core)

# Checkpoint 1 program: renders every component to .bmp images (pure C++).
add_executable(cp1_render apps/cp1_render.cpp)
target_link_libraries(cp1_render PRIVATE graphics_core)

# Checkpoint 2 program: transformations and animations to .bmp and .gif (pure C++).
add_executable(cp2_animate apps/cp2_animate.cpp)
target_link_libraries(cp2_animate PRIVATE graphics_core)

# Automated checks for every algorithm, 2D and 3D (pure C++).
add_executable(render_test tests/render_test.cpp tests/final_tests.cpp)
target_link_libraries(render_test PRIVATE graphics_3d)

# Interactive viewer for Windows: uses only Windows' own window functions.
if(WIN32)
    add_executable(sign_viewer_win32 apps/viewer_win32.cpp apps/ViewerCore.cpp)
    target_include_directories(sign_viewer_win32 PRIVATE apps)
    target_link_libraries(sign_viewer_win32 PRIVATE graphics_core user32 gdi32)
endif()

# OpenGL: the 3D application.
find_package(OpenGL QUIET)
find_package(GLUT QUIET)

# On Windows it needs only Windows' own OpenGL (opengl32): nothing to install.
if(WIN32 AND OpenGL_FOUND)
    add_executable(sign_studio apps/studio_win32.cpp apps/StudioCore.cpp apps/StudioCapture.cpp src/GLRenderer.cpp)
    target_include_directories(sign_studio PRIVATE apps)
    target_link_libraries(sign_studio PRIVATE graphics_3d OpenGL::GL user32 gdi32)
endif()

# With freeglut (any system): the same application in a GLUT window, and the
# optional OpenGL version of the 2D viewer.
if(OpenGL_FOUND AND GLUT_FOUND)
    add_executable(sign_studio_glut apps/studio_glut.cpp apps/StudioCore.cpp apps/StudioCapture.cpp src/GLRenderer.cpp)
    target_include_directories(sign_studio_glut PRIVATE apps)
    target_link_libraries(sign_studio_glut PRIVATE graphics_3d OpenGL::GL GLUT::GLUT)
    add_executable(sign_viewer apps/viewer_glut.cpp apps/ViewerCore.cpp)
    target_include_directories(sign_viewer PRIVATE apps)
    target_link_libraries(sign_viewer PRIVATE graphics_core OpenGL::GL GLUT::GLUT)
else()
    message(STATUS "OpenGL/freeglut not found: skipping the GLUT programs (sign_studio_glut, sign_viewer)")
endif()
