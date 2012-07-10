/*	$NetBSD: terminal.c,v 1.4 2003/06/11 12:00:23 wiz Exp $	*/
/*
 * Copyright (c) 1983-2003, Regents of the University of California.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are 
 * met:
 * 
 * + Redistributions of source code must retain the above copyright 
 *   notice, this list of conditions and the following disclaimer.
 * + Redistributions in binary form must reproduce the above copyright 
 *   notice, this list of conditions and the following disclaimer in the 
 *   documentation and/or other materials provided with the distribution.
 * + Neither the name of the University of California, San Francisco nor 
 *   the names of its contributors may be used to endorse or promote 
 *   products derived from this software without specific prior written 
 *   permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS 
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

# include	"hunt.h"

/* #include <sys/cdefs.h> pushed up in hunt.h. [PSR] */
#ifndef lint
__RCSID("$NetBSD: terminal.c,v 1.4 2003/06/11 12:00:23 wiz Exp $");
#endif /* not lint */

#include <stdarg.h>
#define	TERM_WIDTH	80 /**< Assume terminals are 80-char wide */

/**
 * Move the cursor to the given position on the given player's terminal.
 * @param[in,out] pp A player on which terminal will be moved the position.
 * @param[in] y A coordinate.
 * @param[in] x A coordinate.
 */
void cgoto(PLAYER *pp, int y, int x) {
	if (x == pp->p_curx && y == pp->p_cury){
		return;
	}
	sendcom(pp, MOVE, y, x);
	pp->p_cury = y;
	pp->p_curx = x;
}

/**
 *  Put out a single character.
 *  Uniformed to prototype in hunt.h. [PSR]
 *  @param[in,out] pp A player on which terminal will be displayed the character.
 *  @param[in] ch The character to put out.
 */
void outch(PLAYER *pp, int ch) {
	if (++pp->p_curx >= TERM_WIDTH) {
		pp->p_curx = 0;
		pp->p_cury++;
	}
	(void) putc((char) ch, pp->p_output);
}

/**
 * Put out a string of the given length.
 * @param[in,out] pp A player on which terminal will be displayed the string.
 * @param[in] str The string to put out.
 * @param[in] len The string length.
 */
void outstr(PLAYER *pp, const char *str, int len) {
	pp->p_curx += len;
	pp->p_cury += (pp->p_curx / TERM_WIDTH);
	pp->p_curx %= TERM_WIDTH;
	while (len--){
		(void) putc(*str++, pp->p_output);
	}
}

/**
 * Clear the screen, and reset the current position on the screen.
 * @param[in,out] pp The player which screen will be cleared.
 */
void clrscr(PLAYER *pp) {
	sendcom(pp, CLEAR);
	pp->p_cury = 0;
	pp->p_curx = 0;
}

/**
 * Clear to the end of the line.
 * @param[in] pp The player which screen will be cleared to the end of the line.
 */
void ce(PLAYER *pp) {
	sendcom(pp, CLRTOEOL);
}

#if 0		/* XXX lukem*/
/**
 * Refresh the screen
 * @param[in] pp The player which screen will be refreshed.
 */
void  ref(PLAYER * pp){
	sendcom(pp, REFRESH);
}
#endif

/**
 * Send a command to the given user.
 * @param[in] pp A player on which terminal the command will be sent.
 * @param[in] command A command.
 */
void sendcom(PLAYER *pp, int command, ...) {
	va_list ap;
	int arg1, arg2;

	va_start(ap, command);
	(void) putc(command, pp->p_output);
	switch (command & 0377) {
	case MOVE:
		arg1 = va_arg(ap, int);
		arg2 = va_arg(ap, int);
		(void) putc(arg1, pp->p_output);
		(void) putc(arg2, pp->p_output);
		break;
	case ADDCH:
	case READY:
		arg1 = va_arg(ap, int);
		(void) putc(arg1, pp->p_output);
		break;
	}

	va_end(ap);
	/* No return needed for void functions. */
}
