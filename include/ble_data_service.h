#ifndef BLE_DATA_SERVICE_H_
#define BLE_DATA_SERVICE_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int ble_data_service_init(void);

int ble_data_service_send_text(const char *text);

bool ble_data_service_is_connected(void);

bool ble_data_service_notifications_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_DATA_SERVICE_H_ */