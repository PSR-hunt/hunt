/*	$NetBSD: hunt.c,v 1.23 2004/11/05 21:30:32 dsl Exp $	*/
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
__RCSID("$NetBSD: hunt.c,v 1.23 2004/11/05 21:30:32 dsl Exp $");
#endif /* not lint */

# include	<sys/param.h>
# include	<sys/stat.h>
# include	<sys/time.h>
/* # include	<sys/poll.h> already present in hunt.h. [PSR] */
# include	<ctype.h>
# include	<err.h>
/* # include	<errno.h> already present in hunt.h. [PSR] */
# include	<ncurses.h> /* Edited from curses.h. [PSR] */
# include	<signal.h>
# include	<stdlib.h>
/* # include	<string.h> already present in hunt.h. [PSR] */
# if !defined(USE_CURSES) && defined(BSD_RELEASE) && BSD_RELEASE >= 44
# include	<termios.h>
static struct termios saved_tty;
# endif
# include	<unistd.h>
# include	<ifaddrs.h>

# include 	<getopt.h> /* Explicit declaration of getopt family functions. [PSR]*/

# include	<net/if.h> /* Added library to support net interface socket communication. [PSR]*/

# define EMERGENCY_BUFFER_LENGTH 1024 /**< Overextimated length of a line in configuration file (used only if the program is not able to compute the real input line length). [PSR] */

/*
 * Some old versions of curses don't have these defined
 */
# if !defined(cbreak) && (!defined(BSD_RELEASE) || BSD_RELEASE < 44)
# define	cbreak()	crmode() /**< Turn the terminal CBREAK mode on or off. [PSR] */
# endif

# if !defined(USE_CURSES) || !defined(TERMINFO)
# define	beep()		(void) putchar(CTRL('G')) /**< Writes the character c, cast to an unsigned char, to stream. [PSR] */
# endif
# if !defined(USE_CURSES)
# undef		refresh
# define	refresh()	(void) fflush(stdout); /**< Flushes a stream. [PSR] */
# endif
# ifdef USE_CURSES
# define	clear_eol()	clrtoeol()
# define	put_ch		addch
# define	put_str		addstr
# endif
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256 /**< Defines the maximum length of an host name. [PSR] */
#endif

bool Last_player = false; /**< Defines if a player is the last one or no. [PSR] */
# ifdef MONITOR
bool Am_monitor = false;
# endif

char Buf[BUFSIZ]; /**< General scribbling buffer. Defined but not used in this file. [PSR] */

int Socket; /**< Main socket. [PSR] */
# ifdef INTERNET
char *Sock_host;
char *use_port;
bool Query_driver = false;
char *Send_message = NULL;
bool Show_scores = false;
# endif

SOCKET Daemon; /**< The daemon socket. [PSR] */
# ifdef	INTERNET
# define	DAEMON_SIZE	(sizeof Daemon)
# else
# define	DAEMON_SIZE	(sizeof Daemon - 1) /**< Defines the daemon size in case we are not in INTERNET mode. [PSR] */
# endif

char map_key[256]; /**< what to map keys to */
bool no_beep; /**< If true turns off beeping when you reach the typeahead limit. [PSR] */

static char name[NAMELEN]; /**< Indicates the name of a player. [PSR] */
static char team = ' '; /**< Indicates the belonging team of a player. [PSR] */

static int in_visual; /**< Indicates whether the configurations are correct. TODO [PSR] */

extern int cur_row; /**< Indicates the current row. [PSR] */
extern int cur_col; /**< Indicates the current column. [PSR] */
#ifdef INTERNET
void dump_scores(SOCKET);
#endif
long env_init(long);
long var_env_init(long);
void fill_in_blanks(void);
void leave(int, const char *) __attribute__((__noreturn__));
void leavex(int, const char *) __attribute__((__noreturn__));
void fincurs(void);
void usage(void);
int main(int, char *[]);
# ifdef INTERNET
SOCKET *list_drivers(void);
# endif
long fchars_in_line(FILE*); /* Added explicit declaration to avoid implicit declaration. [PSR] */

#ifdef OTTO
extern int Otto_mode;
#endif

extern int _tty_ch;
extern struct sgttyb _tty;

/**
 * Main program for local process.
 */
