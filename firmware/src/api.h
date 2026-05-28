#pragma once
#include "data.h"

void api_init(void);

// Fetch usage data from the configured endpoint.
// Returns true and populates *out on success.
bool api_fetch(UsageData* out);

bool api_last_ok(void);
unsigned long api_last_fetch_ms(void);
