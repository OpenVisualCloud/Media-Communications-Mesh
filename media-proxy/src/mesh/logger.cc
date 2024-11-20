/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>

namespace mesh::log {

void StandardFormatter::formatMessage(std::ostringstream& ostream, Level level,
                                      const char *format, va_list args)
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    // ostream << "\x1b[34m";
    ostream << std::put_time(std::gmtime(&in_time_t), "\x1b[38;5;245m" "%b %d %H:%M:%S");
    ostream << '.' << std::setfill('0') << std::setw(3) << ms.count() << ' ';

    switch (level) {
        case Level::info: 
            ostream << "\x1b[38;5;14m" "[INFO] \x1b[0m";
            break;
        case Level::warn:
            ostream << "\x1b[38;5;214m" "[WARN] ";
            break;
        case Level::error:
            ostream << "\x1b[38;5;9m" "[ERRO] ";
            break;
        case Level::debug:
            ostream << "\x1b[38;5;227m" "[DEBU] \x1b[0m";
            break;
        case Level::fatal:
            ostream << "\x1b[31m" "[FATA] ";
            break;
    }

    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    ostream << buffer;
}

void StandardFormatter::formatKeyValueBefore(std::ostringstream& ostream,
                                             const char *key)
{
    ostream << "\x1b[38;5;245m " << key << "=\x1b[0m";
}

void StandardFormatter::formatAfter(std::ostringstream& ostream)
{
    ostream << "\x1b[0m";
}

void JsonFormatter::formatMessage(std::ostringstream& ostream, Level level,
                                  const char *format, va_list args)
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    ostream << "\"time\":\"";
    ostream << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%S");
    ostream << '.' << std::setfill('0') << std::setw(3) << ms.count() << "Z\",";

    switch (level) {
        case Level::info: 
            ostream << "\"level\":\"info\",";
            break;
        case Level::warn:
            ostream << "\"level\":\"warn\",";
            break;
        case Level::error:
            ostream << "\"level\":\"error\",";
            break;
        case Level::debug:
            ostream << "\"level\":\"debug\",";
            break;
        case Level::fatal:
            ostream << "\"level\":\"fatal\",";
            break;
    }

    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    ostream << "\"msg\":\"" << buffer << "\"";
}

void JsonFormatter::formatKeyValueBefore(std::ostringstream& ostream,
                                         const char *key)
{
    ostream << ",\"" << key << "\":";
}

void JsonFormatter::formatBefore(std::ostringstream& ostream)
{
    ostream << "{";
}

void JsonFormatter::formatAfter(std::ostringstream& ostream)
{
    ostream << "}";
}

std::unique_ptr<Formatter> formatter = std::make_unique<StandardFormatter>();

void setFormatter(std::unique_ptr<Formatter> new_formatter)
{
    formatter = std::move(new_formatter);
}

Logger::Logger(Level level, const char *format, va_list args)
{
    if (formatter) {
        formatter->formatBefore(ostream);
        formatter->formatMessage(ostream, level, format, args);
    }
}

Logger::~Logger()
{
    if (formatter) 
        formatter->formatAfter(ostream);

    if (!ostream.str().empty())
        std::cout << ostream.str() << std::endl;
}

Logger info(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    Logger logger(Level::info, format, args);
    va_end(args);
    return logger;
}

Logger warn(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    Logger logger(Level::warn, format, args);
    va_end(args);
    return logger;
}

Logger error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    Logger logger(Level::error, format, args);
    va_end(args);
    return logger;
}

Logger debug(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    Logger logger(Level::debug, format, args);
    va_end(args);
    return logger;
}

Logger fatal(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    Logger logger(Level::fatal, format, args);
    va_end(args);
    return logger;
}

} // namespace mesh::log
