#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "bma400_app.h"

#define SAMPLE_PERIOD_MS 20

int main(void)
{
    int ret;
    uint32_t sample_id = 0;
    int64_t t0_ms;

    k_msleep(3000);

    printk("\n\n=== BMA400 firmware boot ===\n");
    printk("Before bma400_app_init()\n");

    ret = bma400_app_init();

    printk("After bma400_app_init(), ret=%d\n", ret);

    if (ret != 0) {
        while (1) {
            printk("BMA400 init failed, ret=%d\n", ret);
            k_sleep(K_MSEC(1000));
        }
    }

    printk("sample_id,t_ms,ax_raw,ay_raw,az_raw,ax_mg,ay_mg,az_mg\n");

    t0_ms = k_uptime_get();

    while (1) {
        struct bma400_app_accel_raw accel;
        int64_t t_ms = k_uptime_get() - t0_ms;

        ret = bma400_app_read_accel_raw(&accel);

        if (ret == 0) {
            int32_t ax_mg = bma400_app_raw_to_mg(accel.x);
            int32_t ay_mg = bma400_app_raw_to_mg(accel.y);
            int32_t az_mg = bma400_app_raw_to_mg(accel.z);

            printk("%u,%lld,%d,%d,%d,%d,%d,%d\n",
                   sample_id,
                   t_ms,
                   accel.x,
                   accel.y,
                   accel.z,
                   (int)ax_mg,
                   (int)ay_mg,
                   (int)az_mg);

            sample_id++;
        } else {
            printk("bma400_app_read_accel_raw failed: %d\n", ret);
        }

        k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
    }

    return 0;
}