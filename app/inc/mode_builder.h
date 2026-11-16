/*
************************************************************************************************************************
*           Pedalboard builder: the pedalboard graph on the panel.
*
*           The Duo X has no modes the way the Dwarf has: the builder sits on top of the system menu,
*           entered from CURRENT PEDALBOARD > EDIT PEDALBOARD, and gives the menu back when it is left.
*           The menu tools stay on underneath the whole time, so nothing the host sends meanwhile
*           draws a control over the graph.
*
*           Left display: the graph, or the plugins by name when the board is too big to find a
*           box in the picture; holding ENC1 switches between the two. ENC1 walks the view on the
*           left, BTN1 goes back to the menu.
*
*           Right display: the parameters of the plugin under the cursor, a page at a time. ENC2
*           walks them; a click flips a switch or fires a trigger, and on anything else starts and
*           ends the edit, during which ENC2 turns the value.
************************************************************************************************************************
*/

#ifndef MODE_BUILDER_H
#define MODE_BUILDER_H


/*
************************************************************************************************************************
*           INCLUDE FILES
************************************************************************************************************************
*/

#include <stdint.h>
#include "data.h"


/*
************************************************************************************************************************
*           DO NOT CHANGE THESE DEFINES
************************************************************************************************************************
*/

// the three buttons between the footswitches, as BM_button_pressed() numbers them
#define BM_BUTTON_BACK      0
#define BM_BUTTON_ADD       1
#define BM_BUTTON_DEL       2

// rows of the parameter page on the right display; mod-ui sends one page as controls with
// hw_id 0 .. BM_PARAM_ROWS - 1
#define BM_PARAM_ROWS       7

// The Duo's SRAM has no room for the builder next to the rest: its state goes to the AHB SRAM
// (see LPC.ld), and so do the buffers of the screens that draw it
#define BM_RAM              __attribute__((section(".bss.$RAM2")))


/*
************************************************************************************************************************
*           DATA TYPES
************************************************************************************************************************
*/

typedef struct PLUGIN_PARAMS_T {
    const char *title;
    control_t *controls[BM_PARAM_ROWS];     // the page in hand, by hw_id
    uint8_t total;          // parameters of the plugin, with bypass and presets
    uint8_t first;          // position of controls[0] among them
    uint8_t hover;          // the cursor, among them
    uint8_t editing;        // ENC2 turns the value under the cursor
} plugin_params_t;


/*
************************************************************************************************************************
*           FUNCTION PROTOTYPES
************************************************************************************************************************
*/

void BM_init(void);

// Whether the builder owns the panel. Read from every task, so it is only a flag.
uint8_t BM_is_active(void);

// From the menu: fetches the graph, asks the host to watch the board and draws both displays.
void BM_enter(void);

// Back to the menu. Tells the host to stop watching, so it must not be called from a protocol
// callback; BM_drop() is for those.
void BM_exit(void);

// Leaves without a word to the host, for the paths that come from a protocol callback, where
// a command sent and waited on would deadlock. Only clears the state: the caller is about to
// redraw the panel itself, or asks BM_tick() to put the menu back.
void BM_drop(uint8_t restore_menu);

void BM_up(uint8_t encoder);
void BM_down(uint8_t encoder);
void BM_encoder_click(uint8_t encoder);
void BM_encoder_hold(uint8_t encoder);
void BM_encoder_released(uint8_t encoder);
void BM_button_pressed(uint8_t button);

// A control_add of the host. Taken only while a page of parameters is being asked for, since
// mod-ui sends it with the hw_ids of the real actuators; returns 0 when it is not ours.
uint8_t BM_add_control(control_t *control);

// The host changed a value on the page, a preset load most of the time.
void BM_set_control(uint8_t hw_id, float value);

// The graph again around <focus>, for the managers once they have changed the board.
void BM_refresh_graph(int16_t focus);

// NEW PEDALBOARD from the menu: the builder opens on it once its load has ended, which the
// host reports with BM_pedalboard_loaded().
void BM_enter_after_load(void);
void BM_pedalboard_loaded(void);

// The host says the board changed. Only a flag: the refetch happens in BM_tick().
void BM_plugin_map_stale(void);

// From the displays task: refetches a stale graph and finishes a BM_drop().
void BM_tick(void);

void BM_print_screen(void);


/*
************************************************************************************************************************
*           END HEADER
************************************************************************************************************************
*/

#endif
