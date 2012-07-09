/*	$NetBSD: driver.c,v 1.10 2004/01/27 20:30:29 jsm Exp $	*/
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
__RCSID("$NetBSD: driver.c,v 1.10 2004/01/27 20:30:29 jsm Exp $");
#endif /* not lint */

/* # include	<sys/ioctl.h> already present in hunt.h. [PSR] */
# include	<sys/stat.h>
# include	<sys/time.h>
# include	<err.h>
/* # include	<errno.h> already present in hunt.h. [PSR] */
# include	<signal.h>
# include	<stdlib.h>
# include	<time.h>
# include	<unistd.h>
# include 	<getopt.h> /* Explicit declaration of getopt family functions. [PSR] */

# ifndef pdp11
# define	RN	(((Seed = Seed * 11109 + 13849) >> 16) & 0xffff) /**< A random number. [PSR] */
# else
# define	RN	((Seed = Seed * 11109 + 13849) & 0x7fff)
# endif

int Seed = 0; /**< A random number. [PSR] */

SOCKET Daemon; /**< The daemon socket. [PSR] */
char *First_arg; /**< Pointer to argv[0] */
char *Last_arg; /**< Pointer to end of argv/environ */
# ifdef	INTERNET
int Test_socket; /* test socket to answer datagrams */
bool inetd_spawned; /* invoked via inetd */
bool standard_port = true; /* true if listening on standard port */
/* port # of tcp listen socket */
unsigned short sock_port; /* Changed from u_short. [PSR] */
/* port # of statistics tcp socket */
unsigned short stat_port; /* Changed from u_short. [PSR] */
# define	DAEMON_SIZE	(sizeof Daemon)
# else
# define	DAEMON_SIZE	(sizeof Daemon - 1) /**< TODO [PSR] */
# endif

static void clear_scores(void);
static int havechar(PLAYER *, int);
static void init(void);
int main(int, char *[], char *[]);
#ifdef BOOTS
static void makeboots(void);
#endif
static void send_stats(void);
static void zap(PLAYER *, bool, int);
void erred(char *[]);


/**
 * The main program.
 */
