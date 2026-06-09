#ifndef BLE_DATA_SERVICE_H_
#define BLE_DATA_SERVICE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ble_command_handler_t)(const char *command);

int ble_data_service_init(void);

int ble_data_service_send_text(const char *text);

int ble_data_service_send_bytes(const uint8_t *data, size_t len);

bool ble_data_service_is_connected(void);

bool ble_data_service_notifications_enabled(void);

void ble_data_service_set_command_handler(ble_command_handler_t handler);

#ifdef __cplusplus
}
#endif

#endif /* BLE_DATA_SERVICE_H_ */