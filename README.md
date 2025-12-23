# PostgrestClient library

PostgrestClient is a library for Arduino, Mbed, and ESPxx microcontrollers that lets you directly store and retrieve data in PostgreSQL databases using the PostgREST extension.

PostgREST is an open-source PostgreSQL extension (see https://docs.postgrest.org/) and serves a RESTful API from any PostgreSQL database.
Major cloud DBaaS providers like neon.com and supabase.com have adopted PostgREST and provide a Data API that is PostgREST-compatible.

This library provides a thin client wrapper using ArduinoJson to make it easy to connect your microcontroller to the PostgREST Data API.

Use this to store sensor data directly in a PostgreSQL database without a middleman server such as Home Assistant.
You can also read values from the database and use them to control actuators — any application that can write to the database can trigger actions on your microcontroller project.

## Important note

To allow different WiFi client implementations the library does not depend on a specific WiFi client library.
Instead, you must manually install a WiFi client library that implements the `WiFiClient` interface for your microcontroller.
This library has been tested with the following WiFi client libraries:
- [WiFiNINA](https://docs.arduino.cc/libraries/wifinina/) (for example, Arduino RP2040 Connect)
- [Espressif ESP32 WiFi library](https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi) — tested with XIAO ESP32C6

## Important security note

This library compiles credentials (for example, email/password for your PostgreSQL DBaaS auth and WiFi credentials from `arduino_secrets.h`) into the binary deployed to the microcontroller.

It is important to correctly set up authentication and Row-Level Security (RLS) on your database so that the credentials only provide access to the subset of data you want exposed to the microcontroller.

We recommend signing up a specific auth user with a throwaway email (for example, Apple Hide My Email) for each microcontroller in the authentication service of your PostgreSQL DBaaS provider (examples below).

If your microcontroller is compromised (for example, stolen), you can delete the specific auth user from the auth database to revoke that device's access.

## Usage overview

For an introduction to the PostgREST API with examples, see https://docs.postgrest.org/en/stable/references/api/tables_views.html

### Example PostgreSQL table used in usage overview

```sql
CREATE TABLE "sensorvalues" (
 "measure_time" timestamp DEFAULT now(),
 ...
 "sensor_name" text,
 "sensor_value" double precision,
 CONSTRAINT "sensorvalues_pkey" PRIMARY KEY("sensor_name","measure_time")
);
```

### Instantiate an instance of the PostgrestClient

arduino_secrets.h - do NOT commit your arduino_secrets.h to your source code repository and use a .gitignore rule

```c
// Neon project specific hostname
#define AUTH_HOST "ep-steep-wind-redacted.neonauth.c-2.eu-central-1.aws.neon.tech"
// neon auth route
#define AUTH_PATH "/neondb/auth"
// Neon project specific hostname
#define API_HOST "ep-steep-wind-redacted.apirest.c-2.eu-central-1.aws.neon.tech"
// Neon PostgREST route
#define API_PATH "/neondb/rest/v1"
// replace your own values: Neon auth user's email and password
#define USER_EMAIL "eingriffe_lerche.1v@icloud.com"
#define USER_PASSWORD "yourverystrongpassword"
// WiFi credentials
#define SECRET_SSID "MyGuestNetwork"
#define SECRET_PASS "MyVerySecureWifiPassword"
```

CRUDPostgrestSketch.ino

```c
#include <WiFiNINA.h>        // use WiFiClient implementation for your microcontroller
#include "arduino_secrets.h" // copy arduino_secrets.h.example to arduino_secrets.h and fill in your secrets
#include <PostgrestClient.h>

char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password (use for WPA, or use as key for WEP)

WiFiSSLClient client;
NeonPostgrestClient pgClient(client, AUTH_HOST, AUTH_PATH, API_HOST, API_PATH);
```

### Connect to WiFi and authenticate with the PostgreSQL authentication server to generate a JWT

```c
status = WiFi.begin(ssid, pass);
...
const char *errorMessage = pgClient.signIn(USER_EMAIL, USER_PASSWORD);
```

### Insert sensor values

```c
JsonDocument &request = pgClient.getJsonRequest();
request["sensor_name"] = "temperature";
request["sensor_value"] = 21.5;
errorMessage = pgClient.doPost("/sensorvalues");
```

### Query / retrieve sensor values

```c
    errorMessage = pgClient.doGet("/sensorvalues?sensor_name=eq.temperature");
    ...
    JsonDocument &response = pgClient.getJsonResult();
    ...
    serializeJsonPretty(response, Serial);
```

### Update values

```c
    request = pgClient.getJsonRequest();
    request["sensor_value"] = 17.5;
    errorMessage = pgClient.doPatch("/sensorvalues?sensor_value=gt.21.0");
```

### Delete rows based on search filter

```c
errorMessage = pgClient.doDelete("/sensorvalues?sensor_value=lt.20.0");
```

## Vendor support

This  library currently provides specific subclasses for 

- Neon DBaaS (neon.com) with Data API and RLS enabled
- Supabase DBaaS (supabase.com) with PostgREST and RLS enabled

## Prerequisites

### PostgreSQL with PostgREST extension

The library has been tested with the RESTful data apis provided by Neon and Supabase.
Other vendors could work out of the box (not tested) or easily added by adding additional subclasses.
Your own deployment of PostgREST on PostgreSQL should also work if you enable authentication with a JWT.

### WiFi

You need a microcontroller fast enough to run an SSL client (like BearSSL) and a Wifi client (like WiFiNINA or Espressif ESP32 Wifi library).

Your WiFi controller must have a supported library that implements the `WiFiClient` interface

```C
// WifiClient.h
class WiFiClient : public Client {
    ...
}
```
for example 
- Arduino WiFiNINA https://docs.arduino.cc/libraries/wifinina/
- [Espressif ESP32 Wifi library](https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/src/WiFi.h)

### ArduinoJson library (bblanchon/ArduinoJson)

This library defines a dependency on the ArduinoJson library which is automatically installed by the Arduino IDE.
You can also install a specific version >=  7.3 of the library manually from the Arduino IDE Library Manager.

This library is used to create and parse the JSON payloads that are sent to and received from the PostgreSQL database's PostgREST extension and/or auth provider.

### base64 library (Densaugeo/base64_arduino)

Needed to extract some information (like token expiry) from the base64-encoded JWT

## API reference

See [src/PostgrestClient.h](src/PostgrestClient.h) for the latest API reference (doc comments)

## List of examples

### Examples directory

- [SignupWithEmail](examples/SignupWithEmail) — Neon sketch to sign up a new user at Neon auth with email/password; creates an email with an OTP token for verification (using the curl scripts is recommended).
- [VerifyEmail](examples/VerifyEmail) — Neon sketch to confirm your email by entering the received token (using the curl scripts is recommended).
- [SignInWithEmail](examples/SignInWithEmail) — Neon sketch to sign in with email/password and generate a JWT.
- [CRUDPostgrest](examples/CRUDPostgrest) — Neon sketch that shows a complete sign-in, Create, Read, Update, Delete flow.
- [CRUDPostgrestSupabase](examples/CRUDPostgrestSupabase) — Supabase sketch that shows a complete sign-in, Create, Read, Update, Delete flow.
- [TemperatureHumiditySensor](examples/TemperatureHumiditySensor) — A realistic example that periodically reads sensor values and writes them into a PostgreSQL database. Shows how to use the Mbed watchdog to recover from connection failures for unattended operation.

### Directory curlscripts_neon

We provide bash scripts that show the same REST API invocations used by the library with curl. The first two scripts can be used to register a new user with email/password at the Neon authentication service.

- [curlscripts_neon/01_auth_signup_with_email.sh](curlscripts_neon/01_auth_signup_with_email.sh) — Sign up a new user at Neon auth with email/password (creates an email with an OTP token).
- [curlscripts_neon/02_auth_verify_email.sh](curlscripts_neon/02_auth_verify_email.sh) — Confirm your email by entering the received token.
- [curlscripts_neon/03_auth_signin_with_email.sh](curlscripts_neon/03_auth_signin_with_email.sh) — Sign in with email/password and generate a JWT.
- [curlscripts_neon/04_data_api_insert_row.sh](curlscripts_neon/04_data_api_insert_row.sh) — Use the JWT to call the data API and insert a row.
- [curlscripts_neon/05_data_api_get_rows.sh](curlscripts_neon/05_data_api_get_rows.sh) — Use the JWT to call the data API and retrieve rows.

### Directory curlscripts_supabase

We provide bash scripts that show the same REST API invocations used by the library with curl. The first two scripts can be used to register a new user with email/password at the Supabase authentication service.

- [curlscripts_supabase/01_auth_signup_with_email.sh](curlscripts_supabase/01_auth_signup_with_email.sh) — Sign up a new user at Supabase auth with email/password (creates an email with an OTP token).
- [curlscripts_supabase/02_auth_verify_email.sh](curlscripts_supabase/02_auth_verify_email.sh) — Confirm your email by entering the received token.
- [curlscripts_supabase/03_auth_signin_with_email.sh](curlscripts_supabase/03_auth_signin_with_email.sh) — Sign in with email/password and generate a JWT.
- [curlscripts_supabase/04_data_api_insert_row.sh](curlscripts_supabase/04_data_api_insert_row.sh) — Use the JWT to call the data API and insert a row.
- [curlscripts_supabase/05_data_api_get_rows.sh](curlscripts_supabase/05_data_api_get_rows.sh) — Use the JWT to call the data API and retrieve rows.

### Database schema and RLS policies used in the examples

#### Neon

Schema

```sql
DROP TABLE IF EXISTS "actorvalues";
DROP TABLE IF EXISTS "sensorvalues";
--CREATE SCHEMA "public";
CREATE TABLE "actorvalues" (
 "sent_time" timestamp DEFAULT now(),
    user_id text DEFAULT (auth.user_id()) NOT NULL,
 "acknowledge_time" timestamp,
 "actor_name" text,
 "actor_value" text NOT NULL,
 CONSTRAINT "actorvalues_pkey" PRIMARY KEY("actor_name","sent_time")
);
CREATE TABLE "sensorvalues" (
 "measure_time" timestamp DEFAULT now(),
    user_id text DEFAULT (auth.user_id()) NOT NULL,
 "sensor_name" text,
 "sensor_value" double precision,
 CONSTRAINT "sensorvalues_pkey" PRIMARY KEY("sensor_name","measure_time")
);
-- Schema usage
GRANT USAGE ON SCHEMA public TO authenticated;
-- For existing tables
GRANT SELECT, UPDATE, INSERT, DELETE ON ALL TABLES IN SCHEMA public TO authenticated;
-- For future tables
ALTER DEFAULT PRIVILEGES IN SCHEMA public
GRANT SELECT, UPDATE, INSERT, DELETE ON TABLES TO authenticated;
-- For sequences (for identity columns)
GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO authenticated;
```

enable RLS

```sql
ALTER TABLE "actorvalues" ENABLE ROW LEVEL SECURITY;
ALTER TABLE "sensorvalues" ENABLE ROW LEVEL SECURITY;
-- policies
-- allow only users access to their own rows, regardless of select, insert, update, delete
CREATE POLICY "Own actor data only" ON "actorvalues" AS PERMISSIVE FOR ALL TO "authenticated" USING ((SELECT auth.user_id()= "actorvalues"."user_id")) WITH CHECK (auth.user_id()= "actorvalues"."user_id");

CREATE POLICY "Own sensor data only" ON "sensorvalues" AS PERMISSIVE FOR ALL TO "authenticated" USING ((SELECT auth.user_id()= "sensorvalues"."user_id")) WITH CHECK (auth.user_id()= "sensorvalues"."user_id");
```


#### Supabase

Schema

```sql
DROP TABLE IF EXISTS "actorvalues";
DROP TABLE IF EXISTS "sensorvalues";
--CREATE SCHEMA "public";
CREATE TABLE "actorvalues" (
 "sent_time" timestamp DEFAULT now(),
    user_id uuid DEFAULT (auth.uid()) NOT NULL,
 "acknowledge_time" timestamp,
 "actor_name" text,
 "actor_value" text NOT NULL,
 CONSTRAINT "actorvalues_pkey" PRIMARY KEY("actor_name","sent_time")
);
CREATE TABLE "sensorvalues" (
 "measure_time" timestamp DEFAULT now(),
    user_id uuid DEFAULT (auth.uid()) NOT NULL,
 "sensor_name" text,
 "sensor_value" double precision,
 CONSTRAINT "sensorvalues_pkey" PRIMARY KEY("sensor_name","measure_time")
);
-- Schema usage
GRANT USAGE ON SCHEMA public TO authenticated;
-- For existing tables
GRANT SELECT, UPDATE, INSERT, DELETE ON ALL TABLES IN SCHEMA public TO authenticated;
-- For future tables
ALTER DEFAULT PRIVILEGES IN SCHEMA public
GRANT SELECT, UPDATE, INSERT, DELETE ON TABLES TO authenticated;
-- For sequences (for identity columns)
GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO authenticated;
```

## Enable RLS

```sql
ALTER TABLE "actorvalues" ENABLE ROW LEVEL SECURITY;
ALTER TABLE "sensorvalues" ENABLE ROW LEVEL SECURITY;
-- policies
-- allow only users access to their own rows, regardless of select, insert, update, delete
CREATE POLICY "Own actor data only" ON "actorvalues" AS PERMISSIVE FOR ALL TO authenticated USING ((SELECT auth.uid()= "actorvalues"."user_id")) WITH CHECK (auth.uid()= "actorvalues"."user_id");

CREATE POLICY "Own sensor data only" ON "sensorvalues" AS PERMISSIVE FOR ALL TO authenticated USING ((SELECT auth.uid()= "sensorvalues"."user_id")) WITH CHECK (auth.uid()= "sensorvalues"."user_id");
```

## Enabling PostgREST, Neon Auth and RLS on Neon

- create a new project in the Neon console https://console.neon.tech
- enable the Data API (which is a synonym for Postgrest in Neon's implementation) https://console.neon.tech/app/projects/<yourproject>/branches/<yourbranch>/data-api
  - use Neon Auth and enable Row-Level Security (RLS) 
- Configure Neon auth https://console.neon.tech/app/projects/<yourproject>/branches/<yourbranch>/auth?tab=configuration
    - Enable Sign-up with Email
    - Enable Verify at Sign-up 
    - Select: Verification method: Verification code
    - Enable Sign-in with Email
    - Email provider: when testing this library we have used the Neon default email provider with sender auth@mail.myneon.app
- Either use the Neon console SQL Editor or any database client to create your database schema, GRANTS to RLS roles like "authenticated" and RLS policy. An example schema and RLS policies used in the examples is given above

## Enabling PostgREST, Supabase Auth and RLS on Supabase

- create a new project in the Supabase console https://supabase.com/dashboard/organizations
- the PostgREST Data API is enabled by default for Supabase projects. WARNING: configure RLS and RLS policies carefully to avoid exposing all data in the public schema to everyone on the internet
- configure sign up with email verification and token in the Supabase Auth console https://supabase.com/dashboard/project/<yourproject>/auth/templates/confirm-sign-up and use an email template that includes the token, for example

```html
<h2>One time login code</h2>

<p>Please enter this code: {{ .Token }}</p>
```

- Either use the Supabse console SQL Editor or any database client to create your database schema, GRANTS to RLS roles like "authenticated" and RLS policy. An example schema and RLS policies used in the examples is given above.



