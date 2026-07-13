#pragma once
#include "ftxui/component/app.hpp"
#include "swarm.hpp"
#include <string>


class DashScreen{
public:
    DashScreen();

    ftxui::Component get_component();
    void set_file_path(std::string& file_path);
    void attach_swarm(SwarmManager* manager);
    void update_stats(const SwarmState& stats);
private:
    std::string file_path_;
    SwarmState current_stats_;
    ftxui::Component main_layout_;
    SwarmManager* manager_;

    void build_ui();

};
