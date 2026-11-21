cmake_minimum_required(VERSION 3.15)
project(BarryStroke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Where the OpenFX SDK lives. Change this one line if the SDK moves.
set(OFX_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../openfx" CACHE PATH "Path to the OpenFX SDK")

# An OFX plugin is a DLL that the host loads at runtime: a MODULE library.
# Add new .cpp files to this list.
add_library(BarryStroke MODULE
    src/Plugin.cpp
    src/Host.cpp
    src/Inspector.cpp
    src/FrameParams.cpp
    src/Render.cpp
)

# Where to find ofxImageEffect.h, ofxCore.h and the other OpenFX headers.
target_include_directories(BarryStroke PRIVATE "${OFX_SDK_DIR}/include")

# Name the file BarryStroke.ofx instead of libBarryStroke.dll.
set_target_properties(BarryStroke PROPERTIES PREFIX "" SUFFIX ".ofx")

# Link the C++ runtime statically so the .ofx doesn't depend on VCRUNTIME140*.dll.
if(MSVC)
  set_property(TARGET BarryStroke PROPERTY
      MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
