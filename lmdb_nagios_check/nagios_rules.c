/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * nagios_rules.c
 *
 * A list of rules mapping license tuples (feature:vendor:version) to
 * inclusion/exclusion and alternate warning/critical levels.
 *
 */

#include "nagios_rules.h"
#include "fscanln.h"
#include "lmlog.h"
#include "mempool.h"
#include "util_fns.h"

#include <fnmatch.h>
#include <regex.h>

typedef enum {
  nagios_rule_type_unknown = 0,
  nagios_rule_type_string,
  nagios_rule_type_regex,
  nagios_rule_type_pattern,
  nagios_rule_type_default
} nagios_rule_type;

//

typedef struct _nagios_rule_node {
  nagios_rule_type    rule_type;
  union {
    char      *string;
    regex_t   regex;
    char      *pattern;
  } rule_data;
  nagios_rule_result  result;
  nagios_threshold		warn, crit;

  struct _nagios_rule_node  *next;
} nagios_rule_node;

//

nagios_rule_node*
__nagios_rule_node_alloc_with_string(
  const char          *string,
  size_t              string_len
)
{
  nagios_rule_node    *new_node = malloc(sizeof(nagios_rule_node) + string_len + 1);
  
  if ( new_node ) {
    memset(new_node, 0, sizeof(nagios_rule_node));
    new_node->rule_type = nagios_rule_type_string;
    new_node->rule_data.string = ((void*)new_node) + sizeof(nagios_rule_node);
    strncpy(new_node->rule_data.string, string, string_len);
    new_node->rule_data.string[string_len] = '\0';
    new_node->result = nagios_rule_result_decline;
    new_node->warn = new_node->crit = nagios_threshold_default;
  }
  return new_node;
}

//

nagios_rule_node*
__nagios_rule_node_alloc_with_pattern(
  const char          *pattern,
  size_t              pattern_len
)
{
  nagios_rule_node    *new_node = malloc(sizeof(nagios_rule_node) + pattern_len + 1);
  
  if ( new_node ) {
    memset(new_node, 0, sizeof(nagios_rule_node));
    new_node->rule_type = nagios_rule_type_pattern;
    new_node->rule_data.pattern = ((void*)new_node) + sizeof(nagios_rule_node);
    strncpy(new_node->rule_data.pattern, pattern, pattern_len);
    new_node->rule_data.pattern[pattern_len] = '\0';
    new_node->result = nagios_rule_result_decline;
    new_node->warn = new_node->crit = nagios_threshold_default;
  }
  return new_node;
}

//

nagios_rule_node*
__nagios_rule_node_alloc_with_regex(
  const char          *regex
)
{
  nagios_rule_node    *new_node = malloc(sizeof(nagios_rule_node));
  
  if ( new_node ) {
    int               rc;
    
    memset(new_node, 0, sizeof(nagios_rule_node));
    new_node->rule_type = nagios_rule_type_regex;
    new_node->result = nagios_rule_result_decline;
    new_node->warn = new_node->crit = nagios_threshold_default;
    if ( (rc = regcomp(&new_node->rule_data.regex, regex, REG_EXTENDED)) != 0 ) {
      lmlogf(lmlog_level_error, "failed to compile regex '%s' (rc = %d)", regex, rc);
      free((void*)new_node);
      new_node = NULL;
    }
  }
  return new_node;
}

//

nagios_rule_node*
__nagios_rule_node_alloc()
{
  nagios_rule_node    *new_node = malloc(sizeof(nagios_rule_node));
  
  if ( new_node ) {
    int               rc;
    
    memset(new_node, 0, sizeof(nagios_rule_node));
    new_node->rule_type = nagios_rule_type_default;
    new_node->result = nagios_rule_result_decline;
    new_node->warn = new_node->crit = nagios_threshold_default;
  }
  return new_node;
}

//

void
__nagios_rule_node_free(
  nagios_rule_node    *rule_node
)
{
  if ( rule_node->rule_type == nagios_rule_type_regex ) regfree(&rule_node->rule_data.regex);
  free((void*)rule_node);
}

//

void
__nagios_rule_node_summary(
	nagios_rule_node		*rule_node
)
{
	fprintf(stderr, "nagios_rule_node@%p { type=%d; result=%d; warn=(%d;%f); crit=(%d;%f) } ",
			rule_node, rule_node->rule_type, rule_node->result, rule_node->warn.type, rule_node->warn.value, rule_node->crit.type, rule_node->crit.value
		);
	switch ( rule_node->rule_type ) {
		case nagios_rule_type_string:
			fprintf(stderr, "\"%s\"", rule_node->rule_data.string);
			break;
		case nagios_rule_type_pattern:
			fprintf(stderr, "\"%s\"", rule_node->rule_data.pattern);
			break;
		case nagios_rule_type_regex:
			fprintf(stderr, "%p", &rule_node->rule_data.regex);
			break;
		case nagios_rule_type_default:
		case nagios_rule_type_unknown:
    	break;
	}
}

//

