/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * fscanln.c
 *
 * Simple file scanner pseudo-class.
 *
 */
 
#include "fscanln.h"
#include "mempool.h"
#include "lmlog.h"

#include <fcntl.h>

//

enum {
  fscanln_flags_should_close_on_dealloc = 1 << 0,
  fscanln_flags_should_use_pclose_on_dealloc = 1 << 1,
  fscanln_flags_should_use_waitpid_on_dealloc = 1 << 2,
  fscanln_flags_is_regex_present = 1 << 3,
  fscanln_flags_is_regex_enabled = 1 << 4
};

typedef struct _fscanln {
  unsigned int    ref_count;
  unsigned int    flags;
  
  pid_t           child_pid;
  
  FILE            *fptr_src;

  unsigned long   line_number;
  unsigned long   line_number_inc;
  
  regex_t         line_regex;
  size_t          sub_match_max;
  regmatch_t      *sub_matches;
  const char*     *sub_match_strings;
  mempool_ref     string_pool;
  
  bool            is_internal_buffer;
  char            *buffer;
  size_t          buffer_length;
  size_t          buffer_capacity;
} fscanln;

#ifndef FSCANLN_INTERNAL_BUFFER_CAPACITY
#define FSCANLN_INTERNAL_BUFFER_CAPACITY  (4096 - sizeof(fscanln))
#endif

#ifndef FSCANLN_INTERNAL_BUFFER_DELTA
#define FSCANLN_INTERNAL_BUFFER_DELTA     4096
#endif

//

fscanln*
__fscanln_alloc(void)
{
  fscanln       *new_obj = malloc(sizeof(fscanln) + FSCANLN_INTERNAL_BUFFER_CAPACITY);
  
  if ( new_obj ) {
    new_obj->ref_count            = 1;
    new_obj->flags                = 0;
    
    new_obj->fptr_src             = NULL;
    new_obj->line_number          = 0;
    new_obj->line_number_inc      = 1;
    
    new_obj->sub_match_max        = 0;
    new_obj->sub_matches          = NULL;
    new_obj->sub_match_strings    = NULL;
    new_obj->string_pool          = mempool_alloc();
    
    new_obj->is_internal_buffer   = true;
    new_obj->buffer               = (char*)((void*)new_obj + sizeof(fscanln));
    new_obj->buffer_length        = 0;
    new_obj->buffer_capacity      = FSCANLN_INTERNAL_BUFFER_CAPACITY;
  }
  return new_obj;
}

//

bool
__fscanln_get_flags(
  fscanln     *f,
  unsigned int  flag
)
{
  if ( flag == 0 ) flag = UINT_MAX;
  return ((f->flags & flag) == flag) ? true : false;
}

//

void
__fscanln_set_flags(
  fscanln     *f,
  unsigned int  flag,
  bool          state
)
{
  if ( state ) {
    f->flags |= flag;
  } else {
    f->flags &= ~flag;
  }
}

//

void
__fscanln_dealloc(
  fscanln     *f
)
{
  if ( f ) {
    if ( f->buffer && ! f->is_internal_buffer ) free((void*)f->buffer);
    if ( __fscanln_get_flags(f, fscanln_flags_is_regex_present) ) regfree(&f->line_regex);
    if ( f->sub_matches ) free((void*)f->sub_matches);
    if ( f->sub_match_strings ) free((void*)f->sub_match_strings);
    if ( f->string_pool ) mempool_dealloc(f->string_pool);
    
    if ( __fscanln_get_flags(f, fscanln_flags_should_use_waitpid_on_dealloc) ) {
      int   rc, exit_status;
      
      rc = waitpid(f->child_pid, &exit_status, WUNTRACED);
      if ( rc == -1 ) {
        lmlogf(lmlog_level_debug, "fscanln:  waitpid on %ld failed (errno = %d)", (long int)f->child_pid, errno);
      } else {
        lmlogf(lmlog_level_debug, "fscanln:  child pid %ld returned with exit status %d", (long int)f->child_pid, exit_status);
      }
    }
    
    if ( __fscanln_get_flags(f, fscanln_flags_should_close_on_dealloc) ) {
      if ( __fscanln_get_flags(f, fscanln_flags_should_use_pclose_on_dealloc) ) {
        pclose(f->fptr_src);
      } else {
        fclose(f->fptr_src);
      }
    }
    
    free((void*)f);
  }
}

