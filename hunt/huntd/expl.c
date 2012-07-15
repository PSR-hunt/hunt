/*	$NetBSD: expl.c,v 1.4 2004/01/27 20:30:29 jsm Exp $	*/
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
__RCSID("$NetBSD: expl.c,v 1.4 2004/01/27 20:30:29 jsm Exp $");
#endif /* not lint */

# include	<stdlib.h>

static	void	remove_wall(int, int);

/**
 * Show the explosions as they currently are.
 */
void showexpl(int y,int x,char type){
	PLAYER	*pp;
	EXPL	*ep;

	if (y < 0 || y >= HEIGHT){
		return;
	}
	if (x < 0 || x >= WIDTH){
		return;
	}
	ep = (EXPL *) malloc(sizeof (EXPL));	/* NOSTRICT */
	if (ep == NULL) {
# ifdef LOG
		iso_syslog(LOG_ERR, "Out of memory");
# else
		warnx("Out of memory");
# endif
		cleanup(1);
	}
	ep->e_y = y;
	ep->e_x = x;
	ep->e_char = type;
	ep->e_next = NULL;
	if (last_expl == NULL){
		expl_glob[0] = ep;
	}
	else{
		last_expl->e_next = ep;
	}
	last_expl = ep;
	for (pp = player; pp < end_player; pp++) {
		if (pp->p_maze[y][x] == type){
			continue;
		}
		pp->p_maze[y][x] = type;
		cgoto(pp, y, x);
		outch(pp, type);
	}
# ifdef MONITOR
	for (pp = monitor; pp < end_monitor; pp++) {
		if (pp->p_maze[y][x] == type){
			continue;
		}
		pp->p_maze[y][x] = type;
		cgoto(pp, y, x);
		outch(pp, type);
	}
# endif
	switch (maze[y][x]) {
	  case WALL1:
	  case WALL2:
	  case WALL3:
# ifdef RANDOM
	  case DOOR:
# endif
# ifdef REFLECT
	  case WALL4:
	  case WALL5:
# endif
		if (y >= UBOUND && y < DBOUND && x >= LBOUND && x < RBOUND){
			remove_wall(y, x);
		}
		break;
	}
}

/**
 * Roll the explosions over, so the next one in the list is at the top.
 */
void rollexpl(){
	EXPL	*ep;
	PLAYER	*pp;
	int	y, x;
	char	c;
	EXPL	*nextep;

	for (ep = expl_glob[EXPLEN - 1]; ep != NULL; ep = nextep) {
		nextep = ep->e_next;
		y = ep->e_y;
		x = ep->e_x;
		if (y < UBOUND || y >= DBOUND || x < LBOUND || x >= RBOUND){
			c = maze[y][x];
		}
		else{
			c = SPACE;
		}
		for (pp = player; pp < end_player; pp++){
			if (pp->p_maze[y][x] == ep->e_char) {
				pp->p_maze[y][x] = c;
				cgoto(pp, y, x);
				outch(pp, c);
			}
		}
# ifdef MONITOR
		for (pp = monitor; pp < end_monitor; pp++){
			check(pp, y, x);
		}
# endif
		free((char *) ep);
	}
	for (x = EXPLEN - 1; x > 0; x--){
		expl_glob[x] = expl_glob[x - 1];
	}
	last_expl = expl_glob[0] = NULL;
}

/* There's about 700 walls in the initial maze.  So we pick a number
 * that keeps the maze relatively full. */
# define MAXREMOVE	40 /**< Maximum number of removed walls. [PSR] */

static	REGEN	removed[MAXREMOVE]; /**< Removed walls container. [PSR] */
static	REGEN	*rem_index = removed; /**< Pointer to the firts element of removed. [PSR] */

/**
 *  remove_wall - add a location where the wall was blown away.
 *	If there is no space left over, put the a wall at the location currently pointed at.
 *	@param[in] y A coordinate
 *	@param[in] x A coordinate
 */
static void remove_wall(int y,int x){
	REGEN	*r;

	bool found;
	found = 0;

# if defined(MONITOR) || defined(FLY)
	PLAYER	*pp;
# endif
# ifdef	FLY
	char	save_char = 0;
# endif

	r = rem_index;
	while (r->r_y != 0) {
# ifdef FLY
		switch (maze[r->r_y][r->r_x]) {
		  case SPACE:
		  case LEFTS:
		  case RIGHT:
		  case ABOVE:
		  case BELOW:
		  case FLYER:
			save_char = maze[r->r_y][r->r_x];
			found=1;
		}
# else
		if (maze[r->r_y][r->r_x] == SPACE ){
			break;
		}
# endif
		if ((++r >= &removed[MAXREMOVE])&&!found){
			r = removed;
		}
	}

	if (r->r_y != 0) {
		/* Slot being used, put back this wall */
# ifdef FLY
		if (save_char == SPACE){
			maze[r->r_y][r->r_x] = orig_maze[r->r_y][r->r_x];
		}
		else {
			pp = play_at(r->r_y, r->r_x);
			if (pp->p_flying >= 0){
				pp->p_flying += rand_num(10);
			}
			else {
				pp->p_flying = rand_num(20);
				pp->p_flyx = 2 * rand_num(6) - 5;
				pp->p_flyy = 2 * rand_num(6) - 5;
			}
			pp->p_over = orig_maze[r->r_y][r->r_x];
			pp->p_face = FLYER;
			maze[r->r_y][r->r_x] = FLYER;
			showexpl(r->r_y, r->r_x, FLYER);
		}
# else
		maze[r->r_y][r->r_x] = orig_maze[r->r_y][r->r_x];
# endif
# ifdef RANDOM
		if (rand_num(100) == 0){
			maze[r->r_y][r->r_x] = DOOR;
		}
# endif
# ifdef REFLECT
		if (rand_num(100) == 0)	{	/* one percent of the time */
			maze[r->r_y][r->r_x] = WALL4;
		}
# endif
# ifdef MONITOR
		for (pp = monitor; pp < end_monitor; pp++){
			check(pp, r->r_y, r->r_x);
		}
# endif
	}

	r->r_y = y;
	r->r_x = x;
	if (++r >= &removed[MAXREMOVE]){
		rem_index = removed;
	}
	else{
		rem_index = r;
	}

	maze[y][x] = SPACE;
# ifdef MONITOR
	for (pp = monitor; pp < end_monitor; pp++){
		check(pp, y, x);
	}
# endif
}

/**
 * Clear out the walls array.
 */
void clearwalls(){
	REGEN	*rp;

	for (rp = removed; rp < &removed[MAXREMOVE]; rp++){
		rp->r_y = 0;
	}
	rem_index = removed;
}
