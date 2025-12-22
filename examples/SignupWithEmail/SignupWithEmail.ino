// PostgrestClient example - https://github.com/bodobolero/PostgrestClient
// Copyright © 2025, Peter Bendel
// MIT License

// ---------------------------------------------------------------------------------------------------------
// This is a basic example that does not need any sensors or actuators connected to your microcontroller.
// It only shows how to sign up to the Neon Auth service with email and password.
// Note that the signup process needs to be continued within a few minutes by verifying the email address
// using the OTP code that is sent to the email address you provide during signup.
// For security reasons it is recommended to do the signup and email verification NOT from the arduino device.
// Better is to do it manually using curl or from a secure backend server.
// See the curlscripts/ directory in the librarie's repository for example curl scripts that do the signup and email verification.
// After successful invocation of this example continue with the VerifyEmailWithOTP example to verify your email address.
// ---------------------------------------------------------------------------------------------------------

#include <SPI.h>
#include <WiFiNINA.h>        // use WifiClient implementation for your microcontroller
#include "arduino_secrets.h" // copy arduino_secrets.h.example to arduino_secrets.h and fill in your secrets
#include <PostgrestClient.h>

///////please enter your sensitive data in the Secret tab/arduino_secrets.h (see arduino_secrets.example.h) //////////
char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password (use for WPA, or use as key for WEP)

int status = WL_IDLE_STATUS;

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
WiFiSSLClient client;
NeonPostgrestClient pgClient(client, AUTH_HOST, AUTH_PATH, API_HOST, API_PATH);

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

    Serial.println("\nSigning up your email in Neon auth...");
    const char *errorMessage = pgClient.signUp(USER_NAME, USER_EMAIL, USER_PASSWORD);
    if (errorMessage)
    {
        Serial.print("Sign up failed: ");
        Serial.println(errorMessage);
    }
    else
    {
        Serial.println("Sign up successful. Now verify your email using the OTP code sent to your email address.");
        Serial.println("Continue with the VerifyEmailWithOTP example.");
    }
}

void loop()
{
    // nothing to do here
}