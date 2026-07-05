/**
 * @file chat_core.c
 * @brief Implementation of the statically-allocated chat engine (Stage 1.1).
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-02
 * @license MIT
 *
 */

#include "chat_core.h"

#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"

/* --- Tunable, non-magic configuration --------------------------------------- */
#define CHAT_CONTACT_TASK_STACK_WORDS   (4096U)
#define CHAT_CONTACT_TASK_PRIORITY      (4U)
#define CHAT_CONTACT_QUEUE_LENGTH       (8U)
#define CHAT_CONTACT_REPLY_DELAY_MS     (700U)
#define CHAT_MS_PER_SECOND              (1000U)

static const char *TAG = "chat_core";

/** Display name of the built-in virtual contact (ASCII only for LVGL fonts). */
static const char CHAT_CONTACT_NAME[] = "Virtual Contact";

/* --- Statically allocated RTOS objects -------------------------------------- */
static StaticSemaphore_t s_store_mutex_buffer;
static SemaphoreHandle_t s_store_mutex = NULL;

static StaticQueue_t s_contact_queue_struct;
static uint8_t       s_contact_queue_storage[CHAT_CONTACT_QUEUE_LENGTH * sizeof(chat_message_t)];
static QueueHandle_t s_contact_queue = NULL;

static StackType_t s_contact_task_stack[CHAT_CONTACT_TASK_STACK_WORDS];
static StaticTask_t s_contact_task_tcb;
static TaskHandle_t s_contact_task = NULL;

/* --- Message history (rolling ring buffer, no dynamic memory) --------------- */
static chat_message_t s_history[CHAT_CORE_HISTORY_CAPACITY];
static uint32_t       s_history_head  = 0U;  /**< Index of the oldest message. */
static uint32_t       s_history_count = 0U;  /**< Number of valid messages.    */
static uint32_t       s_next_sequence = 0U;  /**< Next message sequence id.    */

/* --- Single observer -------------------------------------------------------- */
static chat_message_observer_t s_observer      = NULL;
static void                   *s_observer_ctx  = NULL;

static bool s_initialized = false;

/**
 * @brief Append a message to history and notify the observer.
 *
 * @param[in] author  Message author.
 * @param[in] text    NUL-terminated source text (already validated).
 */
static void chat_commit_message(chat_author_t author, const char *text)
{
    chat_message_t snapshot;

    /* Critical section: mutate the shared ring buffer under the mutex only. */
    (void)xSemaphoreTake(s_store_mutex, portMAX_DELAY);
    {
        uint32_t slot;

        if (s_history_count < CHAT_CORE_HISTORY_CAPACITY)
        {
            slot = (s_history_head + s_history_count) % CHAT_CORE_HISTORY_CAPACITY;
            s_history_count++;
        }
        else
        {
            /* Buffer full: overwrite the oldest entry and advance the head. */
            slot = s_history_head;
            s_history_head = (s_history_head + 1U) % CHAT_CORE_HISTORY_CAPACITY;
        }

        s_history[slot].sequence     = s_next_sequence;
        s_history[slot].author       = author;
        s_history[slot].timestamp_ms = (uint32_t)(esp_timer_get_time() / (int64_t)CHAT_MS_PER_SECOND);
        (void)strncpy(s_history[slot].text, text, CHAT_CORE_TEXT_CAPACITY - 1U);
        s_history[slot].text[CHAT_CORE_TEXT_CAPACITY - 1U] = '\0';

        s_next_sequence++;

        /* Take a stable copy so the observer runs outside the critical section. */
        snapshot = s_history[slot];
    }
    (void)xSemaphoreGive(s_store_mutex);

    ESP_LOGI(TAG, "[#%u] %s: %s",
             (unsigned int)snapshot.sequence,
             (snapshot.author == CHAT_AUTHOR_LOCAL) ? "me" : CHAT_CONTACT_NAME,
             snapshot.text);

    if (s_observer != NULL)
    {
        s_observer(&snapshot, s_observer_ctx);
    }
}

/**
 * @brief Virtual contact task: consumes local messages and echoes replies.
 *
 * @param[in] arg  Unused FreeRTOS task argument.
 */
