#include "screens/title_screen.hpp"
#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/dom/elements.hpp"

using namespace ftxui;

const auto LOGO_ASCI = vbox({
    text(R"( __  __  _                             _______                                _   )"),
    text(R"(|  \/  |(_)                           |__   __|                              | |  )"),
    text(R"(| \  / | _   ___  _ __  ___              | |  ___   _ __  _ __   ___  _ __   | |_ )"),
    text(R"(| |\/| || | / __|| '__|/ _ \             | | / _ \ | '__|| '__| / _ \| '_ \  | __|)"),
    text(R"(| |  | || || (__ | |  | (_) |            | || (_) || |   | |   |  __/| | | | | |_ )"),
    text(R"(|_|  |_||_| \___||_|   \___/             |_| \___/ |_|   |_|    \___||_| |_|  \__|)")
}) | bold | color(Color::GrayLight) | center;
TitleScreen::TitleScreen(std::function<void(std::string&)> on_engine_provided) : on_engine_provided_(on_engine_provided) { build_ui(); }

void TitleScreen::build_ui() {

  InputOption input_options;
  input_options.on_enter = [this] { validate_input(); };
  input_options.transform = [](InputState state) {
    Element e = state.element | bgcolor(Color::Default);
    return e;
  };

  input_component_ = Input(&file_path_, "Torrent Path..", input_options);

  auto container = Container::Vertical({
          input_component_
      });
  main_layout_ = Renderer(container, [&] {
    return vbox({LOGO_ASCI,

                 hbox({input_component_->Render() | flex}) | border,

                 (error_message_.empty()
                      ? text("")
                      : text(error_message_) | color(Color::Red) | center)}) |
           center;
  });
}

Component TitleScreen::get_component() { return main_layout_; }

void TitleScreen::set_error(std::string msg) {
    error_message_ = msg;
}

void TitleScreen::validate_input() {
    error_message_ = "";

    // Clean up paths pasted from terminal frames (e.g. carriage returns)
    if (!file_path_.empty() && (file_path_.back() == '\n' || file_path_.back() == '\r')) {
        file_path_.pop_back();
    }

    if (file_path_.empty()) {
        error_message_ = "Error: Path cannot be empty.";
        return;
    }

    if (file_path_.length() <= 8 || file_path_.compare(file_path_.length() - 8, 8 , ".torrent") != 0 ) {
        error_message_ = "Error: Not a valid .torrent file path";
        return;
    }

    if (on_engine_provided_) {
        on_engine_provided_(file_path_);
    }
}
