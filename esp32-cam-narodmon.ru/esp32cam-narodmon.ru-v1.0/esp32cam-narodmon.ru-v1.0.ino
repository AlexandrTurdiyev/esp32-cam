//=========================================== by #ElectroHobby channel on YouTube ===============================================//
//   - Выбираем плату "ESP32 Wrover Module"
//   - Выбираем в Partion Scheme "Huge APP (3MB No OTA)
//   - GPIO 0 и GND замыкаем перемычкой для заливки скетча
//   - Подключаем UART TTL переходник к соответсвующим выводам TX_RX ESP32
//   - После заливки скетча, нужно отключить контроллер и разомкнуть перемычку, для того чтоб ESP32 загрузилась в стадартном режиме и заработала наша программ
//   - Только передача фото c ESP32 https://youtu.be/g_-D4k37Sgk
//   - Более подробное видео по работе программной части на другой ESP32CAM https://youtu.be/KKQpNcL-_ik
//   - Добавление ESP32CAM в ардуино IDE https://youtu.be/kB1gSdDu0wA
//   - Добавьте эту строчку в Файл - Настройки - Дополнительные ссылки для Менеджера плат: https://dl.espressif.com/dl/package_esp32_index.json
//   - Для DS18B20 необходимо установить дополнительные библиотеки !!! они находятся в папке со скетчем!!! Скетч-Подключить библиотеку-Добавить .zip библиотеку(указать путь к папке со скетчем)
//================================================ #electrohobby on YouTube ====================================================//

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include "time.h"
#include "esp_timer.h"
#include "Arduino.h"
#include "soc/rtc_cntl_reg.h" 
// Модель камеры
#define CAMERA_MODEL_AI_THINKER
#if defined(CAMERA_MODEL_AI_THINKER)
  #define PWDN_GPIO_NUM     32
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM      0
  #define SIOD_GPIO_NUM     26
  #define SIOC_GPIO_NUM     27
  #define Y9_GPIO_NUM       35
  #define Y8_GPIO_NUM       34
  #define Y7_GPIO_NUM       39
  #define Y6_GPIO_NUM       36
  #define Y5_GPIO_NUM       21
  #define Y4_GPIO_NUM       19
  #define Y3_GPIO_NUM       18
  #define Y2_GPIO_NUM        5
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     23
  #define PCLK_GPIO_NUM     22
#else
  #error "Модель камеры не выбрана"
#endif

#define  DEBUG                  1
#define  HTTP_PORT              80
#define  TELEMETRY_PORT         8283
#define  uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define  TIME_TO_SLEEP  60*15    /* Time ESP32 will go to sleep (in seconds) */
#define  TIME_TO_SLEEP  60*15 
#define  WIFI_CONNECTION_ATTEMPTS 20
//========================================================================================================//
//#include "OneWire.h"            // раскоментировать при использованиии датчика температуры
//#include "DallasTemperature.h"  // раскоментировать при использованиии датчика температуры
//#define ONE_WIRE_BUS 14         // раскоментировать при использованиии датчика температуры
//#define __DS18B20               // раскоментировать при использованиии датчика температуры 
const char *key =               "********";            //1. Put your API Key here "KEY"!!!
const char *ssid =              "********";             //2. Put your SSID here 1_
const char *password =          "********";          //3. Put your PASSWORD here 1_
const char macAddr[] =          "********";        //4. можно  сгенерировать тут https://www.miniwebtool.com/mac-address-generator/
const char sensor_ID[] =         "DS18B20";            //5.
//========================================================================================================//
const char *ntpServer =         "pool.ntp.org";
const char *request_content = "--------------------------ef73a32d43e7f04d\r\n"
                        "Content-Disposition: form-data; name=\"%s\"; filename=\"%s.jpg\"\r\n"
                        "Content-Type: image/jpeg\r\n\r\n";

