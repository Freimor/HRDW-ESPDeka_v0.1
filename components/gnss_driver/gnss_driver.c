/**
 * @file gnss_driver.c
 * @brief RUSHFPV M10 PRO GNSS driver (NMEA UART + magnetometer I2C).
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-03
 * @license MIT
 *
 */

#include "gnss_driver.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "expand_io_hal.h"
#include "expand_io_pins.h"

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

/* --- Configuration ---------------------------------------------------------- */
#define GNSS_UART_RX_BUF_SIZE           (1024U)
#define GNSS_UART_TX_BUF_SIZE           (256U)
#define GNSS_RX_TASK_STACK_WORDS        (4096U)
#define GNSS_RX_TASK_PRIORITY           (5U)
#define GNSS_NMEA_LINE_CAPACITY         (128U)
#define GNSS_UART_READ_CHUNK            (64U)
#define GNSS_UART_READ_TIMEOUT_MS       (50U)
#define GNSS_I2C_TIMEOUT_MS             (100U)
#define GNSS_COMPASS_PROBE_INTERVAL_MS  (1000U)
#define GNSS_DEGREES_PER_SEMICIRCLE     (180.0f)
#define GNSS_HEADING_FULL_CIRCLE        (360.0f)
#define GNSS_NMEA_FIELD_MIN_LEN         (2U)
#define GNSS_LAT_LON_FIELD_LEN          (2U)

/* HMC5883 magnetometer (advertised on RUSHFPV M10 PRO). */
#define GNSS_HMC5883_I2C_ADDR           (0x1EU)
#define GNSS_HMC5883_REG_CRA             (0x00U)
#define GNSS_HMC5883_REG_CRB             (0x01U)
#define GNSS_HMC5883_REG_MODE            (0x02U)
#define GNSS_HMC5883_REG_DATA            (0x03U)
#define GNSS_HMC5883_CRA_VALUE           (0x70U)
#define GNSS_HMC5883_CRB_VALUE           (0x20U)
#define GNSS_HMC5883_MODE_CONTINUOUS     (0x00U)

/* QMC5883L (common substitute on clone modules). */
#define GNSS_QMC5883L_I2C_ADDR          (0x0DU)
#define GNSS_QMC5883L_REG_DATA           (0x00U)
#define GNSS_QMC5883L_REG_CONTROL1       (0x09U)
#define GNSS_QMC5883L_CTRL1_VALUE        (0x1DU)

static const char *TAG = "gnss_driver";

typedef enum
{
    GNSS_COMPASS_NONE = 0,
    GNSS_COMPASS_HMC5883,
    GNSS_COMPASS_QMC5883L
} gnss_compass_type_t;

static gnss_snapshot_t       s_snapshot;
static StaticSemaphore_t     s_snapshot_mutex_buffer;
static SemaphoreHandle_t     s_snapshot_mutex = NULL;
static StackType_t           s_rx_task_stack[GNSS_RX_TASK_STACK_WORDS];
static StaticTask_t          s_rx_task_tcb;
static TaskHandle_t          s_rx_task        = NULL;
static i2c_master_dev_handle_t s_compass_dev  = NULL;
static gnss_compass_type_t   s_compass_type   = GNSS_COMPASS_NONE;
static bool                  s_initialized    = false;

/**
 * @brief Convert an NMEA latitude/longitude field (ddmm.mmmm) to decimal degrees.
 */
static double gnss_nmea_to_degrees(const char *field, char hemi)
{
    double value    = 0.0;
    double degrees  = 0.0;
    double minutes  = 0.0;
    char   deg_buf[GNSS_LAT_LON_FIELD_LEN + 1U];

    if ((field == NULL) || (field[0] == '\0'))
    {
        value = 0.0;
    }
    else
    {
        (void)memset(deg_buf, 0, sizeof(deg_buf));
        (void)strncpy(deg_buf, field, GNSS_LAT_LON_FIELD_LEN);
        degrees = strtod(deg_buf, NULL);
        minutes = strtod(field + GNSS_LAT_LON_FIELD_LEN, NULL);
        value   = degrees + (minutes / 60.0);

        if ((hemi == 'S') || (hemi == 'W'))
        {
            value = -value;
        }
    }

    return value;
}

