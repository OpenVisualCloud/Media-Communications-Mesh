/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <sstream>
#include <string>
#include <cstdarg>
#include <memory>
#include <type_traits>

namespace mesh::log {

enum class Level {
    info,
    warn,
    error,
    debug,
    fatal
};

extern Level currentLogLevel;

class Formatter {
public:
    virtual void formatMessage(std::ostringstream& ostream,
                               Level level,
                               const char *format,
                               va_list args) = 0;

    virtual void formatKeyValueBefore(std::ostringstream& ostream,
                                      const char *key) = 0;

    virtual void formatKeyValueAfter(std::ostringstream& ostream,
                                     const char *key) {}

    virtual void formatBefore(std::ostringstream& ostream) {}
    virtual void formatAfter(std::ostringstream& ostream) {}
};

class StandardFormatter : public Formatter {
public:
    void formatMessage(std::ostringstream& ostream, Level level,
                       const char *format, va_list args) override;

    void formatKeyValueBefore(std::ostringstream& ostream,
                              const char *key) override;

    void formatAfter(std::ostringstream& ostream) override;
};

class JsonFormatter : public Formatter {
public:
    void formatMessage(std::ostringstream& ostream, Level level,
                       const char *format, va_list args) override;

    void formatKeyValueBefore(std::ostringstream& ostream,
                              const char *key) override;

    void formatBefore(std::ostringstream& ostream) override;
    void formatAfter(std::ostringstream& ostream) override;
};

extern std::unique_ptr<Formatter> formatter;

class Logger {
public:
    Logger(Level level, const char *format, va_list args);
    Logger(Logger&& other) noexcept : level(other.level), ostream(std::move(other.ostream)) {}
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    ~Logger();

    template<typename T>
    Logger& operator()(const char *key, const T& value) {
        if (formatter && level >= currentLogLevel)
            formatter->formatKeyValueBefore(ostream, key);

        using DecayedT = std::decay_t<T>;
        if constexpr (std::is_same_v<DecayedT, const char *> ||
            std::is_same_v<DecayedT, char *> ||
            std::is_same_v<DecayedT, std::string>)
            ostream << '"' << value << '"';
        else if (std::is_same_v<DecayedT, bool>)
            ostream << (value ? "true" : "false");
        else
            ostream << value;

        if (formatter && level >= currentLogLevel)
            formatter->formatKeyValueAfter(ostream, key);
        return *this;
    }

private:
    Level level;
    std::ostringstream ostream;
};

/**
 * Structured log functions.
 * Log format options:
 *  - Standard log format for development and single node environments.
 *  - Json log format for k8s and cloud deployments.
 *  
 * Example A: Standard log format (default)
 * ========================================
 * mesh::log::setFormatter(std::make_unique<mesh::log::StandardFormatter>());
 * 
 * log::info("Connection created")("id", "123456")("num", 789);
 * log::warn("Low temperature")("temp", -35.7)("healthy", true);
 * log::error("High load")("percent", 99.8)("num_clients", 9801);
 * log::debug("Counter incremented")("cnt", 355);
 * log::fatal("Emergency exit")("err_code", 312645);
 * 
 * Output:
 * Nov 15 00:26:07.672 [INFO] Connection created id="123456" num=789
 * Nov 15 00:26:07.672 [WARN] Low temperature temp=-35.7 healthy=true
 * Nov 15 00:26:07.672 [ERRO] High load percent=99.8 num_clients=9801
 * Nov 15 00:26:07.672 [DEBU] Counter incremented cnt=355
 * Nov 15 00:26:07.672 [FATA] Emergency exit err_code=312645
 * 
 * Example B: Same messages in JSON log format
 * ===========================================
 * mesh::log::setFormatter(std::make_unique<mesh::log::JsonFormatter>());
 * 
 * Output:
 * {"time":"2024-11-15T00:27:30.300Z","level":"info","msg":"Connection created","id":"123456","num":789}
 * {"time":"2024-11-15T00:27:30.300Z","level":"warn","msg":"Low temperature","temp":-35.7,"healthy":true}
 * {"time":"2024-11-15T00:27:30.300Z","level":"error","msg":"High load","percent":99.8,"num_clients":9801}
 * {"time":"2024-11-15T00:27:30.300Z","level":"debug","msg":"Counter incremented","cnt":355}
 * {"time":"2024-11-15T00:27:30.300Z","level":"fatal","msg":"Emergency exit","err_code":312645}
 * Example C: Setting the log level dynamically
 * ============================================
 * mesh::log::setLogLevel(mesh::log::Level::warn); // Set minimum log level to WARN
 * 
 * log::info("This message will not be displayed")("id", "123456");
 * log::warn("Low memory warning")("available_mb", 512);
 * log::error("Critical error occurred")("error_code", 5001);
 * log::debug("Debugging details")("step", "init");
 * 
 * Output:
 * Nov 15 00:26:07.672 [WARN] Low memory warning available_mb=512
 * Nov 15 00:26:07.672 [ERRO] Critical error occurred error_code=5001
 * Nov 15 00:26:07.672 [DEBU] Debugging details step=init
 * Example D: Adjusting log levels during runtime
 * ==============================================
 * mesh::log::setLogLevel(mesh::log::Level::info); // Enable all log messages
 * log::info("Re-enabled info logging")("reason", "debugging mode");
 * 
 * Output:
 * Nov 15 00:26:07.672 [INFO] Re-enabled info logging reason="debugging mode"
 * 
 * Features:
 *  - Use `mesh::log::setLogLevel(mesh::log::Level)` to dynamically adjust log filtering.
 *  - Supported log levels: `info`, `warn`, `error`, `debug`, `fatal`.
 *  - Messages below the set log level will be ignored.
 */
Logger info(const char* format, ...);
Logger warn(const char* format, ...);
Logger error(const char* format, ...);
Logger debug(const char* format, ...);
Logger fatal(const char* format, ...);

void setFormatter(std::unique_ptr<Formatter> new_formatter);
void setLogLevel(Level level);

} // namespace mesh::log

#endif // LOGGER_H
