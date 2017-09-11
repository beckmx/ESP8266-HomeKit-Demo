/*
 *  Copyright 2016 HomeACcessoryKid - HacK - homeaccessorykid@gmail.com
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*****************************************************************************************
 * Welcome to the HomeACcessoryKid hkc demo
 * With a few lines of code we demonstrate the easy setup of your ESP8266 as an accessory.
 * Start defining your accessory in hkc_user_init and execute other pending init tasks.
 * For each Service characteristic a callback function is defined.
 * An ACC_callback will be called in different modes.
 * - mode=0: initialize your service (init)
 * - mode=1: a request for a change  is received on which you could act (write)
 * - mode=2: a request for a refresh is received where you might update  (read)
 * A callback should return QUICKLY, else use a Task as demonstrated below.
 *
 * If something changes from inside, you can use change_value and send_events in return.
 * You use aid and iid to know which characteristic to handle and cJSON for the value.
 *
 * Use iOS10 Home app or Eve or other app to test all the features and enjoy
*****************************************************************************************/
 
#include "esp_common.h"
#include "hkc.h"
#include "gpio.h"
#include "queue.h"
#include "FreeRTOS.h"
#include "esp_spiffs.h"
#include "task.h"

#include "spiffs.h"
#include <stdlib.h>
#include "spiffs_test_params.h"

#include "fcntl.h"
#include "unistd.h"

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>


xQueueHandle identifyQueue;

struct esp_spiffs_config config;

struct  gpio {
    int aid;
    int iid;
} gpio2;

void    led_task(void *arg) //make transfer of gpio via arg, starting as a static variable in led routine
{
    int     i,original;
    cJSON   *value;

    os_printf("led_task started\n");
    value=cJSON_CreateBool(0); //value doesn't matter
    while(1) {
        vTaskDelay(1500); //15 sec
        original=GPIO_INPUT(GPIO_Pin_2); //get original state
//      os_printf("original:%d\n",original);
        value->type=original^1;
        GPIO_OUTPUT(GPIO_Pin_2,original^1); // and toggle
        change_value(    gpio2.aid,gpio2.iid,value);
        send_events(NULL,gpio2.aid,gpio2.iid);
    }
}




void led(int aid, int iid, cJSON *value, int mode)
{
    GPIO_ConfigTypeDef gpio2_in_cfg;

    switch (mode) {
        case 1: { //changed by gui
            char *out; out=cJSON_Print(value);  os_printf("led %s\n",out);  free(out);  // Print to text, print it, release the string.
            if (value) GPIO_OUTPUT(GPIO_Pin_2, value->type);
        }break;
        case 0: { //init
            gpio2_in_cfg.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;         //no interrupt
            gpio2_in_cfg.GPIO_Mode     = GPIO_Mode_Output;              //Output mode
            gpio2_in_cfg.GPIO_Pullup   = GPIO_PullUp_EN;                //improves transitions
            gpio2_in_cfg.GPIO_Pin      = GPIO_Pin_2;                    //Enable GPIO
            gpio_config(&gpio2_in_cfg);                                 //Initialization function
            led(aid,iid,value,1);
            gpio2.aid=aid; gpio2.iid=iid;
            xTaskCreate(led_task,"led",512,NULL,2,NULL);
        }break;
        case 2: { //update
            //do nothing
        }break;
        default: {
            //print an error?
        }break;
    }
}



void identify_task(void *arg)
{
    int i,original;

    os_printf("identify_task started\n");
    while(1) {
        while(!xQueueReceive(identifyQueue,NULL,10));//wait for a queue item
        original=GPIO_INPUT(GPIO_Pin_2); //get original state
        for (i=0;i<2;i++) {
            GPIO_OUTPUT(GPIO_Pin_2,original^1); // and toggle
            vTaskDelay(30); //0.3 sec
            GPIO_OUTPUT(GPIO_Pin_2,original^0);
            vTaskDelay(30); //0.3 sec
        }
    }
}

void identify(int aid, int iid, cJSON *value, int mode)
{
    switch (mode) {
        case 1: { //changed by gui
            xQueueSend(identifyQueue,NULL,0);
        }break;
        case 0: { //init
        identifyQueue = xQueueCreate( 1, 0 );
        xTaskCreate(identify_task,"identify",256,NULL,2,NULL);
        }break;
        case 2: { //update
            //do nothing
        }break;
        default: {
            //print an error?
        }break;
    }
}

