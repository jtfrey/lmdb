/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * lmconfig.c
 *
 * Program configuration parsing and a few helper functions.
 *
 */

#include "util_fns.h"
#include "lmlog.h"

#include <ctype.h>
#include <sys/stat.h>

//

bool
file_exists(
  const char    *path
)
{
  struct stat   finfo;

  if ( stat(path, &finfo) == 0 ) {
    if ( S_ISREG(finfo.st_mode) ) return true;
  }
  return false;
}

//

bool
directory_exists(
  const char    *path
)
{
  struct stat   finfo;

  if ( stat(path, &finfo) == 0 ) {
    if ( S_ISDIR(finfo.st_mode) ) return true;
  }
  return false;
}

//

const char*
strappendm(
  const char *s1,
  ...
)
{
  const char    *s;
  char          *out = NULL;
  int            s1_len = ( s1 ? strlen(s1) : 0 );
  size_t        l = s1_len;
  va_list       vargs;

  va_start(vargs, s1);
  while ( (s = va_arg(vargs, const char*)) ) {
    l += strlen(s);
  }
  va_end(vargs);

  out = realloc((char*)s1, l + 1);
  if ( out ) {
    char      *dst = out + s1_len;

    va_start(vargs, s1);
    while ( (s = va_arg(vargs, const char*)) ) {
      dst = stpcpy(dst, s);
    }
    va_end(vargs);
  } else {
    free((void*)s1);
    out = NULL;
  }
  return out;
}

//

const char*
strcatm(
  const char *s1,
  ...
)
{
  const char    *s = s1;
  size_t        l = 0;
  char          *out = NULL;
  va_list       vargs;

  va_start(vargs, s1);
  while ( s ) {
    l += strlen(s);
    s = va_arg(vargs, const char*);
  }
  va_end(vargs);

  out = malloc(l + 1);
  if ( out ) {
    char      *dst = out;

    va_start(vargs, s1);
    s = s1;
    while ( s ) {
      dst = stpcpy(dst, s);
      s = va_arg(vargs, const char*);
    }
    va_end(vargs);
  } else {
    lmlogf(lmlog_level_warn, "strcatm: failed to allocate space for concatenated strings (errno = %d)", errno);
  }
  return out;
}

//

const char*
strdcatm(
  const char  *delim,
  ...
)
{
  const char    *s;
  size_t        l = 0, delim_len = strlen(delim);
  char          *out = NULL;
  va_list       vargs;

  va_start(vargs, delim);
  while ( (s = va_arg(vargs, const char*)) ) {
    if ( l ) l += delim_len;
    l += strlen(s);
  }
  va_end(vargs);

  out = malloc(l + 1);
  if ( out ) {
    char      *dst = out;

    va_start(vargs, delim);
    while ( ( s = va_arg(vargs, const char*)) ) {
      if ( dst > out ) dst = stpcpy(dst, delim);
      dst = stpcpy(dst, s);
    }
    va_end(vargs);
  } else {
    lmlogf(lmlog_level_warn, "strdcatm: failed to allocate space for concatenated strings (errno = %d)", errno);
  }
  return out;
}

//

const char*
strcatf(
	const char		*format,
	...
)
{
	const char    *s;
  size_t        l = 0;
  char          *out = NULL;
  va_list       vargs;

  va_start(vargs, format);
  l = vsnprintf(NULL, 0, format, vargs);
  va_end(vargs);

  out = malloc(l + 1);
  if ( out ) {
    va_start(vargs, format);
    vsnprintf((char*)out, l + 1, format, vargs);
    va_end(vargs);
  } else {
    lmlogf(lmlog_level_warn, "strcatf: failed to allocate space for string (errno = %d)", errno);
  }
  return out;
}

//

int
str_next_word(
  const char*   *s,
  mempool_ref   pool,
  const char*   *word
)
{
  char          quote = '\0';
  const char    *start = *s, *end;
  size_t        word_len = 0;

  while ( *start && isspace(*start) ) start++;

  if ( *start ) {
    char        prev_c = '\0';

    switch ( *start ) {
      case '"':
      case '\'':
        quote = *start++;
        break;
    }
    end = start;
    while ( *end ) {
      if ( quote && (*end == quote) ) {
        if (  prev_c != '\\' ) break;
        word_len--;
      }
      else if ( ! quote && isspace(*end) ) break;
      prev_c = *end++;
      word_len++;
    }
    if ( quote && (*end != quote) ) {
      lmlog(lmlog_level_error, "no terminating quote on quoted string ");
      return str_next_word_error;
    }
    if ( end > start ) {
      if ( pool && word ) {
        char        *p = mempool_alloc_bytes(pool, word_len + 1);

        if ( p ) {
          char      *dst = p;

          prev_c = '\0';
          while ( start < end ) {
            if ( quote && (*start == quote) ) {
              if ( prev_c == '\\' ) dst--;
            }
            prev_c = *start;
            *dst++ = *start++;
          }
          *dst = '\0';
          *word = p;

        } else {
          lmlog(lmlog_level_error, "unable to allocate space for next word ");
          return str_next_word_error;
        }
      }
      if ( quote ) end++;
      *s = end;

      return str_next_word_ok;
    }
  }
  return str_next_word_none;
}
