#include "bma400_app.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

#include "bma400.h"

/*
 * This alias must exist in devicetree overlay:
 *
 * / {
 *     aliases {
 *         bma400 = &bma400;
 *     };
 * };
 */
#define BMA400_NODE DT_ALIAS(bma400)

/*
 * BMA400 raw acceleration registers.
 * According to BMA400 register map:
 * X/Y/Z acceleration data starts at 0x04.
 *
 * Layout:
 * 0x04 X LSB
 * 0x05 X MSB
 * 0x06 Y LSB
 * 0x07 Y MSB
 * 0x08 Z LSB
 * 0x09 Z MSB
 */
#define BMA400_REG_ACCEL_X_LSB 0x04
#define BMA400_ACCEL_DATA_LEN  6

static const struct i2c_dt_spec bma400_i2c = I2C_DT_SPEC_GET(BMA400_NODE);

static struct bma400_dev bma400_dev_ctx;

/*
 * Bosch API I2C read callback.
 */
static BMA400_INTF_RET_TYPE bma400_zephyr_i2c_read(uint8_t reg_addr,
                                                    uint8_t *reg_data,
                                                    uint32_t len,
                                                    void *intf_ptr)
{
    ARG_UNUSED(intf_ptr);

    int ret = i2c_burst_read_dt(&bma400_i2c, reg_addr, reg_data, len);

    if (ret == 0) {
        return BMA400_INTF_RET_SUCCESS;
    }

    return BMA400_E_COM_FAIL;
}

/*
 * Bosch API I2C write callback.
 */
static BMA400_INTF_RET_TYPE bma400_zephyr_i2c_write(uint8_t reg_addr,
                                                     const uint8_t *reg_data,
                                                     uint32_t len,
                                                     void *intf_ptr)
{
    ARG_UNUSED(intf_ptr);

    int ret = i2c_burst_write_dt(&bma400_i2c, reg_addr, reg_data, len);

    if (ret == 0) {
        return BMA400_INTF_RET_SUCCESS;
    }

    return BMA400_E_COM_FAIL;
}

/*
 * Bosch API delay callback.
 */
static void bma400_zephyr_delay_us(uint32_t period_us, void *intf_ptr)
{
    ARG_UNUSED(intf_ptr);

    if (period_us < 1000U) {
        k_busy_wait(period_us);
    } else {
        k_usleep(period_us);
    }
}

static int16_t sign_extend_12bit(uint16_t raw)
{
    raw &= 0x0FFF;

    if (raw & 0x0800) {
        raw |= 0xF000;
    }

    return (int16_t)raw;
}

int bma400_app_init(void)
{
    int8_t rslt;
    struct bma400_sensor_conf conf;

    if (!device_is_ready(bma400_i2c.bus)) {
        printk("ERROR: BMA400 I2C bus is not ready\n");
        return -1;
    }

    bma400_dev_ctx.intf = BMA400_I2C_INTF;
    bma400_dev_ctx.read = bma400_zephyr_i2c_read;
    bma400_dev_ctx.write = bma400_zephyr_i2c_write;
    bma400_dev_ctx.delay_us = bma400_zephyr_delay_us;
    bma400_dev_ctx.intf_ptr = NULL;

    /*
     * Some Bosch SensorAPI versions expose read_write_len.
     * If your bma400_dev struct does not have this field, remove this line.
     */
#ifdef BMA400_ENABLE
    bma400_dev_ctx.read_write_len = 32;
#endif

    rslt = bma400_init(&bma400_dev_ctx);
    if (rslt != BMA400_OK) {
        printk("bma400_init failed: %d\n", rslt);
        return rslt;
    }

    rslt = bma400_soft_reset(&bma400_dev_ctx);
    if (rslt != BMA400_OK) {
        printk("bma400_soft_reset failed: %d\n", rslt);
        return rslt;
    }

    k_msleep(10);

    conf.type = BMA400_ACCEL;

    rslt = bma400_get_sensor_conf(&conf, 1, &bma400_dev_ctx);
    if (rslt != BMA400_OK) {
        printk("bma400_get_sensor_conf failed: %d\n", rslt);
        return rslt;
    }

    /*
     * Start configuration for data collection.
     *
     * For first tests:
     * - 100 Hz
     * - +-2g
     * - filtered accel data
     *
     * Later we can switch to:
     * - 50 Hz for walking experiments
     * - +-4g or +-8g if running causes saturation
     */
    conf.param.accel.odr = BMA400_ODR_100HZ;
    conf.param.accel.range = BMA400_RANGE_2G;
    conf.param.accel.data_src = BMA400_DATA_SRC_ACCEL_FILT_1;

    rslt = bma400_set_sensor_conf(&conf, 1, &bma400_dev_ctx);
    if (rslt != BMA400_OK) {
        printk("bma400_set_sensor_conf failed: %d\n", rslt);
        return rslt;
    }

    rslt = bma400_set_power_mode(BMA400_MODE_NORMAL, &bma400_dev_ctx);
    if (rslt != BMA400_OK) {
        printk("bma400_set_power_mode failed: %d\n", rslt);
        return rslt;
    }

    k_msleep(50);

    printk("BMA400 initialized successfully\n");

    return 0;
}

int bma400_app_read_accel_raw(struct bma400_app_accel_raw *accel)
{
    uint8_t data[BMA400_ACCEL_DATA_LEN];
    int ret;

    if (accel == NULL) {
        return -1;
    }

    ret = i2c_burst_read_dt(&bma400_i2c,
                            BMA400_REG_ACCEL_X_LSB,
                            data,
                            sizeof(data));

    if (ret != 0) {
        return ret;
    }

    /*
     * BMA400 acceleration data is 12-bit signed.
     * Data format:
     * raw_x = (msb << 8 | lsb) >> 4
     */
    uint16_t raw_x = ((uint16_t)data[1] << 8) | data[0];
    uint16_t raw_y = ((uint16_t)data[3] << 8) | data[2];
    uint16_t raw_z = ((uint16_t)data[5] << 8) | data[4];

    accel->x = sign_extend_12bit(raw_x >> 4);
    accel->y = sign_extend_12bit(raw_y >> 4);
    accel->z = sign_extend_12bit(raw_z >> 4);

    return 0;
}

int32_t bma400_app_raw_to_mg(int16_t raw)
{
    /*
     * For +-2g and 12-bit signed output:
     * range is -2048..2047 for -2g..+2g.
     * Therefore 1g ~= 1024 LSB.
     */
    return ((int32_t)raw * 1000) / 1024;
}