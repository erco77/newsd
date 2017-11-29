//
// Article.H -- Manage newsgroup articles
// 1.00 erco@3dsite.com 12/16/02
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

#ifndef ARTICLE_H
#define ARTICLE_H

#include "everything.H"

class Article
{
    string group;		// group name
    string filename;		// full path to the article
    ulong  number;		// article number
    int    valid;		// article is valid (0=invalid)

    // commonly accessed header info
    string from;		// From: field
    string date;		// Date: field
    string messageid;		// Message-ID: field
    string subject;		// Subject: field
    string references;		// References: field
    string xref;		// Xref: field
    int    lines;		// Lines: field

    string errmsg;		// error message

    void _Copy(const Article& o)
    {
        group      = o.group;
        filename   = o.filename;
        number     = o.number;
	valid      = o.valid;
        from       = o.from;
        date       = o.date;
        messageid  = o.messageid;
        subject    = o.subject;
        references = o.references;
	xref       = o.xref;
	lines      = o.lines;
	errmsg     = o.errmsg;
    }

    int    _ParseHeader(string& key, string& val);

public:

    Article() 
    {
        group      = "";
        filename   = "";
	number     = 0; 
	valid      = 0;
	from       = "";
	date       = "";
	messageid  = "";
	subject    = "";
	references = "";
	xref       = ""; 
	lines      = 0;
	errmsg     = "";
    }

    ~Article() { } 

    Article(const Article& o)
	{ _Copy(o); }

    Article& operator=(const Article& o)
	{ _Copy(o); return(*this); }

    int            IsValid()    { return(valid); }
    const char    *Group()      { return(group.c_str()); }
    const char    *Filename()   { return(filename.c_str()); }
    const char    *From()       { return(from.c_str()); }
    const char    *Date()       { return(date.c_str()); }
    const char    *MessageID()  { return(messageid.c_str()); }
    const char    *Subject()    { return(subject.c_str()); }
    const char    *Xref()       { return(xref.c_str()); }
    const char    *References() { return(references.c_str()); }
    int            Lines()      { return(lines); }
    ulong          Number()     { return(number); }
    const char    *Errmsg()     { return(errmsg.c_str()); }

    // load info for group/article
    int Load(const char *group, ulong num);

    // load info for article
    int Load(ulong num);

    // send article to remote via fd
    int SendArticle(int fd, int head=1, int body=1);

    // send article head only
    int SendHead(int fd);

    // send article body only
    int SendBody(int fd);
    string Overview(const char *overview[]);

    // Return path to specified article in specified group
    static string GetArticlePath(const char* group, ulong artnum);
};

#endif /*!ARTICLE_H*/
