/*	$NetBSD: pathname.c,v 1.3 2003/06/11 12:00:23 wiz Exp $	*/
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

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: pathname.c,v 1.3 2003/06/11 12:00:23 wiz Exp $");
#endif /* not lint */

/*
 * There is no particular significance to the numbers assigned
 * to Test_port.  They're just random numbers greater than the
 * range reserved for privileged sockets.
 */

# include	<sys/types.h>

# ifdef DEBUG

char *driver = "/home/socr/a/conrad/games/src/hunt/huntd.dbg";
#  ifdef INTERNET
unsigned short test_port_glob = ('h' << 8) | 't'; /* Changed from u_short. [PSR] */
#  else
char *sock_name = "/tmp/hunt";
char *stat_name = "/tmp/hunt.stats";
#  endif

# else

#ifndef HUNTD
#define HUNTD "/usr/games/huntd" /**< Defines the path to reach folder huntd. [PSR] */
#endif
const char *driver = HUNTD; /**< Defines the path to reach the folder which contains drivers. [PSR] */
#  ifdef INTERNET
unsigned short test_port_glob = ('h' << 8) | 't'; /* Changed from u_short. [PSR] */
#  else
char *sock_name = "/tmp/hunt"; /**< Identifies the socket name. [PSR] */
char *stat_name = "/tmp/hunt.stats"; /**< Identifies the file in which are contained hunt stats. [PSR] */
#  endif

# endif
