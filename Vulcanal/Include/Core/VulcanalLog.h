#pragma once

// MW @gotcha: This file should only be included in the PCH, or else the Linux build will fail due to redefinition
//             fmt overloads at the end of the file.

// Disable warnings caused by fmt overloads.
// ReSharper disable CppMemberFunctionMayBeStatic

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/fmt/ostr.h" // Needed for logging some types

extern std::shared_ptr<spdlog::logger> g_VulcanalLogger;

void InitLog(const char *prefPath);
void ShutdownLog();
void AddSinkToLog(const spdlog::sink_ptr &sink);

#ifndef VULC_NO_LOG
	#define VULC_TRACE(format, ...)                   g_VulcanalLogger->trace(format "\n" __VA_OPT__(,) __VA_ARGS__)
	#define VULC_INFO(format, ...)                    g_VulcanalLogger->info(format "\n" __VA_OPT__(,) __VA_ARGS__)
	#define VULC_INFO(format, ...)                    g_VulcanalLogger->info(format "\n" __VA_OPT__(,) __VA_ARGS__)
	#define VULC_WARN(format, ...)                    g_VulcanalLogger->warn(format "\n" __VA_OPT__(,) __VA_ARGS__)
	#define VULC_ERROR(format, ...)                   g_VulcanalLogger->error(format "\n" __VA_OPT__(,) __VA_ARGS__)
	#define VULC_CRITICAL(format, ...)                g_VulcanalLogger->critical(format "\n" __VA_OPT__(,) __VA_ARGS__)

	#define VULC_TRACE_NO_NEWLINE(format, ...)        g_VulcanalLogger->trace(format __VA_OPT__(,) __VA_ARGS__)
	#define VULC_INFO_NO_NEWLINE(format, ...)         g_VulcanalLogger->info(format __VA_OPT__(,) __VA_ARGS__)
	#define VULC_WARN_NO_NEWLINE(format, ...)         g_VulcanalLogger->warn(format __VA_OPT__(,) __VA_ARGS__)
	#define VULC_ERROR_NO_NEWLINE(format, ...)        g_VulcanalLogger->error(format __VA_OPT__(,) __VA_ARGS__)
	#define VULC_CRITICAL_NO_NEWLINE(format, ...)     g_VulcanalLogger->critical(format __VA_OPT__(,) __VA_ARGS__)
#else
	#define VULC_TRACE(...)      
	#define VULC_INFO(...)       
	#define VULC_WARN(...)       
	#define VULC_ERROR(...)      
	#define VULC_CRITICAL(...)

	#define VULC_TRACE_NO_NEWLINE(format, ...)     
	#define VULC_INFO_NO_NEWLINE(format, ...)      
	#define VULC_WARN_NO_NEWLINE(format, ...)      
	#define VULC_ERROR_NO_NEWLINE(format, ...)     
	#define VULC_CRITICAL_NO_NEWLINE(format, ...)  
#endif
