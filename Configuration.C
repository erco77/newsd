//
// Configuration.C -- News configuration class
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
// 80 //////////////////////////////////////////////////////////////////////////

#include "everything.H"
#include <stdarg.h>
#include <syslog.h>
#include <limits.h>	/* UINT_MAX */


// Initialize default configuration values...
Configuration::Configuration()
{
    char name[1024];			// Hostname string

    // Set defaults...
    HostnameLookups(0);

    norecurse_msgdir = 1;
    msgmod_dirs      = 0;

    Listen(119);

    log      = stderr;
    errorlog = "stderr";
    log_ino  = 0;

    LogLevel(L_INFO);

    MaxClients(0);
    MaxLogSize("1m");

    gethostname(name, sizeof(name));
    ServerName(name);

    SendMail(SENDMAIL " -t");

    SpamFilter("");

    SpoolDir(SPOOL_DIR);

    Timeout(12 * 3600);

    User("news");

    auth_user  = "-";
    auth_pass  = "-";
    auth_sleep = 5;
    auth_flags = AUTH_NOAUTH;
    auth_protect = 0;
}

// Listen on a specific address and port...
void Configuration::Listen(const char *l)
{
    char		hostname[256];	// Hostname or IP
    char		portname[256];	// Port number or name
    char		*ptr;		// Pointer into port
    struct hostent	*host;		// Host address
    struct servent	*port;		// Service data
    long		p;		// Port number

    // Initialize the listen address to "nntp"...
    listen.sin_family      = AF_INET;
    listen.sin_addr.s_addr = INADDR_ANY;
    listen.sin_port        = htons(119);

    // Try to grab a hostname and port number...
    switch (sscanf(l, "%255[^:]:%255s", hostname, portname))
    {
	case 1 :
	    // Hostname is a port number...
	    strcpy(portname, hostname);
	    strcpy(hostname, "*");
            break;

	case 2 :
            break;

	default :
	    fprintf(stderr, "news: Unable to decode address '%s'\n", l);
            return;
    }

    // Decode the hostname and port number as needed...
    if (hostname[0] && strcmp(hostname, "*"))
    {
	if ((host = gethostbyname(hostname)) == NULL)
	{
	    fprintf(stderr,
	        "newsd: gethostbyname(%s) failed - %s\n"
	        "newsd: Using address 127.0.0.1 (localhost)\n",
		hostname, hstrerror(h_errno));
	}
	else if (host->h_length != 4 || host->h_addrtype != AF_INET)
	{
	    fprintf(stderr, 
	        "newsd: gethostbyname(%s) did not return an IPv4 address!\n"
	        "newsd: Using address 127.0.0.1 (localhost)\n",
		hostname);
	}
	else
	    memcpy(&(listen.sin_addr), host->h_addr, 4);
    }
    else if (!strcmp(hostname, "*"))
	listen.sin_addr.s_addr = INADDR_ANY;

    if (portname[0] != '\0')
    {
	if (isdigit(portname[0]))
	{
	    p = strtol(portname, &ptr, 10);
	    if (p <= 0 || *ptr)
	    {
	        fprintf(stderr, 
		    "newsd: Bad port number '%s'\n"
        	    "newsd: Using port 119 (nntp)\n",
		    portname);
            }
	    else
		listen.sin_port = htons(p);
        }
	else
	{
	    if ((port = getservbyname(portname, "tcp")) == NULL)
	    {
        	fprintf(stderr,
		    "newsd: getservbyname(\"%s\", \"tcp\") failed!\n"
		    "newsd: Using port 119 (nntp)\n",
		    portname);
	    }
	    else
        	listen.sin_port = port->s_port;
	}
    }
}

// Listen on a port...
void Configuration::Listen(int p)
{
    // Listen on the "any" address with the specified port; needs to be
    // updated for IPv6 at some point...
    listen.sin_family      = AF_INET;
    listen.sin_addr.s_addr = INADDR_ANY;
    listen.sin_port        = htons(p);
}

