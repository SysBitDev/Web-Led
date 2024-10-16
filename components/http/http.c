#include "http.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "led.h"
#include "wifi.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_system.h"
#include "freertos/semphr.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include "dirent.h"
#include "time_sun.h"

static const char *TAG = "http_server";
static httpd_handle_t server = NULL;

#define BASIC_AUTH_USERNAME "admin"
#define BASIC_AUTH_PASSWORD "password"
#define BASIC_AUTH_ENCODED "YWRtaW46cGFzc3dvcmQ="
#define BASIC_AUTH_HEADER "Basic YWRtaW46cGFzc3dvcmQ="

static SemaphoreHandle_t led_mutex = NULL;

static esp_err_t basic_auth_get_handler(httpd_req_t *req) {
    char auth_header[128] = {0};
    size_t auth_header_len = sizeof(auth_header);
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, auth_header_len) != ESP_OK) {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Login Required\"");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
        return ESP_FAIL;
    }
    if (strncmp(auth_header, BASIC_AUTH_HEADER, strlen(BASIC_AUTH_HEADER)) != 0) {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Login Required\"");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void list_spiffs_files(void) {
    ESP_LOGI(TAG, "Listing files in SPIFFS:");
    DIR* dir = opendir("/spiffs");
    if (dir != NULL) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            ESP_LOGI(TAG, " - %s", ent->d_name);
        }
        closedir(dir);
    } else {
        ESP_LOGE(TAG, "Failed to open directory.");
    }
}

static esp_err_t spiffs_get_handler(httpd_req_t *req) {
    char filepath[ESP_VFS_PATH_MAX + 128];
    strlcpy(filepath, "/spiffs", sizeof(filepath));
    if (strcmp(req->uri, "/") == 0) {
        strcat(filepath, "/index.html");
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }

    ESP_LOGI(TAG, "Requested file: %s", filepath);

    const char *mode = "r";
    if (strstr(filepath, ".ico")) {
        mode = "rb";
    }
    FILE* file = fopen(filepath, mode);
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    // Determine MIME type
    const char* mime_type = "text/plain";
    if (strstr(filepath, ".html")) {
        mime_type = "text/html";
    } else if (strstr(filepath, ".css")) {
        mime_type = "text/css";
    } else if (strstr(filepath, ".js")) {
        mime_type = "application/javascript";
    } else if (strstr(filepath, ".png")) {
        mime_type = "image/png";
    } else if (strstr(filepath, ".jpg") || strstr(filepath, ".jpeg")) {
        mime_type = "image/jpeg";
    } else if (strstr(filepath, ".gif")) {
        mime_type = "image/gif";
    } else if (strstr(filepath, ".svg")) {
        mime_type = "image/svg+xml";
    } else if (strstr(filepath, ".ico")) {
        mime_type = "image/x-icon";
    }

    httpd_resp_set_type(req, mime_type);

    char buffer[1024];
    size_t read_size;

    while ((read_size = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, read_size) != ESP_OK) {
            fclose(file);
            ESP_LOGE(TAG, "Failed to send chunk of %s", filepath);
            return ESP_FAIL;
        }
    }

    fclose(file);

    if (httpd_resp_send_chunk(req, NULL, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to finalize response for %s", filepath);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Successfully sent %s", filepath);
    return ESP_OK;
}


