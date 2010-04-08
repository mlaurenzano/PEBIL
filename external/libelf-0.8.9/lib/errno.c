/*
errno.c - implementation of the elf_errno(3) function.
Copyright (C) 1995 - 1998 Michael Riepe

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <private.h>

#ifndef lint
static const char rcsid[] = "@(#) $Id: errno.c,v 1.6 2005/05/21 15:39:21 michael Exp $";
#endif /* lint */

int
elf_errno(void) {
    int tmp = _elf_errno;

    _elf_errno = 0;
    return tmp;
}
