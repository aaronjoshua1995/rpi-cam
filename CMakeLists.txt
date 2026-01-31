cmake_minimum_required(VERSION 3.10)
project(RPI-NETWORK-STREAM)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBSSH REQUIRED libssh)

pkg_check_modules(OpenCV REQUIRED opencv4)

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
    "${CMAKE_CURRENT_SOURCE_DIR}/functions/*.cpp"
)
file(GLOB HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/functions/*.h"
)

# add_executable(record main.cpp ConfigDefaults.h ConfigDefaults.cpp SSHTunnel.h SSHTunnel.cpp VideoStream.h VideoStream.cpp MotionDetector.h MotionDetector.cpp)
add_executable(record ${SOURCES} ${HEADERS})

# Link libssh to your target
target_include_directories(record PRIVATE ${LIBSSH_INCLUDE_DIRS})
target_link_directories(record PRIVATE ${LIBSSH_LIBRARY_DIRS})
target_link_libraries(record ${LIBSSH_LIBRARIES})
target_compile_options(record PRIVATE ${LIBSSH_CFLAGS_OTHER})

# Required include dirs
target_include_directories(record PRIVATE ${GST_INCLUDE_DIRS})
target_link_directories(record PRIVATE ${GST_LIBRARY_DIRS})
target_link_libraries(record ${GST_LIBRARIES})

# Required include dirs
target_include_directories(record PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_directories(record PRIVATE ${OpenCV_LIBRARY_DIRS})
target_link_libraries(record ${OpenCV_LIBRARIES})

# Ensure compiler sees the right flags
target_compile_options(record PRIVATE ${GST_CFLAGS_OTHER})
target_compile_options(record PRIVATE ${LIBSSH_CFLAGS_OTHER})

target_link_libraries(record pthread)
