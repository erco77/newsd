//
// Group.C -- Manage newsgroup groups
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

#include "Group.H"
#include <dirent.h>


// RETURN ASCII VERSION OF AN UNSIGNED LONG
static const char *ultoa(unsigned long num)
{
    static char s[80];
    sprintf(s, "%lu", num);
    return(s);
}

const char *Group::Dirname()
{
    // CONVERT GROUP NAME TO A DIRECTORY NAME
    //    eg. rush.general -> /var/spool/news/rush/general
    //
    dirname = G_conf.SpoolDir();
    dirname += "/";
    dirname += name;
    for ( int t=0; dirname[t]; t++ )
        if ( dirname[t] == '.' )
	    dirname[t] = '/';

    return(dirname.c_str());
}

// SAVE OUT GROUP'S ".info" FILE
//     Returns -1 on error, errmsg has reason.
//
int Group::SaveInfo(int dolock)
{
    string path = Dirname();
    path += "/.info";

    // WRITE OUT INFO FILE
    int ilock = -1;
    if ( dolock ) { ilock = WriteLock(); }
    {
	FILE *fp = fopen(path.c_str(), "w");
	if ( fp == NULL )
	{
	    errmsg = path;
	    errmsg += ": ";
	    errmsg += strerror(errno);
	    G_conf.LogMessage(L_ERROR, "Group::SaveInfo(): %s", errmsg.c_str());
	    if ( dolock ) Unlock(ilock);
	    return(-1);
	}

	WriteString(fp, "start       ");
	WriteString(fp, ultoa(start));
	WriteString(fp, "\n");

	WriteString(fp, "end         ");
	WriteString(fp, ultoa(end));
	WriteString(fp, "\n");

	WriteString(fp, "total       ");
	WriteString(fp, ultoa(total));
	WriteString(fp, "\n");

	fflush(fp);
	fsync(fileno(fp));
	fclose(fp);
    }
    if ( dolock ) { Unlock(ilock); }
    return(0);
}

// BUILD GROUP INFO FROM ACTUAL ARTICLES ON DISK
//    Do this if info file doesn't already exist
//    Returns -1 on error, errmsg has reason.
//
int Group::BuildInfo(int dolock)
{
    int wlock = -1;
    int ret = 0;
    if ( dolock ) { wlock = WriteLock(); }

    start   = 0;
    end     = 0;
    total   = 0;

    DIR *dir;
    struct dirent *dent;
    struct stat fileinfo;
    string filename;
    unsigned long temp;

    if ((dir = opendir(dirname.c_str())) != NULL)
    {
        while ((dent = readdir(dir)) != NULL)
        {
            // Skip entries that don't start with a number
            if (!isdigit(dent->d_name[0] & 255)) continue;

            filename = dirname + "/" + dent->d_name;

            // Using "modulus dirs" for optimization?
            if (G_conf.MsgModDirs())
            {
                // Expect a modulus subdir
                if (stat(filename.c_str(), &fileinfo)) continue;
                if (!S_ISDIR(fileinfo.st_mode)) continue;

                DIR *modulus_dir;
                struct dirent *modulus_dent;
                string modulus_filename;

                // Descend into 'modulus dir'
                //     /path/fltk/general/1000/1999
                //           ------------ ---- ----
                //                |        |    |
                //                |        |    Msg#
                //                |        Modulus dir
                //                Group name
                //
                if ((modulus_dir = opendir(filename.c_str())) != NULL)
                {
                    while ((modulus_dent = readdir(modulus_dir)) != NULL)
                    {
                        // Skip entries that don't start with a number
                        if (!isdigit(modulus_dent->d_name[0] & 255)) continue;

                        modulus_filename = filename + "/" + modulus_dent->d_name;

                        // Skip directories
                        if (stat(modulus_filename.c_str(), &fileinfo)) continue;
                        if (S_ISDIR(fileinfo.st_mode)) continue;

                        // Convert message number to an integer and compare...
                        temp = strtoul(modulus_dent->d_name, NULL, 10);
                        if (temp < start || total == 0) start = temp;
                        if (temp > end || total == 0) end = temp;
                        total ++;
                    }
                }
                closedir(modulus_dir);
            }
            else
            {
              // Skip directories
              if (stat(filename.c_str(), &fileinfo)) continue;
              if (S_ISDIR(fileinfo.st_mode)) continue;

              // Convert message number to an integer, cacl start/end/total
              temp = strtoul(dent->d_name, NULL, 10);
              if (temp < start || total == 0) start = temp;
              if (temp > end || total == 0) end = temp;
              total ++;
            }
        }
        closedir(dir);
    }

    ret = SaveInfo(0);

    if ( dolock ) { Unlock(wlock); }

    return(ret);
}