//

void
__fscanln_clear_sub_matches(
  fscanln     *f
)
{
  if ( f->sub_matches ) {
    memset(f->sub_matches, 0, f->sub_match_max * sizeof(regmatch_t));
  }
  if ( f->string_pool ) mempool_reset(f->string_pool);
  if ( f->sub_match_strings ) {
    int         i = 0;
    
    while ( i < f->sub_match_max ) f->sub_match_strings[i++] = NULL;
  }
}

//

bool
__fscanln_set_regex(
  fscanln     *f,
  const char    *regex,
  int           regex_flags,
  int           sub_match_max
)
{
  int           rc;
  
  if ( __fscanln_get_flags(f, fscanln_flags_is_regex_present) ) {
    __fscanln_clear_sub_matches(f);
    regfree(&f->line_regex);
    __fscanln_set_flags(f, fscanln_flags_is_regex_present | fscanln_flags_is_regex_enabled, false);
  }
  rc = regcomp(&f->line_regex, regex, regex_flags);
  if ( rc != 0 ) return false;
  
  if ( sub_match_max > f->sub_match_max ) {
    void        *p = realloc(f->sub_matches, sub_match_max * sizeof(regmatch_t));
    void        *s = realloc(f->sub_match_strings, sub_match_max * sizeof(const char*));
    
    if ( ! p || !s ) {
      lmlog(lmlog_level_warn, "fscanln: failure to allocate/grow regex match set; no regex set");
      regfree(&f->line_regex);
      __fscanln_set_flags(f, fscanln_flags_is_regex_present | fscanln_flags_is_regex_enabled, false);
      return false;
    }
    f->sub_match_max = sub_match_max;
    
    if ( ! f->sub_matches ) memset(p, 0, sub_match_max * sizeof(regmatch_t));
    f->sub_matches = (regmatch_t*)p;
    
    if ( ! f->sub_match_strings ) memset(s, 0, sub_match_max * sizeof(const char*));
    f->sub_match_strings = (const char**)s;
  }
  
  __fscanln_set_flags(f, fscanln_flags_is_regex_present | fscanln_flags_is_regex_enabled, true);
  return true;
}

//

bool
__fscanln_grow(
  fscanln     *f
)
{
  bool          ok = false;
  
  if ( f->is_internal_buffer ) {
    /* Allocate externally: */
    size_t      l = 2 * FSCANLN_INTERNAL_BUFFER_DELTA;
    char        *p = malloc(l);
    
    if ( p ) {
      memcpy(p, f->buffer, f->buffer_length);
      f->buffer = p;
      f->is_internal_buffer = false;
      f->buffer_capacity = l;
      ok = true;
    } else {
      lmlogf(lmlog_level_warn, "fscanln: failure to externally-allocate scanner buffer (errno = %d)", errno);
    }
  } else {
    /* Reallocate: */
    size_t      l = f->buffer_capacity + FSCANLN_INTERNAL_BUFFER_DELTA;
    char        *p = realloc(f->buffer, l);
    
    if ( p ) {
      f->buffer = p;
      f->buffer_capacity = l;
      ok = true;
    } else {
      lmlogf(lmlog_level_warn, "fscanln: failure to grow scanner buffer (errno = %d)", errno);
    }
  }
  return ok;
}

//

bool
__fscanln_append_char(
  fscanln     *f,
  char          c
)
{
  if ( f->buffer_length == f->buffer_capacity ) {
    if ( ! __fscanln_grow(f) ) return false;
  }
  f->buffer[f->buffer_length++] = c;
  return true;
}

//

