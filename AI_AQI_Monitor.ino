#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>
#define WEBSERVER_H           // This trick prevents ESPAsyncWebServer from re-defining HTTP methods
#include <ESPAsyncWebServer.h> // to be included after #define WEBSERVER_H  
#include <Adafruit_ADS1X15.h>
#include <Adafruit_SSD1306.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <FirebaseESP8266.h>
#include <ArduinoJson.h>

// --- Configuration & Pins ---
#define DHTPIN 2
#define BLUE_LED 14
#define RED_LED 12
#define YELLOW_LED 13
#define BUZZER 15

// --- Global Objects ---
Adafruit_ADS1115 ads;
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHT22);
AsyncWebServer server(80);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig fbConfig;

// --- Global Variables ---
float mq2_a, mq2_b, mq135, temp, hum, fusedAQI;
int outdoorAQI = 0; 
bool mq2_a_ok = true, mq135_ok = true;
String airAdvice = "Analyzing...";
String systemStatus = "Initializing";
String weatherForecastHtml = "Loading...";

char custom_pincode[7] = "110001";
const char* weatherKey = "YOUR_OPENWEATHER_API_KEY";

// Timing intervals (Restored from Old Code)
unsigned long lastFirebasePush = 0;
unsigned long lastWeatherUpdate = 0;
unsigned long lastWebActivity = 0;

// --- 1. Cloud Intelligence (Geocoding & Weather Table) ---
void updateEnvironmentalContext() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClient client;
  HTTPClient http;

  // Geocoding + Air Pollution
  String geoUrl = "http://api.openweathermap.org/geo/1.0/zip?zip=" + String(custom_pincode) + ",IN&appid=" + String(weatherKey);
  http.begin(client, geoUrl);
  if (http.GET() == HTTP_CODE_OK) {
    DynamicJsonDocument geoDoc(512);
    deserializeJson(geoDoc, http.getStream());
    float lat = geoDoc["lat"], lon = geoDoc["lon"];
    http.end();

    String polUrl = "http://api.openweathermap.org/data/2.5/air_pollution?lat=" + String(lat) + "&lon=" + String(lon) + "&appid=" + String(weatherKey);
    http.begin(client, polUrl);
    if (http.GET() == HTTP_CODE_OK) {
      DynamicJsonDocument polDoc(1024);
      deserializeJson(polDoc, http.getStream());
      outdoorAQI = polDoc["list"][0]["main"]["aqi"];
    }
  }
  http.end();
}

void updateWeather() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClient client;
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/forecast?zip=" + String(custom_pincode) + ",in&units=metric&cnt=24&appid=" + String(weatherKey);

  if (http.begin(client, url)) {
    if (http.GET() == HTTP_CODE_OK) {
      StaticJsonDocument<200> filter;
      filter["list"][0]["dt_txt"] = true;
      filter["list"][0]["main"]["temp"] = true;
      filter["list"][0]["weather"][0]["main"] = true;

      DynamicJsonDocument doc(3072);
      DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));

      if (!error) {
        String html = "<table class='table table-dark table-sm'><tr><th>Date</th><th>°C</th><th>Sky</th></tr>";
        JsonArray list = doc["list"];
        for (size_t i = 0; i < list.size(); i += 8) {
          html += "<tr><td>" + list[i]["dt_txt"].as<String>().substring(5, 10) + "</td>";
          html += "<td>" + String(list[i]["main"]["temp"].as<float>(), 0) + "</td>";
          html += "<td>" + list[i]["weather"][0]["main"].as<String>() + "</td></tr>";
        }
        html += "</table>";
        weatherForecastHtml = html;
      }
    }
    http.end();
  }
}

// --- 2. Full AI Sensor Fusion (Restored Detailed Health Checks) ---
void performSensorFusion() {
  temp = dht.readTemperature();
  hum = dht.readHumidity();
  if (isnan(temp)) temp = 25.0;

  mq2_a = (float)ads.readADC_SingleEnded(0);
  mq2_b = (float)ads.readADC_SingleEnded(1);
  mq135 = (float)ads.readADC_SingleEnded(2);

  float avgMQ2 = (mq2_a + mq2_b) / 2.0;
  float deviation = (avgMQ2 > 10) ? (abs(mq2_a - mq2_b) / avgMQ2) * 100.0 : 0;

  mq2_a_ok = (deviation < 30.0);
  mq135_ok = (mq135 > 50 && mq135 < 18000);

  if (!mq2_a_ok) {
    systemStatus = "Sensor Fault";
    digitalWrite(RED_LED, (millis() / 500) % 2);
    fusedAQI = (min(mq2_a, mq2_b) + mq135) / 2.0;
  } else {
    digitalWrite(RED_LED, LOW);
    if (avgMQ2 > 2000 && mq135 > 1500) {
      systemStatus = "GAS ALERT!";
      digitalWrite(BUZZER, HIGH);
    } else {
      systemStatus = "Normal";
      digitalWrite(BUZZER, LOW);
    }
    fusedAQI = (avgMQ2 * 0.6) + (mq135 * 0.4);
  }

  // Comparison Advice
  int inLvl = (fusedAQI < 150) ? 1 : (fusedAQI < 300) ? 2 : (fusedAQI < 500) ? 3 : 4;
  if (inLvl > outdoorAQI) airAdvice = "OPEN WINDOWS";
  else if (inLvl < outdoorAQI) airAdvice = "STAY INSIDE";
  else airAdvice = "AIR BALANCED";
}

