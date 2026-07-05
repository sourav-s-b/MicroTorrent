#pragma once


#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <mutex>
#include <string>
#include <vector>
struct EngineState {
  std::mutex mt;

  std::string torrent_name = "Initializing...";
  uint32_t downloaded_pieces = 0;
  uint32_t total_pieces = 1;

  uint32_t active_peers = 0;
  uint32_t pool_size = 0;

  std::vector<std::string> log_messages;
};

class Dashboard {
public:
    Dashboard(EngineState& state);

    void run();
    void quit();

private:
    EngineState& state_;
    ftxui::ScreenInteractive screen_;
};