int main(int argc, char* argv[]) {

	char *term;
	int c;
	long enter_status;
	bool exit_outer_loop;

	/* Revoke setgid privileges */
	setregid(getgid(), getgid());

	enter_status = env_init((long) Q_CLOAK);
	while ((c = getopt(argc, argv, "Sbcfh:l:mn:op:qst:w:")) != -1) {
		switch (c) {
		case 'l': /* rsh compatibility */
		case 'n':
			(void) strncpy(name, optarg, NAMELEN);
			break;
		case 't':
			team = *optarg;
			if (!isdigit((unsigned char)team)) {
				warnx("Team names must be numeric");
				team = ' ';
			}
			break;
		case 'o':
# ifndef OTTO
			warnx("The -o flag is reserved for future use.");
			usage();
# else
			Otto_mode = true;
# endif
			break;
		case 'm':
# ifdef MONITOR
			Am_monitor = true;
# else
			warnx("The monitor was not compiled in.");
# endif
			break;
# ifdef INTERNET
			case 'S':
			Show_scores = true;
			break;
			case 'q': /* query whether hunt is running */
			Query_driver = true;
			break;
			case 'w':
			Send_message = optarg;
			break;
			case 'h':
			Sock_host = optarg;
			break;
			case 'p':
			use_port = optarg;
			Test_port = atoi(use_port);
			break;
# else
		case 'S':
		case 'q':
		case 'w':
		case 'h':
		case 'p':
			wanrx("Need TCP/IP for S, q, w, h, and p options.");
			break;
# endif
		case 'c':
			enter_status = Q_CLOAK;
			break;
		case 'f':
# ifdef FLY
			enter_status = Q_FLY;
# else
			warnx("The flying code was not compiled in.");
# endif
			break;
		case 's':
			enter_status = Q_SCAN;
			break;
		case 'b':
			no_beep = !no_beep;
			break;
		default:
			fputs(
					"usage:\thunt [-qmcsfS] [-n name] [-t team] [-p port] [-w message] [host]\n",
					stderr);
			exit(1);
		}
	}
# ifdef INTERNET
	if (optind + 1 < argc) {
		usage();
	}
	else if (optind + 1 == argc) {
		Sock_host = argv[argc - 1];
	}
# else
	if (optind > argc) {
		usage();
	}
# endif

# ifdef INTERNET
	if (Show_scores) {
		SOCKET *hosts;

		for (hosts = list_drivers(); hosts->sin_port != 0; hosts += 1) {
			dump_scores(*hosts);
		}
		exit(0);
	}
	if (Query_driver) {
		SOCKET *hosts;

		for (hosts = list_drivers(); hosts->sin_port != 0; hosts += 1) {
			struct hostent *hp;
			int num_players;

			hp = gethostbyaddr((char *) &hosts->sin_addr,
					sizeof hosts->sin_addr, AF_INET);
			num_players = ntohs(hosts->sin_port);
			printf("%d player%s hunting on %s!\n",
					num_players, (num_players == 1) ? "" : "s",
					hp != NULL ? hp->h_name :
					inet_ntoa(hosts->sin_addr));
		}
		exit(0);
	}
# endif
# ifdef OTTO
	if (Otto_mode){
		(void) strncpy(name, "otto", NAMELEN);
	}
	else
# endif
	fill_in_blanks();

	(void) fflush(stdout);
	if (!isatty(0) || (term = getenv("TERM")) == NULL) {
		errx(1, "no terminal type");
	}
# ifdef USE_CURSES
	initscr();
	(void) noecho();
	(void) cbreak();
# else /* !USE_CURSES */
# if !defined(BSD_RELEASE) || BSD_RELEASE < 44
	_tty_ch = 0;
# endif
	gettmode();
	(void) setterm(term);
	(void) noecho();
	(void) cbreak();
# if defined(BSD_RELEASE) && BSD_RELEASE >= 44
	tcgetattr(0, &saved_tty);
# endif
# if defined(TI) && defined(VS)
	_puts(TI);
	_puts(VS);
#else
	errx(1, "Missing necessary configuration entries.\nHunt will quit.\n");
#endif
# endif /* !USE_CURSES */
	in_visual = true;
	if (LINES < SCREEN_HEIGHT || COLS < SCREEN_WIDTH) {
		leavex(1, "Need a larger window");
	}
	clear_the_screen();
	(void) signal(SIGINT, intr);
	(void) signal(SIGTERM, sigterm);
	(void) signal(SIGUSR1, sigusr1);
	(void) signal(SIGPIPE, SIG_IGN);
#if !defined(USE_CURSES) && defined(SIGTSTP)
	(void) signal(SIGTSTP, tstp);
#endif

	exit_outer_loop = FALSE;
	while (!exit_outer_loop) {
# ifdef	INTERNET
		find_driver(true);

		if (Daemon.sin_port == 0) {
			leavex(1, "Game not found, try again");
		}

		while(1) {
			do {
#ifdef SO_USELOOPBACK
				int option;
#endif

				Socket = socket(SOCK_FAMILY, SOCK_STREAM, 0);
				if (Socket < 0) {
					err(1, "socket");
				}

#ifdef SO_USELOOPBACK
				option = 1;
				if (setsockopt(Socket, SOL_SOCKET, SO_USELOOPBACK,
								&option, sizeof option) < 0) {
					warn("setsockopt loopback");
				}
#endif
				errno = 0;
				if (connect(Socket, (struct sockaddr *) &Daemon,
								DAEMON_SIZE) < 0) {
					if (errno != ECONNREFUSED) {
						leave(1, "connect");
					}
				} else {
					break;
				}
				sleep(1);
			}while (close(Socket) == 0);

			do_connect(name, team, enter_status);

			if (Send_message != NULL) {
				do_message();
				if (enter_status == Q_MESSAGE) {
					exit_outer_loop = TRUE;
					break;
				} else {
					Send_message = NULL;
				}
			} else {
				break;
			}
		}

		if(exit_outer_loop) {
			break;
		}

# else /* !INTERNET */
		/*
		 * set up a socket
		 */

		if ((Socket = socket(SOCK_FAMILY, SOCK_STREAM, 0)) < 0) {
			err(1, "socket");
		}

		/*
		 * attempt to connect the socket to a name; if it fails that
		 * usually means that the driver isn't running, so we start
		 * up the driver.
		 */

		Daemon.sun_family = SOCK_FAMILY;
		(void) strcpy(Daemon.sun_path, Sock_name);
		if (connect(Socket, &Daemon, DAEMON_SIZE) < 0) {
			if (errno != ENOENT) {
				leavex(1, "connect2");
			}
			start_driver();

			do {
				(void) close(Socket);
				if ((Socket = socket(SOCK_FAMILY, SOCK_STREAM, 0)) < 0) {
					err(1, "socket");
				}
				sleep(2);
			} while (connect(Socket, &Daemon, DAEMON_SIZE) < 0);
		}

		do_connect(name, team, enter_status);
# endif

		playit();
		if ((enter_status = quit(enter_status)) == Q_QUIT) {
			break;
		}
	}
	leavex(0, (char *) NULL);
	/* NOTREACHED */
	return (0);
}

