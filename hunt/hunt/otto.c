/*	$NetBSD: otto.c,v 1.8 2004/11/05 21:30:32 dsl Exp $	*/
# ifdef OTTO
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

/*
 *	otto	- a hunt otto-matic player
 *
 *		This guy is buggy, unfair, stupid, and not extensible.
 *	Future versions of hunt will have a subroutine library for
 *	automatic players to link to.  If you write your own "otto"
 *	please let us know what subroutines you would expect in the
 *	subroutine library.
 *
 *	Id: otto.c,v 1.14 2003/04/16 06:11:54 gregc Exp
 */

# include	"hunt.h"

/**< #include <sys/cdefs.h> pushed up in hunt.h. */
#ifndef lint
__RCSID("$NetBSD: otto.c,v 1.8 2004/11/05 21:30:32 dsl Exp $");
#endif /* not lint */

# include	<sys/time.h>
# include	<ncurses.h> /**< Edited from curses.h. */
# include	<ctype.h>
# include	<signal.h>
# include	<stdlib.h>
# include	<unistd.h>

# undef		WALL
# undef		NORTH
# undef		SOUTH
# undef		WEST
# undef		EAST
# undef		FRONT
# undef		LEFT
# undef		BACK
# undef		RIGHT

# ifdef HPUX
# define	random		rand
# endif

# ifndef USE_CURSES
extern char screen[SCREEN_HEIGHT][SCREEN_WIDTH2];
# define	SCREEN(y, x)	screen[y][x]
# else
# define	SCREEN(y, x)	mvinch(y, x)
# endif

# ifndef DEBUG
# define	STATIC		static
# else
# define	STATIC
# endif

# define	OPPONENT	"{}i!"
# define	PROPONENT	"^v<>"
# define	WALL		"+\\/#*-|"
# define	PUSHOVER	" bg;*#&"
# define	SHOTS		"$@Oo:"

/* number of "directions" */
# define	NUMDIRECTIONS	4

/* absolute directions (facings) - counterclockwise */
# define	NORTH		0
# define	WEST		1
# define	SOUTH		2
# define	EAST		3
# define	ALLDIRS		0xf

/* relative directions - counterclockwise */
# define	FRONT		0
# define	LEFT		1
# define	BACK		2
# define	RIGHT		3

# define	ABSCHARS	"NWSE"
# define	RELCHARS	"FLBR"
# define	DIRKEYS		"khjl"

STATIC char command[BUFSIZ];
STATIC int comlen;

# ifdef	DEBUG
STATIC FILE *debug = NULL;
# endif

# define	DEADEND		0x1
# define	ON_LEFT		0x2
# define	ON_RIGHT	0x4
# define	ON_SIDE		(ON_LEFT|ON_RIGHT)
# define	BEEN		0x8
# define	BEEN_SAME	0x10

struct item {
	char what;
	int distance;
	int flags;
};

STATIC struct item flbr[NUMDIRECTIONS];

# define	fitem	flbr[FRONT]
# define	litem	flbr[LEFT]
# define	bitem	flbr[BACK]
# define	ritem	flbr[RIGHT]

STATIC int facing;
STATIC int row, col;
STATIC int num_turns; /* for wandering */
STATIC char been_there[HEIGHT][WIDTH2];
STATIC struct itimerval pause_time = { {0, 0}, {0, 55000}};

STATIC void attack(int, struct item *);
STATIC void duck(int);
STATIC void face_and_move_direction(int, int);
STATIC int go_for_ammo(char);
STATIC void ottolook(int, struct item *);
STATIC void look_around(void);
STATIC int stop_look(struct item *, char, int, int);
STATIC void wander(void);
void empty_handler(int);

extern int Otto_count;

/**
 * Helper function created in order to manage signals.
 * @param param a not used integer to mantain the function signature.
 */
void empty_handler(int param) {

}

//TODO da qui iniziare documentazione

