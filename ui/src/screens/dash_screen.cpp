#include "screens/dash_screen.hpp"
#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "swarm.hpp"
#include <iomanip>

using namespace ftxui;

DashScreen::DashScreen() : manager_(nullptr) { build_ui(); }

Component DashScreen::get_component() { return main_layout_; }

void DashScreen::update_stats(const SwarmState &stats) {
  current_stats_ = stats;
}

void DashScreen::attach_swarm(SwarmManager *manager) { manager_ = manager; }

void DashScreen::set_file_path(std::string &file_path) {
  file_path_ = file_path;
}

void DashScreen::build_ui() {

  main_layout_ = Renderer([this] {
    if (!manager_) {
      return vbox({filler(),
                   text("Loading Torrent Engine...") | bold | center |
                       color(Color::Yellow),
                   filler()});
    }

    current_stats_ = manager_->get_stats();

    // 1. Calculate progress factor safely
    float progress = 0.0f;
    if (current_stats_.total_pieces > 0) {
      progress = static_cast<float>(current_stats_.downloaded_pieces) /
                 static_cast<float>(current_stats_.total_pieces);
    }

    // 2. Format progress percentage string
    std::stringstream percent_ss;
    percent_ss << std::fixed << std::setprecision(1) << (progress * 100.0f)
               << "%";

    // 3. Determine Swarm Status indicator
    Element status_indicator =
        (current_stats_.active_peers > 0 ||
         current_stats_.downloaded_pieces > 0)
            ? text("● RUNNING") | color(Color::Green) | bold
            : text("○ LOADING") | color(Color::Yellow) | dim;

    // 4. Create the grid layouts for stats metrics
    Element left_column = vbox({
        hbox({text("Active Peers:  "),
              text(std::to_string(current_stats_.active_peers)) |
                  color(Color::Cyan) | bold}),
        hbox({text("Total Pool:    "),
              text(std::to_string(current_stats_.pool_size)) |
                  color(Color::GrayLight)}),
    });

    Element right_column = vbox({
        hbox({text("Downloaded:  "),
              text(std::to_string(current_stats_.downloaded_pieces)) |
                  color(Color::White)}),
        hbox({text("Total Pieces:  "),
              text(std::to_string(current_stats_.total_pieces)) |
                  color(Color::White)}),
    });

    // 5. Assemble the complete view
    return window(
        text("Dashboard ") | bold | center,
        vbox({// Row 1: Status [Left] and File Name [Right]
              hbox({status_indicator, filler(), text("File: ") | dim,
                    text(current_stats_.torrent_name) | bold |
                        color(Color::BlueLight)}),

              separator(),

              // Row 2: Progress bar with numeric overlay
              hbox({text("Progress: "), gauge(progress) | color(Color::Green),
                    text(" " + percent_ss.str()) | bold |
                        color(Color::GreenLight)}),

              separator(),

              // Row 3: Two-column grid detailing stats metrics
              hbox({left_column | flex, separator(), right_column | flex})}));
  });
}
