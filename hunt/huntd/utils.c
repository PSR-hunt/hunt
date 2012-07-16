#define _XOPEN_SOURCE 700

#include "hunt.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef LOG
#define GENERIC_LOG_NAME "HUNTerrorlog"
#endif

char *dyn_strcat(const char *, const char *, size_t);

static bool logactive = false; /**< A flag that indicates whether the log is active or no. [PSR] */

/**
 * Appends n characters from string s2 to string s1.
 * Dynamically allocate the correct amount of memory to generate a new string made by the entire s1
 * string followed by maximum the first n characters of s2 overriding s1 string terminator and adding
 * a string terminator at the end of the resulting string.
 * @param[in] s1 The first string.
 * @param[in] s2 The second string.
 * @param[in] n Maximum number of character copied from s2. If set to 0 or negative, the entire s2 will
 * be copied.
 * \return The concatenated string.
 */
char *dyn_strcat(const char *s1, const char *s2, size_t n) {

	char *result;

	if (n > strlen(s2) || n <= 0) {
		n = strlen(s2);
	}

	result = (char *) malloc((strlen(s1) + n + 1) * sizeof(char));
	if (result == NULL) {
		exit(EXIT_FAILURE);
	}

	strcpy(result, s1);
	strncat(result, s2, n);

	return result;
}

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
	char *saved_remaining = NULL; //Salva il valore iniziale di remaining, per liberarlo eventualmente
	char *iso_format = NULL;
	char *tmp = NULL;

	remaining = saved_remaining = strdup(format);

	while ((placeholder = strchr(remaining, '%'))) {
		if (iso_format != NULL) {
			tmp = dyn_strcat(iso_format, remaining, placeholder - remaining);
			free(iso_format);
			iso_format = strdup(tmp);
			free(tmp);
		} else {
			iso_format = strndup(remaining, (placeholder - remaining));
		}

		if (*(placeholder + 1) == 'm') {
			tmp = dyn_strcat(iso_format, strerror(errno), -1);
		} else {
			tmp = dyn_strcat(iso_format, placeholder, 2);
		}

		free(iso_format);
		iso_format = strdup(tmp);
		free(tmp);
		remaining = placeholder + 2;
	}

	tmp = dyn_strcat(iso_format, remaining, -1);
	free(iso_format);
	iso_format = strdup(tmp);
	free(tmp);

	va_start(args, format);

	vsyslog(priority, iso_format, args);

	va_end(args);

	free(saved_remaining);
	free(iso_format);
	remaining = saved_remaining = NULL;
	iso_format = NULL;

	return;
}

/**
 * Forces the system to open a log file.
 * @param[in] name The name of the log file.
 * [PSR]
 */
void forcelogopen(const char *name) {
	if (!logactive) {
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
void safe_write(int fd, const void *buf, size_t count) {
	if (write(fd, buf, count) < 0) {
		forcelogopen("HUNTerrorlog");
		iso_syslog(LOG_USER, "Error calling write function: %m");
	}
}

/**
 * Wrapper for sendto function that prints on a log in case of failure.
 * [PSR]
 */
void safe_sendto(int sockfd, const void *buf, size_t len, int flags,
		const struct sockaddr *destaddr, socklen_t addrlen) {
	if (sendto(sockfd, buf, len, flags, destaddr, addrlen) < 0) {
		forcelogopen("HUNTerrorlog");
		iso_syslog(LOG_USER, "Error calling sendto function: %m");
	}
}

/**
 * Wrapper for read function that prints on a log in case of failure.
 * [PSR]
 */
void safe_read(int fd, void *buf, size_t nbytes) {
	if (read(fd, buf, nbytes) < 0) {
		forcelogopen("HUNTerrorlog");
		iso_syslog(LOG_USER, "Error calling read function: %m");
	}
}

/**
 * Wrapper for chdir function that prints on a log in case of failure.
 * [PSR]
 */
void safe_chdir(const char *path) {
	if (chdir(path) < 0) {
		forcelogopen("HUNTerrorlog");
		iso_syslog(LOG_USER, "Error calling chdir function: %m");
	}
}

/**
 * Wrapper for close function that prints on a log in case of failure.
 * [PSR]
 */
void safe_close(int fd) {
	if (close(fd) < 0) {
		forcelogopen("HUNTerrorlog");
		iso_syslog(LOG_USER, "Error calling close function: %m");
	}
}

/**
 * Wrapper for fclose function that prints on a log in case of failure.
 * [PSR]
 */
void safe_fclose(FILE *fp) {
	if (fclose(fp) != 0) {
		forcelogopen("HUNTerrorlog");
		iso_syslog(LOG_USER, "Error calling fclose function: %m");
	}
}

/**
 * Wrapper for write function that forces stream flush.
 * [PSR]
 */
void write_and_push(int fd, const void *buf, size_t count) {
	safe_write(fd, buf, count);
	fsync(fd);
}

/**
 * Wrapper for sendto function that forces stream flush.
 * [PSR]
 */
void sendto_and_push(int sockfd, const void *buf, size_t len, int flags,
		const struct sockaddr *destaddr, socklen_t addrlen) {
	safe_sendto(sockfd, buf, len, flags, destaddr, addrlen);
	fsync(sockfd);
}

