#include "http.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "led.h"
#include "wifi.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_system.h"
#include "mbedtls/base64.h" 

static const char *TAG = "http_server";
static httpd_handle_t server = NULL;

#define BASIC_AUTH_USERNAME "admin"
#define BASIC_AUTH_PASSWORD "password"

static char *http_auth_basic(const char *username, const char *password) {
    char *user_info = NULL;
    char *digest = NULL;
    size_t n = 0;
    size_t out_len = 0;
    int ret;

    size_t user_info_len = strlen(username) + 1 + strlen(password) + 1;
    user_info = malloc(user_info_len);
    if (!user_info) {
        ESP_LOGE(TAG, "Failed to allocate memory for user_info");
        return NULL;
    }
    sprintf(user_info, "%s:%s", username, password);

    ret = mbedtls_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));
    if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
        ESP_LOGE(TAG, "Failed to calculate base64 buffer size");
        free(user_info);
        return NULL;
    }

    digest = malloc(n + 1);
    if (!digest) {
        ESP_LOGE(TAG, "Failed to allocate memory for digest");
        free(user_info);
        return NULL;
    }

    ret = mbedtls_base64_encode((unsigned char *)digest, n, &out_len, (const unsigned char *)user_info, strlen(user_info));
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to encode base64");
        free(user_info);
        free(digest);
        return NULL;
    }
    digest[out_len] = '\0';

    free(user_info);
    return digest;
}

static esp_err_t basic_auth_get_handler(httpd_req_t *req) {
    char *buf = NULL;
    size_t buf_len = 0;
    char *auth_header = NULL;

    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK) {
            auth_header = buf;
        }
    }

    if (!auth_header || strncmp(auth_header, "Basic ", 6) != 0) {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Login Required\"");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
        if (buf) free(buf);
        return ESP_FAIL;
    } else {
        char *auth_credentials = auth_header + 6;
        char *expected_credentials = http_auth_basic(BASIC_AUTH_USERNAME, BASIC_AUTH_PASSWORD);
        if (!expected_credentials) {
            if (buf) free(buf);
            return ESP_FAIL;
        }
        if (strcmp(auth_credentials, expected_credentials) != 0) {
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Login Required\"");
            httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
            free(expected_credentials);
            if (buf) free(buf);
            return ESP_FAIL;
        }
        free(expected_credentials);
    }
    if (buf) free(buf);
    return ESP_OK;
}

