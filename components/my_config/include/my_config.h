#pragma once

void spiffs_init(void);
void config_load_ct_values(const char *json);
void config_load_from_spiffs(const char *path);
void get_ct_range(int ch, int *min_ct, int *max_ct);
void get_ct_sorted(int ch, int *ct_ww, int *ct_cw, int *ch_ww, int *ch_cw);