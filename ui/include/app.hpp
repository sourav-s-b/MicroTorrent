
#pragma once

#include "ftxui/component/app.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "screens/dash_screen.hpp"
#include "screens/title_screen.hpp"
#include "swarm.hpp"
#include <memory>
class AppRunner{
    public:
        AppRunner();
        void run();

    private:

        int active_tab_ = 0;

        ftxui::ScreenInteractive screen_;

        TitleScreen title_screen_;
        DashScreen dash_screen_;

        std::unique_ptr<SwarmManager> swarm_manager_;

        ftxui::Component main_layout_;
        ftxui::Component router_;

        void build_ui();
        void start_download(const std::string &file_path);
};