void otto(int y,int x,char face) {
	int i;
	struct sigaction handler, old_handler;
	sigset_t old_mask, sig_mask;

	bool done;

# ifdef	DEBUG
	if (debug == NULL) {
		debug = fopen("bug", "w");
		setbuf(debug, NULL);
	}
	fprintf(debug, "\n%c(%d,%d)", face, y, x);
# endif
	handler.sa_handler=&empty_handler;
	sigaction(SIGALRM, &handler, &old_handler);
	sigemptyset(&sig_mask);
	sigaddset(&sig_mask, SIGALRM);
	sigprocmask(SIG_BLOCK, &sig_mask, &old_mask);
	setitimer(ITIMER_REAL, &pause_time, NULL);
	sigsuspend(&old_mask);
	sigprocmask(SIG_SETMASK, &old_mask, NULL);

	/* save away parameters so other functions may use/update info */
	switch (face) {
		case '^': facing = NORTH; break;
		case '<': facing = WEST; break;
		case 'v': facing = SOUTH; break;
		case '>': facing = EAST; break;
		default: abort();
	}
	row = y; col = x;
	been_there[row][col] |= 1 << facing;

	/* initially no commands to be sent */
	comlen = 0;

	/* find something to do */
	look_around();

	done=FALSE;

	for (i = 0; i < NUMDIRECTIONS; i++) {
		if (strchr(OPPONENT, flbr[i].what) != NULL) {
			attack(i, &flbr[i]);
			memset(been_there, 0, sizeof been_there);
			done=TRUE;
			break;
		}
	}

	if(!done) {
		if (strchr(SHOTS, bitem.what) != NULL && !(bitem.what & ON_SIDE)) {
			duck(BACK);
			memset(been_there, 0, sizeof been_there);
# ifdef BOOTS
		} else if (go_for_ammo(BOOT_PAIR)) {
			memset(been_there, 0, sizeof been_there);
		} else if (go_for_ammo(BOOT)) {
			memset(been_there, 0, sizeof been_there);
# endif
		} else if (go_for_ammo(GMINE))
		memset(been_there, 0, sizeof been_there);
		else if (go_for_ammo(MINE))
		memset(been_there, 0, sizeof been_there);
		else
		wander();
	}

	dbg_write(Socket, command, comlen);
	Otto_count += comlen;
# ifdef	DEBUG
	(void) fwrite(command, 1, comlen, debug);
# endif
}

# define	direction(abs,rel)	(((abs) + (rel)) % NUMDIRECTIONS)

STATIC int stop_look(struct item *itemp,char c,int dist,int side) {
	switch (c) {

		case SPACE:
		if (side)
		itemp->flags &= ~DEADEND;
		return 0;

		case MINE:
		case GMINE:
# ifdef BOOTS
		case BOOT:
		case BOOT_PAIR:
# endif
		if (itemp->distance == -1) {
			itemp->distance = dist;
			itemp->what = c;
			if (side < 0)
			itemp->flags |= ON_LEFT;
			else if (side > 0)
			itemp->flags |= ON_RIGHT;
		}
		return 0;

		case SHOT:
		case GRENADE:
		case SATCHEL:
		case BOMB:
# ifdef OOZE
		case SLIME:
# endif
		if (itemp->distance == -1 || (!side
						&& (itemp->flags & ON_SIDE
								|| itemp->what == GMINE || itemp->what == MINE))) {
			itemp->distance = dist;
			itemp->what = c;
			itemp->flags &= ~ON_SIDE;
			if (side < 0)
			itemp->flags |= ON_LEFT;
			else if (side > 0)
			itemp->flags |= ON_RIGHT;
		}
		return 0;

		case '{':
		case '}':
		case 'i':
		case '!':
		itemp->distance = dist;
		itemp->what = c;
		itemp->flags &= ~(ON_SIDE|DEADEND);
		if (side < 0)
		itemp->flags |= ON_LEFT;
		else if (side > 0)
		itemp->flags |= ON_RIGHT;
		return 1;

		default:
		/* a wall or unknown object */
		if (side)
		return 0;
		if (itemp->distance == -1) {
			itemp->distance = dist;
			itemp->what = c;
		}
		return 1;
	}
}

