# AI-AQI-Monitor

### *Advanced Local-to-Cloud Environment Monitor with ESP8266*

This project is a high-precision IoT monitoring station that fuses local air quality data with global atmospheric intelligence. Using an ESP8266, it monitors indoor pollutants, fetches real-time outdoor AQI via the OpenWeather API, and provides actionable health advice through a dual-display interface.


---

## ✨ Features
* **Dual-Display Interface:**
    * **OLED (SSD1306):** Mobile-style UI with status icons (WiFi signal, Sensor Health) and alternating AQI screens.
    * **LCD (16x2):** Persistent data dashboard for real-time monitoring of critical vitals without screen toggling.
* **AI Sensor Fusion:**
    * Fuses data from **MQ-2** (Smoke/LPG) and **MQ-135** (General Pollutants).
    * **System Health Check:** Monitors deviation between dual sensors; if one fails, it flags a "Sensor Fault" but maintains operation using the healthy node.
* **Cloud Intelligence:**
    * **OpenWeather API:** Real-time outdoor AQI fetching based on your local Pincode.
    * **Decision Logic:** Compares Indoor vs. Outdoor levels to advise: "OPEN WINDOWS" or "STAY INSIDE."
    * **Firebase Integration:** Real-time data streaming for remote monitoring.
* **Smart Connectivity:** Powered by **WiFiManager** for easy configuration of WiFi and Pincodes via a smartphone portal.

---

## 🔌 Hardware Configuration

| Component | Pin (NodeMCU) | Function |
| :--- | :--- | :--- |
| **ADS1115 (I2C)** | D2 (SDA), D1 (SCL) | 16-bit ADC for high-precision gas readings |
| **SSD1306 OLED** | D2 (SDA), D1 (SCL) | Graphical User Interface |
| **LCD 16x2 I2C** | D2 (SDA), D1 (SCL) | Persistent Status Dashboard |
| **DHT22** | D4 (GPIO 2) | Temperature & Humidity |
| **Buzzer** | D8 (GPIO 15) | Audible Gas Alarm |
| **RGB/Status LEDs** | D5, D6, D7


Troubleshooting the Compilation Warning for LCD
If you want to get that "Warning" text out of your console for a "cleaner" build:
Open your Arduino Library folder (usually Documents/Arduino/libraries).
Find the LiquidCrystal_I2C folder and open the library.properties file.
Find the line architectures=* or architectures=avr.
Change it to architectures=esp8266,avr,*.
Save and re-compile. The warning will vanish.
