/*	$NetBSD: extern.c,v 1.4 2003/06/11 12:00:22 wiz Exp $	*/
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
__RCSID("$NetBSD: extern.c,v 1.4 2003/06/11 12:00:22 wiz Exp $");
#endif /* not lint */

# ifdef MONITOR
bool am_monitor = false; /* current process is a monitor */
# endif

char gen_buf[BUFSIZ]; /**< General scribbling buffer */
char maze[HEIGHT][WIDTH2]; /**< The maze */
char Orig_maze[HEIGHT][WIDTH2]; /**< The original maze */

struct pollfd fdset[3 + MAXPL + MAXMON]; /**< A set of file descriptors. [PSR] */
int Nplayer = 0; /**< Number of players */
int main_socket; /**< Main socket */
int Status; /**< Stat socket */
int See_over[NASCII]; /**< Lookup table for determining whether character represents "transparent" item */

BULLET *Bullets = NULL; /**< Linked list of bullets */

EXPL *Expl[EXPLEN]; /**< Explosion lists */
EXPL *Last_expl; /**< Last explosion on Expl[0] */

PLAYER player[MAXPL]; /**< All the players */
PLAYER *end_player = player; /**< Last active player slot */
# ifdef BOOTS
PLAYER Boot[NBOOTS]; /* all the boots */
# endif
IDENT *scores; /**< Score cache */
# ifdef MONITOR
PLAYER monitor[MAXMON]; /* all the monitors */
PLAYER *end_monitor = monitor; /* last active monitor slot */
# endif

# ifdef VOLCANO
int volcano = 0; /* Explosion size */
# endif

int shot_req[MAXBOMB] = { BULREQ, GRENREQ, SATREQ, BOMB7REQ, BOMB9REQ,
		BOMB11REQ, BOMB13REQ, BOMB15REQ, BOMB17REQ, BOMB19REQ, BOMB21REQ, }; /**< The possible shot requests. [PSR] */
int shot_type[MAXBOMB] = { SHOT, GRENADE, SATCHEL, BOMB, BOMB, BOMB, BOMB, BOMB,
		BOMB, BOMB, BOMB, }; /**< The possible shot types. [PSR] */
#ifdef OOZE
int slime_req[MAXSLIME] = { SLIMEREQ, SSLIMEREQ, SLIME2REQ, SLIME3REQ, };
#endif