// LOAD GROUP INFO
//    If none exists, create a .info file.
//    Returns -1 on error, errmsg has reason.
//
int Group::LoadInfo(int dolock)
{
    // LOAD INFO FILE
    string path = Dirname();
    path += "/.info";

    FILE *fp = fopen(path.c_str(), "r");
    if ( fp == NULL )
    {
	// NO INFO FILE? BUILD ONE
	if ( errno == ENOENT )
        {
            // Only build if its a valid group (has a .config file)
            if ( ! IsValidGroup() )
            {
                errmsg = "invalid group";
                G_conf.LogMessage(L_ERROR, "LoadInfo(): %s", errmsg.c_str());
                return(-1);
            }

            return(BuildInfo(dolock));
        }

	errmsg = path;
	errmsg += ": ";
	errmsg += strerror(errno);
	G_conf.LogMessage(L_ERROR, "Group::LoadInfo(): %s", errmsg.c_str());
	return(-1);
    }

    int ilock = -1;
    if ( dolock ) { ilock = ReadLock(); }
    {
	char buf[LINE_LEN];
	// char foo[256];
	while ( fgets(buf, LINE_LEN-1, fp) )
	{
	    // REMOVE TRAILING \n
	    if ( strlen(buf) > 1 )
		buf[strlen(buf)-1] = 0;

	    // SKIP BLANK LINES AND COMMENTS
	    if ( buf[0] == '#' || buf[0] == '\n' ) continue;

	    if ( sscanf(buf, "start %lu", &start) == 1 ) 
		{ continue; }
	    if ( sscanf(buf, "end %lu", &end  ) == 1 ) 
		{ continue; }
	    if ( sscanf(buf, "total %lu", &total) == 1 )
		{ continue; }
	}
	fclose(fp);
    }
    if ( dolock ) { Unlock(ilock); }
    return(0);
}

// LOAD GROUP'S ".config" FILE
//    Returns -1 on error, errmsg has reason.
//
int Group::LoadConfig(int dolock)
{
    // LOAD CONFIG FILE
    string path = Dirname();
    path += "/.config";

    FILE *fp = fopen(path.c_str(), "r");
    if ( fp == NULL )
    {
	errmsg = path;
	errmsg += ": ";
	errmsg += strerror(errno);
	G_conf.LogMessage(L_ERROR, "Group::LoadConfig(): %s", errmsg.c_str());
	return(-1);
    }

    int ilock = -1;
    if ( dolock ) { ilock = ReadLock(); }
    {
	char buf[LINE_LEN];
	char foo[256];
	ccpost = "";
	while ( fgets(buf, LINE_LEN-1, fp) )
	{
	    // REMOVE TRAILING \n
	    if ( strlen(buf) > 1 )
		buf[strlen(buf)-1] = 0;

	    // SKIP BLANK LINES AND COMMENTS
	    if ( buf[0] == '#' || buf[0] == '\n' ) continue;

	    if ( strncmp(buf, "description ", strlen("description ")) == 0 )
	    {
		desc = buf + strlen("description ");
		continue;
	    }
	    if ( sscanf(buf, "creator %255s", foo) == 1 )
		{ creator = foo; continue; }
	    if ( sscanf(buf, "postok %d", &postok) == 1 )
		{ continue; }
	    if ( sscanf(buf, "postlimit %d", &postlimit) == 1 )
		{ continue; }
	    if ( sscanf(buf, "ccpost %255s", foo) == 1 )
	    { 
		// Add trailing comma if none
		if ( ccpost != "" && ccpost != "-" )
		{
		    const char *end = ccpost.c_str() + ccpost.length() - 1;
		    if ( *end != ',' )
			{ ccpost += ","; }
		}
		ccpost += foo;
		continue;
	    }
	    if ( sscanf(buf, "replyto %255s", foo) == 1 )
		{ replyto = foo; continue; }
	    if ( sscanf(buf, "voidemail %255s", foo) == 1 )
		{ voidemail = foo; continue; }
	}
	fclose(fp);
    }
    if ( dolock ) { Unlock(ilock); }

    if ( ccpost == "" ) ccpost = "-";

    G_conf.LogMessage(L_DEBUG, "ccpost is now '%s' %s", ccpost.c_str(),
                      errmsg.c_str());
    return(0);
}

