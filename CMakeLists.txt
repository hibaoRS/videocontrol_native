cmake_minimum_required(VERSION 3.15)
project(helo_lvgl)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -static-libgcc -static-libstdc++")
set(CMAKE_BUILD_TYPE Debug )
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")

file(GLOB_RECURSE INCLUDES "lvgl/*.h" "lv_drivers/*.h" "lv_examples/*.h" "./*.h")
file(GLOB_RECURSE SOURCES "lvgl/*.c" "lv_drivers/*.c" "lv_examples/*.c")
file(GLOB_RECURSE COMMON "common/*.h" "common/*.c")

INCLUDE_DIRECTORIES(include)
link_directories(lib)

add_executable(helo_lvgl main.cpp ${COMMON} ${SOURCES} ${INCLUDES})
#add_executable(helo_lvgl ${COMMON} main2.c)
target_link_libraries(helo_lvgl dl pthread mpi libaec.a libanr.a libresampler.a libVoiceEngine.a libjpeg.a)
