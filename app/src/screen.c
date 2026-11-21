
/*
************************************************************************************************************************
*           INCLUDE FILES
************************************************************************************************************************
*/

#include "screen.h"
#include "utils.h"
#include "glcd.h"
#include "glcd_widget.h"
#include "naveg.h"
#include "hardware.h"
#include "images.h"
#include "protocol.h"
#include "plugin_map.h"
#include "glcd_clip.h"
#include "mode_builder.h"
#include "widget_area.h"
#include <string.h>


/*
************************************************************************************************************************
*           LOCAL DEFINES
************************************************************************************************************************
*/

#define FOOTER_NAME_WIDTH       50
#define FOOTER_VALUE_WIDTH      (DISPLAY_WIDTH - FOOTER_NAME_WIDTH - 10)

enum {BANKS_LIST, PEDALBOARD_LIST};

/*
************************************************************************************************************************
*           LOCAL CONSTANTS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           LOCAL DATA TYPES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           LOCAL MACROS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           LOCAL GLOBAL VARIABLES
************************************************************************************************************************
*/

static tuner_t g_tuner = {0, NULL, 0, 1};

// the loaded pedalboard, as the host last named it; the top bar and the builder title show it
static char g_pedalboard_name[20] = "DEFAULT";

/*
************************************************************************************************************************
*           LOCAL FUNCTION PROTOTYPES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           LOCAL CONFIGURATION ERRORS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           LOCAL FUNCTIONS
************************************************************************************************************************
*/

// A control value in at most 5 characters, as the encoder screen shows it: 10999 becomes 11K, and
// the smaller the value the more decimals. <text> holds at least 6 bytes.
static void value_to_text(float value, char *text)
{
    //value_str is our temporary value which we use for what kind of value representation we need
    char value_str[10] = {0};

    //if the value becomes bigger then 9999 (4 characters), then switch to another view 10999 becomes 10.9K
    if (value > 9999) {
        int_to_str(value/1000, value_str, sizeof(value_str) - 1, 0);
        strcat(value_str, "K");
    }
    //else if we have a value bigger then 100, we dont display decimals anymore
    else if (value > 99.9) {
        int_to_str(value, value_str, sizeof(value_str), 0);
    }
    //else if we have a value bigger then 10 we display just one decimal
    else if (value > 9.9) {
        float_to_str(value, value_str, sizeof(value_str), 1);
    }
    //if the value becomes less then 0 we change to 1 or 0 decimals
    else if (value < 0) {
        if (value > -99.9) {
            float_to_str(value, value_str, sizeof(value_str), 1);
        }
        else if (value < -9999.9) {
            int_to_str((value/1000), value_str, sizeof(value_str) - 1, 0);
            strcat(value_str, "K");
        }
        else int_to_str(value, value_str, sizeof(value_str), 0);
    }
    //for values between 0 and 10 display 2 decimals
    else {
        float_to_str(value, value_str, sizeof(value_str), 2);
    }

    //copy the first 5 char (value_str is zero filled), and terminate the text with line ending
    memcpy(text, value_str, 5);
    text[5] = '\0';
}


/*
 * A knob next to a plugin widget that takes the other half of its rows: drawn the way the Dwarf draws its
 * encoders, its name over a thin bar and the value under it, in a box `width` wide. The rows run from y to
 * y + 24.
 */
static void print_small_encoder(glcd_t *display, uint8_t display_id, const control_t *control, uint8_t x, uint8_t y,
                                uint8_t width)
{
    static char *labels_list[64];
    char buffer[17];
    const uint8_t chars = width / 4;
    const uint8_t middle = x + (width / 2);

    //no control
    if (!control)
    {
        strcpy(buffer, "KNOB #");
        buffer[5] = display_id + '1';
        glcd_text(display, middle - 2*strlen(buffer), y + 10, buffer, Terminal3x5, GLCD_BLACK);
        return;
    }

    //the name, as much of it as the width takes
    strncpy(buffer, control->label, chars);
    buffer[chars] = '\0';
    glcd_text(display, middle - 2*strlen(buffer), y, buffer, Terminal3x5, GLCD_BLACK);

    // list type control
    if (control->properties & (FLAG_CONTROL_ENUMERATION | FLAG_CONTROL_SCALE_POINTS))
    {
        uint8_t count = control->scale_points_count > 64 ? 64 : control->scale_points_count;

        uint8_t i;
        for (i = 0; i < count; i++)
        {
            labels_list[i] = control->scale_points[i]->label;
        }

        listbox_t list;
        list.x = x;
        list.y = y + 7;
        list.width = width;
        list.height = 19;
        list.color = GLCD_BLACK;
        list.selected = control->step;
        list.count = count;
        list.list = labels_list;
        widget_list_value(display, &list);
    }
    else if (control->properties & (FLAG_CONTROL_TOGGLED | FLAG_CONTROL_BYPASS))
    {
        toggle_t toggle;
        toggle.x = x;
        toggle.y = y + 6;
        toggle.width = width;
        toggle.height = 18;
        toggle.color = GLCD_BLACK;
        toggle.value = (control->properties & FLAG_CONTROL_TOGGLED)?control->value:!control->value;
        widget_toggle_small(display, &toggle);
    }
    //linear / log / int
    else
    {
        char str_bfr[15] = {0};

        if ((control->properties & FLAG_CONTROL_INTEGER) || (control->value > 999.9f) || (control->value < -999.9f))
            int_to_str(control->value, str_bfr, sizeof(str_bfr), 0);
        else if ((control->value > 99.99f) || (control->value < -99.99f))
            float_to_str((control->value), str_bfr, sizeof(str_bfr), 1);
        else if ((control->value > 9.99f) || (control->value < -9.99f))
            float_to_str((control->value), str_bfr, sizeof(str_bfr), 2);
        else
            float_to_str((control->value), str_bfr, sizeof(str_bfr), 3);

        str_bfr[chars < sizeof(str_bfr) ? chars : sizeof(str_bfr) - 1] = '\0';

        bar_t bar;
        bar.x = x;
        bar.y = y;
        bar.width = width;
        bar.height = 6;
        bar.color = GLCD_BLACK;
        bar.step = control->step;
        bar.steps = control->steps - 1;
        bar.label = str_bfr;
        widget_bar_encoder(display, &bar);

        if (strcmp("", control->unit) != 0)
        {
            strncpy(buffer, control->unit, chars);
            buffer[chars] = '\0';
            glcd_text(display, middle - 2*strlen(buffer), y + 19, buffer, Terminal3x5, GLCD_BLACK);
        }
    }
}


/*
************************************************************************************************************************
*           GLOBAL FUNCTIONS
************************************************************************************************************************
*/

void screen_clear(uint8_t display_id)
{
    glcd_clear(hardware_glcds(display_id), GLCD_WHITE);
}

