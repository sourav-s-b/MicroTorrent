cmake_minimum_required(VERSION 3.15)
project(MicroTorrent CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
)
FetchContent_MakeAvailable(asio)

file(GLOB SOURCE_FILES "src/*.cpp")

add_executable(asio_test ${SOURCE_FILES})

target_include_directories(asio_test PRIVATE
    include
    ${asio_SOURCE_DIR}/asio/include
)
target_compile_definitions(asio_test PRIVATE ASIO_STANDALONE)
