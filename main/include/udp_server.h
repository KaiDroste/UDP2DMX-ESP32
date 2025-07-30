#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Network configuration
#define UDP_DEFAULT_PORT 6454
#define UDP_BUFFER_SIZE 1024

// UDP Server functions
esp_err_t udp_server_init(uint16_t port);
void udp_server_deinit(void);
bool udp_server_is_running(void);

// Server control
esp_err_t udp_server_start(void);
esp_err_t udp_server_stop(void);
esp_err_t udp_server_restart(void);

// Statistics
typedef struct {
    uint32_t packets_received;
    uint32_t packets_processed;
    uint32_t packets_invalid;
    uint32_t commands_executed;
    uint32_t command_errors;
} udp_server_stats_t;

udp_server_stats_t udp_server_get_stats(void);
void udp_server_reset_stats(void);

#ifdef __cplusplus
}
#endif
