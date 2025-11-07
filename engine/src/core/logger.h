#pragma once

#include "defines.h"

#define LOG_WARN_ENABLED 1
#define LOG_INFO_ENABLED 1
#define LOG_DEBUG_ENABLED 1
#define LOG_TRACE_ENABLED 1

#if KRELEASE == 1
#define LOG_DEBUG_ENABLED 0
#define LOG_TRACE_ENABLED 0
#endif

typedef enum log_level {
    LOG_LEVEL_FATAL = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_LEVEL_TRACE = 5
} log_level;

/**
 * @brief Initialises logging system. Call first with state = 0 to get memory
 * size. The second time, pass the memory to the state;
 *
 * @param memory_requirement A pointer to hold the required memory size of the
 * internal state struct.
 * @param state 0 if just requesting memory requirments, otherwise, allocated block of memory.
 * @return True on success; otherwise false.
 */
b8 initialize_logging(u64 *memory_requirement, void *state);
void shutdown_logging(void *state);

KAPI void log_output(log_level level, const char *, ...);

#define KFATAL(message, ...)                                                   \
    log_output(LOG_LEVEL_FATAL, message, ##__VA_ARGS__);

#define KERROR(message, ...)                                                   \
    log_output(LOG_LEVEL_ERROR, message, ##__VA_ARGS__);

#if LOG_WARN_ENABLED == 1
#define KWARN(message, ...) log_output(LOG_LEVEL_WARN, message, ##__VA_ARGS__);
#else
#define KWARN(message, ...)
#endif

#if LOG_INFO_ENABLED == 1
#define KINFO(message, ...) log_output(LOG_LEVEL_INFO, message, ##__VA_ARGS__);
#else
#define KINFO(message, ...)
#endif

#if LOG_DEBUG_ENABLED == 1
#define KDEBUG(message, ...)                                                   \
    log_output(LOG_LEVEL_DEBUG, message, ##__VA_ARGS__);
#else
#define KDEBUG(message, ...)
#endif

#if LOG_TRACE_ENABLED == 1
#define KTRACE(message, ...)                                                   \
    log_output(LOG_LEVEL_TRACE, message, ##__VA_ARGS__);
#else
#define KTRACE(message, ...)
#endif