const char *request_end = "\r\n--------------------------ef73a32d43e7f04d--\r\n";
const long gmtOffset_sec =      3600;
const int daylightOffset_sec =  3600;
camera_fb_t * fb = NULL;
WiFiClient client;
IPAddress server(185,245,187,136);
//=========================================================================================================//
//+++++++++++++++++++++++++++++++++++++ send image to narodmon.ru +++++++++++++++++++++++++++++++++++++++++//
//=========================================================================================================//
void update_image(void)
{
    char status[64] = {0};
    char buf[1024];
    struct tm timeinfo;

    Serial.println("Connect to narodmon image server");
    if (!client.connect(server, HTTP_PORT))
    {
        Serial.println("Connection failed");
        return;
    }
    //Serial.println("Get time");
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        snprintf(buf, sizeof(buf), request_content,key, String(millis()).c_str());
    }
    else
    {
        strftime(status, sizeof(status), "%Y%m%d%H%M%S", &timeinfo);
        snprintf(buf, sizeof(buf), request_content,key, status);
    }

    uint32_t content_len = fb->len + strlen(buf) + strlen(request_end);


    String request = "POST /post HTTP/1.1\r\n";
    request += "Host: narodmon.ru\r\n";
    request += "User-Agent: curl/7.67.0\r\n";
    request += "Accept: */*\r\n";
    request += "Content-Length: " + String(content_len) + "\r\n";
    request += "Content-Type: multipart/form-data; boundary=------------------------ef73a32d43e7f04d\r\n";
    request += "Expect: 100-continue\r\n";
    request += "\r\n";
    Serial.println(request);
    client.print(request);

    client.readBytesUntil('\r', status, sizeof(status));
    Serial.println(status);
    if (strcmp(status, "HTTP/1.1 100 Continue") != 0)
    {
        Serial.print("Unexpected response: ");
        client.stop();
        return;
    }
    Serial.print(buf);
    client.print(buf);

    uint8_t *image = fb->buf;
    size_t size = fb->len;
    size_t offset = 0;
    size_t ret = 0;
    Serial.println("start send image");
    while (1)
    {
        ret = client.write(image + offset, size);
        offset += ret;
        size -= ret;
        if (fb->len == offset)
        {
            break;
        }
    }
    client.print(request_end);
    if(DEBUG)Serial.println("stop send image");
    if(DEBUG)Serial.println(request_end);
    client.find("\r\n");

    bzero(status, sizeof(status));
    client.readBytesUntil('\r', status, sizeof(status));
    if (strncmp(status, "HTTP/1.1 200 OK", strlen("HTTP/1.1 200 OK")))
    {
        if(DEBUG)Serial.print("response: error");
        if(DEBUG)Serial.println(status);
        client.stop();
        return;
    }else{
      if(DEBUG)Serial.println("response: OK");
    }

    if (!client.find("\r\n\r\n"))
    {
        if(DEBUG)Serial.println("Invalid response");
    }
     if(DEBUG)Serial.println("client.stop");
     client.stop();

     esp_camera_fb_return(fb);
}
#if defined(__DS18B20) 
//=========================================================================================================//
//++++++++++++++++++++++++++++++++++++++++++ read sesnsor +++++++++++++++++++++++++++++++++++++++++++++++++//
//=========================================================================================================//
float read_sensor(uint8_t index){
  OneWire oneWire_DS18B20(ONE_WIRE_BUS);
  DallasTemperature _ds18b20_(&oneWire_DS18B20);
  _ds18b20_.begin();
  if(DEBUG)Serial.print("Requesting temperatures...");
  _ds18b20_.requestTemperatures();
  if(DEBUG)Serial.println(" done");
  if(DEBUG)Serial.print("ds18b20 data: ");
  float temp = _ds18b20_.getTempCByIndex(index);
  if(DEBUG) Serial.println(temp);
  return temp;
  }
//=========================================================================================================//
//+++++++++++++++++++++++++++ send sensensors data to narodmon.ru +++++++++++++++++++++++++++++++++++++++++//
//=========================================================================================================//
void send_sensors(void){
    char status[64] = {0};
    
    Serial.println("Connect to narodmon image server");
    if (!client.connect(server, TELEMETRY_PORT))
    {
        Serial.println("Connection failed");
        return;
    }
    float ds18b20_temp = read_sensor(0);
    String temp = String(ds18b20_temp);
    String request = "#";
    request +=  macAddr;
    request += "\n#";
    request += sensor_ID;
    request += "#";
    request += temp; //ds18b20 data
    request += "\n##\r\n";
    if(DEBUG)Serial.println(request);
    client.print(request);

    client.readBytesUntil('\r', status, sizeof(status));
    if(DEBUG)Serial.println(status);
    if (strstr(status,"OK")== NULL)
    {
        Serial.println("Unexpected response !!!");
        client.stop();
        return;
    }
     if(DEBUG)Serial.println("client.stop");
     client.stop();
  }
#endif
//=========================================================================================================//
//++++++++++++++++++++++++++++++++++++++++++++ wifi init ++++++++++++++++++++++++++++++++++++++++++++++++++//
//=========================================================================================================//  
void wifi_init(){ 
  uint16_t count = 0;
    // Wi-Fi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if(DEBUG)Serial.print(".");
    count++;
    if (count > WIFI_CONNECTION_ATTEMPTS) ESP.restart();
  }
  if(DEBUG)Serial.println("");
  if(DEBUG)Serial.println("WiFi connected");
  if(DEBUG)Serial.println(WiFi.localIP());
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);  
  }
//=========================================================================================================//
//++++++++++++++++++++++++++++++++++++++++++++++ setup ++++++++++++++++++++++++++++++++++++++++++++++++++++//
//=========================================================================================================//
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
 
  Serial.begin(500000);
  Serial.setDebugOutput(false);
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 
  config.frame_size = FRAMESIZE_UXGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;
  
  // Camera init
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    if(DEBUG)Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  wifi_init();
  if(DEBUG)Serial.println("send camera data to narodmon");
  fb = esp_camera_fb_get();
  if (!fb && DEBUG)Serial.println("Camera capture failed");
  update_image();
#if defined (__DS18B20)  
  send_sensors();  
#endif  
  delay(1000);
  Serial.flush();
  esp_deep_sleep_start();
  
}
//=========================================================================================================//
//+++++++++++++++++++++++++++++++++++++++++++++ main ++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//=========================================================================================================//
void loop() {  
    //This is not going to be called
  
    Serial.println("I AM NOT SLEEP!!!");
    delay(1000);
    Serial.flush();
    esp_deep_sleep_start();
}
