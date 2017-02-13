#include "bluewhale.h"

#include "monome.h"
#include "util.h"

#include "hardware.h"

whale_set_t w;

edit_mode_t edit_mode;
uint8_t edit_cv_step;
uint8_t edit_cv_ch;
int8_t edit_cv_value;
uint8_t edit_prob;
uint8_t scale_select;
uint8_t pattern;
uint8_t next_pattern;
uint8_t pattern_jump;

uint8_t series_pos;
uint8_t series_next;
uint8_t series_jump;
uint8_t series_playing;
uint8_t series_scroll_pos;

uint8_t key_alt;
uint8_t key_meta;
uint8_t held_keys[32];
uint8_t key_count;
uint8_t key_times[256];
uint8_t keyfirst_pos;
uint8_t keysecond_pos;
int8_t keycount_pos;
int8_t keycount_series;
int8_t keycount_cv;

int8_t pos;
int8_t cut_pos;
int8_t next_pos;
int8_t drunk_step;
int8_t triggered;
uint8_t cv_chosen[2];
uint16_t cv0, cv1;

uint8_t series_step;

uint8_t preset_mode;
uint8_t preset_select;
uint8_t front_timer;
uint8_t glyph[8];

const uint8_t kGridWidth = 16;
const uint8_t kMaxLoopLength = 15;  // this value should be 16 and the code
                                    // should be modified to that effect


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

void bw_monome_connect() {
    keycount_pos = 0;
    key_count = 0;
}

void bw_set_param(uint16_t param) {
    if (key_meta) {
        if (param > 58) param = 58;
        if (param != series_scroll_pos) {
            series_scroll_pos = param;
            monomeFrameDirty++;
        }
    }
}

