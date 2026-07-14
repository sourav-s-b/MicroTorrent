cmake_minimum_required(VERSION 3.15)
project(MicroTorrent CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOGDI _WIN32_WINNT=0x0A00)
    add_compile_options(/EHsc)
endif()

find_package(OpenSSL REQUIRED)

include(FetchContent)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
)
FetchContent_MakeAvailable(asio)

# Add the modular sub-projects
add_subdirectory(engine)
add_subdirectory(cli)
add_subdirectory(ui)
