#include "esp_log.h"
#include "fonts.h"
#include "ssd1306.hpp"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include <string>
#include <sstream>
#include <iostream>

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

using namespace std;

OLED oled = OLED(GPIO_NUM_18, GPIO_NUM_19, SSD1306_128x64);

void myTask(void *pvParameters) {
	adc1_config_width(ADC_WIDTH_12Bit);
	adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_0db);
	bool flag=true;

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

esp_err_t event_handler(void *ctx, system_event_t *event)
{
	ostringstream os;
   if (event->event_id == SYSTEM_EVENT_SCAN_DONE) {
      uint16_t apCount = 0;
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
               authmode = "WIFI_AUTH_OPEN";
               break;
            case WIFI_AUTH_WEP:
               authmode = "WIFI_AUTH_WEP";
               break;           
            case WIFI_AUTH_WPA_PSK:
               authmode = "WIFI_AUTH_WPA_PSK";
               break;           
            case WIFI_AUTH_WPA2_PSK:
               authmode = "WIFI_AUTH_WPA2_PSK";
               break;           
            case WIFI_AUTH_WPA_WPA2_PSK:
               authmode = "WIFI_AUTH_WPA_WPA2_PSK";
               break;
            default:
               authmode = "Unknown";
               break;
         }
         printf("%26.26s    |    % 4d    |    %22.22s\n",list[i].ssid, list[i].rssi, authmode);
         os.str("");
         os << list[i].ssid;
         oled.fill_rectangle(1,51,126,11,WHITE);
         oled.select_font(1);
         oled.draw_string(1,51,os.str(),BLACK,WHITE);
         oled.refresh(true);
         vTaskDelay(2000/portTICK_PERIOD_MS);
      }
      free(list);
      printf("\n\n");
   }
   return ESP_OK;
}

#ifdef __cplusplus
extern "C" {
#endif
void app_main() {
	nvs_flash_init();
	system_init();

	//oled test
	oled = OLED(GPIO_NUM_18, GPIO_NUM_19, SSD1306_128x64);
	if (oled.init()) {
		ESP_LOGI("OLED", "oled inited");
		oled.draw_rectangle(10, 30, 20, 20, WHITE);
		oled.draw_rectangle(0, 0, 128, 64, WHITE);
		oled.select_font(0);
		//deprecated conversion from string constant to 'char*'
		oled.draw_string(1, 1, "glcd_5x7_font_info", WHITE, BLACK);
		ESP_LOGI("OLED", "String length:%d",
				oled.measure_string("glcd_5x7_font_info"));
		oled.select_font(1);
		oled.draw_string(1, 18, "tahoma_8pt_font_info", WHITE, BLACK);
		ESP_LOGI("OLED", "String length:%d",
				oled.measure_string("tahoma_8pt_font_info"));
		oled.draw_string(55, 30, "Hello ESP32!", WHITE, BLACK);
		oled.refresh(true);
		vTaskDelay(5000 / portTICK_PERIOD_MS);
		//xTaskCreatePinnedToCore(&myTask, "adctask", 2048, NULL, 5, NULL, 1);
	} else {
		ESP_LOGE("OLED", "oled init failed");
	}

	//wifi scan
   tcpip_adapter_init();
   ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
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

   //start wifi scan
   ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, true));    //The true parameter cause the function to block until
   //stop wifi scan                                                               //the scan is done.
   ESP_ERROR_CHECK(esp_wifi_scan_stop()); 
   // while(true){
   //     ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, true));    //The true parameter cause the function to block until
   //                                                               //the scan is done.
   //     vTaskDelay(1000 / portTICK_PERIOD_MS);
   // }
   
}
#ifdef __cplusplus
}
#endif