void bw_key_timer() {
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

    for (uint16_t i1 = 0; i1 < key_count; i1++) {
        if (key_times[held_keys[i1]])
            if (--key_times[held_keys[i1]] == 0) {
                if (edit_mode != mSeries && preset_mode == 0) {
                    // preset copy
                    if (held_keys[i1] / 16 == 2) {
                        uint16_t x = held_keys[i1] % 16;
                        for (uint16_t n1 = 0; n1 < 16; n1++) {
                            w.wp[x].tr_steps[n1] = w.wp[pattern].tr_steps[n1];
                            w.wp[x].tr_step_probs[n1] =
                                w.wp[pattern].tr_step_probs[n1];
                            w.wp[x].cv_values[n1] = w.wp[pattern].cv_values[n1];
                            w.wp[x].cv_steps[0][n1] =
                                w.wp[pattern].cv_steps[0][n1];
                            w.wp[x].cv_probs[0][n1] =
                                w.wp[pattern].cv_probs[0][n1];
                            w.wp[x].cv_steps[1][n1] =
                                w.wp[pattern].cv_steps[1][n1];
                            w.wp[x].cv_probs[1][n1] =
                                w.wp[pattern].cv_probs[1][n1];
                        }

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

////////////////////////////////////////////////////////////////////////////////
// application clock code

void bw_clock_high(void);
void bw_clock_low(void);

void bw_clock(uint8_t phase) {
    if (phase)
        bw_clock_high();
    else
        bw_clock_low();
}

void bw_clock_high() {
    set_clock_output(true);

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

        uint8_t count = 0;
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
    if ((rnd() % 255) < p->tr_step_probs[pos]) {
        if (p->step_choice & 1 << pos) {
            uint8_t count = 0;
            uint16_t found[16];
            for (uint8_t i = 0; i < 4; i++)
                if (p->tr_steps[pos] >> i & 1) {
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
            triggered = p->tr_steps[pos];
        }

        if (p->tr_mode == mGate) {
            if (triggered & 0x1 && w.tr_mute[0]) set_gate_output(0, true);
            if (triggered & 0x2 && w.tr_mute[1]) set_gate_output(1, true);
            if (triggered & 0x4 && w.tr_mute[2]) set_gate_output(2, true);
            if (triggered & 0x8 && w.tr_mute[3]) set_gate_output(3, true);
        }
        else {  // tr_mode == mPulse
            if (w.tr_mute[0]) { set_gate_output(0, triggered & 0x1); }
            if (w.tr_mute[1]) { set_gate_output(1, triggered & 0x2); }
            if (w.tr_mute[2]) { set_gate_output(2, triggered & 0x4); }
            if (w.tr_mute[3]) { set_gate_output(3, triggered & 0x8); }
        }
    }

    monomeFrameDirty++;


    // PARAM 0
    if ((rnd() % 255) < p->cv_probs[0][pos] && w.cv_mute[0]) {
        uint8_t count = 0;
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

    // PARAM 1
    if ((rnd() % 255) < p->cv_probs[1][pos] && w.cv_mute[1]) {
        uint8_t count = 0;
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


    // write to DAC
    set_cv_output(0, cv0);
    set_cv_output(1, cv1);
}

void bw_clock_low() {
    set_clock_output(false);

    if (w.wp[pattern].tr_mode == mGate) {
        set_gate_output(0, false);
        set_gate_output(1, false);
        set_gate_output(2, false);
        set_gate_output(3, false);
    }
}


////////////////////////////////////////////////////////////////////////////////
// application grid redraw
void refresh() {
    whale_pattern_t *p = &w.wp[pattern];

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
    else if (edit_mode == mCV) {
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
        uint8_t dim = p->tr_mode == mGate ? 4 : 0;
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
    if (p->loop_dir) {
        for (uint8_t i = 0; i < kGridWidth; i++) {
            if (p->loop_dir == 1 && i >= p->loop_start && i <= p->loop_end)
                monomeLedBuffer[16 + i] = 4;
            else if (p->loop_dir == 2 &&
                     (i <= p->loop_end || i >= p->loop_start))
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
                    if ((p->tr_steps[i] & (1 << j)) && i == pos &&
                        (triggered & 1 << j) && w.tr_mute[j])
                        monomeLedBuffer[(j + 4) * 16 + i] = 11;
                    else if (p->tr_steps[i] & (1 << j) &&
                             (p->step_choice & 1 << i))
                        monomeLedBuffer[(j + 4) * 16 + i] = 4;
                    else if (p->tr_steps[i] & (1 << j))
                        monomeLedBuffer[(j + 4) * 16 + i] = 7;
                    else if (i == pos)
                        monomeLedBuffer[(j + 4) * 16 + i] = 4;
                    else
                        monomeLedBuffer[(j + 4) * 16 + i] = 0;
                }

                // probs
                if (p->tr_step_probs[i] == 255)
                    monomeLedBuffer[48 + i] = 11;
                else if (p->tr_step_probs[i] > 0)
                    monomeLedBuffer[48 + i] = 4;
            }
        }
        else if (edit_prob == 1) {
            for (uint8_t i = 0; i < kGridWidth; i++) {
                monomeLedBuffer[64 + i] = 4;
                monomeLedBuffer[80 + i] = 4;
                monomeLedBuffer[96 + i] = 4;
                monomeLedBuffer[112 + i] = 4;

                if (p->tr_step_probs[i] == 255)
                    monomeLedBuffer[48 + i] = 11;
                else if (p->tr_step_probs[i] == 0) {
                    monomeLedBuffer[48 + i] = 0;
                    monomeLedBuffer[112 + i] = 7;
                }
                else if (p->tr_step_probs[i]) {
                    monomeLedBuffer[48 + i] = 4;
                    monomeLedBuffer[64 + 16 * (3 - (p->tr_step_probs[i] >> 6)) +
                                    i] = 7;
                }
            }
        }
    }

    // show map
    else if (edit_mode == mCV) {
        if (edit_prob == 0) {
            if (!scale_select) {
                for (uint8_t i = 0; i < kGridWidth; i++) {
                    // probs
                    if (p->cv_probs[edit_cv_ch][i] == 255)
                        monomeLedBuffer[48 + i] = 11;
                    else if (p->cv_probs[edit_cv_ch][i] > 0)
                        monomeLedBuffer[48 + i] = 7;

                    // clear edit select line
                    monomeLedBuffer[64 + i] = 4;

                    // show current edit value, selected
                    if (edit_cv_value != -1) {
                        if ((p->cv_values[edit_cv_value] >> 8) >= i)
                            monomeLedBuffer[80 + i] = 7;
                        else
                            monomeLedBuffer[80 + i] = 0;

                        if (((p->cv_values[edit_cv_value] >> 4) & 0xf) >= i)
                            monomeLedBuffer[96 + i] = 4;
                        else
                            monomeLedBuffer[96 + i] = 0;
                    }
                    else {
                        monomeLedBuffer[80 + i] = 0;
                        monomeLedBuffer[96 + i] = 0;
                    }

                    // show steps
                    if (p->cv_steps[edit_cv_ch][edit_cv_step] & (1 << i))
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
                    if (p->cv_probs[edit_cv_ch][i] == 255)
                        monomeLedBuffer[48 + i] = 11;
                    else if (p->cv_probs[edit_cv_ch][i] > 0)
                        monomeLedBuffer[48 + i] = 7;

                    monomeLedBuffer[64 + i] = (i < 8) * 4;
                    monomeLedBuffer[80 + i] = (i < 8) * 4;
                    monomeLedBuffer[96 + i] = (i < 8) * 4;
                    monomeLedBuffer[112 + i] = 0;
                }

                monomeLedBuffer[112] = 7;
            }
        }
        else if (edit_prob == 1) {
            for (uint8_t i = 0; i < kGridWidth; i++) {
                monomeLedBuffer[64 + i] = 4;
                monomeLedBuffer[80 + i] = 4;
                monomeLedBuffer[96 + i] = 4;
                monomeLedBuffer[112 + i] = 4;

                if (p->cv_probs[edit_cv_ch][i] == 255)
                    monomeLedBuffer[48 + i] = 11;
                else if (p->cv_probs[edit_cv_ch][i] == 0) {
                    monomeLedBuffer[48 + i] = 0;
                    monomeLedBuffer[112 + i] = 7;
                }
                else if (p->cv_probs[edit_cv_ch][i]) {
                    monomeLedBuffer[48 + i] = 4;
                    monomeLedBuffer[64 +
                                    16 * (3 -
                                          (p->cv_probs[edit_cv_ch][i] >> 6)) +
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
                if (i + series_scroll_pos == w.series_start ||
                    i + series_scroll_pos == w.series_end)
                    monomeLedBuffer[32 + i * 16 + j] = 4;
                else
                    monomeLedBuffer[32 + i * 16 + j] = 0;
            }

            // scroll position helper
            monomeLedBuffer[32 + i * 16 +
                            ((series_scroll_pos + i) / (64 / kGridWidth))] = 4;

            // sidebar selection indicators
            if (i + series_scroll_pos > w.series_start &&
                i + series_scroll_pos < w.series_end) {
                monomeLedBuffer[32 + i * 16] = 4;
                monomeLedBuffer[32 + i * 16 + kMaxLoopLength] = 4;
            }

            for (uint8_t j = 0; j < kGridWidth; j++) {
                // show possible states
                if ((w.series_list[i + series_scroll_pos] >> j) & 1)
                    monomeLedBuffer[32 + (i * 16) + j] = 7;
            }
        }

        // highlight playhead
        if (series_pos >= series_scroll_pos &&
            series_pos < series_scroll_pos + 6) {
            monomeLedBuffer[32 + (series_pos - series_scroll_pos) * 16 +
                            series_playing] = 11;
        }
    }

    monome_set_quadrant_flag(0);
    monome_set_quadrant_flag(1);
}


////////////////////////////////////////////////////////////////////////////////
// application grid code

void bw_key_press(uint8_t x, uint8_t y, uint8_t z) {
    //// TRACK LONG PRESSES
    if (z) {
        uint8_t index = y * 16 + x;
        held_keys[key_count] = index;
        key_count++;
        key_times[index] = 10;  //// THRESHOLD key hold time
    }
    else {
        uint8_t index = y * 16 + x;
        uint8_t found = 0;  // "found"
        for (uint8_t i = 0; i < key_count; i++) {
            if (held_keys[i] == index) found++;
            if (found) held_keys[i] = held_keys[i + 1];
        }
        key_count--;

        // FAST PRESS
        if (key_times[index] > 0) {
            if (edit_mode != mSeries && preset_mode == 0) {
                if (index / 16 == 2) {
                    uint8_t i = index % 16;
                    if (key_alt)
                        next_pattern = i;
                    else {
                        pattern = i;
                        next_pattern = i;
                    }
                }
            }
            // PRESET MODE FAST PRESS DETECT
            else if (preset_mode == 1) {
                if (x == 0 && y != preset_select) {
                    preset_select = y;
                    for (uint8_t i = 0; i < 8; i++)
                        glyph[i] = flashy.glyph[preset_select][i];
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

        whale_pattern_t *p = &w.wp[pattern];

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
                        p->step_mode = mForward;
                    else if (x == kMaxLoopLength - 1)
                        p->step_mode = mReverse;
                    else if (x == kMaxLoopLength - 2)
                        p->step_mode = mDrunk;
                    else if (x == kMaxLoopLength - 3)
                        p->step_mode = mRandom;
                    // FIXME
                    else if (x == 0) {
                        if (pos == p->loop_start)
                            next_pos = p->loop_end;
                        else if (pos == 0)
                            next_pos = kMaxLoopLength;
                        else
                            next_pos--;
                        cut_pos = 1;
                        monomeFrameDirty++;
                    }
                    // FIXME
                    else if (x == 1) {
                        if (pos == p->loop_end)
                            next_pos = p->loop_start;
                        else if (pos == kMaxLoopLength)
                            next_pos = 0;
                        else
                            next_pos++;
                        cut_pos = 1;
                        monomeFrameDirty++;
                    }
                    else if (x == 2) {
                        next_pos = (rnd() % (p->loop_len + 1)) + p->loop_start;
                        cut_pos = 1;
                        monomeFrameDirty++;
                    }
                }
            }
            else if (keycount_pos == 2 && z) {
                p->loop_start = keyfirst_pos;
                p->loop_end = x;
                monomeFrameDirty++;
                if (p->loop_start > p->loop_end)
                    p->loop_dir = 2;
                else if (p->loop_start == 0 && p->loop_end == kMaxLoopLength)
                    p->loop_dir = 0;
                else
                    p->loop_dir = 1;

                p->loop_len = p->loop_end - p->loop_start;

                if (p->loop_dir == 2)
                    p->loop_len =
                        (kMaxLoopLength - p->loop_start) + p->loop_end + 1;
            }
        }

        // top row
        else if (y == 0) {
            if (x == kMaxLoopLength) {
                key_alt = z;
                monomeFrameDirty++;
            }
            else if (x < 4 && z) {
                if (key_alt) {
                    if (p->tr_mode == mPulse)
                        p->tr_mode = mGate;
                    else
                        p->tr_mode = mPulse;
                }
                else if (key_meta)
                    w.tr_mute[x] ^= 1;
                else
                    edit_mode = mTrig;
                edit_prob = 0;
                monomeFrameDirty++;
            }
            else if (x > 3 && x < 12 && z) {
                edit_cv_ch = (x - 4) / 4;
                edit_prob = 0;

                if (key_meta)
                    w.cv_mute[edit_cv_ch] ^= 1;
                else
                    edit_mode = mCV;

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
                    p->tr_steps[pos] |= 1 << (y - 4);
                else if (key_meta) {
                    p->step_choice ^= (1 << x);
                }
                else
                    p->tr_steps[x] ^= (1 << (y - 4));
                monomeFrameDirty++;
            }
            // step probs
            else if (z && y == 3) {
                if (key_alt)
                    edit_prob = 1;
                else {
                    if (p->tr_step_probs[x] == 255)
                        p->tr_step_probs[x] = 0;
                    else
                        p->tr_step_probs[x] = 255;
                }
                monomeFrameDirty++;
            }
            else if (edit_prob == 1) {
                if (z) {
                    if (y == 4)
                        p->tr_step_probs[x] = 192;
                    else if (y == 5)
                        p->tr_step_probs[x] = 128;
                    else if (y == 6)
                        p->tr_step_probs[x] = 64;
                    else
                        p->tr_step_probs[x] = 0;
                }
            }
        }

        // edit map and probs
        else if (edit_mode == mCV) {
            // step probs
            if (z && y == 3) {
                if (key_alt)
                    edit_prob = 1;
                else {
                    if (p->cv_probs[edit_cv_ch][x] == 255)
                        p->cv_probs[edit_cv_ch][x] = 0;
                    else
                        p->cv_probs[edit_cv_ch][x] = 255;
                }

                monomeFrameDirty++;
            }
            // edit data
            else if (edit_prob == 0) {
                if (scale_select && z) {
                    // index -= 64;
                    uint8_t index = (y - 4) * 8 + x;
                    if (index < 24 && y < 8) {
                        for (uint8_t i = 0; i < 16; i++)
                            p->cv_values[i] = SCALES[index][i];
                    }

                    scale_select = 0;
                    monomeFrameDirty++;
                }
                else {
                    if (z && y == 4) {
                        edit_cv_step = x;
                        uint8_t count = 0;
                        for (uint8_t i = 0; i < 16; i++)
                            if ((p->cv_steps[edit_cv_ch][edit_cv_step] >> i) &
                                1) {
                                count++;
                                edit_cv_value = i;
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
                    else if ((y == 5 || y == 6) && z && x < 4 &&
                             edit_cv_step != -1) {
                        int16_t delta = 0;
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
                            for (uint8_t i = 0; i < 16; i++) {
                                if (p->cv_values[i] + delta > 4092)
                                    p->cv_values[i] = 4092;
                                else if (delta < 0 &&
                                         p->cv_values[i] < -1 * delta)
                                    p->cv_values[i] = 0;
                                else
                                    p->cv_values[i] += delta;
                            }
                        }
                        else {
                            if (p->cv_values[edit_cv_value] + delta > 4092)
                                p->cv_values[edit_cv_value] = 4092;
                            else if (delta < 0 &&
                                     p->cv_values[edit_cv_value] < -1 * delta)
                                p->cv_values[edit_cv_value] = 0;
                            else
                                p->cv_values[edit_cv_value] += delta;
                        }

                        monomeFrameDirty++;
                    }
                    // choose values
                    else if (y == 7) {
                        keycount_cv += z * 2 - 1;
                        if (keycount_cv < 0) keycount_cv = 0;

                        if (z) {
                            uint8_t count = 0;
                            for (uint8_t i = 0; i < 16; i++)
                                if ((p->cv_steps[edit_cv_ch][edit_cv_step] >>
                                     i) &
                                    1)
                                    count++;

                            // single press toggle
                            if (keycount_cv == 1 && count < 2) {
                                p->cv_steps[edit_cv_ch][edit_cv_step] =
                                    (1 << x);
                                edit_cv_value = x;
                            }
                            // multiselect
                            else if (keycount_cv > 1 || count > 1) {
                                p->cv_steps[edit_cv_ch][edit_cv_step] ^=
                                    (1 << x);

                                if (!p->cv_steps[edit_cv_ch][edit_cv_step])
                                    p->cv_steps[edit_cv_ch][edit_cv_step] =
                                        (1 << x);

                                count = 0;
                                for (uint8_t i = 0; i < 16; i++)
                                    if ((p->cv_steps[edit_cv_ch]
                                                    [edit_cv_step] >>
                                         i) &
                                        1) {
                                        count++;
                                        edit_cv_value = i;
                                    }

                                if (count > 1) edit_cv_value = -1;
                            }

                            monomeFrameDirty++;
                        }
                    }
                }
            }
            else if (edit_prob == 1) {
                if (z) {
                    if (y == 4)
                        p->cv_probs[edit_cv_ch][x] = 192;
                    else if (y == 5)
                        p->cv_probs[edit_cv_ch][x] = 128;
                    else if (y == 6)
                        p->cv_probs[edit_cv_ch][x] = 64;
                    else
                        p->cv_probs[edit_cv_ch][x] = 0;
                }
            }
        }

        // series mode
        else if (edit_mode == mSeries) {
            if (z && key_alt) {
                if (x == 0)
                    series_next = y - 2 + series_scroll_pos;
                else if (x == kMaxLoopLength - 1)
                    w.series_start = y - 2 + series_scroll_pos;
                else if (x == kMaxLoopLength)
                    w.series_end = y - 2 + series_scroll_pos;

                if (w.series_end < w.series_start)
                    w.series_end = w.series_start;
            }
            else {
                keycount_series += z * 2 - 1;
                if (keycount_series < 0) keycount_series = 0;

                if (z) {
                    uint8_t count = 0;
                    for (uint8_t i = 0; i < 16; i++)
                        count +=
                            (w.series_list[y - 2 + series_scroll_pos] >> i) & 1;

                    // single press toggle
                    if (keycount_series == 1 && count < 2) {
                        w.series_list[y - 2 + series_scroll_pos] = (1 << x);
                    }
                    // multi-select
                    else if (keycount_series > 1 || count > 1) {
                        w.series_list[y - 2 + series_scroll_pos] ^= (1 << x);

                        // ensure not fully clear
                        if (!w.series_list[y - 2 + series_scroll_pos])
                            w.series_list[y - 2 + series_scroll_pos] = (1 << x);
                    }
                }
            }

            monomeFrameDirty++;
        }
    }
}
