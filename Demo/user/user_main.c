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
#include <lwip/api.h>

#include "espconn.h"


#define DEMO_AP_SSID      "WT_TEST"
#define DEMO_AP_PASSWORD  "suitch"
#define SOFT_AP_SSID      "DEMO_AP"
#define SOFT_AP_PASSWORD  "demodemo"


xQueueHandle identifyQueue;
extern spiffs workingFS;
//struct esp_spiffs_config config;
char mac_address[6]; // note 6, not 5, there's one there for the null terminator
char suitch_ssid[9]="Suitch-v1";


struct  gpio {
    int aid;
    int iid;
} gpio2;

void    led_task(void *arg) //make transfer of gpio via arg, starting as a static variable in led routine
{
    int     i,original;
    cJSON   *value;

    os_printf("led_task started--no timer\n");
    value=cJSON_CreateBool(0); //value doesn't matter
    //while(1) {
        //vTaskDelay(1500); //15 sec
        original=GPIO_INPUT(GPIO_Pin_2); //get original state
//      os_printf("original:%d\n",original);
        value->type=original^1;
        GPIO_OUTPUT(GPIO_Pin_2,original^1); // and toggle
        change_value(    gpio2.aid,gpio2.iid,value);
        send_events(NULL,gpio2.aid,gpio2.iid);
    //}
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
            //xTaskCreate(led_task,"led",512,NULL,2,NULL);
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




// void mount_filesystem()
// {
    
    
//         config.phys_size = FS3_FLASH_SIZE;
//         config.phys_addr = FS3_FLASH_ADDR;
//         config.phys_erase_block = SECTOR_SIZE;
//         config.log_block_size = LOG_BLOCK;
//         config.log_page_size = LOG_PAGE;
//         config.fd_buf_size = FD_BUF_SIZE * 2;
//         config.cache_buf_size = CACHE_BUF_SIZE;
    
//     if ( esp_spiffs_init(&config) != SPIFFS_OK) {
//         os_printf("Error mount SPIFFS\n");
//     }

//     // while (1) {
//     //     vTaskDelay(2000 / portTICK_RATE_MS);

//     //     example_write_file();

//     //     // example_read_file_posix();

//     //     example_read_file_spiffs();

        

//     //     os_printf("\n\n");
//     // }
// }

void saveToFile(char *content, char *fileName){
    strcat(content,"$");
    int pfd = open(fileName, O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if(pfd <= 3) {
        os_printf("open file error \n");
    }
    int write_byte = write(pfd, content, strlen(content));
    if (write_byte <= 0)
    {
        os_printf("write file error \n");
    }
    close(pfd);
}

void saveSSID(char *content){
    uint32  start,erasesector;
    char buff[16] = {0};
    //strcpy(buff, content);

    size_t len = strlen(content);
    strcpy(buff, content);
    //buff[len]='\0';
    
    start=0x79455;
    erasesector=0x7945;
    //spi_flash_erase_sector(erasesector);
    spi_flash_write(start,(uint32 *)buff,16);
}

void savePassword(char *content){
    uint32  start;
    char buff[16] = {0};

    size_t len = strlen(content);
    strcpy(buff, content);
    //buff[len]='\0';

    start=0x794c4;
    //spi_flash_erase_sector(0x794c);
    spi_flash_write(start,(uint32 *)buff,16); 
}



char* getParamValue(char *paramName, char *queryString){
    char *token;
    char *paramValue;
    /* get the first token */
    token = strtok(queryString, "&");
    while( token != NULL ) 
    {
        //os_printf("--> %s <--token\n", token);
       if (strstr(token, paramName) != NULL) {
        // contains
        paramValue=(char*)token + strlen(paramName)+2;
        }
        token = strtok (NULL, "&");
    }
    return paramValue;
}

char* read_file(char *fileName)
{
    
    
    char *out = malloc (sizeof (char) * 20);
    int pfd = open(fileName,O_RDWR);
    if (read(pfd, out, 20) < 0){
        os_printf("read errno \n");
    }   
    close(pfd);
    os_printf("--> %s <--\n", out);
    return strtok(out, "$");;
}

void soft_ap_init(void)
{
    wifi_set_opmode(SOFTAP_MODE);
    struct softap_config *config = (struct softap_config *) zalloc(sizeof(struct softap_config)); // initialization
    wifi_softap_get_config(config); // Get soft-AP config first.
    sprintf(config->ssid, suitch_ssid);
    sprintf(config->password, SOFT_AP_PASSWORD);
    config->authmode = AUTH_WPA_WPA2_PSK;
    config->ssid_len = 0; // or its actual SSID length
    config->max_connection = 4;
    wifi_softap_set_config(config); // Set ESP8266 soft-AP config
    free(config);
    struct station_info * station = wifi_softap_get_station_info();
    while (station) {
        os_printf("bssid : MACSTR, ip : IPSTR/n", MAC2STR(station->bssid), IP2STR(&station->ip));
        station = STAILQ_NEXT(station, next);
    }
    wifi_softap_free_station_info(); // Free it by calling functionss
    wifi_softap_dhcps_stop(); // disable soft-AP DHCP server
    struct ip_info info;
    IP4_ADDR(&info.ip, 192, 168, 5, 1); // set IP
    IP4_ADDR(&info.gw, 192, 168, 5, 1); // set gateway
    IP4_ADDR(&info.netmask, 255, 255, 255, 0); // set netmask
    wifi_set_ip_info(SOFTAP_IF, &info);
    struct dhcps_lease dhcp_lease;
    IP4_ADDR(&dhcp_lease.start_ip, 192, 168, 5, 100);
    IP4_ADDR(&dhcp_lease.end_ip, 192, 168, 5, 105);
    wifi_softap_set_dhcps_lease(&dhcp_lease);
    wifi_softap_dhcps_start(); // enable soft-AP DHCP server
}

void httpd_task(void *pvParameters)
{
    struct netconn *client = NULL;
    struct netconn *nc = netconn_new(NETCONN_TCP);
    if (nc == NULL) {
        os_printf("Failed to allocate socket\n");
        vTaskDelete(NULL);
    }
    netconn_bind(nc, IP_ADDR_ANY, 80);
    netconn_listen(nc);
    char buf[2048];
    const char *saveOk={
        "HTTP/1.1 200 OK\r\n"
    };
    const char *webpage = {
        "HTTP/1.1 200 OK\r\n"
        "Content-type: text/html\r\n\r\n"
        "<html><head><title>HTTP Server</title>"
        "<script type=\"text/javascript\">function proceed () {"
        "var xhttp = new XMLHttpRequest();"
        "xhttp.open(\"POST\", \"savewifi\", true);"
        "xhttp.setRequestHeader(\"Content-type\", \"application/x-www-form-urlencoded\");"
        "xhttp.send(\"$fname=Henry&$lname=Ford\");"
        "}"
        "</script>"
        "<style> div.main {"
        "font-family: Arial;"
        "padding: 0.01em 16px;"
        "box-shadow: 2px 2px 1px 1px #d2d2d2;"
        "background-color: #f1f1f1;}"
        "</style></head>"
        "<body><div class='main'>"
        "<h3>HTTP Server</h3>"
        "<p>URL: %s</p>"
        "<p>Uptime: %d seconds</p>"
        "<p>Free heap: %d bytes</p>"
        "<button onclick=\"proceed();\" type='button'>"
        "LED On</button></p>"
        "<button onclick=\"location.href='/off'\" type='button'>"
        "LED Off</button></p>"
        "<form id=\"ninja\" action=\"/savewifi\" method=\"POST\">"
        "<input id=\"ssid_name\" name=\"$ssid\" value=\"a\">"
        "<input id=\"password_name\" name=\"$password\" value=\"a\">"
        "<button type=\"submit\">Save WIFI settings</button>"
        "</form>"
        "</div></body></html>"
    };
    while (1) {
        err_t err = netconn_accept(nc, &client);
        if (err == ERR_OK) {
            struct netbuf *nb;
            if ((err = netconn_recv(client, &nb)) == ERR_OK) {
                void *data;
                u16_t len;
                netbuf_data(nb, &data, &len);
                os_printf("data: %s\n", data);
                /* check for a GET request */
                if (!strncmp(data, "GET ", 4)) {
                    char uri[16];
                    const int max_uri_len = 16;
                    char *sp1, *sp2;
                    /* extract URI */
                    sp1 = (char*)data + 4;
                    sp2 = memchr(sp1, ' ', max_uri_len);
                    int len = sp2 - sp1;
                    memcpy(uri, sp1, len);
                    uri[len] = '\0';
                    os_printf("uri: %s\n", uri);
                    if (!strncmp(uri, "/on", max_uri_len)) {
                        // gpio_write(2, false);
                        os_printf("should turn ON led\n");
                        snprintf(buf, sizeof(buf), webpage,
                            uri,
                            xTaskGetTickCount() * portTICK_RATE_MS / 1000,
                            (int) xPortGetFreeHeapSize());
                    netconn_write(client, buf, strlen(buf), NETCONN_COPY);
                    } else if (!strncmp(uri, "/off", max_uri_len)){
                        //gpio_write(2, true);
                        os_printf("should turn OFF led\n");
                        snprintf(buf, sizeof(buf), webpage,
                            uri,
                            xTaskGetTickCount() * portTICK_RATE_MS / 1000,
                            (int) xPortGetFreeHeapSize());
                    netconn_write(client, buf, strlen(buf), NETCONN_COPY);
                    } else if (!strncmp(uri, "/reset", max_uri_len)){
                        os_printf("resetting data0\n");
                        
                        // struct softap_config *config = (struct softap_config *) zalloc(sizeof(struct softap_config)); // initialization
                        // wifi_softap_get_config(config); // Get soft-AP config first.
                        // sprintf(config->password, "suitch");
                        // sprintf(config->ssid, suitch_ssid);
                        // os_printf("resetting data1\n");
                        
                        // config->authmode = AUTH_WPA_WPA2_PSK;
                        // config->ssid_len = 0; // or its actual SSID length
                        // config->max_connection = 4;
                        os_printf("resetting data2\n");
                        //wifi_softap_set_config(config); // Set ESP8266 soft-AP config
                        
                        
                        
                        os_printf("resetting data3\n");
                        
                        snprintf(buf, sizeof(buf), webpage,
                            uri,
                            xTaskGetTickCount() * portTICK_RATE_MS / 1000,
                            (int) xPortGetFreeHeapSize());
                        netconn_write(client, buf, strlen(buf), NETCONN_COPY);
                        soft_ap_init();
                    }
                    
                }
                if (!strncmp(data, "POST ", 4)) {
                    char uri[16];
                    char *mypwd;
                    char *myssid;
                    const int max_uri_len = 16;
                    char *sp1, *sp2;
                    /* extract URI */
                    sp1 = (char*)data + 4;
                    char *dest_init = strstr(data, "$");
                    int index;
                    char *dest_end=strchr(dest_init, '\n');
                    index = (int)(dest_end - dest_init);
                    char dest[40];
                    memcpy( dest, &dest_init[0], index-1 );
                    dest[index-1]='\0';
                    os_printf("dest: %s\n", dest);
                    mypwd=getParamValue("password",dest);
                    myssid=getParamValue("ssid",dest);
                    //looks like first the last parameters must be requested
                    os_printf("password: %s\n", mypwd);
                    //savePassword(getParamValue("password",dest));
                    //os_printf("data: %s\n", data);
                    os_printf("ssid: %s\n", myssid);
                    //saveSSID(getParamValue("ssid",dest));
                    // *config = (struct softap_config *) zalloc(sizeof(struct softap_config)); // initialization
                    // //struct softap_config *config = (struct softap_config *) zalloc(sizeof(struct softap_config)); // initialization
                    // wifi_softap_get_config(config); // Get soft-AP config first.
                    // sprintf(config->password, mypwd);
                    // sprintf(config->ssid, myssid);
                    
                    wifi_set_opmode(STATION_MODE); 
                    struct station_config *sconfig = (struct station_config *)zalloc(sizeof(struct station_config));
                    sprintf(sconfig->password, mypwd); //don't forget to set this if you use it
                    sprintf(sconfig->ssid, myssid); //don't forget to set this if you use it
                    
                    wifi_station_set_config(sconfig);
                    free(sconfig);
                    wifi_station_connect();
                    
                    
                    
                    
                    // if (!strncmp(uri, "/on", max_uri_len))
                    //     // gpio_write(2, false);
                    //     os_printf("should turn ON led");
                    // else if (!strncmp(uri, "/off", max_uri_len))
                    //     //gpio_write(2, true);
                    //     os_printf("should turn OFF led");
                    snprintf(buf, sizeof(buf), webpage,
                            uri,
                            xTaskGetTickCount() * portTICK_RATE_MS / 1000,
                            (int) xPortGetFreeHeapSize());
                    netconn_write(client, buf, strlen(buf), NETCONN_COPY);
                }

            }
            netbuf_delete(nb);
        }
        os_printf("Closing connection\n");
        netconn_close(client);
        netconn_delete(client);
    }
}





void append(char* s, char c) {
        int len = strlen(s);
        s[len] = c;
        s[len+1] = '\0';
}
/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{   
    //os_printf("start of user_init @ %d\n",system_get_time()/1000);
    
//use this block only once to set your favorite access point or put your own selection routine
    /*wifi_set_opmode(STATION_MODE); 
    struct station_config *sconfig = (struct station_config *)zalloc(sizeof(struct station_config));
    sprintf(sconfig->ssid, "Unknown"); //don't forget to set this if you use it
    sprintf(sconfig->password, "daredevilme"); //don't forget to set this if you use it
    wifi_station_set_config(sconfig);
    free(sconfig);
    wifi_station_connect(); /**/



    

    struct softap_config *config = (struct softap_config *) zalloc(sizeof(struct softap_config)); // initialization
    wifi_softap_get_config(config); // Get soft-AP config first.
    os_printf("CURRENT_SSID:%s\n", config->ssid);
    os_printf("CURRENT_PWD:%s\n", config->password);
    
    uint8_t hwaddr[6];
    
    char my_id[32];

    wifi_get_macaddr(STATION_IF, (uint8_t*)hwaddr);

    
    snprintf(my_id, sizeof(my_id), "%02x%02x%02x%02x%02x%02x", MAC2STR(hwaddr));
    int i=0;
    for(i = 8; my_id[i] != '\0'; ++i) {
        //str[i] = toupper(str[i]);
        os_printf("char:%c\n", my_id[i]);
        //append(my_id2,my_id[i]);
        suitch_ssid[i]=my_id[i];
        suitch_ssid[i+1] = '\0';
    }

    
   
    os_printf("CURRENT_MAC:%s\n", suitch_ssid);
    //mount_filesystem();
    //if(strlen(read_file("ssid.txt"))>3){
    if(strstr(config->ssid, suitch_ssid)!=NULL && strcmp(config->password, SOFT_AP_PASSWORD)==0){
        int r = rand() % 999;
        soft_ap_init();
        xTaskCreate(&httpd_task, "http_server", 1024, NULL, 2, NULL);
        os_printf("RESET-BRAND-NEW\n");
    } else {
        // os_printf("FLASH:%s\n",read_file("ssid.txt"));
        wifi_set_opmode(STATION_MODE); 
        // struct station_config *sconfig = (struct station_config *)zalloc(sizeof(struct station_config));
        // sprintf(sconfig->ssid, "Unknown"); //don't forget to set this if you use it
        // sprintf(sconfig->password, "daredevilme"); //don't forget to set this if you use it
        //wifi_station_set_config(sconfig);
        xTaskCreate(&httpd_task, "http_server", 1024, NULL, 2, NULL);
        //free(sconfig);
        wifi_station_connect();
        hkc_init("HomeACcessory");
    }
    
    
    
    //try to only do the bare minimum here and do the rest in hkc_user_init
    // if not you could easily run out of stack space during pairing-setup
    //hkc_init("HomeACcessory");
    
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