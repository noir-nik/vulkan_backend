#pragma once

#ifndef VB_USE_STD_MODULE
#include <iostream>
#include <string_view>
#elif defined(VB_DEV)
import std;
#endif

#include "vulkan_backend/classes/no_copy_no_move.hpp"

#ifndef VB_NO_LOG

#ifndef VB_LOG_TRACE
#define VB_LOG_TRACE(fmt, ...) \
	Logger::Get().Log(LogLevel::Trace, fmt, ## __VA_ARGS__)
#endif

#ifndef VB_LOG_INFO
#define VB_LOG_INFO(fmt, ...) \
	Logger::Get().Log(LogLevel::Info, fmt, ## __VA_ARGS__)
#endif

#ifndef VB_LOG_WARN
#define VB_LOG_WARN(fmt, ...) \
	Logger::Get().Log(LogLevel::Warning, fmt, ## __VA_ARGS__)
#endif

#ifndef VB_LOG_ERROR
#define VB_LOG_ERROR(fmt, ...) \
	Logger::Get().Log(LogLevel::Error, fmt, ## __VA_ARGS__)
#endif

#endif // !VB_NO_LOG

VB_EXPORT
namespace VB_NAMESPACE {
enum class LogLevel {
	Trace,
	Info,
	Warning,
	Error
};

using MessageCallback = void (&)(LogLevel, char const*);
void MessageCallbackDefault(LogLevel level, char const* message);

class Logger : NoCopyNoMove {
public:
	// Get the singleton instance of the Logger
	static Logger& Get();

	// Set function to be called when on message
	void SetMessageCallback(MessageCallback callback);

	// Call function if message level >= current log level
	void SetLogLevel(LogLevel level);

	// Logging function
	void Log(LogLevel level, char const* format, ...);
private:
	Logger() = default;
	void (*messageCallback)(LogLevel, char const*) = MessageCallbackDefault;
	LogLevel logLevel = LogLevel::Info;
};

void SetLogLevel(LogLevel level);

void SetMessageCallback(MessageCallback callback);

} // namespace VB_NAMESPACE
