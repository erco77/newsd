//
// Server.H -- News Server Class
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

#ifndef SERVER_H
#define SERVER_H

#include "everything.H"
#include "Group.H"
#include "Article.H"

class Server
{
    // Server-specific data...
    int sock,		// listener socket (accept())
        msgsock;	// transaction socket (read/write)
    char *buf;		// line buffer (internal)
    struct sockaddr_in sin;

    Group group;	// current group
    Article article;	// current article
    string errmsg;

public:

    Server()
    {
        sock = msgsock = -1;
	buf = (char*)malloc(LINE_LEN);
    }

    ~Server()
    {
        if ( msgsock != -1 )
	    { close(msgsock); msgsock = -1; }
        if ( sock != -1 )
	    { close(sock); sock = -1; }
	if ( buf )
	    { free(buf); buf = 0; }
    }

    const char *Errmsg() { return(errmsg.c_str()); }
    int MsgSock() { return(msgsock); }
    int Sock() { return(sock); }
    const char *GetRemoteIPStr()
	{ return(inet_ntoa(sin.sin_addr)); }

    int Send(const char *msg);
    int IsAllowed(int op);
    int ValidGroup(const char *groupname);
    int NewGroup(const char *the_group);

    // TCP CONNECTIONS
    int Listen();
    int Accept(ostringstream& remote_msg);
    int CommandLoop(const char *overview[]);
};

#endif /*!SERVER_H*/
