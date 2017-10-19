/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * lmlog.h
 *
 * Simple logging mechanism
 *
 */
 
#ifndef __LMLOG_H__
#define __LMLOG_H__

#include "config.h"

/*!
  @typedef lmlog_level
  Type that enumerates the output levels for this API.  Levels
  are ordered by decreasing verbosity.
*/
typedef enum {
  lmlog_level_debug = 0,
  lmlog_level_info,
  lmlog_level_warn,
  lmlog_level_error,
  lmlog_level_alert
} lmlog_level;

/*!
  @function lmlog_get_file_ptr
  Returns the FILE* of the i/o stream that receives logged
  messages.  Defaults to stderr.
*/
FILE* lmlog_get_file_ptr(void);

/*!
  @function lmlog_open_file
  Attempt to open the file at path for writing (or appending, if
  should_append is true) and subsequently log messages to that
  stream.  If a stream has been opened previously (and requires
  closing) it will be closed if the new stream is successfully
  opened.
*/
void lmlog_open_file(const char *path, bool should_append);

/*!
  @function lmlog_set_file_ptr
  Subsequently log messages to the_log_file stream.  If a stream
  has been opened previously (and requires closing) it will be
  closed.  If should_close_when_done is true, the next successful
  alteration to the logging stream will have the_log_file being
  closed.
*/
void lmlog_set_file_ptr(FILE *the_log_file, bool should_close_when_done);

/*!
  @function lmlog_get_base_level
  Returns the current mimimum logging level.
*/
lmlog_level lmlog_get_base_level(void);

/*!
  @function lmlog_set_base_level
  Alter the minimum logging level.  Any messages logged with a level
  less than log_level will not be displayed.
*/
void lmlog_set_base_level(lmlog_level log_level);

/*!
  @function lmlog_get_should_show_timestamps
  Returns true if timestamp strings will be prepended to all logged
  messages.
*/
bool lmlog_get_should_show_timestamps(void);

/*!
  @function lmlog_set_should_show_timestamps
  If should_show_timestamps is true, then a timestamp string will be
  prepended to all logged messages.
*/
void lmlog_set_should_show_timestamps(bool should_show_timestamps);

/*!
  @function lmlog
  Write the C string a_string to the logging stream if log_level
  meets or exceeds the current minimum logging level.
*/
void lmlog(lmlog_level log_level, const char *a_string);

/*!
  @function lmlogf
  Write a string formed according to the format string and any additional
  arguments to the logging stream if log_level meets or exceeds the current
  minimum logging level.
  
  Uses the printf() family of functions, so all documentation of your C
  library's printf() function applies equally well here.
*/
void lmlogf(lmlog_level log_level, const char *format, ...);

/*!
  @defined LMASSERT
  If the conditional test passed to this macro evaluates to false, an
  error message citing the location within the code and the failed
  assertion itself is logged and execution is immediately terminated.
*/
#ifdef NO_LMASSERT
# define LMASSERT(COND)
#else
# define LMASSERT(COND) \
  if ( ! (COND) ) { \
    lmlogf(lmlog_level_alert, "%s:%d > %s(): failed assertion '%s'", \
        __FILE__, __LINE__, __FUNCTION__, #COND \
      ); \
    exit(1); \
  }
#endif

/*!
  @defined LMDEBUG
  Convenience macro that expands to a format string and zero or more
  arguments to a lmlogf() call at the debug level:
  
    LMDEBUG("user limit exceeded: %d > %d", current, limit);
*/
#ifdef NO_LMDEBUG
# define LMDEBUG(FMT, ...)
#else
# define LMDEBUG(FMT, ...) \
  lmlogf(lmlog_level_debug, "%s:%d > %s(): "FMT, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);
#endif


#endif /* __LMLOG_H__ */
