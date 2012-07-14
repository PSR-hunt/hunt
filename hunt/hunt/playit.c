/*	$NetBSD: playit.c,v 1.8 2004/01/27 20:30:29 jsm Exp $	*/
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
__RCSID("$NetBSD: playit.c,v 1.8 2004/01/27 20:30:29 jsm Exp $");
#endif /* not lint */

# include	<sys/file.h>
# include	<sys/poll.h>
# include	<err.h>
/* # include	<errno.h> already present in hunt.h. [PSR] */
# include	<ncurses.h> /* Edited from curses.h. [PSR] */
# include	<ctype.h>
# include	<signal.h>
# include	<sys/time.h>
# if defined(HPUX) || (defined(BSD_RELEASE) && BSD_RELEASE >= 44)
# include	<termios.h>
# include	<unistd.h>
# endif

# ifndef FREAD
# define	FREAD	1 /**< Never used. [PSR] */
# endif

# if !defined(USE_CURSES) || !defined(TERMINFO)
# define	beep()		(void) putchar(CTRL('G')) /**< Defines beep() that writes the character G, cast to an unsigned char, to stream. [PSR] */
# endif
# if !defined(USE_CURSES)
# undef		refresh
# define	refresh()	(void) fflush(stdout); /**< Defines refresh() that flushes stdout stream. [PSR] */
# endif
# ifdef USE_CURSES
# define	clear_eol()	clrtoeol()
# define	put_ch		addch
# define	put_str		addstr
# endif

static int nchar_send; /**< Number of sent characters. [PSR] */
# ifndef USE_CURSES
char screen[SCREEN_HEIGHT][SCREEN_WIDTH2]; /**< The game area. [PSR] */
char blanks[SCREEN_WIDTH]; /**< Contains elements that will be copied. [PSR] */
int cur_row; /**< Indicates the current row. [PSR] */
int cur_col; /**< Indicates the current column. [PSR] */
# endif
# ifdef OTTO
int Otto_count;
int Otto_mode;
static int otto_y, otto_x;
static char otto_face;
# endif

# define	MAX_SEND	5 /**< Maximum number of characters to send. [PSR] */
# define	STDIN		0 /**< File descriptor for stdin. [PSR] */

/*
 * ibuf is the input buffer used for the stream from the driver.
 * It is small because we do not check for user input when there
 * are characters in the input buffer.
 */
static int icnt = 0; /**< Number of read characters. [PSR] */
static unsigned char ibuf[256]; /**< The read buffer (ibuf is the input buffer used for the stream from the driver.). [PSR] */
static unsigned char *iptr = ibuf; /**< ibuf first position pointer. [PSR] */

#define	GETCHR()	(--icnt < 0 ? getchr() : *iptr++) /**< Reads from input if there are no things to read in ibuf, reads the first character otherwise. [PSR] */

/*Seems to be unused [PSR]
 #if !defined(BSD_RELEASE) || BSD_RELEASE < 44
 extern int _putchar();
 #endif
 */

static unsigned char getchr(void);
static void send_stuff(void);

/**
 * Play a given game, handling all the curses commands from
 * the driver.
 */