# ifdef INTERNET
# ifdef BROADCAST
/**
 * Counts the number of network interfaces only in the case
 * they are of type INET and BROADCAST.
 * @param[out] vector The place where the list of interfaces is saved.
 * \return the counted number of network interfaces.
 * [PSR]
 */
int broadcast_vec(struct sockaddr **vector) {
	/* int			s;		 socket Deprecated. [PSR] */
	int vec_cnt;
	struct ifaddrs *ifp, *ip;

	*vector = NULL; /* Initialize the sockadrr vector. [PSR] */
	if (getifaddrs(&ifp) < 0) { /* Acquires network interfaces. [PSR] */
		return 0; /* Exit on failure acquisition. [PSR] */
	}

	vec_cnt = 0;
	for (ip = ifp; ip; ip = ip->ifa_next) { /* ip is now on the last interface of the list. [PSR] */
		if ((ip->ifa_addr->sa_family == AF_INET) &&
				(ip->ifa_flags & IFF_BROADCAST)) {
			vec_cnt++;
		}
	}

	*vector = (struct sockaddr *)
	malloc(vec_cnt * sizeof(struct sockaddr_in));
	if (*vector == NULL) {
		leave(1, "Out of memory!");
	}

	vec_cnt = 0;
	for (ip = ifp; ip; ip = ip->ifa_next) {
		if ((ip->ifa_addr->sa_family == AF_INET) &&
				(ip->ifa_flags & IFF_BROADCAST)) {
			memcpy(&(*vector)[vec_cnt++], ip->ifa_broadaddr,
					sizeof(struct sockaddr_in));
		}
	}

	freeifaddrs(ifp);
	return vec_cnt;
}
# endif

/**
 * Helper function to print the usage of the program on stderr.
 * [PSR]
 */
void usage() {
	fputs("usage:\thunt [-qmcsfS] [-n name] [-t team] [-p port] [-w message] [host]\n", stderr);
	exit(1);
}

/**
 * Creates a list of local sockets.
 * @\return The created list.
 * [PSR]
 */