bool
__fscanln_fill(
  fscanln     *f
)
{
  int           prev_c = -1;
  bool          exit_loop = false;
  
  if ( feof(f->fptr_src) ) return false;
  
  f->line_number += f->line_number_inc;
  f->line_number_inc = 1;
  f->buffer_length = 0;
  
  while ( ! exit_loop && ! feof(f->fptr_src) ) {
    int         c = fgetc(f->fptr_src);
    
    if ( c == EOF ) break;
    
    else if ( c == '\n' ) {
      if ( prev_c != '\\' ) {
        exit_loop = true;
      } else {
        // Drop the backslash and the newline, keep adding
        // next line from the file:
        f->line_number_inc++;
        f->buffer_length--;
        c = ' ';
      }
    }
    prev_c = c;
    if ( ! __fscanln_append_char(f, c) ) return false;
  }
  return __fscanln_append_char(f, '\0');
}

//
#if 0
#pragma mark -
#endif
//

fscanln_ref
fscanln_create(
  FILE    *fptr,
  bool    should_close_on_dealloc
)
{
  fscanln   *new_scanner = __fscanln_alloc(); 
  
  if ( new_scanner ) {
    new_scanner->fptr_src = fptr;
    if ( should_close_on_dealloc ) __fscanln_set_flags(new_scanner, fscanln_flags_should_close_on_dealloc, true);
  } else {
    lmlog(lmlog_level_warn, "fscanln:  unable to allocate a new scanner");
  }
  return (fscanln_ref)new_scanner;
}

//

fscanln_ref
fscanln_create_with_file(
  const char    *path
)
{
  fscanln_ref   new_scanner = NULL;
  FILE          *fptr = fopen(path, "r");
  
  if ( fptr ) {
    new_scanner = fscanln_create(fptr, true);
    if ( ! new_scanner ) {
      fclose(fptr);
    }
  } else {
    lmlogf(lmlog_level_warn, "fscanln:  fopen(%s) failed: %s", path, strerror(errno));
  }
  return new_scanner;
}

//

fscanln_ref
fscanln_create_with_command(
  const char    *command
)
{
  fscanln_ref   new_scanner = NULL;
  FILE          *fptr = popen(command, "r");
  
  if ( fptr ) {
    new_scanner = fscanln_create(fptr, true);
    if ( ! new_scanner ) {
      pclose(fptr);
    } else {
      __fscanln_set_flags((fscanln*)new_scanner, fscanln_flags_should_use_pclose_on_dealloc, true);
    }
  } else {
    lmlogf(lmlog_level_warn, "fscanln:  popen(%s) failed: %s", command, strerror(errno));
  }
  return new_scanner;
}

//

fscanln_ref
fscanln_create_with_execve(
  const char    *path,
  char* const   *argv,
  char* const   *envp,
  bool          should_merge_stderr
)
{
  fscanln_ref   new_scanner = NULL;
  int           pd[2];
  pid_t         child_pid;
  
  // Get a stdout pipe setup:
  if ( pipe(pd) ) return NULL;
  
  switch ( child_pid = fork() ) {
  
    case 0: {
      int     devnull = open("/dev/null", O_RDONLY);
      
      // This is the child process.  Set stdout/stderr to the write end of the pipe and stdin to /dev/null:
      dup2(devnull, STDIN_FILENO);
      dup2(pd[1], STDOUT_FILENO);
      dup2(pd[1], should_merge_stderr ? STDERR_FILENO : devnull);
      close(devnull);
      close(pd[0]);
      close(pd[1]);
      execve(path, argv, envp);
      _exit(EXIT_FAILURE);
    }
    
    case -1: {
      // The fork() failed!
      lmlogf(lmlog_level_warn, "fscanln:  failed while forking for execve (errno = %d)", errno);
      close(pd[0]);
      close(pd[1]);
      return NULL;
    }
    
    default: {
      // This is the parent process, so we want to continue allocating the object:
      FILE      *fptr = fdopen(pd[0], "r");
      
      close(pd[1]);
      if ( fptr ) {
        new_scanner = fscanln_create(fptr, true);
        if ( new_scanner ) {
          __fscanln_set_flags((fscanln*)new_scanner, fscanln_flags_should_use_waitpid_on_dealloc, true);
          ((fscanln*)new_scanner)->child_pid = child_pid;
        } else {
          fclose(fptr);
        }
      } else {
        close(pd[0]);
      }
    }
  
  }
  return new_scanner;
}

