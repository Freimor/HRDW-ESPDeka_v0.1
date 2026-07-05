/**
 * @file host_uart.c
 * @brief Host UART driver for the LORA coprocessor module.
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-03
 * @license MIT
 *
 */

#include "host_uart.h"

#include <string.h>

#include "board_pins.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "sdkconfig.h"

#define HOST_UART_RX_BUF_SIZE           (1024U)
#define HOST_UART_TX_BUF_SIZE           (256U)
#define HOST_UART_TX_TIMEOUT_MS         (200U)
#define HOST_UART_RX_TASK_STACK_WORDS   (3072U)
#define HOST_UART_RX_TASK_PRIORITY      (5U)
#define HOST_UART_READ_CHUNK            (64U)
#define HOST_UART_READ_TIMEOUT_MS       (50U)
#define HOST_UART_LINE_QUEUE_DEPTH      (4U)

static const char *TAG = "host_uart";

#define HOST_UART_DISPATCH_STACK_WORDS  (3072U)

static uart_port_t           s_uart_port   = UART_NUM_0;
static int                   s_tx_pin      = (int)BOARD_HOST_UART_TX_PIN;
static int                   s_rx_pin      = (int)BOARD_HOST_UART_RX_PIN;
static host_uart_line_cb_t   s_line_cb     = NULL;
static void                 *s_line_ctx    = NULL;
static QueueHandle_t         s_line_queue  = NULL;
static StaticQueue_t         s_line_queue_struct;
static uint8_t               s_line_queue_storage[HOST_UART_LINE_QUEUE_DEPTH * HOST_UART_LINE_CAPACITY];
static StackType_t           s_rx_task_stack[HOST_UART_RX_TASK_STACK_WORDS];
static StaticTask_t          s_rx_task_tcb;
static StackType_t           s_dispatch_task_stack[HOST_UART_DISPATCH_STACK_WORDS];
static StaticTask_t          s_dispatch_task_tcb;
static bool                  s_initialized = false;

/**
 * @brief Resolve UART port and pins from sdkconfig.
 */
static void host_uart_resolve_config(void)
{
#if CONFIG_ESPDEKA_HOST_UART_PORT_1
    s_uart_port = BOARD_HOST_UART1_PORT;
    s_tx_pin    = (int)BOARD_HOST_UART1_TX_PIN;
    s_rx_pin    = (int)BOARD_HOST_UART1_RX_PIN;
#else
    s_uart_port = BOARD_HOST_UART_PORT;
    s_tx_pin    = (int)BOARD_HOST_UART_TX_PIN;
    s_rx_pin    = (int)BOARD_HOST_UART_RX_PIN;
#endif
}

/**
 * @brief Push a completed line into the dispatch queue.
 */
static void host_uart_enqueue_line(const char *line)
{
    char slot[HOST_UART_LINE_CAPACITY];

    if ((line == NULL) || (s_line_queue == NULL))
    {
        /* No-op. */
    }
    else
    {
        (void)strncpy(slot, line, HOST_UART_LINE_CAPACITY - 1U);
        slot[HOST_UART_LINE_CAPACITY - 1U] = '\0';

        if (xQueueSend(s_line_queue, slot, 0) != pdTRUE)
        {
            ESP_LOGW(TAG, "line queue full, dropping: %s", slot);
        }
    }
}

/**
 * @brief Dispatch queued lines on the calling task context.
 */
static void host_uart_dispatch_task(void *arg)
{
    char line[HOST_UART_LINE_CAPACITY];

    (void)arg;

    for (;;)
    {
        if (xQueueReceive(s_line_queue, line, portMAX_DELAY) == pdTRUE)
        {
            if (s_line_cb != NULL)
            {
                s_line_cb(line, s_line_ctx);
            }
        }
    }
}

/**
 * @brief Accumulate UART bytes into lines and enqueue them.
 */
static void host_uart_rx_task(void *arg)
{
    uint8_t chunk[HOST_UART_READ_CHUNK];
    char    line[HOST_UART_LINE_CAPACITY];
    size_t  line_len = 0U;

    (void)arg;

    for (;;)
    {
        const int nbytes = uart_read_bytes(s_uart_port,
                                           chunk,
                                           sizeof(chunk),
                                           pdMS_TO_TICKS(HOST_UART_READ_TIMEOUT_MS));
        if (nbytes > 0)
        {
            int idx;

            for (idx = 0; idx < nbytes; idx++)
            {
                const char ch = (char)chunk[(size_t)idx];

                if ((ch == '\n') || (ch == '\r')
                    || (line_len >= (HOST_UART_LINE_CAPACITY - 1U)))
                {
                    if (line_len > 0U)
                    {
                        line[line_len] = '\0';
                        host_uart_enqueue_line(line);
                        line_len = 0U;
                    }
                }
                else
                {
                    line[line_len] = ch;
                    line_len++;
                }
            }
        }
    }
}

