#include "cJSON.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <stdio.h>

#define MAX_CHANNELS 512
static int ct_config[MAX_CHANNELS]; // 0 = nicht gesetzt

static const char *TAG = "config";

int default_min_ct = 3500;
int default_max_ct = 6700;

void spiffs_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs",
        .max_files = 5,
        .format_if_mount_failed = true};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE("SPIFFS", "SPIFFS Mount fehlgeschlagen: %s", esp_err_to_name(ret));
        return;
    }

    size_t total = 0, used = 0;
    esp_spiffs_info("spiffs", &total, &used);
    ESP_LOGI("SPIFFS", "SPIFFS total: %d, used: %d", total, used);
}

void config_load_ct_values(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    // default_min_ct = 0;
    // default_max_ct = 0;
    if (!root)
    {
        ESP_LOGE(TAG, "JSON-Parsing fehlgeschlagen");
        return;
    }

    cJSON *ct_map = cJSON_GetObjectItem(root, "ct_config");
    if (!cJSON_IsObject(ct_map))
    {
        ESP_LOGW(TAG, "Kein gültiges ct_config-Objekt gefunden");
    }
    else
    {
        for (int i = 0; i < MAX_CHANNELS; ++i)
        {
            ct_config[i] = 0; // nicht gesetzt
        }

        cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, ct_map)
        {
            int ch = atoi(entry->string);
            if (ch >= 1 && ch < MAX_CHANNELS && cJSON_IsNumber(entry))
            {
                ct_config[ch] = entry->valueint;
                ESP_LOGI(TAG, "CT Kanal %d gesetzt auf %d K", ch, ct_config[ch]);
            }
        }
    }

    cJSON *default_ct = cJSON_GetObjectItemCaseSensitive(root, "default_ct");
    if (default_ct)
    {
        cJSON *min_item = cJSON_GetObjectItem(default_ct, "min");
        cJSON *max_item = cJSON_GetObjectItem(default_ct, "max");

        if (cJSON_IsNumber(min_item))
        {
            default_min_ct = min_item->valueint;
        }
        if (cJSON_IsNumber(max_item))
        {
            default_max_ct = max_item->valueint;
        }

        if (default_min_ct > default_max_ct)
        {
            int tmp = default_min_ct;
            default_min_ct = default_max_ct;
            default_max_ct = tmp;
            ESP_LOGW(TAG, "Standard-CT-Werte waren vertauscht – wurden korrigiert");
        }

        if (cJSON_IsNumber(min_item))
        {
            default_min_ct = min_item->valueint;
            ESP_LOGI(TAG, "Standard-CT min gesetzt auf %d K", default_min_ct);
        }
        else
        {
            ESP_LOGW(TAG, "default_ct.min fehlt oder ungültig");
        }

        if (cJSON_IsNumber(max_item))
        {
            default_max_ct = max_item->valueint;
            ESP_LOGI(TAG, "Standard-CT max gesetzt auf %d K", default_max_ct);
        }
        else
        {
            ESP_LOGW(TAG, "default_ct.max fehlt oder ungültig");
        }
    }
    ESP_LOGD(TAG, "Geladene Standardwerte nach Patch: min=%d, max=%d", default_min_ct, default_max_ct);

    cJSON_Delete(root);
}

void config_load_from_spiffs(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        ESP_LOGE(TAG, "Kann Datei nicht öffnen: %s", path);
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer)
    {
        ESP_LOGE(TAG, "Speicher für JSON-Puffer konnte nicht reserviert werden");
        fclose(f);
        return;
    }

    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    config_load_ct_values(buffer);
    free(buffer);
}

void get_ct_range(int ch, int *min_ct, int *max_ct)
{
    int ct1 = (ch >= 1 && ch < MAX_CHANNELS) ? ct_config[ch] : 0;
    int ct2 = (ch + 1 >= 1 && ch + 1 < MAX_CHANNELS) ? ct_config[ch + 1] : 0;

    if (ct1 && ct2)
    {
        *min_ct = (ct1 < ct2) ? ct1 : ct2;
        *max_ct = (ct1 > ct2) ? ct1 : ct2;
    }
    else if (ct1 || ct2)
    {
        if (!ct1)
        {
            ESP_LOGW(TAG, "CT für Kanal %d fehlt – Standard %d K verwendet", ch, default_min_ct);
            ct1 = default_min_ct;
        }
        if (!ct2)
        {
            ESP_LOGW(TAG, "CT für Kanal %d fehlt – Standard %d K verwendet", ch + 1, default_max_ct);
            ct2 = default_max_ct;
        }
        *min_ct = (ct1 < ct2) ? ct1 : ct2;
        *max_ct = (ct1 > ct2) ? ct1 : ct2;
    }
    else
    {
        *min_ct = default_min_ct;
        *max_ct = default_max_ct;
        ESP_LOGW(TAG, "CT-Konfig für Kanäle %d/%d fehlt – Standardwerte %d–%d K verwendet", ch, ch + 1, *min_ct, *max_ct);
    }
}

void get_ct_sorted(int ch, int *ct_ww, int *ct_cw, int *ch_ww, int *ch_cw)
{
    int cta = (ch >= 1 && ch < MAX_CHANNELS) ? ct_config[ch] : 0;
    int ctb = (ch + 1 >= 1 && ch + 1 < MAX_CHANNELS) ? ct_config[ch + 1] : 0;

    if (!cta)
    {
        ESP_LOGW(TAG, "CT für Kanal %d fehlt – Standard %d K verwendet", ch, default_min_ct);
        cta = default_min_ct;
    }

    if (!ctb)
    {
        ESP_LOGW(TAG, "CT für Kanal %d fehlt – Standard %d K verwendet", ch + 1, default_max_ct);
        ctb = default_max_ct;
    }

    if (cta < ctb)
    {
        *ct_ww = cta;
        *ct_cw = ctb;
        *ch_ww = ch;
        *ch_cw = ch + 1;
    }
    else
    {
        *ct_ww = ctb;
        *ct_cw = cta;
        *ch_ww = ch + 1;
        *ch_cw = ch;
    }
}
