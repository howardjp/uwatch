/*
 * $jameshoward: uwatch.c,v 1.4 2000/07/07 03:32:14 howardjp Exp $
 *
 * Copyright (c) 1999
 *      Arbornet, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Arbornet, Inc., and
 *      its contributors.
 * 4. Neither the name of Arbornet, Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ARBORNET, INC. AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL ARBORNET, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* 	watch logins and logouts 

	1994: jared wrote me.
	jared guest  5042 Apr 19  1994 /usr/staff/jared/src/watch.c

	1996: craig fixed some problem, created another.
	root  wheel  4174 Aug 16  1996 /usr/local/src/watch.c

	1997: jor consolidated code, removing bugs and unused features
	      jp2 added features back in, what a hot dog
	      jor broke inline code into subroutines
	
	5/8/97: jp2 started using dates in the comments, better for
		keeping track of updates, added -r option to enable watch
		file, changed the default file to .watchrc ... and hey
		jor, those features I put in were both new and useful :P

        11/1/99: James Howard started cleaning up the code and fixing
                 some of the dumber things in it.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>


struct utsname lh;
int localh, port, where, t, quick, response;
char *watchfile;

#define MAXNAMES 150		/* note there's no table overflow checks */
#define MAXTTYS   64		/* a guess */

struct linerec { char name[10]; char line[20]; } 
       lines[MAXTTYS];		/* a record for each tty/line/port */

char bell, names[MAXNAMES][9];  /* table of names to watch for     */

int wtfd; 			/* wtmp file descriptor */

void notify(char *msg, struct utmp login) {
        char tmpname[65] = "\0", tmphost[65] = "\0", tmpline[65] = "\0", tmptime
[65] = "\0";

        strncpy(tmpname, login.ut_name, UT_NAMESIZE);
        strncpy(tmphost, login.ut_host, UT_HOSTSIZE);
        strncpy(tmpline, login.ut_line, UT_LINESIZE);

        strftime(tmptime, 64, "%B %d, %H:%M", localtime(&login.ut_time));
        printf("\a%s: %s", msg, tmpname);
        if(localh)
                printf("@%s", lh.nodename);
        if(port)
                printf("  Port: %s", tmpline);
        if(where)
                printf("  Host: %s", tmphost);
        if(t)
                printf("  Time: %s", tmptime);
        printf("\n");
}

void read_watchlist()
{	FILE *wl; char name[10]; int n, l; n = 0;
	wl = fopen(watchfile, "r");
	if (wl == NULL) return;
	while (fgets(name, 9, wl) != NULL) {
		l = strlen(name) - 1;
		if (name[l] == '\n') name[l] = '\0'; 
		/* better way to kill newline char? */
		/* I have a much better parser I'll add later -jp2 */
		strcpy(names[n++], name);
	}
	fclose(wl);
}

void usage() 
{

	fprintf(stderr, "\
usage: watch [-f file] [-lpqrtw] [login ...]\n\
\t-f\t--  set response file (-r implied)\n\
\t-l\t--  list localhost\n\
\t-p\t--  list port\n\
\t-q\t--  list only who is logged in and terminate\n\
\t-r\t--  use response file (defualts to ~/.watchrc)\n\
\t-t\t--  list login/logout time\n\
\t-?\t--  print this message\n");
	exit(0);
}


void processparams(argc,argv)
    int argc;
    char **argv;
{
	int ch, x;
	if (argc == 0) return;
	while((ch = getopt(argc, argv, "f:lpqrtw?")) != EOF) {
		switch(ch) {
		case 'f': 
			watchfile = optarg;
		case 'r':
			response = 1;
			break;
		case 'l':
			localh = 1;
			uname(&lh);
			break;
		case 'p':
			port = 1;
			break;
		case 'q':
			quick = 1;
			break;
		case 't':
			t = 1;
			break;
		case 'w':
			where = 1;
			break;
		case '?':
			usage();
		}
	}
	for (x = 1; x < argc; x++)
	{
		if (argv[x][0] != '-')
		{
			int n;
			for (n = 0; n < MAXNAMES; n++)
				if (names[n][0] == 0) break;
			strcpy(names[n], argv[x]);
		} else {
			if (argv[x][1] == 's') bell = 32;
		}
	}
	return;
}


int inlist (char *logname)
{	int n;
	if (logname[0]  == '\0') return 0;
	if (names[0][0] ==   0 ) return 1;

	for (n = 0 ; n < MAXNAMES ; n++)
		if (strcmp(logname,names[n]) == 0) return 1;
	return 0;
}


void read_utmp()
{	int fd, l; struct utmp u; 
	fd = open(_PATH_UTMP, O_RDONLY);
	if (fd == -1)	{ perror(_PATH_UTMP); exit(1); }
	while (read(fd, &u, sizeof (struct utmp)) == sizeof(struct utmp))
	    if (strcmp(u.ut_name, "") != 0)
	    {
		if (inlist(u.ut_name)) notify("In", u);
		for (l = 0; l < MAXTTYS; l++)
			if (strcmp(lines[l].name, "") == 0)
			{	strcpy(lines[l].name,u.ut_name);
				strcpy(lines[l].line,u.ut_line);
				break;
			}
	    }
	close(fd);
}

void open_wtmp()
{	

	wtfd = open(_PATH_WTMP, O_RDONLY);
	if (wtfd == -1) { perror(_PATH_WTMP); exit(1); }	
	lseek(wtfd, 0, SEEK_END); 
}

void tail_wtmp()
{		int l; /*which line? */
		struct utmp uin;
		if (read(wtfd, &uin, sizeof(struct utmp)) 
			          != sizeof(struct utmp)) return;
		for (l = 0; l < MAXTTYS ; l++) 
			if ( strcmp(lines[l].line, uin.ut_line) == 0 
			||   strcmp(lines[l].line, ""         ) == 0 
			   ) break;
		if (inlist(uin.ut_name))
			notify("\r\nIn", uin);
		if (inlist(lines[l].name)) {
			strncpy(uin.ut_name, lines[l].name, UT_NAMESIZE);
			notify("\r\nOut", uin);
			uin.ut_name[0] = '\0';
		}
		strcpy(lines[l].name, uin.ut_name);
		strcpy(lines[l].line, uin.ut_line);
}

char **rargv;

void rehash(int nil) 
{

	if(vfork() )
		execvp(rargv[0], rargv);
	else
		exit(0);
}

int main (argc,argv) int argc; char **argv;
{

	watchfile = strcat(getenv("HOME"), "/.watchrc");

	rargv = argv;
	signal(SIGHUP, rehash);
	processparams(argc,argv);		/* opts, more names  */
	if (response)
		read_watchlist();		/* store in names[n] */
	read_utmp();				/* store in lines[l] */
	if (quick == 1) exit(1);	
	open_wtmp();				/* and seek to eof   */
	if (!fork()) {
		while (1) {
			if (!isatty(STDIN_FILENO)) {
				exit(0);
			}
			sleep(2);
			tail_wtmp();		/* new login/logout? */
		}
	}

	return 0;
}
