/**
 * @file chat_core.h
 * @brief Deterministic, statically-allocated chat engine (Stage 1.1).
 *
 * The chat core owns the message history and the "virtual contact" that echoes
 * replies. It is fully decoupled from any UI: producers push local messages via
 * ::ChatCore_Send_Local and consumers (e.g. the LVGL UI) receive notifications
 * through a registered observer callback. No dynamic memory is used.
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-02
 * @license MIT
 *
 */

#ifndef CHAT_CORE_H
#define CHAT_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of bytes (including the terminating NUL) of a message text. */
#define CHAT_CORE_TEXT_CAPACITY      (128U)

/** Maximum number of messages retained in the rolling history buffer. */
#define CHAT_CORE_HISTORY_CAPACITY   (64U)

/**
 * @brief Identifies who authored a given message.
 */
typedef enum
{
    CHAT_AUTHOR_LOCAL   = 0, /**< The local device user. */
    CHAT_AUTHOR_CONTACT = 1  /**< The (virtual) remote contact. */
} chat_author_t;

/**
 * @brief A single immutable chat message.
 */
typedef struct
{
    uint32_t      sequence;                    /**< Monotonic id, unique per boot. */
    chat_author_t author;                      /**< Message originator. */
    uint32_t      timestamp_ms;                /**< Milliseconds since boot. */
    char          text[CHAT_CORE_TEXT_CAPACITY]; /**< NUL-terminated UTF-8 text. */
} chat_message_t;

/**
 * @brief Observer callback invoked whenever a new message is committed.
 *
 * @note Called from a producer task context (never from an ISR). The callee
 *       must not block for long and is responsible for its own thread safety
 *       (e.g. the UI observer takes the LVGL lock).
 *
 * @param[in] message   Pointer to a stable copy of the new message.
 * @param[in] user_ctx  Opaque context supplied at registration time.
 */
typedef void (*chat_message_observer_t)(const chat_message_t *message, void *user_ctx);

/**
 * @brief Initialize the chat core: static queues, mutex and the virtual contact task.
 *
 * Seeds a welcome message from the virtual contact. Safe to call exactly once
 * during system start-up (from app_main).
 *
 * @return ESP_OK on success, or an esp_err_t error code on failure.
 */
esp_err_t ChatCore_Init(void);

/**
 * @brief Register the single message observer (typically the UI).
 *
 * @param[in] observer  Callback to invoke on every committed message.
 * @param[in] user_ctx  Opaque pointer forwarded to the callback.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if @p observer is NULL,
 *         ESP_ERR_INVALID_STATE if the core is not initialized.
 */
esp_err_t ChatCore_Register_Observer(chat_message_observer_t observer, void *user_ctx);

/**
 * @brief Commit a local (user-authored) message and forward it to the contact.
 *
 * The message is stored, logged and dispatched to the virtual contact task,
 * which will asynchronously produce a reply.
 *
 * @param[in] text  NUL-terminated text. Empty or NULL text is rejected.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for invalid text,
 *         ESP_ERR_INVALID_STATE if not initialized, ESP_FAIL if the queue is full.
 */
esp_err_t ChatCore_Send_Local(const char *text);

/**
 * @brief Get the number of messages currently held in history.
 *
 * @return Message count in the range [0, CHAT_CORE_HISTORY_CAPACITY].
 */
uint32_t ChatCore_Get_History_Count(void);

/**
 * @brief Copy a message from history by chronological index (0 = oldest).
 *
 * @param[in]  index        Zero-based index into the current history window.
 * @param[out] out_message  Destination buffer to receive the message copy.
 *
 * @return true if a message was copied, false if @p index is out of range or
 *         @p out_message is NULL.
 */
bool ChatCore_Get_History_Item(uint32_t index, chat_message_t *out_message);

#ifdef __cplusplus
}
#endif

#endif /* CHAT_CORE_H */