void screen_encoder(uint8_t display_id, control_t *control)
{    
    glcd_t *display = hardware_glcds(display_id);

    //a plugin widget on one half of the knob rows: the knob takes the other half, drawn small. The right half
    //keeps clear of the controls index in its corner.
    static const glcd_rect_t knob_rows = { 0, 10, DISPLAY_WIDTH, 40 };
    const uint8_t halves = WA_halves(display_id, &knob_rows);
    if (halves == 1 || halves == 2)
    {
        glcd_rect_fill(display, 0, 9, DISPLAY_WIDTH - 11, 15, GLCD_WHITE);
        glcd_rect_fill(display, 0, 24, DISPLAY_WIDTH, 26, GLCD_WHITE);
        print_small_encoder(display, display_id, control, (halves == 1) ? 67 : 8, 18, 48);
        return;
    }

    //no control? 
    if (!control)
    {
        glcd_rect_fill(display, 0, 9, DISPLAY_WIDTH, 41, GLCD_WHITE);

        //draw back the title line on 16
        glcd_hline(display, 0, 16, DISPLAY_WIDTH, GLCD_BLACK);

        char text[sizeof(SCREEN_ROTARY_DEFAULT_NAME) + 2];
        strcpy(text, SCREEN_ROTARY_DEFAULT_NAME);
        text[sizeof(SCREEN_ROTARY_DEFAULT_NAME)-1] = display_id + '1';
        text[sizeof(SCREEN_ROTARY_DEFAULT_NAME)] = 0;

        textbox_t title;
        title.color = GLCD_BLACK;
        title.mode = TEXT_SINGLE_LINE;
        title.font = Terminal7x8;
        title.height = 0;
        title.width = 0;
        title.top_margin = 0;
        title.bottom_margin = 0;
        title.left_margin = 0;
        title.right_margin = 0;
        title.text = text;
        title.align = ALIGN_CENTER_NONE;
        title.y = 33 - (Terminal7x8[FONT_HEIGHT] / 2);
        widget_textbox(display, &title);
        return;
    }
    else 
    {
        //clear the area
        glcd_rect_fill(display, 0, 9, DISPLAY_WIDTH - 11, 15, GLCD_WHITE);
        glcd_rect_fill(display, 0, 24, DISPLAY_WIDTH, 26, GLCD_WHITE);

        //draw back the title line on 16
        glcd_hline(display, 0, 16, DISPLAY_WIDTH, GLCD_BLACK);
        
        // control title label
        textbox_t title;
        title.color = GLCD_BLACK;
        title.mode = TEXT_SINGLE_LINE;
        title.font = Terminal5x7;
        title.top_margin = 0;
        title.bottom_margin = 0;
        title.left_margin = 2;
        title.right_margin = 14;
        title.height = 0;
        title.width = 0;
        
        title.y = 13;

        uint8_t char_cnt_name = strlen(control->label);
        //limit if to big
        if (char_cnt_name >= 19) char_cnt_name = 19;

        title.text = control->label;
        title.align = ALIGN_NONE_NONE;

        // clear the name area
        glcd_rect_fill(display, (((char_cnt_name > 16)?DISPLAY_WIDTH-9:DISPLAY_WIDTH) /2) - char_cnt_name*3-1, 12, ((6*char_cnt_name) +3), 9, GLCD_WHITE);

        title.x = (((char_cnt_name > 16)?DISPLAY_WIDTH-9:DISPLAY_WIDTH) /2) - char_cnt_name*3 + 1;
        widget_textbox(display, &title);

        // invert the name area
        glcd_rect_invert(display, (((char_cnt_name > 16)?DISPLAY_WIDTH-9:DISPLAY_WIDTH) /2) - char_cnt_name*3-1, 12, ((6*char_cnt_name) +3), 9);
    }

    // list type control
    if (control->properties & (FLAG_CONTROL_ENUMERATION | FLAG_CONTROL_SCALE_POINTS))
    {
        static char *labels_list[10];

        uint8_t i;
        for (i = 0; i < control->scale_points_count; i++)
        {
            labels_list[i] = control->scale_points[i]->label;
        }

        listbox_t list;
        list.x = 0;
        list.y = 24;
        list.width = DISPLAY_WIDTH;
        list.height = 25;
        list.color = GLCD_BLACK;
        list.font = Terminal3x5;
        list.font_highlight = Terminal5x7;
        list.selected = control->step;
        list.count = control->scale_points_count;
        list.list = labels_list;
        list.line_space = 1;
        list.line_top_margin = 1;
        list.line_bottom_margin = 1;
        list.text_left_margin = 1;
        widget_listbox4(display, &list);
    }
    else if (control->properties & (FLAG_CONTROL_TOGGLED | FLAG_CONTROL_BYPASS))
    {
        toggle_t toggle;
        toggle.x = 20;
        toggle.y = 26;
        toggle.width = 88;
        toggle.height = 21;
        toggle.color = GLCD_BLACK;
        toggle.value = (control->properties & FLAG_CONTROL_TOGGLED)?control->value:!control->value;;
        widget_toggle(display, &toggle);
    }
    else
    {
        bar_t bar;
        bar.x = 4;
        bar.y = 23;
        bar.width = 116;
        bar.height = 14;
        bar.color = GLCD_BLACK;
        bar.step = control->step;
        bar.steps = control->steps - 1;

        char str_bfr[15] = {0};
        if ((control->value > 99.9) || (control->properties & FLAG_CONTROL_INTEGER))
        {
            int_to_str(control->value, str_bfr, sizeof(str_bfr), 0);
        }
        else 
        {
            float_to_str((control->value), str_bfr, sizeof(str_bfr), 2);
        }

        if (strcmp("", control->unit) != 0)
        {
            strcat(str_bfr, " ");
            strcat(str_bfr, control->unit);
        }

        bar.label = str_bfr;

        widget_bar(display, &bar);
    }
}

void screen_controls_index(uint8_t display_id, uint8_t current, uint8_t max)
{
    char str_current[4], str_max[4];
    int_to_str(current, str_current, sizeof(str_current), 2);
    int_to_str(max, str_max, sizeof(str_max), 2);

    if (max == 0) return;

    glcd_t *display = hardware_glcds(display_id);

    //clear the part
    glcd_rect_fill(display, 119, 10, DISPLAY_WIDTH-119, 13, GLCD_WHITE);

    // draws the max field
    textbox_t index;
    index.color = GLCD_BLACK;
    index.mode = TEXT_SINGLE_LINE;
    index.font = Terminal3x5;
    index.height = 0;
    index.width = 0;
    index.bottom_margin = 0;
    index.left_margin = 0;
    index.right_margin = 0;
    index.align = ALIGN_NONE_NONE;
    index.top_margin = 0;
    index.text = str_current;
    index.x = 120;
    index.y = 11;
    widget_textbox(display, &index);

    // draws the current field
    index.y = 17;
    index.text = str_max;
    widget_textbox(display, &index);

    //invert the block
    glcd_rect_invert(display, 119, 10, DISPLAY_WIDTH-119, 13);
}

void screen_footer(uint8_t display_id, const char *name, const char *value, int16_t property)
{
    if (display_id > FOOTSWITCHES_COUNT) return;

    if (naveg_is_tool_mode(display_id)) return;

    glcd_t *display = hardware_glcds(display_id);

    // clear the footer area
    glcd_rect_fill(display, 0, 50, DISPLAY_WIDTH, DISPLAY_HEIGHT-50, GLCD_WHITE);
    glcd_hline(display, 0, 50, DISPLAY_WIDTH, GLCD_BLACK);

    // draws the name field
    textbox_t footer;
    footer.color = GLCD_BLACK;
    footer.mode = TEXT_SINGLE_LINE;
    footer.font = Terminal7x8;
    footer.height = 0;
    footer.width = 0;
    footer.top_margin = 0;
    footer.bottom_margin = 0;
    footer.left_margin = 1;
    footer.right_margin = 1;
    footer.y = 53;

    if (name == NULL || value == NULL)
    {
        char text[sizeof(SCREEN_FOOT_DEFAULT_NAME) + 2];
        strcpy(text, SCREEN_FOOT_DEFAULT_NAME);
        text[sizeof(SCREEN_FOOT_DEFAULT_NAME)-1] = display_id + '1';
        text[sizeof(SCREEN_FOOT_DEFAULT_NAME)] = 0;

        footer.text = text;
        footer.align = ALIGN_CENTER_NONE;
        widget_textbox(display, &footer);
        return;
    }
    //if we are in toggle, trigger or byoass mode we dont have a value
    else if (property & (FLAG_CONTROL_TOGGLED | FLAG_CONTROL_BYPASS | FLAG_CONTROL_TRIGGER | FLAG_CONTROL_MOMENTARY))
    {
        footer.text = name;
        footer.align = ALIGN_CENTER_NONE;
        widget_textbox(display, &footer);
    
        if (property & FLAG_CONTROL_MOMENTARY)
        {
            //reverse
            if (property & FLAG_CONTROL_REVERSE)
            {
                if ((value[1] != 'N') && !(property & FLAG_CONTROL_BYPASS))
                {
                    glcd_rect_invert(display, 0, 51, DISPLAY_WIDTH, DISPLAY_HEIGHT-51);
                }
                else if (property & FLAG_CONTROL_BYPASS)
                {
                    if (value[1] == 'N')
                    {
                        glcd_rect_invert(display, 0, 51, DISPLAY_WIDTH, DISPLAY_HEIGHT-51);
                    }
                }
            }
            else 
            {
                if ((value[1] == 'N') && !(property & FLAG_CONTROL_BYPASS))
                {
                    glcd_rect_invert(display, 0, 51, DISPLAY_WIDTH, DISPLAY_HEIGHT-51);
                }
                else if (property & FLAG_CONTROL_BYPASS)
                {
                    if (value[1] != 'N')
                    {
                        glcd_rect_invert(display, 0, 51, DISPLAY_WIDTH, DISPLAY_HEIGHT-51);
                    }
                } 
            }
        }
        else if (value[1] == 'N')
        {
            glcd_rect_invert(display, 0, 51, DISPLAY_WIDTH, DISPLAY_HEIGHT-51);
        }
    }
    else 
    {
        uint8_t char_cnt_name = strlen(name);
        uint8_t char_cnt_value = strlen(value);

        if ((char_cnt_value + char_cnt_name) > 15)
        {
            //both bigger then the limmit
            if ((char_cnt_value > 7)&&(char_cnt_name > 8))
            {
                char_cnt_name = 8;
                char_cnt_value = 7;
            }
            else if (char_cnt_value > 7)
            {
                char_cnt_value = 15 - char_cnt_name;
            }
            else if (char_cnt_name > 8)
            {
                char_cnt_name = 15 - char_cnt_value;
            }
        }

        char title_str_bfr[char_cnt_name];
        char value_str_bfr[char_cnt_value];

        //draw name
        strncpy(title_str_bfr, name, char_cnt_name);
        title_str_bfr[char_cnt_name] = '\0';
        footer.text = title_str_bfr;
        footer.align = ALIGN_LEFT_NONE;
        widget_textbox(display, &footer);

        // draws the value field
        strncpy(value_str_bfr, value, char_cnt_value);
        value_str_bfr[char_cnt_value] =  '\0';
        footer.text = value_str_bfr;
        footer.width = 0;
        footer.align = ALIGN_RIGHT_NONE;
        widget_textbox(display, &footer);

        // special handling for banks menu, invert
        if (property & FLAG_CONTROL_BANKS)
            glcd_rect_invert(display, DISPLAY_WIDTH - 10, 51, 10, DISPLAY_HEIGHT-52);
    }
}

