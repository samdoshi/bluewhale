/* issues

- preset save blip with internal timer vs ext trig

*/

#include <stdio.h>

// asf
#include "compiler.h"
#include "delay.h"
#include "flashc.h"
#include "gpio.h"
#include "intc.h"
#include "pm.h"
#include "preprocessor.h"
#include "print_funcs.h"
#include "spi.h"
#include "sysclk.h"

// skeleton
#include "adc.h"
#include "events.h"
#include "ftdi.h"
#include "i2c.h"
#include "init_common.h"
#include "init_trilogy.h"
#include "monome.h"
#include "timers.h"
#include "types.h"
#include "util.h"

// this
#include "conf_board.h"
#include "ii.h"


#define FIRSTRUN_KEY 0x22


const uint16_t SCALES[24][16] = {

    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },  // ZERO
    { 0, 68, 136, 170, 238, 306, 375, 409, 477, 545, 579, 647, 715, 784, 818,
      886 },  // ionian [2, 2, 1, 2, 2, 2, 1]
    { 0, 68, 102, 170, 238, 306, 340, 409, 477, 511, 579, 647, 715, 750, 818,
      886 },  // dorian [2, 1, 2, 2, 2, 1, 2]
    { 0, 34, 102, 170, 238, 272, 340, 409, 443, 511, 579, 647, 681, 750, 818,
      852 },  // phrygian [1, 2, 2, 2, 1, 2, 2]
    { 0, 68, 136, 204, 238, 306, 375, 409, 477, 545, 613, 647, 715, 784, 818,
      886 },  // lydian [2, 2, 2, 1, 2, 2, 1]
    { 0, 68, 136, 170, 238, 306, 340, 409, 477, 545, 579, 647, 715, 750, 818,
      886 },  // mixolydian [2, 2, 1, 2, 2, 1, 2]
    { 0, 68, 136, 170, 238, 306, 340, 409, 477, 545, 579, 647, 715, 750, 818,
      886 },  // aeolian [2, 1, 2, 2, 1, 2, 2]
    { 0, 34, 102, 170, 204, 272, 340, 409, 443, 511, 579, 613, 681, 750, 818,
      852 },  // locrian [1, 2, 2, 1, 2, 2, 2]

    { 0, 34, 68, 102, 136, 170, 204, 238, 272, 306, 341, 375, 409, 443, 477,
      511 },  // chromatic
    { 0, 68, 136, 204, 272, 341, 409, 477, 545, 613, 682, 750, 818, 886, 954,
      1022 },  // whole
    { 0, 170, 340, 511, 681, 852, 1022, 1193, 1363, 1534, 1704, 1875, 2045,
      2215, 2386, 2556 },  // fourths
    { 0, 238, 477, 715, 954, 1193, 1431, 1670, 1909, 2147, 2386, 2625, 2863,
      3102, 3340, 3579 },  // fifths
    { 0, 17, 34, 51, 68, 85, 102, 119, 136, 153, 170, 187, 204, 221, 238,
      255 },  // quarter
    { 0, 8, 17, 25, 34, 42, 51, 59, 68, 76, 85, 93, 102, 110, 119,
      127 },  // eighth
    { 0, 61, 122, 163, 245, 327, 429, 491, 552, 613, 654, 736, 818, 920, 982,
      1105 },  // just
    { 0, 61, 130, 163, 245, 337, 441, 491, 552, 621, 654, 736, 828, 932, 982,
      1105 },  // pythagorean

    { 0, 272, 545, 818, 1090, 1363, 1636, 1909, 2181, 2454, 2727, 3000, 3272,
      3545, 3818, 4091 },  // equal 10v
    { 0, 136, 272, 409, 545, 682, 818, 955, 1091, 1228, 1364, 1501, 1637, 1774,
      1910, 2047 },  // equal 5v
    { 0, 68, 136, 204, 272, 341, 409, 477, 545, 613, 682, 750, 818, 886, 954,
      1023 },  // equal 2.5v
    { 0, 34, 68, 102, 136, 170, 204, 238, 272, 306, 340, 374, 408, 442, 476,
      511 },  // equal 1.25v
    { 0, 53, 118, 196, 291, 405, 542, 708, 908, 1149, 1441, 1792, 2216, 2728,
      3345, 4091 },  // log-ish 10v
    { 0, 136, 272, 409, 545, 682, 818, 955, 1091, 1228, 1364, 1501, 1637, 1774,
      1910, 2047 },  // log-ish 5v
    { 0, 745, 1362, 1874, 2298, 2649, 2941, 3182, 3382, 3548, 3685, 3799, 3894,
      3972, 4037, 4091 },  // exp-ish 10v
    { 0, 372, 681, 937, 1150, 1325, 1471, 1592, 1692, 1775, 1844, 1901, 1948,
      1987, 2020, 2047 }  // exp-ish 5v

};

typedef enum { mTrig, mMap, mSeries } edit_mode_t;

typedef enum { mForward, mReverse, mDrunk, mRandom } step_mode_t;

typedef enum { mPulse, mGate } tr_mode_t;

typedef struct {
    uint8_t loop_start, loop_end, loop_len, loop_dir;
    uint16_t step_choice;
    uint8_t cv_mode[2];
    tr_mode_t tr_mode;
    step_mode_t step_mode;
    uint8_t steps[16];
    uint8_t step_probs[16];
    uint16_t cv_values[16];
    uint16_t cv_steps[2][16];
    uint16_t cv_curves[2][16];
    uint8_t cv_probs[2][16];
} whale_pattern_t;

typedef struct {
    whale_pattern_t wp[16];
    uint16_t series_list[64];
    uint8_t series_start, series_end;
    uint8_t tr_mute[4];
    uint8_t cv_mute[2];
} whale_set_t;

typedef const struct {
    uint8_t fresh;
    edit_mode_t edit_mode;
    uint8_t preset_select;
    uint8_t glyph[8][8];
    whale_set_t w[8];
} nvram_data_t;

whale_set_t w;

uint8_t preset_mode, preset_select, front_timer;
uint8_t glyph[8];

edit_mode_t edit_mode;
uint8_t edit_cv_step, edit_cv_ch;
int8_t edit_cv_value;
uint8_t edit_prob, live_in, scale_select;
uint8_t pattern, next_pattern, pattern_jump;

uint8_t series_pos, series_next, series_jump, series_playing, scroll_pos;

uint8_t key_alt, key_meta, center;
uint8_t held_keys[32], key_count, key_times[256];
uint8_t keyfirst_pos, keysecond_pos;
int8_t keycount_pos, keycount_series, keycount_cv;

int8_t pos, cut_pos, next_pos, drunk_step, triggered;
uint8_t cv_chosen[2];
uint16_t cv0, cv1;

uint8_t param_accept, *param_dest8;
uint16_t clip;
uint16_t *param_dest;
uint8_t quantize_in;

uint8_t clock_phase;
uint16_t clock_time, clock_temp;
uint8_t series_step;

uint16_t adc[4];

const uint8_t kGridWidth = 16;
const uint8_t kMaxLoopLength = 15;  // this value should be 16 and the code
                                    // should be modified to that effect


// NVRAM data structure located in the flash array.
__attribute__((__section__(".flash_nvram"))) static nvram_data_t flashy;


////////////////////////////////////////////////////////////////////////////////
// prototypes

static void refresh(void);
static void refresh_preset(void);
static void clock(uint8_t phase);

// start/stop monome polling/refresh timers
extern void timers_set_monome(void);
extern void timers_unset_monome(void);

// check the event queue
static void check_events(void);

// handler protos
static void handler_None(int32_t data) {
    ;
    ;
}
static void handler_KeyTimer(int32_t data);
static void handler_Front(int32_t data);
static void handler_ClockNormal(int32_t data);
static void handler_ClockExt(int32_t data);

static void ww_process_ii(uint8_t *data, uint8_t l);

uint8_t flash_is_fresh(void);
void flash_unfresh(void);
void flash_write(void);
void flash_read(void);


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// application clock code

