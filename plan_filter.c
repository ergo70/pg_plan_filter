
#include "postgres.h"


#include <float.h>

#include "optimizer/planner.h"
#include "utils/guc.h"

PG_MODULE_MAGIC;

static double statement_cost_limit = 0.0;

static planner_hook_type prev_planner_hook = NULL;

static PlannedStmt * limit_func(Query *parse, 
						 int cursorOptions, 
						 ParamListInfo boundParams);

void		_PG_init(void);
void		_PG_fini(void);

/*
 * Module load callback
 */
void
_PG_init(void)
{
    /* Define custom GUC variable. */
    DefineCustomRealVariable("plan_filter.statement_cost_limit",
							 "Sets the maximum allowed plan cost above which "
							 "a statement will not run.",
							 "Zero turns this feature off.",
							 &statement_cost_limit,
							 0.0,
							 0.0, DBL_MAX,
							 PGC_SUSET,
							 0, /* no flags required */
							 NULL,
							 NULL,
							 NULL);
	
	/* install the hook */
	prev_planner_hook = planner_hook;
	planner_hook = limit_func;
		
}

/*
 * Module unload callback
 */
void
_PG_fini(void)
{
    /* Uninstall hook. */
    planner_hook = prev_planner_hook;
}

static PlannedStmt *
limit_func(Query *parse, int cursorOptions, ParamListInfo boundParams)
{
	PlannedStmt *result;

	/* this way we can daisy chain planner hooks if necessary */
    if (prev_planner_hook != NULL)
        result = (*prev_planner_hook) (parse, cursorOptions, boundParams);
    else
        result = standard_planner(parse, cursorOptions, boundParams);

	if (statement_cost_limit > 0.0 && 
		result->planTree->total_cost > statement_cost_limit)
 		ereport(ERROR,
 				(errcode(ERRCODE_STATEMENT_TOO_COMPLEX),
 				 errmsg("plan cost limit exceeded"),
 				 errhint("The plan for your query shows that it would probably "
						 "have an excessive run time. This may be due to a "
						 "logic error in the SQL, or it maybe just a very "
						 "costly query. Rewrite your query or increase the "
						 "configuration parameter "
						 "\"plan_filter.statement_cost_limit\".")));

	return result;

}