void screen_pb_name(const void *data, uint8_t update)
{
    static uint8_t char_cnt = 7;
    glcd_t *display = hardware_glcds(0);

    if (update)
    {
        const char **name_list = (const char**)data;

        // get first list name, copy it to our string buffer
        const char *name_string = *name_list;
        strncpy(g_pedalboard_name, name_string, 19);
        g_pedalboard_name[19] = 0; // strncpy might not have final null byte

        // go to next name in list
        name_string = *(++name_list);

        while (name_string)
        {
            if ((strlen(g_pedalboard_name) + strlen(name_string) + 1) > 18)
            {
                // if we only have a 1 char left, nevermind
                if ((strlen(g_pedalboard_name) + 1) > 18)
                    break;

                strcat(g_pedalboard_name, " ");
                strncat(g_pedalboard_name, name_string, 19 - (strlen(g_pedalboard_name) + 1));
                break;
            }
            else
            {
                strcat(g_pedalboard_name, " ");
                strcat(g_pedalboard_name, name_string);
                name_string = *(++name_list);
            }
        }

        g_pedalboard_name[19] = 0;
        char_cnt = strlen(g_pedalboard_name);
    }

    //we dont display inside a menu
    if (naveg_is_tool_mode(0)) return;

    // clear the name area
    glcd_rect_fill(display, 0, 0, DISPLAY_WIDTH, 10, GLCD_WHITE);

    textbox_t title;
    title.color = GLCD_BLACK;
    title.mode = TEXT_SINGLE_LINE;
    title.font = Terminal5x7;
    title.top_margin = 1;
    title.bottom_margin = 0;
    title.left_margin = 10;
    title.right_margin = 1;
    title.height = 0;
    title.width = 0;
    title.text = g_pedalboard_name;
    title.align = ALIGN_NONE_NONE;
    title.y = 1;
    title.x = ((DISPLAY_WIDTH / 2) - (3*char_cnt) + 7);
    widget_textbox(display, &title);

    icon_pedalboard(display, title.x - 11, 1);

    // top right corner: the WiFi state, when there is an adapter (inverted with the bar)
    switch (system_wifi_icon())
    {
        case WIFI_ICON_AP:
            icon_access_point(display, DISPLAY_WIDTH - 11, 1);
        break;

        case WIFI_ICON_CONNECTED:
        case WIFI_ICON_DISCONNECTED:
            icon_wifi(display, DISPLAY_WIDTH - 11, 1, system_wifi_icon() == WIFI_ICON_CONNECTED);
        break;
    }

    //invert the top bar
    glcd_rect_invert(display, 0, 0, DISPLAY_WIDTH, 9);
}

void screen_ss_name(const void *data, uint8_t update)
{
    static char snapshot_name[20] = {'D','E','F','A','U','L','T','\0'};
    static uint8_t char_cnt = 7;
    glcd_t *display = hardware_glcds(1);

    if (update)
    {
        const char **name_list = (const char**)data;

        // get first list name, copy it to our string buffer
        const char *name_string = *name_list;
        strncpy(snapshot_name, name_string, 19);
        snapshot_name[19] = 0; // strncpy might not have final null byte

        // go to next name in list
        name_string = *(++name_list);

        while (name_string)
        {
            if ((strlen(snapshot_name) + strlen(name_string) + 1) > 18)
            {
                // if we only have a 1 char left, nevermind
                if ((strlen(snapshot_name) + 1) > 18)
                    break;

                strcat(snapshot_name, " ");
                strncat(snapshot_name, name_string, 19 - (strlen(snapshot_name) + 1));
                break;
            }
            else
            {
                strcat(snapshot_name, " ");
                strcat(snapshot_name, name_string);
                name_string = *(++name_list);
            }
        }

        snapshot_name[19] = 0;
        char_cnt = strlen(snapshot_name);
    }

    //we dont display inside a menu
    if (naveg_is_tool_mode(1)) return;

    // clear the name area
    glcd_rect_fill(display, 0, 0, DISPLAY_WIDTH, 10, GLCD_WHITE);

    textbox_t title;
    title.color = GLCD_BLACK;
    title.mode = TEXT_SINGLE_LINE;
    title.font = Terminal5x7;
    title.top_margin = 1;
    title.bottom_margin = 0;
    title.left_margin = 10;
    title.right_margin = 1;
    title.height = 0;
    title.width = 0;
    title.text = snapshot_name;
    title.align = ALIGN_NONE_NONE;
    title.y = 1;
    title.x = ((DISPLAY_WIDTH / 2) - (3*char_cnt) + 7);
    widget_textbox(display, &title);

    icon_snapshot(display, title.x - 11, 1);

    //invert the top bar
    glcd_rect_invert(display, 0, 0, DISPLAY_WIDTH, 9);
}

void screen_tool(uint8_t tool, uint8_t display_id)
{
    bp_list_t *bp_list;
    glcd_t *display = hardware_glcds(display_id);

    switch (tool)
    {
        case DISPLAY_TOOL_SYSTEM:
            naveg_reset_menu();
            naveg_enter(DISPLAY_TOOL_SYSTEM);
            break;

        case DISPLAY_TOOL_TUNER:
            g_tuner.frequency = 0.0;
            g_tuner.note = "?";
            g_tuner.cents = 0;
            widget_tuner(display, &g_tuner);
            break;

        case DISPLAY_TOOL_NAVIG:
            bp_list = naveg_get_banks();

            if (naveg_banks_mode_pb() == BANKS_LIST)
            {
                screen_bp_list("BANKS", bp_list);
            }
            else 
            {
                screen_bp_list(naveg_get_current_pb_name(), bp_list);
            }
            break;
    }
}

void screen_bp_list(const char *title, bp_list_t *list)
{
    screen_bp_list_on(DISPLAY_RIGHT, title, list);
}

