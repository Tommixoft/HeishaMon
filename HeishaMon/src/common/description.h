#ifndef DESCRIPTION_H
#define DESCRIPTION_H

/*
  Looks up the text for a decoded value in a description table such as
  {"2", "Disabled", "Enabled"}. The first entry is the number of names
  that follow; "0" means the table holds a single unit (e.g. "°C") that
  applies to every value. Values without a name return `unknown`.
*/
const char *getDescription(const char *const *description, int value, const char *unknown);

#endif