void playit() {
	int ch;
	int y, x;

	errno = 0;
# ifdef OTTO
	Otto_count = 0;
# endif
	nchar_send = MAX_SEND;
	while ((ch = GETCHR()) != EOF) {
# ifdef DEBUG
		fputc(ch, stderr);
# endif
		switch (ch & 0377) {
		case MOVE:
			y = GETCHR();
			x = GETCHR();
# ifdef USE_CURSES
			move(y, x);
# else
			mvcur(cur_row, cur_col, y, x);
			cur_row = y;
			cur_col = x;
# endif
			break;
		case ADDCH:
			ch = GETCHR();
# ifdef OTTO
			switch (ch) {

				case '<':
				case '>':
				case '^':
				case 'v':
				otto_face = ch;
# ifdef USE_CURSES
				getyx(stdscr, otto_y, otto_x);
# else
				otto_y = cur_row;
				otto_x = cur_col;
# endif
				break;
			}
# endif
			put_ch(ch);
			break;
		case CLRTOEOL:
			clear_eol();
			break;
		case CLEAR:
			clear_the_screen();
			break;
		case REFRESH:
			refresh()
			;
			break;
		case REDRAW:
			redraw_screen();
			refresh()
			;
			break;
		case ENDWIN:
			refresh()
			;
			if ((ch = GETCHR()) == LAST_PLAYER)
				Last_player = true;
			ch = EOF;
//			safe_close(main_socket); Socket termination delegated to main routine.
			return;
		case BELL:
			beep();
			break;
		case READY:
			refresh()
			;
			if (nchar_send < 0)
# if defined(HPUX) || (defined(BSD_RELEASE) && BSD_RELEASE >= 44)
				tcflush(STDIN, TCIFLUSH);
# else
# ifndef TCFLSH
				(void) ioctl(STDIN, TIOCFLUSH, &in);
# else
				(void) ioctl(STDIN, TCFLSH, 0);
# endif
# endif
			nchar_send = MAX_SEND;
# ifndef OTTO
			(void) GETCHR();
# else
			Otto_count -= (GETCHR() & 0xff);
			if (!am_monitor) {
# ifdef DEBUG
				fputc('0' + Otto_count, stderr);
# endif
				if (Otto_count == 0 && Otto_mode) {
					otto(otto_y, otto_x, otto_face);
				}
			}
# endif
			break;
		default:
# ifdef OTTO
			switch (ch) {

				case '<':
				case '>':
				case '^':
				case 'v':
				otto_face = ch;
# ifdef USE_CURSES
				getyx(stdscr, otto_y, otto_x);
# else
				otto_y = cur_row;
				otto_x = cur_col;
# endif
				break;
			}
# endif
			put_ch(ch);
			break;
		}
	}
//	safe_close(main_socket); Socket termination delegated to main routine
}

/**
 *  Grab input and pass it along to the driver
 *	Return any characters from the driver
 *	When this routine is called by GETCHR, we already know there are
 *	no characters in the input buffer.
 *	\return A pointer to the next char.
 */
static unsigned char getchr() {
	struct pollfd set[2];
	int nfds;

	set[0].fd = main_socket;
	set[0].events = POLLIN;
	set[1].fd = STDIN;
	set[1].events = POLLIN;

	while (1) {
		do {
			errno = 0;
			nfds = poll(set, 2, INFTIM);
		} while (nfds <= 0 && errno == EINTR);

		if (set[1].revents && POLLIN) {
			send_stuff();
		}
		if (!(set[0].revents & POLLIN)) {
			continue;
		}
		icnt = read(main_socket, ibuf, sizeof ibuf);
		if (icnt < 0) {
			bad_con();
			/* NOTREACHED */
		}
		if (icnt == 0) {
			continue;
		}
		iptr = ibuf;
		icnt--;
		return *iptr++;
	}
	/* This point should never be reached. [PSR] */
	return -1;
}

/**
 * Send standard input characters to the driver
 */
static void send_stuff() {
	int count;
	char *sp, *nsp;
	static char inp[sizeof gen_buf];

	count = read(STDIN, gen_buf, sizeof gen_buf);
	if (count <= 0) {
		return;
	}
	if (nchar_send <= 0 && !no_beep) {
		safe_write(1, "\7", 1);
		/* CTRL('G') */
		return;
	}

	/*
	 * look for 'q'uit commands; if we find one,
	 * confirm it.  If it is not confirmed, strip
	 * it out of the input
	 */
	gen_buf[count] = '\0';
	nsp = inp;
	for (sp = gen_buf; *sp != '\0'; sp++) {
		if ((*nsp = map_key[(int) *sp]) == 'q') {
			intr(0);
		} else {
			nsp++;
		}
	}
	count = nsp - inp;
	if (count) {
# ifdef OTTO
		Otto_count += count;
# endif
		nchar_send -= count;
		if (nchar_send < 0) {
			count += nchar_send;
		}
		write_and_push(main_socket, inp, count);
	}
}