// the footswitch browser lists on the left display
void screen_bp_list_on(uint8_t display_id, const char *title, bp_list_t *list)
{
    if (!naveg_is_tool_mode(display_id))
        return; 

    listbox_t list_box;
    textbox_t title_box;

    glcd_t *display = hardware_glcds(display_id);

    // clears the title
    glcd_rect_fill(display, 0, 0, DISPLAY_WIDTH, 9, GLCD_WHITE);

    // draws the title
    title_box.color = GLCD_BLACK;
    title_box.mode = TEXT_SINGLE_LINE;
    title_box.font = Terminal3x5;
    title_box.top_margin = 0;
    title_box.bottom_margin = 0;
    title_box.left_margin = 0;
    title_box.right_margin = 0;
    title_box.height = 0;
    title_box.width = 0;
    title_box.text = title;
    title_box.align = ALIGN_CENTER_TOP;
    widget_textbox(display, &title_box);

    // title line separator
    glcd_hline(display, 0, 9, DISPLAY_WIDTH, GLCD_BLACK_WHITE);

    // draws the list
    if (list)
    {
        uint8_t count = strarr_length(list->names);
        list_box.x = 0;
        list_box.y = 11;
        list_box.width = 128;
        list_box.height = 53;
        list_box.color = GLCD_BLACK;
        list_box.hover = list->hover - list->page_min;
        list_box.selected = list->selected - list->page_min;
        list_box.count = count;
        list_box.list = list->names;
        list_box.font = Terminal3x5;
        list_box.line_space = 2;
        list_box.line_top_margin = 1;
        list_box.line_bottom_margin = 1;
        list_box.text_left_margin = 2;
        widget_listbox(display, &list_box);
    }
    else
    {
        if (naveg_ui_status())
        {
            textbox_t message;
            message.color = GLCD_BLACK;
            message.mode = TEXT_SINGLE_LINE;
            message.font = alterebro24;
            message.top_margin = 0;
            message.bottom_margin = 0;
            message.left_margin = 0;
            message.right_margin = 0;
            message.height = 0;
            message.width = 0;
            message.text = "To access banks here please disconnect from the graphical interface";
            message.align = ALIGN_CENTER_MIDDLE;
            widget_textbox(display, &message);
        }
    }
}

// the right display while the footswitch browser is up: what it lists, and what the footswitches do
void screen_browser_keys(const char *title, const char *click, const char *hold)
{
    const char *keys[] = {click, hold, "HOLD FS1: EXIT", NULL};

    glcd_t *display = hardware_glcds(DISPLAY_RIGHT);
    glcd_clear(display, GLCD_WHITE);

    textbox_t box;
    box.color = GLCD_BLACK;
    box.mode = TEXT_SINGLE_LINE;
    box.font = Terminal5x7;
    box.top_margin = 0;
    box.bottom_margin = 0;
    box.left_margin = 0;
    box.right_margin = 0;
    box.height = 0;
    box.width = 0;
    box.text = title;
    box.align = ALIGN_CENTER_TOP;
    widget_textbox(display, &box);

    glcd_hline(display, 0, 9, DISPLAY_WIDTH, GLCD_BLACK_WHITE);

    uint8_t i;
    box.font = Terminal3x5;
    box.align = ALIGN_NONE_NONE;
    for (i = 0; keys[i]; i++)
    {
        // the widget keeps the size of the last text it drew
        box.width = 0;
        box.height = 0;
        box.x = 4;
        box.y = 18 + i * 13;
        box.text = keys[i];
        widget_textbox(display, &box);
    }
}

// the right display of the browser while a pedalboard loads; the name only when known
void screen_browser_loading(const char *name)
{
    glcd_t *display = hardware_glcds(DISPLAY_RIGHT);
    glcd_clear(display, GLCD_WHITE);

    textbox_t box;
    box.color = GLCD_BLACK;
    box.mode = TEXT_SINGLE_LINE;
    box.font = Terminal5x7;
    box.top_margin = 0;
    box.bottom_margin = 0;
    box.left_margin = 0;
    box.right_margin = 0;
    box.height = 0;
    box.width = 0;
    box.text = "LOADING...";
    box.align = ALIGN_NONE_NONE;
    box.x = (DISPLAY_WIDTH - 10 * 6) / 2;
    box.y = 20;
    widget_textbox(display, &box);

    if (!name || !*name) return;

    // the widget keeps the size of the last text it drew
    box.width = 0;
    box.height = 0;
    box.text = name;
    box.x = (DISPLAY_WIDTH - strlen(name) * 6) / 2;
    box.y = 36;
    widget_textbox(display, &box);
}

void screen_system_menu(menu_item_t *item)
{
    // return if system is disabled
    if (!naveg_is_tool_mode(DISPLAY_TOOL_SYSTEM))
        return;

    // the menu is still on under the builder, and must not draw over it
    if (BM_is_active())
        return;

    static menu_item_t *last_item = NULL;

    glcd_t *display;
    if (item->desc->id == ROOT_ID)
    {
        display = hardware_glcds(DISPLAY_TOOL_SYSTEM);
    }
    else if (item->desc->id == TUNER_ID)
    {
        return;
    }
    else
    {
        display = hardware_glcds(DISPLAY_RIGHT);
    }

    //we dont display a menu on the right screen when we are in the banks.
    if ((display == hardware_glcds(DISPLAY_RIGHT)) && (naveg_tool_is_on(DISPLAY_TOOL_NAVIG)))
    {
        return;
    } 

    // clear screen
    glcd_clear(display, GLCD_WHITE);

    // draws the title
    textbox_t title_box;
    title_box.color = GLCD_BLACK;
    title_box.mode = TEXT_SINGLE_LINE;
    title_box.font = Terminal3x5;
    title_box.top_margin = 0;
    title_box.bottom_margin = 0;
    title_box.left_margin = 0;
    title_box.right_margin = 0;
    title_box.height = 0;
    title_box.width = 0;
    title_box.align = ALIGN_CENTER_TOP;
    title_box.text = item->name;

    if (item->desc->type != MENU_CONFIRM2)
    {
        if ((item->desc->type == MENU_NONE) || (item->desc->type == MENU_TOGGLE))
        {
            if (last_item)
            {
                title_box.text = last_item->name;
                if (title_box.text[strlen(item->name) - 1] == ']')
                    title_box.text = last_item->desc->name;
            }
        }
        else if (title_box.text[strlen(item->name) - 1] == ']')
        {
            title_box.text = item->desc->name;
        }
        widget_textbox(display, &title_box);
    }

    // title line separator
    glcd_hline(display, 0, 9, DISPLAY_WIDTH, GLCD_BLACK_WHITE);
    glcd_hline(display, 0, 10, DISPLAY_WIDTH, GLCD_WHITE);

    // menu list
    listbox_t list;
    list.x = 0;
    list.y = 11;
    list.width = 128;
    list.height = 53;
    list.color = GLCD_BLACK;
    list.font = Terminal3x5;
    list.line_space = 2;
    list.line_top_margin = 1;
    list.line_bottom_margin = 1;
    list.text_left_margin = 2;

    // popup
    popup_t popup;
    popup.x = 0;
    popup.y = 0;
    popup.width = DISPLAY_WIDTH;
    popup.height = DISPLAY_HEIGHT;
    popup.font = Terminal3x5;

    switch (item->desc->type)
    {
        case MENU_LIST:
        case MENU_SELECT:
            list.hover = item->data.hover;
            list.selected = item->data.selected;
            list.count = item->data.list_count;
            list.list = item->data.list;
            widget_listbox(display, &list);
            if ((last_item && last_item->desc->id != TEMPO_ID) || item->desc->id == SYSTEM_ID || item->desc->id == BYPASS_ID)
                last_item = item;
            break;

        case MENU_CONFIRM:
        case MENU_CONFIRM2:
        case MENU_CANCEL:
        case MENU_OK:
        case MENU_MESSAGE:
            if (item->desc->type == MENU_CANCEL)
                popup.type = CANCEL_ONLY;
            else if (item->desc->type == MENU_OK)
                popup.type = OK_ONLY;
            else if (item->desc->type == MENU_MESSAGE)
                popup.type = EMPTY_POPUP;
            else
                popup.type = YES_NO;
            popup.title = item->data.popup_header;
            popup.content = item->data.popup_content;
            popup.button_selected = item->data.hover;
            widget_popup(display, &popup);
            break;

        case MENU_TOGGLE:
            if (item->desc->parent_id == PROFILES_ID ||  item->desc->id == EXP_CV_INP || item->desc->id == HP_CV_OUTP)
            {
                popup.type = YES_NO;
                popup.title = item->data.popup_header;
                popup.content = item->data.popup_content;
                popup.button_selected = item->data.hover;
                widget_popup(display, &popup);
            }
            else if (last_item)
            {
                list.hover = last_item->data.hover;
                list.selected = last_item->data.selected;
                list.count = last_item->data.list_count;
                list.list = last_item->data.list;
                widget_listbox(display, &list);
            }
            break;

        case MENU_VOL:
        case MENU_SET:
        case MENU_NONE:
        case MENU_RETURN:
            if (last_item)
            {
                list.hover = last_item->data.hover;
                list.selected = last_item->data.selected;
                list.count = last_item->data.list_count;
                list.list = last_item->data.list;
                widget_listbox(display, &list);
            }
            break;
    }
}

