#include "http.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "led.h"

static const char *TAG = "http_server"; ///< Tag used for ESP logging
static httpd_handle_t server = NULL; ///< Handle for the HTTP server    

/**
 * @brief Handler for the root URI "/"
 * Serves the main HTML interface for LED control.
 * @param req HTTP request information.
 * @return ESP_OK on successful response sending, otherwise ESP_FAIL.
 */
static esp_err_t root_get_handler(httpd_req_t *req) {
    const char resp[] = "<html><head>"
                        "<style>"
                        "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif; background-color: #f5f5f7; color: #1d1d1f; margin: 0; padding: 0; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; }"
                        "h1 { color: #1d1d1f; font-size: 2.5rem; margin-bottom: 30px; }"
                        "button { padding: 15px 25px; font-size: 1rem; margin: 10px; border: none; background-color: #007aff; color: white; cursor: pointer; border-radius: 8px; transition: background-color 0.3s, box-shadow 0.3s; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); }"
                        "button:hover { background-color: #005fda; box-shadow: 0 6px 12px rgba(0, 0, 0, 0.2); }"
                        "input[type='range'], input[type='color'] { width: 300px; margin-top: 20px; }"
                        ".container { text-align: center; background-color: white; padding: 40px; border-radius: 12px; box-shadow: 0 8px 16px rgba(0, 0, 0, 0.15); }"
                        "</style>"
                        "</head><body>"
                        "<div class='container'>"
                        "<h1>LED Controller</h1>"
                        "<button onclick=\"sendRequest('/led-on')\">Turn On LED Strip</button><br><br>"
                        "<button onclick=\"sendRequest('/led-off')\">Turn Off LED Strip</button><br><br>"
                        "<button onclick=\"sendRequest('/wave-effect')\">Start Wave Effect</button><br><br>"
                        "<button onclick=\"sendRequest('/stairs-effect')\">Start Stairs Effect</button><br><br>"
                        "<button onclick=\"sendRequest('/toggle-wave-direction')\">Toggle Wave Direction</button><br><br>"
                        "<input type='range' min='0' max='100' value='100' id='brightnessSlider' oninput='updateBrightness(this.value)'><br><br>"
                        "<input type='color' id='colorPicker' onchange='updateColor(this.value)'><br><br>"
                        "<button onclick=\"sendRequest('/reset-to-rgb')\">Reset to RGB Mode</button><br><br>"
                        "<button onclick=\"sendRequest('/motion-detected-1')\">Motion Detected 1</button><br><br>"
                        "<button onclick=\"sendRequest('/motion-detected-2')\">Motion Detected 2</button><br><br>"

                        "<script>"
                        "function sendRequest(url) {"
                        "  var xhttp = new XMLHttpRequest();"
                        "  xhttp.onreadystatechange = function() {"
                        "    if (this.readyState == 4 && this.status == 200) {"
                        "      console.log(this.responseText);"
                        "    }"
                        "  };"
                        "  xhttp.open('GET', url, true);"
                        "  xhttp.send();"
                        "}"
                        "function updateBrightness(value) {"
                        "  var xhttp = new XMLHttpRequest();"
                        "  xhttp.onreadystatechange = function() {"
                        "    if (this.readyState == 4 && this.status == 200) {"
                        "      console.log(this.responseText);"
                        "    }"
                        "  };"
                        "  xhttp.open('GET', '/set-brightness?value=' + value, true);"
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
                        "  xhttp.send();"
                        "}"
                        "</script>"
                        "</div>"
                        "</body></html>";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief Handler for turning the LED strip on.
 * @param req HTTP request information.
 * @return ESP_OK on success, indicating that the response was successfully sent.
 */
static esp_err_t led_on_handler(httpd_req_t *req) {
    led_strip_start();
    httpd_resp_send(req, "LED Strip Turned On", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief Handler for turning the LED strip off.
 * @param req HTTP request information.
 * @return ESP_OK on success, indicating that the response was successfully sent.
 */
static esp_err_t led_off_handler(httpd_req_t *req) {
    led_strip_stop_effect();
    led_strip_stop();
    httpd_resp_send(req, "LED Strip and Effects Turned Off", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief Handler for starting the wave effect on the LED strip.
 * @param req HTTP request information.
 * @return ESP_OK on success, indicating that the response was successfully sent.
 */
static esp_err_t wave_effect_handler(httpd_req_t *req) {
    led_strip_wave_effect();
    httpd_resp_send(req, "Wave Effect Started", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief Handler for starting the stairs effect on the LED strip.
 * @param req HTTP request information.
 * @return ESP_OK on success, indicating that the response was successfully sent.
 */
static esp_err_t stairs_effect_handler(httpd_req_t *req) {
    led_strip_stairs_effect();
    httpd_resp_send(req, "Stairs Effect Started", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief Handler for toggling the wave direction of the LED strip.
 * @param req HTTP request information.
 * @return ESP_OK on success, indicating that the response was successfully sent.
 */
static esp_err_t toggle_wave_direction_handler(httpd_req_t *req) {
    led_strip_toggle_wave_direction();
    httpd_resp_send(req, "Wave Direction Toggled", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief Handler for setting the brightness of the LED strip.
 * Processes query parameters to adjust the brightness level.
 * @param req HTTP request information.
 * @return ESP_OK on success, indicating that the response was successfully sent.
 */
static esp_err_t set_brightness_handler(httpd_req_t *req) {
    char buf[10];
    int brightness = 100; // Default brightness

    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[6];
        if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
            brightness = atoi(param);
        }
    }

    led_strip_set_brightness(brightness);
    httpd_resp_send(req, "Brightness Set", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief Handler for setting the color of the LED strip.
 * Processes query parameters to adjust the color in RGB format.
 * @param req HTTP request information.
 * @return ESP_OK on success, indicating that the response was successfully sent.
 */
static esp_err_t set_color_handler(httpd_req_t *req) {
    char buf[100];
    int r = 255, g = 255, b = 255; // Default to white color

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

    led_strip_set_color(r, g, b);
    httpd_resp_send(req, "Color Set", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief Handler for resetting to RGB mode on the LED strip.
 * @param req HTTP request information.
 * @return ESP_OK on success, indicating that the response was successfully sent.
 */
static esp_err_t reset_to_rgb_handler(httpd_req_t *req) {
    led_strip_reset_to_rgb();
    httpd_resp_send(req, "RGB Mode Restored", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief Handler for simulating motion detected effect 1.
 * Activates a predefined effect to simulate a motion detection event.
 * @param req HTTP request information.
 * @return ESP_OK on success, indicating that the response was successfully sent.
 */
static esp_err_t motion_detected_1_handler(httpd_req_t *req) {
    led_strip_motion_effect_1();
    httpd_resp_send(req, "Motion 1 Effect Started", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief Handler for simulating motion detected effect 2.
 * Activates a different predefined effect to simulate another motion detection event.
 * @param req HTTP request information.
 * @return ESP_OK on success, indicating that the response was successfully sent.
 */
static esp_err_t motion_detected_2_handler(httpd_req_t *req) {
    led_strip_motion_effect_2();
    httpd_resp_send(req, "Motion 2 Effect Started", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


// URI registration for handlers
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

static httpd_uri_t empty_uri = {
    .uri = "/empty",
    .method = HTTP_GET,
    .handler = NULL,
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


/// @brief Starts the web server and registers URI handlers
void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;  // Adjust stack size as needed
    config.max_uri_handlers = 20;
    
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");

        // Register root handler
        httpd_register_uri_handler(server, &root);

        // Register other handlers
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
        httpd_register_uri_handler(server, &empty_uri);
    } else {
        ESP_LOGI(TAG, "Error starting server!");
    }
}


/// @brief Stops the web server
void stop_webserver(void) {
    if (server) {
        httpd_stop(server);
    }
}