void clock(uint8_t phase) {
    if (phase) {
        gpio_set_gpio_pin(B10);


        if (pattern_jump) {
            pattern = next_pattern;
            next_pos = w.wp[pattern].loop_start;
            pattern_jump = 0;
        }

        // for series mode and delayed pattern change
        if (series_jump) {
            series_pos = series_next;
            if (series_pos == w.series_end)
                series_next = w.series_start;
            else {
                series_next++;
                if (series_next > 63) series_next = w.series_start;
            }

            uint count = 0;
            uint16_t found[16];
            for (uint8_t i = 0; i < 16; i++) {
                if ((w.series_list[series_pos] >> i) & 1) {
                    found[count] = i;
                    count++;
                }
            }

            if (count == 1)
                next_pattern = found[0];
            else {
                next_pattern = found[rnd() % count];
            }

            pattern = next_pattern;
            series_playing = pattern;
            if (w.wp[pattern].step_mode == mReverse)
                next_pos = w.wp[pattern].loop_end;
            else
                next_pos = w.wp[pattern].loop_start;

            series_jump = 0;
            series_step = 0;
        }

        pos = next_pos;

        whale_pattern_t *p = &w.wp[pattern];

        // live param record
        if (param_accept && live_in) {
            param_dest = &p->cv_curves[edit_cv_ch][pos];
            p->cv_curves[edit_cv_ch][pos] = adc[1];
        }

        // calc next step
        if (p->step_mode == mForward) {  // FORWARD
            if (pos == p->loop_end)
                next_pos = p->loop_start;
            else if (pos >= kMaxLoopLength)
                next_pos = 0;
            else
                next_pos++;
            cut_pos = 0;
        }
        else if (p->step_mode == mReverse) {  // REVERSE
            if (pos == p->loop_start)
                next_pos = p->loop_end;
            else if (pos <= 0)
                next_pos = kMaxLoopLength;
            else
                next_pos--;
            cut_pos = 0;
        }
        else if (p->step_mode == mDrunk) {  // DRUNK
            drunk_step += (rnd() % 3) - 1;  // -1 to 1
            if (drunk_step < -1)
                drunk_step = -1;
            else if (drunk_step > 1)
                drunk_step = 1;

            next_pos += drunk_step;
            if (next_pos < 0)
                next_pos = kMaxLoopLength;
            else if (next_pos > kMaxLoopLength)
                next_pos = 0;
            else if (p->loop_dir == 1 && next_pos < p->loop_start)
                next_pos = p->loop_end;
            else if (p->loop_dir == 1 && next_pos > p->loop_end)
                next_pos = p->loop_start;
            else if (p->loop_dir == 2 && next_pos < p->loop_start &&
                     next_pos > p->loop_end) {
                if (drunk_step == 1)
                    next_pos = p->loop_start;
                else
                    next_pos = p->loop_end;
            }

            cut_pos = 1;
        }
        else if (p->step_mode == mRandom) {  // RANDOM
            next_pos = (rnd() % (p->loop_len + 1)) + p->loop_start;
            // print_dbg("\r\nnext pos:");
            // print_dbg_ulong(next_pos);
            if (next_pos > kMaxLoopLength) next_pos -= kMaxLoopLength + 1;
            cut_pos = 1;
        }

        // next pattern?
        if (pos == p->loop_end && p->step_mode == mForward) {
            if (edit_mode == mSeries)
                series_jump++;
            else if (next_pattern != pattern)
                pattern_jump++;
        }
        else if (pos == p->loop_start && p->step_mode == mReverse) {
            if (edit_mode == mSeries)
                series_jump++;
            else if (next_pattern != pattern)
                pattern_jump++;
        }
        else if (series_step == p->loop_len) {
            series_jump++;
        }

        if (edit_mode == mSeries) series_step++;


        // TRIGGER
        triggered = 0;
        if ((rnd() % 255) < p->step_probs[pos]) {
            if (p->step_choice & 1 << pos) {
                uint count = 0;
                uint16_t found[16];
                for (uint8_t i = 0; i < 4; i++)
                    if (p->steps[pos] >> i & 1) {
                        found[count] = i;
                        count++;
                    }

                if (count == 0)
                    triggered = 0;
                else if (count == 1)
                    triggered = 1 << found[0];
                else
                    triggered = 1 << found[rnd() % count];
            }
            else {
                triggered = p->steps[pos];
            }

            if (p->tr_mode == mGate) {
                if (triggered & 0x1 && w.tr_mute[0]) gpio_set_gpio_pin(B00);
                if (triggered & 0x2 && w.tr_mute[1]) gpio_set_gpio_pin(B01);
                if (triggered & 0x4 && w.tr_mute[2]) gpio_set_gpio_pin(B02);
                if (triggered & 0x8 && w.tr_mute[3]) gpio_set_gpio_pin(B03);
            }
            else {  // tr_mode == mPulse
                if (w.tr_mute[0]) {
                    if (triggered & 0x1)
                        gpio_set_gpio_pin(B00);
                    else
                        gpio_clr_gpio_pin(B00);
                }
                if (w.tr_mute[1]) {
                    if (triggered & 0x2)
                        gpio_set_gpio_pin(B01);
                    else
                        gpio_clr_gpio_pin(B01);
                }
                if (w.tr_mute[2]) {
                    if (triggered & 0x4)
                        gpio_set_gpio_pin(B02);
                    else
                        gpio_clr_gpio_pin(B02);
                }
                if (w.tr_mute[3]) {
                    if (triggered & 0x8)
                        gpio_set_gpio_pin(B03);
                    else
                        gpio_clr_gpio_pin(B03);
                }
            }
        }

        monomeFrameDirty++;


        // PARAM 0
        if ((rnd() % 255) < p->cv_probs[0][pos] && w.cv_mute[0]) {
            if (p->cv_mode[0] == 0) { cv0 = p->cv_curves[0][pos]; }
            else {
                uint count = 0;
                uint16_t found[16];
                for (uint8_t i = 0; i < 16; i++)
                    if (p->cv_steps[0][pos] & (1 << i)) {
                        found[count] = i;
                        count++;
                    }
                if (count == 1)
                    cv_chosen[0] = found[0];
                else
                    cv_chosen[0] = found[rnd() % count];
                cv0 = p->cv_values[cv_chosen[0]];
            }
        }

        // PARAM 1
        if ((rnd() % 255) < p->cv_probs[1][pos] && w.cv_mute[1]) {
            if (p->cv_mode[1] == 0) { cv1 = p->cv_curves[1][pos]; }
            else {
                uint count = 0;
                uint16_t found[16];
                for (uint8_t i = 0; i < 16; i++)
                    if (p->cv_steps[1][pos] & (1 << i)) {
                        found[count] = i;
                        count++;
                    }
                if (count == 1)
                    cv_chosen[1] = found[0];
                else
                    cv_chosen[1] = found[rnd() % count];

                cv1 = p->cv_values[cv_chosen[1]];
            }
        }


        // write to DAC
        spi_selectChip(DAC_SPI, DAC_SPI_NPCS);
        spi_write(DAC_SPI, 0x31);  // update A
        spi_write(DAC_SPI, cv0 >> 4);
        spi_write(DAC_SPI, cv0 << 4);
        spi_unselectChip(DAC_SPI, DAC_SPI_NPCS);

        spi_selectChip(DAC_SPI, DAC_SPI_NPCS);
        spi_write(DAC_SPI, 0x38);  // update B
        spi_write(DAC_SPI, cv1 >> 4);
        spi_write(DAC_SPI, cv1 << 4);
        spi_unselectChip(DAC_SPI, DAC_SPI_NPCS);
    }
    else {
        gpio_clr_gpio_pin(B10);

        if (w.wp[pattern].tr_mode == mGate) {
            gpio_clr_gpio_pin(B00);
            gpio_clr_gpio_pin(B01);
            gpio_clr_gpio_pin(B02);
            gpio_clr_gpio_pin(B03);
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
// timers

static softTimer_t clockTimer = {.next = NULL, .prev = NULL };
static softTimer_t keyTimer = {.next = NULL, .prev = NULL };
static softTimer_t adcTimer = {.next = NULL, .prev = NULL };
static softTimer_t monomePollTimer = {.next = NULL, .prev = NULL };
static softTimer_t monomeRefreshTimer = {.next = NULL, .prev = NULL };


static void clockTimer_callback(void *o) {
    if (clock_external == 0) {
        clock_phase++;
        if (clock_phase > 1) clock_phase = 0;
        (*clock_pulse)(clock_phase);
    }
}

static void keyTimer_callback(void *o) {
    static event_t e;
    e.type = kEventKeyTimer;
    e.data = 0;
    event_post(&e);
}

static void adcTimer_callback(void *o) {
    static event_t e;
    e.type = kEventPollADC;
    e.data = 0;
    event_post(&e);
}


// monome polling callback
static void monome_poll_timer_callback(void *obj) {
    // asynchronous, non-blocking read
    // UHC callback spawns appropriate events
    ftdi_read();
}

// monome refresh callback
static void monome_refresh_timer_callback(void *obj) {
    if (monomeFrameDirty > 0) {
        static event_t e;
        e.type = kEventMonomeRefresh;
        event_post(&e);
    }
}

// monome: start polling
void timers_set_monome(void) {
    // print_dbg("\r\n setting monome timers");
    timer_add(&monomePollTimer, 20, &monome_poll_timer_callback, NULL);
    timer_add(&monomeRefreshTimer, 30, &monome_refresh_timer_callback, NULL);
}

// monome stop polling
void timers_unset_monome(void) {
    // print_dbg("\r\n unsetting monome timers");
    timer_remove(&monomePollTimer);
    timer_remove(&monomeRefreshTimer);
}


////////////////////////////////////////////////////////////////////////////////
// event handlers

static void handler_FtdiConnect(int32_t data) {
    ftdi_setup();
}
static void handler_FtdiDisconnect(int32_t data) {
    timers_unset_monome();
}

static void handler_MonomeConnect(int32_t data) {
    keycount_pos = 0;
    key_count = 0;

    timers_set_monome();
}

static void handler_MonomePoll(int32_t data) {
    monome_read_serial();
}
static void handler_MonomeRefresh(int32_t data) {
    if (monomeFrameDirty) {
        if (preset_mode == 0)
            refresh();
        else
            refresh_preset();

        (*monome_refresh)();
    }
}


static void handler_Front(int32_t data) {
    print_dbg("\r\n FRONT HOLD");

    if (data == 0) {
        front_timer = 15;
        if (preset_mode)
            preset_mode = 0;
        else
            preset_mode = 1;
    }
    else {
        front_timer = 0;
    }

    monomeFrameDirty++;
}

static void handler_PollADC(int32_t data) {
    uint16_t i;
    adc_convert(&adc);

    // CLOCK POT INPUT
    i = adc[0];
    i = i >> 2;
    if (i != clock_temp) {
        // 1000ms - 24ms
        clock_time = 25000 / (i + 25);

        timer_set(&clockTimer, clock_time);
    }
    clock_temp = i;

    // PARAM POT INPUT
    if (param_accept && edit_prob) {
        *param_dest8 = adc[1] >> 4;  // scale to 0-255;
    }
    else if (param_accept) {
        if (quantize_in)
            *param_dest = (adc[1] / 34) * 34;
        else
            *param_dest = adc[1];
        monomeFrameDirty++;
    }
    else if (key_meta) {
        i = adc[1] >> 6;
        if (i > 58) i = 58;
        if (i != scroll_pos) {
            scroll_pos = i;
            monomeFrameDirty++;
        }
    }
}

static void handler_SaveFlash(int32_t data) {
    flash_write();
}

static void handler_KeyTimer(int32_t data) {
    static uint16_t i1, x, n1;

    if (front_timer) {
        if (front_timer == 1) {
            static event_t e;
            e.type = kEventSaveFlash;
            event_post(&e);

            preset_mode = 0;
            front_timer--;
        }
        else
            front_timer--;
    }

    for (i1 = 0; i1 < key_count; i1++) {
        if (key_times[held_keys[i1]])
            if (--key_times[held_keys[i1]] == 0) {
                if (edit_mode != mSeries && preset_mode == 0) {
                    // preset copy
                    if (held_keys[i1] / 16 == 2) {
                        x = held_keys[i1] % 16;
                        for (n1 = 0; n1 < 16; n1++) {
                            w.wp[x].steps[n1] = w.wp[pattern].steps[n1];
                            w.wp[x].step_probs[n1] =
                                w.wp[pattern].step_probs[n1];
                            w.wp[x].cv_values[n1] = w.wp[pattern].cv_values[n1];
                            w.wp[x].cv_steps[0][n1] =
                                w.wp[pattern].cv_steps[0][n1];
                            w.wp[x].cv_curves[0][n1] =
                                w.wp[pattern].cv_curves[0][n1];
                            w.wp[x].cv_probs[0][n1] =
                                w.wp[pattern].cv_probs[0][n1];
                            w.wp[x].cv_steps[1][n1] =
                                w.wp[pattern].cv_steps[1][n1];
                            w.wp[x].cv_curves[1][n1] =
                                w.wp[pattern].cv_curves[1][n1];
                            w.wp[x].cv_probs[1][n1] =
                                w.wp[pattern].cv_probs[1][n1];
                        }

                        w.wp[x].cv_mode[0] = w.wp[pattern].cv_mode[0];
                        w.wp[x].cv_mode[1] = w.wp[pattern].cv_mode[1];

                        w.wp[x].loop_start = w.wp[pattern].loop_start;
                        w.wp[x].loop_end = w.wp[pattern].loop_end;
                        w.wp[x].loop_len = w.wp[pattern].loop_len;
                        w.wp[x].loop_dir = w.wp[pattern].loop_dir;

                        w.wp[x].tr_mode = w.wp[pattern].tr_mode;
                        w.wp[x].step_mode = w.wp[pattern].step_mode;

                        pattern = x;
                        next_pattern = x;
                        monomeFrameDirty++;
                    }
                }
                else if (preset_mode == 1) {
                    if (held_keys[i1] % 16 == 0) {
                        preset_select = held_keys[i1] / 16;
                        // flash_write();
                        static event_t e;
                        e.type = kEventSaveFlash;
                        event_post(&e);
                        preset_mode = 0;
                    }
                }
            }
    }
}

static void handler_ClockNormal(int32_t data) {
    clock_external = !gpio_get_pin_value(B09);
}

static void handler_ClockExt(int32_t data) {
    clock(data);
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// application grid code

static void handler_MonomeGridKey(int32_t data) {
    uint8_t x, y, z, index, i1, found, count;
    int16_t delta;
    monome_grid_key_parse_event_data(data, &x, &y, &z);

    //// TRACK LONG PRESSES
    index = y * 16 + x;
    if (z) {
        held_keys[key_count] = index;
        key_count++;
        key_times[index] = 10;  //// THRESHOLD key hold time
    }
    else {
        found = 0;  // "found"
        for (i1 = 0; i1 < key_count; i1++) {
            if (held_keys[i1] == index) found++;
            if (found) held_keys[i1] = held_keys[i1 + 1];
        }
        key_count--;

        // FAST PRESS
        if (key_times[index] > 0) {
            if (edit_mode != mSeries && preset_mode == 0) {
                if (index / 16 == 2) {
                    i1 = index % 16;
                    if (key_alt)
                        next_pattern = i1;
                    else {
                        pattern = i1;
                        next_pattern = i1;
                    }
                }
            }
            // PRESET MODE FAST PRESS DETECT
            else if (preset_mode == 1) {
                if (x == 0 && y != preset_select) {
                    preset_select = y;
                    for (i1 = 0; i1 < 8; i1++)
                        glyph[i1] = flashy.glyph[preset_select][i1];
                }
                else if (x == 0 && y == preset_select) {
                    flash_read();

                    preset_mode = 0;
                }
            }
        }
    }

    // PRESET SCREEN
    if (preset_mode) {
        // glyph magic
        if (z && x > 7) {
            glyph[y] ^= 1 << (x - 8);
            monomeFrameDirty++;
        }
    }
    // NOT PRESET
    else {
        // OPTIMIZE: order this if-branch by common priority/use
        //// SORT

        // cut position
        if (y == 1) {
            keycount_pos += z * 2 - 1;
            if (keycount_pos < 0) keycount_pos = 0;

            if (keycount_pos == 1 && z) {
                if (key_alt == 0) {
                    if (key_meta != 1) {
                        next_pos = x;
                        cut_pos++;
                        monomeFrameDirty++;
                    }
                    keyfirst_pos = x;
                }
                else if (key_alt == 1) {
                    if (x == kMaxLoopLength)
                        w.wp[pattern].step_mode = mForward;
                    else if (x == kMaxLoopLength - 1)
                        w.wp[pattern].step_mode = mReverse;
                    else if (x == kMaxLoopLength - 2)
                        w.wp[pattern].step_mode = mDrunk;
                    else if (x == kMaxLoopLength - 3)
                        w.wp[pattern].step_mode = mRandom;
                    // FIXME
                    else if (x == 0) {
                        if (pos == w.wp[pattern].loop_start)
                            next_pos = w.wp[pattern].loop_end;
                        else if (pos == 0)
                            next_pos = kMaxLoopLength;
                        else
                            next_pos--;
                        cut_pos = 1;
                        monomeFrameDirty++;
                    }
                    // FIXME
                    else if (x == 1) {
                        if (pos == w.wp[pattern].loop_end)
                            next_pos = w.wp[pattern].loop_start;
                        else if (pos == kMaxLoopLength)
                            next_pos = 0;
                        else
                            next_pos++;
                        cut_pos = 1;
                        monomeFrameDirty++;
                    }
                    else if (x == 2) {
                        next_pos = (rnd() % (w.wp[pattern].loop_len + 1)) +
                                   w.wp[pattern].loop_start;
                        cut_pos = 1;
                        monomeFrameDirty++;
                    }
                }
            }
            else if (keycount_pos == 2 && z) {
                w.wp[pattern].loop_start = keyfirst_pos;
                w.wp[pattern].loop_end = x;
                monomeFrameDirty++;
                if (w.wp[pattern].loop_start > w.wp[pattern].loop_end)
                    w.wp[pattern].loop_dir = 2;
                else if (w.wp[pattern].loop_start == 0 &&
                         w.wp[pattern].loop_end == kMaxLoopLength)
                    w.wp[pattern].loop_dir = 0;
                else
                    w.wp[pattern].loop_dir = 1;

                w.wp[pattern].loop_len =
                    w.wp[pattern].loop_end - w.wp[pattern].loop_start;

                if (w.wp[pattern].loop_dir == 2)
                    w.wp[pattern].loop_len =
                        (kMaxLoopLength - w.wp[pattern].loop_start) +
                        w.wp[pattern].loop_end + 1;
            }
        }

        // top row
        else if (y == 0) {
            if (x == kMaxLoopLength) {
                key_alt = z;
                if (z == 0) {
                    param_accept = 0;
                    live_in = 0;
                }
                monomeFrameDirty++;
            }
            else if (x < 4 && z) {
                if (key_alt) {
                    if (w.wp[pattern].tr_mode == mPulse)
                        w.wp[pattern].tr_mode = mGate;
                    else
                        w.wp[pattern].tr_mode = mPulse;
                }
                else if (key_meta)
                    w.tr_mute[x] ^= 1;
                else
                    edit_mode = mTrig;
                edit_prob = 0;
                param_accept = 0;
                monomeFrameDirty++;
            }
            else if (x > 3 && x < 12 && z) {
                param_accept = 0;
                edit_cv_ch = (x - 4) / 4;
                edit_prob = 0;

                if (key_alt)
                    w.wp[pattern].cv_mode[edit_cv_ch] ^= 1;
                else if (key_meta)
                    w.cv_mute[edit_cv_ch] ^= 1;
                else
                    edit_mode = mMap;

                monomeFrameDirty++;
            }
            else if (x == kMaxLoopLength - 1 && z && key_alt) {
                edit_mode = mSeries;
                monomeFrameDirty++;
            }
            else if (x == kMaxLoopLength - 1)
                key_meta = z;
        }


        // toggle steps and prob control
        else if (edit_mode == mTrig) {
            if (z && y > 3 && edit_prob == 0) {
                if (key_alt)
                    w.wp[pattern].steps[pos] |= 1 << (y - 4);
                else if (key_meta) {
                    w.wp[pattern].step_choice ^= (1 << x);
                }
                else
                    w.wp[pattern].steps[x] ^= (1 << (y - 4));
                monomeFrameDirty++;
            }
            // step probs
            else if (z && y == 3) {
                if (key_alt)
                    edit_prob = 1;
                else {
                    if (w.wp[pattern].step_probs[x] == 255)
                        w.wp[pattern].step_probs[x] = 0;
                    else
                        w.wp[pattern].step_probs[x] = 255;
                }
                monomeFrameDirty++;
            }
            else if (edit_prob == 1) {
                if (z) {
                    if (y == 4)
                        w.wp[pattern].step_probs[x] = 192;
                    else if (y == 5)
                        w.wp[pattern].step_probs[x] = 128;
                    else if (y == 6)
                        w.wp[pattern].step_probs[x] = 64;
                    else
                        w.wp[pattern].step_probs[x] = 0;
                }
            }
        }

        // edit map and probs
        else if (edit_mode == mMap) {
            // step probs
            if (z && y == 3) {
                if (key_alt)
                    edit_prob = 1;
                else {
                    if (w.wp[pattern].cv_probs[edit_cv_ch][x] == 255)
                        w.wp[pattern].cv_probs[edit_cv_ch][x] = 0;
                    else
                        w.wp[pattern].cv_probs[edit_cv_ch][x] = 255;
                }

                monomeFrameDirty++;
            }
            // edit data
            else if (edit_prob == 0) {
                // CURVES
                if (w.wp[pattern].cv_mode[edit_cv_ch] == 0) {
                    if (y == 4 && z) {
                        if (center)
                            delta = 3;
                        else if (key_alt)
                            delta = 409;
                        else
                            delta = 34;

                        if (key_meta == 0) {
                            // saturate
                            if (w.wp[pattern].cv_curves[edit_cv_ch][x] + delta <
                                4092)
                                w.wp[pattern].cv_curves[edit_cv_ch][x] += delta;
                            else
                                w.wp[pattern].cv_curves[edit_cv_ch][x] = 4092;
                        }
                        else {
                            for (i1 = 0; i1 < 16; i1++) {
                                // saturate
                                if (w.wp[pattern].cv_curves[edit_cv_ch][i1] +
                                        delta <
                                    4092)
                                    w.wp[pattern].cv_curves[edit_cv_ch][i1] +=
                                        delta;
                                else
                                    w.wp[pattern].cv_curves[edit_cv_ch][i1] =
                                        4092;
                            }
                        }
                    }
                    else if (y == 6 && z) {
                        if (center)
                            delta = 3;
                        else if (key_alt)
                            delta = 409;
                        else
                            delta = 34;

                        if (key_meta == 0) {
                            // saturate
                            if (w.wp[pattern].cv_curves[edit_cv_ch][x] > delta)
                                w.wp[pattern].cv_curves[edit_cv_ch][x] -= delta;
                            else
                                w.wp[pattern].cv_curves[edit_cv_ch][x] = 0;
                        }
                        else {
                            for (i1 = 0; i1 < 16; i1++) {
                                // saturate
                                if (w.wp[pattern].cv_curves[edit_cv_ch][i1] >
                                    delta)
                                    w.wp[pattern].cv_curves[edit_cv_ch][i1] -=
                                        delta;
                                else
                                    w.wp[pattern].cv_curves[edit_cv_ch][i1] = 0;
                            }
                        }
                    }
                    else if (y == 5) {
                        if (z == 1) {
                            center = 1;
                            if (quantize_in)
                                quantize_in = 0;
                            else if (key_alt)
                                w.wp[pattern].cv_curves[edit_cv_ch][x] = clip;
                            else
                                clip = w.wp[pattern].cv_curves[edit_cv_ch][x];
                        }
                        else
                            center = 0;
                    }
                    else if (y == 7) {
                        if (key_alt && z) {
                            param_dest =
                                &w.wp[pattern].cv_curves[edit_cv_ch][pos];
                            w.wp[pattern].cv_curves[edit_cv_ch][pos] =
                                (adc[1] / 34) * 34;
                            quantize_in = 1;
                            param_accept = 1;
                            live_in = 1;
                        }
                        else if (center && z) {
                            if (key_meta == 0)
                                w.wp[pattern].cv_curves[edit_cv_ch][x] =
                                    rand() % ((adc[1] / 34) * 34 + 1);
                            else {
                                for (i1 = 0; i1 < 16; i1++) {
                                    w.wp[pattern].cv_curves[edit_cv_ch][i1] =
                                        rand() % ((adc[1] / 34) * 34 + 1);
                                }
                            }
                        }
                        else {
                            param_accept = z;
                            param_dest =
                                &w.wp[pattern].cv_curves[edit_cv_ch][x];
                            if (z) {
                                w.wp[pattern].cv_curves[edit_cv_ch][x] =
                                    (adc[1] / 34) * 34;
                                quantize_in = 1;
                            }
                            else
                                quantize_in = 0;
                        }
                        monomeFrameDirty++;
                    }
                }
                // MAP
                else {
                    if (scale_select && z) {
                        // index -= 64;
                        index = (y - 4) * 8 + x;
                        if (index < 24 && y < 8) {
                            for (i1 = 0; i1 < 16; i1++)
                                w.wp[pattern].cv_values[i1] = SCALES[index][i1];
                        }

                        scale_select = 0;
                        monomeFrameDirty++;
                    }
                    else {
                        if (z && y == 4) {
                            edit_cv_step = x;
                            count = 0;
                            for (i1 = 0; i1 < 16; i1++)
                                if ((w.wp[pattern]
                                         .cv_steps[edit_cv_ch][edit_cv_step] >>
                                     i1) &
                                    1) {
                                    count++;
                                    edit_cv_value = i1;
                                }
                            if (count > 1) edit_cv_value = -1;

                            keycount_cv = 0;

                            monomeFrameDirty++;
                        }
                        // load scale
                        else if (key_alt && y == 7 && x == 0 && z) {
                            scale_select++;
                            monomeFrameDirty++;
                        }
                        // read pot
                        else if (y == 7 && key_alt && edit_cv_value != -1 &&
                                 x == kMaxLoopLength) {
                            param_accept = z;
                            param_dest =
                                &(w.wp[pattern].cv_values[edit_cv_value]);
                        }
                        else if ((y == 5 || y == 6) && z && x < 4 &&
                                 edit_cv_step != -1) {
                            delta = 0;
                            if (x == 0)
                                delta = 409;
                            else if (x == 1)
                                delta = 239;
                            else if (x == 2)
                                delta = 34;
                            else if (x == 3)
                                delta = 3;

                            if (y == 6) delta *= -1;

                            if (key_alt) {
                                for (i1 = 0; i1 < 16; i1++) {
                                    if (w.wp[pattern].cv_values[i1] + delta >
                                        4092)
                                        w.wp[pattern].cv_values[i1] = 4092;
                                    else if (delta < 0 &&
                                             w.wp[pattern].cv_values[i1] <
                                                 -1 * delta)
                                        w.wp[pattern].cv_values[i1] = 0;
                                    else
                                        w.wp[pattern].cv_values[i1] += delta;
                                }
                            }
                            else {
                                if (w.wp[pattern].cv_values[edit_cv_value] +
                                        delta >
                                    4092)
                                    w.wp[pattern].cv_values[edit_cv_value] =
                                        4092;
                                else if (delta < 0 &&
                                         w.wp[pattern]
                                                 .cv_values[edit_cv_value] <
                                             -1 * delta)
                                    w.wp[pattern].cv_values[edit_cv_value] = 0;
                                else
                                    w.wp[pattern].cv_values[edit_cv_value] +=
                                        delta;
                            }

                            monomeFrameDirty++;
                        }
                        // choose values
                        else if (y == 7) {
                            keycount_cv += z * 2 - 1;
                            if (keycount_cv < 0) keycount_cv = 0;

                            if (z) {
                                count = 0;
                                for (i1 = 0; i1 < 16; i1++)
                                    if ((w.wp[pattern].cv_steps[edit_cv_ch]
                                                               [edit_cv_step] >>
                                         i1) &
                                        1)
                                        count++;

                                // single press toggle
                                if (keycount_cv == 1 && count < 2) {
                                    w.wp[pattern]
                                        .cv_steps[edit_cv_ch][edit_cv_step] =
                                        (1 << x);
                                    edit_cv_value = x;
                                }
                                // multiselect
                                else if (keycount_cv > 1 || count > 1) {
                                    w.wp[pattern]
                                        .cv_steps[edit_cv_ch][edit_cv_step] ^=
                                        (1 << x);

                                    if (!w.wp[pattern].cv_steps[edit_cv_ch]
                                                               [edit_cv_step])
                                        w.wp[pattern].cv_steps[edit_cv_ch]
                                                              [edit_cv_step] =
                                            (1 << x);

                                    count = 0;
                                    for (i1 = 0; i1 < 16; i1++)
                                        if ((w.wp[pattern]
                                                 .cv_steps[edit_cv_ch]
                                                          [edit_cv_step] >>
                                             i1) &
                                            1) {
                                            count++;
                                            edit_cv_value = i1;
                                        }

                                    if (count > 1) edit_cv_value = -1;
                                }

                                monomeFrameDirty++;
                            }
                        }
                    }
                }
            }
            else if (edit_prob == 1) {
                if (z) {
                    if (y == 4)
                        w.wp[pattern].cv_probs[edit_cv_ch][x] = 192;
                    else if (y == 5)
                        w.wp[pattern].cv_probs[edit_cv_ch][x] = 128;
                    else if (y == 6)
                        w.wp[pattern].cv_probs[edit_cv_ch][x] = 64;
                    else
                        w.wp[pattern].cv_probs[edit_cv_ch][x] = 0;
                }
            }
        }

        // series mode
        else if (edit_mode == mSeries) {
            if (z && key_alt) {
                if (x == 0)
                    series_next = y - 2 + scroll_pos;
                else if (x == kMaxLoopLength - 1)
                    w.series_start = y - 2 + scroll_pos;
                else if (x == kMaxLoopLength)
                    w.series_end = y - 2 + scroll_pos;

                if (w.series_end < w.series_start)
                    w.series_end = w.series_start;
            }
            else {
                keycount_series += z * 2 - 1;
                if (keycount_series < 0) keycount_series = 0;

                if (z) {
                    count = 0;
                    for (i1 = 0; i1 < 16; i1++)
                        count += (w.series_list[y - 2 + scroll_pos] >> i1) & 1;

                    // single press toggle
                    if (keycount_series == 1 && count < 2) {
                        w.series_list[y - 2 + scroll_pos] = (1 << x);
                    }
                    // multi-select
                    else if (keycount_series > 1 || count > 1) {
                        w.series_list[y - 2 + scroll_pos] ^= (1 << x);

                        // ensure not fully clear
                        if (!w.series_list[y - 2 + scroll_pos])
                            w.series_list[y - 2 + scroll_pos] = (1 << x);
                    }
                }
            }

            monomeFrameDirty++;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// application grid redraw
static void refresh() {
    // clear top, cut, pattern, prob
    for (uint8_t i = 0; i < 16; i++) {
        monomeLedBuffer[i] = 0;
        monomeLedBuffer[16 + i] = 0;
        monomeLedBuffer[32 + i] = 4;
        monomeLedBuffer[48 + i] = 0;
    }

    // dim mode
    if (edit_mode == mTrig) {
        monomeLedBuffer[0] = 4;
        monomeLedBuffer[1] = 4;
        monomeLedBuffer[2] = 4;
        monomeLedBuffer[3] = 4;
    }
    else if (edit_mode == mMap) {
        monomeLedBuffer[4 + (edit_cv_ch * 4)] = 4;
        monomeLedBuffer[5 + (edit_cv_ch * 4)] = 4;
        monomeLedBuffer[6 + (edit_cv_ch * 4)] = 4;
        monomeLedBuffer[7 + (edit_cv_ch * 4)] = 4;
    }
    else if (edit_mode == mSeries) {
        monomeLedBuffer[kMaxLoopLength - 1] = 7;
    }

    // alt
    monomeLedBuffer[kMaxLoopLength] = 4;
    if (key_alt) monomeLedBuffer[kMaxLoopLength] = 11;

    // show mutes or on steps
    if (key_meta) {
        if (w.tr_mute[0]) monomeLedBuffer[0] = 11;
        if (w.tr_mute[1]) monomeLedBuffer[1] = 11;
        if (w.tr_mute[2]) monomeLedBuffer[2] = 11;
        if (w.tr_mute[3]) monomeLedBuffer[3] = 11;
    }
    else if (triggered) {
        // dim when in mGate mode
        uint8_t dim = w.wp[pattern].tr_mode == mGate ? 4 : 0;
        if (triggered & 0x1 && w.tr_mute[0]) monomeLedBuffer[0] = 11 - dim;
        if (triggered & 0x2 && w.tr_mute[1]) monomeLedBuffer[1] = 11 - dim;
        if (triggered & 0x4 && w.tr_mute[2]) monomeLedBuffer[2] = 11 - dim;
        if (triggered & 0x8 && w.tr_mute[3]) monomeLedBuffer[3] = 11 - dim;
    }

    // cv indication
    if (key_meta) {
        if (w.cv_mute[0]) {
            monomeLedBuffer[4] = 11;
            monomeLedBuffer[5] = 11;
            monomeLedBuffer[6] = 11;
            monomeLedBuffer[7] = 11;
        }
        if (w.cv_mute[1]) {
            monomeLedBuffer[8] = 11;
            monomeLedBuffer[9] = 11;
            monomeLedBuffer[10] = 11;
            monomeLedBuffer[11] = 11;
        }
    }
    else {
        monomeLedBuffer[cv0 / 1024 + 4] = 11;
        monomeLedBuffer[cv1 / 1024 + 8] = 11;
    }

    // show pos loop dim
    if (w.wp[pattern].loop_dir) {
        for (uint8_t i = 0; i < kGridWidth; i++) {
            if (w.wp[pattern].loop_dir == 1 && i >= w.wp[pattern].loop_start &&
                i <= w.wp[pattern].loop_end)
                monomeLedBuffer[16 + i] = 4;
            else if (w.wp[pattern].loop_dir == 2 &&
                     (i <= w.wp[pattern].loop_end ||
                      i >= w.wp[pattern].loop_start))
                monomeLedBuffer[16 + i] = 4;
        }
    }

    // show position and next cut
    if (cut_pos) monomeLedBuffer[16 + next_pos] = 7;
    monomeLedBuffer[16 + pos] = 15;

    // show pattern
    monomeLedBuffer[32 + pattern] = 11;
    if (pattern != next_pattern) monomeLedBuffer[32 + next_pattern] = 7;

    // show step data
    if (edit_mode == mTrig) {
        if (edit_prob == 0) {
            for (uint8_t i = 0; i < kGridWidth; i++) {
                for (uint8_t j = 0; j < 4; j++) {
                    if ((w.wp[pattern].steps[i] & (1 << j)) && i == pos &&
                        (triggered & 1 << j) && w.tr_mute[j])
                        monomeLedBuffer[(j + 4) * 16 + i] = 11;
                    else if (w.wp[pattern].steps[i] & (1 << j) &&
                             (w.wp[pattern].step_choice & 1 << i))
                        monomeLedBuffer[(j + 4) * 16 + i] = 4;
                    else if (w.wp[pattern].steps[i] & (1 << j))
                        monomeLedBuffer[(j + 4) * 16 + i] = 7;
                    else if (i == pos)
                        monomeLedBuffer[(j + 4) * 16 + i] = 4;
                    else
                        monomeLedBuffer[(j + 4) * 16 + i] = 0;
                }

                // probs
                if (w.wp[pattern].step_probs[i] == 255)
                    monomeLedBuffer[48 + i] = 11;
                else if (w.wp[pattern].step_probs[i] > 0)
                    monomeLedBuffer[48 + i] = 4;
            }
        }
        else if (edit_prob == 1) {
            for (uint8_t i = 0; i < kGridWidth; i++) {
                monomeLedBuffer[64 + i] = 4;
                monomeLedBuffer[80 + i] = 4;
                monomeLedBuffer[96 + i] = 4;
                monomeLedBuffer[112 + i] = 4;

                if (w.wp[pattern].step_probs[i] == 255)
                    monomeLedBuffer[48 + i] = 11;
                else if (w.wp[pattern].step_probs[i] == 0) {
                    monomeLedBuffer[48 + i] = 0;
                    monomeLedBuffer[112 + i] = 7;
                }
                else if (w.wp[pattern].step_probs[i]) {
                    monomeLedBuffer[48 + i] = 4;
                    monomeLedBuffer[64 +
                                    16 * (3 -
                                          (w.wp[pattern].step_probs[i] >> 6)) +
                                    i] = 7;
                }
            }
        }
    }

    // show map
    else if (edit_mode == mMap) {
        if (edit_prob == 0) {
            // CURVES
            if (w.wp[pattern].cv_mode[edit_cv_ch] == 0) {
                for (uint8_t i = 0; i < kGridWidth; i++) {
                    // probs
                    if (w.wp[pattern].cv_probs[edit_cv_ch][i] == 255)
                        monomeLedBuffer[48 + i] = 11;
                    else if (w.wp[pattern].cv_probs[edit_cv_ch][i] > 0)
                        monomeLedBuffer[48 + i] = 7;

                    monomeLedBuffer[112 + i] =
                        (w.wp[pattern].cv_curves[edit_cv_ch][i] > 1023) * 7;
                    monomeLedBuffer[96 + i] =
                        (w.wp[pattern].cv_curves[edit_cv_ch][i] > 2047) * 7;
                    monomeLedBuffer[80 + i] =
                        (w.wp[pattern].cv_curves[edit_cv_ch][i] > 3071) * 7;
                    monomeLedBuffer[64 + i] = 0;
                    monomeLedBuffer[64 +
                                    16 * (3 - (w.wp[pattern]
                                                   .cv_curves[edit_cv_ch][i] >>
                                               10)) +
                                    i] =
                        (w.wp[pattern].cv_curves[edit_cv_ch][i] >> 7) & 0x7;
                }

                // play step highlight
                monomeLedBuffer[64 + pos] += 4;
                monomeLedBuffer[80 + pos] += 4;
                monomeLedBuffer[96 + pos] += 4;
                monomeLedBuffer[112 + pos] += 4;
            }
            // MAP
            else {
                if (!scale_select) {
                    for (uint8_t i = 0; i < kGridWidth; i++) {
                        // probs
                        if (w.wp[pattern].cv_probs[edit_cv_ch][i] == 255)
                            monomeLedBuffer[48 + i] = 11;
                        else if (w.wp[pattern].cv_probs[edit_cv_ch][i] > 0)
                            monomeLedBuffer[48 + i] = 7;

                        // clear edit select line
                        monomeLedBuffer[64 + i] = 4;

                        // show current edit value, selected
                        if (edit_cv_value != -1) {
                            if ((w.wp[pattern].cv_values[edit_cv_value] >> 8) >=
                                i)
                                monomeLedBuffer[80 + i] = 7;
                            else
                                monomeLedBuffer[80 + i] = 0;

                            if (((w.wp[pattern].cv_values[edit_cv_value] >> 4) &
                                 0xf) >= i)
                                monomeLedBuffer[96 + i] = 4;
                            else
                                monomeLedBuffer[96 + i] = 0;
                        }
                        else {
                            monomeLedBuffer[80 + i] = 0;
                            monomeLedBuffer[96 + i] = 0;
                        }

                        // show steps
                        if (w.wp[pattern].cv_steps[edit_cv_ch][edit_cv_step] &
                            (1 << i))
                            monomeLedBuffer[112 + i] = 7;
                        else
                            monomeLedBuffer[112 + i] = 0;
                    }

                    // show play position
                    monomeLedBuffer[64 + pos] = 7;
                    // show edit position
                    monomeLedBuffer[64 + edit_cv_step] = 11;
                    // show playing note
                    monomeLedBuffer[112 + cv_chosen[edit_cv_ch]] = 11;
                }
                else {
                    for (uint8_t i = 0; i < kGridWidth; i++) {
                        // probs
                        if (w.wp[pattern].cv_probs[edit_cv_ch][i] == 255)
                            monomeLedBuffer[48 + i] = 11;
                        else if (w.wp[pattern].cv_probs[edit_cv_ch][i] > 0)
                            monomeLedBuffer[48 + i] = 7;

                        monomeLedBuffer[64 + i] = (i < 8) * 4;
                        monomeLedBuffer[80 + i] = (i < 8) * 4;
                        monomeLedBuffer[96 + i] = (i < 8) * 4;
                        monomeLedBuffer[112 + i] = 0;
                    }

                    monomeLedBuffer[112] = 7;
                }
            }
        }
        else if (edit_prob == 1) {
            for (uint8_t i = 0; i < kGridWidth; i++) {
                monomeLedBuffer[64 + i] = 4;
                monomeLedBuffer[80 + i] = 4;
                monomeLedBuffer[96 + i] = 4;
                monomeLedBuffer[112 + i] = 4;

                if (w.wp[pattern].cv_probs[edit_cv_ch][i] == 255)
                    monomeLedBuffer[48 + i] = 11;
                else if (w.wp[pattern].cv_probs[edit_cv_ch][i] == 0) {
                    monomeLedBuffer[48 + i] = 0;
                    monomeLedBuffer[112 + i] = 7;
                }
                else if (w.wp[pattern].cv_probs[edit_cv_ch][i]) {
                    monomeLedBuffer[48 + i] = 4;
                    monomeLedBuffer[64 +
                                    16 * (3 - (w.wp[pattern]
                                                   .cv_probs[edit_cv_ch][i] >>
                                               6)) +
                                    i] = 7;
                }
            }
        }
    }

    // series
    else if (edit_mode == mSeries) {
        for (uint8_t i = 0; i < 6; i++) {
            for (uint8_t j = 0; j < kGridWidth; j++) {
                // start/end bars, clear
                if (i + scroll_pos == w.series_start ||
                    i + scroll_pos == w.series_end)
                    monomeLedBuffer[32 + i * 16 + j] = 4;
                else
                    monomeLedBuffer[32 + i * 16 + j] = 0;
            }

            // scroll position helper
            monomeLedBuffer[32 + i * 16 +
                            ((scroll_pos + i) / (64 / kGridWidth))] = 4;

            // sidebar selection indicators
            if (i + scroll_pos > w.series_start &&
                i + scroll_pos < w.series_end) {
                monomeLedBuffer[32 + i * 16] = 4;
                monomeLedBuffer[32 + i * 16 + kMaxLoopLength] = 4;
            }

            for (uint8_t j = 0; j < kGridWidth; j++) {
                // show possible states
                if ((w.series_list[i + scroll_pos] >> j) & 1)
                    monomeLedBuffer[32 + (i * 16) + j] = 7;
            }
        }

        // highlight playhead
        if (series_pos >= scroll_pos && series_pos < scroll_pos + 6) {
            monomeLedBuffer[32 + (series_pos - scroll_pos) * 16 +
                            series_playing] = 11;
        }
    }

    monome_set_quadrant_flag(0);
    monome_set_quadrant_flag(1);
}


static void refresh_preset() {
    for (uint8_t i = 0; i < 128; i++) { monomeLedBuffer[i] = 0; }

    monomeLedBuffer[preset_select * 16] = 11;

    for (uint8_t i = 0; i < 8; i++) {
        for (uint8_t j = 0; j < 8; j++) {
            if (glyph[i] & (1 << j)) monomeLedBuffer[i * 16 + j + 8] = 11;
        }
    }

    monome_set_quadrant_flag(0);
    monome_set_quadrant_flag(1);
}


static void ww_process_ii(uint8_t *data, uint8_t l) {
    uint8_t i;
    int d;

    i = data[0];
    d = (data[1] << 8) + data[2];

    switch (i) {
        case WW_PRESET:
            if (d < 0 || d > 7) break;
            preset_select = d;
            flash_read();
            break;
        case WW_POS:
            if (d < 0 || d > 15) break;
            next_pos = d;
            cut_pos++;
            monomeFrameDirty++;
            break;
        case WW_SYNC:
            if (d < 0 || d > 15) break;
            next_pos = d;
            cut_pos++;
            timer_set(&clockTimer, clock_time);
            clock_phase = 1;
            (*clock_pulse)(clock_phase);
            break;
        case WW_START:
            if (d < 0 || d > 15) break;
            w.wp[pattern].loop_start = d;
            if (w.wp[pattern].loop_start > w.wp[pattern].loop_end)
                w.wp[pattern].loop_dir = 2;
            else if (w.wp[pattern].loop_start == 0 &&
                     w.wp[pattern].loop_end == kMaxLoopLength)
                w.wp[pattern].loop_dir = 0;
            else
                w.wp[pattern].loop_dir = 1;

            w.wp[pattern].loop_len =
                w.wp[pattern].loop_end - w.wp[pattern].loop_start;

            if (w.wp[pattern].loop_dir == 2)
                w.wp[pattern].loop_len =
                    (kMaxLoopLength - w.wp[pattern].loop_start) +
                    w.wp[pattern].loop_end + 1;
            monomeFrameDirty++;
            break;
        case WW_END:
            if (d < 0 || d > 15) break;
            w.wp[pattern].loop_end = d;
            if (w.wp[pattern].loop_start > w.wp[pattern].loop_end)
                w.wp[pattern].loop_dir = 2;
            else if (w.wp[pattern].loop_start == 0 &&
                     w.wp[pattern].loop_end == kMaxLoopLength)
                w.wp[pattern].loop_dir = 0;
            else
                w.wp[pattern].loop_dir = 1;

            w.wp[pattern].loop_len =
                w.wp[pattern].loop_end - w.wp[pattern].loop_start;

            if (w.wp[pattern].loop_dir == 2)
                w.wp[pattern].loop_len =
                    (kMaxLoopLength - w.wp[pattern].loop_start) +
                    w.wp[pattern].loop_end + 1;
            monomeFrameDirty++;
            break;
        case WW_PMODE:
            if (d < 0 || d > 3) break;
            w.wp[pattern].step_mode = d;
            break;
        case WW_PATTERN:
            if (d < 0 || d > 15) break;
            pattern = d;
            next_pattern = d;
            monomeFrameDirty++;
            break;
        case WW_QPATTERN:
            if (d < 0 || d > 15) break;
            next_pattern = d;
            monomeFrameDirty++;
            break;
        case WW_MUTE1:
            if (d)
                w.tr_mute[0] = 1;
            else
                w.tr_mute[0] = 0;
            break;
        case WW_MUTE2:
            if (d)
                w.tr_mute[1] = 1;
            else
                w.tr_mute[1] = 0;
            break;
        case WW_MUTE3:
            if (d)
                w.tr_mute[2] = 1;
            else
                w.tr_mute[2] = 0;
            break;
        case WW_MUTE4:
            if (d)
                w.tr_mute[3] = 1;
            else
                w.tr_mute[3] = 0;
            break;
        case WW_MUTEA:
            if (d)
                w.cv_mute[0] = 1;
            else
                w.cv_mute[0] = 0;
            break;
        case WW_MUTEB:
            if (d)
                w.cv_mute[1] = 1;
            else
                w.cv_mute[1] = 0;
            break;
        default: break;
    }
}


// assign event handlers
static inline void assign_main_event_handlers(void) {
    app_event_handlers[kEventFront] = &handler_Front;
    // app_event_handlers[ kEventTimer ]	= &handler_Timer;
    app_event_handlers[kEventPollADC] = &handler_PollADC;
    app_event_handlers[kEventKeyTimer] = &handler_KeyTimer;
    app_event_handlers[kEventSaveFlash] = &handler_SaveFlash;
    app_event_handlers[kEventClockNormal] = &handler_ClockNormal;
    app_event_handlers[kEventClockExt] = &handler_ClockExt;
    app_event_handlers[kEventFtdiConnect] = &handler_FtdiConnect;
    app_event_handlers[kEventFtdiDisconnect] = &handler_FtdiDisconnect;
    app_event_handlers[kEventMonomeConnect] = &handler_MonomeConnect;
    app_event_handlers[kEventMonomeDisconnect] = &handler_None;
    app_event_handlers[kEventMonomePoll] = &handler_MonomePoll;
    app_event_handlers[kEventMonomeRefresh] = &handler_MonomeRefresh;
    app_event_handlers[kEventMonomeGridKey] = &handler_MonomeGridKey;
}

// app event loop
void check_events(void) {
    static event_t e;
    if (event_next(&e)) { (app_event_handlers)[e.type](e.data); }
}

// flash commands
uint8_t flash_is_fresh(void) {
    return (flashy.fresh != FIRSTRUN_KEY);
}

// write fresh status
void flash_unfresh(void) {
    flashc_memset8((void *)&(flashy.fresh), FIRSTRUN_KEY, 4, true);
}

void flash_write(void) {
    flashc_memcpy((void *)&flashy.w[preset_select], &w, sizeof(w), true);
    flashc_memcpy((void *)&flashy.glyph[preset_select], &glyph, sizeof(glyph),
                  true);
    flashc_memset8((void *)&(flashy.preset_select), preset_select, 1, true);
    flashc_memset32((void *)&(flashy.edit_mode), edit_mode, 4, true);
}

void flash_read(void) {
    uint8_t i1, i2;

    print_dbg("\r\n read preset ");
    print_dbg_ulong(preset_select);

    for (i1 = 0; i1 < 16; i1++) {
        for (i2 = 0; i2 < 16; i2++) {
            w.wp[i1].steps[i2] = flashy.w[preset_select].wp[i1].steps[i2];
            w.wp[i1].step_probs[i2] =
                flashy.w[preset_select].wp[i1].step_probs[i2];
            w.wp[i1].cv_probs[0][i2] =
                flashy.w[preset_select].wp[i1].cv_probs[0][i2];
            w.wp[i1].cv_probs[1][i2] =
                flashy.w[preset_select].wp[i1].cv_probs[1][i2];
            w.wp[i1].cv_curves[0][i2] =
                flashy.w[preset_select].wp[i1].cv_curves[0][i2];
            w.wp[i1].cv_curves[1][i2] =
                flashy.w[preset_select].wp[i1].cv_curves[1][i2];
            w.wp[i1].cv_steps[0][i2] =
                flashy.w[preset_select].wp[i1].cv_steps[0][i2];
            w.wp[i1].cv_steps[1][i2] =
                flashy.w[preset_select].wp[i1].cv_steps[1][i2];
            w.wp[i1].cv_values[i2] =
                flashy.w[preset_select].wp[i1].cv_values[i2];
        }

        w.wp[i1].step_choice = flashy.w[preset_select].wp[i1].step_choice;
        w.wp[i1].loop_end = flashy.w[preset_select].wp[i1].loop_end;
        w.wp[i1].loop_len = flashy.w[preset_select].wp[i1].loop_len;
        w.wp[i1].loop_start = flashy.w[preset_select].wp[i1].loop_start;
        w.wp[i1].loop_dir = flashy.w[preset_select].wp[i1].loop_dir;
        w.wp[i1].step_mode = flashy.w[preset_select].wp[i1].step_mode;
        w.wp[i1].cv_mode[0] = flashy.w[preset_select].wp[i1].cv_mode[0];
        w.wp[i1].cv_mode[1] = flashy.w[preset_select].wp[i1].cv_mode[1];
        w.wp[i1].tr_mode = flashy.w[preset_select].wp[i1].tr_mode;
    }

    w.series_start = flashy.w[preset_select].series_start;
    w.series_end = flashy.w[preset_select].series_end;

    w.tr_mute[0] = flashy.w[preset_select].tr_mute[0];
    w.tr_mute[1] = flashy.w[preset_select].tr_mute[1];
    w.tr_mute[2] = flashy.w[preset_select].tr_mute[2];
    w.tr_mute[3] = flashy.w[preset_select].tr_mute[3];
    w.cv_mute[0] = flashy.w[preset_select].cv_mute[0];
    w.cv_mute[1] = flashy.w[preset_select].cv_mute[1];

    for (i1 = 0; i1 < 64; i1++)
        w.series_list[i1] = flashy.w[preset_select].series_list[i1];
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// main

int main(void) {
    sysclk_init();

    init_dbg_rs232(FMCK_HZ);

    init_gpio();
    assign_main_event_handlers();
    init_events();
    init_tc();
    init_spi();
    init_adc();

    irq_initialize_vectors();
    register_interrupts();
    cpu_irq_enable();

    init_usb_host();
    init_monome();

    init_i2c_slave(0x10);


    print_dbg("\r\n\n// white whale //////////////////////////////// ");
    print_dbg_ulong(sizeof(flashy));

    print_dbg(" ");
    print_dbg_ulong(sizeof(w));

    print_dbg(" ");
    print_dbg_ulong(sizeof(glyph));

    if (flash_is_fresh()) {
        print_dbg("\r\nfirst run.");
        flash_unfresh();
        flashc_memset8((void *)&(flashy.edit_mode), mTrig, 4, true);
        flashc_memset32((void *)&(flashy.preset_select), 0, 4, true);


        // clear out some reasonable defaults
        for (uint8_t i = 0; i < 16; i++) {
            for (uint8_t j = 0; j < 16; j++) {
                w.wp[i].steps[j] = 0;
                w.wp[i].step_probs[j] = 255;
                w.wp[i].cv_probs[0][j] = 255;
                w.wp[i].cv_probs[1][j] = 255;
                w.wp[i].cv_curves[0][j] = 0;
                w.wp[i].cv_curves[1][j] = 0;
                w.wp[i].cv_values[j] = SCALES[2][j];
                w.wp[i].cv_steps[0][j] = 1 << j;
                w.wp[i].cv_steps[1][j] = 1 << j;
            }
            w.wp[i].step_choice = 0;
            w.wp[i].loop_end = 15;
            w.wp[i].loop_len = 15;
            w.wp[i].loop_start = 0;
            w.wp[i].loop_dir = 0;
            w.wp[i].step_mode = mForward;
            w.wp[i].cv_mode[0] = 0;
            w.wp[i].cv_mode[1] = 0;
            w.wp[i].tr_mode = mPulse;
        }

        w.series_start = 0;
        w.series_end = 3;

        w.tr_mute[0] = 1;
        w.tr_mute[1] = 1;
        w.tr_mute[2] = 1;
        w.tr_mute[3] = 1;
        w.cv_mute[0] = 1;
        w.cv_mute[1] = 1;

        for (uint8_t i = 0; i < 64; i++) w.series_list[i] = 1;

        // save all presets, clear glyphs
        for (uint8_t i = 0; i < 8; i++) {
            flashc_memcpy((void *)&flashy.w[i], &w, sizeof(w), true);
            glyph[i] = (1 << i);
            flashc_memcpy((void *)&flashy.glyph[i], &glyph, sizeof(glyph),
                          true);
        }
    }
    else {
        // load from flash at startup
        preset_select = flashy.preset_select;
        edit_mode = flashy.edit_mode;
        flash_read();
        for (uint8_t i = 0; i < 8; i++)
            glyph[i] = flashy.glyph[preset_select][i];
    }

    process_ii = &ww_process_ii;

    clock_pulse = &clock;
    clock_external = !gpio_get_pin_value(B09);

    timer_add(&clockTimer, 120, &clockTimer_callback, NULL);
    timer_add(&keyTimer, 50, &keyTimer_callback, NULL);
    timer_add(&adcTimer, 100, &adcTimer_callback, NULL);
    clock_temp = 10000;  // out of ADC range to force tempo

    while (true) { check_events(); }
}
