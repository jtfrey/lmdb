/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * fscanln.h
 *
 * A file scanner attaches to a C file i/o stream and processes the
 * stream one line at a time.  A regular expression can be set to filter
 * which lines are returned.  Line-continuation (last character is a
 * backslash) is removed before lines are returned.
 *
 */
 
#ifndef __FSCANLN_H__
#define __FSCANLN_H__

#include "config.h"
#include <regex.h>

/*!
	@typedef fscanln_ref
	Type of an opaque reference to an fscanln object.
*/
typedef const struct _fscanln * fscanln_ref;

/*!
	@function fscanln_create
	Create a new file scanner that wraps the given FILE pointer.  Passing true for
	the should_close_on_dealloc argument requests that the file scanner should close
	the file when it is destroyed.
	
	Returns NULL in case of any error.
*/
fscanln_ref fscanln_create(FILE *fptr, bool should_close_on_dealloc);
/*!
	@function fscanln_create_with_file
	Create a new file scanner by opening the given path for reading.  If the path cannot
	be opened, NULL is returned.
*/
fscanln_ref fscanln_create_with_file(const char *path);
/*!
	@function fscanln_create_with_command
	Create a new file scanner that executes the given command in a shell and reads its
	output.  Returns NULL in case of error.
*/
fscanln_ref fscanln_create_with_command(const char *command);
/*!
	@function fscanln_create_with_execve
	Create a new file scanner that forks and executes the program at path with the
	given argument and environment lists (a'la execve()).  If should_merge_stderr is
	true, stdout and stderr for the forked process will be mereged and read by the
	scanner.  Returns NULL in case of error.
*/
fscanln_ref fscanln_create_with_execve(const char *path, char* const *argv, char* const *envp, bool should_merge_stderr);
/*!
	@function fscanln_retain
	Return a reference to a file scan object.
*/
fscanln_ref fscanln_retain(fscanln_ref f);
/*!
	@function fscanln_release
  Release a reference to a file scan object.
*/
void fscanln_release(fscanln_ref f);
/*!
	@function fscanln_unset_line_regex
	Remove the current line-matching regular expression from the file scanner.
*/
void fscanln_unset_line_regex(fscanln_ref f);
/*!
	@function fscanln_set_line_regex
	Set a line-matching regular expression on the file scanner, f.  Attempts to compile
	the regex string with the regex_flags (using regcomp()).  Allocates room for up to
	sub_match_max (plus one) pattern substrings.
	
	Returns false if the regex string could not be compiled or substring match space
	could not be allocated.
*/
bool fscanln_set_line_regex(fscanln_ref f, const char *regex, int regex_flags, int sub_match_max);
/*!
	@function fscanln_get_is_regex_enabled
	Returns true if a regular expression has been set on the file scanner and it
	is currently enabled for line filtering.
*/
bool fscanln_get_is_regex_enabled(fscanln_ref f);
/*!
	@function fscanln_set_is_regex_enabled
	If is_regex_enabled is false, subsequent calls to fscanln_get_line() will not
	filter lines using the regular expression.  The filtering can later be reenabled
	by passing is_regex_enabled of true.  (This avoids the overhead of repreatedly
	recompiling and freeing the regular expression itself.)
*/
void fscanln_set_is_regex_enabled(fscanln_ref f, bool is_regex_enabled);
/*!
	@function fscanln_get_line_number
	Returns the index (one-based) of the last line scanned.
*/
unsigned long fscanln_get_line_number(fscanln_ref f);
/*!
	@function fscanln_get_line
	Attempts to read a line from the stream associated with the file scanner.  If
	a regular expression filter is enabled, any non-matching lines are discarded.
	
	If out_line_ptr is not NULL, the pointer to which it points is set to a pointer
	to a buffer containing the line read.  The line is NUL-terminated and includes the
	terminating newline character.  The buffer is owned by the file scanner object and
	should not be modified and will be invalidated the next time the
	fscanln_get_line() function is called.
*/
bool fscanln_get_line(fscanln_ref f, const char **out_line_ptr, size_t *out_line_len);
/*!
	@function fscanln_get_sub_match_string
	If a regular expression filter is set and enabled, then after a successful call to
	fscanln_get_line()  any parenthesized sub-matches can be retrieved using this function.
	The zeroeth match is the entire matched string; the first (sub_match_idx of 1) match
	corresponds to the left-most parenthetical sub-match; etc.
	
	Returned strings are owned by the file scanner and will be invalidatd the next time
	the fscanln_get_line() is called.
	
	Returns NULL in case of any error (or if no line has been read).
*/
const char* fscanln_get_sub_match_string(fscanln_ref f, int sub_match_idx);

#endif /* __FSCANLN_H__ */