// SAVE OUT GROUP'S ".config" FILE
//    This should only be done by manually invoking 'newsd -newgroup'.
//    Returns -1 on error, errmsg has reason.
//
int Group::SaveConfig()
{
    string path = Dirname();
    path += "/.config";

    // WRITE OUT CONFIG FILE
    int ilock = WriteLock();
    {
	FILE *fp = fopen(path.c_str(), "w");
	if ( fp == NULL )
	{
	    errmsg = path;
	    errmsg += ": ";
	    errmsg += strerror(errno);
	    Unlock(ilock);
	    return(-1);
	}

	WriteString(fp, "description ");
	WriteString(fp, desc.c_str());           
	WriteString(fp, "\n");

	WriteString(fp, "creator     ");
	WriteString(fp, creator.c_str());
	WriteString(fp, "\n");

	WriteString(fp, "postok      ");
	WriteString(fp, ultoa((unsigned long)postok));
	WriteString(fp, "\n");

	WriteString(fp, "postlimit   ");
	WriteString(fp, ultoa((unsigned long)postlimit));
	WriteString(fp, "\n");

	WriteString(fp, "ccpost      ");
	WriteString(fp, ccpost.c_str());
	WriteString(fp, "\n");

	WriteString(fp, "replyto     ");
	WriteString(fp, replyto.c_str());
	WriteString(fp, "\n");

	WriteString(fp, "voidemail   ");
	WriteString(fp, voidemail.c_str());
	WriteString(fp, "\n");

	fflush(fp);
	fsync(fileno(fp));
	fclose(fp);
    }
    Unlock(ilock);
    return(0);
}

// LOAD INFO FOR GROUP
//    Set dolock=0 if lock has already been made
//    to prevent deadlocks.
//
int Group::Load(const char *group_name, int dolock)
{
    valid = 0;			// assume failed until success

    if ( strlen(group_name) >= GROUP_MAX )
         { errmsg = "Group name too long"; return(-1); }

    struct stat sbuf;
    if ( stat(Dirname(), &sbuf) < 0 )
    {
	errmsg = "invalid group name '";
	errmsg += group_name;
	errmsg += "': ";
	errmsg += strerror(errno);
	G_conf.LogMessage(L_ERROR, "Group::Load(): %s", errmsg.c_str());
	return(-1);
    }

    // GET CREATION TIME FROM DIR'S DATESTAMP
    ctime = sbuf.st_ctime;

    // LOAD INFO FILE
    //    Builds one if it doesn't exist
    //
    name = group_name;
    if ( LoadInfo(dolock) < 0 )
	{ return(-1); }

    // LOAD CONFIG FILE
    if ( LoadConfig(dolock) < 0 )
	{ return(-1); }

    // GROUP IS NOW VALID
    valid = 1;

    return(0);
}

// WRITE NULL TERMINATED STRING TO FD
int Group::WriteString(FILE *fp, const char *buf)
{
    size_t len = strlen(buf);
    if ( fwrite(buf, 1, len, fp) != len)
    {
        errmsg = "write error";
	G_conf.LogMessage(L_ERROR, "Group::WriteString(): %s", errmsg.c_str());
	return(-1);
    }
    return(0);
}

