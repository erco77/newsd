// newsd -- A simple news server - erco@3dsite.com
//
// Copyright 2003-2004 Michael Sweet
// Copyright 2002 Greg Ercolano
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public Licensse as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
// This program was designed based on the RFCs:
//
//	rfc1036.txt
//	rfc2980.txt
//	rfc977.txt
//
// TODO:
//      Should probably reorganize both NNTP headers and mail headers
//      so when browsing messages, headers don't jump around
//
//      Cleanup the whole mail gateway posting/mail vs regular posting/mail;
//      lots of redundant code.
//
//      IPv6 support!
//

#define _BSD_SIGNALS 1			/* IRIX */
#define _USE_BSD			/* LINUX */
#define _BSD				/* OSF1 */

// Convenience macro...
#define ISHEAD(a)	(strncasecmp(head[t].c_str(), (a), strlen(a))==0)

#include "Server.H"

// Global configuration data...
Configuration G_conf;

// Number of child processes...
static unsigned G_numclients = 0;

// Overview data headers...
static const char *overview[] =
{
    "Subject:",
    "From:",
    "Date:",
    "Message-ID:",
    "References:",
    "Bytes:",
    "Lines:",
    "Xref:full",
    "Reply-To:",
    NULL
};

// HANDLE SIGCHLD
//    Daemon reaps child processes to keep count.
//
void sigcld_handler(int)
{
    // REAPER
    //    for() limits #children reaped at a time mainly to prevent 
    //    possible infinite loop.
    //
    pid_t	pid;			// Process ID
    int		status;			// Exit status

    for (int t = 0; t < 100 && (pid = waitpid(-1, &status, WNOHANG)) > 0; t ++)
    {
        if ( pid > 0 && G_numclients > 0 ) 
	    G_numclients --;
    }
}

void HelpAndExit()
{
    fputs("newsd - a simple news daemon (V " VERSION ")\n"
          "        See LICENSE file packaged with newsd for license/copyright info.\n"
          "\n"
	  "Usage:\n"
          "    newsd [-c configfile] [-d] [-f] -- start server\n"
	  "    newsd -mailgateway <group>      -- used in /etc/aliases\n"
	  "    newsd -newgroup                 -- used to create new groups\n"
	  "    newsd -rotate                   -- force log rotation\n",
	  stderr);
    exit(1);
}

// RUN AS A PARTICULAR USER
int RunAs()
{
    int err = 0;

    if (chdir("/")) { perror("newsd: chdir(\"/\")"); err = 1; }

    if (G_conf.BadUser()) err = 1;
    else if (!geteuid())
    {
        gid_t gid = G_conf.GID();
        setgroups(1, &gid);
	if ( setgid(gid) < 0 ) { perror("newsd: setgid()"); err = 1; }
	if ( setuid(G_conf.UID()) < 0 ) { perror("newsd: setuid()"); err = 1; }
    }

    return(err);
}

// CREATE DEAD LETTER FILE, CHOWN ACCORDINGLY
int CreateDeadLetter()
{
    string filename = G_conf.SpoolDir();
    filename += "/.deadletters";
    fprintf(stderr, "(Creating %s)\n", filename.c_str());
    FILE *fp = fopen(filename.c_str(), "a");
    if ( fp == NULL ) { perror(filename.c_str()); return(-1); }
    fchmod(fileno(fp), 0600);
    fchown(fileno(fp), G_conf.UID(), G_conf.GID());
    fclose(fp);

    return(0);
}

// WRITE A MESSAGE (header+body) TO DEAD LETTER FILE
void DeadLetter(const char *errmsg, vector<string>&head, vector<string>&body)
{
    string filename = G_conf.SpoolDir();
    filename += "/.deadletters";
    FILE *fp = fopen(filename.c_str(), "a");
    fchmod(fileno(fp), 0600);
    fchown(fileno(fp), G_conf.UID(), G_conf.GID());
    if ( fp == NULL )
        { perror(filename.c_str()); return; }
    for ( uint t=0; t<head.size(); t++ )
        fprintf(fp, "%s\n", head[t].c_str());
    fprintf(fp, "\n");
    for ( uint t=0; t<body.size(); t++ )
        fprintf(fp, "%s\n", body[t].c_str());
    fprintf(fp, "\n");
    fclose(fp);
}