/**
 * @brief Return a pointer to the @p index-th comma-separated NMEA field.
 */
static const char *gnss_nmea_field(const char *sentence, uint32_t index)
{
    const char *cursor = sentence;
    uint32_t    current = 0U;

    if (sentence == NULL)
    {
        cursor = NULL;
    }
    else
    {
        while ((current < index) && (cursor != NULL) && (*cursor != '\0'))
        {
            cursor = strchr(cursor, ',');
            if (cursor != NULL)
            {
                cursor++;
                current++;
            }
        }
    }

    return cursor;
}

/**
 * @brief Copy one NMEA field (stops at comma or asterisk) into @p out_buf.
 */
static void gnss_copy_field(const char *field, char *out_buf, size_t out_capacity)
{
    size_t i = 0U;

    if ((field == NULL) || (out_buf == NULL) || (out_capacity == 0U))
    {
        /* No-op: invalid arguments. */
    }
    else
    {
        while ((field[i] != '\0') && (field[i] != ',') && (field[i] != '*') && (i < (out_capacity - 1U)))
        {
            out_buf[i] = field[i];
            i++;
        }
        out_buf[i] = '\0';
    }
}

/**
 * @brief Parse a GGA sentence and update the shared snapshot.
 */
static void gnss_parse_gga(const char *sentence)
{
    char field_buf[24];
    const char *field;
    uint8_t fix_quality = 0U;
    char lat_hemi = 'N';
    char lon_hemi = 'E';

    field = gnss_nmea_field(sentence, 6U);
    gnss_copy_field(field, field_buf, sizeof(field_buf));
    fix_quality = (uint8_t)strtoul(field_buf, NULL, 10);

    field = gnss_nmea_field(sentence, 7U);
    gnss_copy_field(field, field_buf, sizeof(field_buf));
    s_snapshot.satellites = (uint8_t)strtoul(field_buf, NULL, 10);

    field = gnss_nmea_field(sentence, 9U);
    gnss_copy_field(field, field_buf, sizeof(field_buf));
    s_snapshot.altitude_m = (float)strtod(field_buf, NULL);

    if (fix_quality == 0U)
    {
        s_snapshot.fix = GNSS_FIX_NONE;
    }
    else if (fix_quality == 1U)
    {
        s_snapshot.fix = GNSS_FIX_2D;
        s_snapshot.data_valid = true;
    }
    else
    {
        s_snapshot.fix = GNSS_FIX_3D;
        s_snapshot.data_valid = true;
    }

    field = gnss_nmea_field(sentence, 2U);
    if ((field != NULL) && (field[0] != '\0'))
    {
        field = gnss_nmea_field(sentence, 3U);
        if ((field != NULL) && (field[0] != '\0'))
        {
            lat_hemi = field[0];
        }

        field = gnss_nmea_field(sentence, 5U);
        if ((field != NULL) && (field[0] != '\0'))
        {
            lon_hemi = field[0];
        }

        s_snapshot.latitude_deg  = gnss_nmea_to_degrees(gnss_nmea_field(sentence, 2U), lat_hemi);
        s_snapshot.longitude_deg = gnss_nmea_to_degrees(gnss_nmea_field(sentence, 4U), lon_hemi);
    }
}

/**
 * @brief Parse an RMC sentence and update speed/course fields.
 */
static void gnss_parse_rmc(const char *sentence)
{
    char field_buf[24];
    const char *status_field;

    status_field = gnss_nmea_field(sentence, 2U);
    if ((status_field != NULL) && (status_field[0] == 'A'))
    {
        gnss_copy_field(gnss_nmea_field(sentence, 7U), field_buf, sizeof(field_buf));
        s_snapshot.speed_mps = (float)(strtod(field_buf, NULL) * 0.514444);

        gnss_copy_field(gnss_nmea_field(sentence, 8U), field_buf, sizeof(field_buf));
        s_snapshot.course_deg = (float)strtod(field_buf, NULL);
        s_snapshot.data_valid = true;

        s_snapshot.latitude_deg = gnss_nmea_to_degrees(gnss_nmea_field(sentence, 3U),
                                                       gnss_nmea_field(sentence, 4U)[0]);
        s_snapshot.longitude_deg = gnss_nmea_to_degrees(gnss_nmea_field(sentence, 5U),
                                                        gnss_nmea_field(sentence, 6U)[0]);
    }
}

