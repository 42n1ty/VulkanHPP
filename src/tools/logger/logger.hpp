#pragma once

#include <fmt/format.h>
#include <fmt/color.h>
#include <fmt/chrono.h>

#include <string_view>
#include <chrono>
#include <thread>
#include <sstream>
#include <mutex>

template <>
struct fmt::formatter<std::thread::id> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }
  template <typename FormatContext>
  auto format(const std::thread::id& id, FormatContext& ctx) const -> decltype(ctx.out()) {
    std::stringstream ss;
    ss << id;
    return fmt::format_to(ctx.out(), "{}", ss.str());
  }
};

namespace V {
  class Logger {
  public:
    
    template<typename... Args>
    static void info(fmt::format_string<Args...> fmt_str, Args&&... args) {
        log(fmt::color::aquamarine, "INFO", fmt_str, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void warn(fmt::format_string<Args...> fmt_str, Args&&... args) {
        log(fmt::color::light_yellow, "WARN", fmt_str, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void error(fmt::format_string<Args...> fmt_str, Args&&... args) {
        log(fmt::color::red, "ERROR", fmt_str, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void debug(fmt::format_string<Args...> fmt_str, Args&&... args) {
        log(fmt::color::light_green, "DEBUG", fmt_str, std::forward<Args>(args)...);
    }
    
  private:
    template<typename... Args>
    static void log(fmt::color color, std::string_view level, fmt::format_string<Args...> fmt_str, Args&&... args) {
      auto now = std::chrono::system_clock::now();
      std::string user_message = fmt::format(fmt_str, std::forward<Args>(args)...);
      std::string formattedMsg = fmt::format(fmt::fg(color), "{:%H:%M:%S} [{}] [{}] {}\n",
                              fmt::localtime(std::chrono::system_clock::to_time_t(now)),
                              std::this_thread::get_id(),
                              level,
                              user_message);
      {
        std::lock_guard<std::mutex> guard(m_logMutex);
        fmt::print(stderr, "{}", formattedMsg);
      }
    }
    inline static std::mutex m_logMutex;
};
} // V