SOCKET * list_drivers() {
	int option;
	unsigned short msg; /* Changed from u_short. [PSR] */
	unsigned short port_num; /* Changed from u_short. [PSR] */
	static SOCKET test;
	int test_socket;
	/*
	 * Edited namelen declaration type to unsigned int to match recvfrom() parameter.
	 * [PSR]
	 */
	unsigned int namelen;
	char local_name[MAXHOSTNAMELEN + 1];
	static int initial = true;
	static struct in_addr local_address;
	struct hostent *hp;
# ifdef BROADCAST
	static int brdc;
	static SOCKET *brdv;
# else
	unsigned long local_net; /* Changed from u_long. [PSR] */
# endif
	int i;
	static SOCKET *listv;
	static unsigned int listmax;
	unsigned int listc;
	struct pollfd set[1];

	if (initial) { /* do one time initialization */
# ifndef BROADCAST
		sethostent(1); /* don't bother to close host file */
# endif
		if (gethostname(local_name, sizeof local_name) < 0) {
			leavex(1, "Sorry, I have no name.");
			/* NOTREACHED */
		}
		local_name[sizeof(local_name) - 1] = '\0';
		if ((hp = gethostbyname(local_name)) == NULL) {
			leavex(1, "Can't find myself.");
			/* NOTREACHED */
		}
		local_address = * ((struct in_addr *) hp->h_addr);

		listmax = 20;
		listv = (SOCKET *) malloc(listmax * sizeof (SOCKET));
		if (listv == NULL) {
			leave(1, "Out of memory!");
		}
	} else if (Sock_host != NULL) {
		return listv; /* address already valid */
	}

	test_socket = socket(SOCK_FAMILY, SOCK_DGRAM, 0); /* Creation of an UDP socket. [PSR] */
	if (test_socket < 0) {
		leave(1, "socket system call failed");
		/* NOTREACHED */
	}
	test.sin_family = SOCK_FAMILY;
	test.sin_port = htons(Test_port);
	listc = 0;

	if (Sock_host != NULL) { /* explicit host given */
		if ((hp = gethostbyname(Sock_host)) == NULL) {
			leavex(1, "Unknown host");
			/* NOTREACHED */
		}
		test.sin_addr = *((struct in_addr *) hp->h_addr);
		msg = htons(C_TESTMSG());
		(void) sendto(test_socket, (char *) &msg, sizeof msg, 0,
				(struct sockaddr *) &test, DAEMON_SIZE);
	} else {

		if (!initial) {
			/* favor host of previous session by broadcasting to it first */
			test.sin_addr = Daemon.sin_addr;
			msg = htons(C_PLAYER); /* Must be playing! */
			(void) sendto(test_socket, (char *) &msg, sizeof msg, 0,
					(struct sockaddr *) &test, DAEMON_SIZE);
		}

# ifdef BROADCAST
		if (initial) {
			brdc = broadcast_vec((void *) &brdv);
		}

# ifdef SO_BROADCAST
		/* Sun's will broadcast even though this option can't be set */
		option = 1;
		if (setsockopt(test_socket, SOL_SOCKET, SO_BROADCAST,
						&option, sizeof option) < 0) {
			leave(1, "setsockopt broadcast");
			/* NOTREACHED */
		}
# endif

		/* send broadcast packets on all interfaces */
		msg = htons(C_TESTMSG());
		for (i = 0; i < brdc; i++) {
			test.sin_addr = brdv[i].sin_addr;
			if (sendto(test_socket, (char *) &msg, sizeof msg, 0,
							(struct sockaddr *) &test, DAEMON_SIZE) < 0) {
				leave(1, "sendto");
				/* NOTREACHED */
			}
		}
		test.sin_addr = local_address;
		if (sendto(test_socket, (char *) &msg, sizeof msg, 0,
						(struct sockaddr *) &test, DAEMON_SIZE) < 0) {
			leave(1, "sendto");
			/* NOTREACHED */
		}
# else /* !BROADCAST */
		/* loop thru all hosts on local net and send msg to them. */
		msg = htons(C_TESTMSG());
		local_net = inet_netof(local_address);
		sethostent(0); /* rewind host file */
		while (hp = gethostent()) {
			if (local_net == inet_netof(* ((struct in_addr *) hp->h_addr))) {
				test.sin_addr = * ((struct in_addr *) hp->h_addr);
				(void) sendto(test_socket, (char *) &msg, sizeof msg, 0,
						(struct sockaddr *) &test, DAEMON_SIZE);
			}
		}
# endif
	}

	namelen = DAEMON_SIZE;
	errno = 0;
	set[0].fd = test_socket;
	set[0].events = POLLIN;
	for (;;) {
		if (listc + 1 >= listmax) {
			listmax += 20;
			listv = (SOCKET *) realloc((char *) listv,
					listmax * sizeof(SOCKET));
			if (listv == NULL) {
				leave(1, "Out of memory!");
			}
		}

		if (poll(set, 1, 1000) == 1 &&
				recvfrom(test_socket, (char *) &port_num, sizeof(port_num),
						0, (struct sockaddr *) &listv[listc], &namelen) > 0) {
			/*
			 * Note that we do *not* convert from network to host
			 * order since the port number *should* be in network
			 * order:
			 */
			for (i = 0; (unsigned)i < listc; i += 1) {
				if (listv[listc].sin_addr.s_addr
						== listv[i].sin_addr.s_addr) {
					break;
				}
			}
			if ((unsigned)i == listc) {
				listv[listc++].sin_port = port_num;
			}
			continue;
		}

		if (errno != 0 && errno != EINTR) {
			leave(1, "poll/recvfrom");
			/* NOTREACHED */
		}

		/* terminate list with local address */
		listv[listc].sin_family = SOCK_FAMILY;
		listv[listc].sin_addr = local_address;
		listv[listc].sin_port = htons(0);

		(void) close(test_socket);
		initial = false;
		return listv;
	}

}

/**
 * Prints a list of drivers and let the user select one.
 * @param[in] do_startup If it is true the user can select a driver, otherwise no.
 * [PSR]
 */