static esp_err_t led_on_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        led_strip_start();
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }
    httpd_resp_send(req, "LED Strip Turned On", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t led_off_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        led_strip_stop_effect();
        led_strip_stop();
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }
    httpd_resp_send(req, "LED Strip and Effects Turned Off", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t wave_effect_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        led_strip_wave_effect();
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }
    httpd_resp_send(req, "Wave Effect Started", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t stairs_effect_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        led_strip_stairs_effect();
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }
    httpd_resp_send(req, "Stairs Effect Started", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t toggle_wave_direction_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        led_strip_toggle_wave_direction();
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }
    httpd_resp_send(req, "Wave Direction Toggled", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t set_brightness_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    char buf[32];
    int brightness = 100;
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[12];
        if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
            brightness = atoi(param);
            if (brightness < 0) brightness = 0;
            if (brightness > 100) brightness = 100;
        }
    }
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        led_strip_set_brightness((uint8_t)brightness);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }
    httpd_resp_send(req, "Brightness Set", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t set_stairs_speed_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    char buf[32];
    int speed = 100;
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[12];
        if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
            speed = atoi(param);
            if (speed < 10) speed = 10;
            if (speed > 100) speed = 100;
        }
    }
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        led_strip_set_stairs_speed((uint16_t)speed);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }
    httpd_resp_send(req, "Stairs Speed Set", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t set_stairs_group_size_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    char buf[32];
    int size = 1;
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[12];
        if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
            size = atoi(param);
            if (size < 1) size = 1;
            uint16_t led_length = led_strip_get_length();
            if (size > led_length) size = led_length;
        }
    }
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        led_strip_set_stairs_group_size((uint16_t)size);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }
    httpd_resp_send(req, "Stairs Group Size Set", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t set_color_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    char buf[128];
    int r = 255, g = 255, b = 255;
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[12];
        if (httpd_query_key_value(buf, "r", param, sizeof(param)) == ESP_OK) {
            r = atoi(param);
            if (r < 0) r = 0;
            if (r > 255) r = 255;
        }
        if (httpd_query_key_value(buf, "g", param, sizeof(param)) == ESP_OK) {
            g = atoi(param);
            if (g < 0) g = 0;
            if (g > 255) g = 255;
        }
        if (httpd_query_key_value(buf, "b", param, sizeof(param)) == ESP_OK) {
            b = atoi(param);
            if (b < 0) b = 0;
            if (b > 255) b = 255;
        }
    }
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        led_strip_set_color((uint8_t)r, (uint8_t)g, (uint8_t)b);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }
    httpd_resp_send(req, "Color Set", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t reset_to_rgb_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        led_strip_reset_to_rgb();
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }
    httpd_resp_send(req, "RGB Mode Restored", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t save_parameters_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        led_strip_save_parameters();
        uint8_t brightness = led_strip_get_brightness();
        uint8_t r, g, b;
        led_strip_get_color(&r, &g, &b);
        bool custom_color = led_strip_get_custom_color_mode();
        uint16_t stairs_speed = led_strip_get_stairs_speed();
        uint16_t stairs_group_size = led_strip_get_stairs_group_size();
        uint16_t led_count = led_strip_get_length();
        xSemaphoreGive(led_mutex);
        char resp[256];
        snprintf(resp, sizeof(resp), "Parameters Saved:\nBrightness: %d\nColor Mode: %s\nR:%d G:%d B:%d\nStairs Speed: %d ms\nStairs Group Size: %d\nLED Count: %d",
                 brightness, custom_color ? "Custom" : "RGB", r, g, b, stairs_speed, stairs_group_size, led_count);
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }
}

static esp_err_t erase_network_data_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        erase_wifi_config();
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }
    httpd_resp_send(req, "Network data erased. Restarting...", HTTPD_RESP_USE_STRLEN);
    esp_restart();
    return ESP_OK;
}

static esp_err_t set_led_count_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    char buf[32];
    int count = 460;
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[12];
        if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
            count = atoi(param);
            if (count < 1) count = 1;
            if (count > 1000) count = 1000;
        }
    }
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        led_strip_set_length((uint16_t)count);
        xSemaphoreGive(led_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }
    httpd_resp_send(req, "LED Count Set", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t get_settings_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    if (xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE) {
        uint8_t brightness = led_strip_get_brightness();
        uint8_t r, g, b;
        led_strip_get_color(&r, &g, &b);
        uint16_t stairs_speed = led_strip_get_stairs_speed();
        uint16_t stairs_group_size = led_strip_get_stairs_group_size();
        uint16_t led_count = led_strip_get_length();
        xSemaphoreGive(led_mutex);
        char resp[256];
        snprintf(resp, sizeof(resp),
                 "{\"brightness\":%d,\"color\":{\"r\":%d,\"g\":%d,\"b\":%d},\"stairs_speed\":%d,\"stairs_group_size\":%d, \"led_count\":%d}",
                 brightness, r, g, b, stairs_speed, stairs_group_size, led_count);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to take led_mutex");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error");
        return ESP_FAIL;
    }
}

static esp_err_t favicon_get_handler(httpd_req_t *req) {
    const char *filepath = "/spiffs/favicon.ico";

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open favicon: %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/x-icon");

    char buffer[512];
    size_t read_size;

    while ((read_size = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, read_size) != ESP_OK) {
            fclose(file);
            ESP_LOGE(TAG, "Failed to send chunk of favicon");
            return ESP_FAIL;
        }
    }

    fclose(file);

    if (httpd_resp_send_chunk(req, NULL, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to finalize favicon response");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Successfully sent favicon.ico");
    return ESP_OK;
}


static esp_err_t restart_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    httpd_resp_send(req, "Restarting...", HTTPD_RESP_USE_STRLEN);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    esp_restart();
    return ESP_OK;
}

