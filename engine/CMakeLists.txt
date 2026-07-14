file(GLOB ENGINE_SOURCES "src/*.cpp")

add_library(torrent_engine STATIC ${ENGINE_SOURCES})

target_include_directories(torrent_engine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${asio_SOURCE_DIR}/asio/include
)

target_compile_definitions(torrent_engine PUBLIC ASIO_STANDALONE)

target_link_libraries(torrent_engine PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
)
