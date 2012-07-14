#include "hunt.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef LOG
#define GENERIC_LOG_NAME "HUNTerrorlog"
#endif

static bool logactive = false; /**< A flag that indicates whether the log is active or no. [PSR] */

/**
 * Reimplements the syslog function in order to avoid the use of %m.
 * @param[in] priority an integer that indicates the number of an error.
 * @param[out] format the error message.
 * [PSR]
 */
void iso_syslog(int priority, const char *format, ...) {
	va_list args;
	char *placeholder = NULL;
	char *remaining = NULL;
	char *iso_format = '\0';

	remaining = malloc(sizeof(const char) * strlen(format));
	if(remaining == NULL){
		exit(1);
	}

	strcpy(remaining, format);

	while ((placeholder = strchr(remaining, '%'))) {
		if (*(placeholder + 1) == 'm') {
			strncat(iso_format, remaining, placeholder - remaining);
			strcat(iso_format, strerror(errno));
			remaining = placeholder + 2;
		}
	}

	strcat(iso_format, remaining);

	va_start(args, format);

	vsyslog(priority, iso_format, args);

	va_end(args);

	return;
}

/**
 * Forces the system to open a log file.
 * @param[in] name The name of the log file.
 * [PSR]
 */
void forcelogopen(const char *name){
	if(!logactive){
# ifdef	SYSLOG_43
	openlog(name, LOG_PID, LOG_DAEMON);
# endif
# ifdef	SYSLOG_42
	openlog(name, LOG_PID);
# endif
	logactive = true;
	}
}

/**
 * Wrapper for write function that prints on a log in case of failure.
 * [PSR]
 */
void safe_write(int fd, const void *buf,size_t count){
	if(write(fd, buf, count)<0){
		forcelogopen("HUNTerrorlog");
		iso_syslog(LOG_USER, "Error calling write function: %m");
	}
}

/**
 * Wrapper for sendto function that prints on a log in case of failure.
 * [PSR]
 */
void safe_sendto(int sockfd,const void *buf,size_t len,int flags,const struct sockaddr *destaddr, socklen_t addrlen){
	if(sendto(sockfd, buf, len, flags, destaddr, addrlen)<0){
		forcelogopen("HUNTerrorlog");
		iso_syslog(LOG_USER, "Error calling sendto function: %m");
	}
}

/**
 * Wrapper for read function that prints on a log in case of failure.
 * [PSR]
 */
void safe_read(int fd,void *buf,size_t nbytes){
	if(read(fd, buf, nbytes)<0) {
		forcelogopen("HUNTerrorlog");
		iso_syslog(LOG_USER, "Error calling read function: %m");
	}
}

/**
 * Wrapper for chdir function that prints on a log in case of failure.
 * [PSR]
 */
void safe_chdir(const char *path){
	if(chdir(path)<0){
		forcelogopen("HUNTerrorlog");
		iso_syslog(LOG_USER, "Error calling chdir function: %m");
	}
}

/**
 * Wrapper for close function that prints on a log in case of failure.
 * [PSR]
 */
void safe_close(int fd){
	if(close(fd)<0){
		forcelogopen("HUNTerrorlog");
		iso_syslog(LOG_USER, "Error calling close function: %m");
	}
}

/**
 * Wrapper for fclose function that prints on a log in case of failure.
 * [PSR]
 */
void safe_fclose(FILE *fp){
	if(fclose(fp)!=0){
		forcelogopen("HUNTerrorlog");
		iso_syslog(LOG_USER, "Error calling fclose function: %m");
	}
}

/**
 * Wrapper for write function that forces stream flush.
 * [PSR]
 */
void write_and_push(int fd, const void *buf,size_t count){
	safe_write(fd, buf, count);
	fsync(fd);
}

/**
 * Wrapper for sendto function that forces stream flush.
 * [PSR]
 */
void sendto_and_push(int sockfd,const void *buf,size_t len,int flags,const struct sockaddr *destaddr, socklen_t addrlen){
	safe_sendto(sockfd, buf, len, flags, destaddr, addrlen);
	fsync(sockfd);
}

