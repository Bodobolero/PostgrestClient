// copy this file into arduino_secrets.h and replace the #defines with your own values

// your microcontroller needs to access your WiFi network to connect to Neon service in the cloud
#define SECRET_SSID "yourWifiSSID"     // your network SSID (name)
#define SECRET_PASS "yourWifiPassword" // your network password (use for WPA, or use as key for WEP)

// SUPABASE_AUTH_URL = "https://uozclsutdytqnxzjqasb.supabase.co/auth/v1/"
// SUPABASE_DATA_API_URL = "https://uozclsutdytqnxzjqasb.supabase.co/rest/v1/"
#define AUTH_HOST "uozclsutdytqnxzjqasb.supabase.co"
#define AUTH_PATH "/auth/v1"
#define API_HOST "uozclsutdytqnxzjqasb.supabase.co"
#define API_PATH "/rest/v1"
#define SUPABASE_PUBLIC_ANON_KEY "sb_publishable_u-4hoznHNNhf6MviMytZNQ_dpYtaMru"

// valid email that you own - recommended to use "Apple hide my email" or other throw-away email address
#define USER_EMAIL "eingriffe_lerche.1v@icloud.com"
// new strong password
#define USER_PASSWORD "yourverystrongpassword"