// HANDLE GATEWAYING MAIL INTO THE NEWSGROUP
//    Reads email message from stdin.
//
int MailGateway(const char *groupname)
{
    Group group;
    if ( group.Load(groupname) < 0 )
    {
	fprintf(stderr, "newsd: Unknown group \"%s\": %s\n", groupname,
                group.Errmsg());
	return(1);
    }

    // COLLECT EMAIL FROM STDIN
    int linechars = 0,
	linecount = 0,
	toolong = 0;
    char c;
    string msg;

    // CONVERT EMAIL HEADER -> NEWSGROUP HEADER
    {
	// Newsgroups:
	msg = "Newsgroups: ";
	msg += groupname;
	msg += "\r\n";

	// X-News-Gateway:
	//    I pulled this outta my ass; need some way to indicate
	//    msg passed through a mail -> news gateway.
	//
	msg += "X-Mail-To-News-Gateway: via newsd ";
	msg += VERSION;
	msg += "\r\n";
    }

    while (read(0, &c, 1) == 1 )
    {
        if ( c == '\r' ) continue;              // ignore \r, we handle it ourself

	// KEEP TRACK OF #LINES
	//    Lines longer than 80 chars count as multiple lines.
	//    If posting too long, stop accumulating message in ram,
	//    but keep reading until they've sent the terminating "."
	//
	++linechars;
	if ( linechars > 80 || c == '\n' )
	    { linechars = 0; linecount++; }
	if ( group.PostLimit() > 0 && linecount > group.PostLimit() )
	    { toolong = 1; continue; }

        if ( c == '\n' ) msg += '\r';
        msg += c;
    }

    // POSTING TOO LONG? FAIL
    if ( toolong )
    {
	fprintf(stderr, "newsd: Article not posted to %s: longer than %d lines.\n",
	        groupname, group.PostLimit());
	return(1);
    }

    // BLESS ARTICLE -- VERIFY HEADER INTEGRITY, ADD NEEDED HEADERS
    vector<string> head;
    vector<string> body;
    if ( group.ParseArticle(msg, head, body) < 0 )
    {
	fprintf(stderr, "newsd: Article not posted to %s: %s.\n",
	        groupname, group.Errmsg());
	return(1);
    }

    // UPDATE 'Path:'
    group.UpdatePath(head);

    // CHECK FOR LOOPS, MASSAGE HEADERS
    for ( uint t=0; t<head.size(); t++ )
    {
	// CONVERT RFC822 "From .." -> "X-Original-From: .."
	if ( ISHEAD("From ") )
	{
	    string newfrom = head[t];
	    newfrom.replace(0, strlen("From "), "X-Original-From: ");
	    head[t] = newfrom;
	}
	// SHORT CIRCUIT LOOP DELIVERY
	//     Example: "X-Loop: outOfSpace"
	//
	else if ( ISHEAD("X-Newsd-Loop:") || ISHEAD("X-Loop:") )
	{
	    string errmsg = "newsd: -mailgateway ";
	    errmsg += groupname;
	    errmsg += ": NOT POSTED: '";
	    errmsg += head[t];
	    errmsg += "' mail loop detected: message dropped to "
	              SPOOL_DIR "/.deadletters";
	    DeadLetter(errmsg.c_str(), head, body);
	    fprintf(stderr, "%s\n", 
	        (const char*)errmsg.c_str());
	    return(1);
	}
    }

    {for (uint t=0; t<head.size(); t++) { G_conf.LogMessage(L_DEBUG, "Gateway Post: --- head[%03d]: '%s'\n", t, head[t].c_str()); } }
    {for (uint t=0; t<body.size(); t++) { G_conf.LogMessage(L_DEBUG, "Gateway Post: --- body[%03d]: '%s'\n", t, body[t].c_str()); } }

    // POST ARTICLE
    //    Don't affect 'current group' or 'current article'.
    //
    if ( group.Post(overview, head, body, "localhost", true) < 0 )
    {
	fprintf(stderr, "newsd: Article not posted to %s: %s.\n",
	        groupname, group.Errmsg());
	return(1);
    }

    // CC MESSAGE TO MAIL ADDRESS?
    if ( group.IsCCPost() )
    {
	string from = "Anonymous",
	       subject = "-";

	// HANDLE PRESERVING FIELDS FROM NNTP POSTING -> SMTP
	string preserve;
	int pflag = 0;
	for ( unsigned t=0; t<head.size(); t++ )
	{
	    char c = head[t].c_str()[0];

	    // CONTINUATION OF HEADER LINE?
	    if ( c == ' ' || c == 9 )
	    {
		// CONTINUATION OF PREVIOUS PRESERVED HEADER LINE?
		if ( pflag )
		    { preserve += head[t]; preserve += "\n"; }
		continue;
	    }

	    // ZERO OUT PRESERVE -- NO MORE CONTINUATIONS
	    pflag = 0;

	    // CHECK FOR PRESERVE FIELDS
	    if ( ISHEAD("From: ") ||			// must
		 ISHEAD("Subject: ") ||			// must
		 ISHEAD("References: ") ||		// needed to preserve threading
		 ISHEAD("Xref: ") ||			// ?
		 ISHEAD("Path: ") ||			// RFC 1036 2.1.6 (STR #15)
		 ISHEAD("Content-Type: ") ||		// mime related
		 ISHEAD("MIME-Version: ") ||		// mime related
		 ISHEAD("Message-ID: ") )		// needed to preserve threading
	    {
		pflag = 1;
		preserve += head[t];
		preserve += "\n";
	    }
	}

        G_conf.LogMessage(L_DEBUG, "popen(%s,\"w\")..", 
	    (const char*)G_conf.SendMail());

	FILE *fp = popen(G_conf.SendMail(), "w");
	if ( ! fp )
	{
            G_conf.LogMessage(L_ERROR, 
	        "mailgateway: ccpost popen() can't execute '%s': %s",
                (const char*)G_conf.SendMail(),
                (const char*)strerror(errno));
	}
	else
	{
	    fprintf(fp, "To: %s\n", (const char*)group.VoidEmail());
	    fprintf(fp, "Bcc: %s\n", (const char*)group.CCPost());
	    fprintf(fp, "%s", preserve.c_str());

	    // Reply-To: Needed for mail gateway
	    if ( group.IsReplyTo() )
		fprintf(fp, "Reply-To: %s\n", (const char*)group.ReplyTo());

	    // Errors-To: advised so admin hears about problems, in addition
	    //            to the real person who sent the message.
	    //
	    fprintf(fp, "Errors-To: %s\n", (const char*)group.Creator());
	    fprintf(fp, "\n");
	    fprintf(fp, "[posted to %s]\n\n", (const char*)group.Name());
	    for (unsigned t = 0; t < body.size(); t ++ )
		fprintf(fp, "%s\n", body[t].c_str());
	    if (pclose(fp) < 0)
                G_conf.LogMessage(L_ERROR, 
		    "mailgateway: ccpost pclose() failed for '%s': %s",
                    (const char*)G_conf.SendMail(),
                    (const char*)strerror(errno));
	}
    }
    
    return(0);
}

