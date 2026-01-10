// PostgrestClient example - https://github.com/bodobolero/PostgrestClient
// Copyright © 2025, Peter Bendel
// MIT License

// ---------------------------------------------------------------------------------------------------------
// This example uses a DH20 temperature and humidity sensor and periodically perists the current
// temperature and humidity in the Postgres database.
// ---------------------------------------------------------------------------------------------------------

#include <SPI.h>
#include <WiFiNINA.h>        // use WifiClient implementation for your microcontroller
#include "arduino_secrets.h" // copy arduino_secrets.h.example to arduino_secrets.h and fill in your secrets
#include <PostgrestClient.h>

#include "Wire.h" // I2C library
#include <Wire.h>
#include <AHT20.h>
AHT20 AHT;      // Temperature and Humidity sensor using I2C, good 0.3 degree Celcius precision
float dht_hum;  // humidity
float dht_temp; // temperature
bool toggle = false;

#include <Arduino_LSM6DSOX.h>

// Oled, see https://github.com/olikraus/u8g2/issues/1850
#define U8X8_DO_NOT_SET_WIRE_CLOCK
#include "U8x8lib.h" // OLED Display
U8X8_SSD1306_128X64_NONAME_HW_I2C Oled(/* reset=*/U8X8_PIN_NONE);

unsigned long last_calc = 0;   // last time we took the time
unsigned long last_upload = 0; // last time we stored in postgres

// Watchdog to make sure the script can run unattended 7x24 for many days
// You can use the Watchdog interface to set up a hardware watchdog timer that resets
// the system in the case of system failures or malfunctions.
// see https://os.mbed.com/docs/mbed-os/v6.16/apis/watchdog.html
#include <mbed.h>

///////please enter your sensitive data in the Secret tab/arduino_secrets.h (see arduino_secrets.example.h) //////////
char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password (use for WPA, or use as key for WEP)

int status = WL_IDLE_STATUS;

WiFiClient client;
SelfHostedPostgrestClient pgClient(client, AUTH_HOST, AUTH_PATH, API_HOST, API_PATH, POSTGREST_PORT);

void setup()
{
  // Initialize serial and wait for port to open:
  Serial.begin(115200);

  delay(1500);

  if (!IMU.begin())
  {
    Serial.println("Failed to initialize IMU!");
    while (1)
      ;
  }

  if (!AHT.begin())
  {
    Serial.println("Failed to initialize I2C communication with AHT20");
    while (1)
      ;
  }
  Serial.println("AHT20 initialized");

  // initialize OLED
  //  Oled.setBusClock(400000u);  // set bus clock to same speed as other i2c devices integrated on RP2040 Connect board
  if (!Oled.begin())
  {
    Serial.println("Failed to initialize I2C communication with Oled display");
    while (1)
      ;
  }
  Oled.setFlipMode(true);
  Serial.println("Oled initialized");

  // This delay gives the chance to wait for a Serial Monitor without blocking if none is found
  delay(1500);

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE)
  {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true)
      ;
  }
  last_calc = millis();
  last_upload = millis();
  delay(5000);
  // watchdog to make sure the script can run unattended 7x24 for many days
  // watchdog resets board after max_timeout if it is not kicked
  // so that we start all over
  mbed::Watchdog::get_instance().start();

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    if (status != WL_CONNECTED)
    {
      delay(10000);
    }
  }
  Serial.println("Connected to WiFi");

  mbed::Watchdog::get_instance().kick();

  Serial.println("\nSigning in with email in self-hosted Postgrest auth...");
  const char *errorMessage = pgClient.signIn(USER_EMAIL, USER_PASSWORD);
  if (errorMessage)
  {
    Serial.print("Sign in failed: ");
    Serial.println(errorMessage);
    return;
  }
  else
  {
    Serial.println("Sign in successful.");
    pgClient.printJwt();
  }
  mbed::Watchdog::get_instance().kick(); // restart the watchdog timer
}

void loop()
{
  // read temperature every six seconds (every second AHT is not available)
  if ((millis() - last_calc) > 3000)
  {
    if (toggle)
    {
      dht_temp = AHT.getTemperature();
      Serial.print("Temperature: ");
      Serial.print(dht_temp);
      toggle = false;

      if ((millis() - last_upload) > 300000)
      { // every 5 minutes - persist in postgres
        Serial.println("\nInserting sensor values...");
        JsonDocument &request = pgClient.getJsonRequest();
        request.clear();
        request["sensor_name"] = "Hobbyraum";
        request["temperature"] = dht_temp;
        request["humidity"] = dht_hum;
        const char *errorMessage = pgClient.doPost("/temphum_values");
        if (errorMessage)
        {
          Serial.print("Insert failed: ");
          Serial.println(errorMessage);
          return;
        }
        else
        {
          Serial.print("Insert successful:");
          Serial.print(" temperature=");
          Serial.print(dht_temp);
          Serial.print(" humidity=");
          Serial.println(dht_hum);
          // only reset watchdog if we had a successful execution
          mbed::Watchdog::get_instance().kick(); // Reset the watchdog timer
          last_upload = millis();
        }
      }
      else
      {
        mbed::Watchdog::get_instance().kick(); // Reset the watchdog timer
      }
    }
    else
    {
      dht_hum = AHT.getHumidity();
      Serial.print("Humidity: ");
      Serial.print(dht_hum);
      toggle = true;
      mbed::Watchdog::get_instance().kick(); // Reset the watchdog timer
    }
    last_calc = millis();
    oledDisplayTemperature(dht_temp, dht_hum);
    checkAndPrintWiFiStatus();
  }
}

void oledDisplayTemperature(float temperature, float humidity)
{
  Oled.setFont(u8x8_font_amstrad_cpc_extended_r);
  // print temperature
  Oled.setCursor(0, 1);
  Oled.print("Temperatur: ");
  Oled.setCursor(3, 2);
  Oled.print(temperature);
  Oled.print(" C   "); // The unit for  Celsius because        original arduino don't support speical symbols
  // // print humidity data
  Oled.setCursor(0, 3);
  Oled.print("Luftfeuchtigk.:");
  Oled.setCursor(3, 4);
  Oled.print(humidity);
  Oled.print(" %  ");

  Oled.refreshDisplay(); // Update the Display
}

void checkAndPrintWiFiStatus()
{
  status = WiFi.status();
  while (status != WL_CONNECTED)
  {
    WiFi.disconnect();
    // wait 10 seconds before trying to connect again
    delay(1000);
    Serial.print("Lost connection - attempting to re-connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    mbed::Watchdog::get_instance().kick(); // Reset the watchdog timer
  }
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}