void screen_tuner(float frequency, char *note, int8_t cents)
{
    g_tuner.frequency = frequency;
    g_tuner.note = note;
    g_tuner.cents = cents;

    // checks if tuner is enable and update it
    if (naveg_is_tool_mode(DISPLAY_TOOL_TUNER))
        widget_tuner(hardware_glcds(1), &g_tuner);
}

void screen_tuner_input(uint8_t input)
{
    g_tuner.input = input;

    // checks if tuner is enable and update it
    if (naveg_is_tool_mode(DISPLAY_TOOL_TUNER))
        widget_tuner(hardware_glcds(1), &g_tuner);
}

void screen_image(uint8_t display, const uint8_t *image)
{
    glcd_t *display_img = hardware_glcds(display);
    glcd_draw_image(display_img, 0, 0, image, GLCD_BLACK);
}

/*
 * BUILDER: the outlines around the graph and the labels of the three buttons under it, in the
 * Dwarf builder's geometry. A button with nothing to do has no box, and the bottom edge runs
 * through where it would be. The one about to act flashes, in step with what it acts on.
 */
static void print_builder_buttons(glcd_t *display, const builder_chrome_t *chrome)
{
    const char * const *labels = chrome->labels;
    uint8_t i;

    glcd_vline(display, 0, 7, DISPLAY_HEIGHT - 11, GLCD_BLACK);
    glcd_vline(display, DISPLAY_WIDTH - 1, 7, DISPLAY_HEIGHT - 11, GLCD_BLACK);
    glcd_hline(display, 0, DISPLAY_HEIGHT - 5, DISPLAY_WIDTH, GLCD_BLACK);

    for (i = 0; i < 3; i++)
    {
        uint8_t x = 14 + (i * 34);

        if (!labels[i]) continue;

        glcd_rect_fill(display, x, DISPLAY_HEIGHT - 9, 31, 9, GLCD_WHITE);
        glcd_rect(display, x, DISPLAY_HEIGHT - 9, 31, 9, GLCD_BLACK);
        glcd_text(display, x + (31 - glcd_text_width(Terminal3x5, labels[i])) / 2, DISPLAY_HEIGHT - 7,
                  labels[i], Terminal3x5, GLCD_BLACK);

        if (chrome->blink == i)
            glcd_rect_invert(display, x, DISPLAY_HEIGHT - 9, 31, 9);
    }
}

// the inverted bar across the top of a display
static void print_builder_title(glcd_t *display, const char *text)
{
    textbox_t title_box = {};
    title_box.color = GLCD_BLACK;
    title_box.mode = TEXT_SINGLE_LINE;
    title_box.font = Terminal3x5;
    title_box.top_margin = 1;
    title_box.align = ALIGN_CENTER_TOP;
    title_box.text = text;
    widget_textbox(display, &title_box);
    glcd_rect_invert(display, 0, 0, DISPLAY_WIDTH, 7);
}

// left edge that centres a string, or the panel edge when it is wider than the panel
static uint8_t centred_x(const uint8_t *font, const char *text)
{
    int16_t width = glcd_text_width(font, text);
    return (width < DISPLAY_WIDTH) ? (DISPLAY_WIDTH - width) / 2 : 0;
}

static void print_empty_pedalboard(glcd_t *display)
{
    static const char first[] = "EMPTY";
    static const char second[] = "PEDALBOARD";

    glcd_text(display, centred_x(Terminal7x8, first), DISPLAY_HEIGHT / 2 - 10, first, Terminal7x8, GLCD_BLACK);
    glcd_text(display, centred_x(Terminal7x8, second), DISPLAY_HEIGHT / 2, second, Terminal7x8, GLCD_BLACK);
}

/*
 * BUILDER: the list view, the plugins by name in the viewport of the graph
 *
 * Drawn the way the banks list is, by widget_listbox(), with the cursor inverted. The title
 * says where the cursor is in the whole list, of which only a window is held.
 */
static void print_plugin_list(glcd_t *display, const glcd_rect_t *view, const plugin_list_t *list,
                              char *title, uint8_t title_size)
{
    static char *names[PLUGIN_LIST_MAX_ROWS] BM_RAM;
    uint8_t i, length;

    length = strlen(title);
    title[length++] = ' ';
    length += int_to_str(list->hover + 1, &title[length], title_size - length, 0);
    title[length++] = '/';
    int_to_str(list->total, &title[length], title_size - length, 0);

    if (list->count == 0)
    {
        static const char empty[] = "NO PLUGINS";
        glcd_text(display, centred_x(Terminal5x7, empty), 27, empty, Terminal5x7, GLCD_BLACK);
        return;
    }

    for (i = 0; i < list->count; i++)
        names[i] = (char *) list->rows[i].title;

    listbox_t list_box = {};
    list_box.x = view->x;
    list_box.y = view->y;
    list_box.width = view->width;
    list_box.height = view->height - 1;
    list_box.color = GLCD_BLACK;
    list_box.hover = list->hover - list->first;
    list_box.selected = 0xFF;
    list_box.count = list->count;
    list_box.list = names;
    list_box.font = Terminal3x5;
    list_box.line_space = 2;
    list_box.line_top_margin = 1;
    list_box.line_bottom_margin = 1;
    list_box.text_left_margin = 2;
    widget_listbox(display, &list_box);
}

/*
 * BUILDER: draw the pedalboard graph, or the list in its place
 *
 * The pedalboard name in the title bar, the graph in the viewport mode_builder.c set, and the
 * buttons under it. plugin_map_draw() clips to the viewport, so nothing can spill over the
 * button labels.
 */
void screen_plugin_map(const plugin_map_t *map, uint8_t loaded, const plugin_list_t *list,
                       const builder_chrome_t *chrome)
{
    glcd_t *display = hardware_glcds(DISPLAY_LEFT);
    char title[32];

    glcd_clear(display, GLCD_WHITE);

    strcpy(title, "PLUGINS");
    if (list)
        print_plugin_list(display, &map->view, list, title, sizeof(title));

    // a title of the caller's, while the picture is a chooser, or what the view is
    print_builder_title(display, chrome->title ? chrome->title : (list ? title : g_pedalboard_name));

    print_builder_buttons(display, chrome);

    if (list) return;

    if (loaded && map->n_nodes > 0)
        plugin_map_draw(display, map);
    else
        print_empty_pedalboard(display);
}

/*
 * BUILDER: the box under the cursor
 *
 * The graph cuts a box label to the width of the box; here it is the longer title mod-ui sends
 * for it, broken over two lines at a space when it does not fit on one.
 */
