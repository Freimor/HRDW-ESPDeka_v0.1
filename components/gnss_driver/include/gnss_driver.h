/**
 * @file gnss_driver.h
 * @brief Driver for the RUSHFPV M10 PRO GNSS module (UART NMEA + I2C compass).
 *
 * Parses NMEA sentences from the module UART link and reads the onboard
 * magnetometer (HMC5883 or QMC5883L) over the shared Expand IO I2C bus.
 *
 * @author Freimor
 * @agent Cursor AI
 * @repository https://github.com/Freimor/HRDW-ESPDeka_v0.1
 * @date 2026-07-03
 * @license MIT
 *
 */

#ifndef GNSS_DRIVER_H
#define GNSS_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default UART baud rate for the u-blox M10 on the RUSHFPV module. */
#define GNSS_DRIVER_UART_BAUD           (115200U)

/** Maximum length of the human-readable status string. */
#define GNSS_DRIVER_STATUS_CAPACITY     (96U)

/**
 * @brief GNSS fix dimensionality reported by the receiver.
 */
typedef enum
{
    GNSS_FIX_NONE = 0,
    GNSS_FIX_2D   = 1,
    GNSS_FIX_3D   = 2
} gnss_fix_t;

/**
 * @brief Snapshot of the latest GNSS position and compass reading.
 */
typedef struct
{
    bool         data_valid;     /**< True when at least one NMEA fix was parsed. */
    gnss_fix_t   fix;            /**< Fix type derived from GGA quality field. */
    uint8_t      satellites;     /**< Satellites in use (GGA). */
    double       latitude_deg;   /**< WGS-84 latitude in degrees. */
    double       longitude_deg;  /**< WGS-84 longitude in degrees. */
    float        altitude_m;     /**< Altitude above MSL in metres (GGA). */
    float        speed_mps;        /**< Ground speed in m/s (RMC). */
    float        course_deg;     /**< Course over ground in degrees (RMC). */
    bool         compass_valid;  /**< True when magnetometer data is available. */
    float        heading_deg;    /**< Compass heading 0..360 degrees. */
    char         status[GNSS_DRIVER_STATUS_CAPACITY]; /**< Short summary for chat. */
} gnss_snapshot_t;

/**
 * @brief Initialize the GNSS UART receiver task and compass on the I2C bus.
 *
 * Requires ::ExpandIo_Init to have succeeded beforehand.
 *
 * @return ESP_OK on success, or an esp_err_t error code.
 */
esp_err_t Gnss_Driver_Init(void);

/**
 * @brief Copy the latest GNSS/compass snapshot (thread-safe).
 *
 * @param[out] out_snapshot  Destination structure.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if @p out_snapshot is NULL,
 *         ESP_ERR_INVALID_STATE if not initialized.
 */
esp_err_t Gnss_Driver_Get_Snapshot(gnss_snapshot_t *out_snapshot);

#ifdef __cplusplus
}
#endif

#endif /* GNSS_DRIVER_H */
