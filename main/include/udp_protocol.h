#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "dmx_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

// UDP Protocol Configuration
#define UDP_PORT 6454
#define MAX_UDP_BUFFER_SIZE 1024

// Protocol command types
typedef enum {
    UDP_CMD_CHANNEL = 'C',          // Direct channel control
    UDP_CMD_PERCENTAGE = 'P',       // Percentage control
    UDP_CMD_RGB = 'R',              // RGB control
    UDP_CMD_TUNABLE_WHITE = 'W',    // Tunable white control
    UDP_CMD_LIGHT_CT = 'L'          // Light with color temperature
} udp_command_type_t;

// Parsed command structure
typedef struct {
    udp_command_type_t type;
    int channel;
    int value;
    int speed;
    bool valid;
} udp_parsed_command_t;

// Protocol functions
esp_err_t udp_protocol_init(void);
void udp_protocol_deinit(void);

// Command parsing and execution
udp_parsed_command_t udp_parse_command(const char* cmd);
dmx_command_result_t udp_execute_command(const udp_parsed_command_t* cmd);
dmx_command_result_t udp_handle_raw_command(const char* cmd);

// Utility functions
int udp_speed_to_milliseconds(int speed);
bool udp_is_valid_command_format(const char* cmd);

#ifdef __cplusplus
}
#endif