static void chat_contact_task(void *arg)
{
    chat_message_t incoming;
    /* Oversized so the "Echo: " prefix + full incoming text can never truncate
     * here; the stored copy is bounded to CHAT_CORE_TEXT_CAPACITY on commit. */
    char           reply[CHAT_CORE_TEXT_CAPACITY + sizeof("Echo: ")];

    (void)arg;

    for (;;)
    {
        if (xQueueReceive(s_contact_queue, &incoming, portMAX_DELAY) == pdTRUE)
        {
            /* Simulated over-the-air latency of the future LORA link. This is a
             * cosmetic delay, not a hard real-time deadline, so vTaskDelay is OK. */
            vTaskDelay(pdMS_TO_TICKS(CHAT_CONTACT_REPLY_DELAY_MS));

            (void)snprintf(reply, sizeof(reply), "Echo: %s", incoming.text);
            chat_commit_message(CHAT_AUTHOR_CONTACT, reply);
        }
    }
}

esp_err_t ChatCore_Init(void)
{
    esp_err_t status = ESP_OK;

    if (s_initialized)
    {
        status = ESP_ERR_INVALID_STATE;
    }
    else
    {
        s_store_mutex = xSemaphoreCreateMutexStatic(&s_store_mutex_buffer);

        s_contact_queue = xQueueCreateStatic(CHAT_CONTACT_QUEUE_LENGTH,
                                             sizeof(chat_message_t),
                                             s_contact_queue_storage,
                                             &s_contact_queue_struct);

        if ((s_store_mutex == NULL) || (s_contact_queue == NULL))
        {
            status = ESP_ERR_NO_MEM;
        }
        else
        {
            s_contact_task = xTaskCreateStatic(chat_contact_task,
                                               "chat_contact",
                                               CHAT_CONTACT_TASK_STACK_WORDS,
                                               NULL,
                                               CHAT_CONTACT_TASK_PRIORITY,
                                               s_contact_task_stack,
                                               &s_contact_task_tcb);

            if (s_contact_task == NULL)
            {
                status = ESP_ERR_NO_MEM;
            }
            else
            {
                s_initialized = true;
                chat_commit_message(CHAT_AUTHOR_CONTACT,
                                    "Hi! I'm your virtual contact. Type a message and I'll echo it back.");
                ESP_LOGI(TAG, "chat core initialized");
            }
        }
    }

    return status;
}

esp_err_t ChatCore_Register_Observer(chat_message_observer_t observer, void *user_ctx)
{
    esp_err_t status = ESP_OK;

    if (!s_initialized)
    {
        status = ESP_ERR_INVALID_STATE;
    }
    else if (observer == NULL)
    {
        status = ESP_ERR_INVALID_ARG;
    }
    else
    {
        (void)xSemaphoreTake(s_store_mutex, portMAX_DELAY);
        s_observer     = observer;
        s_observer_ctx = user_ctx;
        (void)xSemaphoreGive(s_store_mutex);
    }

    return status;
}

esp_err_t ChatCore_Send_Local(const char *text)
{
    esp_err_t status = ESP_OK;

    if (!s_initialized)
    {
        status = ESP_ERR_INVALID_STATE;
    }
    else if ((text == NULL) || (text[0] == '\0'))
    {
        status = ESP_ERR_INVALID_ARG;
    }
    else
    {
        chat_message_t outgoing;

        chat_commit_message(CHAT_AUTHOR_LOCAL, text);

        /* Re-read the last committed local message so the contact task echoes the
         * stored (truncated) form rather than the raw, possibly longer, input. */
        (void)memset(&outgoing, 0, sizeof(outgoing));
        outgoing.author = CHAT_AUTHOR_LOCAL;
        (void)strncpy(outgoing.text, text, CHAT_CORE_TEXT_CAPACITY - 1U);
        outgoing.text[CHAT_CORE_TEXT_CAPACITY - 1U] = '\0';

        if (xQueueSend(s_contact_queue, &outgoing, 0) != pdTRUE)
        {
            ESP_LOGW(TAG, "contact queue full, reply dropped");
            status = ESP_FAIL;
        }
    }

    return status;
}

uint32_t ChatCore_Get_History_Count(void)
{
    uint32_t count = 0U;

    if (s_initialized)
    {
        (void)xSemaphoreTake(s_store_mutex, portMAX_DELAY);
        count = s_history_count;
        (void)xSemaphoreGive(s_store_mutex);
    }

    return count;
}

bool ChatCore_Get_History_Item(uint32_t index, chat_message_t *out_message)
{
    bool copied = false;

    if (s_initialized && (out_message != NULL))
    {
        (void)xSemaphoreTake(s_store_mutex, portMAX_DELAY);
        if (index < s_history_count)
        {
            uint32_t slot = (s_history_head + index) % CHAT_CORE_HISTORY_CAPACITY;
            *out_message = s_history[slot];
            copied = true;
        }
        (void)xSemaphoreGive(s_store_mutex);
    }

    return copied;
}