STATIC void ottolook(int rel_dir,struct item *itemp) {
	int r, c;
	char ch;
	bool exit;

	r = 0;
	itemp->what = 0;
	itemp->distance = -1;
	itemp->flags = DEADEND|BEEN; /* true until proven false */

	switch (direction(facing, rel_dir)) {

		case NORTH:
		if (been_there[row - 1][col] & NORTH)
		itemp->flags |= BEEN_SAME;

		exit = FALSE;
		for (r = row - 1; r >= 0 && !exit; r--) {
			for (c = col - 1; (c < col + 2) && !exit; c++) {
				ch = SCREEN(r, c);
				if (stop_look(itemp, ch, row - r, c - col)) {
					exit = TRUE;
				} else {
					if (c == col && !been_there[r][c]) {
						itemp->flags &= ~BEEN;
					}
				}
			}
		}

		if (itemp->flags & DEADEND) {
			itemp->flags |= BEEN;
			been_there[r][col] |= NORTH;
			for (r = row - 1; r > row - itemp->distance; r--) {
				been_there[r][col] = ALLDIRS;
			}
		}
		break;

		case SOUTH:
		if (been_there[row + 1][col] & SOUTH)
		itemp->flags |= BEEN_SAME;
		exit=FALSE;
		for (r = row + 1; r < HEIGHT && !exit; r++) {
			for (c = col - 1; (c < col + 2) && !exit; c++) {
				ch = SCREEN(r, c);
				if (stop_look(itemp, ch, r - row, col - c)) {
					exit=TRUE;
				} else {
					if (c == col && !been_there[r][c]) {
						itemp->flags &= ~BEEN;
					}
				}
			}
		}

		if (itemp->flags & DEADEND) {
			itemp->flags |= BEEN;
			been_there[r][col] |= SOUTH;
			for (r = row + 1; r < row + itemp->distance; r++) {
				been_there[r][col] = ALLDIRS;
			}
		}
		break;

		case WEST:
		if (been_there[row][col - 1] & WEST)
		itemp->flags |= BEEN_SAME;
		exit=FALSE;
		for (c = col - 1; c >= 0 && !exit; c--) {
			for (r = row - 1; r < (row + 2)&&!exit; r++) {
				ch = SCREEN(r, c);
				if (stop_look(itemp, ch, col - c, row - r)) {
					exit=TRUE;
				} else {
					if (r == row && !been_there[r][c]) {
						itemp->flags &= ~BEEN;
					}
				}
			}
		}

		if (itemp->flags & DEADEND) {
			itemp->flags |= BEEN;
			been_there[r][col] |= WEST;
			for (c = col - 1; c > col - itemp->distance; c--) {
				been_there[row][c] = ALLDIRS;
			}
		}
		break;

		case EAST:
		if (been_there[row][col + 1] & EAST)
		itemp->flags |= BEEN_SAME;
		exit=FALSE;
		for (c = col + 1; c < WIDTH && !exit; c++) {
			for (r = row - 1; r < (row + 2) && !exit; r++) {
				ch = SCREEN(r, c);
				if (stop_look(itemp, ch, c - col, r - row)) {
					exit=TRUE;
				} else {
					if (r == row && !been_there[r][c]) {
						itemp->flags &= ~BEEN;
					}
				}
			}
		}

		if (itemp->flags & DEADEND) {
			itemp->flags |= BEEN;
			been_there[r][col] |= EAST;
			for (c = col + 1; c < col + itemp->distance; c++) {
				been_there[row][c] = ALLDIRS;
			}
		}
		break;

		default:
		abort();
	}
}

STATIC void look_around() {
	int i;

	for (i = 0; i < NUMDIRECTIONS; i++) {
		ottolook(i, &flbr[i]);
# ifdef	DEBUG
		fprintf(debug, " ottolook(%c)=%c(%d)(0x%x)",
				RELCHARS[i], flbr[i].what, flbr[i].distance, flbr[i].flags);
# endif
	}
}

/*
 *	as a side effect modifies facing and location (row, col)
 */

STATIC void face_and_move_direction(int rel_dir,int distance) {
	int old_facing;
	char cmd;

	old_facing = facing;
	cmd = DIRKEYS[facing = direction(facing, rel_dir)];

	if (rel_dir != FRONT) {
		int i;
		struct item items[NUMDIRECTIONS];

		command[comlen++] = toupper((unsigned char)cmd);
		if (distance == 0) {
			/* rotate ottolook's to be in right position */
			for (i = 0; i < NUMDIRECTIONS; i++)
			items[i] =
			flbr[(i + old_facing) % NUMDIRECTIONS];
			memcpy(flbr, items, sizeof flbr);
		}
	}
	while (distance--) {
		command[comlen++] = cmd;
		switch (facing) {

			case NORTH: row--; break;
			case WEST: col--; break;
			case SOUTH: row++; break;
			case EAST: col++; break;
		}
		if (distance == 0)
		look_around();
	}
}

STATIC void attack(int rel_dir,struct item *itemp) {
	if (!(itemp->flags & ON_SIDE)) {
		face_and_move_direction(rel_dir, 0);
		command[comlen++] = 'o';
		command[comlen++] = 'o';
		duck(FRONT);
		command[comlen++] = ' ';
	} else if (itemp->distance > 1) {
		face_and_move_direction(rel_dir, 2);
		duck(FRONT);
	} else {
		face_and_move_direction(rel_dir, 1);
		if (itemp->flags & ON_LEFT)
		rel_dir = LEFT;
		else
		rel_dir = RIGHT;
		(void) face_and_move_direction(rel_dir, 0);
		command[comlen++] = 'f';
		command[comlen++] = 'f';
		duck(FRONT);
		command[comlen++] = ' ';
	}
}