#define BAD_VALUE()	\
    fprintf(stderr,	\
	    "newsd: Bad %s value '%s' on line %d of '%s'\n", \
	    name, value, linenum, conffile)

#define WARN_DEPRECATED(new_name)	\
    fprintf(stderr,			\
	    "newsd: %s on line %d of '%s' is deprecated; use '%s' instead\n", \
	    name, linenum, conffile, new_name)

// Load configuration values from a file...
void Configuration::Load(const char *conffile)
{
    FILE	*fp;			// Config file
    int		linenum;		// Line number
    char	line[1024],		// Line from config file
		*ptr,			// Pointer into line
		*name,			// Config directive
		*value;			// Config value
    long	lvalue;			// Integer value

    if ((fp = fopen(conffile, "r")) == NULL)
    {
        fprintf(stderr,
	    "newsd: Unable to open configuration file '%s' - %s!\n",
	    conffile, strerror(errno));
        return;
    }

    linenum = 0;

    while (fgets(line, sizeof(line), fp))
    {
        // Increment line counter...
        linenum ++;

        // Strip comments...
	if ((ptr = strchr(line, '#')) != NULL)
	    *ptr = '\0';

        // Strip trailing whitespace...
	for (ptr = line + strlen(line)-1; ptr >= line && isspace(*ptr); ptr--)
            *ptr = '\0';

	// Ignore blank lines...
	if (!line[0])
	    continue;

        // Find directive name...
	for (name = line; isspace(*name); name ++);

        // Separate directive from value...
	for (value = name + 1; !isspace(*value) && *value; value ++);

	while (isspace(*value))
	    *value++ = '\0';

	if (!*value)
	{
	    fprintf(stderr, "newsd: %s missing value on line %d of '%s'\n", 
	        name, linenum, conffile);
	    continue;
	}

        // Decode configuration directive...
	if (!strcasecmp(name, "HostnameLookups"))
	{
	    if (!strcasecmp(value, "off") || !strcasecmp(value, "no"))
	    {
	        HostnameLookups(0);
	    }
	    else if (!strcasecmp(value, "on") || !strcasecmp(value, "yes"))
	    {
	        HostnameLookups(1);
	    }
	    else if (!strcasecmp(value, "double"))
	    {
	        HostnameLookups(2);
	    }
	    else
	        BAD_VALUE();
	}
	else if (!strcasecmp(name, "NoRecurseMsgDir"))
	{
	         if (!strcasecmp(value, "off") || !strcasecmp(value, "no")) norecurse_msgdir = 0;
	    else if (!strcasecmp(value, "on") || !strcasecmp(value, "yes")) norecurse_msgdir = 1;
            else BAD_VALUE();
	}
	else if (!strcasecmp(name, "MsgModDirs"))
	{
	         if (!strcasecmp(value, "off") || !strcasecmp(value, "no")) msgmod_dirs = 0;
	    else if (!strcasecmp(value, "on") || !strcasecmp(value, "yes")) msgmod_dirs = 1;
            else BAD_VALUE();
	}
	else if (!strcasecmp(name, "Listen"))
	{
	    Listen(value);
	}
	else if (!strcasecmp(name, "LogFile"))
	{
	    WARN_DEPRECATED("ErrorLog");
	    ErrorLog(value);
	}
	else if (!strcasecmp(name, "ErrorLog"))
	{
	    ErrorLog(value);
	}
	else if (!strcasecmp(name, "LogLevel"))
	{
	    if (!strcasecmp(value, "error"))
	        LogLevel(L_ERROR);
	    else if (!strcasecmp(value, "info"))
	        LogLevel(L_INFO);
	    else if (!strcasecmp(value, "debug"))
	        LogLevel(L_DEBUG);
	    else
	        BAD_VALUE();
	}
	else if (!strcasecmp(name, "MaxClients"))
	{
	    lvalue = strtol(value, &ptr, 10);

	    if (lvalue < 0 || *ptr)
	        BAD_VALUE();
	    else
	        MaxClients(lvalue);
	}
	else if (!strcasecmp(name, "MaxLogSize"))
	{
	    MaxLogSize(value);
	}
	else if (!strcasecmp(name, "NewsHostname"))
	{
	    WARN_DEPRECATED("ErrorLog");
	    ServerName(value);
	}
	else if (!strcasecmp(name, "ServerName"))
	{
	    ServerName(value);
	}
	else if (!strcasecmp(name, "SendMail"))
	{
	    SendMail(value);
	}
	else if (!strcasecmp(name, "SpamFilter"))
	{
	    SpamFilter(value);
	}
	else if (!strcasecmp(name, "SpoolDir"))
	{
	    SpoolDir(value);
	}
	else if (!strcasecmp(name, "Timeout"))
	{
	    lvalue = strtol(value, &ptr, 10);

	    if (lvalue < 0 || *ptr)
	        BAD_VALUE();
	    else
	        Timeout(lvalue);
	}
	else if (!strcasecmp(name, "User"))
	{
	    User(value);
            if (bad_user)
	        BAD_VALUE();
	}
	else if (!strcasecmp(name, "Auth.User"))
	{
	    auth_user  = value;
	    auth_flags = ( auth_user == "-" ) ? AUTH_NOAUTH : AUTH_FAIL;
	}
	else if (!strcasecmp(name, "Auth.Pass"))
	{
	    auth_pass  = value;
	    auth_flags = ( auth_pass == "-" ) ? AUTH_NOAUTH : AUTH_FAIL;
	}
	else if (!strcasecmp(name, "Auth.Protect"))
	{
	         if (!strcasecmp(value, "post")) auth_protect = AUTH_POST;
	    else if (!strcasecmp(value, "read")) auth_protect = AUTH_READ;
	    else if (!strcasecmp(value, "all" )) auth_protect = AUTH_POST | AUTH_READ;
	    else if (!strcasecmp(value, "-"   )) auth_protect = 0;
	    else BAD_VALUE();
	}
	else if (!strcasecmp(name, "Auth.Sleep"))
	{
	    unsigned lval;
	    if ( sscanf(value, "%u", &lval) == 1 )
	        auth_sleep = lval;
	    else
	        BAD_VALUE();
	}
	else
	{
	    fprintf(stderr, 
	        "newsd: Unknown config file command '%s' on line %d of '%s'\n",
		line, linenum, conffile);
	}
    }

    // Close the config file and return...
    fclose(fp);
}