int main(int argc, char* argv[], char* env[]) {
	PLAYER *pp;
#ifdef INTERNET
	unsigned short msg; /**< Changed from u_short. [PSR] */
	unsigned short reply; /** Changed from u_short. [PSR] */
	/**
	 * Edited namelen declaration type in order to match recvfrom() parameter. [PSR]
	 */
	unsigned int namelen; /**< Changed from int. [PSR] */
	SOCKET test;
# endif
	static bool first = true;
	static bool server = false;
	int c, i;
	const int linger = 90 * 1000;

	First_arg = argv[0];
	if (env == NULL || *env == NULL) {
		env = argv + argc;
	}
	while (*env) {
		env++;
	}
	Last_arg = env[-1] + strlen(env[-1]);

	while ((c = getopt(argc, argv, "sp:")) != -1) {
		switch (c) {
		case 's':
			server = true;
			break;
# ifdef INTERNET
			case 'p':
			standard_port = false;
			Test_port = atoi(optarg);
			break;
# endif
		default:
			erred(argv);
			break;
		}
	}
	if (optind < argc) {
		erred(argv);
	}

	init();

	while (true) {
		do {
			errno = 0;
			while (poll(fdset, 3 + MAXPL + MAXMON, INFTIM) < 0) {
				if (errno != EINTR)
# ifdef LOG
					iso_syslog(LOG_WARNING, "select: %m");
# else
					warn("select");
# endif
				errno = 0;
			}
# ifdef INTERNET
			if (fdset[2].revents & POLLIN) {
				namelen = DAEMON_SIZE;
				htons(sock_port);
				(void) recvfrom(Test_socket, (char *) &msg, sizeof msg,
						0, (struct sockaddr *) &test, &namelen);
				switch (ntohs(msg)) {
					case C_MESSAGE:
					if (Nplayer <= 0) {
						break;
					}
					reply = htons((unsigned short) Nplayer);
					(void) sendto(Test_socket, (char *) &reply,
							sizeof reply, 0,
							(struct sockaddr *) &test, DAEMON_SIZE);
					break;
					case C_SCORES:
					reply = htons(stat_port);
					(void) sendto(Test_socket, (char *) &reply,
							sizeof reply, 0,
							(struct sockaddr *) &test, DAEMON_SIZE);
					break;
					case C_PLAYER:
					case C_MONITOR:
					if (msg == C_MONITOR && Nplayer <= 0) {
						break;
					}
					reply = htons(sock_port);
					(void) sendto(Test_socket, (char *) &reply,
							sizeof reply, 0,
							(struct sockaddr *) &test, DAEMON_SIZE);
					break;
				}
			}
# endif
			{
				for (pp = Player, i = 0; pp < End_player; pp++, i++)
					if (havechar(pp, i + 3)) {
						execute(pp);
						pp->p_nexec++;
					}
# ifdef MONITOR
				for (pp = Monitor, i = 0; pp < End_monitor; pp++, i++)
				if (havechar(pp, i + MAXPL + 3)) {
					mon_execute(pp);
					pp->p_nexec++;
				}
# endif
				moveshots();
				for (pp = Player, i = 0; pp < End_player;)
					if (pp->p_death[0] != '\0') {
						zap(pp, true, i + 3);
					} else {
						pp++, i++;
					}
# ifdef MONITOR
				for (pp = Monitor, i = 0; pp < End_monitor; )
				if (pp->p_death[0] != '\0') {
					zap(pp, false, i + MAXPL + 3);
				}
				else {
					pp++, i++;
				}
# endif
			}
			if (fdset[0].revents & POLLIN) {
				if (answer()) {
# ifdef INTERNET
					if (first && standard_port) {
						faketalk();
					}
# endif
					first = false;
				}
			}
			if (fdset[1].revents & POLLIN) {
				send_stats();
			}
			for (pp = Player, i = 0; pp < End_player; pp++, i++) {
				if (fdset[i + 3].revents & POLLIN) {
					sendcom(pp, READY, pp->p_nexec);
				}
				pp->p_nexec = 0;
				(void) fflush(pp->p_output);
			}
# ifdef MONITOR
			for (pp = Monitor, i = 0; pp < End_monitor; pp++, i++) {
				if (fdset[i + MAXPL + 3].revents & POLLIN) {
					sendcom(pp, READY, pp->p_nexec);
				}
				pp->p_nexec = 0;
				(void) fflush(pp->p_output);
			}
# endif
		} while (Nplayer > 0);

		if (poll(fdset, 3 + MAXPL + MAXMON, linger) > 0) {
			continue;
		}
		if (server) {
			clear_scores();
			makemaze();
			clearwalls();
# ifdef BOOTS
			makeboots();
# endif
			first = true;
			continue;
		}
		break;
	}

# ifdef MONITOR
	for (pp = Monitor, i = 0; pp < End_monitor; i++) {
		zap(pp, false, i + MAXPL + 3);
	}
# endif
	cleanup(0);
	/* NOTREACHED */
	return (0);
}

/**
 * Prints on stderr the usage of the function.
 * [PSR]
 */
void erred(char* argv[]) {
	fprintf(stderr, "Usage: %s [-s] [-p port]\n", argv[0]);
	exit(1);
}

/**
 * Initialize the global parameters.
 */
static void init() {
	int i;
# ifdef	INTERNET
	SOCKET test_port;
# ifdef SO_USELOOPBACK
	int msg;
# endif
	/**
	 * Edited len declaration type in order to match getsockname() parameter.
	 */
	unsigned int len; /**< Changed from int. */
# endif

# ifndef DEBUG
	switch (fork()) {
	case -1:
		err(1, "fork");
	case 0:
		break; /* child */
	default:
		exit(0); /* parent */
	}
	if (setsid() == -1) {
		err(1, "setsid");
	}
	(void) signal(SIGHUP, SIG_IGN);
	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGQUIT, SIG_IGN);
	(void) signal(SIGTERM, cleanup);
# endif

	dbg_chdir("/var/tmp");
	/* just in case it core dumps */
	(void) umask(0); /* No privacy at all! */
	(void) signal(SIGPIPE, SIG_IGN);

# ifdef LOG
# ifdef	SYSLOG_43
	openlog("huntd", LOG_PID, LOG_DAEMON);
# endif
# ifdef	SYSLOG_42
	openlog("huntd", LOG_PID);
# endif
# endif

	/*
	 * Initialize statistics socket
	 */
# ifdef	INTERNET
	Daemon.sin_family = SOCK_FAMILY;
	Daemon.sin_addr.s_addr = INADDR_ANY;
	Daemon.sin_port = 0;
