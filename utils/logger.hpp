#ifndef LOGGER_HPP
#define LOGGER_HPP
#include <string>
#include <stdint.h>
#include <stdarg.h>
#include <cstdio> // std::snprintf()
#include <stdexcept>

#define LOGGER_BUFFER_SIZE (20*1024)

enum LOGGER_LEVEL {
    LOGGER_DEBUG_LEVEL,
    LOGGER_INFO_LEVEL,
    LOGGER_WARN_LEVEL,
    LOGGER_ERROR_LEVEL
};

class Logger
{
public:
    static Logger* get_instance();

public:
    void set_filename(const std::string filename);
    void set_level(enum LOGGER_LEVEL level);
    enum LOGGER_LEVEL get_level();
    char* get_buffer();
    
    void logf(const char* level, const char* buffer, const char* filename, int line);

private:
    Logger(const std::string filename = "", enum LOGGER_LEVEL level = LOGGER_INFO_LEVEL);
    ~Logger();

private:
    static Logger s_logger;
    std::string filename_;
    enum LOGGER_LEVEL level_;
    char buffer_[LOGGER_BUFFER_SIZE];
};

inline void snprintbuffer(char* buffer, size_t size, const char* fmt, ...) {
    va_list ap;
 
    va_start(ap, fmt);
    int ret_len = vsnprintf(buffer, size, fmt, ap);
    buffer[ret_len] = 0;
    va_end(ap);

    return;
}

#define log_errorf(...) \
    if (Logger::get_instance()->get_level() <= LOGGER_ERROR_LEVEL)         \
    {                                                                      \
        char* buffer = Logger::get_instance()->get_buffer();               \
        snprintbuffer(buffer, LOGGER_BUFFER_SIZE, __VA_ARGS__);            \
        Logger::get_instance()->logf("ERROR", buffer, __FILE__, __LINE__); \
    }

#define log_warnf(...) \
    if (Logger::get_instance()->get_level() <= LOGGER_WARN_LEVEL)          \
    {                                                                      \
        char* buffer = Logger::get_instance()->get_buffer();               \
        snprintbuffer(buffer, LOGGER_BUFFER_SIZE, __VA_ARGS__);            \
        Logger::get_instance()->logf("ERROR", buffer, __FILE__, __LINE__); \
    }

#define log_infof(...)                                                     \
    if (Logger::get_instance()->get_level() <= LOGGER_INFO_LEVEL)          \
    {                                                                      \
        char* buffer = Logger::get_instance()->get_buffer();               \
        snprintbuffer(buffer, LOGGER_BUFFER_SIZE, __VA_ARGS__);            \
        Logger::get_instance()->logf("INFO", buffer, __FILE__, __LINE__);  \
    }

#define log_debugf(...) \
    if (Logger::get_instance()->get_level() <= LOGGER_DEBUG_LEVEL)         \
    {                                                                      \
        char* buffer = Logger::get_instance()->get_buffer();               \
        snprintbuffer(buffer, LOGGER_BUFFER_SIZE, __VA_ARGS__);            \
        Logger::get_instance()->logf("DEBUG", buffer, __FILE__, __LINE__); \
    }

inline void log_info_data(const uint8_t* data, int len, const char* dscr) {
    char print_data[10*1024];
    size_t print_len = 0;
    const int max_print = 512;

    print_len += snprintf(print_data, sizeof(print_data), "%s:", dscr);
    for (int index = 0; index < (len > max_print ? max_print : len); index++) {
        if ((index%16) == 0) {
            print_len += snprintf(print_data + print_len, sizeof(print_data) - print_len, "\r\n");
        }
        
        print_len += snprintf(print_data + print_len, sizeof(print_data) - print_len,
            " %02x", data[index]);
    }
    print_data[print_len] = 0;
    
    log_infof("%s\r\n", print_data);
    return;
}

class MediaServerError : public std::runtime_error
{
public:
    explicit MediaServerError(const char* description) : std::runtime_error(description)
    {
    }
};

#define MS_THROW_ERROR(desc, ...) \
    do \
    { \
        char exp_buffer[1024]; \
        int exp_ret_len = std::snprintf(exp_buffer, sizeof(exp_buffer), desc, ##__VA_ARGS__); \
        exp_buffer[exp_ret_len] = 0; \
        throw MediaServerError(exp_buffer); \
    } while (false)
#endif