void screen_plugin_details(const char *name, uint8_t kind_id, uint8_t bypassed)
{
    glcd_t *display = hardware_glcds(DISPLAY_RIGHT);
    const char *kind;
    char line[PLUGIN_MAP_TITLE_SIZE];
    uint8_t per_line = DISPLAY_WIDTH / (Terminal5x7_WIDTH + 1);
    uint8_t length, cut, y = 20;

    glcd_clear(display, GLCD_WHITE);

    if (!name)
    {
        kind = "PEDALBOARD";
    }
    else if (kind_id == BM_HW_SOURCE)
    {
        kind = "INPUT";
    }
    else if (kind_id == BM_HW_SINK)
    {
        kind = "OUTPUT";
    }
    else
    {
        kind = "PLUGIN";
    }

    textbox_t title_box = {};
    title_box.color = GLCD_BLACK;
    title_box.mode = TEXT_SINGLE_LINE;
    title_box.font = Terminal3x5;
    title_box.top_margin = 1;
    title_box.align = ALIGN_CENTER_TOP;
    title_box.text = kind;
    widget_textbox(display, &title_box);
    glcd_rect_invert(display, 0, 0, DISPLAY_WIDTH, 7);

    if (!name) return;

    strncpy(line, name, sizeof(line) - 1);
    line[sizeof(line) - 1] = 0;
    length = strlen(line);

    // the last space that leaves the first line short enough, or a hard cut when there is none
    cut = length;
    if (length > per_line)
    {
        for (cut = per_line; cut > 0 && line[cut] != ' '; cut--);
        if (cut == 0) cut = per_line;
    }

    if (cut < length)
    {
        const char *rest = &line[cut];
        if (*rest == ' ') rest++;

        glcd_text(display, centred_x(Terminal5x7, rest), y + 10, rest, Terminal5x7, GLCD_BLACK);
        line[cut] = 0;
    }
    else
    {
        y += 5;
    }

    glcd_text(display, centred_x(Terminal5x7, line), y, line, Terminal5x7, GLCD_BLACK);

    if (kind_id == BM_PLUGIN && bypassed)
    {
        static const char bypassed[] = "BYPASSED";
        glcd_text(display, centred_x(Terminal3x5, bypassed), 46, bypassed, Terminal3x5, GLCD_BLACK);
    }
}

/*
 * BUILDER: the parameters of the plugin under the cursor
 *
 * One row each, the name on the left and the value on the right, a page at a time with a bar
 * on the right edge when there is more than one. The cursor row is inverted; while it is being
 * edited only its value is, the part the encoder is turning.
 */
#define PARAMS_ROW_Y        9
#define PARAMS_ROW_PITCH    7

static void param_value_text(const control_t *control, char *text, uint8_t size)
{
    if (control->properties & (FLAG_CONTROL_ENUMERATION | FLAG_CONTROL_SCALE_POINTS | FLAG_CONTROL_REVERSE))
    {
        const char *label = (control->scale_points_count && control->step < control->scale_points_count)
                            ? control->scale_points[control->step]->label : "";
        strncpy(text, label, size - 1);
        text[size - 1] = 0;
    }
    else if (control->properties & FLAG_CONTROL_BYPASS)
    {
        // the bypass row is named for the plugin being on, which is the bypass off
        strcpy(text, (control->value != 0) ? "OFF" : "ON");
    }
    else if (control->properties & FLAG_CONTROL_TOGGLED)
    {
        strcpy(text, (control->value != 0) ? "ON" : "OFF");
    }
    else if (control->properties & FLAG_CONTROL_TRIGGER)
    {
        strcpy(text, "PUSH");
    }
    else
    {
        value_to_text(control->value, text);

        if (control->unit && control->unit[0] && strcmp(control->unit, "none") != 0)
        {
            strncat(text, " ", size - strlen(text) - 1);
            strncat(text, control->unit, size - strlen(text) - 1);
        }
    }
}

void screen_plugin_params(const plugin_params_t *params)
{
    glcd_t *display = hardware_glcds(DISPLAY_RIGHT);
    uint8_t i;

    glcd_clear(display, GLCD_WHITE);

    textbox_t title_box = {};
    title_box.color = GLCD_BLACK;
    title_box.mode = TEXT_SINGLE_LINE;
    title_box.font = Terminal3x5;
    title_box.top_margin = 1;
    title_box.align = ALIGN_CENTER_TOP;
    title_box.text = params->title;
    widget_textbox(display, &title_box);
    glcd_rect_invert(display, 0, 0, DISPLAY_WIDTH, 7);

    for (i = 0; i < BM_PARAM_ROWS; i++)
    {
        const control_t *control = params->controls[i];
        uint8_t y = PARAMS_ROW_Y + i * PARAMS_ROW_PITCH;
        char value[24];
        int16_t value_x;
        glcd_rect_t clip;

        if (!control) continue;

        param_value_text(control, value, sizeof(value));
        value_x = DISPLAY_WIDTH - 4 - glcd_text_width(Terminal3x5, value);
        if (value_x < DISPLAY_WIDTH / 2) value_x = DISPLAY_WIDTH / 2;

        // the name gives way to the value
        clip.x = 0;
        clip.y = y;
        clip.width = value_x - 3;
        clip.height = PARAMS_ROW_PITCH;
        glcd_text_clip(display, &clip, 2, y + 1, control->label, Terminal3x5, GLCD_BLACK);

        clip.x = value_x;
        clip.width = DISPLAY_WIDTH - 2 - value_x;
        glcd_text_clip(display, &clip, value_x, y + 1, value, Terminal3x5, GLCD_BLACK);

        if (params->first + i == params->hover)
        {
            if (params->editing)
                glcd_rect_invert(display, value_x - 2, y, DISPLAY_WIDTH - 2 - (value_x - 2), PARAMS_ROW_PITCH);
            else
                glcd_rect_invert(display, 0, y, DISPLAY_WIDTH - 2, PARAMS_ROW_PITCH);
        }
    }

    // where this page is among all of them
    if (params->total > BM_PARAM_ROWS)
    {
        uint8_t track = DISPLAY_HEIGHT - PARAMS_ROW_Y;
        uint8_t top = PARAMS_ROW_Y + (track * params->first) / params->total;
        uint8_t length = (track * BM_PARAM_ROWS) / params->total;

        if (length < 3) length = 3;
        if (top + length > DISPLAY_HEIGHT) length = DISPLAY_HEIGHT - top;

        glcd_vline(display, DISPLAY_WIDTH - 1, top, length, GLCD_BLACK);
    }
}

/*
 * BUILDER: the lists of the managers
 *
 * The left display takes what the first encoder walks, with the buttons under it; the right
 * display what the second walks, the whole height below its title.
 */
#define BUILDER_RIGHT_LIST_Y    9
// rows of the parameter column in the bindings screen: the window the manager asks the host for
#define BUILDER_BIND_MAX_PARAMS 24
#define BUILDER_RIGHT_LIST_H    55

static void print_builder_list(glcd_t *display, uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                               char **rows, uint8_t count, int16_t hover)
{
    if (count == 0) return;

    listbox_t list_box = {};
    list_box.x = x;
    list_box.y = y;
    list_box.width = width;
    list_box.height = height;
    list_box.color = GLCD_BLACK;
    list_box.hover = (hover < 0) ? 0 : hover;
    list_box.selected = 0xFF;
    list_box.count = count;
    list_box.list = rows;
    list_box.font = Terminal3x5;
    list_box.line_space = 2;
    list_box.line_top_margin = 1;
    list_box.line_bottom_margin = 1;
    list_box.text_left_margin = 2;
    widget_listbox(display, &list_box);

    // no row stands under the cursor
    if (hover < 0) glcd_rect_invert(display, x, y + 1, width, 7);
}

/*
 * The other half of a blink on the cursor row: widget_listbox() always draws it in reverse,
 * and inverting it once more puts it back. Worked out the way the widget works it out --
 * there is no asking it where it drew.
 */
static void unblink_builder_row(glcd_t *display, uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                                uint8_t count, int16_t hover)
{
    uint8_t pitch = Terminal3x5[FONT_HEIGHT] + 2;
    uint8_t max_lines = height / pitch;
    uint8_t center = (max_lines / 2) - (1 - (max_lines % 2));
    int16_t first = 0;

    if (hover > center && count > max_lines)
    {
        first = hover - center;
        if (first > (count - max_lines)) first = count - max_lines;
    }

    glcd_rect_invert(display, x, y + 2 + (hover - first) * pitch - 1, width, Terminal3x5[FONT_HEIGHT] + 2);
}

static void print_centred_note(glcd_t *display, uint8_t y, const char *text)
{
    glcd_text(display, centred_x(Terminal3x5, text), y, text, Terminal3x5, GLCD_BLACK);
}

