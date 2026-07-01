#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

enum class LogLevel { DEBUG, INFO, ERROR };

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
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

      std::ostringstream oss;
      oss << std::put_time(std::localtime(&t),"%H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count();

      return oss.str();
  }

public:
  static inline LogLevel current_level = LogLevel::DEBUG;

  static void debug(const std::string &msg) {
    if (current_level <= LogLevel::DEBUG) {
      handle_interleaving();
      std::cout << "[" << timestamp() << "][DEBUG] " << msg << std::endl;
    }
  }

  static void info(const std::string &msg) {
    if (current_level <= LogLevel::INFO) {
        handle_interleaving();
        std::cout << "[" << timestamp() << "][INFO] " << msg << std::endl;
    }
  }

  static void error(const std::string &msg) {
    if (current_level <= LogLevel::ERROR) {
        handle_interleaving();
        std::cout << "[" << timestamp() << "][ERROR] " << msg << std::endl;
    }
  }

  static void progress(const std::string &msg, LogLevel level) {
    if (current_level <= level) {
        std::cout << "\r\x1b[2K[" << timestamp() << "][" << level << "] " << msg << std::flush;
      last_was_progress = true;
    }
  }
};
