/**
 * @file chat_ui.c
 * @brief LVGL chat user interface implementation (Stage 1.1).
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-02
 * @license MIT
 *
 */

#include "chat_ui.h"
#include "chat_core.h"

#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"

static const char *TAG = "chat_ui";

/* --- Layout geometry (non-magic) -------------------------------------------- */
#define CHAT_UI_HEADER_HEIGHT_PX    (44)
#define CHAT_UI_INPUT_HEIGHT_PX     (52)
#define CHAT_UI_KEYBOARD_HEIGHT_PX  (280)
#define CHAT_UI_BUBBLE_WIDTH_PCT    (76)
#define CHAT_UI_GAP_PX              (6)
#define CHAT_UI_PAD_PX              (8)
#define CHAT_UI_BUBBLE_RADIUS_PX    (10)

/* --- Palette ---------------------------------------------------------------- */
#define CHAT_UI_COLOR_SCREEN_BG     (0xF3F4F6)
#define CHAT_UI_COLOR_HEADER_BG     (0x111827)
#define CHAT_UI_COLOR_HEADER_TEXT   (0xFFFFFF)
#define CHAT_UI_COLOR_LOCAL_BG      (0x2563EB)
#define CHAT_UI_COLOR_LOCAL_TEXT    (0xFFFFFF)
#define CHAT_UI_COLOR_CONTACT_BG    (0xE5E7EB)
#define CHAT_UI_COLOR_CONTACT_TEXT  (0x111827)

/* --- Persistent widget handles ---------------------------------------------- */
static lv_obj_t *s_msg_area = NULL;
static lv_obj_t *s_input    = NULL;

/**
 * @brief Append one message bubble to the scrollable message area.
 *
 * @param[in] message  Message to render (must not be NULL).
 */
static void chat_ui_add_bubble(const chat_message_t *message)
{
    bool     is_local   = (message->author == CHAT_AUTHOR_LOCAL);
    uint32_t bubble_bg   = is_local ? CHAT_UI_COLOR_LOCAL_BG   : CHAT_UI_COLOR_CONTACT_BG;
    uint32_t bubble_text = is_local ? CHAT_UI_COLOR_LOCAL_TEXT : CHAT_UI_COLOR_CONTACT_TEXT;

    /* Full-width transparent row used only to left/right align the bubble. */
    lv_obj_t *row = lv_obj_create(s_msg_area);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row,
                          is_local ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    lv_obj_t *bubble = lv_obj_create(row);
    lv_obj_set_width(bubble, lv_pct(CHAT_UI_BUBBLE_WIDTH_PCT));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(bubble, CHAT_UI_PAD_PX, 0);
    lv_obj_set_style_radius(bubble, CHAT_UI_BUBBLE_RADIUS_PX, 0);
    lv_obj_set_style_border_width(bubble, 0, 0);
    lv_obj_set_style_bg_color(bubble, lv_color_hex(bubble_bg), 0);

    lv_obj_t *label = lv_label_create(bubble);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_color(label, lv_color_hex(bubble_text), 0);
    lv_label_set_text(label, message->text);

    /* Keep the newest message visible. */
    lv_obj_scroll_to_view_recursive(row, LV_ANIM_ON);
}

/**
 * @brief Chat core observer: render newly committed messages.
 *
 * @param[in] message  New message.
 * @param[in] user_ctx Unused.
 */
static void chat_ui_on_message(const chat_message_t *message, void *user_ctx)
{
    (void)user_ctx;

    /* Marshal onto the LVGL thread; the port lock is recursive so this is safe
     * even when invoked from the LVGL context (e.g. via the send button). */
    (void)lvgl_port_lock(0);
    chat_ui_add_bubble(message);
    lvgl_port_unlock();
}

/**
 * @brief Read the input field and submit it to the chat core.
 */
static void chat_ui_submit_input(void)
{
    const char *text = lv_textarea_get_text(s_input);

    if ((text != NULL) && (text[0] != '\0'))
    {
        (void)ChatCore_Send_Local(text);
        lv_textarea_set_text(s_input, "");
    }
}

