#pragma once
#include "../state/state.h"

// Formatting utilities
int unit_multiplier(Unit unit);
void format_amount(const PersistedState *state, int ml, char *buffer, size_t size);
