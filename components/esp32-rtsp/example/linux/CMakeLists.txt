cmake_minimum_required(VERSION 3.5 FATAL_ERROR)  

project(rtsp_test)  

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

include_directories(../../src)
aux_source_directory(../../src SRCS)
aux_source_directory(../../src/port SRCS)

list(APPEND SRCS "server.c" "v4l2uvc.c" "../simple/media/audio/wave.c" "yuv2jpg.c")

set(CMAKE_C_FLAGS "${CAMKE_C_FLAGS}  -pthread -O0")

add_executable(rtsp_test ${SRCS})