void find_driver(bool do_startup) {
	SOCKET *hosts;

	hosts = list_drivers();
	if (hosts[0].sin_port != htons(0)) { /* hosts[0] is different from the local address. [PSR] */
		int i, c;

		if (hosts[1].sin_port == htons(0)) {
			Daemon = hosts[0];
			return;
		}
		/* go thru list and return host that matches daemon */
		clear_the_screen();
# ifdef USE_CURSES
		move(1, 0);
# else
		mvcur(cur_row, cur_col, 1, 0);
		cur_row = 1;
		cur_col = 0;
# endif
		put_str("Pick one:");
		for (i = 0; i < HEIGHT - 4 && hosts[i].sin_port != htons(0);
				i += 1) {
			struct hostent *hp;
			char buf[80];

# ifdef USE_CURSES
			move(3 + i, 0);
# else
			mvcur(cur_row, cur_col, 3 + i, 0);
			cur_row = 3 + i;
			cur_col = 0;
# endif
			hp = gethostbyaddr((char *) &hosts[i].sin_addr,
					sizeof hosts[i].sin_addr, AF_INET);
			(void) sprintf(buf, "%8c    %.64s", 'a' + i,
					hp != NULL ? hp->h_name
					: inet_ntoa(hosts->sin_addr));
			put_str(buf);
		}
# ifdef USE_CURSES
		move(4 + i, 0);
# else
		mvcur(cur_row, cur_col, 4 + i, 0);
		cur_row = 4 + i;
		cur_col = 0;
# endif
		put_str("Enter letter: ");
		refresh();
		while (!islower(c = getchar()) || (c -= 'a') >= i) {
			beep();
			refresh();
		}
		Daemon = hosts[c];
		clear_the_screen();
		return;
	}
	if (!do_startup) {
		return;
	}

	start_driver();
	sleep(2);
	find_driver(false);
}
#ifdef INTERNET
/**
 * Connects to the host and prints all the received informations.
 * @param[in] host The socket that identifies the host.
 * [PSR]
 */
void dump_scores(SOCKET host) {
	struct hostent *hp;
	int s;
	char buf[BUFSIZ];
	int cnt;

	hp = gethostbyaddr((char *) &host.sin_addr, sizeof host.sin_addr,
			AF_INET);
	printf("\n%s:\n", hp != NULL ? hp->h_name : inet_ntoa(host.sin_addr));
	fflush(stdout);

	s = socket(SOCK_FAMILY, SOCK_STREAM, 0);
	if (s < 0) {
		err(1, "socket");
	}
	if (connect(s, (struct sockaddr *) &host, sizeof host) < 0) {
		err(1, "connect");
	}
	while ((cnt = read(s, buf, BUFSIZ)) > 0) {
		dbg_write(fileno(stdout), buf, cnt);
	}
	(void) close(s);
}
# endif
# endif

/**
 * Manages the game connection.
 * [PSR]
 */
void start_driver() {
	int procid;

# ifdef MONITOR
	if (Am_monitor) {
		leavex(1, "No one playing.");
		/* NOTREACHED */
	}
# endif

# ifdef INTERNET
	if (Sock_host != NULL) {
		sleep(3);
		return;
	}
# endif

# ifdef USE_CURSES
	move(HEIGHT, 0);
# else
	mvcur(cur_row, cur_col, HEIGHT, 0);
	cur_row = HEIGHT;
	cur_col = 0;
# endif
	put_str("Starting...");
	refresh();
	procid = fork();
	if (procid == -1) {
		leave(1, "fork failed.");
	}
	if (procid == 0) {
		(void) signal(SIGINT, SIG_IGN);
# ifndef INTERNET
		(void) close(Socket);
# else
		if (use_port == NULL)
# endif
		execl(Driver, "HUNT", (char *) NULL);
# ifdef INTERNET
		else
		execl(Driver, "HUNT", "-p", use_port, (char *) NULL);
# endif
		/* only get here if exec failed */
		(void) kill(getppid(), SIGUSR1); /* tell mom */
		_exit(1);
	}
# ifdef USE_CURSES
	move(HEIGHT, 0);
# else
	mvcur(cur_row, cur_col, HEIGHT, 0);
	cur_row = HEIGHT;
	cur_col = 0;
# endif
	put_str("Connecting...");
	refresh();
}

/**
 * We had a bad connection.
 * For the moment we assume that this means the game is full.
 */
void bad_con() {
	leavex(1, "The game is full.  Sorry.");
	/* NOTREACHED */
}

/**
 * Version number mismatch.
 */
void bad_ver() {
	leavex(1, "Version number mismatch. No go.");
	/* NOTREACHED */
}

/**
 * Handle a terminate signal.
 */SIGNAL_TYPE sigterm(int dummy __attribute__((__unused__))) {
	leavex(0, (char *) NULL);
	/* NOTREACHED */
}

/**
 * Handle a usr1 signal.
 */SIGNAL_TYPE sigusr1(int dummy __attribute__((__unused__))) {
	leavex(1, "Unable to start driver.  Try again.");
	/* NOTREACHED */
}

# ifdef INTERNET
/**
 * Handle an alarm signal.
 */
SIGNAL_TYPE sigalrm(int dummy __attribute__((__unused__))) {
	return;
}
# endif

/**
 * Remove a '\n' at the end of a string if there is one.
 * @param[in] s The string we want to remove the '\n' at the end.
 */
void rmnl(char *s) {
	char *cp;

	cp = strrchr(s, '\n');
	if (cp != NULL) {
		*cp = '\0';
	}
}

