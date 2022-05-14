#include "logger.hpp"
#include "timeex.hpp"
#include <stdio.h>
#include <string>
#include <sstream>
#include <iostream>

Logger Logger::s_logger;

Logger::Logger(const std::string filename, enum LOGGER_LEVEL level):filename_(filename)
    , level_(level)
{

}

Logger::~Logger()
{

}

Logger* Logger::get_instance() {
    return &s_logger;
}

void Logger::set_filename(const std::string& filename) {
    filename_ = filename;
}

void Logger::set_level(enum LOGGER_LEVEL level) {
    level_ = level;
}

enum LOGGER_LEVEL Logger::get_level() {
    return level_;
}

char* Logger::get_buffer() {
    return buffer_;
}

void Logger::enable_console() {
    console_enable_ = true;
}

void Logger::disable_console() {
    console_enable_ = false;
}

void Logger::logf(const char* level, const char* buffer, const char* filename, int line) {
    std::stringstream ss;
    std::string name(filename);

    size_t pos = name.rfind("/");
    if (pos != name.npos) {
        name = name.substr(pos + 1);
    }

    ss << "[" << level << "]" << "[" << get_now_str() << "]"
       << "[" << name << ":" << line << "]"
       << buffer << "\r\n";
    
    if (filename_.empty()) {
        std::cout << ss.str();
    } else {
        FILE* file_p = fopen(filename_.c_str(), "ab+");
        if (file_p) {
            fwrite(ss.str().c_str(), ss.str().length(), 1, file_p);
            fclose(file_p);
        }
        if (console_enable_) {
            std::cout << ss.str();
        }
    }
    
}
