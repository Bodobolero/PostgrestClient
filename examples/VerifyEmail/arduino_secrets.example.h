// copy this file into arduino_secrets.h and replace the #defines with your own values

// your microcontroller needs to access your WiFi network to connect to Neon service in the cloud
#define SECRET_SSID "yourWifiSSID"     // your network SSID (name)
#define SECRET_PASS "yourWifiPassword" // your network password (use for WPA, or use as key for WEP)

// #define NEON_AUTH_URL "https://ep-steep-wind-secret.neonauth.c-2.eu-central-1.aws.neon.tech/neondb/auth"
// #define NEON_DATA_API_URL "https://ep-steep-wind-secret.apirest.c-2.eu-central-1.aws.neon.tech/neondb/rest/v1/"
// following 4 variables manually extracted from AUTH_URL and DATA_API_URL
#define AUTH_HOST "ep-steep-wind-secret.neonauth.c-2.eu-central-1.aws.neon.tech"
#define AUTH_PATH "/neondb/auth"
#define API_HOST "ep-steep-wind-secret.apirest.c-2.eu-central-1.aws.neon.tech"
#define API_PATH "/neondb/rest/v1/"

// valid email that you own - recommended to use "Apple hide my email" or other throw-away email address
#define USER_EMAIL "eingriffe_lerche.1v@icloud.com"
// new strong password
#define USER_PASSWORD "yourverystrongpassword"
// replace with the OTP code you received in your email after "signup with email" and before "verify email"
#define VERIFY_MAIL_OTP "123456"