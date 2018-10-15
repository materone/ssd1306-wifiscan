#include "esp_log.h"
#include "fonts.h"
#include "ssd1306.hpp"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <cstdio>
#include <string>
#include <sstream>
#include <iostream>

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

//for sntp
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_deep_sleep.h"

#include "lwip/err.h"
#include "lwip/dns.h"
#include "apps/sntp/sntp.h"

//wifi pass define
// #define STA_NAME   "iCoolDog"
// #define WIFI_PASS "pass"
#include "wifipass.h"

using namespace std;


OLED oled = OLED(GPIO_NUM_18, GPIO_NUM_19, SSD1306_128x64);

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;
const int SCANDONE_BIT = BIT1;


static const char *TAG = "OLED-WiFi-RTC";

static const char  STAS[][32] = {"A","C"};
static const char  PASSS[][64] = {"B","D"};

int staidx = 0;

/* Variable holding number of times ESP32 restarted since first boot.
 * It is placed into RTC memory using RTC_DATA_ATTR and
 * maintains its value when ESP32 wakes from deep sleep.
 */
static RTC_DATA_ATTR int boot_count ;
void RTC_IRAM_ATTR esp_wake_deep_sleep(void){
   esp_default_wake_deep_sleep();
   static RTC_RODATA_ATTR const char fmt_str[] = "Wake count: %d\n";
   ets_printf(fmt_str,boot_count);
   boot_count++;
}

// static void obtain_time(void);
// static void initialize_sntp(void);
// static void initialise_wifi(void);
// static esp_err_t event_handler(void *ctx, system_event_t *event);


void myTask(void *pvParameters) {
	adc1_config_width(ADC_WIDTH_12Bit);
	adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_0db);
	// bool flag=true;

	ostringstream os;
//	char *data = (char*) malloc(32);
	uint16_t graph[128];
	memset(graph, 0, sizeof(graph));