// FIND ARTICLE NUMBER GIVEN A MESSAGEID
int Group::FindArticleByMessageID(const char *message_id, unsigned long &number)
{
    char	*ptr;

    // See if we have a Message-Id of the form number-groupname@servername...
    ptr    = NULL;
    number = strtoul(message_id + 1, &ptr, 10);
    if (!ptr || *ptr != '-')
    {
	// No, do a slow lookup...
	int idlen = strlen(message_id);
	unsigned long temp;
	char line[1024];
	FILE *fp;

	// Load dirname with the correct group directory...
	Dirname();

	for (number = 0, temp = Start(); temp <= End(); temp ++)
	{
            snprintf(line, sizeof(line), "%s/%lu", dirname.c_str(), temp);

	    if ((fp = fopen(line, "r")) != NULL)
	    {
		while (fgets(line, sizeof(line), fp) != NULL)
	            // Stop at the first blank line or a Message-ID: header
	            if (line[0] == '\n' || !strncasecmp(line, "Message-Id:", 11))
			break;

        	fclose(fp);

        	if (line[0] != '\n')
		{
	            // Compare this message ID against the search ID...
		    for (ptr = line + 11; *ptr && isspace(*ptr & 255); ptr ++)
		        { }

		    // TODO: is the message ID case insensitive???
                    if (!strncmp(ptr, message_id, idlen) && ptr[idlen] == '\n')
		    {
		        // Found it!
		        number = temp;
			break;
		    }
		}
	    }
	}

	// Return an error if necessary...
	if (!number)
	{
	    errmsg = string("Message-ID not found: ") + message_id;
	    G_conf.LogMessage(L_ERROR, "Message-ID not found: %s", message_id);
	    return(-1);
	}
    }

    return (0);
}

// READ LOCK
int Group::ReadLock()
{
    string lockpath = Dirname();
    lockpath += "/.lock";
    int fd = open(lockpath.c_str(), O_CREAT|O_WRONLY, 0644);
    if ( fd < 0 )
    {
        errmsg = lockpath;
	errmsg += ": ";
	errmsg += strerror(errno);
	G_conf.LogMessage(L_ERROR, "Group::ReadLock(): %s", errmsg.c_str());
	return(-1);
    }
    if ( flock(fd, LOCK_SH) < 0 )
    {
        errmsg = "flock(";
	errmsg += lockpath;
	errmsg += ", SHARED): ";
	errmsg += strerror(errno);
	G_conf.LogMessage(L_ERROR, "Group::ReadLock(): %s", errmsg.c_str());
	return(-1);
    }
    return(fd);
}

// WRITE LOCK
int Group::WriteLock()
{
    string lockpath = Dirname();
    lockpath += "/.lock";
    int fd = open(lockpath.c_str(), O_CREAT|O_WRONLY, 0644);
    if ( fd < 0 )
    {
        errmsg = lockpath;
	errmsg += ": ";
	errmsg += strerror(errno);
	G_conf.LogMessage(L_ERROR, "Group::WriteLock(): %s", errmsg.c_str());
	return(-1);
    }
    if ( flock(fd, LOCK_EX) < 0 )
    {
        errmsg = "flock(";
	errmsg += lockpath;
	errmsg += ", EXCLUSIVE): ";
	errmsg += strerror(errno);
	G_conf.LogMessage(L_ERROR, "Group::WriteLock(): %s", errmsg.c_str());
	return(-1);
    }
    return(fd);
}

// RELEASE POSTING LOCK
void Group::Unlock(int fd)
{
    if ( fd > 0 )
	{ flock(fd, LOCK_UN); close(fd); }
}

// REORDER AN ARTICLE'S HEADER
//     Why is this here? Because POST is a group method,
//     so we need it here.
//
void Group::ReorderHeader(const char*overview[], vector<string>& head)
{
    // REFORMAT HEADER IN OVERVIEW FORMAT
    vector<string> newhead;

    // SORT OVERVIEW HEADERS TO TOP
    for ( int t=0; overview[t]; t++ )
    {
        for ( unsigned int r=0; r<head.size(); r++ )
	{
	    if (strncmp(overview[t], head[r].c_str(), strlen(overview[t])) == 0)
	    {
	        newhead.push_back(head[r]);
		head.erase(head.begin() + r);
		break;
	    }
	}
    }
    // APPEND THE REST
    for ( unsigned int r=0; r<head.size(); r++ )
        newhead.push_back(head[r]);

    // UPDATE HEAD WITH NEW SORT ORDER
    head = newhead;
}