/**
 * @brief Dispatch a complete NMEA sentence to the appropriate parser.
 */
static void gnss_handle_sentence(const char *sentence)
{
    if ((sentence == NULL) || (sentence[0] != '$'))
    {
        /* Ignore non-NMEA lines. */
    }
    else if ((strstr(sentence, "GGA") != NULL) || (strstr(sentence, "GNGGA") != NULL))
    {
        gnss_parse_gga(sentence);
    }
    else if ((strstr(sentence, "RMC") != NULL) || (strstr(sentence, "GNRMC") != NULL))
    {
        gnss_parse_rmc(sentence);
    }
    else
    {
        /* Other talkers are ignored in stage 1.2. */
    }
}

/**
 * @brief Refresh the short status string from current snapshot fields.
 */
static void gnss_refresh_status_string(void)
{
    if (!s_snapshot.data_valid)
    {
        (void)snprintf(s_snapshot.status, sizeof(s_snapshot.status), "no fix");
    }
    else
    {
        const char *fix_label = "3D";

        if (s_snapshot.fix == GNSS_FIX_2D)
        {
            fix_label = "2D";
        }

        if (s_snapshot.compass_valid)
        {
            (void)snprintf(s_snapshot.status, sizeof(s_snapshot.status),
                           "%s lat=%.5f lon=%.5f alt=%.0fm sats=%u hdg=%.0f",
                           fix_label,
                           s_snapshot.latitude_deg,
                           s_snapshot.longitude_deg,
                           (double)s_snapshot.altitude_m,
                           (unsigned int)s_snapshot.satellites,
                           (double)s_snapshot.heading_deg);
        }
        else
        {
            (void)snprintf(s_snapshot.status, sizeof(s_snapshot.status),
                           "%s lat=%.5f lon=%.5f alt=%.0fm sats=%u",
                           fix_label,
                           s_snapshot.latitude_deg,
                           s_snapshot.longitude_deg,
                           (double)s_snapshot.altitude_m,
                           (unsigned int)s_snapshot.satellites);
        }
    }
}

/**
 * @brief Probe and add an I2C compass device on the shared bus.
 */
static esp_err_t gnss_compass_probe(uint8_t i2c_addr, gnss_compass_type_t type)
{
    esp_err_t status = ESP_OK;
    i2c_device_config_t dev_cfg =
    {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = i2c_addr,
        .scl_speed_hz    = 100000,
    };

    status = i2c_master_bus_add_device(ExpandIo_I2c_Get_Bus(), &dev_cfg, &s_compass_dev);

    if (status == ESP_OK)
    {
        s_compass_type = type;
        ESP_LOGI(TAG, "compass detected at 0x%02X (%s)",
                 (unsigned int)i2c_addr,
                 (type == GNSS_COMPASS_HMC5883) ? "HMC5883" : "QMC5883L");
    }

    return status;
}

/**
 * @brief Configure the detected magnetometer for continuous sampling.
 */
static esp_err_t gnss_compass_configure(void)
{
    esp_err_t status = ESP_OK;
    uint8_t   tx_buf[2];

    if (s_compass_type == GNSS_COMPASS_HMC5883)
    {
        tx_buf[0] = GNSS_HMC5883_REG_CRA;
        tx_buf[1] = GNSS_HMC5883_CRA_VALUE;
        status = i2c_master_transmit(s_compass_dev, tx_buf, 2U, GNSS_I2C_TIMEOUT_MS);

        if (status == ESP_OK)
        {
            tx_buf[0] = GNSS_HMC5883_REG_CRB;
            tx_buf[1] = GNSS_HMC5883_CRB_VALUE;
            status = i2c_master_transmit(s_compass_dev, tx_buf, 2U, GNSS_I2C_TIMEOUT_MS);
        }

        if (status == ESP_OK)
        {
            tx_buf[0] = GNSS_HMC5883_REG_MODE;
            tx_buf[1] = GNSS_HMC5883_MODE_CONTINUOUS;
            status = i2c_master_transmit(s_compass_dev, tx_buf, 2U, GNSS_I2C_TIMEOUT_MS);
        }
    }
    else if (s_compass_type == GNSS_COMPASS_QMC5883L)
    {
        tx_buf[0] = GNSS_QMC5883L_REG_CONTROL1;
        tx_buf[1] = GNSS_QMC5883L_CTRL1_VALUE;
        status = i2c_master_transmit(s_compass_dev, tx_buf, 2U, GNSS_I2C_TIMEOUT_MS);
    }
    else
    {
        status = ESP_ERR_INVALID_STATE;
    }

    return status;
}