nagios_rule_node*
__nagios_rule_node_create_with_string(
  const char      *node_definition,
  mempool_ref     pool
)
{
  nagios_rule_node    *new_node = NULL;
  const char          *word;
  nagios_rule_result  rc = nagios_rule_result_decline;
  
  if ( str_next_word(&node_definition, pool, &word) == str_next_word_ok ) {
    double            		warn = -1.0, crit = -1.0;
    nagios_threshold_type	warn_type = nagios_threshold_type_fraction, crit_type = nagios_threshold_type_fraction;
    const char*       		arg = NULL;
    nagios_rule_type  		type = nagios_rule_type_unknown;
    
    if ( strcasecmp(word, "include") == 0 ) {
      rc = nagios_rule_result_include;
    }
    else if ( strcasecmp(word, "exclude") == 0 ) {
      rc = nagios_rule_result_exclude;
    }
    else {
      lmlogf(lmlog_level_error, "invalid nagios rule definition result: %s", word);
      return NULL;
    }
    
    // Process each word:
    while ( str_next_word(&node_definition, pool, &word) == str_next_word_ok ) {
      if ( strncmp(word, "string=", 7) == 0 ) {
        type = nagios_rule_type_string;
        arg = word + 7;
      }
      else if ( strncmp(word, "pattern=", 8) == 0 ) {
        type = nagios_rule_type_pattern;
        arg = word + 8;
      }
      else if ( strncmp(word, "regex=", 6) == 0 ) {
        type = nagios_rule_type_regex;
        arg = word + 6;
      }
      else if ( strncmp(word, "warn=", 5) == 0 ) {
        char    *endp;
        double  value;
        
        word += 5;
        // What sort of value are we looking at:
        if ( strchr(word, '.') ) {
        	// Fractional value:
        	value = strtod(word, &endp);
					if ( endp > word ) {
						if ( *endp == '%' ) value *= 0.01;
						warn = value;
						warn_type = nagios_threshold_type_fraction;
					} else {
						lmlogf(lmlog_level_error, "invalid nagios rule definition warn level: %s", word);
						return NULL;
					}
				} else {
        	// Possibly a count if no % follows:
        	value = strtod(word, &endp);
					if ( endp > word ) {
						if ( *endp == '%' ) {
							value *= 0.01;
							warn_type = nagios_threshold_type_fraction;
						} else {
							warn_type = nagios_threshold_type_count;
						}
						warn = value;
					} else {
						lmlogf(lmlog_level_error, "invalid nagios rule definition warn level: %s", word);
						return NULL;
					}
				}
      }
      else if ( strncmp(word, "crit=", 5) == 0 ) {
        char    *endp;
        double  value;
        
        word += 5;
        // What sort of value are we looking at:
        if ( strchr(word, '.') ) {
        	// Fractional value:
        	value = strtod(word, &endp);
					if ( endp > word ) {
						if ( *endp == '%' ) value *= 0.01;
						crit = value;
						crit_type = nagios_threshold_type_fraction;
					} else {
						lmlogf(lmlog_level_error, "invalid nagios rule definition crit level: %s", word);
						return NULL;
					}
				} else {
        	// Possibly a count if no % follows:
        	value = strtod(word, &endp);
					if ( endp > word ) {
						if ( *endp == '%' ) {
							value *= 0.01;
							crit_type = nagios_threshold_type_fraction;
						} else {
							crit_type = nagios_threshold_type_count;
						}
						crit = value;
					} else {
						lmlogf(lmlog_level_error, "invalid nagios rule definition crit level: %s", word);
						return NULL;
					}
				}
			}
    }
    switch ( type ) {
    
      case nagios_rule_type_string:
        new_node = __nagios_rule_node_alloc_with_string(arg, strlen(arg));
        break;
        
      case nagios_rule_type_pattern:
        new_node = __nagios_rule_node_alloc_with_pattern(arg, strlen(arg));
        break;
        
      case nagios_rule_type_regex:
        new_node = __nagios_rule_node_alloc_with_regex(arg);
        break;
        
      case nagios_rule_type_default:
        break;
    
      case nagios_rule_type_unknown:
        lmlogf(lmlog_level_error, "invalid nagios rule definition: %s", node_definition);
        break;
    
    }
    if ( new_node ) {
      new_node->warn.type = warn_type;
      new_node->warn.value = warn;
      new_node->crit.type = crit_type;
      new_node->crit.value = crit;
      new_node->result = rc;
    }
  }
  return new_node;
}

//

nagios_rule_result
__nagios_rule_node_apply(
  nagios_rule_node    *rule_node,
  const char          *license_tuple
)
{
  nagios_rule_result  rc = nagios_rule_result_decline;
  
  switch ( rule_node->rule_type ) {
  
    case nagios_rule_type_string:
      if ( strcmp(license_tuple, rule_node->rule_data.string) == 0 ) {
        rc = rule_node->result;
      }
      break;
  
    case nagios_rule_type_pattern:
      if ( fnmatch(rule_node->rule_data.pattern, license_tuple, 0) == 0 ) {
        rc = rule_node->result;
      }
      break;
    
    case nagios_rule_type_regex:
      if ( regexec(&rule_node->rule_data.regex, license_tuple, 0, NULL, 0) == 0 ) {
        rc = rule_node->result;
      }
      break;
      
    case nagios_rule_type_default:
      rc = rule_node->result;
      break;
    
    case nagios_rule_type_unknown:
      break;
  
  }
  return rc;
}

