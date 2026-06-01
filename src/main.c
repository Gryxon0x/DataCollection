#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>

#define SLEEP_TIME_MS 100

static void print_sensor_value(const char *name, const struct sensor_value *val)
{
    printk("%s=%d.%06d ", name, val->val1, val->val2);
}

int main(void)
{
    const struct device *accel = DEVICE_DT_GET(DT_ALIAS(accel0));

    printk("BMA400 readout start\n");

    if (!device_is_ready(accel)) {
        printk("ERROR: BMA400 device is not ready\n");
        return 0;
    }

    printk("BMA400 device is ready: %s\n", accel->name);

    while (1) {
        struct sensor_value accel_xyz[3];

        int ret = sensor_sample_fetch(accel);
        if (ret < 0) {
            printk("sensor_sample_fetch() failed: %d\n", ret);
            k_sleep(K_MSEC(SLEEP_TIME_MS));
            continue;
        }

        ret = sensor_channel_get(accel, SENSOR_CHAN_ACCEL_XYZ, accel_xyz);
        if (ret < 0) {
            printk("sensor_channel_get() failed: %d\n", ret);
            k_sleep(K_MSEC(SLEEP_TIME_MS));
            continue;
        }

        print_sensor_value("ax", &accel_xyz[0]);
        print_sensor_value("ay", &accel_xyz[1]);
        print_sensor_value("az", &accel_xyz[2]);
        printk("m/s^2\n");

        k_sleep(K_MSEC(SLEEP_TIME_MS));
    }

    return 0;
}