//	for(uint8_t i=0;i<64;i++){
//		oled.clear();
//		oled.draw_hline(0, i, 100, WHITE);
//		sprintf(data,"%d",i);
//		oled.draw_string(105, 10, string(data), BLACK, WHITE);
//		oled.refresh(false);
//		vTaskDelay(1000 / portTICK_PERIOD_MS);
//	}
	while (1) {
		os.str("");
		os << "ADC_CH4(GPIO32):" << adc1_get_voltage(ADC1_CHANNEL_4);
		for (int i = 0; i < 127; i++) {
			graph[i] = graph[i + 1];
		}
		graph[127] = adc1_get_voltage(ADC1_CHANNEL_4);
		oled.clear();
//		sprintf(data, "01");
//		oled.select_font(2);
//		oled.draw_string(0, 0, string(data), WHITE, BLACK);
//		sprintf(data, ":%d", graph[127]);
		oled.select_font(1);
		oled.draw_string(0, 0, os.str(), WHITE, BLACK);
//		oled.draw_string(33, 4, string(data), WHITE, BLACK);
		graph[127] = graph[127] * 48 / 4096;
		for (uint8_t i = 0; i < 128; i++) {
			oled.draw_pixel(i, 63 - graph[i], WHITE);
		}
		oled.draw_pixel(127, 63 - graph[127], WHITE);
		oled.refresh(true);
		vTaskDelay(5000 / portTICK_PERIOD_MS);
		// flag=~flag;
		// oled.invert_display(flag);
	}

}

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t event_handler_scan(void *ctx, system_event_t *event)
{
	// ostringstream os;
   if (event->event_id == SYSTEM_EVENT_SCAN_DONE) {
      uint16_t apCount = 0;
      bool flag = false;
      esp_wifi_scan_get_ap_num(&apCount);
      printf("Number of access points found: %d\n",event->event_info.scan_done.number);
      if (apCount == 0) {
         return ESP_OK;
      }
      wifi_ap_record_t *list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
      ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, list));
      int i;
      printf("======================================================================\n");
      printf("             SSID             |    RSSI    |           AUTH           \n");
      printf("======================================================================\n");
      for (i=0; i<apCount; i++) {
         char *authmode;
         switch(list[i].authmode) {
            case WIFI_AUTH_OPEN:
               authmode = (char *)"WIFI_AUTH_OPEN";
               break;
            case WIFI_AUTH_WEP:
               authmode = (char *)"WIFI_AUTH_WEP";
               break;
            case WIFI_AUTH_WPA_PSK:
               authmode = (char *)"WIFI_AUTH_WPA_PSK";
               break;
            case WIFI_AUTH_WPA2_PSK:
               authmode = (char *)"WIFI_AUTH_WPA2_PSK";
               break;
            case WIFI_AUTH_WPA_WPA2_PSK:
               authmode = (char *)"WIFI_AUTH_WPA_WPA2_PSK";
               break;
            default:
               authmode = (char *)"Unknown";
               break;
         }
         printf("%26.26s    |    % 4d    |    %22.22s\n",list[i].ssid, list[i].rssi, authmode);
         for (int j = 0; j < sizeof(STAS)/sizeof(STAS[0]); ++j)
         {
            if(strcmp((const char *)(list[i].ssid),STAS[j]) == 0){
              flag = true;
              staidx = j;
              break;
           }
         }
         
         // os.str("");
         // os << list[i].ssid;
         // oled.fill_rectangle(1,51,126,11,WHITE);
         // oled.select_font(1);
         // oled.draw_string(1,51,os.str(),BLACK,WHITE);
         // oled.refresh(true);
         // vTaskDelay(2000/portTICK_PERIOD_MS);
      }

     if (flag){
       ESP_LOGI(TAG,"Find Config AP %s",STAS[staidx]);
     }
    free(list);
    xEventGroupSetBits(wifi_event_group, SCANDONE_BIT);
   }
   return ESP_OK;
}


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
      case SYSTEM_EVENT_STA_START:
          esp_wifi_connect();
          break;
      case SYSTEM_EVENT_STA_GOT_IP:
          xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
          xEventGroupClearBits(wifi_event_group, SCANDONE_BIT);
          break;
      case SYSTEM_EVENT_STA_DISCONNECTED:
          /* This is a workaround as ESP32 WiFi libs don't currently
             auto-reassociate. */
          esp_wifi_connect();
          xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
          break;
      default:
          break;
    }
    return ESP_OK;
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, (char *)"0.pool.ntp.org");//45.76.98.188 pool.ntp.org 192.168.78.51
    sntp_setservername(1, (char *)"1.pool.ntp.org");//45.76.98.188 pool.ntp.org 192.168.78.51
    sntp_setservername(2, (char *)"2.pool.ntp.org");//45.76.98.188 pool.ntp.org 192.168.78.51
    sntp_setservername(3, (char *)"3.pool.ntp.org");//45.76.98.188 pool.ntp.org 192.168.78.51
    sntp_init();
}

static void initialise_wifi(void)
{
   tcpip_adapter_init();
   wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
   ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
   ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
   wifi_config_t wifi_config ;
   // wifi_config.sta.ssid = STAS[staidx];
   // wifi_config.sta.password = PASSS[staidx];
   memcpy(wifi_config.sta.ssid,STAS[staidx],sizeof(uint8_t)*32);
   memcpy(wifi_config.sta.password,PASSS[staidx],sizeof(uint8_t)*64);
   wifi_config.sta.bssid_set = false;

   ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
   oled.draw_string(55, 41, (char *)wifi_config.sta.ssid, WHITE, BLACK);
   oled.refresh(false);
   ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
   ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
   ESP_ERROR_CHECK( esp_wifi_start() );

   tcpip_adapter_ip_info_t ip_info;
   ip4_addr_t ip = {ipaddr_addr("192.168.199.123")};
   ip4_addr_t netmask = {ipaddr_addr("255.255.255.0")};
   ip4_addr_t gw = {ipaddr_addr("192.168.199.1")};
   ip_info.ip = ip;
   ip_info.netmask = netmask;
   ip_info.gw = gw;

   tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
   ESP_ERROR_CHECK( esp_wifi_connect() );
   tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
   //set dns
   ip_addr_t dns ;
   ipaddr_aton("114.114.114.114",&dns);// 8.8.8.8 192.168.70.21 114.114.114.114
   dns_setserver(0,&dns);
}