/**
 * Handle a interrupt signal.
 */SIGNAL_TYPE intr(int dummy __attribute__((__unused__))) {
	int ch;
	int explained;
	int y, x;

	(void) signal(SIGINT, SIG_IGN);
# ifdef USE_CURSES
	getyx(stdscr, y, x);
	move(HEIGHT, 0);
# else
	y = cur_row;
	x = cur_col;
	mvcur(cur_row, cur_col, HEIGHT, 0);
	cur_row = HEIGHT;
	cur_col = 0;
# endif
	put_str("Really quit? ");
	clear_eol();
	refresh();
	explained = false;
	for (;;) {
		ch = getchar();
		if (isupper(ch)) {
			ch = tolower(ch);
		}
		if (ch == 'y') {
			if (Socket != 0) {
				dbg_write(Socket, "q", 1);
				(void) close(Socket);
			}
			leavex(0, (char *) NULL);
		} else if (ch == 'n') {
			(void) signal(SIGINT, intr);
# ifdef USE_CURSES
			move(y, x);
# else
			mvcur(cur_row, cur_col, y, x);
			cur_row = y;
			cur_col = x;
# endif
			refresh();
			return;
		}
		if (!explained) {
			put_str("(Yes or No) ");
			refresh();
			explained = true;
		}
		beep();
		refresh();
	}
}

/**
 * Verifies configuration entries.
 * [PSR]
 */
void fincurs() {
	if (in_visual) {
# ifdef USE_CURSES
		move(HEIGHT, 0);
		refresh();
		endwin();
# else /* !USE_CURSES */
		mvcur(cur_row, cur_col, HEIGHT, 0);
		(void) fflush(stdout); /* flush in case VE changes pages */
# if defined(BSD_RELEASE) && BSD_RELEASE >= 44
		tcsetattr(0, TCSADRAIN, &__orig_termios);
# else
		resetty();
# endif
#if defined(VE) && defined(TE)
		_puts(VE);
		_puts(TE);
#else
		fprintf(stderr,
				"Missing necessary configuration entries.\nHunt will quit.\n");
		exit(-1);
#endif
# endif /* !USE_CURSES */
	}
}

/**
 * Leave the game somewhat gracefully, restoring all current tty stats.
 * @param[in] eval The error status.
 * @param[in] mesg The error format.
 */
void leave(int eval, const char *mesg) {
	int serrno = errno;
	fincurs();
	errno = serrno;
	/*
	 * Introduced errno parameter in err() in order to display it on standard error.
	 * [PSR]
	 */
	err(eval, (mesg ? mesg : ""), errno);
}

/**
 * Leave the game somewhat gracefully, restoring all current tty stats.
 * @param[in] eval The error status.
 * @param[in] mesg The error format.
 */
void leavex(int eval, const char *mesg) {
	fincurs();
	/*
	 * Introduced a NULL parameter in errx() since the routine shows a generic error message.
	 * [PSR]
	 */
	errx(eval, (mesg ? mesg : ""), NULL);
}

#if !defined(USE_CURSES) && defined(SIGTSTP)
/**
 * Handle stop and start signals
 */SIGNAL_TYPE tstp(int dummy) {
# if BSD_RELEASE < 44
	static struct sgttyb tty;
# endif
	int y, x;

	y = cur_row;
	x = cur_col;
	mvcur(cur_row, cur_col, HEIGHT, 0);
	cur_row = HEIGHT;
	cur_col = 0;
# if !defined(BSD_RELEASE) || BSD_RELEASE < 44
	tty = _tty;
# endif
#if defined(VE) && defined(TE)
	_puts(VE);
	_puts(TE);
#else
	errx(1, "Missing necessary configuration entries.\nHunt will quit.\n");
#endif
	(void) fflush(stdout);
# if defined(BSD_RELEASE) && BSD_RELEASE >= 44
	tcsetattr(0, TCSADRAIN, &__orig_termios);
# else
	resetty();
# endif
	(void) kill(getpid(), SIGSTOP);
	(void) signal(SIGTSTP, tstp);
# if defined(BSD_RELEASE) && BSD_RELEASE >= 44
	tcsetattr(0, TCSADRAIN, &saved_tty);
# else
	_tty = tty;
#ifdef TIOCSTEP
	ioctl(_tty_ch, TIOCSETP, &_tty);
#else
	errx(1, "Missing necessary configuration entries.\nHunt will quit.\n");
#endif
# endif
#if defined(TI) && defined(VS)
	_puts(TI);
	_puts(VS);
#else
	errx(1, "Missing necessary configuration entries.\nHunt will quit.\n");
#endif
	cur_row = y;
	cur_col = x;
#ifdef CM
	_puts(tgoto(CM, cur_row, cur_col));
#endif
	redraw_screen();
	(void) fflush(stdout);
}
#endif /* !defined(USE_CURSES) && defined(SIGTSTP) */

# if defined(BSD_RELEASE) && BSD_RELEASE < 43
char * strpbrk(char *s,char *brk) {
	char *p;
	c;

	while (c = *s) {
		for (p = brk; *p; p++)
		if (c == *p)
		return (s);
		s++;
	}
	return (0);
}
# endif