// CREATE THE NEWS LOG DIRECTORY IF IT DOESNT EXIST
//    Help zero admin for users using 'all defaults'.
//    (If they change any of the defaults, assume they know what they're doing)
//    No return; all of this is optional.
//    Assumes config file has already been loaded.
//
void Configuration::FixNewsLogDir(const char* newslogdir)
{
    int statok = 1;
    int domkdir = 0;
    struct stat logdir_stat;

    // Does log dir exist?
    if ( stat(newslogdir, &logdir_stat) == 0 )
    {
        // Stat ok, is the newslogdir a file instead of a directory?
	if ( ! S_ISDIR(logdir_stat.st_mode) )
	{
	    // Rename away old news file, then create dir
	    //    Rename /var/log/news -> /var/log/news.old
	    //
	    string oldfile; 
	    oldfile = newslogdir;
	    oldfile += ".old";
	    if ( rename(newslogdir, oldfile.c_str()) < 0 )
	    {
		fprintf(stderr,
		    "newsd: Can't rename away old news file to make log dir:\n"
		    "newsd: rename(%s,%s): %s\n", 
		    newslogdir, oldfile.c_str(), strerror(errno));
	    }
	    domkdir = 1;
	}
    }
    else
        // Stat failed? Dir does not exist
        { domkdir = 1; }

    // Need to create directory?
    if ( domkdir )
    {
        // Logdir does not exist? Create it
	if ( mkdir(newslogdir, 0755) )
	    fprintf(stderr, "newsd: mkdir(%s): %s\n",
	        newslogdir, strerror(errno));

	// Redo stat to get perms etc.
	if ( stat(newslogdir, &logdir_stat) < 0 )
	{
	    statok = 0;
	    fprintf(stderr, "newsd: stat(%s): %s\n", 
	        newslogdir, strerror(errno));
	}
    }

    // Now check perms on newslogdir
    if ( statok )
    {
	// Dir owned by "news" user?
	if ( UID() != logdir_stat.st_uid || GID() != logdir_stat.st_gid )
	{
	    // No? change it
	    if ( chown(newslogdir, UID(), GID()) < 0 )
	    {
		fprintf(stderr,
		    "newsd: Can't change perms on log directory '%s': "
		    "chown(%s,%lu,%lu) failed: %s\n",
		    newslogdir, newslogdir, (ulong)UID(), (ulong)GID(), strerror(errno));
	    }
	}
    }
}

