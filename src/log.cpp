#include <cstdarg> // va_list, va_start, va_end
#include <cstdio> // stdout and stderr macros are not accessible with std module

#ifndef VB_USE_STD_MODULE
#include <cstdlib> // std::abort()
#else
import std;
#endif

#include "vulkan_backend/log.hpp"

namespace VB_NAMESPACE {
int static constexpr kMessageBufferSize = 1024;

void MessageCallbackDefault(LogLevel level, char const* message) {
	// if (level < LogLevel::Info) [[likely]] {
	// 	return;
	// }
	if (level < LogLevel::Error) [[likely]] {
		std::fprintf(stdout, "%s\n", message);
	} else {
		std::fprintf(stderr, "%s\n", message);
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

void Logger::Log(LogLevel level, char const* format, ...) {
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

} // namespace VB_NAMESPACE