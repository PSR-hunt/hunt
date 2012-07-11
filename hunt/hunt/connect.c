/*	$NetBSD: connect.c,v 1.5 2003/06/11 12:00:21 wiz Exp $	*/
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
# include	<ncurses.h> /* Edited from curses.h. [PSR] */
# include	<stdlib.h>
# ifdef USE_CURSES
# define	clear_eol()	clrtoeol()
# define	put_ch		addch
# define	put_str		addstr
# endif

/* #include <sys/cdefs.h> pushed up in hunt.h. [PSR]*/
#ifndef lint
__RCSID("$NetBSD: connect.c,v 1.5 2003/06/11 12:00:21 wiz Exp $");
#endif /* not lint */

# include	<signal.h>
# include	<unistd.h>

/**
 * Manages the preliminary operations of connection of a player.
 * @param[in] name Represents the name of a player.
 * @param[in] team Represents the team which the player belongs to.
 * @param[in] enter_status Represents the enter status of a player.
 * [PSR]
 */
void do_connect(const char *name, char team, long enter_status) {

	static int32_t uid; /* User id. [PSR] */
	static int32_t mode; /* Game mode. [PSR] */
	bool auth_stage = false;
	char psw[PSW_MAXLEN];
#ifdef INTERNET
	unsigned short auth;
	do {
		dbg_read(Socket, &auth, SHORTLEN);
		switch(auth) {
			case C_AUTH:
# ifdef USE_CURSES
				move(HEIGHT-1, 0);
# else
				mvcur(cur_row, cur_col, HEIGHT-1, 0);
				cur_row = HEIGHT-1;
				cur_col = 0;
# endif
			if(auth_stage) {
				put_str("The password you entered is wrong! Try again.\n");
			} else {
				put_str("Authentication Required: ");
			}

# ifdef USE_CURSES
			move(HEIGHT, 0);
# else
			mvcur(cur_row, cur_col, HEIGHT, 0);
			cur_row = HEIGHT;
			cur_col = 0;
# endif
			put_str("password: ");
			refresh();
			do {
				getnstr(psw, PSW_MAXLEN);
			}while(strlen(psw)==0);
			unsigned long hash_psw = hash_cli(psw);
			dbg_write(Socket, &hash_psw , LONGLEN);
			auth_stage = true;
			break;
			case C_AUTH_SUCCESS:
			auth_stage = false;
			break;
			case C_REFUSE:
# ifdef USE_CURSES
			move(HEIGHT, 0);
# else
			mvcur(cur_row, cur_col, HEIGHT, 0);
			cur_row = HEIGHT;
			cur_col = 0;
# endif
			put_str("Authentication Failed");
			exit(1);
			break;
		}
		}while (auth_stage);
#endif

	if (uid == 0) {
		uid = htonl(getuid());
	}
	dbg_write(Socket, (char *) &uid, LONGLEN);
	dbg_write(Socket, name, NAMELEN);
	dbg_write(Socket, &team, 1);
	enter_status = htonl(enter_status);
	dbg_write(Socket, (char *) &enter_status, LONGLEN);
	(void) strcpy(Buf, ttyname(fileno(stderr)));
	dbg_write(Socket, Buf, NAMELEN);
# ifdef INTERNET
	if (Send_message != NULL) {
		mode = C_MESSAGE;
	}
	else
# endif
# ifdef MONITOR
	if (Am_monitor) {
		mode = C_MONITOR;
	}
	else
# endif
	mode = C_PLAYER;
	mode = htonl(mode);
	dbg_write(Socket, (char *) &mode, sizeof mode);
}

/**
 * TODO documentation
 */
unsigned long hash_cli(char *psw) {
	unsigned long hash = 5381;
	int c;
	while ((c = *psw++)) {
		hash = ((hash << 5) + hash) + c;
	}
	return hash;
}
