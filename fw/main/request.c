#include "main.h"

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"

static const char *TAG = "HTTP client";

#define min(_a, _b) ((_a) < (_b) ? (_a) : (_b))

// ============ "Simple request" ============
// Fixed functionality: only cares about GET requests, plaintext responses, and Set-Cookie

// Saves response buffer in `evt->user_data`
// If `Set-Cookie` header is present, its header line is prepended to the payload
// NOTE: Non-reentrant
#define SIMPLE_BUFFER_SIZE 4096
static esp_err_t simple_http_event_handler(esp_http_client_event_t *evt)
{
  static int output_ptr;
  switch (evt->event_id) {
    case HTTP_EVENT_ERROR: break;
    case HTTP_EVENT_ON_CONNECTED: break;
    case HTTP_EVENT_HEADER_SENT: break;
    case HTTP_EVENT_ON_HEADER:
      if (strcmp(evt->header_key, "Set-Cookie") == 0) {
        output_ptr += snprintf(
          evt->user_data + output_ptr,
          SIMPLE_BUFFER_SIZE - output_ptr,
          "Set-Cookie: %s;", evt->header_value);
      }
      break;
    case HTTP_EVENT_ON_DATA:
      int copy_len = min(evt->data_len, SIMPLE_BUFFER_SIZE - output_ptr);
      memcpy(evt->user_data + output_ptr, evt->data, copy_len);
      output_ptr += copy_len;
      break;
    case HTTP_EVENT_ON_FINISH:
      output_ptr = 0;
      break;
    case HTTP_EVENT_DISCONNECTED:
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

  static char local_response_buffer[SIMPLE_BUFFER_SIZE];

  esp_err_t err = ESP_OK;

  for (int att = 0; att < 3; att++) {
    esp_http_client_handle_t client = esp_http_client_init(&(esp_http_client_config_t){
      .url = url,
      .auth_type = HTTP_AUTH_TYPE_NONE,
      .transport_type = HTTP_TRANSPORT_OVER_SSL,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .event_handler = simple_http_event_handler,
      .method = HTTP_METHOD_GET,
      .timeout_ms = 3000,
      .user_data = local_response_buffer,
    });

    if (cookies != NULL)
      esp_http_client_set_header(client, "Cookie", cookies);

    err = esp_http_client_perform(client);

    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_http_client_perform() failed with error %d", (int)err);
      goto continue_retry;
    }

    int64_t len = esp_http_client_get_content_length(client);
    if (len > SIMPLE_BUFFER_SIZE - 1) {
      ESP_LOGI(TAG, "Content length too large (%"PRId64" bytes), truncated", len);
      len = SIMPLE_BUFFER_SIZE - 1;
    }

    local_response_buffer[len] = '\0';
    ESP_LOGI(TAG, "Response %s", local_response_buffer);

  continue_retry:
    esp_http_client_cleanup(client);
    if (err == ESP_OK) break;

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  if (err != ESP_OK) return "\x7f(error)";
  return local_response_buffer;
}

// ============ Streaming POST request ============

typedef struct post_handle_t {
  esp_http_client_handle_t client;
} post_handle_t;

post_handle_t *post_create()
{
  struct post_handle_t *p = malloc(sizeof(struct post_handle_t));

  p->client = esp_http_client_init(&(esp_http_client_config_t){
    // .url = "https://play.ayu.land/ya",
    .url = "http://45.63.5.138:24678/",
    .auth_type = HTTP_AUTH_TYPE_NONE,
    .transport_type = HTTP_TRANSPORT_OVER_SSL,
    .crt_bundle_attach = esp_crt_bundle_attach,
    .method = HTTP_METHOD_POST,
    .timeout_ms = 30000,
    .user_data = p,
  });

  return p;
}

void post_open(const post_handle_t *p)
{
  // Chunked encoding
  // Ref: esp-idf/components/esp_http_client/esp_http_client.c, `http_client_prepare_first_line()`
  esp_http_client_open(p->client, -1);
}

void post_write(const post_handle_t *p, const void *data, size_t len)
{
  ESP_LOGD(TAG, "writing request payload, %zu bytes", len);
  char s[16];
  int s_len = snprintf(s, sizeof s, "%zx\r\n", len);
  esp_http_client_write(p->client, s, s_len);
  esp_http_client_write(p->client, data, len);
  s[0] = '\r'; s[1] = '\n';
  esp_http_client_write(p->client, s, 2);
}

const char *post_finish(const post_handle_t *p)
{
  esp_http_client_write(p->client, "0\r\n\r\n", 5);
  static char buf[1024];
  int content_len = esp_http_client_fetch_headers(p->client);
  int read_len = esp_http_client_read(p->client, buf, (sizeof buf) - 1);
  esp_http_client_close(p->client);
  if (content_len == -1 || read_len < content_len) {
    ESP_LOGW(TAG, "did not receive complete response (content-length %d, read %d)", content_len, read_len);
    return NULL;
  }
  buf[read_len] = '\0';
  return buf;
}

int http_test()
{
  static const char *TAG = "HTTP(S) test";

  ESP_LOGI(TAG, "Testing network reachability");

  esp_err_t err;
  esp_http_client_handle_t client = esp_http_client_init(&(esp_http_client_config_t){
    .url = "https://ayu.land/",
    .auth_type = HTTP_AUTH_TYPE_NONE,
    .transport_type = HTTP_TRANSPORT_OVER_SSL,
    .crt_bundle_attach = esp_crt_bundle_attach,
    .method = HTTP_METHOD_GET,
    .timeout_ms = 30000,
  });

  err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Client open failed: %s", esp_err_to_name(err));
    return -1;
  }

  const int BUF_SIZE = 1024;
  char *buf = heap_caps_malloc(BUF_SIZE, MALLOC_CAP_SPIRAM);
  int content_len = esp_http_client_fetch_headers(client);
  int read_len = esp_http_client_read(client, buf, BUF_SIZE - 1);
  if (content_len == -1) {
    ESP_LOGW(TAG, "Did not receive complete response: %s", esp_err_to_name(err));

    esp_http_client_close(client);
    free(buf);
    return -2;
  }
  buf[read_len] = '\0';

  int status = esp_http_client_get_status_code(client);
  ESP_LOGI(TAG, "HTTP response code %d, content length %d (read %d):\n%s",
    status, content_len, read_len, buf);

  esp_http_client_close(client);
  free(buf);
  return 1;
}
