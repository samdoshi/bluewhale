#ifndef _HARDWARE_H_
#define _HARDWARE_H_

#include "bluewhale.h"

typedef const struct {
    uint8_t fresh;
    edit_mode_t edit_mode;
    uint8_t preset_select;
    uint8_t glyph[8][8];
    whale_set_t w[8];
} nvram_data_t;

extern nvram_data_t flashy;

extern uint8_t flash_is_fresh(void);
extern void flash_unfresh(void);
extern void flash_write(void);
extern void flash_read(void);

#endif
