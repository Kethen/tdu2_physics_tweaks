// unix-ish
#include <fcntl.h>
#include <unistd.h>

// pthread
#include <pthread.h>

int _log_fd = -1;
pthread_mutex_t _log_mutex;

void init_logging(){
	pthread_mutex_init(&_log_mutex, NULL);
	const char *log_path = "./tdu2_physics_tweaks_log.txt";
	unlink(log_path);
	_log_fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 00644);
}

int read_data_from_fd(int fd, char *buffer, int len){
	int bytes_read = 0;
	while(bytes_read < len){
		int bytes_read_this_cycle = read(fd, &buffer[bytes_read], len - bytes_read);
		if(bytes_read_this_cycle < 0){
			return bytes_read_this_cycle;
		}
		bytes_read += bytes_read_this_cycle;
	}
	return bytes_read;
}

int write_data_to_fd(int fd, const char *buffer, int len){
	int bytes_written = 0;
	while(bytes_written < len){
		int bytes_written_this_cycle = write(fd, &buffer[bytes_written], len - bytes_written);
		if(bytes_written_this_cycle < 0){
			return bytes_written_this_cycle;
		}
		bytes_written += bytes_written_this_cycle;
	}
	return bytes_written;
}

