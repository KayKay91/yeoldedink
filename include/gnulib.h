/* xalloc.h -- malloc with out-of-memory checking

   Copyright (C) 1990-2000, 2003-2004, 2006-2023 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

//ye: gnulib mem stuff I didn't want to rewrite

static inline void *
xmalloc (size_t n)
{
  void *p = malloc (n);
  //if (!p && n != 0)
  //  xalloc_die ();
  return p;
}

static inline void *
xrealloc (void *p, size_t n)
{
  p = realloc (p, n);
  //if (!p && n != 0)
  //  xalloc_die ();
  return p;
}

static inline void *
xnmalloc (size_t n, size_t s)
{
//if (xalloc_oversized (n, s))
//xalloc_die ();
return xmalloc (n * s);
}

# define XNMALLOC(n, t) ((t *) (sizeof (t) == 1 ? xmalloc (n) : xnmalloc (n, sizeof (t))))

static inline void *
xzalloc (size_t s)
{
  return memset (xmalloc (s), 0, s);
}

# define XZALLOC(t) ((t *) xzalloc (sizeof (t)))