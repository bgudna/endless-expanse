cmake_minimum_required(VERSION 3.20)
project(EndlessExpanse VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(EE_WARNINGS_AS_ERRORS "Treat warnings as errors for game targets" ON)
option(EE_BUILD_TESTS "Build unit tests" ON)

include(cmake/Warnings.cmake)

# ---- Dependencies ---------------------------------------------------------
find_package(SDL2 REQUIRED)

include(FetchContent)
if(EE_BUILD_TESTS)
    FetchContent_Declare(doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.12)
    FetchContent_MakeAvailable(doctest)
endif()

# ---- Core library: no SDL / GL includes allowed here ----------------------
add_library(ee_core STATIC
    src/core/FixedTimestep.cpp
    src/core/StateStack.cpp
    src/core/Log.cpp)
target_include_directories(ee_core PUBLIC src)
ee_enable_warnings(ee_core)

# ---- Game executable --------------------------------------------------------
add_executable(endless_expanse
    src/main.cpp
    src/app/Application.cpp
    src/app/States.cpp
    src/platform/Window.cpp
    src/platform/Input.cpp)
target_link_libraries(endless_expanse PRIVATE ee_core SDL2::SDL2)
if(TARGET SDL2::SDL2main)
    target_link_libraries(endless_expanse PRIVATE SDL2::SDL2main)
endif()
if(APPLE)
    target_compile_definitions(endless_expanse PRIVATE GL_SILENCE_DEPRECATION)
    target_link_libraries(endless_expanse PRIVATE "-framework OpenGL")
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(endless_expanse PRIVATE OpenGL::GL)
endif()
ee_enable_warnings(endless_expanse)

# ---- Tests --------------------------------------------------------------------
if(EE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
