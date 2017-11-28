//
// everything.H -- everything in the way of macros and includes
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

#ifndef EVERYTHING_H
#define EVERYTHING_H

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>	// waitpid
#include <sys/file.h>	// flock()
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include "VERSION.H"
using namespace std;	// for clang, must appear after std #include's 

// Constants...
#define LINE_LEN	4096
#define GROUP_MAX	1024
#define FIELD_MAX	1024

#define AUTH_FAIL    0		// no auth supplied yet
#define AUTH_READ    1		// allow reading
#define AUTH_POST    2		// allow posting
#define AUTH_ALL     3		// allow read + post
#define AUTH_NOAUTH  4		// no auth needed

#include "Configuration.H"

#endif /*!EVERYTHING_H*/