int main(int argc, const char *argv[])
{
    // HANDLE SIGCHLD, IGNORE SIGPIPE + SIGALRM
    signal(SIGCHLD, sigcld_handler);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGALRM, SIG_IGN);
    umask(022);		// enforce rw-r--r-- perms

    Server server;
    const char *conffile = CONFIG_FILE;
    const char *mailgateway = NULL;
    int newgroup = 0;
    int dodebug = 0,
        dofork = 1,
        dorotate = 0;

    // Scan command-line...
    for (int t = 1; t < argc; t ++)
    {
        if (!strcmp(argv[t], "-c"))
	{
	    if (++t >= argc)
	    {
	        fputs("newsd: Expected filename after \"-c\"!\n", stderr);
		HelpAndExit();
	    }

            conffile = argv[t];
	}
	else if (!strcmp(argv[t], "-d"))
	    { dodebug = 1; dofork = 0; }
	else if (!strcmp(argv[t], "-f"))
	    { dofork = 0; }
	else if (!strncmp(argv[t], "-h", 2))
	    { HelpAndExit(); }
        else if (!strcmp(argv[t], "-mailgateway"))
	{
	    if (++t >= argc)
	    {
	        fputs("newsd: Expected groupname after \"-mailgateway\"!\n", stderr);
		HelpAndExit();
	    }

            mailgateway = argv[t];
	    dofork      = 0;
	}
        else if (!strcmp(argv[t], "-newgroup"))
	    { newgroup = 1; dofork = 0; }
        else if (!strcmp(argv[t], "-rotate"))
	    { dorotate = 1; dofork = 0; }
	else
	    { fprintf(stderr, "newsd: Unknown argument '%s'\n", argv[t]); HelpAndExit(); }
    }

    // Load global config data...
    G_conf.Load(conffile);

    if (dodebug)
    {
        G_conf.LogLevel(L_DEBUG);
	G_conf.ErrorLog("stderr");
    }

    // Do stuff...
    if (dorotate)
    {
        G_conf.InitLog();	// open log (it isn't yet)
        G_conf.LogLock();	// lock while rotating
	G_conf.Rotate(true);	// force rotation
	G_conf.LogUnlock();
	exit(0);
    }
    else if (mailgateway)
    {
	if (RunAs()) return(1);

	return(MailGateway(mailgateway));
    }
    else if (newgroup)
    {
	if (RunAs()) return(1);

	// CREATE DEAD LETTER FILE, IF NONE
	if ( CreateDeadLetter() < 0 )
	    { fprintf(stderr, "news: CreateDeadLetter(): failed\n"); return(1); }

	Group tmp;
	return(tmp.NewGroup());
    }

    // Start logging...
    G_conf.InitLog();

    // Log server information...
    G_conf.LogMessage(L_INFO, "-- newsd started - V%s--", VERSION);
    G_conf.LogMessage(L_INFO, "-- start config summary --");
    G_conf.LogSelf(L_INFO);
    G_conf.LogMessage(L_INFO, "-- end config summary --");

    if (server.Listen() < 0)
    {
        G_conf.LogMessage(L_ERROR, "Unable to listen for connections: %s",
	                  server.Errmsg());
        return(1);
    }

    // RUN AS THE USER 'NEWS'
    //     Now that we've opened the reserved port, 
    //     we no longer need to be root.
    //
    if (RunAs())
        return(1);

    // Fork into the background...
    if (dofork)
    {
	pid_t pid = fork();
	switch ( pid )
	{
	    case -1: // ERROR
	        G_conf.LogMessage(L_ERROR, "daemonize fork(): %s (exiting)", 
		                  strerror(errno));
		return(1);

	    case 0:  // CHILD
        	// "Daemonize" our application so it is no longer connected to
		// the current terminal session...
        	close(0);
		open("/dev/null", O_RDONLY);
		close(1);
		open("/dev/null", O_WRONLY);
		close(2);
		open("/dev/null", O_WRONLY);

	        if ( setsid() < 0 )
		    G_conf.LogMessage(L_ERROR, "setsid() failed (ignored): %s", 
				      strerror(errno));
		break;
		
	    default: // PARENT
		return(0);
	}
    }

    // ACCEPT NEW CONNECTIONS LOOP
    for (;;)
    {
        if (server.Accept() < 0)
	{
	    G_conf.LogMessage(L_ERROR, "Unable to accept new connection: %s",
	                      server.Errmsg());
	    sleep(10);
	    continue;
        }

	// TOO MANY CHILDREN?
	if (G_conf.MaxClients() != 0 && G_numclients >= G_conf.MaxClients())
	{
	    server.Send("400 Server has too many connections open -- try again later");
	    close(server.MsgSock());
	    continue;
	}

	// FORK A CHILD TO HANDLE CONNECTION
	//    News readers can keep a connection open for the entire
	//    duration of the news reading session.
	//
	pid_t pid;
	while ((pid = fork()) == -1)
	{
	    G_conf.LogMessage(L_ERROR, "Unable to fork handler process: %s",
	                      strerror(errno));
	    sleep(10);
        }

	G_numclients ++;

	switch (pid)
	{
	    case 0 :	// CHILD
	        G_conf.ErrorLog(G_conf.ErrorLog());
		server.CommandLoop(overview);
		exit(0);

	    default :	// PARENT
		close(server.MsgSock());
	        break;
	}
    }
    //NOTREACHED
}
