//=========================================== by Electrohobby channel on YouTube ===============================================//
//   - Выбираем плату "ESP32 Wrover Module"
//   - Выбираем в Partion Scheme "Huge APP (3MB No OTA)
//   - GPIO 0 и GND замыкаем перемычкой для заливки скетча
//   - После заливки скетча, нужно отключить контроллер и разомкнуть перемычку, для того чтоб ESP32 загрузилась в стадартном режиме и заработала наша программ
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

#define  DEBUG 1
#define  HTTP_PORT              80
#define  SEND_NARODMON_DELAY    (60*1000*15)         // ms
const char *key =               "********";         // Put your API Key here "KEY"!!!
const char *ssid =              "********";             // Put your SSID here
const char *password =          "********";          // Put your PASSWORD here
const char *ntpServer =         "pool.ntp.org";
const char *request_content = "--------------------------ef73a32d43e7f04d\r\n"
                        "Content-Disposition: form-data; name=\"%s\"; filename=\"%s.jpg\"\r\n"
                        "Content-Type: image/jpeg\r\n\r\n";

const char *request_end = "\r\n--------------------------ef73a32d43e7f04d--\r\n";
const long gmtOffset_sec =      3600;
const int daylightOffset_sec =  3600;
unsigned long last_time_ms = 0;
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

    Serial.println("Connect to narodmon");
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
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    if(DEBUG)Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  // Wi-Fi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if(DEBUG)Serial.print(".");
  }
  if(DEBUG)Serial.println("");
  if(DEBUG)Serial.println("WiFi connected");
  
  if(DEBUG)Serial.println(WiFi.localIP());
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}
//=========================================================================================================//
//+++++++++++++++++++++++++++++++++++++++++++++ main ++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//=========================================================================================================//
void loop() {

    if(millis() - last_time_ms  > SEND_NARODMON_DELAY)
    { 
      if(DEBUG)Serial.println("send_to_narodmon");
      fb = esp_camera_fb_get();
      if (!fb) {
      if(DEBUG)Serial.println("Camera capture failed");
      }
      last_time_ms = millis();
      update_image();
    }
}