static esp_err_t root_get_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }

    static const char resp[] = "<!DOCTYPE html>"
                               "<html lang='en'>"
                               "<head>"
                               "<meta charset='UTF-8'>"
                               "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                               "<title>LED Controller</title>"
                               "<link href='https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700&family=Roboto+Mono:wght@400;700&display=swap' rel='stylesheet'>"
                               "<style>"
                               "body {"
                               "    font-family: 'Orbitron', sans-serif;"
                               "    background-color: #1c1c1c;"
                               "    color: #e0e0e0;"
                               "    margin: 0;"
                               "    padding: 0;"
                               "    display: flex;"
                               "    flex-direction: column;"
                               "    align-items: center;"
                               "    justify-content: center;"
                               "    min-height: 100vh;"
                               "}"
                               ".container {"
                               "    text-align: center;"
                               "    background-color: rgba(28, 28, 28, 0.95);"
                               "    padding: 40px;"
                               "    border-radius: 12px;"
                               "    box-shadow: 0 0 20px rgba(0, 0, 0, 0.7);"
                               "    max-width: 800px;"
                               "    width: 90%;"
                               "}"
                               "h1 {"
                               "    color: #00ffcc;"
                               "    font-size: 2.5rem;"
                               "    margin-bottom: 30px;"
                               "    text-shadow: 0 0 10px #00ffcc;"
                               "}"
                               ".button-group {"
                               "    display: flex;"
                               "    flex-wrap: wrap;"
                               "    justify-content: center;"
                               "    gap: 15px;"
                               "    margin-bottom: 30px;"
                               "}"
                               "button {"
                               "    padding: 15px 25px;"
                               "    font-size: 1rem;"
                               "    border: 2px solid #00ffcc;"
                               "    background-color: #333333;"
                               "    color: #00ffcc;"
                               "    cursor: pointer;"
                               "    border-radius: 8px;"
                               "    transition: background-color 0.3s, box-shadow 0.3s, transform 0.2s;"
                               "    box-shadow: 0 4px 6px rgba(0, 0, 0, 0.3);"
                               "    flex: 1 1 200px;"
                               "    max-width: 250px;"
                               "}"
                               "button:hover {"
                               "    background-color: #00ffcc;"
                               "    color: #1c1c1c;"
                               "    box-shadow: 0 6px 12px rgba(0, 255, 204, 0.5);"
                               "    transform: scale(1.05);"
                               "}"
                               ".controls {"
                               "    display: flex;"
                               "    flex-direction: column;"
                               "    align-items: center;"
                               "    gap: 20px;"
                               "    margin-bottom: 30px;"
                               "}"
                               ".control-row {"
                               "    display: flex;"
                               "    flex-wrap: wrap;"
                               "    justify-content: center;"
                               "    align-items: center;"
                               "    gap: 20px;"
                               "}"
                               ".control-group {"
                               "    display: flex;"
                               "    flex-direction: column;"
                               "    align-items: center;"
                               "}"
                               ".group-buttons {"
                               "    display: flex;"
                               "    gap: 10px;"
                               "    margin-top: 10px;"
                               "}"
                               ".group-buttons button {"
                               "    padding: 10px 15px;"
                               "    flex: none;"
                               "    max-width: none;"
                               "}"
                               "label {"
                               "    font-size: 1.2rem;"
                               "    margin-bottom: 5px;"
                               "}"
                               "input[type='range'], input[type='color'] {"
                               "    width: 100%;"
                               "    max-width: 300px;"
                               "    padding: 5px;"
                               "}"
                               "input[type='range'] {"
                               "    -webkit-appearance: none;"
                               "    appearance: none;"
                               "    height: 10px;"
                               "    background: #444444;"
                               "    border-radius: 5px;"
                               "    outline: none;"
                               "    margin-bottom: 5px;"
                               "}"
                               "input[type='range']::-webkit-slider-thumb {"
                               "    -webkit-appearance: none;"
                               "    appearance: none;"
                               "    width: 20px;"
                               "    height: 20px;"
                               "    background: #00ffcc;"
                               "    cursor: pointer;"
                               "    border-radius: 50%;"
                               "    box-shadow: 0 0 5px #00ffcc;"
                               "}"
                               "input[type='range']::-moz-range-thumb {"
                               "    width: 20px;"
                               "    height: 20px;"
                               "    background: #00ffcc;"
                               "    cursor: pointer;"
                               "    border-radius: 50%;"
                               "    box-shadow: 0 0 5px #00ffcc;"
                               "}"
                               ".info {"
                               "    background-color: rgba(50, 50, 50, 0.8);"
                               "    padding: 20px;"
                               "    border-radius: 8px;"
                               "    box-shadow: 0 0 10px rgba(0, 0, 0, 0.5);"
                               "    font-family: 'Roboto Mono', monospace;"
                               "    font-size: 0.9rem;"
                               "    color: #00ffcc;"
                               "}"
                               ".value-display {"
                               "    font-size: 1.2rem;"
                               "    color: #00ffcc;"
                               "    margin-left: 10px;"
                               "}"
                               "</style>"
                               "</head>"
                               "<body>"
                               "<div class='container'>"
                               "<h1>LED Controller</h1>"
                               "<div class='button-group'>"
                               "    <button onclick=\"sendRequest('/led-on')\">Turn On LED Strip</button>"
                               "    <button onclick=\"sendRequest('/led-off')\">Turn Off LED Strip</button>"
                               "    <button onclick=\"sendRequest('/wave-effect')\">Start Wave Effect</button>"
                               "    <button onclick=\"sendRequest('/stairs-effect')\">Start Stairs Effect</button>"
                               "    <button onclick=\"sendRequest('/toggle-wave-direction')\">Toggle Wave Direction</button>"
                               "    <button onclick=\"sendRequest('/reset-to-rgb')\">Reset to RGB Mode</button>"
                               "    <button onclick=\"saveParameters()\">Save Parameters</button>"
                               "    <button onclick=\"eraseNetworkData()\">Erase Network Data</button>"
                               "</div>"
                               "<div class='controls'>"
                               "    <div class='control-row'>"
                               "        <div class='control-group'>"
                               "            <label for='brightnessSlider'>Brightness: <span id='brightnessValue'>100</span></label>"
                               "            <input type='range' min='0' max='100' value='100' id='brightnessSlider' oninput='updateBrightness(this.value)'>"
                               "        </div>"
                               "        <div class='control-group'>"
                               "            <label for='stairsSpeedSlider'>Stairs Effect Speed (ms): <span id='stairsSpeedValue'>100</span></label>"
                               "            <input type='range' min='10' max='1000' value='100' id='stairsSpeedSlider' oninput='updateStairsSpeed(this.value)'>"
                               "        </div>"
                               "    </div>"
                               "    <div class='control-group'>"
                               "        <label for='colorPicker'>Color:</label>"
                               "        <input type='color' id='colorPicker' onchange='updateColor(this.value)'>"
                               "    </div>"
                               "    <div class='control-group'>"
                               "        <label>Stairs Effect Group Size:</label>"
                               "        <div class='group-buttons'>"
                               "            <button onclick='updateStairsGroupSize(1)'>1</button>"
                               "            <button onclick='updateStairsGroupSize(2)'>2</button>"
                               "            <button onclick='updateStairsGroupSize(3)'>3</button>"
                               "            <button onclick='updateStairsGroupSize(4)'>4</button>"
                               "            <button onclick='updateStairsGroupSize(5)'>5</button>"
                               "        </div>"
                               "    </div>"
                               "</div>"
                               "<div class='info'>"
                               "    <p>Adjust the brightness, color, and speed of your LED strip using the controls above.</p>"
                               "    <p>Use the buttons to toggle effects and manage network settings.</p>"
                               "</div>"
                               "<script>"
                               "function sendRequest(url) {"
                               "  var xhttp = new XMLHttpRequest();"
                               "  xhttp.onreadystatechange = function() {"
                               "    if (this.readyState == 4 && this.status == 200) {"
                               "      console.log(this.responseText);"
                               "    }"
                               "  };"
                               "  xhttp.open('GET', url, true);"
                               "  xhttp.setRequestHeader('Authorization', 'Basic ' + btoa('" BASIC_AUTH_USERNAME ":" BASIC_AUTH_PASSWORD "'));"
                               "  xhttp.send();"
                               "}"
                               "function updateBrightness(value) {"
                               "  document.getElementById('brightnessValue').innerText = value;"
                               "  var xhttp = new XMLHttpRequest();"
                               "  xhttp.onreadystatechange = function() {"
                               "    if (this.readyState == 4 && this.status == 200) {"
                               "      console.log(this.responseText);"
                               "    }"
                               "  };"
                               "  xhttp.open('GET', '/set-brightness?value=' + value, true);"
                               "  xhttp.setRequestHeader('Authorization', 'Basic ' + btoa('" BASIC_AUTH_USERNAME ":" BASIC_AUTH_PASSWORD "'));"
                               "  xhttp.send();"
                               "}"
                               "function updateStairsSpeed(value) {"
                               "  document.getElementById('stairsSpeedValue').innerText = value;"
                               "  if (value < 10) value = 10;"
                               "  if (value > 1000) value = 1000;"
                               "  var xhttp = new XMLHttpRequest();"
                               "  xhttp.onreadystatechange = function() {"
                               "    if (this.readyState == 4 && this.status == 200) {"
                               "      console.log(this.responseText);"
                               "    }"
                               "  };"
                               "  xhttp.open('GET', '/set-stairs-speed?value=' + value, true);"
                               "  xhttp.setRequestHeader('Authorization', 'Basic ' + btoa('" BASIC_AUTH_USERNAME ":" BASIC_AUTH_PASSWORD "'));"
                               "  xhttp.send();"
                               "}"
                               "function updateColor(value) {"
                               "  var r = parseInt(value.substr(1, 2), 16);"
                               "  var g = parseInt(value.substr(3, 2), 16);"
                               "  var b = parseInt(value.substr(5, 2), 16);"
                               "  var xhttp = new XMLHttpRequest();"
                               "  xhttp.onreadystatechange = function() {"
                               "    if (this.readyState == 4 && this.status == 200) {"
                               "      console.log(this.responseText);"
                               "    }"
                               "  };"
                               "  xhttp.open('GET', '/set-color?r=' + r + '&g=' + g + '&b=' + b, true);"
                               "  xhttp.setRequestHeader('Authorization', 'Basic ' + btoa('" BASIC_AUTH_USERNAME ":" BASIC_AUTH_PASSWORD "'));"
                               "  xhttp.send();"
                               "}"
                               "function updateStairsGroupSize(value) {"
                               "  var buttons = document.querySelectorAll('.group-buttons button');"
                               "  buttons.forEach(function(btn) {"
                               "    btn.style.backgroundColor = '#333333';"
                               "    btn.style.color = '#00ffcc';"
                               "  });"
                               "  buttons[value - 1].style.backgroundColor = '#00ffcc';"
                               "  buttons[value - 1].style.color = '#1c1c1c';"
                               "  var xhttp = new XMLHttpRequest();"
                               "  xhttp.onreadystatechange = function() {"
                               "    if (this.readyState == 4 && this.status == 200) {"
                               "      console.log(this.responseText);"
                               "    }"
                               "  };"
                               "  xhttp.open('GET', '/set-stairs-group-size?value=' + value, true);"
                               "  xhttp.setRequestHeader('Authorization', 'Basic ' + btoa('" BASIC_AUTH_USERNAME ":" BASIC_AUTH_PASSWORD "'));"
                               "  xhttp.send();"
                               "}"
                               "function saveParameters() {"
                               "  var xhttp = new XMLHttpRequest();"
                               "  xhttp.onreadystatechange = function() {"
                               "    if (this.readyState == 4 && this.status == 200) {"
                               "      console.log(this.responseText);"
                               "      alert(this.responseText);"
                               "    }"
                               "  };"
                               "  xhttp.open('GET', '/save-parameters', true);"
                               "  xhttp.setRequestHeader('Authorization', 'Basic ' + btoa('" BASIC_AUTH_USERNAME ":" BASIC_AUTH_PASSWORD "'));"
                               "  xhttp.send();"
                               "}"
                               "function eraseNetworkData() {"
                               "  if (confirm('Are you sure you want to erase network data?')) {"
                               "    var xhttp = new XMLHttpRequest();"
                               "    xhttp.onreadystatechange = function() {"
                               "      if (this.readyState == 4 && this.status == 200) {"
                               "        alert(this.responseText);"
                               "      }"
                               "    };"
                               "    xhttp.open('GET', '/erase-network-data', true);"
                               "    xhttp.setRequestHeader('Authorization', 'Basic ' + btoa('" BASIC_AUTH_USERNAME ":" BASIC_AUTH_PASSWORD "'));"
                               "    xhttp.send();"
                               "  }"
                               "}"
                               "// Ініціалізація значень при завантаженні сторінки"
                               "window.onload = function() {"
                               "  updateStairsGroupSize(1);"
                               "};"
                               "</script>"
                               "</div>"
                               "</body></html>";

    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}