// --- 3. Throttled Firebase Sync (Restored) ---
void updateFirebase() {
  if (WiFi.status() == WL_CONNECTED && (millis() - lastFirebasePush > 20000 || systemStatus == "GAS ALERT!")) {
    lastFirebasePush = millis();
    FirebaseJson json;
    json.add("aqi", (int)fusedAQI);
    json.add("status", systemStatus);
    json.add("temp", temp);
    // As this code throwing Firebase.RTDB.setJSON is Private error/warning (!Firebase.RTDB.setJSON(&fbdo, "/live_data", &json)) Serial.println(fbdo.errorReason()); 
    if (!Firebase.setJSON(fbdo, "/live_data", json)) {
    Serial.println(fbdo.errorReason());
}
  }
}

// --- 4. Mobile-Style OLED UI (Latest Design) ---
void updateOLED() {
  oled.clearDisplay();
  oled.setTextColor(1);
  
  // Status Bar
  oled.setTextSize(1);
  oled.setCursor(0,0);
  oled.print(mq2_a_ok ? "M2:OK" : "M2:ERR");
  oled.setCursor(45,0);
  oled.print(mq135_ok ? "M1:OK" : "M1:ERR");
  
  // WiFi Icon
  if (WiFi.status() == WL_CONNECTED) {
    oled.drawRect(115, 2, 2, 6, 1); oled.drawRect(120, 0, 2, 8, 1);
  }

  oled.drawFastHLine(0, 10, 128, 1);

  // Main Display (Alternating)
  if ((millis() / 3000) % 2 == 0) {
    oled.setCursor(0, 15); oled.print("ROOM AQI");
    oled.setTextSize(3); oled.setCursor(10, 26); oled.print((int)fusedAQI);
  } else {
    oled.setCursor(0, 15); oled.print("ONLINE AQI");
    oled.setTextSize(3); oled.setCursor(10, 26); oled.print(outdoorAQI);
    oled.setTextSize(1); oled.setCursor(80, 40); oled.print("Index");
  }

  // Bottom Advice Bar
  oled.drawFastHLine(0, 50, 128, 1);
  oled.setTextSize(1);
  oled.setCursor(0, 55);
  if (systemStatus == "GAS ALERT!" && (millis() / 200) % 2) {
    oled.print("!!! GAS DETECTED !!!");
  } else {
    oled.print(airAdvice);
  }
  oled.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(BLUE_LED, OUTPUT); pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT); pinMode(BUZZER, OUTPUT);
  
  ads.begin(); oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  lcd.init(); lcd.backlight(); dht.begin();

  WiFiManager wm;
  WiFiManagerParameter p_pin("pin", "Pincode", custom_pincode, 6);
  wm.addParameter(&p_pin);
  if(!wm.autoConnect("AI_Air_Node")) ESP.restart();
  strcpy(custom_pincode, p_pin.getValue());

  fbConfig.database_url = "your-db.firebaseio.com";
  fbConfig.signer.tokens.legacy_token = "your-secret";
  Firebase.begin(&fbConfig, &auth);

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *r){
    lastWebActivity = millis();
    digitalWrite(YELLOW_LED, HIGH);
    String j = "{\"aqi\":" + String(fusedAQI) + ",\"o_aqi\":" + String(outdoorAQI) + 
               ",\"adv\":\"" + airAdvice + "\",\"stat\":\"" + systemStatus + 
               "\",\"temp\":" + String(temp,1) + ",\"hum\":" + String(hum,0) + 
               ",\"weather\":\"" + weatherForecastHtml + "\"}";
    r->send(200, "application/json", j);
  });
  server.begin();
  updateEnvironmentalContext();
  updateWeather();
}

void loop() {
  digitalWrite(BLUE_LED, WiFi.status() == WL_CONNECTED);
  performSensorFusion();
  updateFirebase();
  updateOLED();// Handles the graphical mobile-style UI

  // --- Enhanced LCD Backup Display ---
  // We use the LCD to show constant values so you don't have to 
  // wait for the OLED to "toggle" between Indoor and Online.
  lcd.setCursor(0, 0);
  lcd.print("In:"); 
  lcd.print((int)fusedAQI);
  lcd.print("  Out:"); 
  lcd.print(outdoorAQI); 
  lcd.print("   "); // Clear trailing characters

  lcd.setCursor(0, 1);
  if (systemStatus == "GAS ALERT!") {
    lcd.print("!! GAS ALERT !! ");
  } else {
    // Show Temperature and abbreviated status
    lcd.print(temp, 1);
    lcd.print("C ");
    lcd.print(systemStatus.substring(0, 10));
    lcd.print("      "); // Buffer to clear "Sensor Fault" text
  }

  // --- Cloud & Timers ---
  if (millis() - lastWeatherUpdate > 3600000) {
    lastWeatherUpdate = millis();
    updateEnvironmentalContext();
    updateWeather();
  }

  if (millis() - lastWebActivity > 5000) digitalWrite(YELLOW_LED, LOW);

  delay(200);

  
}