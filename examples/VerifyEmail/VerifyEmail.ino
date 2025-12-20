// PostgrestClient example - https://github.com/bodobolero/PostgrestClient
// Copyright © 2025, Peter Bendel
// MIT License

// ---------------------------------------------------------------------------------------------------------
// This is a basic example that does not need any sensors or actuators connected to your microcontroller.
// It only shows how to complete the sign up to the Neon Auth service by verifying your email with an OTP code.
// Before this example use the SignupWithEmail example to sign up with your email and password.
// After successful invocation of this example you can continue with other examples that need a verified email address.
// Note that for security reasons it is recommended to do the signup and email verification NOT from the arduino device.
// Better is to do it manually using curl or from a secure backend server.
// See the curlscripts/ directory in the librarie's repository for example curl scripts that do the signup and email verification.
// Replace VERIFY_MAIL_OTP in arduino_secrets.h with the OTP you received in your email after signup.
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
PostgrestClient pgClient(client, AUTH_HOST, AUTH_PATH, API_HOST, API_PATH);

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

    // create table for sensor values
    Serial.println("\nVerifying your email in Neon auth...");
    const char *errorMessage = pgClient.verifyEmail(USER_EMAIL, VERIFY_MAIL_OTP);
    if (errorMessage)
    {
        Serial.print("Email verification failed: ");
        Serial.println(errorMessage);
    }
    else
    {
        Serial.println("Verification successful.");
        Serial.println("You can now user your email/password combination to sign in to Neon auth.");
    }
}

void loop()
{
    // nothing to do here
}