static esp_err_t toggle_ignore_sun_handler(httpd_req_t *req) {
    ignore_sun = !ignore_sun;
    const char* resp_str = ignore_sun ? "Sun is now ignored." : "Sun is now considered.";
    char resp_json[128];
    snprintf(resp_json, sizeof(resp_json), "{\"ignore_sun\":%s,\"message\":\"%s\"}", ignore_sun ? "true" : "false", resp_str);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static httpd_uri_t toggle_ignore_sun_uri = {
    .uri      = "/toggle-ignore-sun",
    .method   = HTTP_GET,
    .handler  = toggle_ignore_sun_handler,
    .user_ctx = NULL
};

static httpd_uri_t favicon = {
    .uri = "/favicon.ico",
    .method = HTTP_GET,
    .handler = favicon_get_handler,
    .user_ctx = NULL
};

static httpd_uri_t led_on = {
    .uri = "/led-on",
    .method = HTTP_GET,
    .handler = led_on_handler,
    .user_ctx = NULL
};

static httpd_uri_t led_off = {
    .uri = "/led-off",
    .method = HTTP_GET,
    .handler = led_off_handler,
    .user_ctx = NULL
};

static httpd_uri_t wave_effect = {
    .uri = "/wave-effect",
    .method = HTTP_GET,
    .handler = wave_effect_handler,
    .user_ctx = NULL
};

static httpd_uri_t stairs_effect = {
    .uri = "/stairs-effect",
    .method = HTTP_GET,
    .handler = stairs_effect_handler,
    .user_ctx = NULL
};

static httpd_uri_t toggle_wave_direction = {
    .uri = "/toggle-wave-direction",
    .method = HTTP_GET,
    .handler = toggle_wave_direction_handler,
    .user_ctx = NULL
};

static httpd_uri_t set_brightness = {
    .uri = "/set-brightness",
    .method = HTTP_GET,
    .handler = set_brightness_handler,
    .user_ctx = NULL
};

static httpd_uri_t set_stairs_speed = {
    .uri = "/set-stairs-speed",
    .method = HTTP_GET,
    .handler = set_stairs_speed_handler,
    .user_ctx = NULL
};

static httpd_uri_t set_stairs_group_size = {
    .uri = "/set-stairs-group-size",
    .method = HTTP_GET,
    .handler = set_stairs_group_size_handler,
    .user_ctx = NULL
};

static httpd_uri_t set_color = {
    .uri = "/set-color",
    .method = HTTP_GET,
    .handler = set_color_handler,
    .user_ctx = NULL
};

static httpd_uri_t reset_to_rgb = {
    .uri = "/reset-to-rgb",
    .method = HTTP_GET,
    .handler = reset_to_rgb_handler,
    .user_ctx = NULL
};

static httpd_uri_t save_parameters = {
    .uri = "/save-parameters",
    .method = HTTP_GET,
    .handler = save_parameters_handler,
    .user_ctx = NULL
};

static httpd_uri_t erase_network_data = {
    .uri = "/erase-network-data",
    .method = HTTP_GET,
    .handler = erase_network_data_handler,
    .user_ctx = NULL
};

static httpd_uri_t set_led_count = {
    .uri = "/set-led-count",
    .method = HTTP_GET,
    .handler = set_led_count_handler,
    .user_ctx = NULL
};

static httpd_uri_t get_settings = {
    .uri = "/get-settings",
    .method = HTTP_GET,
    .handler = get_settings_handler,
    .user_ctx = NULL
};

static httpd_uri_t restart = {
    .uri = "/restart",
    .method = HTTP_GET,
    .handler = restart_handler,
    .user_ctx = NULL
};

void init_spiffs(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format SPIFFS");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "SPIFFS partition not found");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS Partition size: total: %d, used: %d", total, used);
    }

    ESP_LOGI(TAG, "Listing files in SPIFFS:");
    DIR* dir = opendir("/spiffs");
    if (dir != NULL) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            ESP_LOGI(TAG, " - %s", ent->d_name);
        }
        closedir(dir);
    } else {
        ESP_LOGE(TAG, "Failed to open directory.");
    }
}

void start_webserver(void) {
    static bool led_initialized = false;
    if (!led_initialized) {
        led_strip_init();
        led_strip_load_parameters();
        led_initialized = true;
    }
    led_mutex = xSemaphoreCreateMutex();
    if (led_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create led_mutex");
        return;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 40960;
    config.max_uri_handlers = 24;
    config.server_port = 80;
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &led_on);
        httpd_register_uri_handler(server, &led_off);
        httpd_register_uri_handler(server, &wave_effect);
        httpd_register_uri_handler(server, &stairs_effect);
        httpd_register_uri_handler(server, &toggle_wave_direction);
        httpd_register_uri_handler(server, &set_brightness);
        httpd_register_uri_handler(server, &set_stairs_speed);
        httpd_register_uri_handler(server, &set_stairs_group_size);
        httpd_register_uri_handler(server, &set_color);
        httpd_register_uri_handler(server, &reset_to_rgb);
        httpd_register_uri_handler(server, &save_parameters);
        httpd_register_uri_handler(server, &erase_network_data);
        httpd_register_uri_handler(server, &set_led_count);
        httpd_register_uri_handler(server, &get_settings);
        httpd_register_uri_handler(server, &restart);
        httpd_register_uri_handler(server, &favicon);
        httpd_register_uri_handler(server, &toggle_ignore_sun_uri);


        static httpd_uri_t spiffs_uri = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = spiffs_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &spiffs_uri);
    } else {
        ESP_LOGE(TAG, "Error starting server!");
        vSemaphoreDelete(led_mutex);
    }
}

void stop_webserver(void) {
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
    if (led_mutex) {
        vSemaphoreDelete(led_mutex);
        led_mutex = NULL;
    }
}
