#pragma once
#include "ftxui/component/app.hpp"
#include <functional>
#include <string>

class TitleScreen {
    public:
        TitleScreen(std::function<void(std::string&)> on_engine_provided);

        ftxui::Component get_component();
        void set_error(std::string mssg);
    private:
        std::string file_path_;
        std::string error_message_;

        std::function<void(std::string&)> on_engine_provided_;

        void validate_input();
        void build_ui();

        ftxui::Component main_layout_;
        ftxui::Component input_component_;
};