static esp_err_t led_on_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    led_strip_start();
    httpd_resp_send(req, "LED Strip Turned On", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t led_off_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    led_strip_stop_effect();
    led_strip_stop();
    httpd_resp_send(req, "LED Strip and Effects Turned Off", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t wave_effect_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    led_strip_wave_effect();
    httpd_resp_send(req, "Wave Effect Started", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t stairs_effect_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    led_strip_stairs_effect();
    httpd_resp_send(req, "Stairs Effect Started", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t toggle_wave_direction_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    led_strip_toggle_wave_direction();
    httpd_resp_send(req, "Wave Direction Toggled", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t set_brightness_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    char buf[10];
    int brightness = 100;
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[6];
        if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
            brightness = atoi(param);
        }
    }
    led_strip_set_brightness((uint8_t)brightness);
    httpd_resp_send(req, "Brightness Set", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t set_stairs_speed_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    char buf[10];
    int speed = 100;
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[6];
        if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
            speed = atoi(param);
            if (speed < 10) speed = 10;
            if (speed > 1000) speed = 1000;
            led_strip_set_stairs_speed((uint16_t)speed);
            httpd_resp_send(req, "Stairs Speed Set", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
    }
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Request");
    return ESP_FAIL;
}

static esp_err_t set_stairs_group_size_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    char buf[10];
    int size = 1;
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[6];
        if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
            size = atoi(param);
            if (size < 1) size = 1;
            if (size > LED_STRIP_LEN) size = LED_STRIP_LEN;
            led_strip_set_stairs_group_size((uint16_t)size);
            httpd_resp_send(req, "Stairs Group Size Set", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
    }
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Request");
    return ESP_FAIL;
}

static esp_err_t set_color_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    char buf[100];
    int r = 255, g = 255, b = 255;
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[4];
        if (httpd_query_key_value(buf, "r", param, sizeof(param)) == ESP_OK) {
            r = atoi(param);
        }
        if (httpd_query_key_value(buf, "g", param, sizeof(param)) == ESP_OK) {
            g = atoi(param);
        }
        if (httpd_query_key_value(buf, "b", param, sizeof(param)) == ESP_OK) {
            b = atoi(param);
        }
    }
    led_strip_set_color((uint8_t)r, (uint8_t)g, (uint8_t)b);
    httpd_resp_send(req, "Color Set", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t reset_to_rgb_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    led_strip_reset_to_rgb();
    httpd_resp_send(req, "RGB Mode Restored", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t motion_detected_1_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    led_strip_motion_effect_1();
    httpd_resp_send(req, "Motion 1 Effect Started", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t motion_detected_2_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    led_strip_motion_effect_2();
    httpd_resp_send(req, "Motion 2 Effect Started", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t save_parameters_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    led_strip_save_parameters();
    uint8_t brightness = led_strip_get_brightness();
    uint8_t r, g, b;
    led_strip_get_color(&r, &g, &b);
    bool custom_color = led_strip_get_custom_color_mode();
    uint16_t stairs_speed = led_strip_get_stairs_speed();
    uint16_t stairs_group_size = led_strip_get_stairs_group_size();
    char resp[200];
    snprintf(resp, sizeof(resp), "Parameters Saved:\nBrightness: %d\nColor Mode: %s\nR:%d G:%d B:%d\nStairs Speed: %d ms\nStairs Group Size: %d",
            brightness, custom_color ? "Custom" : "RGB", r, g, b, stairs_speed, stairs_group_size);
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


static esp_err_t erase_network_data_handler(httpd_req_t *req) {
    if (basic_auth_get_handler(req) != ESP_OK) {
        return ESP_FAIL;
    }
    erase_wifi_config();
    httpd_resp_send(req, "Network data erased. Restarting...", HTTPD_RESP_USE_STRLEN);
    esp_restart();
    return ESP_OK;
}

static httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler,
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

static httpd_uri_t motion_detected_1 = {
    .uri = "/motion-detected-1",
    .method = HTTP_GET,
    .handler = motion_detected_1_handler,
    .user_ctx = NULL
};

static httpd_uri_t motion_detected_2 = {
    .uri = "/motion-detected-2",
    .method = HTTP_GET,
    .handler = motion_detected_2_handler,
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

void start_webserver(void) {
    static bool led_initialized = false;
    if (!led_initialized) {
        led_strip_init();
        led_strip_load_parameters();
        led_initialized = true;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 20480;
    config.max_uri_handlers = 23;
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &led_on);
        httpd_register_uri_handler(server, &led_off);
        httpd_register_uri_handler(server, &wave_effect);
        httpd_register_uri_handler(server, &stairs_effect);
        httpd_register_uri_handler(server, &toggle_wave_direction);
        httpd_register_uri_handler(server, &set_brightness);
        httpd_register_uri_handler(server, &set_color);
        httpd_register_uri_handler(server, &reset_to_rgb);
        httpd_register_uri_handler(server, &motion_detected_1);
        httpd_register_uri_handler(server, &motion_detected_2);
        httpd_register_uri_handler(server, &save_parameters);
        httpd_register_uri_handler(server, &erase_network_data);
        httpd_register_uri_handler(server, &set_stairs_speed);
        httpd_register_uri_handler(server, &set_stairs_group_size);
    } else {
        ESP_LOGI(TAG, "Error starting server!");
    }
}

void stop_webserver(void) {
    if (server) {
        httpd_stop(server);
    }
}