/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * lmconfig.c
 *
 * Program configuration parsing and a few helper functions.
 *
 */

#include "lmconfig.h"
#include "mempool.h"
#include "fscanln.h"
#include "lmfeature.h"
#include "lmlog.h"
#include "util_fns.h"

//

#ifndef LMDB_VERSION_STRING
# error LMDB_VERSION_STRING is not defined
#endif
const char   *lmdb_version_str= LMDB_VERSION_STRING;

//

#ifndef LMDB_PREFIX_DIR
# error LMDB_PREFIX_DIR is not defined
#endif
const char   *lmdb_prefix_dir = LMDB_PREFIX_DIR;

//

#ifndef LMDB_SYSCONF_DIR
# error LMDB_SYSCONF_DIR is not defined
#endif
const char  *lmdb_sysconf_dir = LMDB_SYSCONF_DIR;

//

#ifndef LMDB_STATE_DIR
# error LMDB_STATE_DIR is not defined
#endif
const char  *lmdb_state_dir = LMDB_STATE_DIR;

//

#ifndef LMDB_DEFAULT_CONF_FILE
# error LMDB_DEFAULT_CONF_FILE is not defined
#endif
const char   *lmdb_default_conf_file = LMDB_DEFAULT_CONF_FILE;

//

#ifndef LMDB_DISABLE_RRDTOOL 
# ifndef LMDB_RRD_REPODIR
#  error LMDB_RRD_REPODIR is not defined
# endif
const char   *lmdb_rrd_repodir = LMDB_RRD_REPODIR;
#endif

//

#ifdef LMDB_APPLICATION_CLI

const char*   lmconfig_lmstat_interface_kind_str[] = {
    "<not set>",
    "<static output in file>",
    "<simple shell command>",
    "<command with arguments>",
    NULL
  };

#endif

#ifdef LMDB_APPLICATION_REPORT

const char*   lmconfig_lmdb_aggregate_str[] = {
                        "undef",
                        "none",
                        "hourly",
                        "daily",
                        "weekly",
                        "monthly",
                        "yearly",
                        "total",
                        NULL
                      };

const char*   lmconfig_lmdb_range_str[] = {
                        "undef",
                        "none",
                        "last_check",
                        "hour",
                        "day",
                        "week",
                        "month",
                        "year",
                        NULL
                      };

const char*   lmconfig_report_format_str[] = {
                        "column",
                        "csv",
                        NULL
                      };

#endif

//

typedef struct _lmconfig_private {
  lmconfig          public;
  //
  mempool_ref       pool;
} lmconfig_private;

//

lmconfig_private*
__lmconfig_alloc(void)
{
  lmconfig_private    *new_config = malloc(sizeof(lmconfig_private));

  if ( new_config ) {
    memset(new_config, 0, sizeof(lmconfig_private));
    new_config->public.base_config_path = lmdb_default_conf_file;
#ifdef LMDB_APPLICATION_CLI
# ifndef LMDB_DISABLE_RRDTOOL
    new_config->public.should_update_rrds = true;
    new_config->public.rrd_repodir = lmdb_rrd_repodir;
# endif
#endif
#ifdef LMDB_APPLICATION_NAGIOS_CHECK
    new_config->public.nagios_default_warn = nagios_threshold_make(nagios_threshold_type_fraction, 0.95);
    new_config->public.nagios_default_crit = nagios_threshold_make(nagios_threshold_type_fraction, 0.99);
#endif
#ifdef LMDB_APPLICATION_REPORT
    new_config->public.report_aggregate = lmdb_usage_report_aggregate_total;
    new_config->public.report_range = lmdb_usage_report_range_last_day;
    new_config->public.should_show_headers = true;
    new_config->public.fields_for_display = field_selection_default;
#endif
#ifdef LMDB_APPLICATION_LS
    new_config->public.match_id = lmfeature_no_id;
#endif
    new_config->pool = mempool_alloc();
    if ( ! new_config->pool ) {
      free((void*)new_config);
      new_config = NULL;
    }
  }
  return new_config;
}

//

void
__lmconfig_dealloc(
  lmconfig_private    *the_config
)
{
#ifdef LMDB_APPLICATION_NAGIOS_CHECK
  if ( the_config->public.nagios_rules ) nagios_rules_release(the_config->public.nagios_rules);
#endif
  if ( the_config->pool ) mempool_dealloc(the_config->pool);
  free((void*)the_config);
}

//
#if 0
#pragma mark -
#endif
//

bool
__lmconfig_param_cmp(
  const char    *from_file,
  size_t        from_file_len,
  const char    *static_value
)
{
  size_t        static_value_len = strlen(static_value);

  if ( (from_file_len == static_value_len) && strncmp(from_file, static_value, from_file_len) == 0 ) return true;
  return false;
}

//

