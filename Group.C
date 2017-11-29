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

// RETURN STRING VERSION OF UNSIGNED LONG
static string ultos(ulong num)
{
    ostringstream buffer;
    buffer << num;
    return(buffer.str());
}

// CONVERT CURRENT GROUP NAME TO A DIRECTORY NAME
//    Returns the full path to the current group set by Name(),
//    updating the internal member 'dirname' in the process.
//
//    Example: if name="rush.general", then on return 'dirname'
//    will be "/var/spool/newsd/rush/general", and this value is
//    returned as a c_str().
//
const char *Group::Dirname()
{
    string grpdir = name;			          // e.g. "rush.general"..
    std::replace(grpdir.begin(), grpdir.end(), '.', '/'); // ..now "rush/general"
    dirname = G_conf.SpoolDir();
    dirname += "/";
    dirname += grpdir;
    return dirname.c_str();	// safe, since this is an internal member
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
	WriteString(fp, ultos(start));
	WriteString(fp, "\n");

	WriteString(fp, "end         ");
	WriteString(fp, ultos(end));
	WriteString(fp, "\n");

	WriteString(fp, "total       ");
	WriteString(fp, ultos(total));
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
    ulong temp;

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
	WriteString(fp, desc);
	WriteString(fp, "\n");

	WriteString(fp, "creator     ");
	WriteString(fp, creator);
	WriteString(fp, "\n");

	WriteString(fp, "postok      ");
	WriteString(fp, ultos((ulong)postok));
	WriteString(fp, "\n");

	WriteString(fp, "postlimit   ");
	WriteString(fp, ultos((ulong)postlimit));
	WriteString(fp, "\n");

	WriteString(fp, "ccpost      ");
	WriteString(fp, ccpost);
	WriteString(fp, "\n");

	WriteString(fp, "replyto     ");
	WriteString(fp, replyto);
	WriteString(fp, "\n");

	WriteString(fp, "voidemail   ");
	WriteString(fp, voidemail);
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
int Group::LoadInfo(const char *group_name, int dolock)
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
	G_conf.LogMessage(L_ERROR, "Group::LoadInfo(): %s", errmsg.c_str());
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

// WRITE NULL TERMINATED STRING TO FD
int Group::WriteString(FILE *fp, const string& buf)
{
    if ( fwrite(buf.c_str(), 1, buf.size(), fp) != buf.size())
    {
        errmsg = "write error";
	G_conf.LogMessage(L_ERROR, "Group::WriteString(): %s", errmsg.c_str());
	return(-1);
    }
    return(0);
}

// RETURN ABSOLUTE PATHNAME TO ARTICLE#
//     Returns path to artcile# in 'artnum'.
//    
string Group::GetPathForArticle(ulong artnum)
{
    string apath = Dirname();		// path to the article
    apath += "/";

    // Using modulus dirs?
    if ( G_conf.MsgModDirs() )
    {
        // Include extra directory name..
        apath += ultos((artnum/1000)*1000);
	apath += "/";
	apath += ultos(artnum);		// "/path/fltk/general/1000/1234"
    }
    else
    {
       apath += ultos(artnum);		// "/path/fltk/general/1234"
    }

    //DEBUG G_conf.LogMessage(L_DEBUG, "Group::PathToArticle(%ld): %s", artnum, apath.c_str());

    return apath;
}

// RETURN THE "Message-ID" FOR ARTICLE IN A GROUP
//     'group' is the group to search (e.g. "fltk.general"),
//     'artnum' is the article# to get Message-ID for.
//     The returned value includes the angle brackets.
//
// Returns:
//     0 on success, 'msgid' has the Message-ID value, e.g. "<some_string>"
//    -1 if not found
//
int Group::GetMessageID(ulong artnum, string& msgid)
{
    string apath = GetPathForArticle(artnum);

    FILE *fp;
    int ret = -1;			// assume failure unless successful
    char line[1024];
    if ((fp = fopen(apath.c_str(), "r")) != NULL)
    {
	// Parse article until Message-ID: field found, or until EOH
	while (fgets(line, sizeof(line), fp) != NULL)
	{
	    if ( line[0] == '\n' ) { ret = -1; break; }      // eoh? not found..
	    if ( strncasecmp(line, "Message-ID:", 11) == 0)  // Message-ID: <xyz>\n?
	    {
		char *s = line + 11;	                     // " <xyz>\n"
		while ( *s && isspace(*s & 255)) s++;        // Skip leading white, "<xyz>\n"
		const char *id = s;			     // "<xyz>\n"
		while ( *s )                                 // Remove trailing \n
		    if ( *s == '\n' ) { *s = 0; break; }
		    else s++;
		msgid = id;			             // "<xyz>"
		ret = 0;                                     // success
		break;
	    }
	}
	fclose(fp);
    }
    return ret;
}

// FIND ARTICLE NUMBER GIVEN A MESSAGEID
//     This does a linear lookup; can take a while for group with many articles.
//     Search starts at most recent article ( End() ) back to beginning of time,
//     since people are usually interested in recent articles, not old ones.
//
// Returns:
//     0 on success, 'number' contains the article#
//    -1 on failure, errmsg has reason to return to user (cc'ed to log)
//
int Group::FindArticleByMessageID(const char *ccp_msgid, ulong &number)
{
    string msgid = ccp_msgid;
    // Do a slow lookup...
    // Walk all articles until we find matching Message-ID
    string art_msgid;
    ulong count = 0;
    for (ulong artnum = End(); artnum >= Start(); artnum-- )
    {
        ++count;
	if ( GetMessageID(artnum, art_msgid) == 0 )     // found msgid value?
	{
	    if ( art_msgid == msgid )	                // matches search?
	    {
		G_conf.LogMessage(L_INFO, "Message-ID '%s' found after searching "
		                          "%ld articles", msgid.c_str(), count);
	        number = artnum;	// save article#, done
		return 0;
	    }
	}
    }

    // Return an error if not found..
    errmsg = string("Message-ID not found: ") + msgid;
    G_conf.LogMessage(L_ERROR, "Message-ID '%s' not found after searching "
                               "%ld articles", msgid.c_str(), count);
    return(-1);
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
		bool force_post)
{
    string postgroup;
    if ( GetHeaderValue(head, "Newsgroups:", postgroup) == -1 )
	{ errmsg = "article has no 'Newsgroups' field"; return(-1); }

    // LOCK FOR POSTING
    Name(postgroup);
    int plock = WriteLock();
    {
	// LOAD INFO FOR THIS GROUP
	if ( LoadInfo(postgroup, 0) < 0 )
	    { errmsg = "no such group"; Unlock(plock); return(-1); }

	if ( postok == 0 && !force_post)
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
	ulong msgnum = 0;
	int fd;
	for ( msgnum=End() + 1; 1; msgnum++ )
	{
            // Build path to article
	    string path;

            // Using modulus dirs?
            if ( G_conf.MsgModDirs() ) 
            {
                path = Dirname();                    // "/path/fltk/general"
                path += "/";                         // "/path/fltk/general/"
                path += ultos((msgnum/1000)*1000);   // "/path/fltk/general/1000"

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
                path += "/";           // "/path/fltk/general/1000/"
                path += ultos(msgnum); // "/path/fltk/general/1000/1999"
            }
            else
            {
                path = Dirname();      // "/path/fltk/general"
                path += "/";           // "/path/fltk/general/"
                path += ultos(msgnum); // "/path/fltk/general/1999"
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
	{
	    int index;

	    // Remove Date: (if any)
	    if ( ( index = GetHeaderIndex(head, "Date:") ) != -1 )
	        { head.erase( head.begin() + index); }

	    // Remove NNTP-Posting-Host: (if any)
	    if ( ( index = GetHeaderIndex(head, "NNTP-Posting-Host:") ) != -1 )
	        { head.erase( head.begin() + index); }
	}

	// HEADERS ADDED BY NEWS SERVER
	{
	    ostringstream os;
	    os << "Xref: " << G_conf.ServerName()
	       << " " << postgroup << ":" << msgnum;
	    head.push_back(os.str());
	}
	{
	    ostringstream os;
	    os << "Date: " << DateRFC822();
	    head.push_back(os.str());
	}
	{
	    ostringstream os;
	    os << "NNTP-Posting-Host: " << remoteip_str;
	    head.push_back(os.str());
	}

        // Only set Message-ID: if client /didn't/ specify it
	string value;
	if ( GetHeaderValue(head, "Message-ID:", value) == -1 )
        {
	    ostringstream os;
	    os << "Message-ID: <" << msgnum << "-" << postgroup
	       << "@" << G_conf.ServerName() << ">";
	    head.push_back(os.str());
        }

        // Only set Lines: if client /didn't/ specify it
	if ( GetHeaderValue(head, "Lines:", value) == -1 )
        {
	    ostringstream os;
	    os << "Lines: " << body.size();
            head.push_back(os.str());
        }

	// DONT DO THIS -- MESSES UP MULTILINE FIELDS
	// ReorderHeader(overview, head);

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
//    Writes out a new group config file.
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
        string cmd = "/bin/mkdir -m 0755 -p ";
	cmd += Dirname();
	G_conf.LogMessage(L_DEBUG, "Executing: %s", cmd.c_str());
	if ( system(cmd.c_str()) ) { return(1); }
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
    //     Subject: a message with a long multiline
    //      text subject line
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

// RETURN VALUE FOR SPECIFIED HEADER
//    'fieldname' is a header field name, e.g. "Message-ID:".
//    The trailing : in 'fieldname' must be included.
//
//    Returns 0 on success with 'value' continaing the field's value.
//    Returns -1 on failure with 'value' unchanged.
//
//    The search for 'fieldname' will be case insensitive (RFC 1036).
//    Example: news clients can generate either "Message-ID" or "Message-Id".
//
int Group::GetHeaderValue(vector<string> &header,
			  const char *fieldname,
			  string& value) const
{
    int len = strlen(fieldname);
    for ( uint t=0; t<header.size(); t++ )
    {
        if ( strncasecmp(header[t].c_str(), fieldname, len) == 0 )
        {
            const char *vp = header[t].c_str() + len;     // "Message-ID: <foo>" -> " <foo>"
            while ( *vp && isspace(*vp & 255) ) ++vp;     // " <foo>" -> "<foo>"
	    // printf("DEBUG: found '%s', value='%s'\n", fieldname, vp);
	    value = vp;
            return 0;
        }
    }
    // printf("DEBUG: NOT FOUND '%s'\n", fieldname);
    return -1;
}

// RETURN head[] INDEX FOR SPECIFIED HEADER
//    'fieldname' is a header field name, e.g. "Message-ID:".
//    The trailing : must be included.
//    Returns index into head[] vector of matching field, or -1 if not found.
//
//    The search for 'fieldname' will be case insensitive (RFC 1036).
//    Example: news clients can generate either "Message-ID" or "Message-Id".
//
int Group::GetHeaderIndex(vector<string> &header, const char *fieldname) const
{
    int len = strlen(fieldname);
    for ( uint t=0; t<header.size(); t++ )
    {
        if ( strncasecmp(header[t].c_str(), fieldname, len) == 0 )
	{
	    // printf("DEBUG: found '%s' at index %u\n", fieldname, t);
	    return int(t);
	}
    }
    // printf("DEBUG: NOT FOUND '%s'\n", fieldname);
    return -1;
}