/**
 * Handle the end of the game when the player dies.
 * @param[in] old_status An integer that represents the status of a player.
 * \return The status of the player re-entering the game.
 */
int quit(int old_status) {
	int explain, ch;
	/*
	 * Complete version. Deactivated due to authentication protocol conflict. [PSR]
	 */
//	int second_ch;

	if (Last_player) {
		return Q_QUIT;
	}
# ifdef OTTO
	if (Otto_mode) {
		return Q_CLOAK;
	}
# endif
# ifdef USE_CURSES
	move(HEIGHT, 0);
# else
	mvcur(cur_row, cur_col, HEIGHT, 0);
	cur_row = HEIGHT;
	cur_col = 0;
# endif
	/*
	 * Complete version. Deactivated due to authentication protocol conflict. [PSR]
	 */
//	put_str("Re-enter game [ynwo]? ");
	/*
	 * Short version. Authentication protocol compliancy. [PSR]
	 */
	put_str("Start or connect to a new game [yn]? ");

	clear_eol();
	explain = false;
	for (;;) {
		refresh()
		;
		if (isupper(ch = getchar())) {
			ch = tolower(ch);
		}

		if (ch == 'y') {
			return old_status; /* The player wants to re-enter the game with his last status. [PSR] */
		}
		/*
		 * Complete version. Deactivated due to authentication protocol conflict. [PSR]
		 */

//		else if (ch == 'o'){
//			break; /* The player wants to re-enter the game but wants to change the play mode. [PSR] */
//		}
		else if (ch == 'n') {
			/*
			 * Complete version commented. Deactivated due to authentication protocol conflict. [PSR]
			 */
//# ifndef INTERNET
			return Q_QUIT; /* The player wants to exit. [PSR] */
//# else
//# ifdef USE_CURSES
//			move(HEIGHT, 0);
//# else
//			mvcur(cur_row, cur_col, HEIGHT, 0);
//			cur_row = HEIGHT;
//			cur_col = 0;
//# endif
//			put_str("Write a parting message [yn]? ");
//			clear_eol();
//			refresh();
//			for (;;) {
//				if (isupper(second_ch = getchar())) {
//					second_ch = tolower(ch);
//				}
//				if (second_ch == 'y' || second_ch == 'n') {
//					break;
//				}
//			}
//# endif
		}

		/*
		 * Complete version. Deactivated due to authentication protocol conflict. [PSR]
		 */
//# ifdef INTERNET
//		if( ch == 'n' && second_ch == 'n') {
//			return Q_QUIT; /* The player wants to leave the game without sending a message. [PSR] */
//		} else if ((ch == 'n' && second_ch == 'y') || ch == 'w') {
//			static char buf[WIDTH + WIDTH % 2];
//			char *cp, c;
//
//			c = ch; /* save how we got here */
//# ifdef USE_CURSES
//			move(HEIGHT, 0);
//# else
//			mvcur(cur_row, cur_col, HEIGHT, 0);
//			cur_row = HEIGHT;
//			cur_col = 0;
//# endif
//			put_str("Message: ");
//			clear_eol();
//			refresh();
//			cp = buf;
//			for (;;) { /* echoes user input while writing a message */
//				refresh();
//				if ((ch = getchar()) == '\n' || ch == '\r') {
//					break;
//				}
//# if defined(TERMINFO) || BSD_RELEASE >= 44
//				if (ch == erasechar())
//# else
//				if (ch == _tty.sg_erase)
//# endif
//				{
//					if (cp > buf) {
//# ifdef USE_CURSES
//						int y, x;
//						getyx(stdscr, y, x);
//						move(y, x - 1);
//# else
//						mvcur(cur_row, cur_col, cur_row,
//								cur_col - 1);
//						cur_col -= 1;
//# endif
//						cp -= 1;
//						clear_eol();
//					}
//					continue;
//				}
//# if defined(TERMINFO) || BSD_RELEASE >= 44
//				else if (ch == killchar())
//# else
//				else if (ch == _tty.sg_kill)
//# endif
//				{
//# ifdef USE_CURSES
//					int y, x;
//					getyx(stdscr, y, x);
//					move(y, x - (cp - buf));
//# else
//					mvcur(cur_row, cur_col, cur_row,
//							cur_col - (cp - buf));
//					cur_col -= cp - buf;
//# endif
//					cp = buf;
//					clear_eol();
//					continue;
//				} else if (!isprint(ch)) {
//					beep();
//					continue;
//				}
//				put_ch(ch);
//				*cp++ = ch;
//				if (cp + 1 >= buf + sizeof buf) {
//					break;
//				}
//			}
//			*cp = '\0';
//			send_message = buf; /* stores the message into outgoing message */
//			return (c == 'w') ? old_status : Q_MESSAGE;
//		}
//# endif
		beep();
		if (!explain) {
			/*
			 * Complete version. Deactivated due to authentication protocol conflict. [PSR]
			 */
//			put_str("(Yes, No, Write message, or Options) ");
			/*
			 * Simple version. Authentication protocol compliancy. [PSR]
			 */
			put_str("(Yes or No)");
			explain = true;
		}
	}

# ifdef USE_CURSES
	move(HEIGHT, 0);
# else
	mvcur(cur_row, cur_col, HEIGHT, 0);
	cur_row = HEIGHT;
	cur_col = 0;
# endif
# ifdef FLY
	put_str("Scan, Cloak, Flying, or Quit? ");
# else
	put_str("Scan, Cloak, or Quit? ");
# endif
	clear_eol();
	refresh()
	;
	explain = false;
	for (;;) {
		if (isupper(ch = getchar())) {
			ch = tolower(ch);
		}
		if (ch == 's') {
			return Q_SCAN;
		} else if (ch == 'c') {
			return Q_CLOAK;
		}
# ifdef FLY
		else if (ch == 'f') {
			return Q_FLY;
		}
# endif
		else if (ch == 'q') {
			return Q_QUIT;
		}
		beep();
		if (!explain) {
# ifdef FLY
			put_str("[SCFQ] ");
# else
			put_str("[SCQ] ");
# endif
			explain = true;
		}
		refresh()
		;
	}
	/* This point should never be reached [PSR] */
	return -1;
}