/**
 * @brief Read magnetometer raw axes and compute heading.
 */
static void gnss_compass_poll(void)
{
    esp_err_t status = ESP_OK;
    int16_t   raw_x  = 0;
    int16_t   raw_y  = 0;
    int16_t   raw_z  = 0;
    float     heading = 0.0f;

    if (s_compass_dev == NULL)
    {
        /* Compass not present. */
    }
    else if (s_compass_type == GNSS_COMPASS_HMC5883)
    {
        uint8_t reg = GNSS_HMC5883_REG_DATA;
        uint8_t data[6];

        status = i2c_master_transmit_receive(s_compass_dev, &reg, 1U, data, 6U, GNSS_I2C_TIMEOUT_MS);
        if (status == ESP_OK)
        {
            raw_x = (int16_t)((((int16_t)data[0]) << 8) | data[1]);
            raw_z = (int16_t)((((int16_t)data[2]) << 8) | data[3]);
            raw_y = (int16_t)((((int16_t)data[4]) << 8) | data[5]);
            (void)raw_z;

            heading = atan2f((float)raw_y, (float)raw_x) * (GNSS_HEADING_FULL_CIRCLE / (2.0f * GNSS_DEGREES_PER_SEMICIRCLE));
            if (heading < 0.0f)
            {
                heading += GNSS_HEADING_FULL_CIRCLE;
            }
            s_snapshot.heading_deg   = heading;
            s_snapshot.compass_valid = true;
        }
    }
    else if (s_compass_type == GNSS_COMPASS_QMC5883L)
    {
        uint8_t reg = GNSS_QMC5883L_REG_DATA;
        uint8_t data[6];

        status = i2c_master_transmit_receive(s_compass_dev, &reg, 1U, data, 6U, GNSS_I2C_TIMEOUT_MS);
        if (status == ESP_OK)
        {
            raw_x = (int16_t)((int16_t)data[1] | (((int16_t)data[0]) << 8));
            raw_y = (int16_t)((int16_t)data[3] | (((int16_t)data[2]) << 8));
            raw_z = (int16_t)((int16_t)data[5] | (((int16_t)data[4]) << 8));
            (void)raw_z;

            heading = atan2f((float)raw_y, (float)raw_x) * (GNSS_HEADING_FULL_CIRCLE / (2.0f * GNSS_DEGREES_PER_SEMICIRCLE));
            if (heading < 0.0f)
            {
                heading += GNSS_HEADING_FULL_CIRCLE;
            }
            s_snapshot.heading_deg   = heading;
            s_snapshot.compass_valid = true;
        }
    }
    else
    {
        /* No compass configured. */
    }
}

/**
 * @brief Background task: drain GNSS UART, parse NMEA, poll compass.
 */