//

fscanln_ref
fscanln_retain(
	fscanln_ref		f
)
{
	fscanln				*F = (fscanln*)f;
	
	F->ref_count++;
	return f;
}

//

void
fscanln_release(
  fscanln_ref   f
)
{
	fscanln				*F = (fscanln*)f;
	
	if ( --F->ref_count == 0 ) __fscanln_dealloc((fscanln*)f);
}

//

void
fscanln_unset_line_regex(
  fscanln_ref   f
)
{
  fscanln       *F = (fscanln*)f;
  
  if ( __fscanln_get_flags(F, fscanln_flags_is_regex_present) ) {
    regfree(&F->line_regex);
    __fscanln_set_flags(F, fscanln_flags_is_regex_present | fscanln_flags_is_regex_enabled, false);
  }
}

//

bool
fscanln_set_line_regex(
  fscanln_ref   f,
  const char    *regex,
  int           regex_flags,
  int           sub_match_max
)
{
  return __fscanln_set_regex((fscanln*)f, regex, regex_flags, sub_match_max ? sub_match_max + 1 : 0);
}

//

bool
fscanln_get_is_regex_enabled(
  fscanln_ref   f
)
{
  return __fscanln_get_flags((fscanln*)f, fscanln_flags_is_regex_present | fscanln_flags_is_regex_enabled);
}

//

void
fscanln_set_is_regex_enabled(
  fscanln_ref   f,
  bool          is_regex_enabled
)
{
  if ( __fscanln_get_flags((fscanln*)f, fscanln_flags_is_regex_present) ) {
    __fscanln_set_flags((fscanln*)f, fscanln_flags_is_regex_enabled, is_regex_enabled);
  }
}

//

unsigned long
fscanln_get_line_number(
  fscanln_ref   f
)
{
  return f->line_number;
}

//

bool
fscanln_get_line(
  fscanln_ref   f,
  const char    **out_line_ptr,
  size_t        *out_line_len
)
{
  fscanln       *F = (fscanln*)f;
  
  if ( __fscanln_get_flags(F, fscanln_flags_is_regex_present | fscanln_flags_is_regex_enabled) ) {
    __fscanln_clear_sub_matches((fscanln*)f);
    while ( __fscanln_fill((fscanln*)f) ) {
      if ( regexec(&f->line_regex, f->buffer, f->sub_match_max, f->sub_matches, 0) == 0 ) {
				if ( out_line_ptr ) *out_line_ptr = (const char*)f->buffer;
				if ( out_line_len ) *out_line_len = f->buffer_length;
				return true;
      }
    }
  } else {
    if ( __fscanln_fill((fscanln*)f) ) {
      if ( out_line_ptr ) *out_line_ptr = (const char*)f->buffer;
      if ( out_line_len ) *out_line_len = f->buffer_length;
      return true;
    }
  }
  return false;
}

//

const char*
fscanln_get_sub_match_string(
  fscanln_ref   f,
  int           sub_match_idx
)
{
  fscanln       *F = (fscanln*)f;
  
  if ( __fscanln_get_flags(F, fscanln_flags_is_regex_present | fscanln_flags_is_regex_enabled) ) {
    if ( sub_match_idx < f->sub_match_max ) {
      if ( ! f->sub_match_strings[sub_match_idx] ) {
        size_t  match_len = f->sub_matches[sub_match_idx].rm_eo - f->sub_matches[sub_match_idx].rm_so;
        char    *s = mempool_alloc_bytes(f->string_pool, match_len + 1);
        
        if ( s ) {
          memcpy(s, f->buffer + f->sub_matches[sub_match_idx].rm_so, match_len);
          s[match_len] = '\0';
          f->sub_match_strings[sub_match_idx] = s;
          return s;
        }
        lmlog(lmlog_level_warn, "failed to allocate space for sub-match string from scanner memory pool");
      } else {
        return f->sub_match_strings[sub_match_idx];
      }
    }
  }
  return NULL;
}
