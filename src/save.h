#ifndef GBCC_SAVE_H
#define GBCC_SAVE_H

#include "gbcc.h"

void gbcc_save(struct gbcc *gbc);
void gbcc_load(struct gbcc *gbc);
void gbcc_save_state(struct gbcc *gbc);
void gbcc_load_state(struct gbcc *gbc);
bool gbcc_check_savestate(struct gbcc *gbc, int state);

#endif /* GBCC_SAVE_H */
