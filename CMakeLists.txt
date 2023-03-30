cmake_minimum_required(VERSION 3.0.1)
project(T503)
set(CMAKE_BUILD_TYPE "Release")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)

add_executable(T503d src/T503d.c src/T503.c)
target_include_directories(T503d PUBLIC ${LIBEVDEV_INCLUDE_DIRS})
target_link_libraries(T503d ${LIBEVDEV_LIBRARIES} usb-1.0)