/**
 * @brief Send button / keyboard "ready" event handler.
 *
 * @param[in] event  LVGL event descriptor (unused payload).
 */
static void chat_ui_send_event_cb(lv_event_t *event)
{
    (void)event;
    chat_ui_submit_input();
}

/**
 * @brief Construct all static widgets of the chat screen.
 */
static void chat_ui_build(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(CHAT_UI_COLOR_SCREEN_BG), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    /* Header bar. */
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, CHAT_UI_HEADER_HEIGHT_PX);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(CHAT_UI_COLOR_HEADER_BG), 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "ESPDeka  |  Virtual Contact");
    lv_obj_set_style_text_color(title, lv_color_hex(CHAT_UI_COLOR_HEADER_TEXT), 0);

    /* Scrollable message history (fills remaining vertical space). */
    s_msg_area = lv_obj_create(screen);
    lv_obj_set_width(s_msg_area, lv_pct(100));
    lv_obj_set_flex_grow(s_msg_area, 1);
    lv_obj_set_style_bg_opa(s_msg_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_msg_area, 0, 0);
    lv_obj_set_style_pad_all(s_msg_area, CHAT_UI_PAD_PX, 0);
    lv_obj_set_style_pad_row(s_msg_area, CHAT_UI_GAP_PX, 0);
    lv_obj_set_flex_flow(s_msg_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_msg_area, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    /* Input row: text field + send button. */
    lv_obj_t *input_row = lv_obj_create(screen);
    lv_obj_remove_style_all(input_row);
    lv_obj_set_width(input_row, lv_pct(100));
    lv_obj_set_height(input_row, CHAT_UI_INPUT_HEIGHT_PX);
    lv_obj_set_style_pad_all(input_row, CHAT_UI_PAD_PX / 2, 0);
    lv_obj_set_style_pad_column(input_row, CHAT_UI_GAP_PX, 0);
    lv_obj_set_flex_flow(input_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(input_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_input = lv_textarea_create(input_row);
    lv_obj_set_flex_grow(s_input, 1);
    lv_obj_set_height(s_input, lv_pct(100));
    lv_textarea_set_one_line(s_input, true);
    lv_textarea_set_placeholder_text(s_input, "Type a message");
    lv_obj_add_event_cb(s_input, chat_ui_send_event_cb, LV_EVENT_READY, NULL);

    lv_obj_t *send_btn = lv_button_create(input_row);
    lv_obj_set_height(send_btn, lv_pct(100));
    lv_obj_add_event_cb(send_btn, chat_ui_send_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *send_lbl = lv_label_create(send_btn);
    lv_label_set_text(send_lbl, LV_SYMBOL_OK " Send");
    lv_obj_center(send_lbl);

    /* On-screen keyboard bound to the text field. */
    lv_obj_t *keyboard = lv_keyboard_create(screen);
    lv_obj_set_width(keyboard, lv_pct(100));
    lv_obj_set_height(keyboard, CHAT_UI_KEYBOARD_HEIGHT_PX);
    lv_keyboard_set_textarea(keyboard, s_input);
}

esp_err_t ChatUi_Init(void)
{
    esp_err_t status;
    uint32_t  count;
    uint32_t  index;

    (void)lvgl_port_lock(0);
    chat_ui_build();

    /* Render the history that already exists (e.g. the welcome message) before
     * subscribing, so live updates never duplicate pre-existing entries. */
    count = ChatCore_Get_History_Count();
    for (index = 0U; index < count; index++)
    {
        chat_message_t message;
        if (ChatCore_Get_History_Item(index, &message))
        {
            chat_ui_add_bubble(&message);
        }
    }
    lvgl_port_unlock();

    status = ChatCore_Register_Observer(chat_ui_on_message, NULL);
    ESP_LOGI(TAG, "chat UI initialized (history=%u, status=%d)", (unsigned int)count, (int)status);

    return status;
}
