/*
 * utils.c
 *
 *  Created on: 30/giu/2012
 */

#include "hunt.h"
#include <stdarg.h>

void iso_syslog(int priority, const char *format, ...){
	va_list args;
	char *placeholder = NULL;
	char *remaining = NULL;
	char *iso_format = '\0';
//	int i;
//
//	for(i = 0; strchr(format,'%'); i++)
//		;

	strcpy(remaining,format);

	while((placeholder = strchr(remaining,'%'))){
		if(*(placeholder+1) == 'm'){
//			i--;
			strncat(iso_format,remaining, placeholder - remaining);
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