STATIC void duck(int rel_dir) {
	int dir;

	switch (dir = direction(facing, rel_dir)) {

		case NORTH:
		case SOUTH:
		if (strchr(PUSHOVER, SCREEN(row, col - 1)) != NULL)
		command[comlen++] = 'h';
		else if (strchr(PUSHOVER, SCREEN(row, col + 1)) != NULL)
		command[comlen++] = 'l';
		else if (dir == NORTH
				&& strchr(PUSHOVER, SCREEN(row + 1, col)) != NULL)
		command[comlen++] = 'j';
		else if (dir == SOUTH
				&& strchr(PUSHOVER, SCREEN(row - 1, col)) != NULL)
		command[comlen++] = 'k';
		else if (dir == NORTH)
		command[comlen++] = 'k';
		else
		command[comlen++] = 'j';
		break;

		case WEST:
		case EAST:
		if (strchr(PUSHOVER, SCREEN(row - 1, col)) != NULL)
		command[comlen++] = 'k';
		else if (strchr(PUSHOVER, SCREEN(row + 1, col)) != NULL)
		command[comlen++] = 'j';
		else if (dir == WEST
				&& strchr(PUSHOVER, SCREEN(row, col + 1)) != NULL)
		command[comlen++] = 'l';
		else if (dir == EAST
				&& strchr(PUSHOVER, SCREEN(row, col - 1)) != NULL)
		command[comlen++] = 'h';
		else if (dir == WEST)
		command[comlen++] = 'h';
		else
		command[comlen++] = 'l';
		break;
	}
}

/*
 *	go for the closest mine if possible
 */

STATIC int go_for_ammo(char mine) {
	int i, rel_dir, dist;

	rel_dir = -1;
	dist = WIDTH;
	for (i = 0; i < NUMDIRECTIONS; i++) {
		if (flbr[i].what == mine && flbr[i].distance < dist) {
			rel_dir = i;
			dist = flbr[i].distance;
		}
	}
	if (rel_dir == -1)
	return false;

	if (!(flbr[rel_dir].flags & ON_SIDE)
			|| flbr[rel_dir].distance > 1) {
		if (dist > 4)
		dist = 4;
		face_and_move_direction(rel_dir, dist);
	} else
	return false; /* until it's done right */
	return true;
}

STATIC void wander() {
	int i, j, rel_dir, dir_mask, dir_count;

	for (i = 0; i < NUMDIRECTIONS; i++)
	if (!(flbr[i].flags & BEEN) || flbr[i].distance <= 1)
	break;
	if (i == NUMDIRECTIONS)
	memset(been_there, 0, sizeof been_there);
	dir_mask = dir_count = 0;
	for (i = 0; i < NUMDIRECTIONS; i++) {
		j = (RIGHT + i) % NUMDIRECTIONS;
		if (flbr[j].distance <= 1 || flbr[j].flags & DEADEND)
		continue;
		if (!(flbr[j].flags & BEEN_SAME)) {
			dir_mask = 1 << j;
			dir_count = 1;
			break;
		}
		if (j == FRONT
				&& num_turns > 4 + (random() %
						((flbr[FRONT].flags & BEEN) ? 7 : HEIGHT)))
		continue;
		dir_mask |= 1 << j;
# ifdef notdef
		dir_count++;
# else
		dir_count = 1;
		break;
# endif
	}
	if (dir_count == 0) {
		duck(random() % NUMDIRECTIONS);
		num_turns = 0;
		return;
	} else if (dir_count == 1)
	rel_dir = ffs(dir_mask) - 1;
	else {
		rel_dir = ffs(dir_mask) - 1;
		dir_mask &= ~(1 << rel_dir);
		while (dir_mask != 0) {
			i = ffs(dir_mask) - 1;
			if (random() % 5 == 0)
			rel_dir = i;
			dir_mask &= ~(1 << i);
		}
	}
	if (rel_dir == FRONT)
	num_turns++;
	else
	num_turns = 0;

# ifdef DEBUG
	fprintf(debug, " w(%c)", RELCHARS[rel_dir]);
# endif
	face_and_move_direction(rel_dir, 1);
}

# endif /* OTTO */
