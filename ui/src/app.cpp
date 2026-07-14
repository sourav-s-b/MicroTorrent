#include "app.hpp"
#include "bencode.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "logger.hpp"
#include "screens/dash_screen.hpp"
#include "tracker.hpp"
#include <cstddef>

using namespace ftxui;

AppRunner::AppRunner()
    : screen_(ScreenInteractive::Fullscreen()),
      title_screen_(TitleScreen([this](std::string &file_path) {
        dash_screen_.set_file_path(file_path);
        active_tab_ = 1;
        this->start_download(file_path);
      })),
      dash_screen_(DashScreen()) {

  build_ui();
}

void AppRunner::build_ui() {

  router_ = Container::Tab(
      {title_screen_.get_component(), dash_screen_.get_component()},
      &active_tab_);

  main_layout_ = CatchEvent(router_, [this](Event event) {
    if (active_tab_ == 0 && event == Event::Escape) {
      screen_.ExitLoopClosure()();
      return true;
    }
    return false;
  });
}

void AppRunner::run() { screen_.Loop(main_layout_); }

void AppRunner::start_download(const std::string &file_path) {
  try {
    TorrentFile torrent(file_path);

    Logger::info("Initializing Tracker Connection...");

    // 1. Create the instance and fetch the peers directly
    TrackerClient tracker;

    // Note: If you named your public method 'fetch_peers' to handle multiple
    // URLs, use that instead of 'announce'
    std::vector<PeerData> peer_list =
        tracker.fetch_peers(torrent.announce_url, torrent.announce_list,
                            torrent.info_hash, torrent.total_length);

    Logger::info("Successfully extracted " + std::to_string(peer_list.size()) +
                 " peers from the tracker.");

    // 2. Fire up the Swarm Manager with the freshly parsed data
    uint32_t max_connections = 13;
    swarm_manager_ =
        std::make_unique<SwarmManager>(torrent, peer_list, max_connections);

    // 3. Attach it to your dashboard screen
    dash_screen_.attach_swarm(swarm_manager_.get());

    // 4. Spawns network loop on background thread
    std::thread network_thread([this]() { swarm_manager_->start_download(); });
    network_thread.detach();

    // 5. Switch tabs to the dashboard layout
    active_tab_ = 1;

  } catch (const std::exception &e) {
    Logger::error("Failed to start swarm: " + std::string(e.what()));
    active_tab_ = 0;
    title_screen_.set_error(
        std::string("Failed to start swarm: " + std::string(e.what())));
  }
}
