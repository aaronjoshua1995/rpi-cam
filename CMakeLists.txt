cmake_minimum_required(VERSION 3.10)
project(RPI-CAM)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)

# Find both GStreamer and RTSP Server
pkg_check_modules(GST REQUIRED
    gstreamer-1.0
    gstreamer-app-1.0
    gstreamer-base-1.0
    gstreamer-video-1.0
)

include_directories(
    ${CMAKE_SOURCE_DIR}/functions
)

file(GLOB SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
file(GLOB HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_executable(app ${SOURCES} ${HEADERS})

# Required include dirs
target_include_directories(app PRIVATE ${GST_INCLUDE_DIRS})
target_link_directories(app PRIVATE ${GST_LIBRARY_DIRS})
target_link_libraries(app ${GST_LIBRARIES})

