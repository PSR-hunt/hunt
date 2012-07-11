#include "hunt.h"
#include <stdlib.h>
#include <stdarg.h>

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

	remaining = (char *) malloc(sizeof(char) * strlen(format));
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
