#include "main.h"

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"

#define MAX_HTTP_OUTPUT_BUFFER 4096
#define TAG "HTTP client"

#define min(_a, _b) ((_a) < (_b) ? (_a) : (_b))

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
  static int output_ptr;
  switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
      ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
      break;
    case HTTP_EVENT_HEADER_SENT:
      ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
      break;
    case HTTP_EVENT_ON_HEADER:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
      // XXX: Do not include this for ordinary requests
      if (strcmp(evt->header_key, "Set-Cookie") == 0) {
        output_ptr += snprintf(
          evt->user_data + output_ptr,
          MAX_HTTP_OUTPUT_BUFFER - output_ptr,
          "Set-Cookie: %s;", evt->header_value);
      }
      break;
    case HTTP_EVENT_ON_DATA:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
      if (!esp_http_client_is_chunked_response(evt->client)) {
        int copy_len = min(evt->data_len, MAX_HTTP_OUTPUT_BUFFER - output_ptr);
        memcpy(evt->user_data + output_ptr, evt->data, copy_len);
        output_ptr += copy_len;
      }
      break;
    case HTTP_EVENT_ON_FINISH:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
      output_ptr = 0;
      break;
    case HTTP_EVENT_DISCONNECTED:
      ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
      int mbedtls_err = 0;
      esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
      if (err != 0) {
        ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
        ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
      }
      output_ptr = 0;
      break;
    case HTTP_EVENT_REDIRECT:
      // `HTTP_EVENT_REDIRECT` will not be dispatched if `disable_auto_redirect` is off
      // i.e. redirects automatically (see `esp-idf/components/esp_http_client/esp_http_client.c`)
      break;
  }
  return ESP_OK;
}

const char *simple_request(const char *url, const char *cookies)
{
  ESP_LOGI(TAG, "Simple GET request %s%s%s", url, cookies ? ", Cookie: " : "", cookies ? cookies : "");

  static char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER];

  esp_http_client_handle_t client = esp_http_client_init(&(esp_http_client_config_t){
    .url = url,
    .auth_type = HTTP_AUTH_TYPE_NONE,
    // .transport_type = (memcmp(url, "https:", 6) == 0) ? HTTP_TRANSPORT_OVER_SSL : HTTP_TRANSPORT_OVER_TCP,
    .transport_type = HTTP_TRANSPORT_OVER_SSL,
    .crt_bundle_attach = esp_crt_bundle_attach,
    .event_handler = http_event_handler,
    .user_data = local_response_buffer,
  });

  esp_http_client_set_method(client, HTTP_METHOD_GET);
  if (cookies != NULL)
    esp_http_client_set_header(client, "Cookie", cookies);

  esp_err_t err;
  err = esp_http_client_perform(client);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_http_client_perform() failed with error %d", (int)err);
    return "";
  }

  int64_t len = esp_http_client_get_content_length(client);
  if (len > MAX_HTTP_OUTPUT_BUFFER - 1) {
    ESP_LOGI(TAG, "Content length too large (%"PRId64" bytes), truncated", len);
    len = MAX_HTTP_OUTPUT_BUFFER - 1;
  }

  local_response_buffer[len] = '\0';
  ESP_LOGI(TAG, "Response %s", local_response_buffer);
  return local_response_buffer;
}

void http_get_task(void *_unused)
{
  static char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER];

  esp_err_t err;
  esp_http_client_handle_t client = esp_http_client_init(&(esp_http_client_config_t){
    .url = "https://ayu.land/",
    .auth_type = HTTP_AUTH_TYPE_NONE,
    .transport_type = HTTP_TRANSPORT_OVER_SSL,
    .crt_bundle_attach = esp_crt_bundle_attach,
    .event_handler = http_event_handler,
    .user_data = local_response_buffer,
  });

  esp_http_client_set_timeout_ms(client, 30000);

  while (1) {
    ESP_LOGI(TAG, "HTTP GET sending");

    static bool parity = 0;
    esp_http_client_set_url(client,
      (parity ^= 1) ? "https://www.howsmyssl.com/a/check" : "http://ayu.land"),
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    err = esp_http_client_perform(client);

    if (err == ESP_OK) {
      int64_t len = esp_http_client_get_content_length(client);
      ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
              esp_http_client_get_status_code(client),
              len);
      ESP_LOG_BUFFER_HEX(TAG, local_response_buffer, min(len, MAX_HTTP_OUTPUT_BUFFER));
    } else {
      ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }

  return;
}
