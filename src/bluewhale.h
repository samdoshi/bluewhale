#ifndef _BLUEWHALE_H_
#define _BLUEWHALE_H_

#include <stdint.h>

typedef enum { mTrig, mCV, mSeries } edit_mode_t;

typedef enum { mForward, mReverse, mDrunk, mRandom } step_mode_t;

typedef enum { mPulse, mGate } tr_mode_t;

typedef struct {
    uint8_t loop_start;
    uint8_t loop_end;
    uint8_t loop_len;
    uint8_t loop_dir;
    uint16_t step_choice;       // enabled steps - maybe bool step_choice[16]?
    tr_mode_t tr_mode;          // pulse or gate
    step_mode_t step_mode;      // forward / reverse / etc
    uint8_t tr_steps[16];       //  the state of the 4 triggers for each step
    uint8_t tr_step_probs[16];  // the probability of the a trigger for a step
    uint16_t cv_values[16];  // the 16 defined CV values shared between A and B
    uint16_t cv_steps[2][16];  // the selected value for each step of CV A and B
    uint8_t cv_probs[2][16];   // the probability for each step of CV A and B
} whale_pattern_t;

typedef struct {
    whale_pattern_t wp[16];
    uint16_t series_list[64];
    uint8_t series_start;
    uint8_t series_end;
    uint8_t tr_mute[4];
    uint8_t cv_mute[2];
} whale_set_t;


extern whale_set_t w;
extern uint8_t preset_mode;
extern uint8_t preset_select;
extern edit_mode_t edit_mode;
extern uint8_t front_timer;
extern uint8_t glyph[8];
extern const uint16_t SCALES[24][16];

extern void bw_monome_connect(void);
extern void bw_set_param(uint16_t param);
extern void bw_key_timer(void);

extern void bw_clock(uint8_t phase);
extern void refresh(void);
extern void bw_key_press(uint8_t x, uint8_t y, uint8_t z);

#endif
