#include "time_sun.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"

#define TAG "TIME_SUN"

static region_info_t regions[] = {
    {"Europe/Kyiv", "EET-2EEST,M3.5.0/3,M10.5.0/4", 50.4501, 30.5234},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0/2", 51.5074, -0.1278},
    {"America/New_York", "EST5EDT,M3.2.0/2,M11.1.0/2", 40.7128, -74.0060},
};

#define RESPONSE_BUFFER_SIZE 8192
static char response_buffer[RESPONSE_BUFFER_SIZE] = {0};
static size_t response_len = 0;

static int num_regions = sizeof(regions)/sizeof(regions[0]);

static region_info_t current_region = {"Europe/Kyiv", "EET-2EEST,M3.5.0/3,M10.5.0/4", 50.4501, 30.5234};

static char current_sunrise_formatted[32] = {0};
static char current_sunset_formatted[32] = {0};

static void log_time_task(void *pvParameter);
static void sync_time_task(void *pvParameter);
static void fetch_sun_times_task(void *pvParameter);

static esp_err_t _http_event_handler(esp_http_client_event_t *evt);
static void get_sunrise_sunset_times(char *sunrise_time_str, size_t sunrise_str_size, char *sunset_time_str, size_t sunset_str_size);

void get_current_time_str(char *buffer, size_t buffer_size) {
    time_t now;
    struct tm timeinfo;

    time(&now);

    localtime_r(&now, &timeinfo);

    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

static void set_timezone_func(const char *tz)
{
    setenv("TZ", tz, 1);
    tzset();
    ESP_LOGI(TAG, "Часовий пояс встановлено: %s", tz);
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Ініціалізація SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
}

static void obtain_time(void)
{
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;

    while (timeinfo.tm_year < (2023 - 1900) && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Очікування встановлення системного часу... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (retry == retry_count)
    {
        ESP_LOGE(TAG, "Не вдалося отримати час");
    }
    else
    {
        ESP_LOGI(TAG, "Час успішно отримано: %s", asctime(&timeinfo));
    }
}

void set_current_region(const char *region, const char *timezone)
{
    for (int i = 0; i < num_regions; i++) {
        if (strcmp(regions[i].region, region) == 0 && strcmp(regions[i].timezone, timezone) == 0) {
            current_region = regions[i];
            set_timezone_func(current_region.timezone);
            ESP_LOGI(TAG, "Поточний регіон встановлено: %s", current_region.region);
            time_sun_display();
            return;
        }
    }
    ESP_LOGE(TAG, "Регіон не знайдено: %s", region);
}

static void get_sunrise_sunset_times(char *sunrise_time_str, size_t sunrise_str_size, char *sunset_time_str, size_t sunset_str_size)
{
    static const char *api_template = "https://api.sunrisesunset.io/json?lat=%.6f&lng=%.6f&formatted=0";
    char api_url[256];
    snprintf(api_url, sizeof(api_url), api_template, current_region.lat, current_region.lon);

    esp_http_client_config_t config = {
        .url = api_url,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 5000,
        .event_handler = _http_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Не вдалося ініціалізувати HTTP клієнта");
        return;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "HTTP Статус = %d, довжина контенту = %d", status, content_length);

        if (status == 200) {
            if (response_len > 0) {
                cJSON *json = cJSON_Parse(response_buffer);
                if (json == NULL) {
                    ESP_LOGE(TAG, "Не вдалося розпарсити JSON");
                    esp_http_client_cleanup(client);
                    return;
                }

                cJSON *results = cJSON_GetObjectItem(json, "results");
                if (results == NULL) {
                    ESP_LOGE(TAG, "В JSON немає поля 'results'");
                    cJSON_Delete(json);
                    esp_http_client_cleanup(client);
                    return;
                }

                cJSON *sunrise_json = cJSON_GetObjectItem(results, "sunrise");
                cJSON *sunset_json = cJSON_GetObjectItem(results, "sunset");

                if (cJSON_IsString(sunrise_json) && (sunrise_json->valuestring != NULL) &&
                    cJSON_IsString(sunset_json) && (sunset_json->valuestring != NULL)) {
                    strncpy(sunrise_time_str, sunrise_json->valuestring, sunrise_str_size);
                    strncpy(sunset_time_str, sunset_json->valuestring, sunset_str_size);
                    ESP_LOGI(TAG, "Схід: %s", sunrise_time_str);
                    ESP_LOGI(TAG, "Захід: %s", sunset_time_str);
                } else {
                    ESP_LOGE(TAG, "Неправильний формат часу сходу або заходу сонця в JSON");
                }

                cJSON_Delete(json);
            } else {
                ESP_LOGE(TAG, "Порожній буфер відповіді");
            }
        } else {
            ESP_LOGE(TAG, "Отримано HTTP статус відмінний від 200");
        }
    } else {
        ESP_LOGE(TAG, "HTTP запит не вдався: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (evt->data_len > 0) {
            if (response_len + evt->data_len < RESPONSE_BUFFER_SIZE - 1) {
                memcpy(response_buffer + response_len, evt->data, evt->data_len);
                response_len += evt->data_len;
                response_buffer[response_len] = '\0';
            } else {
                ESP_LOGE(TAG, "Відповідь занадто велика для буфера");
                return ESP_FAIL;
            }
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "Буфер відповіді: %s", response_buffer);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void time_sun_display(void)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "Поточна дата/час у %s: %s", current_region.region, strftime_buf);

    char sunrise_str[64];
    char sunset_str[64];
    get_sunrise_sunset_times(sunrise_str, sizeof(sunrise_str), sunset_str, sizeof(sunset_str));

    char date_str[32];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", &timeinfo);

    char sunrise_datetime_str[96];
    snprintf(sunrise_datetime_str, sizeof(sunrise_datetime_str), "%s %s", date_str, sunrise_str);

    char sunset_datetime_str[96];
    snprintf(sunset_datetime_str, sizeof(sunset_datetime_str), "%s %s", date_str, sunset_str);

    struct tm sunrise_tm = {0};
    struct tm sunset_tm = {0};
    char *result;

    ESP_LOGI(TAG, "Сформований рядок часу сходу сонця: %s", sunrise_datetime_str);
    ESP_LOGI(TAG, "Сформований рядок часу заходу сонця: %s", sunset_datetime_str);

    result = strptime(sunrise_datetime_str, "%Y-%m-%d %I:%M:%S %p", &sunrise_tm);
    if (result == NULL) {
        ESP_LOGE(TAG, "Не вдалося розпарсити час сходу сонця");
    } else {
        ESP_LOGI(TAG, "Час сходу сонця успішно розпарсено");
    }

    result = strptime(sunset_datetime_str, "%Y-%m-%d %I:%M:%S %p", &sunset_tm);
    if (result == NULL) {
        ESP_LOGE(TAG, "Не вдалося розпарсити час заходу сонця");
    } else {
        ESP_LOGI(TAG, "Час заходу сонця успішно розпарсено");
    }

    time_t sunrise_time = mktime(&sunrise_tm);
    time_t sunset_time = mktime(&sunset_tm);

    if (sunrise_time == -1) {
        ESP_LOGE(TAG, "mktime не вдалося для часу сходу сонця");
    }
    if (sunset_time == -1) {
        ESP_LOGE(TAG, "mktime не вдалося для часу заходу сонця");
    }

    double time_until_sunrise = difftime(sunrise_time, now);
    double time_until_sunset = difftime(sunset_time, now);

    char sunrise_formatted[32];
    char sunset_formatted[32];
    strftime(sunrise_formatted, sizeof(sunrise_formatted), "%H:%M:%S", &sunrise_tm);
    strftime(sunset_formatted, sizeof(sunset_formatted), "%H:%M:%S", &sunset_tm);

    strncpy(current_sunrise_formatted, sunrise_formatted, sizeof(current_sunrise_formatted));
    strncpy(current_sunset_formatted, sunset_formatted, sizeof(current_sunset_formatted));

    ESP_LOGI(TAG, "Час сходу сонця: %s", sunrise_formatted);
    ESP_LOGI(TAG, "Час заходу сонця: %s", sunset_formatted);

    if (time_until_sunrise > 0) {
        ESP_LOGI(TAG, "Час до сходу сонця: %.0f секунд", time_until_sunrise);
    } else if (time_until_sunset > 0) {
        ESP_LOGI(TAG, "Час до заходу сонця: %.0f секунд", time_until_sunset);
    } else {
        ESP_LOGI(TAG, "Сонце вже зайшло сьогодні.");
    }
}

void get_sun_times_json(char *buffer, size_t buffer_size)
{
    snprintf(buffer, buffer_size, "{\"sunrise\":\"%s\",\"sunset\":\"%s\"}", current_sunrise_formatted, current_sunset_formatted);
}

void time_sun_init(void)
{
    set_timezone_func(current_region.timezone);

    initialize_sntp();

    obtain_time();

    xTaskCreate(&log_time_task, "log_time_task", 2048, NULL, 5, NULL);

    xTaskCreate(&sync_time_task, "sync_time_task", 2048, NULL, 5, NULL);

    xTaskCreate(&fetch_sun_times_task, "fetch_sun_times_task", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "Годинник запущено");
}

static void log_time_task(void *pvParameter)
{
    char time_str[64];
    while (1)
    {
        get_current_time_str(time_str, sizeof(time_str));
        ESP_LOGI(TAG, "Поточний час: %s", time_str);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void sync_time_task(void *pvParameter)
{
    const TickType_t sync_interval = 3600 * 1000 / portTICK_PERIOD_MS;

    while (1)
    {
        ESP_LOGI(TAG, "Синхронізація часу з NTP почалася");
        obtain_time();
        ESP_LOGI(TAG, "Синхронізація часу з NTP завершена");
        vTaskDelay(sync_interval);
    }
}

static void fetch_sun_times_task(void *pvParameter)
{
    const TickType_t fetch_interval = 6 * 3600 * 1000 / portTICK_PERIOD_MS;

    while (1)
    {
        ESP_LOGI(TAG, "Отримання часу сходу та заходу сонця");
        time_sun_display();
        vTaskDelay(fetch_interval);
    }
}
