/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * lmlog.c
 *
 * Simple logging mechanism
 *
 */

#include "lmlog.h"

#include <time.h>

static bool           __lmlog_is_inited = false;
static FILE           *__lmlog_file_ptr = NULL;
static lmlog_level    __lmlog_base_level = lmlog_level_error;
static bool           __lmlog_should_close_when_done = false;
static bool           __lmlog_should_show_timestamps = false;

//

static const char*    __lmlog_level_strings[] = {
                        "DEBUG",
                        "INFO ",
                        "WARN ",
                        "ERROR",
                        "ALERT"
                      };

//

FILE*
lmlog_get_file_ptr()
{
  if ( ! __lmlog_is_inited ) {
    const char    *e;
    
    __lmlog_file_ptr = stderr;
    __lmlog_is_inited = true;
    
    // Does the env tell us we should emit timestamps?
    if ( (e = getenv("LMDB_LOG_TIMESTAMPS")) && *e ) __lmlog_should_show_timestamps = true;
  }
  return __lmlog_file_ptr;
}

//

void
lmlog_open_file(
  const char  *path,
  bool        should_append
)
{
  FILE        *fptr = fopen(path, should_append ? "a" : "w");
  
  if ( fptr ) lmlog_set_file_ptr(fptr, true);
}

//

void
lmlog_set_file_ptr(
  FILE        *the_log_file,
  bool        should_close_when_done
)
{
  if ( __lmlog_file_ptr && __lmlog_should_close_when_done ) fclose(__lmlog_file_ptr);
  
  __lmlog_file_ptr = the_log_file;
  __lmlog_should_close_when_done = should_close_when_done;
}

//

lmlog_level
lmlog_get_base_level()
{
  return __lmlog_base_level;
}

//

void
lmlog_set_base_level(
  lmlog_level   log_level
)
{
  if ( log_level >= lmlog_level_debug && log_level <= lmlog_level_alert ) __lmlog_base_level = log_level;
}

//

bool
lmlog_get_should_show_timestamps()
{
  return __lmlog_should_show_timestamps;
}

//

void
lmlog_set_should_show_timestamps(
  bool          should_show_timestamps
)
{
  __lmlog_should_show_timestamps = should_show_timestamps;
}

//

void
__lmlog_write_timestamp()
{
  char        timestamp_str[32];
  struct tm   when;
  time_t      now = time(NULL);
  
  localtime_r(&now, &when);
  strftime(timestamp_str, sizeof(timestamp_str), "[%Y-%d-%m %H:%M:%S%z]", &when);
  fputs(timestamp_str, lmlog_get_file_ptr());
}

//

void
lmlog(
  lmlog_level   log_level,
  const char    *a_string
)
{
  if ( log_level >= lmlog_level_debug && log_level <= lmlog_level_alert && log_level >= __lmlog_base_level ) {
    FILE          *fptr = lmlog_get_file_ptr();
    
    if ( __lmlog_should_show_timestamps ) __lmlog_write_timestamp();
    fprintf(fptr, "[%s] %s\n", __lmlog_level_strings[log_level], a_string);
  }
}

//

void
lmlogf(
  lmlog_level   log_level,
  const char    *format,
  ...
)
{
  if ( log_level >= lmlog_level_debug && log_level <= lmlog_level_alert && log_level >= __lmlog_base_level ) {
    FILE          *fptr = lmlog_get_file_ptr();
    va_list       vargs;
    
    if ( __lmlog_should_show_timestamps ) __lmlog_write_timestamp();
    va_start(vargs, format);
    fprintf(fptr, "[%s] ", __lmlog_level_strings[log_level]);
    vfprintf(fptr, format, vargs);
    fputc('\n', fptr);
    va_end(vargs);
  }
}
