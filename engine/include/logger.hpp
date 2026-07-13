#pragma once

#include <chrono>
#include <ctime>
#include <deque>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

enum class LogLevel { DEBUG, INFO, ERROR };

enum class LogChannel { PEER, SWARM, DISK, MESSAGE, TRACKER, GENERAL };

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
  static inline std::deque<std::string> log_buffer;
  static inline const size_t MAX_LOGS = 100;
  static inline std::mutex log_mtx;
  
  static void handle_interleaving() {
    if (last_was_progress) {
      if (console_enabled_log)
        std::cout << std::endl;
      last_was_progress = false;
    }
  }

  static void write_log(std::string message, bool progress = false) {
      std::lock_guard<std::mutex> lock(log_mtx);
    if (log_buffer.size() >= MAX_LOGS) {
      log_buffer.pop_front();
    }

    log_buffer.push_back(message);
    if (console_enabled_log) {
      std::cout << message;
      if (!progress) {
        std::cout << std::endl;
      } else {
        std::cout << std::flush;
      }
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
  static inline bool console_enabled_log = true;
  static inline LogLevel current_level = LogLevel::DEBUG;

  static std::vector<std::string> get_logs() {
      std::lock_guard<std::mutex> lock(log_mtx);
      return std::vector<std::string>(log_buffer.begin(),log_buffer.end());
  }
  
  static void debug(const std::string &msg,
                    LogChannel ch = LogChannel::GENERAL) {
    if (current_level <= LogLevel::DEBUG && is_channel_enabled(ch)) {
      handle_interleaving();
      write_log("[" + timestamp() + "][DEBUG] " + msg);
    }
  }

  static void info(const std::string &msg,
                   LogChannel ch = LogChannel::GENERAL) {
    if (current_level <= LogLevel::INFO && is_channel_enabled(ch)) {
      handle_interleaving();
      write_log("[" + timestamp() + "][INFO] " + msg);
    }
  }

  static void error(const std::string &msg,
                    LogChannel ch = LogChannel::GENERAL) {
    if (current_level <= LogLevel::ERROR && is_channel_enabled(ch)) {
      handle_interleaving();
      write_log("[" + timestamp() + "][ERROR] " + msg);
    }
  }

  static void progress(const std::string &msg, LogLevel level) {
    if (current_level <= level) {
      write_log("\r\x1b[2K[" + timestamp() + "] " + msg, true);
      last_was_progress = true;
    }
  }

  static inline std::unordered_set<LogChannel> enabled_channels = {
      LogChannel::PEER,    LogChannel::SWARM,   LogChannel::DISK,
      LogChannel::MESSAGE, LogChannel::TRACKER, LogChannel::GENERAL};

  static bool is_channel_enabled(LogChannel ch) {
    return enabled_channels.count(ch) > 0;
  }

  static void disable_channel(LogChannel ch) { enabled_channels.erase(ch); }
  static void enable_channel(LogChannel ch) { enabled_channels.insert(ch); }
};
