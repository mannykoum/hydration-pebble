#include "format.h"
#include <stdio.h>
#include <stdlib.h>

int unit_multiplier(Unit unit) {
  switch(unit) {
    case UNIT_CUPS: return 240;
    case UNIT_PINTS: return 473;
    case UNIT_ML:
    default: return 1;
  }
}

void format_amount(const PersistedState *state, int ml, char *buffer, size_t size) {
  int mul = unit_multiplier((Unit)state->unit);
  int abs_ml = abs(ml);
  int whole = abs_ml / mul;
  int frac = (abs_ml % mul) * 10 / mul;
  char sign = ml < 0 ? '-' : '+';
  const char *suffix = state->unit == UNIT_ML ? "ml" : (state->unit == UNIT_CUPS ? "c" : "pt");
  if (state->unit == UNIT_ML) {
    snprintf(buffer, size, "%+d%s", ml, suffix);
  } else {
    if (frac == 0) {
      snprintf(buffer, size, "%c%d%s", sign, whole, suffix);
    } else {
      snprintf(buffer, size, "%c%d.%d%s", sign, whole, frac, suffix);
    }
  }
}