// RETURN CURRENT DATE IN RFC822 FORMAT
//    eg: Sun, 27 Mar 83 20:39:37 -0800
//
//    Modified to use 4 digit year instead of lossy 2 digit.
//    This appears to be some kind of modern deviation;
//    I see nothing in RFC 822 about this. Maybe a post Y2K mod?
//                                                     -erco
//
//    4-digit year format was added in RFC 2822
//        - mike
const char *Group::DateRFC822()
{
    time_t lt = time(NULL);
    struct tm *tm = localtime(&lt);
    const char *wday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    const char *mon[]  = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    // spec: Wdy, DD Mon YY HH:MM:SS TZ   (where TZ is a GMT offset "<+/->HHMM")
    //   eg: Sun, 27 Mar 83 20:39:37 -0800
    //       Sun, 27 Mar 83 20:39:37 +0200
    //
    char gmtoff_sign = (tm->tm_gmtoff<0) ? '-' : '+';
    long gmtoff_abs  = (tm->tm_gmtoff<0) ? -(tm->tm_gmtoff) : (tm->tm_gmtoff);
    int  gmtoff_hour = gmtoff_abs / 3600;	// secs -> hours
    int  gmtoff_min  = (gmtoff_abs / 60) % 60;	// secs -> mins
    sprintf(datebuf, "%.3s, %02d %.3s %d %02d:%02d:%02d %c%02d%02d",
	(const char*)wday[tm->tm_wday],
	(int)tm->tm_mday,
	(const char*)mon[tm->tm_mon],
	(int)tm->tm_year + 1900,// 4 digit date instead of 2 digit (mozilla)
	(int)tm->tm_hour,
	(int)tm->tm_min,
	(int)tm->tm_sec,
	(char)gmtoff_sign,
	(int)gmtoff_hour,
	(int)gmtoff_min);
    return(datebuf);
}

