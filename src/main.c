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

// libavr32
#include "adc.h"
#include "conf_board.h"
#include "events.h"
#include "ftdi.h"
#include "i2c.h"
#include "ii.h"
#include "init_common.h"
#include "init_trilogy.h"
#include "monome.h"
#include "timers.h"
#include "types.h"
#include "util.h"

// bluewhale
#include "bluewhale.h"
#include "hardware.h"
#include "helpers.h"

#define FIRSTRUN_KEY 0x22


uint8_t clock_phase;
uint16_t clock_time;
uint16_t clock_temp;


// NVRAM data structure located in the flash array.
__attribute__((__section__(".flash_nvram"))) nvram_data_t flashy;


////////////////////////////////////////////////////////////////////////////////
// prototypes

static void refresh_preset(void);

// start/stop monome polling/refresh timers
static void timers_set_monome(void);
static void timers_unset_monome(void);

// check the event queue
static void check_events(void);

// handler protos
static void handler_KeyTimer(int32_t data);
static void handler_Front(int32_t data);
static void handler_ClockNormal(int32_t data);
static void handler_ClockExt(int32_t data);

static void bw_process_ii(uint8_t *data, uint8_t l);

////////////////////////////////////////////////////////////////////////////////
// timers

static softTimer_t clockTimer = {.next = NULL, .prev = NULL };
static softTimer_t keyTimer = {.next = NULL, .prev = NULL };
static softTimer_t adcTimer = {.next = NULL, .prev = NULL };
static softTimer_t monomePollTimer = {.next = NULL, .prev = NULL };
static softTimer_t monomeRefreshTimer = {.next = NULL, .prev = NULL };


static void clockTimer_callback(void *NOTUSED(obj)) {
    if (clock_external == 0) {
        clock_phase++;
        if (clock_phase > 1) clock_phase = 0;
        (*clock_pulse)(clock_phase);
    }
}

static void keyTimer_callback(void *NOTUSED(obj)) {
    static event_t e;
    e.type = kEventKeyTimer;
    e.data = 0;
    event_post(&e);
}

static void adcTimer_callback(void *NOTUSED(obj)) {
    static event_t e;
    e.type = kEventPollADC;
    e.data = 0;
    event_post(&e);
}


// monome polling callback
static void monome_poll_timer_callback(void *NOTUSED(obj)) {
    // asynchronous, non-blocking read
    // UHC callback spawns appropriate events
    ftdi_read();
}

// monome refresh callback
static void monome_refresh_timer_callback(void *NOTUSED(obj)) {
    if (monomeFrameDirty > 0) {
        static event_t e;
        e.type = kEventMonomeRefresh;
        event_post(&e);
    }
}

