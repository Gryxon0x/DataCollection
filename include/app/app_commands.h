#ifndef APP_COMMANDS_H
#define APP_COMMANDS_H

#include <app/app_context.h>

void app_commands_init(struct app_context *ctx);
void app_command_handler(const char *command);

#endif