// at most <room> characters of <text> at <at> in <dst>; returns where they end
static uint8_t copy_upto(char *dst, uint8_t at, const char *text, uint8_t room)
{
    uint8_t length = (uint8_t) strlen(text);

    if (length > room) length = room;
    memcpy(&dst[at], text, length);

    return (uint8_t)(at + length);
}

/*
 * BUILDER: the connections of one box
 *
 * The cables on the right display, one row per box at the far end, with the cable the row
 * stands for named under them; the graph stays on the left. While the box to wire to is
 * picked the graph is the chooser, and the right display names the boxes it offers, the one
 * selected on the graph under the cursor.
 */
#define SCREEN_PAIR_Y       56
// half the width, less the " + " between the two names and the arrows at either edge
#define SCREEN_PAIR_ROOM    13

void screen_connections(const connections_t *model, const plugin_map_t *map, uint8_t loaded,
                        const plugin_list_t *picks, const char *pick_title)
{
    glcd_t *display = hardware_glcds(DISPLAY_RIGHT);
    builder_chrome_t chrome = { .title = NULL, .labels = { "BACK", NULL, NULL }, .blink = -1 };
    uint8_t list_height = BUILDER_RIGHT_LIST_H;

    /*
     * One button for three things, told apart by the list: ADD while it is idle, DEL once a
     * cable is armed -- flashing with the row it would delete -- and SELECT in the lists
     * that follow ADD, and while a box to wire to is being picked on the graph.
     */
    if (pick_title)
    {
        chrome.title = pick_title;
        chrome.labels[1] = "SELECT";
    }
    else if (model->can_delete)
    {
        chrome.labels[1] = "DEL";
        if (model->blink_reverse) chrome.blink = 1;
    }
    else if (model->can_add)
    {
        chrome.labels[1] = "ADD";
    }
    else if (model->can_select)
    {
        chrome.labels[1] = "SELECT";
    }

    // the filter cycles on the third button, so its label is what it currently shows
    chrome.labels[2] = model->filter;

    screen_plugin_map(map, loaded, NULL, &chrome);

    glcd_clear(display, GLCD_WHITE);

    if (pick_title)
    {
        static char *names[PLUGIN_LIST_MAX_ROWS] BM_RAM;
        const plugin_map_node_t *node = loaded ? plugin_map_selected(map) : NULL;
        int16_t hover = -1;
        uint8_t i, count = (picks && picks->loaded) ? picks->count : 0;

        for (i = 0; i < count; i++)
        {
            names[i] = (char *) picks->rows[i].title;
            if (node && picks->rows[i].id == node->id) hover = i;
        }

        print_builder_title(display, pick_title);
        if (count == 0) print_centred_note(display, 30, "NOWHERE TO CONNECT");
        print_builder_list(display, 0, BUILDER_RIGHT_LIST_Y, DISPLAY_WIDTH, BUILDER_RIGHT_LIST_H,
                           names, count, hover);
        return;
    }

    print_builder_title(display, model->title ? model->title : "CONNECTIONS");

    if (model->pair_source && model->pair_sink) list_height -= 8;

    if (model->count == 0 && model->can_add)
        print_centred_note(display, 30, "NO CONNECTIONS");

    print_builder_list(display, 0, BUILDER_RIGHT_LIST_Y, DISPLAY_WIDTH, list_height,
                       model->rows, model->count, model->hover);

    if (model->count > 0 && model->can_delete && !model->blink_reverse)
        unblink_builder_row(display, 0, BUILDER_RIGHT_LIST_Y, DISPLAY_WIDTH, list_height,
                            model->count, model->hover);

    /*
     * The cable the row under the cursor stands for, end to end, in reverse so it reads as
     * a caption. A row is one line per box at the far end however many cables run there,
     * so the first encoder walks them and the arrows at the edges say there are more.
     */
    if (model->pair_source && model->pair_sink)
    {
        char line[2 * SCREEN_PAIR_ROOM + 4];
        uint8_t at = 0;

        at = copy_upto(line, at, model->pair_source, SCREEN_PAIR_ROOM);
        line[at++] = ' ';
        line[at++] = '+';
        line[at++] = ' ';
        at = copy_upto(line, at, model->pair_sink, SCREEN_PAIR_ROOM);
        line[at] = 0;

        print_centred_note(display, SCREEN_PAIR_Y + 1, line);

        if (model->pair_count > 1)
        {
            glcd_text(display, 2, SCREEN_PAIR_Y + 1, "<", Terminal3x5, GLCD_BLACK);
            glcd_text(display, DISPLAY_WIDTH - 5, SCREEN_PAIR_Y + 1, ">", Terminal3x5, GLCD_BLACK);
        }

        glcd_rect_invert(display, 0, SCREEN_PAIR_Y, DISPLAY_WIDTH, 7);
    }
}

/*
 * BUILDER: adding a plugin
 *
 * The categories on the left, what is in the one under the cursor on the right. While the
 * second encoder is held the plugins are walked by first letter, which shows over the list.
 */
void screen_plugin_manager(const plugin_manager_t *model)
{
    glcd_t *display = hardware_glcds(DISPLAY_LEFT);
    builder_chrome_t chrome = { .title = NULL, .labels = { "BACK", "ADD", model->filter }, .blink = -1 };

    glcd_clear(display, GLCD_WHITE);
    print_builder_list(display, 2, 9, DISPLAY_WIDTH - 4, 42, model->categories, model->category_count,
                       model->category_hover);
    print_builder_title(display, "CATEGORIES");
    print_builder_buttons(display, &chrome);

    display = hardware_glcds(DISPLAY_RIGHT);
    glcd_clear(display, GLCD_WHITE);

    print_builder_title(display, (model->category_count && model->category_hover >= 0 &&
                                  model->category_hover < model->category_count)
                                 ? model->categories[model->category_hover] : "PLUGINS");

    if (model->plugin_count == 0)
        print_centred_note(display, 30, "NO PLUGINS");

    print_builder_list(display, 0, BUILDER_RIGHT_LIST_Y, DISPLAY_WIDTH, BUILDER_RIGHT_LIST_H,
                       model->plugins, model->plugin_count, model->plugin_hover);

    if (model->scrub)
    {
        uint8_t width = glcd_text_width(Terminal7x8, model->scrub);
        uint8_t x = (DISPLAY_WIDTH - width) / 2;

        glcd_rect_fill(display, x - 5, 24, width + 10, 16, GLCD_WHITE);
        glcd_rect(display, x - 5, 24, width + 10, 16, GLCD_BLACK);
        glcd_text(display, x, 28, model->scrub, Terminal7x8, GLCD_BLACK);
    }
}

/*
 * BUILDER: what the host knows about one plugin, over the list of plugins
 *
 * The description has the display to itself, so it is worth wrapping properly. Terminal3x5
 * is fixed width -- three pixels and a gap between glyphs -- so a line holds width/4
 * characters and the wrap is a count rather than a measurement. Greedy, breaking on the
 * last space that fits and falling back to a hard cut for a word longer than the line.
 */
#define SCREEN_INFO_PITCH   4
#define SCREEN_INFO_X       3
#define SCREEN_INFO_W       116     /* the arrows at the right margin take the rest */
#define SCREEN_INFO_ARROW   121
// two inverted lines: the name and the port shape, the brand and the category
#define SCREEN_INFO_HEAD    15
#define SCREEN_INFO_ROW_1   1
#define SCREEN_INFO_ROW_2   9
// 17, 24 ... 52: never a row that would make glcd_text() blank a page under a line drawn above
#define SCREEN_INFO_Y       17

static uint8_t wrap_room(void)
{
    uint8_t room = SCREEN_INFO_W / SCREEN_INFO_PITCH;

    return room > SCREEN_INFO_ROOM ? SCREEN_INFO_ROOM : room;
}

/* how much of `text` fits one line, and how many spaces to step over after it */
static uint8_t wrap_take(const char *text, uint8_t room, uint8_t *skip)
{
    uint8_t at = 0, take = 0, gap = 0;

    while (text[at] && at < room)
    {
        if (text[at] == ' ') take = at;
        at++;
    }

    // the tail fits whole, or the break lands exactly where the line ends
    if (text[at] == 0 || text[at] == ' ') take = at;
    else if (take == 0) take = at;          // one word longer than the line

    while (text[take + gap] == ' ') gap++;

    *skip = gap;
    return take;
}