// POST ARTICLE
int Group::Post(const char *overview[], 
		vector<string> &head,
		vector<string> &body,
		const char *remoteip_str,
		bool force)
{
    // DETERMINE GROUP BEING POSTED TO
    int found = 0;
    char postgroup[255];
    for ( unsigned int t=0; !found && t<head.size(); t++ )
    {
        if ( strncmp(head[t].c_str(), "Newsgroups: ", strlen("Newsgroups: ")) 
	     == 0 )
	{
	    if ( sscanf(head[t].c_str(), "Newsgroups: %254s", postgroup) == 1 )
		{ found = 1; }
	}
    }

    if ( ! found )
        { errmsg = "article has no 'Newsgroups' field"; return(-1); }

    // LOCK FOR POSTING
    Name(postgroup);
    int plock = WriteLock();
    {
	// LOAD INFO FOR THIS GROUP
	if ( Load(postgroup, 0) < 0 )
	    { errmsg = "no such group"; Unlock(plock); return(-1); }

	if ( postok == 0 && !force)
	{
	    errmsg = "posting disabled for group '";
	    errmsg += postgroup;
	    errmsg += "'";
	    G_conf.LogMessage(L_ERROR, "Group::Post(): %s", errmsg.c_str());

	    Unlock(plock);
	    return(-1);
	}

        if (*G_conf.SpamFilter())
	{
	    // Run spam filter to see if this is spam before we post...
	    FILE	*p;		// Pipe stream
	    int		status;		// Exit status
	    char	command[1024];	// Command to run

            snprintf(command, sizeof(command), "%s >/dev/null 2>/dev/null",
	             G_conf.SpamFilter());

            if ((p = popen(command, "w")) == NULL)
	    {
	        errmsg = "spam filter command failed to execute";
		Unlock(plock);
		return(-1);
	    }

            // Send the message to the filter...
	    for ( unsigned int t=0; t<head.size(); t++ )
		fprintf(p, "%s\n", head[t].c_str());

	    fputs("\n", p);

	    for ( unsigned int t=0; t<body.size(); t++ )
		fprintf(p, "%s\n", body[t].c_str());

            // Close the pipe to the command and get the exit status...
	    status = pclose(p);

	    if (status)
	    {
	        errmsg = "spam filter rejected message";
		Unlock(plock);
		return(-1);
	    }
	}

	// OPEN NEW ARTICLE
	unsigned long msgnum = 0;
	int fd;
	for ( msgnum=End() + 1; 1; msgnum++ )
	{
            // Build path to article
	    string path;

            // Using modulus dirs?
            if ( G_conf.MsgModDirs() ) 
            {
                path = Dirname();                       // "/path/fltk/general"
                path += "/";                            // "/path/fltk/general/"
                path += ultoa((msgnum/1000)*1000);      // "/path/fltk/general/1000"

                // See if modulus directory exists -- if not, create
                struct stat sbuf;
                if ( stat(path.c_str(), &sbuf) < 0 )
                {
                    if ( mkdir(path.c_str(), 0777) )
                    {
                        errmsg = "can't create modulus dir: mkdir(";
                        errmsg += path;
                        errmsg += ",0777): ";
                        errmsg += strerror(errno);
                        G_conf.LogMessage(L_ERROR, "Group::Post(): ", errmsg.c_str());

                        Unlock(plock);
                        return(-1);
                    }
                }
                else if (!S_ISDIR(sbuf.st_mode))
                {
                    errmsg = path;
                    errmsg += " is not a directory (expected a modulus dir)";
                    G_conf.LogMessage(L_ERROR, "Group::Post(): ", errmsg.c_str());

                    Unlock(plock);
                    return(-1);
                }
                path += "/";            // "/path/fltk/general/1000/"
                path += ultoa(msgnum);  // "/path/fltk/general/1000/1999"
            }
            else
            {
                path = Dirname();       // "/path/fltk/general"
                path += "/";            // "/path/fltk/general/"
                path += ultoa(msgnum);  // "/path/fltk/general/1999"
            }

	    if ((fd = open(path.c_str(), O_CREAT|O_EXCL|O_WRONLY, 0644)) == -1)
	    {
		if ( errno == EEXIST )
		    { continue; }		// try next article number

		errmsg = path;
		errmsg += ": ";
		errmsg += strerror(errno);
		G_conf.LogMessage(L_ERROR, "Group::Post(): ", errmsg.c_str());

		Unlock(plock);
		return(-1);
	    }
	    break;
	}

	// STRIP UNWANTED INFO FROM HEADER
	// HEADER NAMES ARE CASE INSENSITIVE: INTERNET DRAFT (Son of RFC1036)
	for ( unsigned int t=0; t<head.size(); t++ )
	{
            if ( !strncasecmp(head[t].c_str(), "Lines: ", 7) ||
	         !strncasecmp(head[t].c_str(), "Message-ID: ", 12) ||
	         !strncasecmp(head[t].c_str(), "Date: ", 6) ||
		 !strncasecmp(head[t].c_str(), "NNTP-Posting-Host: ", 19) )
	    {
	        head.erase( head.begin() + t);
		--t;
	    }
	}

	// HEADERS ADDED BY NEWS SERVER
	char misc[LINE_LEN];
	sprintf(misc, "Xref: %s %s:%lu",
	    (const char*)G_conf.ServerName(),
	    (const char*)postgroup,
	    (unsigned long)msgnum);
	head.push_back(misc);

	sprintf(misc, "Date: %s", 
	    (const char*)DateRFC822());
	head.push_back(misc);

	sprintf(misc, "NNTP-Posting-Host: %s", 
	    (const char*)remoteip_str);
	head.push_back(misc);

	sprintf(misc, "Message-ID: <%lu-%s@%s>",
	    (unsigned long)msgnum,
	    (const char*)postgroup,
	    (const char*)G_conf.ServerName());
	head.push_back(misc);

	sprintf(misc, "Lines: %u",
	    (unsigned int)body.size());
	head.push_back(misc);

	ReorderHeader(overview, head);

	// WRITE HEADER
	for ( unsigned int t=0; t<head.size(); t++ )
	{
	    write(fd, head[t].c_str(), head[t].length());
	    write(fd, "\n", 1);
	}

	// WRITE SEPARATOR
	write(fd, "\n", 1);

	// WRITE BODY
	for ( unsigned int t=0; t<body.size(); t++ )
	{
	    write(fd, body[t].c_str(), body[t].length());
	    write(fd, "\n", 1);
	}

	// FIRST MSG? START AT 1
	if ( Total() == 0 && Start() == 0 )
	    Start(1);

	// THIS IS NEW HIGHEST ARTICLE
	End(msgnum);
	Total(Total()+1);

	SaveInfo(0);
    }
    Unlock(plock);
    return(0);
}

