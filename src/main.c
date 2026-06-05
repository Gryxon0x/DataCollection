#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "bma400_app.h"

#define SAMPLE_PERIOD_MS 20

int main(void)
{
    int ret;
    uint32_t sample_id = 0;
    int64_t t0_ms;

    printk("BMA400 data collection start\n");

    ret = bma400_app_init();
    if (ret != 0) {
        printk("BMA400 init failed: %d\n", ret);
        return 0;
    }

    printk("sample_id,t_ms,ax_raw,ay_raw,az_raw,ax_mg,ay_mg,az_mg\n");

    t0_ms = k_uptime_get();

    while (1) {
        struct bma400_app_accel_raw accel;
        int64_t t_ms = k_uptime_get() - t0_ms;

        ret = bma400_app_read_accel_raw(&accel);

        if (ret == 0) {
            printk("%u,%lld,%d,%d,%d,%ld,%ld,%ld\n",
                   sample_id,
                   t_ms,
                   accel.x,
                   accel.y,
                   accel.z,
                   bma400_app_raw_to_mg(accel.x),
                   bma400_app_raw_to_mg(accel.y),
                   bma400_app_raw_to_mg(accel.z));
            sample_id++;
        } else {
            printk("bma400_app_read_accel_raw failed: %d\n", ret);
        }

        k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
    }

    return 0;
}