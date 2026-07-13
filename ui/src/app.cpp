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
    std::string response = TrackerClient::announce(
        torrent.announce_url, torrent.info_hash, torrent.total_length);

    size_t header_end = response.find("\r\n\r\n");
    if (header_end == std::string::npos) {
      throw std::runtime_error(
          "Invalid Tracker Response: Missing HTTP body delimiter.");
    }

    std::string body = response.substr(header_end + 4);
    Logger::debug("Response Arrived");

    size_t parse_index = 0;
    BencodeNode tracker_node = parse_bencode(body, parse_index);
    BencodeDict tracker_dict = std::get<BencodeDict>(tracker_node.data);

    std::string peers_binary =
        std::get<std::string>(tracker_dict.at("peers").data);

    Logger::info("Successfully extracted " +
                 std::to_string(peers_binary.length() / 6) +
                 " peers from the tracker.");

    std::vector<PeerData> peer_list;
    for (size_t i = 0; i < peers_binary.length(); i += 6) {


      uint8_t ip1 = static_cast<uint8_t>(peers_binary[i]);
      uint8_t ip2 = static_cast<uint8_t>(peers_binary[i + 1]);
      uint8_t ip3 = static_cast<uint8_t>(peers_binary[i + 2]);
      uint8_t ip4 = static_cast<uint8_t>(peers_binary[i + 3]);

      PeerData pd;
      pd.ip = std::to_string(ip1) + "." + std::to_string(ip2) + "." +
              std::to_string(ip3) + "." + std::to_string(ip4);


      uint8_t port_high = static_cast<uint8_t>(peers_binary[i + 4]);
      uint8_t port_low = static_cast<uint8_t>(peers_binary[i + 5]);
      pd.port = (port_high << 8) | port_low;

      peer_list.push_back(pd);
    }

    // 1. Fire up the Swarm Manager with the freshly parsed data
    uint32_t max_connections = 13;
    swarm_manager_ =
        std::make_unique<SwarmManager>(torrent, peer_list, max_connections);

    // 2. Attach it to your dashboard screen
    dash_screen_.attach_swarm(swarm_manager_.get());

    // 3. Spawns network loop on background thread
    std::thread network_thread([this]() { swarm_manager_->start_download(); });
    network_thread.detach();

    // 4. Switch tabs to the dashboard layout
    active_tab_ = 1;

  } catch (const std::exception &e) {
    Logger::error("Failed to start swarm: " + std::string(e.what()));
    active_tab_ = 0;
    title_screen_.set_error(std::string("Failed to start swarm: " + std::string(e.what())));
  }
}