// monome: start polling
void timers_set_monome() {
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
// hardware

void set_clock_output(bool value) {
    if (value)
        gpio_set_gpio_pin(B10);
    else
        gpio_clr_gpio_pin(B10);
}

void set_gate_output(uint8_t index, bool value) {
    const uint32_t gate_pins[4] = { B00, B01, B02, B03 };
    if (value)
        gpio_set_gpio_pin(gate_pins[index]);
    else
        gpio_clr_gpio_pin(gate_pins[index]);
}

void set_cv_output(uint8_t index, uint16_t value) {
    const uint8_t dac_addresses[2] = {
        0x31,  // A
        0x38   // B
    };
    spi_selectChip(DAC_SPI, DAC_SPI_NPCS);
    spi_write(DAC_SPI, dac_addresses[index]);  // update A
    spi_write(DAC_SPI, value >> 4);
    spi_write(DAC_SPI, value << 4);
    spi_unselectChip(DAC_SPI, DAC_SPI_NPCS);
}

////////////////////////////////////////////////////////////////////////////////
// event handlers

static void handler_FtdiConnect(int32_t NOTUSED(data)) {
    ftdi_setup();
}
static void handler_FtdiDisconnect(int32_t NOTUSED(data)) {
    timers_unset_monome();
}

static void handler_MonomeConnect(int32_t NOTUSED(data)) {
    bw_monome_connect();
    timers_set_monome();
}

static void handler_MonomeDisconnect(int32_t NOTUSED(data)) {}

static void handler_MonomePoll(int32_t NOTUSED(data)) {
    monome_read_serial();
}
static void handler_MonomeRefresh(int32_t NOTUSED(data)) {
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

static void handler_PollADC(int32_t NOTUSED(data)) {
    uint16_t adc[4];
    adc_convert(&adc);

    // Clock pot
    uint16_t clock = adc[0] >> 2;
    if (clock != clock_temp) {
        // 1000ms - 24ms
        clock_time = 25000 / (clock + 25);

        timer_set(&clockTimer, clock_time);
    }
    clock_temp = clock;

    uint16_t param = adc[1] >> 6;
    bw_set_param(param);
}

static void handler_SaveFlash(int32_t NOTUSED(data)) {
    flash_write();
}

static void handler_KeyTimer(int32_t NOTUSED(data)) {
    bw_key_timer();
}

static void handler_ClockNormal(int32_t NOTUSED(data)) {
    clock_external = !gpio_get_pin_value(B09);
}

static void handler_ClockExt(int32_t data) {
    bw_clock(data);
}


static void handler_MonomeGridKey(int32_t data) {
    uint8_t x, y, z;
    monome_grid_key_parse_event_data(data, &x, &y, &z);
    bw_key_press(x, y, z);
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


static void bw_process_ii(uint8_t *NOTUSED(data), uint8_t NOTUSED(l)) {}


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
    app_event_handlers[kEventMonomeDisconnect] = &handler_MonomeDisconnect;
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
uint8_t flash_is_fresh() {
    return (flashy.fresh != FIRSTRUN_KEY);
}

// write fresh status
void flash_unfresh() {
    flashc_memset8((void *)&(flashy.fresh), FIRSTRUN_KEY, 4, true);
}

void flash_write() {
    flashc_memcpy((void *)&flashy.w[preset_select], &w, sizeof(w), true);
    flashc_memcpy((void *)&flashy.glyph[preset_select], &glyph, sizeof(glyph),
                  true);
    flashc_memset8((void *)&(flashy.preset_select), preset_select, 1, true);
    flashc_memset32((void *)&(flashy.edit_mode), edit_mode, 4, true);
}

void flash_read() {
    uint8_t i1, i2;

    print_dbg("\r\n read preset ");
    print_dbg_ulong(preset_select);

    for (i1 = 0; i1 < 16; i1++) {
        for (i2 = 0; i2 < 16; i2++) {
            w.wp[i1].tr_steps[i2] = flashy.w[preset_select].wp[i1].tr_steps[i2];
            w.wp[i1].tr_step_probs[i2] =
                flashy.w[preset_select].wp[i1].tr_step_probs[i2];
            w.wp[i1].cv_probs[0][i2] =
                flashy.w[preset_select].wp[i1].cv_probs[0][i2];
            w.wp[i1].cv_probs[1][i2] =
                flashy.w[preset_select].wp[i1].cv_probs[1][i2];
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
                w.wp[i].tr_steps[j] = 0;
                w.wp[i].tr_step_probs[j] = 255;
                w.wp[i].cv_probs[0][j] = 255;
                w.wp[i].cv_probs[1][j] = 255;
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

    process_ii = &bw_process_ii;

    clock_pulse = &bw_clock;
    clock_external = !gpio_get_pin_value(B09);

    timer_add(&clockTimer, 120, &clockTimer_callback, NULL);
    timer_add(&keyTimer, 50, &keyTimer_callback, NULL);
    timer_add(&adcTimer, 100, &adcTimer_callback, NULL);
    clock_temp = 10000;  // out of ADC range to force tempo

    while (true) { check_events(); }
}