# else
	Daemon.sun_family = SOCK_FAMILY;
	(void) strcpy(Daemon.sun_path, Stat_name);
# endif

	Status = socket(SOCK_FAMILY, SOCK_STREAM, 0);
	if (bind(Status, (struct sockaddr *) &Daemon, DAEMON_SIZE) < 0) {
		if (errno == EADDRINUSE) {
			exit(0);
		} else {
# ifdef LOG
			iso_syslog(LOG_ERR, "bind: %m");
# else
			warn("bind");
# endif
			cleanup(1);
		}
	}
	(void) listen(Status, 5);

# ifdef INTERNET
	len = sizeof (SOCKET);
	if (getsockname(Status, (struct sockaddr *) &Daemon, &len) < 0) {
# ifdef LOG
		iso_syslog(LOG_ERR, "getsockname: %m");
# else
		warn("getsockname");
# endif
		exit(1);
	}
	stat_port = ntohs(Daemon.sin_port);
# endif

	/*
	 * Initialize main socket
	 */
# ifdef	INTERNET
	Daemon.sin_family = SOCK_FAMILY;
	Daemon.sin_addr.s_addr = INADDR_ANY;
	Daemon.sin_port = 0;
# else
	Daemon.sun_family = SOCK_FAMILY;
	(void) strcpy(Daemon.sun_path, Sock_name);
# endif

	Socket = socket(SOCK_FAMILY, SOCK_STREAM, 0);
# ifdef INTERNET
# ifdef SO_USELOOPBACK
	msg = 1;
	if (setsockopt(Socket, SOL_SOCKET, SO_USELOOPBACK, &msg, sizeof msg)<0)
# ifdef LOG
	iso_syslog(LOG_WARNING, "setsockopt loopback %m");
# else
	warn("setsockopt loopback");
# endif
#endif
# endif
	if (bind(Socket, (struct sockaddr *) &Daemon, DAEMON_SIZE) < 0) {
		if (errno == EADDRINUSE) {
			exit(0);
		} else {
# ifdef LOG
			iso_syslog(LOG_ERR, "bind: %m");
# else
			warn("bind");
# endif
			cleanup(1);
		}
	}
	(void) listen(Socket, 5);

# ifdef INTERNET
	len = sizeof (SOCKET);
	if (getsockname(Socket, (struct sockaddr *) &Daemon, &len) < 0) {
# ifdef LOG
		iso_syslog(LOG_ERR, "getsockname: %m");
# else
		warn("getsockname");
# endif
		exit(1);
	}
	sock_port = ntohs(Daemon.sin_port);
# endif

	/*
	 * Initialize minimal select mask
	 */
	fdset[0].fd = Socket;
	fdset[0].events = POLLIN;
	fdset[1].fd = Status;
	fdset[1].events = POLLIN;

# ifdef INTERNET
	len = sizeof (SOCKET);
	if (getsockname(0, (struct sockaddr *) &test_port, &len) >= 0
			&& test_port.sin_family == AF_INET) {
		inetd_spawned = true;
		Test_socket = 0;
		if (test_port.sin_port != htons((unsigned short) Test_port)) {
			standard_port = false;
			Test_port = ntohs(test_port.sin_port);
		}
	} else {
		test_port = Daemon;
		test_port.sin_port = htons((unsigned short) Test_port);

		Test_socket = socket(SOCK_FAMILY, SOCK_DGRAM, 0);
		if (bind(Test_socket, (struct sockaddr *) &test_port,
						DAEMON_SIZE) < 0) {
# ifdef LOG
			iso_syslog(LOG_ERR, "bind: %m");
# else
			warn("bind");
# endif
			exit(1);
		}
		(void) listen(Test_socket, 5);
	}

	fdset[2].fd = Test_socket;
	fdset[2].events = POLLIN;
# else
	fdset[2].fd = -1;
# endif

	Seed = getpid() + time((time_t *) NULL);
	makemaze();
# ifdef BOOTS
	makeboots();
# endif

	for (i = 0; i < NASCII; i++) {
		See_over[i] = true;
	}
	See_over[DOOR] = false;
	See_over[WALL1] = false;
	See_over[WALL2] = false;
	See_over[WALL3] = false;
# ifdef REFLECT
	See_over[WALL4] = false;
	See_over[WALL5] = false;
# endif

}

# ifdef BOOTS
/**
 * Put the boots in the maze.
 */
