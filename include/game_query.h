#pragma once
#include <switch.h>

/* Resolve the application process directly, independently of the title catalog. */
Result game_query_current(u64 *pid, u64 *application_id);

/* Terminate only the PID returned by the current-application query. */
Result game_query_close_current(void);