//
#if 0
#pragma mark -
#endif
//

typedef struct _nagios_rules {
	nagios_rule_matching	matching;
  nagios_rule_node      *rules;
} nagios_rules;

//

nagios_rules_ref
nagios_rules_create_with_file(
  const char          *file
)
{
  fscanln_ref   			scanner = fscanln_create_with_file(file);
	nagios_rules				*new_rules = NULL;
	
  if ( scanner ) {
		new_rules = malloc(sizeof(nagios_rules));
		if ( new_rules ) {
			const char        *line;
			bool              ok = true;
			mempool_ref				pool = mempool_alloc();
			
			if ( pool ) {
				nagios_rule_node	*last_node = NULL;
				
				new_rules->matching = nagios_rule_matching_first;
				new_rules->rules = NULL;
				while ( ok && fscanln_get_line(scanner, &line, NULL) ) {
					nagios_rule_node		*next_node;
				
					/* Drop any leading whitespace: */
					while ( is_not_eol(*line) && isspace(*line) ) line++;

					/* Comment? */
					if ( is_eol(*line) || (*line == '#') ) continue;
					
					/* Option line? */
					if ( strncasecmp(line, "option", 6) == 0 ) {
						const char		*word;
						
						line += 6;
						if ( str_next_word(&line, pool, &word) == str_next_word_ok ) {
							if ( strcasecmp(word, "matching") == 0 ) {
								if ( str_next_word(&line, pool, &word) == str_next_word_ok ) {
									/* What style of matching? */
									if ( strcasecmp(word, "first") == 0 ) {
										new_rules->matching = nagios_rule_matching_first;
									}
									else if ( strcasecmp(word, "last") == 0 ) {
										new_rules->matching = nagios_rule_matching_last;
									}
								}
							}
						}
					} else {
						/* Attempt to parse and create a node: */
						next_node = __nagios_rule_node_create_with_string(line, pool);
						if ( next_node ) {
							if ( ! new_rules->rules ) {
								new_rules->rules = next_node;
							} else {
								last_node->next = next_node;
							}
							last_node = next_node;
						}
					}
					mempool_reset(pool);
				}
				mempool_dealloc(pool);
			} else {
				free((void*)new_rules);
				new_rules = NULL;
			}
		}
		fscanln_release(scanner);
	}
	return new_rules;
}

//

void
nagios_rules_release(
	nagios_rules_ref	 the_rules
)
{
	nagios_rule_node		*n = the_rules->rules;
	
	while ( n ) {
		nagios_rule_node	*next = n->next;
		
		__nagios_rule_node_free(n);
		n = next;
	}
	free((void*)the_rules);
}

//

nagios_rule_result
nagios_rules_apply(
	nagios_rules_ref	the_rules,
	const char				*license_tuple,
	nagios_threshold	*warn,
	nagios_threshold	*crit
)
{
	nagios_rule_node		*n = the_rules->rules;
	nagios_rule_result	rc = nagios_rule_result_decline;
	nagios_threshold		W = nagios_threshold_default;
	nagios_threshold		C = nagios_threshold_default;
	
	while ( n ) {
		nagios_rule_result	rule_rc = __nagios_rule_node_apply(n, license_tuple);
		
		if ( rule_rc != nagios_rule_result_decline ) {
			rc = n->result;
			W = n->warn;
			C = n->crit;
			if ( the_rules->matching == nagios_rule_matching_first ) break;
		}
		n = n->next;
	}
	if ( rc != nagios_rule_result_decline ) {
		*warn = W;
		*crit = C;
	}
	return rc;
}

//

void
nagios_rules_summary(
	nagios_rules_ref		the_rules
)
{
	nagios_rule_node		*n = the_rules->rules;
	
	fprintf(stderr, "nagios_rules@%p ( matching=%d ) {\n", the_rules, the_rules->matching);
	while ( n ) {
		fprintf(stderr, "  ");
		__nagios_rule_node_summary(n);
		fprintf(stderr, ",\n");
		n = n->next;
	}
	fprintf(stderr, "}\n");
}

//
#if 0
#pragma mark -
#endif
//

nagios_threshold nagios_threshold_default = { .type = nagios_threshold_type_fraction, .value = -1.0 };

//

bool
nagios_threshold_is_default(
	nagios_threshold		*a_threshold
)
{
	return ( a_threshold->value < 0.0 ) ? true : false;
}

//

bool
nagios_threshold_match(
	nagios_threshold		*a_threshold,
	int								  in_use,
	int									issued
)
{
	switch ( a_threshold->type ) {
	
		case nagios_threshold_type_fraction: {
			double		frac = (double)in_use / (double)issued;
		
			if ( frac >= a_threshold->value ) return true;
			break;
		}
		
		case nagios_threshold_type_count: {
			if ( in_use >= a_threshold->value ) return true;
			break;
		}
	
	}
	return false;
}