static void makeboots() {
	int x, y;
	PLAYER *pp;

	do {
		x = rand_num(WIDTH - 1) + 1;
		y = rand_num(HEIGHT - 1) + 1;
	}while (Maze[y][x] != SPACE);
	Maze[y][x] = BOOT_PAIR;
	for (pp = Boot; pp < &Boot[NBOOTS]; pp++)
	pp->p_flying = -1;
}
# endif

/**
 * Check the damage to the given player, and see if s/he is killed.
 * @param[in] ouch The victim of the attack. TODO
 * @param[in] gotcha The attacker. TODO
 * @param[in] credit The identifier of the attacker. TODO
 * @param[in] amt The damage amount
 * @param[in] shot_type The type of inferred attack.
 */
void checkdam(PLAYER *ouch, PLAYER *gotcha, IDENT *credit, int amt,
		char shot_type) {
	const char *cp;

	if (ouch->p_death[0] != '\0') {
		return;
	}
# ifdef BOOTS
	if (shot_type == SLIME)
	switch (ouch->p_nboots) {
		default:
		break;
		case 1:
		amt = (amt + 1) / 2;
		break;
		case 2:
		if (gotcha != NULL) {
			message(gotcha, "He has boots on!");
		}
		return;
	}
# endif
	ouch->p_damage += amt;
	if (ouch->p_damage <= ouch->p_damcap) {
		(void) sprintf(Buf, "%2d", ouch->p_damage);
		cgoto(ouch, STAT_DAM_ROW, STAT_VALUE_COL);
		outstr(ouch, Buf, 2);
		return;
	}

	/* Someone DIED */
	switch (shot_type) {
	default:
		cp = "Killed";
		break;
# ifdef FLY
		case FALL:
		cp = "Killed on impact";
		break;
# endif
	case KNIFE:
		cp = "Stabbed to death";
		ouch->p_ammo = 0; /* No exploding */
		break;
	case SHOT:
		cp = "Shot to death";
		break;
	case GRENADE:
	case SATCHEL:
	case BOMB:
		cp = "Bombed";
		break;
	case MINE:
	case GMINE:
		cp = "Blown apart";
		break;
# ifdef	OOZE
		case SLIME:
		cp = "Slimed";
		if (credit != NULL) {
			credit->i_slime++;
		}
		break;
# endif
# ifdef	VOLCANO
		case LAVA:
		cp = "Baked";
		break;
# endif
# ifdef DRONE
		case DSHOT:
		cp = "Eliminated";
		break;
# endif
	}
	if (credit == NULL) {
		(void) sprintf(ouch->p_death, "| %s by %s |", cp,
				(shot_type == MINE || shot_type == GMINE) ?
						"a mine" : "act of God");
		return;
	}

	(void) sprintf(ouch->p_death, "| %s by %s |", cp, credit->i_name);

	if (ouch == gotcha) { /* No use killing yourself */
		credit->i_kills--;
		credit->i_bkills++;
	} else if (ouch->p_ident->i_team == ' '
			|| ouch->p_ident->i_team != credit->i_team) {
		credit->i_kills++;
		credit->i_gkills++;
	} else {
		credit->i_kills--;
		credit->i_bkills++;
	}
	credit->i_score = credit->i_kills / (double) credit->i_entries;
	ouch->p_ident->i_deaths++;
	if (ouch->p_nchar == 0) {
		ouch->p_ident->i_stillb++;
	}
	if (gotcha == NULL) {
		return;
	}
	gotcha->p_damcap += STABDAM;
	gotcha->p_damage -= STABDAM;
	if (gotcha->p_damage < 0) {
		gotcha->p_damage = 0;
	}
	(void) sprintf(Buf, "%2d/%2d", gotcha->p_damage, gotcha->p_damcap);
	cgoto(gotcha, STAT_DAM_ROW, STAT_VALUE_COL);
	outstr(gotcha, Buf, 5);
	(void) sprintf(Buf, "%3d", (gotcha->p_damcap - MAXDAM) / 2);
	cgoto(gotcha, STAT_KILL_ROW, STAT_VALUE_COL);
	outstr(gotcha, Buf, 3);
	(void) sprintf(Buf, "%5.2f", gotcha->p_ident->i_score);
	for (ouch = Player; ouch < End_player; ouch++) {
		cgoto(ouch, STAT_PLAY_ROW + 1 + (gotcha - Player), STAT_NAME_COL);
		outstr(ouch, Buf, 5);
	}
# ifdef MONITOR
	for (ouch = Monitor; ouch < End_monitor; ouch++) {
		cgoto(ouch, STAT_PLAY_ROW + 1 + (gotcha - Player),
				STAT_NAME_COL);
		outstr(ouch, Buf, 5);
	}
# endif
}

