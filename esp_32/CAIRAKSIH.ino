#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Keypad.h>
// #include <FirebaseESP32.h>
#include <PubSubClient.h>
#include <WiFi.h>

// #define WIFI_SSID "codot_kwe"
// #define WIFI_PASSWORD "22222222"
// #define FIREBASE_HOST "https://kanso-suhu-default-rtdb.asia-southeast1.firebasedatabase.app/"
// #define FIREBASE_AUTH "tAIzaSyBSG9E_g84r7bGZD0j7gBwMDUEfCB0E144"


#define WIFI_SSID "SamaElla"
#define WIFI_PASSWORD "12345678"
#define MQTT_USER "fd-polines"
#define MQTT_PASSWORD "@Robot_"

const char* mqtt_server = ".....";

WiFiClient espClient;
PubSubClient client(espClient);

#define DHTPIN 14        // Pin yang terhubung dengan sensor DHT22
#define DHTTYPE DHT22   // Tipe sensor DHT22

DHT_Unified dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 20, 4);

const int buzzerPin = 13;
const int heaterPin = 25;
const int kipasPinIn = 26;
const int kipasPinOut = 27;

const byte ROWS = 4; // Jumlah baris pada keypad
const byte COLS = 4; // Jumlah kolom pada keypad

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {19, 18, 5, 17};
byte colPins[COLS] = {16, 4, 12, 15}; 

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

int suhuMin = 0;
int suhuMax = 0;
int timer = 0;
bool suhuMaxInputDone = false;
bool suhuMinInputDone = false;
bool timerInputDone = false;
bool prosesAktif = false;
unsigned long previousMillis = 0;
unsigned long buzzerInterval = 2000;
bool buzzerOn = false;
bool deleteMode = false;
int oldTimer = 0;
int oldTemp = 0;
int oldHum = 0;