static void gnss_rx_task(void *arg)
{
    uint8_t  chunk[GNSS_UART_READ_CHUNK];
    char     line[GNSS_NMEA_LINE_CAPACITY];
    size_t   line_len = 0U;
    TickType_t last_compass_tick = xTaskGetTickCount();

    (void)arg;

    for (;;)
    {
        const int nbytes = ExpandIo_Uart_Read(EXPAND_IO_UART_PORT_GNSS,
                                              chunk,
                                              sizeof(chunk),
                                              GNSS_UART_READ_TIMEOUT_MS);
        if (nbytes > 0)
        {
            int idx;

            for (idx = 0; idx < nbytes; idx++)
            {
                const char ch = (char)chunk[(size_t)idx];

                if (ch == '\n')
                {
                    if (line_len > 0U)
                    {
                        line[line_len] = '\0';
                        (void)xSemaphoreTake(s_snapshot_mutex, portMAX_DELAY);
                        gnss_handle_sentence(line);
                        gnss_refresh_status_string();
                        (void)xSemaphoreGive(s_snapshot_mutex);
                        line_len = 0U;
                    }
                }
                else if ((ch != '\r') && (line_len < (GNSS_NMEA_LINE_CAPACITY - 1U)))
                {
                    line[line_len] = ch;
                    line_len++;
                }
                else
                {
                    /* Drop overflow or CR characters. */
                }
            }
        }

        if ((xTaskGetTickCount() - last_compass_tick) >= pdMS_TO_TICKS(GNSS_COMPASS_PROBE_INTERVAL_MS))
        {
            last_compass_tick = xTaskGetTickCount();
            (void)xSemaphoreTake(s_snapshot_mutex, portMAX_DELAY);
            gnss_compass_poll();
            gnss_refresh_status_string();
            (void)xSemaphoreGive(s_snapshot_mutex);
        }
    }
}

esp_err_t Gnss_Driver_Init(void)
{
    esp_err_t status = ESP_OK;

    if (s_initialized)
    {
        status = ESP_ERR_INVALID_STATE;
    }
    else if (ExpandIo_I2c_Get_Bus() == NULL)
    {
        status = ESP_ERR_INVALID_STATE;
        ESP_LOGE(TAG, "Expand IO I2C bus not initialized");
    }
    else
    {
        (void)memset(&s_snapshot, 0, sizeof(s_snapshot));
        (void)strncpy(s_snapshot.status, "init", sizeof(s_snapshot.status) - 1U);

        s_snapshot_mutex = xSemaphoreCreateMutexStatic(&s_snapshot_mutex_buffer);

        status = ExpandIo_Uart_Init(EXPAND_IO_UART_PORT_GNSS,
                                    (int)EXPAND_IO_PIN_GNSS_TX,
                                    (int)EXPAND_IO_PIN_GNSS_RX,
                                    GNSS_DRIVER_UART_BAUD,
                                    GNSS_UART_RX_BUF_SIZE,
                                    GNSS_UART_TX_BUF_SIZE);

        if ((status == ESP_OK) && (s_snapshot_mutex != NULL))
        {
            if (gnss_compass_probe(GNSS_HMC5883_I2C_ADDR, GNSS_COMPASS_HMC5883) != ESP_OK)
            {
                (void)gnss_compass_probe(GNSS_QMC5883L_I2C_ADDR, GNSS_COMPASS_QMC5883L);
            }

            if (s_compass_dev != NULL)
            {
                if (gnss_compass_configure() != ESP_OK)
                {
                    ESP_LOGW(TAG, "compass configuration failed");
                }
            }
            else
            {
                ESP_LOGW(TAG, "no compass found on I2C bus");
            }

            s_rx_task = xTaskCreateStatic(gnss_rx_task,
                                          "gnss_rx",
                                          GNSS_RX_TASK_STACK_WORDS,
                                          NULL,
                                          GNSS_RX_TASK_PRIORITY,
                                          s_rx_task_stack,
                                          &s_rx_task_tcb);

            if (s_rx_task == NULL)
            {
                status = ESP_ERR_NO_MEM;
            }
            else
            {
                s_initialized = true;
                ESP_LOGI(TAG, "GNSS driver ready (UART%d @ %u baud)",
                         (int)EXPAND_IO_UART_PORT_GNSS,
                         (unsigned int)GNSS_DRIVER_UART_BAUD);
            }
        }
        else if (s_snapshot_mutex == NULL)
        {
            status = ESP_ERR_NO_MEM;
        }
    }

    return status;
}

esp_err_t Gnss_Driver_Get_Snapshot(gnss_snapshot_t *out_snapshot)
{
    esp_err_t status = ESP_OK;

    if (!s_initialized)
    {
        status = ESP_ERR_INVALID_STATE;
    }
    else if (out_snapshot == NULL)
    {
        status = ESP_ERR_INVALID_ARG;
    }
    else
    {
        (void)xSemaphoreTake(s_snapshot_mutex, portMAX_DELAY);
        *out_snapshot = s_snapshot;
        (void)xSemaphoreGive(s_snapshot_mutex);
    }

    return status;
}
