
/*
************************************************************************************************************************
*
************************************************************************************************************************
*/

#ifndef SCREEN_H
#define SCREEN_H


/*
************************************************************************************************************************
*           INCLUDE FILES
************************************************************************************************************************
*/

#include <stdint.h>
#include "config.h"
#include "data.h"
#include "plugin_map.h"
#include "mode_builder.h"


/*
************************************************************************************************************************
*           DO NOT CHANGE THESE DEFINES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           CONFIGURATION DEFINES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           DATA TYPES
************************************************************************************************************************
*/

// the builder screens: the managers fill these, the screen functions draw them

// what goes around the left display of the builder: the title bar, and the three buttons
typedef struct BUILDER_CHROME_T {
    const char *title;          // NULL for the pedalboard name, or the list's position
    const char *labels[3];      // BTN1..BTN3, NULL for a button with nothing to do
    int8_t blink;               // the button flashing in reverse, or -1
} builder_chrome_t;

typedef struct {
    /* name of the box whose connections these are */
    const char *title;
    /* one row per cable, or per port in the lists that follow "add" */
    char **rows;
    uint8_t count;
    int16_t hover;
    /* which signal type the list is filtered to, as a word for the footer */
    const char *filter;
    /* the second button: DEL once a cable is armed, ADD while none is, and SELECT in the
       three lists that follow ADD */
    uint8_t can_delete;
    uint8_t can_add;
    uint8_t can_select;
    /* The strip under the list: one of the cables the hovered row stands for, feeding end
       first. NULL where the row is a port rather than a cable. `pair_count` above one is
       what puts the arrows on, since the third encoder can then walk them. */
    const char *pair_source;
    const char *pair_sink;
    uint8_t pair_count;
    uint8_t pair_index;
    /* which half of the armed cable's blink this is: the hovered row and DEL are drawn
       in reverse together, so the two read as one thing about to happen */
    uint8_t blink_reverse;
} connections_t;

typedef struct {
    /* the two columns: categories on the left, what is in one of them on the right */
    char **categories;
    uint8_t category_count;
    int16_t category_hover;

    char **plugins;
    uint8_t plugin_count;
    int16_t plugin_hover;

    /* the signal type the lists are filtered to, as a word for the footer */
    const char *filter;

    /* the letter being scrubbed to, drawn over the lists; NULL when not scrubbing */
    const char *scrub;
} plugin_manager_t;

/* how much of a description the overlay shows at once, and the widest line it wraps to */
#define SCREEN_INFO_LINES   6
#define SCREEN_INFO_ROOM    31

typedef struct {
    const char *name;
    const char *brand;
    const char *category;
    /* audio in, audio out, MIDI in, MIDI out, CV in, CV out */
    const uint8_t *ports;
    /* the plugin's own description, one long line; the screen wraps it */
    const char *comment;
    /* the line of it to start at, which the first encoder walks */
    uint8_t first_line;
} plugin_info_t;

typedef struct {
    /* the box whose parameters these are */
    const char *title;

    /* column one: what there is to bind */
    char **params;
    uint8_t param_count;
    int16_t param_hover;
    /* one per row above: the parameter is bound, on this page or another */
    const uint8_t *param_bound;

    /* which page of the board, as a heading over the second column */
    const char *page;

    /* column two: every slot on the panel, as the host lists them */
    char **actuators;
    uint8_t actuator_count;
    int16_t actuator_hover;
    /* one per row above: the slot on this page already holds an addressing */
    const uint8_t *actuator_taken;

    /* the hovered slot holds something, so the third button offers to take it off */
    uint8_t can_delete;
    uint8_t armed;
    /* which half of the armed row's blink this is */
    uint8_t blink_reverse;
} bindings_t;


/*
************************************************************************************************************************
*           GLOBAL VARIABLES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           MACRO'S
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           FUNCTION PROTOTYPES
************************************************************************************************************************
*/

void screen_clear(uint8_t display_id);
void screen_encoder(uint8_t display_id, control_t *control);
void screen_controls_index(uint8_t display_id, uint8_t current, uint8_t max);
void screen_pb_name(const void *data, uint8_t update);
void screen_ss_name(const void *data, uint8_t update);
void screen_footer(uint8_t display_id, const char *name, const char *value, int16_t property);
void screen_tool(uint8_t tool, uint8_t display_id);
void screen_bp_list(const char *title, bp_list_t *list);
void screen_bp_list_on(uint8_t display_id, const char *title, bp_list_t *list);
void screen_browser_keys(const char *title, const char *click, const char *hold, const char *knob);
void screen_browser_loading(const char *name);
void screen_browser_ui_connected(void);
void screen_system_menu(menu_item_t *item);
void screen_tuner(float frequency, char *note, int8_t cents);
void screen_tuner_input(uint8_t input);
void screen_image(uint8_t display, const uint8_t *image);


// the builder: the pedalboard graph on the left display, or the plugins by name when <list> is
// given, and the box under the cursor on the right; a NULL <name> is no box at all
void screen_plugin_map(const plugin_map_t *map, uint8_t loaded, const plugin_list_t *list,
                       const builder_chrome_t *chrome);
void screen_plugin_details(const char *name, uint8_t kind, uint8_t bypassed);
void screen_plugin_params(const plugin_params_t *params);
// the managers of the builder; each draws both displays, but for the plugin info, which goes
// over the plugin list on the right one
// <picks> and <pick_title> while the box to wire to is chosen, NULL otherwise
void screen_connections(const connections_t *model, const plugin_map_t *map, uint8_t loaded,
                        const plugin_list_t *picks, const char *pick_title);
void screen_plugin_manager(const plugin_manager_t *model);
void screen_plugin_info(const plugin_info_t *model);
// how many lines screen_plugin_info() wraps a description into, for clamping the scroll
uint8_t screen_plugin_info_lines(const char *text);
void screen_bindings(const bindings_t *model);
// a two-line box over the right display, while the host takes its time
void screen_notice(const char *first, const char *second);

/*
************************************************************************************************************************
*           CONFIGURATION ERRORS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           END HEADER
************************************************************************************************************************
*/

#endif