// INTERACTIVELY PROMPT FOR NEW GROUP
//    Steps on group's internal variables.
//    It's advised parent created a throw-away instance.
//
int Group::NewGroup()
{
    int i;
    char s[256], in[256];

    fprintf(stderr, "Enter the new group's name (eg. 'electronics.ttl'):\n");
    if ( fgets(in, sizeof(in)-1, stdin) == NULL ) return(1);
    if ( sscanf(in, "%255s", s) != 1 ) return(1);
    name = s;
    struct stat buf;
    if ( stat(Dirname(), &buf) < 0 )
    {
	// NEWSGROUP DIR DOESNT EXIST, CREATE IT
        string cmd = "mkdir -m 0755 -p ";
	cmd += Dirname();
	G_conf.LogMessage(L_DEBUG, "Executing: %s", cmd.c_str());
	if ( system(cmd.c_str()) )
	    { return(1); }
        G_conf.LogMessage(L_ERROR, "OK");
    }

    fprintf(stderr, "\nIs posting to this group allowed? (Y/n):\n");
    if ( fgets(in, sizeof(in)-1, stdin) == NULL ) return(1);
    postok = ( in[0] == 'n' || in[0] == 'N' ) ? 0 : 1;

    if ( postok )
    {
        fprintf(stderr,
	    "\nMaximum #lines for postings, '0' if no max (default=1000):\n");
        if ( fgets(in, sizeof(in)-1, stdin) == NULL ) return(1);
        if ( sscanf(in, "%d", &i) != 1 ) i = 1000;
        postlimit = i;
    }
    else
        { postlimit = 0; }

    fprintf(stderr, "\nDescription of newsgroup in one short line, '-' if none "
		    "(eg. 'Discussion of TTL electronics'):\n");
    if ( fgets(in, sizeof(in)-1, stdin) == NULL ) return(1);
    in[strlen(in)-1] = 0;
    desc = in;

    fprintf(stderr, "\nAdministrator's email address for group, '-' if none "
		    "(default='-'):\n");
    if ( fgets(in, sizeof(in)-1, stdin) == NULL ) return(1);
    if ( sscanf(in, "%255s", s) != 1 ) strcpy(s, "-");
    creator = s;

    fprintf(stderr, "\nBCC all postings to these email address(es), "
		    "'-' if none (default='-')\n");
    if ( fgets(in, sizeof(in)-1, stdin) == NULL ) return(1);
    if ( sscanf(in, "%255s", s) != 1 ) strcpy(s, "-");
    ccpost = s;

    if ( ccpost != "-" )
    {
	fprintf(stderr,
	    "\nReply-To address for all emails, '-' if none (default='-')\n");
	if ( fgets(in, sizeof(in)-1, stdin) == NULL ) return(1);
	if ( sscanf(in, "%255s", s) != 1 ) strcpy(s, "-");
	replyto = s;

	fprintf(stderr,
	    "\nVoid email address, 'root' if none (default='root')\n");
	if ( fgets(in, sizeof(in)-1, stdin) == NULL ) return(1);
	if ( sscanf(in, "%255s", s) != 1 ) strcpy(s, "root");
	voidemail = s;
    }

    if ( SaveConfig() < 0 )
    {
	G_conf.LogMessage(L_ERROR, "ERROR: %s", Errmsg());
	return(1);
    }

    // CREATE AN INFO FILE, SO IT SHOWS UP IN 'LIST'
    //     This lets the admin be able to subscribe to the group
    //     and post the first test msg to it.
    //
    if ( BuildInfo(1) < 0 )
    {
	G_conf.LogMessage(L_ERROR, "ERROR: %s", Errmsg());
	return(1);
    }

    fprintf(stderr, "\n"
		    "----------\n"
		    "--- OK ---\n"
		    "----------\n"
		    "\n"
		    "    o New group %s was created.\n"
		    "\n"
		    "    o Use your news reader to post some test messages.\n"
		    "\n"
		    "    o You can edit %s/.config later\n"
		    "      to make changes.\n",
	    (const char*)Name(),
	    (const char*)Dirname());

    return(0);
}