/**
 * Parses configuration file and initializes game environment.
 * This function retrives the configuration file and parses it to initialize the game environment.
 * In case of file unavailability tries to retrive configuration options from HUNT environment
 * variable using var_env_init() function.
 * @param[in] enter_status_in Status used if no status modifier is set in configuration file.
 * \return Configured enter status.
 * [PSR]
 */
long env_init(long enter_status_in) {

	long enter_status = enter_status_in;

	/*
	 * Variables supporting configuration file parsing.
	 * [PSR]
	 */
	FILE* config; /* Points to configuration file. [PSR] */
	FILE* c; /* Points to configuration file. Used for char counting. [PSR] */
	bool opened_c;
	long input_len; /* Input row length. [PSR] */
	char* input_row; /* A row from configuration file. [PSR] */
	char* equal; /* Points to the position of '=' into input_row. [PSR] */
	char* input_value; /* Value associated to a configuration tag. [PSR] */
	long tag_len; /* Tag field length. [PSR] */
	char* read;

	int i;

	/*
	 * Generates a map for extended ASCII conversion.
	 * [PSR]
	 */
	for (i = 0; i < 256; i++) {
		map_key[i] = (char) i;
	}

	if (!(config = fopen(CONFIGURATION_FILE, "r"))) {
		return var_env_init(enter_status);
	} else {
		if ((c = fopen(CONFIGURATION_FILE, "r"))) {
			opened_c = false;
			input_len = EMERGENCY_BUFFER_LENGTH;
		} else {
			input_len = fchars_in_line(c);
			opened_c = true;
		}
		input_row = malloc(sizeof(char) * (input_len + 1));
		read = fgets(input_row, input_len + 1, config);
		while (!feof(config)) {
			/*
			 * Avoids infinite loop in case of fgets failure.
			 * [PSR]
			 */
			if (read == NULL) {
				break;
			}
			equal = strpbrk(input_row, "=");
			input_value = (equal != NULL) ? equal + 1 : input_row + input_len;
			tag_len = input_value - input_row;
			if (strncmp(input_row, "cloak", tag_len) == 0) {
				enter_status = Q_CLOAK;
			} else if (strncmp(input_row, "scan", tag_len) == 0) {
				enter_status = Q_SCAN;
			} else if (strncmp(input_row, "fly", tag_len) == 0) {
				enter_status = Q_FLY;
			} else if (strncmp(input_row, "nobeep", tag_len) == 0) {
				no_beep = true;
			} else if (strncmp(input_row, "name=", tag_len) == 0) {
				strncpy(name, input_value, NAMELEN);
			}
# ifdef INTERNET
			else if (strncmp(input_row, "port=", tag_len) == 0) {
				use_port = input_value;
				Test_port = atoi(use_port);
			}
			else if (strncmp(input_row, "host=", tag_len) == 0) {
				Sock_host = input_value;
			}
			else if (strncmp(input_row, "message=", tag_len) == 0) {
				Send_message = input_value;
			}
# endif
			else if (strncmp(input_row, "team=", tag_len) == 0) {
				team = *(input_value);
				if (!isdigit((unsigned char)team))
					team = ' ';
			} /* must be last option */
			else if (strncmp(input_row, "mapkey=", tag_len) == 0) {
				for (i = 0; input_value[i] != '\0'; i += 2) {
					map_key[(unsigned int) input_value[i]] = input_value[i + 1];
					if (input_value[i + 1] == '\0') {
						break;
					}
				}
				break;
			} else {
				printf("unknown option %s\n", input_row);
			}
			free(input_row);
			input_row = NULL;
			if (opened_c) {
				input_len = fchars_in_line(c);
			} else {
				input_len = EMERGENCY_BUFFER_LENGTH;
			}
			input_row = malloc(sizeof(char) * (input_len + 1));
			read = fgets(input_row, input_len + 1, config);
		}
		free(input_row);
		input_row = NULL;
		if (opened_c) {
			/*
			 * Hard-wired FILE* c setting to NULL after file closure.
			 * [PSR]
			 */
			fclose(c);
			c = NULL;
		}
		/*
		 * Hard-wired FILE* config setting to NULL after file closure.
		 * [PSR]
		 */
		fclose(config);
		config = NULL;
	}
	return enter_status;
}

/**
 * Imports game settings from environment.
 * It is invoked only in case of configuration file unavailability.
 * @param[in] enter_status_in Status used if no status modifier is set in configuration environment variable..
 * \return Configured enter status.
 * [PSR]
 */