/**
 * Kill off a player and take him out of the game.
 * @param[in] pp A player to remove from the game. TODO
 * @param[in] was_player True if the player to remove was really in the game, false otherwise.
 * @param[in] i An identifier.
 */
static void zap(PLAYER *pp, bool was_player, int i) {
	int n, len;
	BULLET *bp;
	PLAYER *np;
	int x, y;
//	int savefd; /**< For future use. [PSR]*/

	if (was_player) {
		if (pp->p_undershot) {
			fixshots(pp->p_y, pp->p_x, pp->p_over);
		}
		drawplayer(pp, false);
		Nplayer--;
	}

	len = strlen(pp->p_death); /* Display the cause of death */
	x = (WIDTH - len) / 2;
	cgoto(pp, HEIGHT / 2, x);
	outstr(pp, pp->p_death, len);
	for (n = 1; n < len; n++) {
		pp->p_death[n] = '-';
	}
	pp->p_death[0] = '+';
	pp->p_death[len - 1] = '+';
	cgoto(pp, HEIGHT / 2 - 1, x);
	outstr(pp, pp->p_death, len);
	cgoto(pp, HEIGHT / 2 + 1, x);
	outstr(pp, pp->p_death, len);
	cgoto(pp, HEIGHT, 0);

//	savefd = pp->p_fd;

# ifdef MONITOR
	if (was_player) {
# endif
	for (bp = Bullets; bp != NULL; bp = bp->b_next) {
		if (bp->b_owner == pp) {
			bp->b_owner = NULL;
		}
		if (bp->b_x == pp->p_x && bp->b_y == pp->p_y) {
			bp->b_over = SPACE;
		}
	}
#ifdef OOZE
	n = rand_num(pp->p_ammo);
	x = rand_num(pp->p_ammo);
	if (x > n) {
		n = x;
	}
	if (pp->p_ammo == 0) {
		x = 0;
	} else if (n == pp->p_ammo - 1) {
		x = pp->p_ammo;
		len = SLIME;
	} else {
		for (x = MAXBOMB - 1; x > 0; x--) {
			if (n >= shot_req[x]) {
				break;
			}
		}
		for (y = MAXSLIME - 1; y > 0; y--) {
			if (n >= slime_req[y]) {
				break;
			}
		}
		if (y >= 0 && slime_req[y] > shot_req[x]) {
			x = slime_req[y];
			len = SLIME;
		} else if (x != 0) {
			len = shot_type[x];
			x = shot_req[x];
		}
	}
#endif
	if (x > 0) {
		(void) add_shot(len, pp->p_y, pp->p_x, pp->p_face, x, (PLAYER *) NULL,
				true, SPACE);
		(void) sprintf(Buf, "%s detonated.", pp->p_ident->i_name);
		for (np = Player; np < End_player; np++) {
			message(np, Buf);
		}
# ifdef MONITOR
		for (np = Monitor; np < End_monitor; np++) {
			message(np, Buf);
		}
# endif
# ifdef BOOTS
		while (pp->p_nboots-- > 0) {
			for (np = Boot; np < &Boot[NBOOTS]; np++) {
				if (np->p_flying < 0) {
					break;
				}
			}
			if (np >= &Boot[NBOOTS]) {
				err(1, "Too many boots");
			}
			np->p_undershot = false;
			np->p_x = pp->p_x;
			np->p_y = pp->p_y;
			np->p_flying = rand_num(20);
			np->p_flyx = 2 * rand_num(6) - 5;
			np->p_flyy = 2 * rand_num(6) - 5;
			np->p_over = SPACE;
			np->p_face = BOOT;
			showexpl(np->p_y, np->p_x, BOOT);
		}
# endif
	}
# ifdef BOOTS
	else if (pp->p_nboots > 0) {
		if (pp->p_nboots == 2) {
			Maze[pp->p_y][pp->p_x] = BOOT_PAIR;
		}
		else {
			Maze[pp->p_y][pp->p_x] = BOOT;
		}
		if (pp->p_undershot) {
			fixshots(pp->p_y, pp->p_x,
					Maze[pp->p_y][pp->p_x]);
		}
	}
# endif

# ifdef VOLCANO
	volcano += pp->p_ammo - x;
	if (rand_num(100) < volcano / 50) {
		do {
			x = rand_num(WIDTH / 2) + WIDTH / 4;
			y = rand_num(HEIGHT / 2) + HEIGHT / 4;
		}while (Maze[y][x] != SPACE);
		(void) add_shot(LAVA, y, x, LEFTS, volcano,
				(PLAYER *) NULL, true, SPACE);
		for (np = Player; np < End_player; np++) {
			message(np, "Volcano eruption.");
		}
		volcano = 0;
	}
# endif

# ifdef	DRONE
	if (rand_num(100) < 2) {
		do {
			x = rand_num(WIDTH / 2) + WIDTH / 4;
			y = rand_num(HEIGHT / 2) + HEIGHT / 4;
		}while (Maze[y][x] != SPACE);
		add_shot(DSHOT, y, x, rand_dir(),
				shot_req[MINDSHOT +
				rand_num(MAXBOMB - MINDSHOT)],
				(PLAYER *) NULL, false, SPACE);
	}
# endif

	sendcom(pp, ENDWIN);
	(void) putc(' ', pp->p_output);
	(void) fclose(pp->p_output);

	End_player--;
	if (pp != End_player) {
		memcpy(pp, End_player, sizeof(PLAYER));
		fdset[i] = fdset[End_player - Player + 3];
		fdset[End_player - Player + 3].fd = -1;
		(void) sprintf(Buf, "%5.2f%c%-10.10s %c", pp->p_ident->i_score,
				stat_char(pp), pp->p_ident->i_name, pp->p_ident->i_team);
		n = STAT_PLAY_ROW + 1 + (pp - Player);
		for (np = Player; np < End_player; np++) {
			cgoto(np, n, STAT_NAME_COL);
			outstr(np, Buf, STAT_NAME_LEN);
		}
# ifdef MONITOR
		for (np = Monitor; np < End_monitor; np++) {
			cgoto(np, n, STAT_NAME_COL);
			outstr(np, Buf, STAT_NAME_LEN);
		}
# endif
	} else
		fdset[i].fd = -1;

	/* Erase the last player */
	n = STAT_PLAY_ROW + 1 + Nplayer;
	for (np = Player; np < End_player; np++) {
		cgoto(np, n, STAT_NAME_COL);
		ce(np);
	}
# ifdef MONITOR
	for (np = Monitor; np < End_monitor; np++) {
		cgoto(np, n, STAT_NAME_COL);
		ce(np);
	}
}
else {
	sendcom(pp, ENDWIN);
	(void) putc(LAST_PLAYER, pp->p_output);
	(void) fclose(pp->p_output);

	End_monitor--;
	if (pp != End_monitor) {
		memcpy(pp, End_monitor, sizeof (PLAYER));
		fdset[i] = fdset[End_monitor - Monitor + MAXPL + 3];
		fdset[End_monitor - Monitor + MAXPL + 3].fd = -1;
		(void) sprintf(Buf, "%5.5s %-10.10s %c", " ",
				pp->p_ident->i_name, pp->p_ident->i_team);
		n = STAT_MON_ROW + 1 + (pp - Player);
		for (np = Player; np < End_player; np++) {
			cgoto(np, n, STAT_NAME_COL);
			outstr(np, Buf, STAT_NAME_LEN);
		}
		for (np = Monitor; np < End_monitor; np++) {
			cgoto(np, n, STAT_NAME_COL);
			outstr(np, Buf, STAT_NAME_LEN);
		}
	} else
	fdset[i].fd = -1;

	/* Erase the last monitor */
	n = STAT_MON_ROW + 1 + (End_monitor - Monitor);
	for (np = Player; np < End_player; np++) {
		cgoto(np, n, STAT_NAME_COL);
		ce(np);
	}
	for (np = Monitor; np < End_monitor; np++) {
		cgoto(np, n, STAT_NAME_COL);
		ce(np);
	}
}
# endif
}

