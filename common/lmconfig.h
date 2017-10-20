/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * lmconfig.h
 *
 * Shared and per-utility program configuration options.  An API for
 * parsing configuration files and command line options is present.
 *
 */
 
#ifndef __LMCONFIG_H__
#define __LMCONFIG_H__

#include "config.h"

/*
 * The nagios check utility can include a list of license tuple
 * matching rules:
 */
#ifdef LMDB_APPLICATION_NAGIOS_CHECK
# include "nagios_rules.h"
#endif

/*
 * The report utility uses report parameters:
 */
#ifdef LMDB_APPLICATION_REPORT
# include "lmdb.h"
#endif

#ifndef LMDB_DISABLE_RRDTOOL
extern const char   *lmdb_rrd_repodir;
#endif

/*!
  @constant lmdb_version_str
  Version of this program, as a string constant.
*/
extern const char   *lmdb_version_str;

/*!
	@constant lmdb_prefix_dir
	String constant holding the path in which the lmdb software is
	installed.
*/
extern const char   *lmdb_prefix_dir;

/*!
	@constant lmdb_sysconf_dir
	String constant holding the path in which the lmdb configuration
  files are found.
*/
extern const char   *lmdb_sysconf_dir;

/*!
	@constant lmdb_state_dir
	String constant holding the path in which the lmdb local state
  is found.
*/
extern const char   *lmdb_state_dir;

/*!
	@constant lmdb_default_conf_file
	String constant holding the path to the default configuration file
	for the programs.
*/
extern const char   *lmdb_default_conf_file;

#ifdef LMDB_APPLICATION_CLI
/*!
	@typedef lmstat_interface_kind
	Type that enumerates the methods by which lmstat information can
	be gathered:
	
		lmstat_interface_kind_static_output
			Output from lmstat is read from a file.
		
		lmstat_interface_kind_command
			A command string is passed to a shell and the output from that
			shell is scanned.
		
		lmstat_interface_kind_exec
			A command and arguments to that command (a'la execve()) are forked
			and executed and the stdout and stderr are scanned.
*/
typedef enum {
  lmstat_interface_kind_unset = 0,
  lmstat_interface_kind_static_output,
  lmstat_interface_kind_command,
  lmstat_interface_kind_exec
} lmstat_interface_kind;
#endif

#ifdef LMDB_APPLICATION_REPORT
/*!
	@typedef report_format
	Type that enumerates the formats which the lmdb_report tool can export to.
*/
typedef enum {
  report_format_column = 0,
  report_format_csv,
  //
  report_format_max
} report_format;

/*!
	@typedef field_selection
	Type that enumerates the bits that enable/disable specific fields for display
*/
enum {
	field_selection_feature_id				= 1 << 0,
	field_selection_counts						= 1 << 1,
	field_selection_percentage				= 1 << 2,
	field_selection_expire_timestamp	= 1 << 3,
	field_selection_check_timestamp		= 1 << 4,
	//
	field_selection_default						= 0x1e
};
typedef unsigned int field_selection;

#endif

