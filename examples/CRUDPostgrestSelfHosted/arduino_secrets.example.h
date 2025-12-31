// copy this file into arduino_secrets.h and replace the #defines with your own values

// your microcontroller needs to access your WiFi network to connect your postgrest installation
#define SECRET_SSID "yourWifiSSID"     // your network SSID (name)
#define SECRET_PASS "yourWifiPassword" // your network password (use for WPA, or use as key for WEP)

#define AUTH_HOST "192.168.178.90:3000"
#define AUTH_PATH ""
#define API_HOST "192.168.178.90:3000"
#define API_PATH ""

// valid email that you own - recommended to use "Apple hide my email" or other throw-away email address
#define USER_EMAIL "alice@example.com"
// new strong password
#define USER_PASSWORD "S3curePassw0rd!"