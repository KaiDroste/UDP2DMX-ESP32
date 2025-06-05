#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    void my_wifi_init(void);
    void my_wifi_switch_next_network(void);
    void my_wifi_set_connected(bool connected);
    bool my_wifi_is_connected(void);

#ifdef __cplusplus
}
#endif