extern  cJSON       *root;
void    hkc_user_init(char *accname)
{
    //do your init thing beyond the bear minimum
    //avoid doing it in user_init else no heap left for pairing
    cJSON *accs,*sers,*chas,*value;
    int aid=0,iid=0;

    accs=initAccessories();
    
    sers=addAccessory(accs,++aid);
    //service 0 describes the accessory
    chas=addService(      sers,++iid,APPLE,ACCESSORY_INFORMATION_S);
    addCharacteristic(chas,aid,++iid,APPLE,NAME_C,accname,NULL);
    addCharacteristic(chas,aid,++iid,APPLE,MANUFACTURER_C,"HacK",NULL);
    addCharacteristic(chas,aid,++iid,APPLE,MODEL_C,"Rev-1",NULL);
    addCharacteristic(chas,aid,++iid,APPLE,SERIAL_NUMBER_C,"1",NULL);
    addCharacteristic(chas,aid,++iid,APPLE,IDENTIFY_C,NULL,identify);
    //service 1
    chas=addService(      sers,++iid,APPLE,SWITCH_S);
    addCharacteristic(chas,aid,++iid,APPLE,NAME_C,"led",NULL);
    addCharacteristic(chas,aid,++iid,APPLE,POWER_STATE_C,"1",led);
    //service 2
    chas=addService(      sers,++iid,APPLE,LIGHTBULB_S);
    addCharacteristic(chas,aid,++iid,APPLE,NAME_C,"light",NULL);
    addCharacteristic(chas,aid,++iid,APPLE,POWER_STATE_C,"0",NULL);
    addCharacteristic(chas,aid,++iid,APPLE, BRIGHTNESS_C,"0",NULL);

    char *out;
    out=cJSON_Print(root);  os_printf("%s\n",out);  free(out);  // Print to text, print it, release the string.

//  for (iid=1;iid<MAXITM+1;iid++) {
//      out=cJSON_Print(acc_items[iid].json);
//      os_printf("1.%d=%s\n",iid,out); free(out);
//  }
}
static void example_read_file_posix()
{
    const int buf_size = 0xFF;
    uint8_t buf[buf_size];

    int fd = open("test.txt", O_RDONLY);
    if (fd < 0) {
        os_printf("Error opening file\n");
        return;
    }

    int read_bytes = read(fd, buf, buf_size);
    os_printf("Read %d bytes\n", read_bytes);

    buf[read_bytes] = '\0';    // zero terminate string
    os_printf("Data: %s\n", buf);

    close(fd);
}

static void example_read_file_spiffs()
{
    
    char out[20] = {0};
    int pfd = open("myfile",O_RDWR);
    if (read(pfd, out, 20) < 0)
         printf("read errno \n");
    close(pfd);
    printf("--> %s <--\n", out);
}

static void example_write_file()
{
    char *buf="hello world";
    char out[20] = {0};
    int pfd = open("myfile", O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if(pfd <= 3) {
       printf("open file error \n");
    }
    int write_byte = write(pfd, buf, strlen(buf));
    if (write_byte <= 0)
    {
       printf("write file error \n");
    }
    close(pfd);
}

static void example_fs_info()
{
    uint32_t total, used;
    spiffs &csf = esp_spiffs_current();
    SPIFFS_info(csf, &total, &used);
    os_printf("Total: %d bytes, used: %d bytes", total, used);
}

void test_task(void *pvParameters)
{
    
    
        config.phys_size = FS1_FLASH_SIZE;
        config.phys_addr = FS1_FLASH_ADDR;
        config.phys_erase_block = SECTOR_SIZE;
        config.log_block_size = LOG_BLOCK;
        config.log_page_size = LOG_PAGE;
        config.fd_buf_size = FD_BUF_SIZE * 2;
        config.cache_buf_size = CACHE_BUF_SIZE;
    
    if ( esp_spiffs_init(&config) != SPIFFS_OK) {
        os_printf("Error mount SPIFFS\n");
    }

    while (1) {
        vTaskDelay(2000 / portTICK_RATE_MS);

        example_write_file();

        // example_read_file_posix();

        example_read_file_spiffs();

        example_fs_info();

        os_printf("\n\n");
    }
}

void flash_test() {
    os_printf("SDK version:%s\n", system_get_sdk_version());
 
     uint32 temp[4]={122323, 13, 14, 15};
     uint32 temp1[4]={0};
 
     int i = spi_flash_erase_sector(0x8c);
     os_printf("spi_flash_erase_sector: %d\n", i);
     int o = spi_flash_write(0x8c000, temp, sizeof(temp));
     os_printf("spi_flash_write: %d\n", o);
 
     int p = spi_flash_read(0x8c000, temp1, sizeof(temp1));
     os_printf("spi_flash_read: %d\n", p);
 
     os_printf("read :%x, %x, %x, %x\n", temp1[0], temp1[1], temp1[2], temp1[3]);
 }
/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{   
    os_printf("start of user_init @ %d\n",system_get_time()/1000);
    
//use this block only once to set your favorite access point or put your own selection routine
    /*wifi_set_opmode(STATION_MODE); 
    struct station_config *sconfig = (struct station_config *)zalloc(sizeof(struct station_config));
    sprintf(sconfig->ssid, "Unknown"); //don't forget to set this if you use it
    sprintf(sconfig->password, "daredevilme"); //don't forget to set this if you use it
    wifi_station_set_config(sconfig);
    free(sconfig);
    wifi_station_connect(); /**/
    
    //try to only do the bare minimum here and do the rest in hkc_user_init
    // if not you could easily run out of stack space during pairing-setup
    //hkc_init("HomeACcessory");
    xTaskCreate(test_task, "test_task", 1024, NULL, 2, NULL);
    //flash_test();
    os_printf("end of user_init @ %d\n",system_get_time()/1000);
}




/***********************************************************************************
 * FunctionName : user_rf_cal_sector_set forced upon us by espressif since RTOS1.4.2
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal    B : rf init data    C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
***********************************************************************************/
uint32 user_rf_cal_sector_set(void) {
    extern char flashchip;
    SpiFlashChip *flash = (SpiFlashChip*)(&flashchip + 4);
    // We know that sector size is 4096
    //uint32_t sec_num = flash->chip_size / flash->sector_size;
    uint32_t sec_num = flash->chip_size >> 12;
    return sec_num - 5;
}