// INITIALIZE LOGGING
void Configuration::InitLog()
{
    if (log)
    {
        if (log != stderr)
            fclose(log);
	log = 0;
    }

    if (errorlog == "syslog")
        openlog("newsd", LOG_PID, LOG_NEWS);
    else if (errorlog == "stderr")
        log = stderr;
    else if (errorlog.c_str()[0] == '|')
    {
        log = popen(errorlog.c_str() + 1, "w");

	if (!log)
	{
	    fprintf(stderr, "newsd: Unable to open log pipe to \"%s\" - %s.\n",
	            errorlog.c_str() + 1, strerror(errno));
            log = stderr;
	}
    }
    else
        OpenLogAppend();
}

// OPEN LOG FILE FOR APPENDING
//     o Closing existing log if open, new one is opened.
//     o Any LogLock()s are automatically unlocked.
//     o Log will be *UNLOCKED* on return.
//     o This function does *nothing* if errorlog == "stderr" or "syslog"
//
//     Returns -1 on error, error msgs are printed to stderr
//
int Configuration::OpenLogAppend()
{
    // Non-logfile? skip
    if ( errorlog == "stderr" || errorlog == "syslog" ||
         errorlog.c_str()[0] == '|' ) 
        return(0);

    // Close log (if open)
    if ( log )
    {
        if (log != stderr)
            fclose(log);		// (also clears Lock()s, if any)
	log = 0;
    }

    // Open log for append, do NOT apply locks
    log = fopen(errorlog.c_str(), "a");
    if ( !log )
    {
	fprintf(stderr, "newsd: Unable to open log file \"%s\": %s.\n",
		errorlog.c_str(), strerror(errno));
	log = stderr;
	return(-1);
    }

    setbuf(log, NULL);

    // Save inode# for log rotation checks
    {
	struct stat buf;
	log_ino = ( fstat(fileno(log), &buf) == 0 ) ? buf.st_ino : 0;
    }

    // This will fail when running as non-root, so don't bother checking
    // the status here...
    fchmod(fileno(log), 0600);
    fchown(fileno(log), uid, gid);

    return(0);
}

// LOCK THE LOG FILE FOR WRITING/ROTATING
//    Returns -1 on error, error msg sent to stderr
//
int Configuration::LogLock()
{
    if ( !log_ino ) return(0);

    if ( flock(fileno(log), LOCK_EX) < 0 )
    {
        fprintf(stderr, "newsd: LogLock(): flock(LOCK_EX): %s",
	    strerror(errno));
	return(-1);
    }

    return(0);
}

// UNLOCK THE LOG FILE
//    Returns -1 on error, error msg sent to stderr
//
int Configuration::LogUnlock()
{
    if ( !log_ino ) return(0);

    if ( flock(fileno(log), LOCK_UN) < 0 )
    {
        fprintf(stderr, "newsd: LogUnlock(): flock(LOCK_UN): %s",
	    strerror(errno));
	return(-1);
    }

    return(0);
}

