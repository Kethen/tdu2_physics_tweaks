#ifndef __LOGGING_H
#define __LOGGING_H

#include <stdio.h>

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int _log_fd;
extern pthread_mutex_t _log_mutex;

void init_logging();
int read_data_from_fd(int fd, char *buffer, int len);
int write_data_to_fd(int fd, const char *buffer, int len);

#ifdef __cplusplus
};
#endif

#define LOG(...){ \
	pthread_mutex_lock(&_log_mutex); \
	if(_log_fd >= 0){ \
		char _log_buffer[1024]; \
		int _log_len = sprintf(_log_buffer, __VA_ARGS__); \
		write_data_to_fd(_log_fd, _log_buffer, _log_len); \
	} \
	pthread_mutex_unlock(&_log_mutex); \
}

#if 0
#define LOG_VERBOSE(...) LOG(__VA_ARGS__)
#else
#define LOG_VERBOSE(...)
#endif

#endif
