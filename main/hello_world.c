/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "ssd1306.h"
#include "imgbmp.h"

#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"

#define PIN_FONT_CS 25
#define PIN_OLED_CS 5

typedef struct 
{
    uint8_t high;
    uint8_t mid;
    uint8_t low;
} fontaddr_t;

uint8_t jiong1[]={/*--  文字:  囧  --*/
/*--  宋体12;  此字体下对应的点阵为：宽x高=16x16   --*/
0x00,0xFE,0x82,0x42,0xA2,0x9E,0x8A,0x82,0x86,0x8A,0xB2,0x62,0x02,0xFE,0x00,0x00,
0x00,0x7F,0x40,0x40,0x7F,0x40,0x40,0x40,0x40,0x40,0x7F,0x40,0x40,0x7F,0x00,0x00};

uint8_t lei1[]={/*--  文字:  畾  --*/
/*--  宋体12;  此字体下对应的点阵为：宽x高=16x16   --*/
0x80,0x80,0x80,0xBF,0xA5,0xA5,0xA5,0x3F,0xA5,0xA5,0xA5,0xBF,0x80,0x80,0x80,0x00,
0x7F,0x24,0x24,0x3F,0x24,0x24,0x7F,0x00,0x7F,0x24,0x24,0x3F,0x24,0x24,0x7F,0x00};

//Send a command to the LCD. Uses spi_device_transmit, which waits until the transfer is complete.
void spi_cmd(spi_device_handle_t spi, const uint8_t cmd)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    t.user=(void*)0;                //D/C needs to be set to 0
    ret=spi_device_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

//Send data to the LCD. Uses spi_device_transmit, which waits until the transfer is complete.
void spi_data(spi_device_handle_t spi, const uint8_t *data, int len)
{
    esp_err_t ret;
    spi_transaction_t t;
    if (len==0) return;             //no need to send anything
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=len*8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer=data;               //Data
    t.user=(void*)1;                //D/C needs to be set to 1
    ret=spi_device_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}
uint8_t fontbuf[32]; 
uint8_t* spi_get_data(spi_device_handle_t spi,fontaddr_t addr)
{
    // uint8_t fontbuf[32]; 
    //get_id cmd
    spi_cmd( spi, 0x03);

    spi_cmd( spi, addr.high);
    spi_cmd( spi, addr.mid);
    spi_cmd( spi, addr.low);

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length=8*32;
    // t.flags = SPI_TRANS_USE_RXDATA;
    t.user = (void*)1;
    t.rx_buffer = fontbuf;

    esp_err_t ret = spi_device_transmit(spi, &t);
    assert( ret == ESP_OK );

    return fontbuf;
}

uint32_t  fontaddr=0;
fontaddr_t getStingAddr(char *text)//only support 2byte
{
    uint8_t i= 0;
    uint8_t addrHigh,addrMid,addrLow ;
    // uint8_t fontbuf[32]; 
    fontaddr_t addr;         
    if(((text[i]>=0xb0) &&(text[i]<=0xf7))&&(text[i+1]>=0xa1))
    {                       
        /*国标简体（GB2312）汉字在亿阳字库IC中的地址由以下公式来计算：*/
        /*Address = ((MSB - 0xB0) * 94 + (LSB - 0xA1)+ 846)*32+ BaseAdd;BaseAdd=0*/
        /*由于担心8位单片机有乘法溢出问题，所以分三部取地址*/
        fontaddr = (text[i]- 0xb0)*94; 
        fontaddr += (text[i+1]-0xa1)+846;
        fontaddr = (ulong)(fontaddr*32);
        
        addrHigh = (fontaddr&0xff0000)>>16;  /*地址的高8位,共24位*/
        addrMid = (fontaddr&0xff00)>>8;      /*地址的中8位,共24位*/
        addrLow = fontaddr&0xff;         /*地址的低8位,共24位*/
        addr.high = addrHigh;
        addr.mid = addrMid;
        addr.low = addrLow;
    }
    else if(((text[i]>=0xa1) &&(text[i]<=0xa3))&&(text[i+1]>=0xa1))
    {                       
        /*国标简体（GB2312）15x16点的字符在亿阳字库IC中的地址由以下公式来计算：*/
        /*Address = ((MSB - 0xa1) * 94 + (LSB - 0xA1))*32+ BaseAdd;BaseAdd=0*/
        /*由于担心8位单片机有乘法溢出问题，所以分三部取地址*/
        fontaddr = (text[i]- 0xa1)*94; 
        fontaddr += (text[i+1]-0xa1);
        fontaddr = (ulong)(fontaddr*32);
        
        addrHigh = (fontaddr&0xff0000)>>16;  /*地址的高8位,共24位*/
        addrMid = (fontaddr&0xff00)>>8;      /*地址的中8位,共24位*/
        addrLow = fontaddr&0xff;         /*地址的低8位,共24位*/
        addr.high = addrHigh;
        addr.mid = addrMid;
        addr.low = addrLow;
    }
    else if((text[i]>=0x20) &&(text[i]<=0x7e))  
    {                       
        // unsigned char fontbuf[16];          
        fontaddr = (text[i]- 0x20);
        fontaddr = (unsigned long)(fontaddr*16);
        fontaddr = (unsigned long)(fontaddr+0x3cf80);           
        addrHigh = (fontaddr&0xff0000)>>16;
        addrMid = (fontaddr&0xff00)>>8;
        addrLow = fontaddr&0xff;
        addr.high = addrHigh;
        addr.mid = addrMid;
        addr.low = addrLow;
    }
    return addr;
}

