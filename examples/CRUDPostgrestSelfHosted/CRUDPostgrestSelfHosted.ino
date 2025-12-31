// PostgrestClient example - https://github.com/bodobolero/PostgrestClient
// Copyright © 2025, Peter Bendel
// MIT License

// ---------------------------------------------------------------------------------------------------------
// This is a basic example that does not need any sensors or actuators connected to your microcontroller.
// It only shows how to sign in to the Neon Auth service with email and password.
// Note that the signup process needs to be completed before the signin.
// For security reasons it is recommended to do the signup and email verification NOT from the arduino device.
// Better is to do it manually using curl or from a secure backend server.
// See the curlscripts/ directory in the librarie's repository for example curl scripts that do the signup and email verification.
// ---------------------------------------------------------------------------------------------------------

#include <SPI.h>
#include <WiFiNINA.h>        // use WifiClient implementation for your microcontroller
#include "arduino_secrets.h" // copy arduino_secrets.h.example to arduino_secrets.h and fill in your secrets
#include <PostgrestClient.h>

///////please enter your sensitive data in the Secret tab/arduino_secrets.h (see arduino_secrets.example.h) //////////
char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password (use for WPA, or use as key for WEP)

int status = WL_IDLE_STATUS;

// we don't use a reverse proxy like NGINX which supports https in front of Postgrest
// so we need to use HTTP instead of HTTPS
// WiFiSSLClient client;
WiFiClient client;

SelfHostedPostgrestClient pgClient(client, AUTH_HOST, AUTH_PATH, API_HOST, API_PATH);

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

    Serial.println("\nInserting sensor values...");
    JsonDocument &request = pgClient.getJsonRequest();
    request["sensor_name"] = "temperature";
    request["sensor_value"] = 21.5;
    errorMessage = pgClient.doPost("/sensorvalues");
    if (errorMessage)
    {
        Serial.print("Insert failed: ");
        Serial.println(errorMessage);
        return;
    }
    else
    {
        Serial.println("Insert successful.");
    }

    Serial.println("\nRetrieving sensor values...");
    errorMessage = pgClient.doGet("/sensorvalues?sensor_name=eq.temperature");
    if (errorMessage)
    {
        Serial.print("GET sensorvalues failed: ");
        Serial.println(errorMessage);
        return;
    }
    else
    {
        Serial.println("Get successful.");
        JsonDocument &response = pgClient.getJsonResult();
        serializeJsonPretty(response, Serial);
        Serial.println();
    }

    Serial.println("\n Updating sensor values > 21.0 to 17.5...");
    request = pgClient.getJsonRequest();
    request["sensor_value"] = 17.5;
    errorMessage = pgClient.doPatch("/sensorvalues?sensor_value=gt.21.0");
    if (errorMessage)
    {
        Serial.print("Update failed: ");
        Serial.println(errorMessage);
        return;
    }
    else
    {
        Serial.println("Update successful.");
        Serial.println("\nRetrieving sensor values...");
        errorMessage = pgClient.doGet("/sensorvalues?sensor_name=eq.temperature");
        if (errorMessage)
        {
            Serial.print("GET sensorvalues failed: ");
            Serial.println(errorMessage);
            return;
        }
        else
        {
            Serial.println("Get successful.");
            JsonDocument &response = pgClient.getJsonResult();
            serializeJsonPretty(response, Serial);
            Serial.println();
        }
    }
    Serial.println("\n Deleting sensor values < 20.0...");
    errorMessage = pgClient.doDelete("/sensorvalues?sensor_value=lt.20.0");
    if (errorMessage)
    {
        Serial.print("Delete failed: ");
        Serial.println(errorMessage);
        return;
    }
    else
    {
        Serial.println("Delete successful.");
        Serial.println("\nRetrieving sensor values...");
        errorMessage = pgClient.doGet("/sensorvalues?sensor_name=eq.temperature");
        if (errorMessage)
        {
            Serial.print("GET sensorvalues failed: ");
            Serial.println(errorMessage);
            return;
        }
        else
        {
            Serial.println("Get successful.");
            JsonDocument &response = pgClient.getJsonResult();
            serializeJsonPretty(response, Serial);
            Serial.println();
        }
    }
}

void loop()
{
    // nothing to do here
}