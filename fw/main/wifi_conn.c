#include "main.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_eap_client.h"
#include <string.h>

#include "wifi_cred.h"
// EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS or
// EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_EAP_USER, EXAMPLE_ESP_WIFI_EAP_PASS
#if defined(EXAMPLE_ESP_WIFI_EAP_USER) && defined(EXAMPLE_ESP_WIFI_EAP_PASS)
  #define PEAP 1
  #ifndef EXAMPLE_ESP_WIFI_EAP_ANON
  #define EXAMPLE_ESP_WIFI_EAP_ANON "user@tsinghua.edu.cn"
  #endif
#else
  #define PEAP 0
#endif
#if !defined(EXAMPLE_ESP_WIFI_SSID) || (!defined(EXAMPLE_ESP_WIFI_PASS) && !PEAP)
  #error "Define EXAMPLE_ESP_WIFI_SSID and EXAMPLE_ESP_WIFI_PASS in wifi_cred.h"
#endif

#define EXAMPLE_ESP_MAXIMUM_RETRY  5

#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER "PASSWORD IDENTIFIER"
#if PEAP
  #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_ENTERPRISE
#else
  #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#endif

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define TAG "Wi-Fi STA"

static int s_retry_num = 0;


static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
    ESP_LOGI(TAG, "starting");
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG, "disconnected from AP");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

extern uint8_t ca_pem_start[] asm("_binary_ca_pem_start");
extern uint8_t ca_pem_end[]   asm("_binary_ca_pem_end");

void wifi_init_sta(void)
{
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  uint8_t mac[6];
  esp_err_t mac_result = esp_wifi_get_mac(WIFI_IF_STA, mac);
  if (mac_result == ESP_OK) {
    ESP_LOGI(TAG, "MAC address %02x:%02x:%02x:%02x:%02x:%02x",
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  } else {
    ESP_LOGI(TAG, "Cannot get MAC address");
  }

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &event_handler,
                                                      NULL,
                                                      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &event_handler,
                                                      NULL,
                                                      &instance_got_ip));

  esp_log_level_set("wifi", ESP_LOG_VERBOSE);
  esp_log_level_set("wpa", ESP_LOG_VERBOSE);
  esp_log_level_set("eap", ESP_LOG_VERBOSE);

  wifi_config_t wifi_config = {
    .sta = {
      .ssid = EXAMPLE_ESP_WIFI_SSID,
      /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
       * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
       * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
       * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
       */
    #if !PEAP
      .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
      .password = EXAMPLE_ESP_WIFI_PASS,
      .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
      .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
    #endif
    },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

#if PEAP
  ESP_ERROR_CHECK(esp_eap_client_set_ca_cert(ca_pem_start, ca_pem_end - ca_pem_start));
  ESP_ERROR_CHECK(esp_eap_client_set_identity((uint8_t *)EXAMPLE_ESP_WIFI_EAP_ANON, strlen(EXAMPLE_ESP_WIFI_EAP_ANON)));
  ESP_ERROR_CHECK(esp_eap_client_set_username((uint8_t *)EXAMPLE_ESP_WIFI_EAP_USER, strlen(EXAMPLE_ESP_WIFI_EAP_USER)));
  ESP_ERROR_CHECK(esp_eap_client_set_password((uint8_t *)EXAMPLE_ESP_WIFI_EAP_PASS, strlen(EXAMPLE_ESP_WIFI_EAP_PASS)));
  ESP_ERROR_CHECK(esp_wifi_sta_enterprise_enable());
#endif

  ESP_ERROR_CHECK(esp_wifi_start() );

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
   * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(
    s_wifi_event_group,
    WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
    pdFALSE,
    pdFALSE,
    portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s", EXAMPLE_ESP_WIFI_SSID);
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(TAG, "Failed to connect to SSID:%s", EXAMPLE_ESP_WIFI_SSID);
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }
}