# ifndef USE_CURSES
/**
 * Puts a given character in the point of the screen at the current cursor position.
 * @param[in] ch A character.
 * [PSR]
 */
void put_ch(char ch) {
	if (!isprint(ch)) {
		fprintf(stderr, "r,c,ch: %d,%d,%d", cur_row, cur_col, ch);
		return;
	}
	screen[cur_row][cur_col] = ch;
	putchar(ch);
	if (++cur_col >= COLS) {
#if defined(AM) && defined(XN)
		if (!AM || XN) {
			putchar('\n');
		}
#else
		errx(1, "Missing necessary configuration entries.\nHunt will quit.\n");
#endif
		cur_col = 0;
		if (++cur_row >= LINES) {
			cur_row = LINES;
		}
	}
}

/**
 * Puts a given sequence of characters on the screen.
 * @param[in] s A sequence of characters.
 * [PSR]
 */
void put_str(const char *s) {
	while (*s) {
		put_ch(*s++);
	}
}
# endif

/**
 * Cleans the screen.
 * [PSR]
 */
void clear_the_screen() {
# ifdef USE_CURSES
	clear();
	move(0, 0);
	refresh();
# else
	int i;

	if (blanks[0] == '\0')
		for (i = 0; i < SCREEN_WIDTH; i++) {
			blanks[i] = ' ';
		}
#ifdef CL
	if (CL != NULL) {
#if !defined(BSD_RELEASE) || BSD_RELEASE < 44
		tputs(CL, LINES, _putchar);
#else
		tputs(CL, LINES, __cputchar);
#endif
		for (i = 0; i < SCREEN_HEIGHT; i++) {
			memcpy(screen[i], blanks, SCREEN_WIDTH);
		}
	} else
#else
	errx(1, "Missing necessary configuration entries.\nHunt will quit.\n");
#endif
	{
		for (i = 0; i < SCREEN_HEIGHT; i++) {
			mvcur(cur_row, cur_col, i, 0);
			cur_row = i;
			cur_col = 0;
			clear_eol();
		}
		mvcur(cur_row, cur_col, 0, 0);
	}
	cur_row = cur_col = 0;
#endif
}

