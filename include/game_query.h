#pragma once
#include <switch.h>

/* Resolve the application process directly, independently of the title catalog. */
Result game_query_current(u64 *pid, u64 *application_id);
