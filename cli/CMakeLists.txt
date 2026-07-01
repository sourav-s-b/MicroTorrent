add_executable(microtorrent_cli src/main.cpp)

target_link_libraries(microtorrent_cli PRIVATE torrent_engine)
