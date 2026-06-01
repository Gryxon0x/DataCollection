#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

#define BMA400_NODE DT_ALIAS(bma400)
#define BMA400_CHIP_ID_REG 0x00

static const struct i2c_dt_spec bma400 = I2C_DT_SPEC_GET(BMA400_NODE);

int main(void)
{
    uint8_t chip_id = 0;
    int ret;

    printk("BMA400 raw I2C test start\n");

    if (!device_is_ready(bma400.bus)) {
        printk("ERROR: I2C bus is not ready\n");
        return 0;
    }

    printk("I2C bus ready, BMA400 addr=0x%02x\n", bma400.addr);

    while (1) {
        ret = i2c_reg_read_byte_dt(&bma400, BMA400_CHIP_ID_REG, &chip_id);

        if (ret == 0) {
            printk("BMA400 CHIP_ID = 0x%02x\n", chip_id);
        } else {
            printk("I2C read failed: %d\n", ret);
        }

        k_sleep(K_MSEC(1000));
    }

    return 0;
}