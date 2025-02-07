#include <cstdarg> // va_list, va_start, va_end
#include <cstdio> // stdout and stderr macros are not accessible with std module

#ifndef VB_USE_STD_MODULE
#include <cstdlib> // std::abort()
#else
import std;
#endif

#include "vulkan_backend/log.hpp"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

namespace VB_NAMESPACE {
int static constexpr kMessageBufferSize = 1024;

void MessageCallbackDefault(LogLevel level, char const* message) {
	switch (level) {
	[[likely]]
	case LogLevel::Trace:
		std::fprintf(stdout, "%s\n", message);
		break;
	case LogLevel::Info:
		std::fprintf(stdout, "[" ANSI_COLOR_GREEN "I" ANSI_COLOR_RESET "] " "%s\n", message);
		break;
	case LogLevel::Warning:
		std::fprintf(stdout, "[" ANSI_COLOR_YELLOW "W" ANSI_COLOR_RESET "] " "%s\n", message);
		break;
	case LogLevel::Error:
		std::fprintf(stderr, "[" ANSI_COLOR_RED "E" ANSI_COLOR_RESET "] " "%s\n", message);
		break;
	
	}
}

Logger& Logger::Get() {
	static Logger logger;
	return logger;
}

void Logger::SetMessageCallback(MessageCallback callback) {
	messageCallback = callback;
}

void Logger::SetLogLevel(LogLevel level) {
	logLevel = level;
}

void SetLogLevel(LogLevel level) {
	Logger::Get().SetLogLevel(level);
}

void SetMessageCallback(MessageCallback callback) {
	Logger::Get().SetMessageCallback(callback);
}

void Logger::LogFormat(LogLevel level, char const* format, ...) {
	if (level >= logLevel) {
		char buffer[kMessageBufferSize];
		va_list args;
		va_start(args, format);
		int size = vsnprintf(buffer, sizeof(buffer), format, args);
		va_end(args);
		if (size < 0) [[unlikely]] {
			return;
		}
		buffer[sizeof(buffer) - 1] = '\0';
		messageCallback(level, buffer);
	}
}

void Logger::Log(LogLevel level, char const* message) {
	if (level >= logLevel) {
		messageCallback(level, message);	
	}
}


} // namespace VB_NAMESPACE