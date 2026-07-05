#include "dashboard.hpp"
#include <algorithm>
#include <ftxui/dom/elements.hpp>
#include <string>

using namespace ftxui;

Dashboard::Dashboard(EngineState &state)
    : state_(state), screen_(ScreenInteractive::Fullscreen()) {}

void Dashboard::run() {

  auto renderer = Renderer([&] {
    std::lock_guard<std::mutex> lock(state_.mt);

    float progress = 0.0f;
    if (state_.total_pieces > 0) {
      progress =
          static_cast<float>(state_.downloaded_pieces) / state_.total_pieces;
    }

    auto dashboard_layout =
        window(text(" MicroTorrent Engine v1.0") | bold,
               vbox({text("File: " + state_.torrent_name) | color(Color::White),
                     separator(),
                     hbox({text("Progress: "),
                           gauge(progress) | flex | color(Color::BlueLight),
                           text(" " + std::to_string(state_.downloaded_pieces) +
                                "/" + std::to_string(state_.total_pieces))}),
                     separator(),
                     hbox({text("Active Connections: " +
                                std::to_string(state_.active_peers)) |
                               color(Color::Green),
                           text("  |  "),
                           text("Known Peers (Pool): " +
                                std::to_string(state_.pool_size)) |
                               color(Color::Cyan)}),
               }));

    return dashboard_layout;
  });

  screen_.Loop(renderer);
}

void Dashboard::quit() {
    screen_.ExitLoopClosure()();
}
