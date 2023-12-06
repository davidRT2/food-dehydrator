/*
ESP32-CAM Base64
Author : ChungYi Fu (Kaohsiung, Taiwan)  2019-8-14 21:00
https://www.facebook.com/francefu
*/
#include <WiFi.h>
#include <WebSocketsClient.h>  // include before MQTTPubSubClient.h
#include <MQTTPubSubClient.h>

const char* ssid = "SUPARDJO_plus";
const char* pass = "supardjo10";

WebSocketsClient client;
MQTTPubSubClient mqtt;

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "Base64.h"

#include "esp_camera.h"

// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled

//CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

void connect() {
connect_to_wifi:
  Serial.print("connecting to wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println(" connected!");

connect_to_host:
  Serial.println("connecting to host...");
  client.disconnect();
  client.begin("w.....", 8080, "/", "mqtt");  // "mqtt" is required
  client.setReconnectInterval(2000);

  Serial.print("connecting to mqtt broker...");
  while (!mqtt.connect("arduino", "public", "public")) {
    Serial.print(".");
    delay(1000);
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected");
      goto connect_to_wifi;
    }
    if (!client.isConnected()) {
      Serial.println("WebSocketsClient disconnected");
      goto connect_to_host;
    }
  }
  Serial.println(" connected!");
}

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);
  delay(10);
   Serial.begin(115200);
  WiFi.begin(ssid, pass);
  // initialize mqtt client
  mqtt.begin(client);

  // subscribe callback which is called when every packet has come
  mqtt.subscribe([](const String& topic, const String& payload, const size_t size) {
    Serial.println("mqtt received: " + topic + " - " + payload);
  });

  // subscribe topic and callback which is called when /hello has come
  mqtt.subscribe("test/get", [](const String& payload, const size_t size) {
    Serial.print("test/get ");
    Serial.println(payload);
    if(payload=="on"){
      ledcWrite(4, 100);
    }else{
      ledcWrite(4, 0);
    }
  });

  connect();
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

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
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;  //0-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  //drop down frame size for higher initial frame rate
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_VGA);  // UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA
}

void loop()
{
   mqtt.update();  // should be called

  if (!mqtt.isConnected()) {
    connect();
  }

  // publish message
  static uint32_t prev_ms = millis();
  if (millis() > prev_ms + 15000) {
    prev_ms = millis();
      mqtt.publish("fd/send", Photo2Base64());
  }
}

String Photo2Base64() {
    camera_fb_t * fb = NULL;
    fb = esp_camera_fb_get();  
    if(!fb) {
      Serial.println("Camera capture failed");
      return "";
    }
  
    String imageFile = "data:image/jpeg;base64,";
    char *input = (char *)fb->buf;
    char output[base64_enc_len(3)];
    for (int i=0;i<fb->len;i++) {
      base64_encode(output, (input++), 3);
      if (i%3==0) imageFile += urlencode(String(output));
    }

    esp_camera_fb_return(fb);
    
    return imageFile;
}

//https://github.com/zenmanenergy/ESP8266-Arduino-Examples/
String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
    return encodedString;
}