#ifndef USE_CURSES
/**
 * Cleans the end of the line.
 */
void clear_eol() {
#ifdef CE
	if (CE != NULL)
#if !defined(BSD_RELEASE) || BSD_RELEASE < 44
	tputs(CE, 1, _putchar);
#else
	tputs(CE, 1, __cputchar);
#endif
	else {
		fwrite(blanks, sizeof(char), SCREEN_WIDTH - cur_col, stdout);
		if (COLS != SCREEN_WIDTH) {
			mvcur(cur_row, SCREEN_WIDTH, cur_row, cur_col);
		}
		else if (AM) {
			mvcur(cur_row + 1, 0, cur_row, cur_col);
		}
		else {
			mvcur(cur_row, SCREEN_WIDTH - 1, cur_row, cur_col);
		}
	}
	memcpy(&screen[cur_row][cur_col], blanks, SCREEN_WIDTH - cur_col);
#else
	errx(1, "Missing necessary configuration entries.\nHunt will quit.\n");
#endif
}
# endif

/**
 * Repaints the screen.
 * [PSR]
 */
void redraw_screen() {
# ifdef USE_CURSES
	clearok(stdscr, true);
	touchwin(stdscr);
# else
	int i;
# ifndef NOCURSES
	static int first = 1;

	if (first) {
		curscr = newwin(SCREEN_HEIGHT, SCREEN_WIDTH, 0, 0);
		if (curscr == NULL ) {
			errx(1, "Can't create curscr");
		}

		/*Uncomment only for BSD legacy compatibility [PSR]
		 # if !defined(BSD_RELEASE) || BSD_RELEASE < 44
		 for (i = 0; i < SCREEN_HEIGHT; i++)
		 curscr->_y[i] = screen[i];
		 * # endif
		 */
		first = 0;
	}
# if defined(BSD_RELEASE) && BSD_RELEASE >= 44
	for (i = 0; i < SCREEN_HEIGHT; i++) {
		int j;

		for (j = 0; j < SCREEN_WIDTH; j++) {
			curscr->lines[i]->line[j].ch = screen[i][j];
		}
	}
	curscr->cury = cur_row;
	curscr->curx = cur_col;
# else
	curscr->_cury = cur_row;
	curscr->_curx = cur_col;
# endif
	clearok(curscr, true);
	touchwin(curscr);
	wrefresh(curscr);
#else
	mvcur(cur_row, cur_col, 0, 0);
	for (i = 0; i < SCREEN_HEIGHT - 1; i++) {
		fwrite(screen[i], sizeof (char), SCREEN_WIDTH, stdout);
		if (COLS > SCREEN_WIDTH || (COLS == SCREEN_WIDTH && !AM)) {
			putchar('\n');
		}
	}
	fwrite(screen[SCREEN_HEIGHT - 1], sizeof (char), SCREEN_WIDTH - 1,
			stdout);
	mvcur(SCREEN_HEIGHT - 1, SCREEN_WIDTH - 1, cur_row, cur_col);
#endif
#endif
}

/**
 * Send a message to the driver and return
 */
void do_message() {
	u_int32_t version;

	if (read(main_socket, (char *) &version, LONGLEN) != LONGLEN) {
		bad_con();
		/* NOTREACHED */
	}
	if (ntohl(version) != HUNT_VERSION) {
		bad_ver();
		/* NOTREACHED */
	}
# ifdef INTERNET
	if (write(main_socket, send_message, strlen(send_message)) < 0) {
		bad_con();
		/* NOTREACHED */
	}
# endif
//	safe_close(main_socket); Socket termination delegated to main routine
}