// PARSE MESSAGE FOR HEADERS AND BODY
//    On error, returns -1, errmsg contains reason.
//
int Group::ParseArticle(string &msg, vector<string>&head, vector<string>&body)
{
    errmsg = "";

    // REQUIRED
    // --------
    //     From: <who@bar.com>
    //     Date: Wdy, DD Mon YY HH:MM:SS TIMEZONE (or: Wdy Mon DD HH:MM:SS YYYY)
    //     Newsgroups: news.group[,news.group] -- ignore invalid groups
    //     Subject: xx yy zz
    //     Message-ID: <unique@host.domain.com>
    //     Path: <recenthost>,<oldhost>,<olderhost>
    //
    // OPTIONAL
    // --------
    //     Control: <cmd [arg [arg..]]>
    //     Distribution: <state abbrev[,abbrev]>
    //     Organization: <misc text>
    //     References: <message-id-of-original-message>
    //     Lines: <#lines in body>
    //     Xref: <thishost> <newsgroup:article#> [newsgroup:article#]
    //
    // CONTROL MESSAGES
    // ----------------
    //     1) Subject: cmsg <cmd>
    //
    //     2) Control: <cmd>
    //
    //     3) Newsgroups: all.all.ctl
    //        Subject: <cmd>
    //
    int done = 0;
    int headflag = 1;
    string line;
    char *ss = (char*)msg.c_str();

    // PARSE OUT HEADERS AND BODY
    while ( !done )
    {
        switch (*ss)
        {
            case    0: done=1; continue;   // end of string? done
            case '\r': ++ss; continue;     // ignore \r's
            case '\n':
                // Complete line? append, reset
                if ( headflag ) { head.push_back(line); }	// append
                else            { body.push_back(line); }
                line = "";

                // End of header? <CR><LF> followed by <CR><LF>?
                if ( headflag && (*(ss+1) == '\r' && *(ss+2) == '\n' ) )
                {
                    headflag = 0;          // end of header
                    ss += 3;               // skip past blank line ending header
                }
                // End of header? <LF> followed by <LF>?
                else if ( headflag && (*(ss+1) == '\n' ) )
                {
                    headflag = 0;          // end of header
                    ss += 2;               // skip past blank line ending header
                } else ++ss;
                continue;
            default:
                line += *ss;
		++ss;
                continue;
        }
    }

    if ( G_conf.LogLevel() == L_DEBUG )
    {
        uint t;
        for (t=0; t<head.size(); t++)
            { G_conf.LogMessage(L_DEBUG, "ParseArticle: --- head[%03d]: '%s'\n", t, head[t].c_str()); }
        for (t=0; t<body.size(); t++)
            { G_conf.LogMessage(L_DEBUG, "ParseArticle: --- body[%03d]: '%s'\n", t, body[t].c_str()); }
    }

    return(0);
}

// UPDATE THE "Path:" HEADER
void Group::UpdatePath(vector<string>&header)
{
    const char *pathstr = "Path:";
    int         pathlen = strlen(pathstr);
    string      hostname = G_conf.ServerName();

    // ADD TO EXISTING PATH (IF EXISTS)
    int found = 0;
    for ( uint t=0; t<header.size(); t++ )
    {
        if ( strncasecmp(header[t].c_str(), pathstr, pathlen) == 0 )
	{
	    string info = hostname + ", ";
	    if ( header[t].c_str()[5] != ' ' )
	    {
		// "Path:lasthost.." -> "Path: lasthost.."
	        header[t].insert(5," ");
	    }

	    // "Path: lasthost.." -> "Path: thishost, lasthost.."
	    header[t].insert(6, info);
	    found = 1;
	}
    }

    // CREATE NEW PATH (IF NONE EXISTS)
    if ( ! found )
    {
	string newpath = "Path: " + hostname;
	header.push_back(newpath);
    }
}

// IS THIS A VALID GROUP?
//    Check for the existence of a .config file.
//
int Group::IsValidGroup()
{
    // Is this a valid group?
    string cfgpath = Dirname();
    cfgpath += "/.config";
    struct stat sbuf;
    if ( stat(cfgpath.c_str(), &sbuf) < 0 ) return(0);
    return(1);
}
