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

#include "Wire.h"     // I2C library
#include "DHT.h"      // DHT 20 humidity and temperature sensor library
#define DHTTYPE DHT20 // DHT 20 (Temperature and Humidity sensor)

// Watchdog to make sure the script can run unattended 7x24 for many days
// You can use the Watchdog interface to set up a hardware watchdog timer that resets
// the system in the case of system failures or malfunctions.
// see https://os.mbed.com/docs/mbed-os/v6.16/apis/watchdog.html
#include <mbed.h>

///////please enter your sensitive data in the Secret tab/arduino_secrets.h (see arduino_secrets.example.h) //////////
char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password (use for WPA, or use as key for WEP)

int status = WL_IDLE_STATUS;

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
WiFiSSLClient client;
PostgrestClient pgClient(client, AUTH_HOST, AUTH_PATH, API_HOST, API_PATH);

DHT dht(DHTTYPE); // DHT20 don't need to define Pin, uses I2C I/F

void setup()
{
    // Initialize serial and wait for port to open:
    Serial.begin(9600);
    while (!Serial)
    {
        ; // wait for serial port to connect. Needed for native USB port only
    }

    // check for the WiFi module:
    if (WiFi.status() == WL_NO_MODULE)
    {
        Serial.println("Communication with WiFi module failed!");
        // don't continue
        while (true)
            ;
    }
    // watchdog to make sure the script can run unattended 7x24 for many days
    // watchdog resets board after max_timeout if it is not kicked
    // so that we start all over
    mbed::Watchdog::get_instance().start();

    // attempt to connect to WiFi network:
    while (status != WL_CONNECTED)
    {
        // wait 10 seconds before trying to connect
        delay(10000);
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(ssid);
        // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
        status = WiFi.begin(ssid, pass);
    }
    Serial.println("Connected to WiFi");

    // init Temperature and Humidity sensor
    Wire.begin();
    dht.begin();

    Serial.println("\nSigning in with email in Neon auth...");
    const char *errorMessage = "not yet signed in";
    while (errorMessage)
    {
        errorMessage = pgClient.signIn(USER_EMAIL, USER_PASSWORD);
        if (errorMessage)
        {
            Serial.print("Sign in failed: ");
            Serial.println(errorMessage);
            delay(10000); // wait 10 seconds before retrying
        }
        else
        {
            Serial.println("Sign in successful.");
            pgClient.printJwt();
        }
    }
}

void loop()
{
    float temp_hum_val[2] = {0};

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    if (!dht.readTempAndHumidity(temp_hum_val))
    {
        Serial.print("Humidity: ");
        Serial.print(temp_hum_val[0]);
        Serial.print(" %\t");
        Serial.print("Temperature: ");
        Serial.print(temp_hum_val[1]);
        Serial.println(" Degree Celsius");
    }
    else
    {
        Serial.println("Could not read sensor values from DHT20");
    }

    checkAndPrintWiFiStatus();

    Serial.println("\nInserting sensor values...");
    JsonDocument &request = pgClient.getJsonRequest();
    request.clear();
    JsonArray arr = request.to<JsonArray>();
    JsonObject temp = arr.createNestedObject();
    temp["sensor_name"] = "temperature";
    temp["sensor_value"] = temp_hum_val[1];
    JsonObject hum = arr.createNestedObject();
    hum["sensor_name"] = "humidity";
    hum["sensor_value"] = temp_hum_val[0];
    const char *errorMessage = pgClient.doPost("/sensorvalues");
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
        Serial.print(temp_hum_val[1]);
        Serial.print(" humidity=");
        Serial.println(temp_hum_val[0]);
        // only reset watchdog if we had a successful execution
        mbed::Watchdog::get_instance().kick(); // Reset the watchdog timer
    }
}

void checkAndPrintWiFiStatus()
{
    status = WiFi.status();
    while (status != WL_CONNECTED)
    {
        WiFi.disconnect();
        // wait 10 seconds before trying to connect again
        delay(10000);
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