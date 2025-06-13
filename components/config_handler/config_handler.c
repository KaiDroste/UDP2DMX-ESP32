#include "config_handler.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include <stdio.h>

#include "my_config.h"

static const char *TAG = "config_rest";
static const char *CONFIG_PATH = "/spiffs/config.json";

// Liest Datei und gibt sie als Zeichenkette zurück
char *read_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = malloc(len + 1);
    if (!data)
    {
        fclose(f);
        return NULL;
    }

    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);
    return data;
}

// Speichert eine JSON-Zeichenkette in Datei
esp_err_t save_json(const char *path, const char *json)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return ESP_FAIL;

    fwrite(json, 1, strlen(json), f);
    fclose(f);
    return ESP_OK;
}

// GET /config – gibt aktuelle JSON-Datei zurück
esp_err_t get_config_handler(httpd_req_t *req)
{
    char *data = read_file(CONFIG_PATH);
    if (!data)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, data, HTTPD_RESP_USE_STRLEN);
    free(data);
    return ESP_OK;
}

// POST /config – neue JSON-Konfiguration empfangen und speichern
esp_err_t post_config_handler(httpd_req_t *req)
{
    char buffer[2048];
    int total_len = req->content_len;
    int received = 0;

    if (total_len >= sizeof(buffer))
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON zu groß");
        return ESP_FAIL;
    }

    int ret = httpd_req_recv(req, buffer, total_len);
    if (ret <= 0)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buffer[ret] = '\0';

    cJSON *json = cJSON_Parse(buffer);
    if (!json)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Ungültiges JSON");
        return ESP_FAIL;
    }

    cJSON_Delete(json); // Validierung ok

    if (save_json(CONFIG_PATH, buffer) != ESP_OK)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "OK");

    config_load_from_spiffs(CONFIG_PATH);

    return ESP_OK;
}

esp_err_t patch_config_handler(httpd_req_t *req)
{
    char buffer[2048];
    int total_len = req->content_len;

    if (total_len >= sizeof(buffer))
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON zu groß");
        return ESP_FAIL;
    }

    int ret = httpd_req_recv(req, buffer, total_len);
    if (ret <= 0)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buffer[ret] = '\0';

    // Lade bestehende Konfiguration
    char *existing_json = read_file(CONFIG_PATH);
    if (!existing_json)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(existing_json);
    free(existing_json);
    if (!root)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Bestehendes JSON fehlerhaft");
        return ESP_FAIL;
    }

    cJSON *patch = cJSON_Parse(buffer);
    if (!patch)
    {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Ungültiger Patch");
        return ESP_FAIL;
    }

    // Füge/ersetze ct_config-Werte
    cJSON *patch_ct = cJSON_GetObjectItem(patch, "ct_config");
    if (cJSON_IsObject(patch_ct))
    {
        cJSON *root_ct = cJSON_GetObjectItem(root, "ct_config");
        if (!root_ct)
        {
            root_ct = cJSON_CreateObject();
            cJSON_AddItemToObject(root, "ct_config", root_ct);
        }

        cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, patch_ct)
        {
            if (cJSON_IsNumber(entry))
            {
                cJSON *existing = cJSON_GetObjectItem(root_ct, entry->string);
                if (existing)
                {
                    existing->valuedouble = entry->valuedouble;
                }
                else
                {
                    cJSON_AddNumberToObject(root_ct, entry->string, entry->valuedouble);
                }
            }
        }
    }

    // Füge/ersetze default_ct
    cJSON *patch_default = cJSON_GetObjectItem(patch, "default_ct");
    if (cJSON_IsObject(patch_default))
    {
        cJSON *root_default = cJSON_GetObjectItem(root, "default_ct");
        if (!root_default)
        {
            root_default = cJSON_CreateObject();
            cJSON_AddItemToObject(root, "default_ct", root_default);
        }

        cJSON *min_item = cJSON_GetObjectItem(patch_default, "min");
        cJSON *max_item = cJSON_GetObjectItem(patch_default, "max");

        if (cJSON_IsNumber(min_item))
        {
            cJSON_ReplaceItemInObject(root_default, "min", cJSON_Duplicate(min_item, 1));
        }
        if (cJSON_IsNumber(max_item))
        {
            cJSON_ReplaceItemInObject(root_default, "max", cJSON_Duplicate(max_item, 1));
        }
    }

    cJSON_Delete(patch);

    char *updated_json = cJSON_Print(root);
    cJSON_Delete(root);

    if (save_json(CONFIG_PATH, updated_json) != ESP_OK)
    {
        free(updated_json);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    free(updated_json);
    httpd_resp_sendstr(req, "OK");

    config_load_from_spiffs(CONFIG_PATH);
    return ESP_OK;
}

// Initialisiert den HTTP-Server
void start_rest_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t get_uri = {
            .uri = "/config",
            .method = HTTP_GET,
            .handler = get_config_handler,
            .user_ctx = NULL};

        httpd_uri_t post_uri = {
            .uri = "/config",
            .method = HTTP_POST,
            .handler = post_config_handler,
            .user_ctx = NULL};

        httpd_uri_t patch_uri = {
            .uri = "/config/patch",
            .method = HTTP_POST,
            .handler = patch_config_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &patch_uri);

        httpd_register_uri_handler(server, &get_uri);
        httpd_register_uri_handler(server, &post_uri);
        ESP_LOGI(TAG, "REST-Schnittstelle bereit auf /config");
    }
}