const char*
__lmconfig_fixup_path(
  mempool_ref   pool,
  const char    *path
)
{
  static const char   *lmdb_relative_prefix = "%LMDB%";
  static int          lmdb_relative_prefix_len = 6;
  
  static const char   *lmdb_relative_sysconf = "%LMDB_CONFDIR%";
  static int          lmdb_relative_sysconf_len = 14;
  
  static const char   *lmdb_relative_statedir = "%LMDB_STATEDIR%";
  static int          lmdb_relative_statedir_len = 15;

  const char    *out_path = path;

  if ( strncmp(path, lmdb_relative_prefix, lmdb_relative_prefix_len) == 0 ) {
    size_t      alt_path_len = strlen(lmdb_prefix_dir) + 2; /* Prefix + '/' + NUL */
    size_t      orig_offset = lmdb_relative_prefix_len;
    char        *alt_path;

    while ( *(path + orig_offset) == '/' ) orig_offset++;
    alt_path_len += strlen(path + orig_offset);
    alt_path = mempool_alloc_bytes(pool, alt_path_len);
    if ( alt_path ) {
      snprintf(alt_path, alt_path_len, "%s/%s", lmdb_prefix_dir, path + orig_offset);
      out_path = (const char*)alt_path;
    } else {
      lmlogf(lmlog_level_warn, "failed to allocate space to canonicalize %s", path);
      out_path = NULL;
    }
  }
  else if ( strncmp(path, lmdb_relative_sysconf, lmdb_relative_sysconf_len) == 0 ) {
    size_t      alt_path_len = strlen(lmdb_sysconf_dir) + 2; /* Sysconf + '/' + NUL */
    size_t      orig_offset = lmdb_relative_sysconf_len;
    char        *alt_path;
    
    while ( *(path + orig_offset) == '/' ) orig_offset++;
    alt_path_len += strlen(path + orig_offset);
    alt_path = mempool_alloc_bytes(pool, alt_path_len);
    if ( alt_path ) {
      snprintf(alt_path, alt_path_len, "%s/%s", lmdb_sysconf_dir, path + orig_offset);
      out_path = (const char*)alt_path;
    } else {
      lmlogf(lmlog_level_warn, "failed to allocate space to canonicalize %s", path);
      out_path = NULL;
    }
  }
  else if ( strncmp(path, lmdb_relative_statedir, lmdb_relative_statedir_len) == 0 ) {
    size_t      alt_path_len = strlen(lmdb_state_dir) + 2; /* Statedir + '/' + NUL */
    size_t      orig_offset = lmdb_relative_statedir_len;
    char        *alt_path;
    
    while ( *(path + orig_offset) == '/' ) orig_offset++;
    alt_path_len += strlen(path + orig_offset);
    alt_path = mempool_alloc_bytes(pool, alt_path_len);
    if ( alt_path ) {
      snprintf(alt_path, alt_path_len, "%s/%s", lmdb_state_dir, path + orig_offset);
      out_path = (const char*)alt_path;
    } else {
      lmlogf(lmlog_level_warn, "failed to allocate space to canonicalize %s", path);
      out_path = NULL;
    }
  }
  return out_path;
}

//