void app_main()
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    // for (int i = 10; i >= 0; i--) {
    //     printf("Restarting in %d seconds...\n", i);
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }
    // printf("Restarting now.\n");
    fflush(stdout);
    // esp_restart();
     /* Select the font to use with menu and all font functions */
    // ssd1306_setFixedFont(ssd1306xled_font6x8);
    ssd1306_setFreeFont(free_calibri11x12);
    // ssd1306_128x64_i2c_init();
    ssd1306_128x64_spi_init(-1, 5, 21); // Use this line for ESP32 (VSPI)  (gpio22=RST, gpio5=CE for VSPI, gpio21=D/C)

    //ssd1306_fillScreen( 0x01);
    ssd1306_clearScreen();
    //ssd1306_charF12x16 (0,0,"Hi ESP32,Use this line for ESP32 (VSPI)!",STYLE_NORMAL);
    ssd1306_drawBitmap(0,0,128,64, bmp);

    //get font data
    spi_device_handle_t spifont;
    spi_device_interface_config_t devcfg={
        .clock_speed_hz=80*1000*1000,           //Clock out at 10 MHz
        .mode=0,                                //SPI mode 0
        .spics_io_num=PIN_FONT_CS,               //CS pin
        .queue_size=7
    };
    esp_err_t ret ;
    // uint8_t s_spi_bus_id = 1;
    // init your interface here
    // spi_bus_config_t buscfg=
    // {
    //     .miso_io_num= s_spi_bus_id ? 19 : 12,
    //     .mosi_io_num= s_spi_bus_id ? 23 : 13,
    //     .sclk_io_num= s_spi_bus_id ? 18 : 14,
    //     .quadwp_io_num=-1,
    //     .quadhd_io_num=-1,
    //     .max_transfer_sz=32
    // };
    // ret=spi_bus_initialize(VSPI_HOST, &buscfg, 1);
    // ESP_ERROR_CHECK(ret);
    //Attach the LCD to the SPI bus
    ret=spi_bus_add_device(VSPI_HOST, &devcfg, &spifont);
    ESP_ERROR_CHECK(ret);
    //Initialize the Font
    gpio_set_direction(PIN_FONT_CS, GPIO_MODE_OUTPUT);

    //Reset the display
    gpio_set_level(PIN_FONT_CS, 0);
    gpio_set_level(PIN_OLED_CS, 1);
    // uint8_t n[2] = {0xCC,0xB7};//"国"
    uint8_t *s = spi_get_data(spifont,getStingAddr("工"));
    for (int i = 0; i < 32; ++i)
    {
        printf("%0X ", s[i]);    
    }
    printf("\n");

    uint8_t *name = (uint8_t *)"国";
    printf("%0x%0x\n", name[0],name[1]);
    gpio_set_level(PIN_FONT_CS, 1);
    gpio_set_level(PIN_OLED_CS, 0);

    ssd1306_drawBitmap(8,2,16,16, s);
    // ssd1306_drawBitmap(8,4,16,16, lei1);

}
