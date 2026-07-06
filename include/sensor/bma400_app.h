#ifndef BMA400_APP_H_
#define BMA400_APP_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bma400_app_accel_raw {
    int16_t x;
    int16_t y;
    int16_t z;
};

/**
 * @brief Initialize BMA400 using Bosch official driver.
 *
 * Configures:
 * - I2C interface
 * - chip initialization
 * - soft reset
 * - accelerometer ODR
 * - accelerometer range
 * - normal power mode
 *
 * @return 0 on success, negative value on error.
 */
int bma400_app_init(void);

/**
 * @brief Read raw acceleration data from BMA400.
 *
 * Values are raw signed 12-bit accelerometer samples extended to int16_t.
 *
 * @param accel Output raw acceleration structure.
 *
 * @return 0 on success, negative value on error.
 */
int bma400_app_read_accel_raw(struct bma400_app_accel_raw *accel);

/**
 * @brief Convert raw value to approximate mg.
 *
 * For BMA400 configured to +-2g, 12-bit output:
 * full scale = 4096 LSB over 4g span, so 1g ~= 1024 LSB.
 *
 * @param raw Raw accelerometer value.
 *
 * @return Acceleration in mg.
 */
int32_t bma400_app_raw_to_mg(int16_t raw);

#ifdef __cplusplus
}
#endif

#endif /* BMA400_APP_H_ */