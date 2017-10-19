/*
 * lmdb - Simple database to count FLEXlm licenses/features
 * nagios_rules.h
 *
 * A list of rules mapping license tuples (feature:vendor:version) to
 * inclusion/exclusion and alternate warning/critical levels.
 *
 */

#ifndef __NAGIOS_RULES_H__
#define __NAGIOS_RULES_H__

#include "config.h"

typedef enum {
  nagios_rule_result_include,
  nagios_rule_result_exclude,
  nagios_rule_result_decline
} nagios_rule_result;

typedef enum {
	nagios_rule_matching_first,
	nagios_rule_matching_last
} nagios_rule_matching;

typedef enum {
	nagios_threshold_type_fraction,
	nagios_threshold_type_count
} nagios_threshold_type;

typedef struct {
	nagios_threshold_type		type;
	double									value;
} nagios_threshold;

extern nagios_threshold nagios_threshold_default;

static inline nagios_threshold nagios_threshold_make(nagios_threshold_type type, double value)
{
	nagios_threshold		t = { .type = type, .value = value };
	return t;
}

bool nagios_threshold_is_default(nagios_threshold *a_threshold);
bool nagios_threshold_match(nagios_threshold *a_threshold, int in_use, int issued);

typedef struct _nagios_rules * nagios_rules_ref;

nagios_rules_ref nagios_rules_create_with_file(const char *file);
void nagios_rules_release(nagios_rules_ref the_rules);

nagios_rule_matching nagios_rules_get_matching(nagios_rules_ref the_rules);
void nagios_rules_set_matching(nagios_rules_ref the_rules, nagios_rule_matching matching);

nagios_rule_result nagios_rules_apply(nagios_rules_ref the_rules, const char *license_tuple, nagios_threshold *warn, nagios_threshold *crit);

void nagios_rules_summary(nagios_rules_ref the_rules);

#endif /* __NAGIOS_RULES_H__ */