// FirebaseData firebaseData;

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("foodDehydrator-esp32", MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("logesp");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Wire.begin();

  Serial.begin(9600);
  dht.begin();

  lcd.begin(20, 4);
  lcd.backlight();

  pinMode(buzzerPin, OUTPUT);
  pinMode(heaterPin, OUTPUT);
  pinMode(kipasPinIn, OUTPUT);
  pinMode(kipasPinOut, OUTPUT);

  digitalWrite(heaterPin, HIGH);
  digitalWrite(kipasPinIn, HIGH);
  digitalWrite(kipasPinOut, HIGH);
  
  lcd.setCursor(0, 0);
  lcd.print("Menyambung wifi");
  lcd.setCursor(0, 1);


  // Mulai koneksi WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.print(".");
  }
  Serial.println();
  Serial.println("WiFi terhubung!");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Wifi Tersambung");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  delay(500);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Masukkan Suhu Max");
  lcd.setCursor(0, 1);
  lcd.print("tekan #: ");

  // Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  // Firebase.setString(firebaseData, "/data", "OFF");

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  client.publish("kangso/esp32/data", "OFF");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (!suhuMaxInputDone) {
    char key = keypad.getKey();
    if (key == '#') {
      if (suhuMax != 0) {
        suhuMaxInputDone = true;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Masukkan Suhu Min");
        lcd.setCursor(0, 1);
        lcd.print("tekan #: ");
      }
    } else if (key == 'D') {
      suhuMax = 0;
      lcd.setCursor(0, 3);
      lcd.print("          ");
    } else if (key != NO_KEY && key >= '0' && key <= '9') {
      suhuMax = suhuMax * 10 + (key - '0');
      lcd.setCursor(0, 3);
      client.publish("fd/maxtemp", String(suhuMax).c_str());
      lcd.print(suhuMax);
    }
  } else if (!suhuMinInputDone) {
    char key = keypad.getKey();
    if (key == '#') {
      if (suhuMin != 0) {
        suhuMinInputDone = true;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Masukkan Timer");
        lcd.setCursor(0, 1);
        lcd.print("tekan #: ");
      }
    } else if (key == 'D') {
      suhuMin = 0;
      lcd.setCursor(0, 3);
      lcd.print("          ");
    } else if (key != NO_KEY && key >= '0' && key <= '9') {
      suhuMin = suhuMin * 10 + (key - '0');
      lcd.setCursor(0, 3);
      client.publish("fd/mintemp", String(suhuMin).c_str());
      lcd.print(suhuMin);
    }
  } else if (!timerInputDone) {
    char key = keypad.getKey();
    if (key == '#') {
      if (timer != 0) {
        timerInputDone = true;
        prosesAktif = true;
        // Firebase.setString(firebaseData, "/data", "ON");
        client.publish("data", "ON");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Proses Aktif");
      }
    } else if (key == 'D') {
      timer = 0;
      lcd.setCursor(0, 3);
      lcd.print("          ");
    } else if (key != NO_KEY && key >= '0' && key <= '9') {
      timer = timer * 10 + (key - '0');
      lcd.setCursor(0, 3);
      lcd.print(timer);
    }
  }


  if (prosesAktif) {
    unsigned long currentMillis = millis();

    char key = keypad.getKey();
    if (key) {
      if (key == 'B') {
        suhuMaxInputDone = false;
        suhuMinInputDone = false;
        timerInputDone = false;
        prosesAktif = false;

        digitalWrite(heaterPin, HIGH);
        digitalWrite(kipasPinIn, HIGH);
        digitalWrite(kipasPinOut, HIGH);
        // Firebase.setString(firebaseData, "/data", "OFF");
        client.publish("data", "OFF");


        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Masukkan Suhu Max");
        lcd.setCursor(0, 1);
        lcd.print("tekan #: ");
        suhuMin = 0;
        suhuMax = 0;
        timer = 0;
        return ;
      }
    }
    
    if (currentMillis - previousMillis >= 1000) {
      previousMillis = currentMillis;
      oldTimer = timer;
      timer--;
      if (timer <= 0) {
        prosesAktif = false;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Proses Selesai");
        digitalWrite(heaterPin, HIGH);
        digitalWrite(kipasPinIn, HIGH);
        digitalWrite(kipasPinOut, LOW);
        buzzerOn = true;
        tone(buzzerPin, 1000, buzzerInterval);
        
        char key = keypad.getKey();
        while(true) {
          key = keypad.getKey();
          if(key) {
            if (key == 'B') break;
          }
        }

        suhuMaxInputDone = false;
        suhuMinInputDone = false;
        timerInputDone = false;
        prosesAktif = false;

        // Firebase.setString(firebaseData, "/data", "OFF");
        client.publish("data", "OFF");

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Masukkan Suhu Max");
        lcd.setCursor(0, 1);
        lcd.print("tekan #: ");
        suhuMin = 0;
        suhuMax = 0;
        timer = 0;
        return ;
      }
    }

    sensors_event_t eventT;
    dht.temperature().getEvent(&eventT);
    float t = eventT.temperature;
    oldTemp = t;

    sensors_event_t eventH;
    dht.humidity().getEvent(&eventH);
    float h = eventH.relative_humidity;
    oldHum = h;
    if (h != oldHum) {
        lcd.setCursor(0, 0);
        lcd.print("                                  ");
        lcd.setCursor(0, 0);
        lcd.print("Suhu: " + String(t, 2) + " C"); // Menampilkan suhu dengan 2 angka di belakang koma
      }
    lcd.setCursor(0, 0);
    lcd.print("                                      ");
    lcd.setCursor(0, 1);
     if (h != oldHum) {
        lcd.setCursor(0, 1);
        lcd.print("                                  ");
        lcd.setCursor(0, 1);
        lcd.print("Kelembapan: " + String(h, 2) + " %"); // Menampilkan kelembapan dengan 2 angka di belakang koma
      }
    lcd.setCursor(0, 2);
     if (timer != oldTimer) {
        lcd.setCursor(0, 2);
        lcd.print("          ");
        lcd.setCursor(0, 2);
        lcd.print("Timer: " + String(timer) + " s");
      }

    Serial.print("Suhu: ");
    Serial.print(t);
    Serial.println(" C");

        if (t >= suhuMax) {
      digitalWrite(heaterPin, HIGH);
      digitalWrite(kipasPinIn, LOW);
      digitalWrite(kipasPinOut, LOW);
    } else if (t < suhuMin) {
      digitalWrite(heaterPin, LOW);
      digitalWrite(kipasPinIn, LOW);
      digitalWrite(kipasPinOut, HIGH);
    } else {
      digitalWrite(heaterPin, LOW);
      digitalWrite(kipasPinIn, LOW);
      digitalWrite(kipasPinOut, HIGH);
    }


    // Mengirim data suhu dan kelembapan ke Firebase
    // Firebase.setFloat(firebaseData, "/suhu", t);
    // Firebase.setFloat(firebaseData, "/kelembapan", h);
    // Firebase.setInt(firebaseData, "/timer", timer);

    char t_buffer[10];
    dtostrf(t, 6, 2, t_buffer);
    const char* t_value = t_buffer;
    client.publish("fd/temp", t_value);

    char h_buffer[10];
    dtostrf(h, 6, 2, h_buffer);
    const char* h_value = h_buffer;
    client.publish("fd/humid", h_value);

    char timer_buffer[10];
    itoa(timer, timer_buffer, 10);
    const char* timer_value = timer_buffer;
    client.publish("fd/timer", timer_value);
  }

  if (buzzerOn && millis() - previousMillis >= buzzerInterval) {
    buzzerOn = false;
    noTone(buzzerPin);
  }

  delay(100); // Ubah delay menjadi 100 ms untuk responsivitas keypad
}
