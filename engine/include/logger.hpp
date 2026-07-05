#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>

enum class LogLevel { DEBUG, INFO, ERROR };

enum class LogChannel { PEER, SWARM, DISK, MESSAGE,TRACKER, GENERAL };

inline std::ostream &operator<<(std::ostream &os, LogLevel level) {
  switch (level) {
  case LogLevel::DEBUG:
    os << "DEBUG";
    break;
  case LogLevel::INFO:
    os << "INFO";
    break;
  case LogLevel::ERROR:
    os << "ERROR";
    break;
  }
  return os;
}

class Logger {
private:
  static inline bool last_was_progress = false;
  static void handle_interleaving() {
    if (last_was_progress) {
      std::cout << std::endl;
      last_was_progress = false;
    }
  }

  static std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%H:%M:%S") << "."
        << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
  }

public:
  static inline LogLevel current_level = LogLevel::DEBUG;

  static void debug(const std::string &msg,
                    LogChannel ch = LogChannel::GENERAL) {
    if (current_level <= LogLevel::DEBUG && is_channel_enabled(ch)) {
      handle_interleaving();
      std::cout << "[" << timestamp() << "][DEBUG] " << msg << std::endl;
    }
  }

  static void info(const std::string &msg,
                   LogChannel ch = LogChannel::GENERAL) {
    if (current_level <= LogLevel::INFO  && is_channel_enabled(ch)) {
      handle_interleaving();
      std::cout << "[" << timestamp() << "][INFO] " << msg << std::endl;
    }
  }

  static void error(const std::string &msg,
                    LogChannel ch = LogChannel::GENERAL) {
    if (current_level <= LogLevel::ERROR  && is_channel_enabled(ch)) {
      handle_interleaving();
      std::cout << "[" << timestamp() << "][ERROR] " << msg << std::endl;
    }
  }

  static void progress(const std::string &msg, LogLevel level) {
    if (current_level <= level) {
      std::cout << "\r\x1b[2K[" << timestamp() << "][" << level << "] " << msg
                << std::flush;
      last_was_progress = true;
    }
  }

  static inline std::unordered_set<LogChannel> enabled_channels = {
      LogChannel::PEER, LogChannel::SWARM, LogChannel::DISK,LogChannel::MESSAGE,
      LogChannel::TRACKER, LogChannel::GENERAL};

  static bool is_channel_enabled(LogChannel ch) {
    return enabled_channels.count(ch) > 0;
  }

  static void disable_channel(LogChannel ch) { enabled_channels.erase(ch); }
  static void enable_channel(LogChannel ch) { enabled_channels.insert(ch); }
};