// RETURN PATHNAME OF 'OLD' LOGFILE
string Configuration::OldLogFilename()
{
    // Append .O
    return(errorlog + ".O");
}

// PRINT MESSAGE TO LOG WITH DATE STAMP
//     Appends crlf if none specified.
//
void Configuration::DateStampedMessage(FILE *fp, const char *msg)
{
    time_t    secs;		// Current UNIX time
    struct tm *date;		// Current date/time
    char      datestr[1024];	// Date/time string

    time(&secs);
    date = localtime(&secs);
    strftime(datestr, sizeof(datestr), "%c", date);

    fprintf(fp, "%s newsd[%d]: %s%s", 
            datestr, getpid(), msg,
	    (msg[strlen(msg)-1] != '\n') ? "\n" : "");
    fflush(fp);
}

// HANDLE LOG ROTATION
//    Assumes Lock() already applied.
//    On return, Lock() will be applied to new log.
//
//    force: 
//           true  = force log to be rotated (regardless of size)
//           false = rotate only if logsize > maxlogsize
//
//    Returns:
//        0 - no error, log not rotated
//        1 - no error, log rotated
//       -1 - error occurred, error msg printed to log or stderr
//
int Configuration::Rotate(bool force)
{
    // Don't rotate if we aren't logging to file...
    if ( !log_ino ) return (0);

    // Check size of log
    if ( ! force && maxlogsize > 0)
    {
	struct stat buf;
	if ( stat(errorlog.c_str(), &buf) < 0 )
	{
	    string msg = "Log file size check failed: stat(" +
	                 errorlog + "): " + strerror(errno);
	    DateStampedMessage(log, msg.c_str());
	    return(-1);
	}

	// fprintf(stderr, "LOG SIZE CHECK: %lu <= %lu\n",  //DEBUG
	//     (ulong)buf.st_size, (ulong)maxlogsize);	    //DEBUG

        // Log too small? ignore
	if ( buf.st_size <= maxlogsize )
	    { return(0); }
    }

    // Rotate log (lock will follow renamed log)
    string oerrorlog = OldLogFilename();
    if ( rename(errorlog.c_str(), oerrorlog.c_str()) < 0 )
    {
        string msg = "Log file rotation failed: rename(" +
	             errorlog + "," + oerrorlog + "): " + strerror(errno);

	// Include uid/euid info, incase error is permission related
	{
            char junk[256];
	    snprintf(junk, sizeof(junk), " (uid=%lu euid=%lu)",
	        (ulong)getuid(), (ulong)geteuid());
	    msg += junk;
	}

	DateStampedMessage(log, msg.c_str());
	return(-1);
    }

    // Indicate log rotated in old log
    //    Log file is still locked, even though renamed
    //
    DateStampedMessage(log, "*** Log file rotated. ***");

    // Create new log
    //    This will close log (removing any locks)
    //
    if ( OpenLogAppend() < 0 )
        return(-1);

    // Leave with log locked
    //    Another process might sneak some msgs in before us.
    //
    LogLock();

    // Indicate log rotated in new log
    DateStampedMessage(log, "*** Log was rotated. ***");

    return(0);
}

// WAS LOG RECENTLY ROTATED?
//    Returns true if so, false if not.
//    Checks to see if inode number changed.
//
bool Configuration::WasLogRotated()
{
    if ( !log_ino ) return(false);

    struct stat buf;
    if ( stat(errorlog.c_str(), &buf) < 0 )
    {
        fprintf(stderr, "newsd: stat(%s): %s\n", 
	    errorlog.c_str(), strerror(errno));
	return(false);
    }

    return(buf.st_ino != log_ino);
}