/*!
	@typedef lmconfig
	Data structure that holds configuration options for each of the
	programs in this suite.  Note that this data structure is just
	the public portion of that which this API allocates!  The address
	of stack-based instances of this data structure should NEVER be
	passed to the functions in this API.
	
	common options
	==============
	
    base_config_path
      Filesystem path of the baseline configuration file that
      should be read; defaults to lmdb_default_conf_file
      
		license_db_path
			Filesystem path of the SQLite database file containing
			the feature definitions and counts to be used
		
	lmdb_cli options
	================
	
		flexlm_license_path
			Filesystem path of a FLEXlm license configuration file
			to be scanned for feature definitions
		
		lmstat_interface_kind
			The method by which lmstat information is accessed;
			determines which member of the lmstat_interface union
			is in use
		
		lmstat_interface
			Type union spanning the different lmstat_interface_kind
			elements:
			
				lmstat_interface_kind_static_output
					the static_output field holds the path to the file
					to be scanned
				
				lmstat_interface_kind_command
					the command field holds the string to be passed to
					a shell for generating lmstat output to scan
				
				lmstat_interface_kind_exec
					the exec field points to an array of strings (terminal
					array element is a NULL pointer, zeroeth element is the
					executable path) that will be forked and executed using
					execve() to generate lmstat output to scan
      
    rrd_repodir
      Directory that contains RRD files for the features; only
      present if the library is compiled with RRD support enabled
		
	lmdb_nagios_check options
	=========================
	
		nagios_rules
			a list of rules that associate inclusion/exclusion states and
			non-standard warning/critical thresholds with feature-tuple strings,
			patterns, and regular expressions
		
		nagios_default_warn
			the default warning threshold (as a usage percentage) that should
			be applied to features
		
		nagios_default_crit
			the default critical threshold (as a usage percentage) that should
			be applied to features
  
  lmdb_report
  ===========
  
    report_aggregate
      the aggregation method that should be applied to data present in the
      report; defaults to lmdb_usage_report_aggregate_total
    
    report_range
      the abstract date range that should be applied to data present in the
      report; defaults to lmdb_usage_report_range_last_day
    
    should_show_headers
      enables/disables the display of a header line showing column labels for
      report data; default is true
      
    fields_for_display
      bit vector enabling (1) and disabling (0) display of specific fields in
      the report; default is all fields except the feature_id
    
    report_format
      output format for the report; defaults to report_format_column
      
    checked_time
      an array of two Unix timestamps, (start, end), that limit the starting
      and/or ending time that will be considered for the report; if only one is
      set the other is inferred from the report_range that is chosen
      
    match_feature
      pattern string used to limit which features are chosen for the report
      
    match_vendor
      pattern string used to limit which vendors are chosen for the report
      
    match_version
      pattern string used to limit which versions are chosen for the report
  
  ls
  ==
  
    name_only
      only display the name of the feature, not the vendor and version
      
    match_id
      a specific feature id to find
      
    match_feature
      pattern string used to limit which features are shown
    
    match_vendor
      pattern string used to limit which vendors are shown
      
    match_version
      pattern string used to limit which versions are shown
      
*/
typedef struct _lmconfig {
	// options common to all programs:
  const char              *base_config_path;
  const char              *license_db_path;

  
#ifdef LMDB_APPLICATION_CLI
  // options specific to lmdb_cli:
  const char              *flexlm_license_path;
  lmstat_interface_kind   lmstat_interface_kind;
  union {
    const char    *static_output;
    const char    *command;
    char * const  *exec;
  } lmstat_interface;
# ifndef LMDB_DISABLE_RRDTOOL
  bool                    should_update_rrds;
  const char              *rrd_repodir;
# endif
#endif

#ifdef LMDB_APPLICATION_NAGIOS_CHECK
  // options specific to lmdb_nagios_check:
  nagios_rules_ref				nagios_rules;
  nagios_threshold				nagios_default_warn, nagios_default_crit;
#endif

#ifdef LMDB_APPLICATION_REPORT
	// options specific to lmdb_report:
	lmdb_usage_report_aggregate		report_aggregate;
	lmdb_usage_report_range				report_range;
	bool													should_show_headers;
	field_selection								fields_for_display;
	report_format									report_format;
  time_t                        checked_time[2];
  const char                    *match_feature;
  const char                    *match_vendor;
  const char                    *match_version;
#endif

#ifdef LMDB_APPLICATION_LS
	// options specific to lmdb_ls:
  bool                          name_only;
  int                           match_id;
  const char                    *match_feature;
  const char                    *match_vendor;
  const char                    *match_version;
#endif

} lmconfig;

/*!
	@function lmconfig_update_with_file
	
	If the file at path can be opened, scan it for key-value pairs pertinent to
	this program and set the corresponding fields in the_config.
	
	If the_config is NULL, a new empty configuration data structure is first
	allocated.
	
	The pointer to the configuration data structure is returned.  The data
	structure is deallocated and NULL returned if any error occurs.
*/
lmconfig* lmconfig_update_with_file(lmconfig *the_config, const char *path);
/*!
	@function lmconfig_update_with_options
	
	The array of option strings at argv with length argc is processed, with
	fields pertinent to this program being set in the_config.
	
	If the_config is NULL, a new empty configuration data structure is first
	allocated.
	
	The pointer to the configuration data structure is returned.  The data
	structure is deallocated and NULL returned if any error occurs.
*/
lmconfig* lmconfig_update_with_options(lmconfig *the_config, int argc, char * const *argv);
/*!
	@function lmconfig_dealloc
	
	Dispose of the_config.
*/
void lmconfig_dealloc(lmconfig *the_config);

#endif /* __LMCONFIG_H__ */