uint8_t screen_plugin_info_lines(const char *text)
{
    uint8_t room = wrap_room();
    uint8_t lines = 0;

    if (!text) return 0;

    while (*text && lines < 255)
    {
        uint8_t skip;
        uint8_t take = wrap_take(text, room, &skip);

        lines++;
        if (take + skip == 0) break;        // nothing consumed: stop rather than spin
        text += take + skip;
    }

    return lines;
}

static void draw_description(glcd_t *display, const char *text, uint8_t first)
{
    uint8_t room = wrap_room();
    uint8_t y = SCREEN_INFO_Y;
    uint8_t line_no = 0;
    char line[SCREEN_INFO_ROOM + 1];

    if (!text) return;

    while (*text && line_no < (uint16_t)(first + SCREEN_INFO_LINES))
    {
        uint8_t skip;
        uint8_t take = wrap_take(text, room, &skip);

        if (line_no >= first)
        {
            memcpy(line, text, take);
            line[take] = 0;
            glcd_text(display, SCREEN_INFO_X, y, line, Terminal3x5, GLCD_BLACK);
            y += 7;
        }

        line_no++;
        if (take + skip == 0) break;
        text += take + skip;
    }
}

/* "A2/2 M1/0", and nothing at all for a kind of port the plugin does not have */
static void port_shape(char *out, const uint8_t *ports)
{
    static const char tags[3] = { 'A', 'M', 'C' };
    uint8_t at = 0, i;

    for (i = 0; i < 3; i++)
    {
        if (ports[i * 2] == 0 && ports[i * 2 + 1] == 0) continue;

        if (at) out[at++] = ' ';
        out[at++] = tags[i];
        out[at++] = (char)('0' + (ports[i * 2] % 10));
        out[at++] = '/';
        out[at++] = (char)('0' + (ports[i * 2 + 1] % 10));
    }

    out[at] = 0;
}

/* one header row: `right` against the right margin, `left` cut so it cannot reach it */
static void draw_header_row(glcd_t *display, uint8_t y, const char *left, const char *right)
{
    char cut[SCREEN_INFO_ROOM + 1];
    uint8_t width = (right && right[0]) ? glcd_text_width(Terminal3x5, right) : 0;
    uint8_t room, at = 0;

    if (width)
        glcd_text(display, (uint8_t)(DISPLAY_WIDTH - 3 - width), y, right, Terminal3x5, GLCD_BLACK);

    if (!left || !left[0]) return;

    room = (uint8_t)((DISPLAY_WIDTH - 6 - (width ? width + 4 : 0)) / SCREEN_INFO_PITCH);
    if (room > SCREEN_INFO_ROOM) room = SCREEN_INFO_ROOM;

    while (left[at] && at < room) { cut[at] = left[at]; at++; }
    cut[at] = 0;

    glcd_text(display, SCREEN_INFO_X, y, cut, Terminal3x5, GLCD_BLACK);
}

void screen_plugin_info(const plugin_info_t *model)
{
    glcd_t *display = hardware_glcds(DISPLAY_RIGHT);
    char shape[16];
    uint8_t total;

    glcd_clear(display, GLCD_WHITE);

    port_shape(shape, model->ports);
    draw_header_row(display, SCREEN_INFO_ROW_1, model->name, shape);
    draw_header_row(display, SCREEN_INFO_ROW_2, model->brand, model->category);
    glcd_rect_invert(display, 0, 0, DISPLAY_WIDTH, SCREEN_INFO_HEAD);

    draw_description(display, model->comment, model->first_line);

    // which way there is more of it, since the second encoder is what moves it
    total = screen_plugin_info_lines(model->comment);

    if (model->first_line > 0)
        glcd_text(display, SCREEN_INFO_ARROW, SCREEN_INFO_Y, "^", Terminal3x5, GLCD_BLACK);

    if (total > model->first_line + SCREEN_INFO_LINES)
        glcd_text(display, SCREEN_INFO_ARROW, SCREEN_INFO_Y + (SCREEN_INFO_LINES - 1) * 7, "v",
                  Terminal3x5, GLCD_BLACK);
}

/*
 * BUILDER: the bindings of one plugin
 *
 * What there is to bind on the left, the slots of the panel on the right under the page they
 * are on. A slot already taken on that page is marked; one armed for deletion flashes, with
 * DEL.
 */
void screen_bindings(const bindings_t *model)
{
    glcd_t *display = hardware_glcds(DISPLAY_LEFT);
    builder_chrome_t chrome = { .title = model->title, .labels = { "BACK", "BIND", NULL }, .blink = -1 };

    if (model->can_delete)
    {
        chrome.labels[2] = "DEL";
        if (model->armed && model->blink_reverse) chrome.blink = 2;
    }

    glcd_clear(display, GLCD_WHITE);

    // a parameter bound somewhere is marked the way a taken slot is
    {
        static char marked[BUILDER_BIND_MAX_PARAMS][24] BM_RAM;
        static char *rows[BUILDER_BIND_MAX_PARAMS] BM_RAM;
        uint8_t i, count = model->param_count;

        if (count > BUILDER_BIND_MAX_PARAMS) count = BUILDER_BIND_MAX_PARAMS;

        for (i = 0; i < count; i++)
        {
            strncpy(marked[i], model->params[i], sizeof(marked[i]) - 3);
            marked[i][sizeof(marked[i]) - 3] = 0;
            if (model->param_bound && model->param_bound[i]) strcat(marked[i], " *");
            rows[i] = marked[i];
        }

        print_builder_list(display, 2, 9, DISPLAY_WIDTH - 4, 42, rows, count, model->param_hover);
    }
    print_builder_title(display, chrome.title ? chrome.title : "BINDINGS");
    print_builder_buttons(display, &chrome);

    if (model->param_count == 0)
        print_centred_note(display, 27, "NOTHING TO BIND");

    display = hardware_glcds(DISPLAY_RIGHT);
    glcd_clear(display, GLCD_WHITE);
    print_builder_title(display, model->page ? model->page : "ACTUATORS");

    // a slot already holding something on this page is marked, the way the Dwarf marks it
    {
        static char marked[PLUGIN_LIST_MAX_ROWS][24] BM_RAM;
        static char *rows[PLUGIN_LIST_MAX_ROWS] BM_RAM;
        uint8_t i, count = model->actuator_count;

        if (count > PLUGIN_LIST_MAX_ROWS) count = PLUGIN_LIST_MAX_ROWS;

        for (i = 0; i < count; i++)
        {
            strncpy(marked[i], model->actuators[i], sizeof(marked[i]) - 3);
            marked[i][sizeof(marked[i]) - 3] = 0;
            if (model->actuator_taken && model->actuator_taken[i]) strcat(marked[i], " *");
            rows[i] = marked[i];
        }

        print_builder_list(display, 0, BUILDER_RIGHT_LIST_Y, DISPLAY_WIDTH, BUILDER_RIGHT_LIST_H,
                           rows, count, model->actuator_hover);
    }

    if (model->armed && !model->blink_reverse && model->actuator_count > 0)
        unblink_builder_row(display, 0, BUILDER_RIGHT_LIST_Y, DISPLAY_WIDTH, BUILDER_RIGHT_LIST_H,
                            model->actuator_count, model->actuator_hover);
}

/*
 * BUILDER: a two-line box over the right display while the host takes its time
 */
void screen_notice(const char *first, const char *second)
{
    glcd_t *display = hardware_glcds(DISPLAY_RIGHT);
    uint8_t height = second ? 25 : 17;
    uint8_t top = (DISPLAY_HEIGHT / 2) - (height / 2);

    glcd_rect_fill(display, 4, top, DISPLAY_WIDTH - 8, height, GLCD_WHITE);
    glcd_rect(display, 4, top, DISPLAY_WIDTH - 8, height, GLCD_BLACK);

    if (first) print_centred_note(display, top + 6, first);
    if (second) print_centred_note(display, top + 15, second);
}