// Log a message...
void Configuration::LogMessage(int l, const char *m, ...)
{
    // Only log messages as needed...
    if (l > loglevel)
        return;

    // Format the message...
    va_list      ap;			// Argument list pointer
    char         buffer[1024];		// Message buffer

    va_start(ap, m);
    vsnprintf(buffer, sizeof(buffer), m, ap);
    va_end(ap);

    // Send it to the log file or syslog...
    if (log)
    {
        LogLock();
	{
	    // Was log recently rotated? Reopen to write to correct log
	    if ( WasLogRotated() )
		{ OpenLogAppend(); LogLock(); }

	    // Automatic log rotation?
	    if ( maxlogsize > 0 )
	        Rotate(false);			// Handle log rotations

	    DateStampedMessage(log, buffer);	// Log message
	}
        LogUnlock();
    }
    else
        syslog(loglevel == L_ERROR ? LOG_ERR :
	           loglevel == L_INFO ? LOG_INFO : LOG_DEBUG, "%s", buffer);
}

void Configuration::LogSelf(int loglevel)
{
    LogMessage(loglevel, "ErrorLog %s", ErrorLog());
    LogMessage(loglevel, "HostnameLookups %s",
                      HostnameLookups() == 0 ? "off" :
	                  HostnameLookups() == 1 ? "on" : " double");
    struct sockaddr_in *addr = Listen();
    unsigned ipaddr = ntohl(addr->sin_addr.s_addr);
    LogMessage(loglevel, "Listen %u.%u.%u.%u:%d",
        	      (ipaddr >> 24) & 255, (ipaddr >> 16) & 255,
		      (ipaddr >> 8) & 255, ipaddr & 255,
		      ntohs(addr->sin_port));
    LogMessage(loglevel, "LogLevel %s",
        	      LogLevel() == L_ERROR ? "error" :
	        	  LogLevel() == L_INFO ? "info" : "debug");
    LogMessage(loglevel, "MaxClients %u", MaxClients());
    LogMessage(loglevel, "MaxLogSize %ld", MaxLogSize());
    LogMessage(loglevel, "SendMail %s", SendMail());
    LogMessage(loglevel, "ServerName %s", ServerName());
    LogMessage(loglevel, "SpamFilter %s", SpamFilter());
    LogMessage(loglevel, "SpoolDir %s", SpoolDir());
    LogMessage(loglevel, "Timeout %u", Timeout());
    LogMessage(loglevel, "User %s", User());
}

void Configuration::MaxLogSize(const char *val)
{
    double	number;			// Number
    char	units[255];		// Units

    switch (sscanf(val, "%lf%254s", &number, units))
    {
        default :
	    fprintf(stderr, "newsd: Bad MaxLogSize value \"%s\"!\n", val);
	    maxlogsize = 0;
	    break;

        case 1 :
	    maxlogsize = (long)number;
	    break;

	case 2 :
	    if (!strcasecmp(units, "k"))
	        maxlogsize = (long)(number * 1024.0);
	    else if (!strcasecmp(units, "m"))
	        maxlogsize = (long)(number * 1024.0 * 1024.0);
	    else if (!strcasecmp(units, "g"))
	        maxlogsize = (long)(number * 1024.0 * 1024.0 * 1024.0);
	    else
	        maxlogsize = (long)number;
	    break;
    }
}

int Configuration::lookup_user(const char *name)
{
    struct passwd *pw = getpwnam(name);

    if (pw == NULL )
    {
        uid      = UINT_MAX;
	gid      = UINT_MAX;
	bad_user = true;
        return(-1);
    }

    uid      = pw->pw_uid;
    gid      = pw->pw_gid;
    bad_user = false;
    endpwent();
    return(0);
}

// Handle authentication login
//    Returns:
//        1 on success
//       -1 on error, sleeps auth_sleep (if configured)
//
int Configuration::AuthLogin(const string& user, const string& pass)
{
    // SUCCESS
    if ( auth_user == user && auth_pass == pass )
	{ auth_flags = auth_protect; return(1); }
	
    // FAILURE
    if ( auth_sleep > 0 )
	sleep(auth_sleep);
    auth_flags = AUTH_FAIL;

    return(-1);
}