static void obtain_time(void)
{
    // ESP_ERROR_CHECK( nvs_flash_init() );
    initialise_wifi();
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0,0,0,0,0,0,0,0,0};
    int retry = 0;
    const int retry_count = 60;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    ESP_ERROR_CHECK( esp_wifi_stop() );
}

void ntpc()
{
    // ++boot_count;
    ESP_LOGI(TAG, "Boot count: %d", boot_count);

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
    char strftime_buf[64];

    // Set timezone to Eastern Standard Time and print local time
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in New York is: %s", strftime_buf);
    bool once = true;

    // Set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();
    while(1){
      time(&now);
      localtime_r(&now, &timeinfo);
      strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);//%c for common //%y%m%d %T
      if(once){
         ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
         once = false;
      }

    // const int deep_sleep_sec = 10;
    // ESP_LOGI(TAG, "Entering deep sleep for %d seconds", deep_sleep_sec);
    //esp_deep_sleep(1000000LL * deep_sleep_sec);
      oled.fill_rectangle(1,52,126,11,BLACK);
      oled.select_font(1);
      oled.draw_string(8,52,strftime_buf,WHITE,BLACK);
      oled.refresh(false);
      vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

void app_main() {
	nvs_flash_init();
  // system_init();
  //wifi scan
  tcpip_adapter_init();

  wifi_event_group = xEventGroupCreate();
  
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler_scan, NULL));
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  // Let us test a WiFi scan ...
  wifi_scan_config_t scanConf = {
    .ssid = NULL,
    .bssid = NULL,
    .channel = 0,
    .show_hidden = true
  };

  // start wifi scan
  ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, true));    //The true parameter cause the function to block until
  // stop wifi scan                                                               
  // the scan is done.
  ESP_LOGI(TAG,"WiFi Scan Done!");
  xEventGroupWaitBits(wifi_event_group, SCANDONE_BIT,
                        false, true, portMAX_DELAY);
  ESP_LOGI(TAG,"WiFi Scan Done Bit set!");
  ESP_ERROR_CHECK(esp_wifi_scan_stop());
  //for wifi connect in the future
  esp_event_loop_set_cb(event_handler, NULL);
   // boot_count=0;
	//oled test
	oled = OLED(GPIO_NUM_18, GPIO_NUM_19, SSD1306_128x64);
	if (oled.init()) {
		ESP_LOGI(TAG, "oled inited");
		oled.draw_rectangle(10, 30, 20, 20, WHITE);    
    oled.fill_rectangle(12, 32, 16, 16, WHITE);
		oled.draw_rectangle(0, 0, 128, 64, WHITE);
		oled.select_font(0);
		//deprecated conversion from string constant to 'char*'
		oled.draw_string(1, 1, "glcd_5x7_font_info", WHITE, BLACK);
		ESP_LOGI(TAG, "String length:%d",
		oled.measure_string("glcd_5x7_font_info"));
		oled.select_font(1);
		oled.draw_string(1, 18, "tahoma_8pt_font_info", WHITE, BLACK);
		ESP_LOGI(TAG, "String length:%d",
		oled.measure_string("tahoma_8pt_font_info"));
		oled.draw_string(55, 30, "Hello ESP32!", WHITE, BLACK);
		oled.refresh(true);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
		//xTaskCreatePinnedToCore(&myTask, "adctask", 2048, NULL, 5, NULL, 1);
	} else {
		ESP_LOGE(TAG, "oled init failed");
	}

   //ntpc
   ntpc();

}

#ifdef __cplusplus
}
#endif
