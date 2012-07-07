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

/**< #include <sys/cdefs.h> pushed up in hunt.h. [PSR]*/
#ifndef lint
__RCSID("$NetBSD: connect.c,v 1.5 2003/06/11 12:00:21 wiz Exp $");
#endif /* not lint */

# include	<signal.h>
# include	<unistd.h>

/**
 * Manages the preliminary operations of connection of a player with a:
 * @param[in] name Represents the name of a player,
 * @param[in] team Represents the team which the player belongs to,
 * @param[in] enter_status Represents the enter status of a player.
 * [PSR]
 */
void do_connect(const char *name, char team, long enter_status) {

	static int32_t uid; //user id
	static int32_t mode; //game mode

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
