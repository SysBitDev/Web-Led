#include <stdio.h>
#include <time.h>
#include <string.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "time_sun.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include "wifi.h"

#define TAG "TIME_SUN"
#define SUNRISE_SUNSET_API_URL "https://api.sunrisesunset.io/json?lat=49.553516&lng=25.594767&formatted=0"

#define RESPONSE_BUFFER_SIZE 8192
static char response_buffer[RESPONSE_BUFFER_SIZE] = {0};
static size_t response_len = 0;

time_t sunrise_time = 0;
time_t sunset_time = 0;
volatile bool is_night_time = false;
volatile bool ignore_sun = false;

SemaphoreHandle_t is_night_time_mutex;

static void convert_time_to_24h_format(const char *time_str_12h, char *time_str_24h, size_t max_size)
{
    int hour, minute, second;
    char meridiem[3];
    if (sscanf(time_str_12h, "%d:%d:%d %2s", &hour, &minute, &second, meridiem) == 4) {
        if ((strcasecmp(meridiem, "PM") == 0) && hour != 12) {
            hour += 12;
        } else if ((strcasecmp(meridiem, "AM") == 0) && hour == 12) {
            hour = 0;
        }
        snprintf(time_str_24h, max_size, "%02d:%02d:%02d", hour, minute, second);
    } else {
        ESP_LOGE(TAG, "Unable to parse the time string: %s", time_str_12h);
        strncpy(time_str_24h, "00:00:00", max_size);
    }
}

static void clock_task(void *pvParameter)
{
    static int last_day = -1;
    static bool warned = false;

    while (1) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%d.%m.%Y %H:%M:%S", &timeinfo);
        ESP_LOGI(TAG, "Current time: %s", strftime_buf);

        if (timeinfo.tm_mday != last_day) {
            last_day = timeinfo.tm_mday;
            ESP_LOGI(TAG, "Date has changed, updating sunrise and sunset times.");
            time_sun_display();
        }

        if (sunrise_time != 0 && sunset_time != 0) {
            if ((now >= sunset_time) || (now < sunrise_time)) {
                if (!is_night_time) {
                    is_night_time = true;
                    ESP_LOGE(TAG, "Night time has arrived. The value is_night_time = %d", is_night_time);
                }
            } else {
                if (is_night_time) {
                    is_night_time = false;
                    ESP_LOGE(TAG, "Daytime has arrived. The value is_night_time = %d", is_night_time);
                }
            }
            warned = false;
        } else {
            if (!warned) {
                ESP_LOGW(TAG, "Sunrise and sunset times not set yet.");
                warned = true;
            }
            is_night_time = false;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
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
    esp_sntp_set_sync_interval(3600000);
    esp_sntp_init();
}

static void obtain_time(void)
{
    initialize_sntp();

    time_t now = 0;
    struct tm timeinfo = {0};
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

    wifi_wait_connected();

    obtain_time();

    xTaskCreate(clock_task, "clock_task", 4096, NULL, 5, NULL);

    time_sun_display();
    is_night_time_mutex = xSemaphoreCreateMutex();
}

void time_sun_display(void)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%d.%m.%Y %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "Current date/time in Ternopil: %s", strftime_buf);

    char sunrise_str[64];
    char sunset_str[64];
    get_sunrise_sunset_times(sunrise_str, sizeof(sunrise_str), sunset_str, sizeof(sunset_str));

    char sunrise_24h[16];
    char sunset_24h[16];
    convert_time_to_24h_format(sunrise_str, sunrise_24h, sizeof(sunrise_24h));
    convert_time_to_24h_format(sunset_str, sunset_24h, sizeof(sunset_24h));

    ESP_LOGI(TAG, "Sunrise time: %s", sunrise_24h);
    ESP_LOGI(TAG, "Sunset time: %s", sunset_24h);

    struct tm sunrise_tm = {0};
    struct tm sunset_tm = {0};

    char date_str[32];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", &timeinfo);

    char sunrise_datetime_str[64];
    snprintf(sunrise_datetime_str, sizeof(sunrise_datetime_str), "%s %s", date_str, sunrise_24h);

    char sunset_datetime_str[64];
    snprintf(sunset_datetime_str, sizeof(sunset_datetime_str), "%s %s", date_str, sunset_24h);

    if (strptime(sunrise_datetime_str, "%Y-%m-%d %H:%M:%S", &sunrise_tm) == NULL) {
        ESP_LOGE(TAG, "Failed to parse sunrise time");
    } else {
        ESP_LOGI(TAG, "Parsed sunrise time successfully");
    }

    if (strptime(sunset_datetime_str, "%Y-%m-%d %H:%M:%S", &sunset_tm) == NULL) {
        ESP_LOGE(TAG, "Failed to parse sunset time");
    } else {
        ESP_LOGI(TAG, "Parsed sunset time successfully");
    }

    time_t parsed_sunrise_time = mktime(&sunrise_tm);
    time_t parsed_sunset_time = mktime(&sunset_tm);

    sunrise_time = parsed_sunrise_time;
    sunset_time = parsed_sunset_time;

    double time_until_sunrise = difftime(sunrise_time, now);
    double time_until_sunset = difftime(sunset_time, now);

    if (time_until_sunrise > 0) {
        ESP_LOGI(TAG, "Until sunrise: %.0f seconds", time_until_sunrise);
    } else if (time_until_sunset > 0) {
        ESP_LOGI(TAG, "Until sunset: %.0f seconds", time_until_sunset);
    } else {
        ESP_LOGI(TAG, "The sun has already set today.");
    }
}