lmconfig*
lmconfig_update_with_file(
  lmconfig      *the_config,
  const char    *path
)
{
  fscanln_ref   scanner = fscanln_create_with_file(path);

  if ( scanner ) {
    const char        *line;
    bool              ok = true;
    lmconfig_private  *THE_CONFIG;

    THE_CONFIG = the_config ? (lmconfig_private*)the_config : __lmconfig_alloc();

    while ( ok && fscanln_get_line(scanner, &line, NULL) ) {
      const char      *param_start, *param_end;
      const char      *word;
      int             equals;

      param_start = param_end = NULL;
      equals = 0;

      /* Drop any leading whitespace: */
      while ( is_not_eol(*line) && isspace(*line) ) line++;

      /* Comment? */
      if ( is_eol(*line) || (*line == '#') ) continue;

      /* Determine span of the config parameter: */
      param_start = param_end = line;
      while ( is_not_eol(*param_end) && (isalnum(*param_end) || (*param_end == '-') || (*param_end == '_')) ) param_end++;
      line = param_end;

      if ( is_not_eol(*line) ) {
        /* Skip any whitespace (and one equal sign): */
        while ( is_not_eol(*line) && ((isspace(*line) || (*line == '='))) ) {
          if ( *line == '=' ) {
            if ( equals > 0 ) {
              lmlogf(lmlog_level_error, "failed parsing line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
            }
            equals++;
          }
          line++;
        }
      }
      if ( ok ) {
        /* We're now ready to react to the config parameter.  The pointer
         * line is at the first character associated with the parameter, and
         * param_start/param_end are pointers to the name of the parameter.
         */
        if ( __lmconfig_param_cmp(param_start, param_end - param_start, "database-path") ) {
          switch ( str_next_word(&line, THE_CONFIG->pool, &word) ) {
            case str_next_word_ok:
              THE_CONFIG->public.license_db_path = __lmconfig_fixup_path(THE_CONFIG->pool, word);
              break;
            case str_next_word_none:
              lmlogf(lmlog_level_error, "no value provided for database-path parameter at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
            case str_next_word_error:
              fprintf(stderr, "at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
          }
        }

#ifdef LMDB_APPLICATION_CLI
        else if ( __lmconfig_param_cmp(param_start, param_end - param_start, "license-file") ) {
          switch ( str_next_word(&line, THE_CONFIG->pool, &word) ) {
            case str_next_word_ok:
              THE_CONFIG->public.flexlm_license_path = __lmconfig_fixup_path(THE_CONFIG->pool, word);
              break;
            case str_next_word_none:
              lmlogf(lmlog_level_error, "no value provided for license-file parameter at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
            case str_next_word_error:
              fprintf(stderr, "at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
          }
        }

        else if ( __lmconfig_param_cmp(param_start, param_end - param_start, "lmstat-static-output") ) {
          switch ( str_next_word(&line, THE_CONFIG->pool, &word) ) {
            case str_next_word_ok:
              THE_CONFIG->public.lmstat_interface_kind = lmstat_interface_kind_static_output;
              THE_CONFIG->public.lmstat_interface.static_output = __lmconfig_fixup_path(THE_CONFIG->pool, word);
              break;
            case str_next_word_none:
              lmlogf(lmlog_level_error, "no value provided for lmstat-static-output parameter at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
            case str_next_word_error:
              fprintf(stderr, "at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
          }
        }

        else if ( __lmconfig_param_cmp(param_start, param_end - param_start, "lmstat-cmd") ) {
          switch ( str_next_word(&line, THE_CONFIG->pool, &word) ) {
            case str_next_word_ok:
              THE_CONFIG->public.lmstat_interface_kind = lmstat_interface_kind_command;
              THE_CONFIG->public.lmstat_interface.command = word;
              break;
            case str_next_word_none:
              lmlogf(lmlog_level_error, "no value provided for lmstat-cmd parameter at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
            case str_next_word_error:
              fprintf(stderr, "at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
          }
        }

        else if ( __lmconfig_param_cmp(param_start, param_end - param_start, "lmstat-cmd-and-args") ) {
          const char    *savepoint = line;
          unsigned int  argc = 0;
          bool          done = false;

          while ( ! done ) {
            switch ( str_next_word(&line, NULL, NULL) ) {
              case str_next_word_ok:
                argc++;
                break;
              case str_next_word_none:
                done = true;
                break;
              case str_next_word_error:
                fprintf(stderr, "at line %lu\n", fscanln_get_line_number(scanner));
                ok = false;
                done = true;
                break;
            }
          }

          if ( ok && (argc > 0) ) {
            THE_CONFIG->public.lmstat_interface.exec = malloc(sizeof(const char*) * (argc + 1));

            if ( THE_CONFIG->public.lmstat_interface.exec ) {
              const char*     *argv = (const char**)THE_CONFIG->public.lmstat_interface.exec;
              int             parse_rc;

              line = savepoint;
              while ( argc && ((parse_rc = str_next_word(&line, THE_CONFIG->pool, argv)) == str_next_word_ok) ) {
                argv++;
                argc--;
              }
              if ( (parse_rc == str_next_word_ok) && (argc == 0) ) {
                *argv = NULL;
                THE_CONFIG->public.lmstat_interface_kind = lmstat_interface_kind_exec;
              } else {
                lmlog(lmlog_level_error, "unable to initialize argument array for lmstat command and args\n");
                ok = false;
              }
            } else {
              lmlog(lmlog_level_error, "unable to allocate array for lmstat command and args\n");
              ok = false;
            }
          } else if ( ok ) {
            fprintf(stderr, "WARNING:  no value provided for lmstat-cmd-and-args paramter at line %lud\n", fscanln_get_line_number(scanner));
            ok = false;
          }
        }

        else if ( __lmconfig_param_cmp(param_start, param_end - param_start, "include") ) {
          size_t      include_path_len = strlen(line);

          while ( isspace(line[include_path_len - 1]) ) include_path_len--;

          if ( include_path_len > 0 ) {
            char          include_path[include_path_len + 1];
            const char    *final_include_path;

            strncpy(include_path, line, include_path_len);
            include_path[include_path_len] = '\0';

            final_include_path = __lmconfig_fixup_path(THE_CONFIG->pool, include_path);

            LMDEBUG("will attempt to parse included configuration file %s", final_include_path);

            the_config = lmconfig_update_with_file(the_config, final_include_path);
            if ( ! the_config ) {
              fscanln_release(scanner);
              return NULL;
            }
          }
        }

# ifndef LMDB_DISABLE_RRDTOOL
        else if ( __lmconfig_param_cmp(param_start, param_end - param_start, "rrd-repodir") ) {
          switch ( str_next_word(&line, THE_CONFIG->pool, &word) ) {
            case str_next_word_ok:
              THE_CONFIG->public.rrd_repodir = __lmconfig_fixup_path(THE_CONFIG->pool, word);
              break;
            case str_next_word_none:
              lmlogf(lmlog_level_error, "no value provided for rrd-repodir parameter at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
            case str_next_word_error:
              fprintf(stderr, "at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
          }
        }
        
        else if ( __lmconfig_param_cmp(param_start, param_end - param_start, "rrd-updates") ) {
          switch ( str_next_word(&line, THE_CONFIG->pool, &word) ) {
            case str_next_word_ok:
              if ( !strcasecmp(word, "true") || !strcasecmp(word, "yes") || !strcasecmp(word, "t") || !strcasecmp(word, "y") ) {
                THE_CONFIG->public.should_update_rrds = true;
              }
              else if ( !strcasecmp(word, "false") || !strcasecmp(word, "no") || !strcasecmp(word, "f") || !strcasecmp(word, "n") ) {
                THE_CONFIG->public.should_update_rrds = false;
              }
              else {
                lmlogf(lmlog_level_error, "invalid value for rrd-updates parameter at line %lu: %s\n", fscanln_get_line_number(scanner), word);
                ok = false;
              }
              break;
            case str_next_word_none:
              lmlogf(lmlog_level_error, "no value provided for rrd-updates parameter at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
            case str_next_word_error:
              fprintf(stderr, "at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
          }
        }
# endif
#endif

#ifdef LMDB_APPLICATION_NAGIOS_CHECK

        else if ( __lmconfig_param_cmp(param_start, param_end - param_start, "nagios-rules") ) {
          switch ( str_next_word(&line, THE_CONFIG->pool, &word) ) {
            case str_next_word_ok: {
              const char          *file = __lmconfig_fixup_path(THE_CONFIG->pool, word);
              nagios_rules_ref    rules = nagios_rules_create_with_file(file);
              
              if ( rules ) {
                LMDEBUG("successfully parsed nagios rules from '%s'", file);
                THE_CONFIG->public.nagios_rules = rules;
              } else {
                lmlogf(lmlog_level_error, "errors while parsing nagios rules file: %s", file);
                ok = false;
              }
              break;
            }
            case str_next_word_none:
              lmlogf(lmlog_level_error, "no value provided for nagios-include-regex parameter at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
            case str_next_word_error:
              fprintf(stderr, "at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
          }
        }

        else if ( __lmconfig_param_cmp(param_start, param_end - param_start, "nagios-warn") ) {
          switch ( str_next_word(&line, THE_CONFIG->pool, &word) ) {
            case str_next_word_ok: {
              char      *endp;
              double    value = strtod(word, &endp);

              if ( endp > word ) {
                if ( *endp == '%' ) value = value * 0.01;
                THE_CONFIG->public.nagios_default_warn = nagios_threshold_make(nagios_threshold_type_fraction, value);
              }
              break;
            }
            case str_next_word_none:
              lmlogf(lmlog_level_error, "no value provided for nagios-warn parameter at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
            case str_next_word_error:
              fprintf(stderr, "at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
          }
        }

        else if ( __lmconfig_param_cmp(param_start, param_end - param_start, "nagios-crit") ) {
          switch ( str_next_word(&line, THE_CONFIG->pool, &word) ) {
            case str_next_word_ok: {
              char      *endp;
              double    value = strtod(word, &endp);

              if ( endp > word ) {
                if ( *endp == '%' ) value = value * 0.01;
                THE_CONFIG->public.nagios_default_crit = nagios_threshold_make(nagios_threshold_type_fraction, value);
              }
              break;
            }
            case str_next_word_none:
              lmlogf(lmlog_level_error, "no value provided for nagios-crit parameter at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
            case str_next_word_error:
              fprintf(stderr, "at line %lu\n", fscanln_get_line_number(scanner));
              ok = false;
              break;
          }
        }

#endif

      }
    }

    if ( ! ok ) {
      __lmconfig_dealloc(THE_CONFIG);
      THE_CONFIG = NULL;
    }
    fscanln_release(scanner);

    return (lmconfig*)THE_CONFIG;
  } else {
    lmlogf(lmlog_level_error, "failed to open configuration file: %s\n", path);
  }
  return NULL;
}

//

#include <getopt.h>

struct option lmdb_cli_options[] = {
    { "help",                   no_argument,            NULL, 'h' },
    { "verbose",                no_argument,            NULL, 'v' },
    { "quiet",                  no_argument,            NULL, 'q' },
    { "log-timestamps",         no_argument,            NULL, 't' },
    { "conf",                   required_argument,      NULL, 'C' },
    { "database-path",          required_argument,      NULL, 'd' },
#ifdef LMDB_APPLICATION_CLI
    { "license-file",           required_argument,      NULL, 'c' },
    { "lmstat-static-output",   required_argument,      NULL, 'O' },
    { "lmstat-cmd",             required_argument,      NULL, 'e' },
    { "rrd-repodir",            required_argument,      NULL, 'R' },
    { "no-rrd-updates",         no_argument,            NULL, 'u' },
    { "rrd-updates",            no_argument,            NULL, 'U' },
#endif
#ifdef LMDB_APPLICATION_NAGIOS_CHECK
    { "nagios-rules",           required_argument,      NULL, 'r' },
    { "nagios-warn",            required_argument,      NULL, 'w' },
    { "nagios-crit",            required_argument,      NULL, 'c' },
#endif
#ifdef LMDB_APPLICATION_REPORT
    { "report-aggregate",       required_argument,      NULL, 'a' },
    { "report-range",           required_argument,      NULL, 'r' },
    { "report-format",          required_argument,      NULL, 'f' },
    { "start-at",               required_argument,      NULL, 's' },
    { "end-at",                 required_argument,      NULL, 'e' },
    { "match-feature",          required_argument,      NULL, 0x80 },
    { "match-vendor",           required_argument,      NULL, 0x81 },
    { "match-version",          required_argument,      NULL, 0x82 },
    { "no-headers",             no_argument,            NULL, 'H' },
    { "show-feature-id",        no_argument,            NULL, 'F' },
    { "hide-count",             no_argument,            NULL, 'U' },
    { "hide-percentage",        no_argument,            NULL, 'P' },
    { "hide-expire-ts",         no_argument,            NULL, 'E' },
    { "hide-check-ts",          no_argument,            NULL, 'T' },
#endif
#ifdef LMDB_APPLICATION_LS
    { "name-only",              no_argument,            NULL, 'n' },
    { "match-id",               required_argument,      NULL, 'i' },
    { "match-feature",          required_argument,      NULL, 0x80 },
    { "match-vendor",           required_argument,      NULL, 0x81 },
    { "match-version",          required_argument,      NULL, 0x82 },
#endif
    { NULL,                     0,                      NULL, 0 }
  };

#ifdef LMDB_APPLICATION_CLI
const char *lmdb_cli_option_flags = "hvqtC:d:c:O:e:R:uU";
#endif

#ifdef LMDB_APPLICATION_NAGIOS_CHECK
const char *lmdb_cli_option_flags = "hvqtC:d:r:w:c:";
#endif

#ifdef LMDB_APPLICATION_REPORT
const char *lmdb_cli_option_flags = "hvqtC:d:a:r:f:s:e:HFUPET\x80:\x81:\x82:";
#endif

#ifdef LMDB_APPLICATION_LS
const char *lmdb_cli_option_flags = "hvqtC:d:ni:\x80:\x81:\x82:";
#endif

void
lmconfig_usage(
  const char    *exe
)
{
  fprintf(stderr,
      "version %s\n"
      "usage:\n\n"
      "  %s {options}\n\n"
      " options:\n\n"
      "  --help/-h                              this helpful information\n"
      "  --verbose/-v                           increase the amount of output from the program; can be\n"
      "                                         used multiple times\n"
      "  --quiet/-q                             decrease the amount of output from the program; can be\n"
      "                                         used multiple times\n"
      "  --log-timestamps/-t                    include timestamps on all log lines produced as the\n"
      "                                         program executes; this option is also enabled by setting\n"
      "                                         LMDB_LOG_TIMESTAMPS to any non-empty string in the environment\n"
      "  --conf/-C <path>                       read options from the given configuration file\n"
      "  --database-path/-d <path>              an SQLite database at the given path will be used\n"
      "                                         to maintain feature definitions and counts; if the\n"
      "                                         file does not exist it is created and initialized\n"
#ifdef LMDB_APPLICATION_CLI
#ifndef LMDB_DISABLE_RRDTOOL
      "  --no-rrd-updates/-u                    do not attempt to update RRD files\n"
      "  --rrd-updates/-U                       attempt to update RRD files\n"
      "  --rrd-repodir/-R <path>                for each license feature an RRD file will be created\n"
      "                                         and updated in <path>\n"
#endif
      "  --license-file/-c <path>               the FLEXlm license file at the given path will\n"
      "                                         be scanned for features\n"
      "  --lmstat-static-output/-O <path>       if provided, the given file should contain the output\n"
      "                                         from lmstat and will be scanned for license usage\n"
      "                                         counts\n"
      "  --lmstat-cmd/-e <command string>       if provided, the given command will be invoked to\n"
      "                                         produce an extended lmstat listing that can be scanned\n"
      "                                         for license usage\n"
#endif
#ifdef LMDB_APPLICATION_NAGIOS_CHECK
      "  --nagios-rules/-r <path>               load a list of license tuple matching rules from the given\n"
      "                                         path\n"
      "  --nagios-warn/-w <pct|fraction>        warn when license usage exceeds the given pct (e.g. 95%%)\n"
      "                                         or fraction (e.g. 0.95); can be overridden by rules\n"
      "  --nagios-crit/-c <pct|fraction>        critical when license usage exceeds the given pct (e.g.\n"
      "                                         99%%) or fraction (e.g. 0.99)); can be overridden by rules\n"
      "\n"
      "  A nagios rules file provides a way to customize what usage levels produce Nagios warning and\n"
      "  critical dispositions.  Having a default percentage does not cover all possible situations:\n"
      "  95%% versus 99%% of a license with 5 seats doesn't produce the same behavior as a license with\n"
      "  250 seats.  The rules file allows for percentages AND integer counts for the warning and critical\n"
      "  thresholds:\n"
      "\n"
      "    #\n"
      "    # An example nagios rules file\n"
      "    #\n"
      "    \n"
      "    option matching first\n"
      "    \n"
      "    include pattern=MATLAB:*:* warn=200 crit=240\n"
      "    include string=Simscape:MLM:38 warn=1 crit=2\n"
      "    exclude regex=^(Bioinformatics|Database|Compiler)\n"
      "    include pattern=*\n"
      "    \n"
#endif
#ifdef LMDB_APPLICATION_REPORT
      "  --report-aggregate/-a <aggr>           an aggregation mode collapses all reported counts for each\n"
      "                                         license tuple to a temporal granularity, reporting the\n"
      "                                         minimum, maximum, and average values during that period\n"
      "\n"
      "                                           <aggr> = none, hourly, daily, weekly, monthly, yearly,\n"
      "                                                    total\n"
      "\n"
      "  --report-range/-r <range>              limit the temporal range of reported counts\n"
      "\n"
      "                                           <range> = none, last_check, hour, day, week, month, year\n"
      "\n"
      "  --report-format/-f <format>            format for the output:  <format> = column, csv\n"
      "  --start-at/-s <date-time>              only include count checks that happened at or after the given\n"
      "                                         timestamp; <date-time> = 'YYYY-mm-dd{ HH:MM{:SS{±zzzz}}}'\n"
      "  --end-at/-e <date-time>                only include count checks that happened at or before the given\n"
      "                                         timestamp; <date-time> = 'YYYY-mm-dd{ HH:MM{:SS{±zzzz}}}'\n"
      "  --match-feature <pattern>              only include features with the given identity; the <pattern>\n"
      "                                         can be:\n"
      "\n"
      "                                           - a simple string\n"
      "                                           - an SQL LIKE pattern in the form '{L}<string>'\n"
      "                                           - a globbing-pattern in the form '{G}<string>'\n"
      "                                           - a regular expression in the form '{R}<string>'\n"
      "\n"
      "  --match-vendor <pattern>               only include features with the given vendor identity;\n"
      "                                         the <pattern> works the same as for --match-feature\n"
      "  --match-version <pattern>              only include features with the given version;  the\n"
      "                                         <pattern> works the same as for --match-feature\n"
      "  --no-headers/-H                        do not display headers to the data (column titles, etc.)\n"
      "  --show-feature-id/-F                   include the feature ids in the output report\n"
      "  --hide-count/-U                        exclude the seat counts from the output report\n"
      "  --hide-percentage/-P                   exclude the usage percents from the output report\n"
      "  --hide-expire-ts/-E                    exclude the expiration timestamps from the output report\n"
      "  --hide-check-ts/-T                     exclude the check timestamps from the output report\n"
#endif
#ifdef LMDB_APPLICATION_LS
      "  --name-only/-n                         suppress the display of vendor and version\n"
      "  --match-id/-i <#>                      show the feature with the given numerical id\n"
      "  --match-feature <pattern>              only show features with the given identity; the <pattern>\n"
      "                                         can be:\n"
      "\n"
      "                                           - a simple string\n"
      "                                           - an SQL LIKE pattern in the form '{L}<string>'\n"
      "                                           - a globbing-pattern in the form '{G}<string>'\n"
      "                                           - a regular expression in the form '{R}<string>'\n"
      "\n"
      "  --match-vendor <pattern>               only show features with the given vendor identity;\n"
      "                                         the <pattern> works the same as for --match-feature\n"
      "  --match-version <pattern>              only show features with the given version;  the\n"
      "                                         <pattern> works the same as for --match-feature\n"
#endif
      "\n"
      "  By default, a configuration file at\n\n"
      "     %s\n\n"
      "  is scanned first (if present) and then command line arguments are processed.  Options on\n"
      "  the command line override those present in the default configuration file.\n"
      "\n"
#ifdef LMDB_APPLICATION_CLI
# ifndef LMDB_DISABLE_RRDTOOL
      "  The program will also automatically maintain a round-robin database for each license\n"
      "  feature.  The rrd files are named according to the feature id and can be found in:\n\n"
      "     %s\n\n"
# endif
#endif
      ,
      lmdb_version_str,
      exe,
      lmdb_default_conf_file
#ifndef LMDB_DISABLE_RRDTOOL
      ,
      lmdb_rrd_repodir
#endif
    );
}

lmconfig*
lmconfig_update_with_options(
  lmconfig      *the_config,
  int           argc,
  char * const  *argv
)
{
  lmconfig_private    *THE_CONFIG = the_config ? (lmconfig_private*)the_config : __lmconfig_alloc();
  int                 ch_opt;

  while ( (ch_opt = getopt_long(argc, argv, lmdb_cli_option_flags, lmdb_cli_options, NULL)) != -1 ) {

    switch ( ch_opt ) {

      case 'h': {
        lmconfig_usage(argv[0]);
        exit(0);
      }

      case 'v': {
        lmlog_set_base_level(lmlog_get_base_level() - 1);
        break;
      }

      case 'q': {
        lmlog_set_base_level(lmlog_get_base_level() + 1);
        break;
      }

      case 't': {
        lmlog_set_should_show_timestamps(true);
        break;
      }

      case 'C': {
        if ( optarg && *optarg ) {
          const char    *path = __lmconfig_fixup_path(THE_CONFIG->pool, optarg);
          
          if ( path ) THE_CONFIG->public.base_config_path = path;
        } else {
          lmlog(lmlog_level_error, "no file path provided to --conf/-C option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }

      case 'd': {
        if ( optarg && *optarg ) {
          const char    *path = __lmconfig_fixup_path(THE_CONFIG->pool, optarg);

          if ( path == optarg ) path = mempool_strdup(THE_CONFIG->pool, optarg);
          THE_CONFIG->public.license_db_path = path;
          if ( ! THE_CONFIG->public.license_db_path ) {
            lmlog(lmlog_level_error, "unable to allocate space for license database path\n");
            lmconfig_dealloc(the_config);
            return NULL;
          }
        } else {
          lmlog(lmlog_level_error, "no file path provided to --database-path/-d option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }

#ifdef LMDB_APPLICATION_CLI
      case 'c': {
        if ( optarg && *optarg ) {
          const char    *path = __lmconfig_fixup_path(THE_CONFIG->pool, optarg);

          if ( path == optarg ) path = mempool_strdup(THE_CONFIG->pool, optarg);
          THE_CONFIG->public.flexlm_license_path = path;
          if ( ! THE_CONFIG->public.flexlm_license_path ) {
            lmlog(lmlog_level_error, "unable to allocate space for FLEXlm license path\n");
            lmconfig_dealloc(the_config);
            return NULL;
          }
        } else {
          lmlog(lmlog_level_error, "no file path provided to --license-file/-c option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }

      case 'O': {
        if ( optarg && *optarg ) {
          const char    *path = __lmconfig_fixup_path(THE_CONFIG->pool, optarg);

          if ( path == optarg ) path = mempool_strdup(THE_CONFIG->pool, optarg);
          THE_CONFIG->public.lmstat_interface_kind = lmstat_interface_kind_static_output;
          THE_CONFIG->public.lmstat_interface.static_output = path;
          if ( ! THE_CONFIG->public.lmstat_interface.static_output ) {
            lmlog(lmlog_level_error, "unable to allocate space for lmstat static output path\n");
            lmconfig_dealloc(the_config);
            return NULL;
          }
        } else {
          lmlog(lmlog_level_error, "no file path provided to --lmstat-static-output/-O option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }

      case 'e': {
        if ( optarg && *optarg ) {
          THE_CONFIG->public.lmstat_interface_kind = lmstat_interface_kind_command;
          THE_CONFIG->public.lmstat_interface.command = mempool_strdup(THE_CONFIG->pool, optarg);
          if ( ! THE_CONFIG->public.lmstat_interface.static_output ) {
            lmlog(lmlog_level_error, "unable to allocate space for lmstat command\n");
            lmconfig_dealloc(the_config);
            return NULL;
          }
        } else {
          lmlog(lmlog_level_error, "no command provided to --lmstat-cmd/-e option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }
# ifndef LMDB_DISABLE_RRDTOOL
      case 'u':
        THE_CONFIG->public.should_update_rrds = false;
        break;
        
      case 'U':
        THE_CONFIG->public.should_update_rrds = true;
        break;
        
      case 'R': {
        if ( optarg && *optarg ) {
          const char    *path = __lmconfig_fixup_path(THE_CONFIG->pool, optarg);

          if ( path == optarg ) path = mempool_strdup(THE_CONFIG->pool, optarg);
          if ( path ) {
            THE_CONFIG->public.rrd_repodir = path;
          } else {
            lmlog(lmlog_level_error, "unable to allocate space for RRD repository directory\n");
            lmconfig_dealloc(the_config);
            return NULL;
          }
        } else {
          lmlog(lmlog_level_error, "no file path provided to --lmstat-static-output/-O option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }
# endif
#endif
#ifdef LMDB_APPLICATION_NAGIOS_CHECK

      case 'r': {
        if ( optarg && *optarg ) {
          const char    *path = __lmconfig_fixup_path(THE_CONFIG->pool, optarg);

          if ( path ) {
            nagios_rules_ref    rules = nagios_rules_create_with_file(path);
              
            if ( rules ) {
              LMDEBUG("successfully parsed nagios rules from '%s'", path);
              THE_CONFIG->public.nagios_rules = rules;
            } else {
              lmlogf(lmlog_level_error, "errors while parsing nagios rules file: %s", path);
              lmconfig_dealloc(the_config);
              return NULL;
            }
          }
        }
        break;
      }

      case 'w': {
        if ( optarg && *optarg ) {
          char      *endp;
          double    value = strtod(optarg, &endp);

          if ( endp > optarg ) {
            if ( *endp == '%' ) value = value * 0.01;
            THE_CONFIG->public.nagios_default_warn = nagios_threshold_make(nagios_threshold_type_fraction, value);
          } else {
            lmlogf(lmlog_level_warn, "invalid warning threshold: %s", optarg);
          }
        } else {
          lmlog(lmlog_level_error, "no value provided to --warning/-w option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }

      case 'c': {
        if ( optarg && *optarg ) {
          char      *endp;
          double    value = strtod(optarg, &endp);

          if ( endp > optarg ) {
            if ( *endp == '%' ) value = value * 0.01;
            THE_CONFIG->public.nagios_default_crit = nagios_threshold_make(nagios_threshold_type_fraction, value);
          } else {
            lmlogf(lmlog_level_warn, "invalid critical threshold: %s", optarg);
          }
        } else {
          lmlog(lmlog_level_error, "no value provided to --critical/-c option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }
#endif

#ifdef LMDB_APPLICATION_REPORT

      case 'a': {
        if ( optarg && *optarg ) {
          int     i = lmdb_usage_report_aggregate_none;
          
          while ( i < lmdb_usage_report_aggregate_max ) {
            if ( strcasecmp(optarg, lmconfig_lmdb_aggregate_str[i]) == 0 ) break;
            i++;
          }
          if ( i < lmdb_usage_report_aggregate_max ) {
            THE_CONFIG->public.report_aggregate = i;
            LMDEBUG("selected aggregation %d", i);
          } else {
            lmlogf(lmlog_level_warn, "invalid report aggregate: %s", optarg);
          }
        } else {
          lmlog(lmlog_level_error, "no value provided to --report-aggregate/-a option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }

      case 'r': {
        if ( optarg && *optarg ) {
          int     i = lmdb_usage_report_range_none;
          
          while ( i < lmdb_usage_report_range_max ) {
            if ( strcasecmp(optarg, lmconfig_lmdb_range_str[i]) == 0 ) break;
            i++;
          }
          if ( i < lmdb_usage_report_range_max ) {
            THE_CONFIG->public.report_range = i;
            LMDEBUG("selected range %d", i);
          } else {
            lmlogf(lmlog_level_warn, "invalid report range: %s", optarg);
          }
        } else {
          lmlog(lmlog_level_error, "no value provided to --report-range/-r option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }
      
      case 'f': {
        if ( optarg && *optarg ) {
          int     i = report_format_column;
          
          while ( i < report_format_max ) {
            if ( strcasecmp(optarg, lmconfig_report_format_str[i]) == 0 ) break;
            i++;
          }
          if ( i < report_format_max ) {
            THE_CONFIG->public.report_format = i;
            LMDEBUG("selected report format %d", i);
          } else {
            lmlogf(lmlog_level_warn, "invalid report format: %s", optarg);
          }
        } else {
          lmlog(lmlog_level_error, "no value provided to --report-format/-f option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }
      
      case 's': {
        if ( optarg && *optarg ) {
          struct tm     when;
          
          memset(&when, 0, sizeof(when));
          when.tm_isdst = -1;
          if ( strptime(optarg, "%Y-%m-%d %H:%M:%S%z", &when) ||
               strptime(optarg, "%Y-%m-%d %H:%M:%S", &when) ||
               strptime(optarg, "%Y-%m-%d %H:%M", &when) ||
               strptime(optarg, "%Y-%m-%d", &when) )
          {
            time_t      then = mktime(&when);
            
            THE_CONFIG->public.checked_time[0] = then;
            LMDEBUG("starting timestamp %s = %lld", optarg, (long long int)then);
          } else {
            lmlogf(lmlog_level_warn, "unrecognized date string: %s", optarg);
          }
        } else {
          lmlog(lmlog_level_error, "no value provided to --start-at/-s option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }
      
      case 'e': {
        if ( optarg && *optarg ) {
          struct tm     when;
          
          memset(&when, 0, sizeof(when));
          when.tm_isdst = -1;
          if ( strptime(optarg, "%Y-%m-%d %H:%M:%S%z", &when) ||
               strptime(optarg, "%Y-%m-%d %H:%M:%S", &when) ||
               strptime(optarg, "%Y-%m-%d %H:%M", &when) ||
               strptime(optarg, "%Y-%m-%d", &when) )
          {
            time_t      then = mktime(&when);
            
            THE_CONFIG->public.checked_time[1] = then;
            LMDEBUG("ending timestamp %s = %lld", optarg, (long long int)then);
          } else {
            lmlogf(lmlog_level_warn, "unrecognized date string: %s", optarg);
          }
        } else {
          lmlog(lmlog_level_error, "no value provided to --end-at/-e option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }
      
      case 0x80: {
        if ( optarg && *optarg ) {
          THE_CONFIG->public.match_feature = mempool_strdup(THE_CONFIG->pool, optarg);
        } else {
          lmlog(lmlog_level_error, "no value provided to --match-feature option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }
      
      case 0x81: {
        if ( optarg && *optarg ) {
          THE_CONFIG->public.match_vendor = mempool_strdup(THE_CONFIG->pool, optarg);
        } else {
          lmlog(lmlog_level_error, "no value provided to --match-vendor option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }
      
      case 0x82: {
        if ( optarg && *optarg ) {
          THE_CONFIG->public.match_version = mempool_strdup(THE_CONFIG->pool, optarg);
        } else {
          lmlog(lmlog_level_error, "no value provided to --match-version option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }
      
      case 'H':
        THE_CONFIG->public.should_show_headers = false;
        break;
      
      case 'F':
      	THE_CONFIG->public.fields_for_display |= field_selection_feature_id;
      	break;
      
      case 'U':
      	THE_CONFIG->public.fields_for_display &= ~field_selection_counts;
      	break;
      
      case 'P':
      	THE_CONFIG->public.fields_for_display &= ~field_selection_percentage;
      	break;
      
      case 'E':
      	THE_CONFIG->public.fields_for_display &= ~field_selection_expire_timestamp;
      	break;
      
      case 'T':
      	THE_CONFIG->public.fields_for_display &= ~field_selection_check_timestamp;
      	break;

#endif

#ifdef LMDB_APPLICATION_LS

      case 'n':
        THE_CONFIG->public.name_only = true;
        break;
      
      case 'i': {
        if ( optarg && *optarg ) {
          char      *endp;
          long      value = strtol(optarg, &endp, 10);
          
          if ( (value > 0) && (value < INT_MAX) && (endp > optarg) ) {
            THE_CONFIG->public.match_id = value;
          } else {
            lmlogf(lmlog_level_error, "invalid argument to --match-id:  %s\n", optarg);
            lmconfig_dealloc(the_config);
            return NULL;
          }
        } else {
          lmlog(lmlog_level_error, "no value provided to --match-id option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }

      case 0x80: {
        if ( optarg && *optarg ) {
          THE_CONFIG->public.match_feature = mempool_strdup(THE_CONFIG->pool, optarg);
        } else {
          lmlog(lmlog_level_error, "no value provided to --match-feature option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }
      
      case 0x81: {
        if ( optarg && *optarg ) {
          THE_CONFIG->public.match_vendor = mempool_strdup(THE_CONFIG->pool, optarg);
        } else {
          lmlog(lmlog_level_error, "no value provided to --match-vendor option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }
      
      case 0x82: {
        if ( optarg && *optarg ) {
          THE_CONFIG->public.match_version = mempool_strdup(THE_CONFIG->pool, optarg);
        } else {
          lmlog(lmlog_level_error, "no value provided to --match-version option\n");
          lmconfig_dealloc(the_config);
          return NULL;
        }
        break;
      }

#endif

    }

  }
  return (lmconfig*)THE_CONFIG;
}

//

void
lmconfig_dealloc(
  lmconfig      *the_config
)
{
  __lmconfig_dealloc((lmconfig_private*)the_config);
}
