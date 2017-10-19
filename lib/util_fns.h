/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * util_fns.h
 *
 * Shared and per-utility program configuration options.  An API for
 * parsing configuration files and command line options is present.
 *
 */
 
#ifndef __UTIL_FNS_H__
#define __UTIL_FNS_H__

#include "config.h"
#include "mempool.h"

/*!
  @function file_exists
  Returns true if a regular file exists at the given path.
*/
bool file_exists(const char *path);

/*!
  @function directory_exists
  Returns true if a directory exists at the given path.
*/
bool directory_exists(const char *path);

/*!
  @function strappendm
  Attempt to append an arbitrary number of strings to s1, resizing
  s1 to accomodate them all.  The list of strings must be terminated
  by a NULL pointer:
  
    const char    *s2 = strappendm(s1, " another few", " words to", " add.", NULL);
  
  Having s1 of NULL is not an error, it simply behaves like the
  strcatm() function in that case.
    
  The caller owns the returned non-NULL pointer and is responsible for
  free'ing it.
*/
const char* strappendm(const char *s1, ...);

/*!
  @function strcatm
  Allocate and initialize a memory buffer to contain the list of strings
  passed to this function, concatenated in sequence.  The list must be
  terminated by a NULL pointer:
  
    const char    *s = strcatm("/etc", "/init.d", "/httpd", NULL);
  
  The caller owns the returned non-NULL pointer and is responsible for
  free'ing it.
*/
const char* strcatm(const char *s1, ...);

/*!
  @function strdcatm
  Allocate and initialize a memory buffer to contain the list of strings
  passed to this function, concatenated in sequence with the string delim
  occuring between each string.  The list must be terminated by a NULL
  pointer:
  
    const char    *s = strdcatm("/", "/etc", "init.d", "httpd", NULL);
  
  The caller owns the returned non-NULL pointer and is responsible for
  free'ing it.
*/
const char* strdcatm(const char *delim, ...);

/*!
	@function strcatf
	Allocate and initialize a memory buffer that contains the string
	that results from formatting a variable list of arguments according
	to the given format string.  Basically, use vsnprintf() to determine
	the necessary size of the string and then malloc() a buffer to
	hold it, fill-it in, then return it.
	
	The caller owns the returned non-NULL pointer and is responsible for
	free'ing it.
*/
const char*	strcatf(const char *format, ...);

/*!
  @function is_eol
  Returns true if the character argument c is a NUL, CR, or NL
  character.
*/
static inline bool is_eol(char c)
{
  return ( c == '\0' || c == '\n' || c == '\r' ) ? true : false;
}

/*!
  @function is_not_eol
  Returns true if the character argument c is NOT a NUL, CR, or
  NL character.
*/
static inline bool is_not_eol(char c)
{
  return ( c == '\0' || c == '\n' || c == '\r' ) ? false : true;
}

/*!
  @enum word extraction response codes
  Enumerates the response codes from the str_next_word() function.
  @constant str_next_word_ok
    a word was found; memory was allocated from pool to hold the
    word and that pointer was assigned to *word
  @constant str_next_word_none
    no word was found (end of the string, typically)
  @constant str_next_word_error
    an error occurred while attempting to find the next word;
    typically, memory could not be allocated from the pool
*/
enum {
  str_next_word_ok,
  str_next_word_none,
  str_next_word_error
};

/*!
  @function str_next_word
  The C string at *s is analyzed:
  
    - all leading whitespace is skipped
    - the range of all non-whitespace characters is found
    - storage for the non-whitespace characters (plus a NUL
      terminator) is allocated from pool and the "word" is
      copied into it
    - *s is updated to the first character beyond the word
    - *word is set to the pointer allocated from pool (containing
      a copy of the word)
  
  Returns a response code indicating success or failure.
*/
int str_next_word(const char* *s, mempool_ref pool, const char* *word);

#endif /* __UTIL_FNS_H__ */
