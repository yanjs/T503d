cmake_minimum_required(VERSION 3.0.1)
project(T503)
set(CMAKE_BUILD_TYPE "Release")

add_executable(T503d src/T503d.c src/T503.c)
target_include_directories(T503d PUBLIC /include/libevdev-1.0/)
target_link_libraries(T503d evdev usb-1.0)