/**
 * Return a random number in a given range.
 * @param[in] range A range of number.
 * \return A radom number.
 */
int rand_num(int range) {
	return (range == 0 ? 0 : RN % range);
}

/**
 * Check to see if we have any characters in the input queue;
 * if we do, read them, stash them away, and return TRUE; else return FALSE.
 * @param[in] pp A player. TODO
 * @param[in] i An identifier.
 * \return True if there are any characters in the input queue, false otherwise.
 */
static int havechar(PLAYER *pp, int i) {

	if (pp->p_ncount < pp->p_nchar){
		return true;
	}
	if (!(fdset[i].revents & POLLIN)){
		return false;
	}
	while (true) {
		errno = 0;
		if ((pp->p_nchar = read(pp->p_fd, pp->p_cbuf, sizeof pp->p_cbuf))
				<= 0) {
			if (errno == EINTR){
				continue;
			}
			pp->p_cbuf[0] = 'q';
		}
		break;
	}
	pp->p_ncount = 0;
	return true;
}

/**
 * Exit with the given value, cleaning up any droppings lying around.
 * @param[in] eval The value to put as exit parameter.
 */
SIGNAL_TYPE cleanup(int eval) {
	PLAYER *pp;

	for (pp = Player; pp < End_player; pp++) {
		cgoto(pp, HEIGHT, 0);
		sendcom(pp, ENDWIN);
		(void) putc(LAST_PLAYER, pp->p_output);
		(void) fclose(pp->p_output);
	}
# ifdef MONITOR
	for (pp = Monitor; pp < End_monitor; pp++) {
		cgoto(pp, HEIGHT, 0);
		sendcom(pp, ENDWIN);
		(void) putc(LAST_PLAYER, pp->p_output);
		(void) fclose(pp->p_output);
	}
# endif
	(void) close(Socket);
# ifdef AF_UNIX_HACK
	(void) unlink(Sock_name);
# endif

	exit(eval);
}

