set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
    GIT_TAG main
)
FetchContent_MakeAvailable(ftxui)

file(GLOB UI_SOURCES "src/*.cpp")

add_executable(app ${UI_SOURCES})

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(app PRIVATE
    torrent_engine
    ftxui::screen
    ftxui::dom
    ftxui::component
)

if(MSVC)
    target_compile_options(app PRIVATE /utf-8)
endif()
