#include <stdio.h>
#include <time.h>
#include <string.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "time_sun.h"
#include "esp_crt_bundle.h"

#define TAG "TIME_SUN"
#define SUNRISE_SUNSET_API_URL "https://api.sunrisesunset.io/json?lat=49.553516&lng=25.594767&formatted=0"

#define RESPONSE_BUFFER_SIZE 8192
static char response_buffer[RESPONSE_BUFFER_SIZE] = {0};
static size_t response_len = 0;

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
                ESP_LOGE(TAG, "Response too large for buffer");
                return ESP_FAIL;
            }
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "Response Buffer: %s", response_buffer);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
}

static void obtain_time(void)
{
    initialize_sntp();

    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;

    while (timeinfo.tm_year < (2023 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (retry == retry_count) {
        ESP_LOGE(TAG, "Failed to obtain time");
    } else {
        ESP_LOGI(TAG, "Time obtained successfully");
    }
}

static void get_sunrise_sunset_times(char *sunrise_time_str, size_t sunrise_str_size, char *sunset_time_str, size_t sunset_str_size)
{
    response_len = 0;
    memset(response_buffer, 0, sizeof(response_buffer));

    esp_http_client_config_t config = {
        .url = SUNRISE_SUNSET_API_URL,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 5000,
        .event_handler = _http_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "HTTP Status = %d, content_length = %d", status, content_length);

        if (status == 200) {
            if (response_len > 0) {
                cJSON *json = cJSON_Parse(response_buffer);
                if (json == NULL) {
                    ESP_LOGE(TAG, "JSON parsing failed");
                    esp_http_client_cleanup(client);
                    return;
                }

                cJSON *results = cJSON_GetObjectItem(json, "results");
                if (results == NULL) {
                    ESP_LOGE(TAG, "No 'results' field in JSON");
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
                    ESP_LOGI(TAG, "Sunrise: %s", sunrise_time_str);
                    ESP_LOGI(TAG, "Sunset: %s", sunset_time_str);
                } else {
                    ESP_LOGE(TAG, "Invalid sunrise or sunset format in JSON");
                }

                cJSON_Delete(json);
            } else {
                ESP_LOGE(TAG, "Empty response buffer");
            }
        } else {
            ESP_LOGE(TAG, "Received non-200 HTTP status");
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

void time_sun_init(void)
{
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
    tzset();

    obtain_time();
}

void time_sun_display(void)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "Current date/time in Ternopil: %s", strftime_buf);

    char sunrise_str[64];
    char sunset_str[64];
    get_sunrise_sunset_times(sunrise_str, sizeof(sunrise_str), sunset_str, sizeof(sunset_str));

    cJSON *json = cJSON_Parse(response_buffer);
    if (json == NULL) {
        ESP_LOGE(TAG, "JSON parsing failed");
        return;
    }

    cJSON *results = cJSON_GetObjectItem(json, "results");
    if (results == NULL) {
        ESP_LOGE(TAG, "No 'results' field in JSON");
        cJSON_Delete(json);
        return;
    }

    cJSON *date_json = cJSON_GetObjectItem(results, "date");
    if (!cJSON_IsString(date_json) || date_json->valuestring == NULL) {
        ESP_LOGE(TAG, "Invalid date format in JSON");
        cJSON_Delete(json);
        return;
    }
    char date_str[32];
    strncpy(date_str, date_json->valuestring, sizeof(date_str));

    char sunrise_datetime_str[96];
    snprintf(sunrise_datetime_str, sizeof(sunrise_datetime_str), "%s %s", date_str, sunrise_str);

    char sunset_datetime_str[96];
    snprintf(sunset_datetime_str, sizeof(sunset_datetime_str), "%s %s", date_str, sunset_str);

    struct tm sunrise_tm = {0};
    struct tm sunset_tm = {0};
    char *result;

    result = strptime(sunrise_datetime_str, "%Y-%m-%d %I:%M:%S %p", &sunrise_tm);
    if (result == NULL) {
        ESP_LOGE(TAG, "Failed to parse sunrise time");
    } else {
        ESP_LOGI(TAG, "Parsed sunrise time successfully");
    }

    result = strptime(sunset_datetime_str, "%Y-%m-%d %I:%M:%S %p", &sunset_tm);
    if (result == NULL) {
        ESP_LOGE(TAG, "Failed to parse sunset time");
    } else {
        ESP_LOGI(TAG, "Parsed sunset time successfully");
    }

    time_t sunrise_time = mktime(&sunrise_tm);
    time_t sunset_time = mktime(&sunset_tm);

    double time_until_sunrise = difftime(sunrise_time, now);
    double time_until_sunset = difftime(sunset_time, now);

    if (time_until_sunrise > 0) {
        ESP_LOGI(TAG, "Time until sunrise: %.0f seconds", time_until_sunrise);
    } else if (time_until_sunset > 0) {
        ESP_LOGI(TAG, "Time until sunset: %.0f seconds", time_until_sunset);
    } else {
        ESP_LOGI(TAG, "Sun has already set today.");
    }

    cJSON_Delete(json);
}