/**
 * Print stats to requestor.
 */
static void send_stats() {
	IDENT *ip;
	FILE *fp;
	int s;
	SOCKET sockstruct;
	unsigned int socklen; /**< Edited  declaration type in order to match accept() parameter.[PSR] */

	/*
	 * Get the output stream ready
	 */
# ifdef INTERNET
	socklen = sizeof sockstruct;
# else
	socklen = sizeof sockstruct - 1;
# endif
	s = accept(Status, (struct sockaddr *) &sockstruct, &socklen);
	if (s < 0) {
		if (errno == EINTR){
			return;
		}
# ifdef LOG
		iso_syslog(LOG_WARNING, "accept: %m");
# else
		warn("accept");
# endif
		return;
	}
	fp = fdopen(s, "w");
	if (fp == NULL) {
# ifdef LOG
		iso_syslog(LOG_WARNING, "fdopen: %m");
# else
		warn("fdopen");
# endif
		(void) close(s);
		return;
	}

	/*
	 * Send output to requestor
	 */
	fputs(
			"Name\t\tScore\tDucked\tAbsorb\tFaced\tShot\tRobbed\tMissed\tSlimeK\n",
			fp);
	for (ip = Scores; ip != NULL; ip = ip->i_next) {
		fprintf(fp, "%s\t", ip->i_name);
		if (strlen(ip->i_name) < 8){
			putc('\t', fp);
		}
		fprintf(fp, "%.2f\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", ip->i_score,
				ip->i_ducked, ip->i_absorbed, ip->i_faced, ip->i_shot,
				ip->i_robbed, ip->i_missed, ip->i_slime);
	}
	fputs("\n\nName\t\tEnemy\tFriend\tDeaths\tStill\tSaved\n", fp);
	for (ip = Scores; ip != NULL; ip = ip->i_next) {
		if (ip->i_team == ' ') {
			fprintf(fp, "%s\t", ip->i_name);
			if (strlen(ip->i_name) < 8){
				putc('\t', fp);
			}
		} else {
			fprintf(fp, "%s[%c]\t", ip->i_name, ip->i_team);
			if (strlen(ip->i_name) + 3 < 8){
				putc('\t', fp);
			}
		}
		fprintf(fp, "%d\t%d\t%d\t%d\t%d\n", ip->i_gkills, ip->i_bkills,
				ip->i_deaths, ip->i_stillb, ip->i_saved);
	}

	(void) fclose(fp);
}

/**
 * Clear out the scores so the next session start clean.
 */
static void clear_scores() {
	IDENT *ip, *nextip;

	for (ip = Scores; ip != NULL; ip = nextip) {
		nextip = ip->i_next;
		(void) free((char *) ip);
	}
	Scores = NULL;
}

