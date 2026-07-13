#include "app.hpp"
#include "logger.hpp"

int main(){

    Logger::console_enabled_log = false;

    AppRunner app;
    app.run();
    return 0;
}
