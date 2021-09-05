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

void Logger::set_filename(const std::string filename) {
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

void Logger::logf(const char* level, const char* buffer, const char* filename, int line) {
    std::stringstream ss;

    ss << "[" << level << "]" << "[" << get_now_str() << "]"
       << "[" << filename << ":" << line << "]"
       << buffer << "\r\n";
    
    if (filename_.empty()) {
        std::cout << ss.str();
    } else {
        FILE* file_p = fopen(filename_.c_str(), "ab+");
        if (file_p) {
            fwrite(ss.str().c_str(), ss.str().length(), 1, file_p);
            fclose(file_p);
        }
    }
    
}