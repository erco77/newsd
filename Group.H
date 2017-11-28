//
// Group.H -- Manage newsgroup groups
//
// Copyright 2003 Michael Sweet
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

#ifndef GROUP_H
#define GROUP_H

#include "everything.H"

class Group
{
    // ".info" FILE DATA
    unsigned long start, end, total;		// start/end/total articles

    // ".config" FILE DATA
    string desc;			// group description
    string creator;			// group creator email
    string name;			// group name
    string ccpost;			// email address to cc all postings to
    string replyto;			// optional replyto address
    string voidemail;			// bit bucket
    int postok,				// allow posting to this group
	postlimit;			// posting line limit

    string dirname;			// directory name
    time_t ctime;			// creation time
    int valid;				// is group valid? 0=invalid
    string errmsg;			// error message
    char datebuf[80];			// RFC822 date buffer

    // GENERAL LOCKING
    //    Write lock whenever posting or saving info file.
    //    Read lock before accessing info file.
    //
    int  WriteLock();
    int  ReadLock();
    void Unlock(int fd);

    int WriteString(FILE *fp, const char *buf);
    int BuildInfo(int dolock = 1);
    int LoadInfo(int dolock = 1);
    int SaveInfo(int dolock = 1);
    int LoadConfig(int dolock = 1);
    int SaveConfig();

    void ReorderHeader(const char*overview[], vector<string>& head);

    const char *DateRFC822();

    void _Copy(const Group&o)
    {
        start     = o.start;
        end       = o.end;
        total     = o.total;
	desc      = o.desc;
	creator   = o.creator;
	name      = o.name;
	ccpost    = o.ccpost;
	replyto   = o.replyto;
	voidemail = o.voidemail;
	postok    = o.postok;
	postlimit = o.postlimit;
	ctime     = o.ctime;
	valid     = o.valid;
    }

public:
    Group()
    {
        start = end = total = 0;
	desc = "";
	creator = "news";
	name = "";
	ccpost = "-";
	replyto = "-";
	voidemail = "root";
	postok = 0;
	postlimit = 0;
	ctime = 0;
	valid = 0;
    }

    ~Group()
	{ }

    Group(const Group&o)
    {
        _Copy(o);
    }
    
    Group& operator=(const Group&o)
    {
        _Copy(o);
	return(*this);
    }

    // ACCESSORS
    unsigned long Start()       { return(start); }
    unsigned long End()         { return(end); }
    unsigned long Total()       { return(total); }
    int           PostOK()      { return(postok); }
    int           PostLimit()   { return(postlimit); }
    long          Ctime()       { return(ctime); }
    int           IsValid()     { return(valid); }
    int           IsCCPost()    { return(ccpost == "-" ? 0 : 1); }
    int           IsReplyTo()   { return(replyto == "-" ? 0 : 1); }
    const char   *Description() { return(desc.c_str()); }
    const char   *Creator()     { return(creator.c_str()); }
    const char   *Name()        { return(name.c_str()); }
    const char   *CCPost()      { return(ccpost.c_str()); }
    const char   *ReplyTo()     { return(replyto.c_str()); }
    const char   *VoidEmail()   { return(voidemail.c_str()); }
    const char   *Errmsg()      { return(errmsg.c_str()); }

    void Start(unsigned long val)     { start = val; }
    void End(unsigned long val)       { end = val; }
    void Total(unsigned long val)     { total = val; }
    void Name(const char*val) { name = val; }

    int Load(const char *group, int dolock = 1);
    int WriteInfo(int fd);
    int FindArticleByMessageID(const char *find_messageid,
                               unsigned long &articlenum);
    int Post(const char*overview[], vector<string> &head, 
    	     vector<string> &body, const char *remoteip_str, bool force=false);
    const char *Dirname();

    int NewGroup();
    int ParseArticle(string &msg, vector<string>&head, vector<string>&body);
    void UpdatePath(vector<string>&head);
    int IsValidGroup();
};

#endif /*!GROUP_H*/