long var_env_init(long enter_status_in) {

	long enter_status = enter_status_in; /* Hosts the value to return. [PSR] */

	int i;

	char *envp; /* This string contains setting options. [PSR] */
	char *envname; /* Temporarely stores configured player name. [PSR] */
	char *s;

	/*
	 * Generates a map for extended ASCII conversion.
	 * [PSR]
	 */
	for (i = 0; i < 256; i++) {
		map_key[i] = (char) i;
	}

	envname = NULL; /* Verified null pointer safety. [PSR] */

	if ((envp = getenv("HUNT")) != NULL) {
		while ((s = strpbrk(envp, "=,")) != NULL) {
			if (strncmp(envp, "cloak,", s - envp + 1) == 0) { /* Compare characters from the start of the conf string not yet analyzed till the first '='. [PSR] */
				enter_status = Q_CLOAK;
				envp = s + 1; /* Deletes characters already considered. [PSR] */
			} else if (strncmp(envp, "scan,", s - envp + 1) == 0) {
				enter_status = Q_SCAN;
				envp = s + 1;
			} else if (strncmp(envp, "fly,", s - envp + 1) == 0) {
				enter_status = Q_FLY;
				envp = s + 1;
			} else if (strncmp(envp, "nobeep,", s - envp + 1) == 0) {
				no_beep = true;
				envp = s + 1;
			} else if (strncmp(envp, "name=", s - envp + 1) == 0) {
				envname = s + 1;
				if ((s = strchr(envp, ',')) == NULL) {
					*envp = '\0';
					strncpy(name, envname, NAMELEN);
					break;
				}
				*s = '\0';
				strncpy(name, envname, NAMELEN);
				envp = s + 1;
			}
# ifdef INTERNET
			else if (strncmp(envp, "port=", s - envp + 1) == 0) {
				use_port = s + 1;
				Test_port = atoi(use_port);
				if ((s = strchr(envp, ',')) == NULL) {
					*envp = '\0';
					break;
				}
				*s = '\0';
				envp = s + 1;
			}
			else if (strncmp(envp, "host=", s - envp + 1) == 0) {
				Sock_host = s + 1;
				if ((s = strchr(envp, ',')) == NULL) {
					*envp = '\0';
					break;
				}
				*s = '\0';
				envp = s + 1;
			}
			else if (strncmp(envp, "message=", s - envp + 1) == 0) {
				Send_message = s + 1;
				if ((s = strchr(envp, ',')) == NULL) {
					*envp = '\0';
					break;
				}
				*s = '\0';
				envp = s + 1;
			}
# endif
			else if (strncmp(envp, "team=", s - envp + 1) == 0) {
				team = *(s + 1);
				if (!isdigit((unsigned char)team))
					team = ' ';
				if ((s = strchr(envp, ',')) == NULL) {
					*envp = '\0';
					break;
				}
				*s = '\0';
				envp = s + 1;
			} /* must be last option */
			else if (strncmp(envp, "mapkey=", s - envp + 1) == 0) {
				for (s = s + 1; *s != '\0'; s += 2) {
					map_key[(unsigned int) *s] = *(s + 1);
					if (*(s + 1) == '\0') {
						break;
					}
				}
				*envp = '\0';
				break;
			} else {
				*s = '\0';
				printf("unknown option %s\n", envp);
				if ((s = strchr(envp, ',')) == NULL) {
					*envp = '\0';
					break;
				}
				envp = s + 1;
			}
		}
		if (*envp != '\0') {
			if (envname == NULL) {
				strncpy(name, envp, NAMELEN);
			} else {
				printf("unknown option %s\n", envp);
			}
		}
	}
	return enter_status;
}

/**
 * Counts the number of characters of all the lines in a file.
 * @param[in] f The file.
 * \return The counter.
 * [PSR]
 */
long fchars_in_line(FILE* f) {
	long counter = 0;
	char in;
	int read;
	read = fscanf(f, "%c", &in);
	while (!feof(f)) {
		/*
		 * Avoids infinite loop in case of fscanf failure.
		 * [PSR]
		 */
		if (!read) {
			break;
		}
		if (in == '\n') {
			break;
		} else {
			counter++;
			read = fscanf(f, "%c", &in);
		}
	}
	return counter;
}

/**
 * Fills the name and team fields.
 * [PSR]
 */
void fill_in_blanks() {
	int i;
	char *cp;
	bool must_continue;

	must_continue = FALSE;
	while (TRUE) {
		if (name[0] != '\0') {
			printf("Entering as '%s'", name);
			if (team != ' ') {
				printf(" on team %c.\n", team);
			} else {
				putchar('\n');
			}
		} else {
			printf("Enter your code name: ");
			if (fgets(name, NAMELEN, stdin) == NULL) {
				exit(1);
			}
		}
		rmnl(name);
		if (name[0] == '\0') {
			name[0] = '\0';
			printf("You have to have a code name!\n");
			continue;
		}
		for (cp = name; *cp != '\0'; cp++) {
			if (!isprint((unsigned char)*cp)) {
				name[0] = '\0';
				printf("Illegal character in your code name.\n");
				must_continue = TRUE;
				break;
			}
		}
		if (must_continue) {
			continue;
		} else {
			break;
		}
	}

	if (team == ' ') {
		printf("Enter your team (0-9 or nothing): ");
		i = getchar();
		if (isdigit(i)) {
			team = i;
		}
		while (i != '\n' && i != EOF) {
			i = getchar();
		}
	}
}