esp_err_t HostUart_Init(void)
{
    esp_err_t status = ESP_OK;

    if (s_initialized)
    {
        status = ESP_ERR_INVALID_STATE;
    }
    else
    {
        host_uart_resolve_config();

        const uart_config_t uart_cfg =
        {
            .baud_rate           = (int)BOARD_HOST_UART_BAUD,
            .data_bits           = UART_DATA_8_BITS,
            .parity              = UART_PARITY_DISABLE,
            .stop_bits           = UART_STOP_BITS_1,
            .flow_ctrl           = UART_HW_FLOWCTRL_DISABLE,
            .source_clk          = UART_SCLK_DEFAULT,
        };

        status = uart_driver_install(s_uart_port,
                                     (int)HOST_UART_RX_BUF_SIZE,
                                     (int)HOST_UART_TX_BUF_SIZE,
                                     0U,
                                     NULL,
                                     0U);

        if (status == ESP_OK)
        {
            status = uart_param_config(s_uart_port, &uart_cfg);
        }

        if (status == ESP_OK)
        {
            status = uart_set_pin(s_uart_port,
                                  s_tx_pin,
                                  s_rx_pin,
                                  UART_PIN_NO_CHANGE,
                                  UART_PIN_NO_CHANGE);
        }

        if (status == ESP_OK)
        {
            s_initialized = true;
            ESP_LOGI(TAG, "host UART%d ready (TX=%d RX=%d @ %u baud)",
                     (int)s_uart_port,
                     s_tx_pin,
                     s_rx_pin,
                     (unsigned int)BOARD_HOST_UART_BAUD);
        }
        else
        {
            (void)uart_driver_delete(s_uart_port);
        }
    }

    return status;
}

esp_err_t HostUart_Start(host_uart_line_cb_t callback, void *user_ctx)
{
    esp_err_t status = ESP_OK;
    TaskHandle_t rx_task = NULL;
    TaskHandle_t dispatch_task = NULL;

    if (!s_initialized)
    {
        status = ESP_ERR_INVALID_STATE;
    }
    else if (callback == NULL)
    {
        status = ESP_ERR_INVALID_ARG;
    }
    else if (s_line_queue != NULL)
    {
        status = ESP_ERR_INVALID_STATE;
    }
    else
    {
        s_line_cb  = callback;
        s_line_ctx = user_ctx;

        s_line_queue = xQueueCreateStatic(HOST_UART_LINE_QUEUE_DEPTH,
                                          HOST_UART_LINE_CAPACITY,
                                          s_line_queue_storage,
                                          &s_line_queue_struct);

        if (s_line_queue == NULL)
        {
            status = ESP_ERR_NO_MEM;
        }
        else
        {
            rx_task = xTaskCreateStatic(host_uart_rx_task,
                                        "host_uart_rx",
                                        HOST_UART_RX_TASK_STACK_WORDS,
                                        NULL,
                                        HOST_UART_RX_TASK_PRIORITY,
                                        s_rx_task_stack,
                                        &s_rx_task_tcb);

            dispatch_task = xTaskCreateStatic(host_uart_dispatch_task,
                                                "host_uart_dispatch",
                                                HOST_UART_DISPATCH_STACK_WORDS,
                                                NULL,
                                                HOST_UART_RX_TASK_PRIORITY - 1U,
                                                s_dispatch_task_stack,
                                                &s_dispatch_task_tcb);

            if ((rx_task == NULL) || (dispatch_task == NULL))
            {
                status = ESP_ERR_NO_MEM;
            }
        }
    }

    return status;
}

esp_err_t HostUart_Write(const char *text)
{
    esp_err_t status = ESP_OK;

    if (!s_initialized)
    {
        status = ESP_ERR_INVALID_STATE;
    }
    else if (text == NULL)
    {
        status = ESP_ERR_INVALID_ARG;
    }
    else
    {
        const size_t length = strlen(text);
        const int    written = uart_write_bytes(s_uart_port, text, length);

        if (written < 0)
        {
            status = ESP_FAIL;
        }
        else
        {
            status = uart_wait_tx_done(s_uart_port, pdMS_TO_TICKS(HOST_UART_TX_TIMEOUT_MS));
        }
    }

    return status;
}
