//
// Subs.H -- Misc subroutines
//
// Copyright 2017 Greg Ercolano
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

#ifndef SUBS_H
#define SUBS_H

#include "everything.H"

string ultos_SUBS(ulong num);
void TruncateCrlf_SUBS(char *s);
void ReplaceString_SUBS(char *s, char from, char to);

#endif /*!SUBS_H*/

