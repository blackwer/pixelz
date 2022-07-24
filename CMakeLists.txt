cmake_minimum_required(VERSION 3.11)

project(
  pixelz
  VERSION 0.0.1
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)

set(PIXELZ_INCLUDES
  ${PROJECT_SOURCE_DIR}/include
)

find_package(raylib QUIET)
if (NOT raylib_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 4.0.0
    )
    FetchContent_MakeAvailable(raylib)
endif()

find_package(raylib_cpp QUIET)
if (NOT raylib_cpp_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        raylib_cpp
        GIT_REPOSITORY https://github.com/RobLoach/raylib-cpp.git
        GIT_TAG v4.2.2
    )
    FetchContent_MakeAvailable(raylib_cpp)
endif()


add_executable(pixelz src/pixelz.cpp)
target_include_directories(pixelz PRIVATE ${PIXELZ_INCLUDES})
target_link_libraries(pixelz PRIVATE raylib raylib_cpp)
