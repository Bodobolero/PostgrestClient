#ifndef POSTGRESTCLIENT_H
#define POSTGRESTCLIENT_H
#include <ArduinoJson.h>
#include "WiFiClient.h"
#include <cstring>

// need to decode base64 encoded JWT tokens
#define BASE64_URL
#include <base64.hpp>

#ifndef MAX_JWT_LENGTH
#define MAX_JWT_LENGTH 8192
#endif

#define ERROR_NOT_SIGNED_IN "Not signed in"

uint32_t jwt_get_claim_u32_scan(const char *jwt, const char *claim); // see implementation below

class PostgrestClient
{
public:
    virtual ~PostgrestClient() {}

    // Vendor-specific operations: default to not implemented
    virtual const char *signUp(const char *name, const char *email, const char *password, unsigned long timeout = 20000)
    {
        (void)name;
        (void)email;
        (void)password;
        (void)timeout;
        return "not implemented";
    }
    virtual const char *verifyEmail(const char *email, const char *otp, unsigned long timeout = 20000)
    {
        (void)email;
        (void)otp;
        (void)timeout;
        return "not implemented";
    }
    virtual const char *signIn(const char *email, const char *password)
    {
        (void)email;
        (void)password;
        return "not implemented";
    }

    // Print current JWT and its timestamps to Serial for debugging
    virtual void printJwt()
    {
        Serial.print("JWT: ");
        if (_jwtBuffer[0])
            Serial.println(_jwtBuffer);
        else
            Serial.println("<none>");
        Serial.print("token lifetime (s): ");
        Serial.println(_tokenExpiry);
        Serial.print("local time when token issued: ");
        Serial.println(_internalTimeIat / 1000U);
        Serial.print("current local time: ");
        Serial.println(millis() / 1000U);
        Serial.print("token expires in (s): ");
        Serial.println(_tokenExpiry - (millis() - _internalTimeIat) / 1000U);
    }

    JsonDocument &getJsonRequest()
    {
        return request;
    }

    JsonDocument &getJsonResult()
    {
        return response;
    }

    // Common data API methods (shared across vendors)
    const char *doGet(const char *route, unsigned long timeout = 20000)
    {
        if (!_isSignedIn)
            return ERROR_NOT_SIGNED_IN;
        const char *error = refreshTokenIfNeeded();

        if (error)
            return error;
        response.clear();
        error = invokeDataAPI("GET", route, timeout, true);
        request.clear();
        if (error)
            return error;

        return nullptr;
    }

    const char *doPost(const char *route, unsigned long timeout = 20000)
    {
        if (!_isSignedIn)
            return ERROR_NOT_SIGNED_IN;
        const char *error = refreshTokenIfNeeded();
        if (error)
            return error;
        error = invokeDataAPI("POST", route, timeout, false);
        request.clear();
        if (error)
            return error;

        return nullptr;
    }

    const char *doPatch(const char *route, unsigned long timeout = 20000)
    {
        if (!_isSignedIn)
            return ERROR_NOT_SIGNED_IN;
        const char *error = refreshTokenIfNeeded();
        if (error)
            return error;
        error = invokeDataAPI("PATCH", route, timeout, false);
        request.clear();
        if (error)
            return error;

        return nullptr;
    }

    const char *doDelete(const char *route, unsigned long timeout = 20000)
    {
        if (!_isSignedIn)
            return ERROR_NOT_SIGNED_IN;
        const char *error = refreshTokenIfNeeded();
        if (error)
            return error;
        error = invokeDataAPI("DELETE", route, timeout, false);
        request.clear();
        if (error)
            return error;

        return nullptr;
    }

protected:
    // base constructor: only subclasses should create concrete clients
    PostgrestClient(WiFiClient &client) : _client(client), _authHost(nullptr), _authPath(nullptr), _apiHost(nullptr), _apiPath(nullptr), _email(nullptr), _password(nullptr), _isSignedIn(false), _tokenExpiry(0), _internalTimeIat(0)
    {
        request.clear();
        response.clear();
        _status[0] = '\0';
        _jwtBuffer[0] = '\0';
    }

    // Validate current JWT expiry and re-signin if necessary.
    // Returns nullptr on success, or an error message on failure.
    virtual const char *refreshTokenIfNeeded()
    {
        if (!_isSignedIn)
            return ERROR_NOT_SIGNED_IN;

        if (_tokenExpiry == 0)
        {
            if (!_email || !_password)
                return "no credentials to refresh token";
            return signIn(_email, _password);
        }

        unsigned long elapsed_ms = 0;
        if (millis() >= _internalTimeIat)
            elapsed_ms = millis() - _internalTimeIat;
        else
            elapsed_ms = 0;

        if (elapsed_ms / 1000U + 60U >= _tokenExpiry)
        {
            if (!_email || !_password)
                return "no credentials to refresh token";
            return signIn(_email, _password);
        }

        return nullptr;
    }

    // common helper to send requests to the data API
    const char *invokeDataAPI(const char *verb, const char *pathSuffix, unsigned long timeout = 20000, bool expectJsonResult = false)
    {
        if (!_client.connect(_apiHost, 443))
        {
            return "cannot connect to data api host over Wifi";
        }

        _client.print(verb);
        _client.print(" ");
        _client.print(_apiPath);
        _client.print(pathSuffix);
        _client.println(" HTTP/1.1");
        _client.print("Host: ");
        _client.println(_apiHost);
        _client.println("Content-Type: application/json");
        _client.print("Authorization: Bearer ");
        _client.println(_jwtBuffer);

        if (strncmp(verb, "GET", 3) != 0)
        {
            _client.print("Content-Length: ");
            size_t length = measureJson(request);
            _client.print(length);
            _client.print("\r\n\r\n");

            size_t written = serializeJson(request, _client);
            if (written != length)
            {
                _client.stop();
                return "payload serialization error";
            }
        }
        else
        {
            _client.print("\r\n");
        }
        _client.flush();

        unsigned long ms = millis();
        while (!_client.available() && millis() - ms < timeout)
        {
            delay(0);
        }

        if (!_client.available())
        {
            _client.stop();
            return "request timed out";
        }

        size_t bytes_read = _client.readBytesUntil('\n', _status, sizeof(_status) - 1);
        _status[bytes_read] = 0;
        int status_code = 0;
        if (bytes_read >= 12)
            sscanf(_status + 9, "%3d", &status_code);
        if (status_code < 200 || status_code >= 300)
        {
            _client.stop();
            return _status;
        }

        if (expectJsonResult)
        {
            char endOfHeaders[] = "\r\n\r\n";
            if (!_client.find(endOfHeaders))
            {
                _client.stop();
                return "Invalid response";
            }
            DeserializationError err = deserializeJson(response, _client);
            if (err)
            {
                _client.stop();
                return err.c_str();
            }
        }
        _client.stop();
        return nullptr;
    }

protected:
    // common members (protected so vendor subclasses can access)
    WiFiClient &_client;
    const char *_authHost;
    const char *_authPath;
    const char *_apiHost;
    const char *_apiPath;
    const char *_email;    // need to remember for re-signin
    const char *_password; // need to remember for re-signin
    bool _isSignedIn;
    char _status[64];
    uint32_t _tokenExpiry;     // token lifetime in seconds
    uint32_t _internalTimeIat; // millis() at time token was issued
    char _jwtBuffer[MAX_JWT_LENGTH];

    // payload for requests and responses - one at a time
    JsonDocument request;
    JsonDocument response;
};

// Neon specific subclass
class NeonPostgrestClient : public PostgrestClient
{
public:
    NeonPostgrestClient(WiFiClient &client, const char *authHost, const char *authPath, const char *apiHost, const char *apiPath)
        : PostgrestClient(client)
    {
        _authHost = authHost;
        _authPath = authPath;
        _apiHost = apiHost;
        _apiPath = apiPath;
        _email = nullptr;
        _password = nullptr;
        _isSignedIn = false;
        _tokenExpiry = 0;
        _internalTimeIat = 0;
        _sessionCookie[0] = '\0';
        _jwtBuffer[0] = '\0';
        request.clear();
        response.clear();
    }

    // Implement Neon-specific auth flows
    const char *signUp(const char *name, const char *email, const char *password, unsigned long timeout = 20000) override
    {
        request.clear();
        request["email"] = email;
        request["password"] = password;
        request["name"] = name;

        const char *err = postJsonAuth("/sign-up/email", timeout);
        if (err)
            return err;

        JsonObject user = response["user"].as<JsonObject>();
        if (user.isNull())
            return "no user in response";
        const char *resEmail = user["email"];
        const char *resName = user["name"];
        if (!resEmail || !resName)
            return "user missing email or name";
        if (strcmp(resEmail, email) != 0)
            return "email mismatch";
        if (strcmp(resName, name) != 0)
            return "name mismatch";

        request.clear();
        response.clear();

        return nullptr;
    }

    const char *verifyEmail(const char *email, const char *otp, unsigned long timeout = 20000) override
    {
        request.clear();
        request["email"] = email;
        request["otp"] = otp;

        const char *err = postJsonAuth("/email-otp/verify-email", timeout, true);
        if (err)
            return err;

        bool ok = response["status"].as<bool>();
        JsonObject user = response["user"].as<JsonObject>();
        if (!ok)
            return "verification status false";
        if (user.isNull())
            return "no user in response";
        const char *resEmail = user["email"];
        bool emailVerified = user["emailVerified"].as<bool>();
        if (!resEmail)
            return "user missing email";
        if (strcmp(resEmail, email) != 0)
            return "email mismatch";
        if (!emailVerified)
            return "email not verified";

        request.clear();
        response.clear();

        return nullptr;
    }

    const char *signIn(const char *email, const char *password) override
    {
        _email = email;
        _password = password;

        request.clear();
        response.clear();
        request["email"] = email;
        request["password"] = password;
        const char *err = postJsonAuth("/sign-in/email", 20000, true);
        if (err)
            return err;

        if (_sessionCookie[0] == '\0')
            return "no session token in sign-in response";

        request.clear();
        response.clear();

        const char *err2 = getSessionJWTWithCookie(20000);
        if (err2)
            return err2;

        request.clear();
        response.clear();

        return nullptr;
    }

private:
    // Neon-specific helpers (not virtual)
    const char *postJsonAuth(const char *pathSuffix, unsigned long timeout, bool setCookie = false)
    {
        if (!_client.connect(_authHost, 443))
        {
            return "cannot connect to auth host over Wifi";
        }

        _client.print("POST ");
        _client.print(_authPath);
        _client.print(pathSuffix);
        _client.println(" HTTP/1.1");
        _client.print("Host: ");
        _client.println(_authHost);
        _client.println("Content-Type: application/json");
        _client.println("Accept: application/json");
        _client.println("Origin: https://example.com");
        _client.print("Content-Length: ");
        size_t length = measureJson(request);
        _client.print(length);
        _client.print("\r\n\r\n");

        size_t written = serializeJson(request, _client);
        if (written != length)
        {
            _client.stop();
            return "payload serialization error";
        }
        _client.flush();

        unsigned long ms = millis();
        while (!_client.available() && millis() - ms < timeout)
        {
            delay(0);
        }

        if (!_client.available())
        {
            _client.stop();
            return "request timed out";
        }

        size_t bytes_read = _client.readBytesUntil('\n', _status, sizeof(_status) - 1);
        _status[bytes_read] = 0;
        int status_code = 0;
        if (bytes_read >= 12)
            sscanf(_status + 9, "%3d", &status_code);
        if (status_code < 200 || status_code >= 300)
        {
            _client.stop();
            return _status;
        }
        if (setCookie)
        {
            _sessionCookie[0] = '\0';

            while (true)
            {
                size_t hlen = _client.readBytesUntil('\n', _sessionCookie, sizeof(_sessionCookie) - 1);
                if (hlen == 0)
                    break;
                _sessionCookie[hlen] = 0;
                if (hlen == 1 && _sessionCookie[0] == '\r')
                    break;
                const char *p = _sessionCookie;
                while (*p == ' ' || *p == '\t')
                    p++;
                if (strncmp(p, "Set-Cookie:", 11) == 0 || strncmp(p, "set-cookie:", 11) == 0)
                {
                    const char *val = p + 11;
                    while (*val == ' ' || *val == '\t')
                        val++;
                    const char *found = strstr(val, "__Secure-neon-auth.session_token=");
                    if (found)
                    {
                        const char *cookie_start = found + strlen("__Secure-neon-auth.session_token=");
                        const char *end = cookie_start;
                        while (*end && *end != ';' && *end != '\r' && *end != '\n')
                            end++;
                        size_t clen = (size_t)(end - cookie_start);
                        if (clen >= sizeof(_sessionCookie))
                            clen = sizeof(_sessionCookie) - 1;
                        memmove(_sessionCookie, cookie_start, clen);
                        _sessionCookie[clen] = 0;
                        break;
                    }
                }
            }
        }

        char endOfHeaders[] = "\r\n\r\n";
        if (!_client.find(endOfHeaders))
        {
            _client.stop();
            return "Invalid response";
        }

        DeserializationError err = deserializeJson(response, _client);
        if (err)
        {
            _client.stop();
            return err.c_str();
        }
        _client.stop();
        return nullptr;
    }

    const char *getSessionJWTWithCookie(unsigned long timeout)
    {
        if (_sessionCookie[0] == '\0')
            return "empty session token";

        if (!_client.connect(_authHost, 443))
        {
            return "cannot connect to auth host over Wifi";
        }

        _client.print("GET ");
        _client.print(_authPath);
        _client.print("/get-session");
        _client.println(" HTTP/1.1");
        _client.print("Host: ");
        _client.println(_authHost);
        _client.println("Accept: application/json");
        _client.println("Origin: https://example.com");
        _client.print("Cookie: __Secure-neon-auth.session_token=");
        _client.println(_sessionCookie);
        _client.print("\r\n");
        _client.flush();

        unsigned long ms = millis();
        while (!_client.available() && millis() - ms < timeout)
        {
            delay(0);
        }
        if (!_client.available())
        {
            _client.stop();
            return "get-session timed out";
        }

        size_t bytes_read = _client.readBytesUntil('\n', _status, sizeof(_status) - 1);
        _status[bytes_read] = 0;
        int status_code = 0;
        if (bytes_read >= 12)
            sscanf(_status + 9, "%3d", &status_code);
        if (status_code < 200 || status_code >= 300)
        {
            _client.stop();
            return _status;
        }

        bool haveJwt = false;
        while (true)
        {
            size_t hlen = _client.readBytesUntil('\n', _jwtBuffer, sizeof(_jwtBuffer) - 1);
            if (hlen == 0)
                break;
            _jwtBuffer[hlen] = 0;
            if (hlen == 1 && _jwtBuffer[0] == '\r')
                break;
            const char *p = _jwtBuffer;
            while (*p == ' ' || *p == '\t')
                p++;
            if (strncmp(p, "set-auth-jwt:", 13) == 0 || strncmp(p, "Set-Auth-Jwt:", 13) == 0)
            {
                const char *val = p + 13;
                while (*val == ' ' || *val == '\t')
                    val++;
                size_t vlen = strnlen(val, sizeof(_jwtBuffer));
                if (vlen && val[vlen - 1] == '\r')
                    vlen--;
                if (vlen >= sizeof(_jwtBuffer))
                    vlen = sizeof(_jwtBuffer) - 1;
                memmove(_jwtBuffer, val, vlen);
                _jwtBuffer[vlen] = 0;
                haveJwt = true;
                break;
            }
        }

        char endOfHeaders[] = "\r\n\r\n";
        if (!_client.find(endOfHeaders))
        {
            _client.stop();
            return "Invalid response";
        }

        DeserializationError derr = deserializeJson(response, _client);
        if (derr)
        {
            _client.stop();
            return derr.c_str();
        }
        _client.stop();

        if (!haveJwt)
        {
            return "no jwt in get-session response";
        }

        uint32_t tokenIat = jwt_get_claim_u32_scan(_jwtBuffer, "\"iat\"");
        uint32_t tokenExpiry = jwt_get_claim_u32_scan(_jwtBuffer, "\"exp\"");
        _tokenExpiry = tokenExpiry - tokenIat;
        _internalTimeIat = millis();
        _isSignedIn = true;

        return nullptr;
    }

    // Neon specific members
    char _sessionCookie[MAX_JWT_LENGTH];
};

// Supabase specific subclass (templates)
class SupabasePostgrestClient : public PostgrestClient
{
public:
    SupabasePostgrestClient(WiFiClient &client, const char *authHost, const char *authPath, const char *apiHost, const char *apiPath)
        : PostgrestClient(client)
    {
        _authHost = authHost;
        _authPath = authPath;
        _apiHost = apiHost;
        _apiPath = apiPath;
        _email = nullptr;
        _password = nullptr;
        _isSignedIn = false;
        _tokenExpiry = 0;
        _internalTimeIat = 0;
        _jwtBuffer[0] = '\0';
        request.clear();
        response.clear();
    }

    // empty templates to be implemented for Supabase
    const char *signUp(const char *name, const char *email, const char *password, unsigned long timeout = 20000) override
    {
        (void)name;
        (void)email;
        (void)password;
        (void)timeout;
        return "not implemented for supabase";
    }
    const char *verifyEmail(const char *email, const char *otp, unsigned long timeout = 20000) override
    {
        (void)email;
        (void)otp;
        (void)timeout;
        return "not implemented for supabase";
    }
    const char *signIn(const char *email, const char *password) override
    {
        (void)email;
        (void)password;
        return "not implemented for supabase";
    }
};

static const char *
find_char_n(const char *s, size_t n, char c)
{
    const void *p = memchr((const void *)s, (unsigned char)c, n);
    return (const char *)p;
}

static const char *find_sub_n(const char *s, size_t n, const char *sub, size_t sublen)
{
    if (!s || !sub || sublen == 0 || n < sublen)
        return nullptr;
    for (size_t i = 0; i + sublen <= n; i++)
    {
        if (memcmp(s + i, sub, sublen) == 0)
            return s + i;
    }
    return nullptr;
}

uint32_t jwt_get_claim_u32_scan(const char *jwt, const char *claim)
{
    if (!jwt || !claim)
        return 0;

    size_t jwt_len = strnlen(jwt, MAX_JWT_LENGTH + 1);
    if (jwt_len == 0 || jwt_len > MAX_JWT_LENGTH)
        return 0;

    const char *dot1 = find_char_n(jwt, jwt_len, '.');
    if (!dot1)
        return 0;
    size_t after_dot1_n = (size_t)(jwt + jwt_len - (dot1 + 1));
    const char *dot2 = find_char_n(dot1 + 1, after_dot1_n, '.');
    if (!dot2)
        return 0;

    const char *payload = dot1 + 1;
    size_t b64_len = (size_t)(dot2 - payload);
    if (b64_len == 0 || b64_len > MAX_JWT_LENGTH)
        return 0;

    size_t pad = (4 - (b64_len % 4)) % 4;
    size_t b64_padded_len = b64_len + pad;
    size_t decoded_max = (b64_padded_len / 4) * 3;

    char *b64 = (char *)alloca(b64_padded_len + 1);
    if (!b64)
        return 0;
    memcpy(b64, payload, b64_len);
    for (size_t i = 0; i < pad; i++)
        b64[b64_len + i] = '=';
    b64[b64_padded_len] = '\0';

    char *decoded = (char *)alloca(decoded_max + 1);
    if (!decoded)
        return 0;
    unsigned int decoded_len = decode_base64((unsigned char *)b64, (unsigned char *)decoded);
    if (decoded_len == 0 || decoded_len > decoded_max)
        return 0;
    decoded[decoded_len] = '\0';

    size_t claimlen = strnlen(claim, decoded_len);
    if (claimlen == 0)
        return 0;
    const char *k = find_sub_n(decoded, decoded_len, claim, claimlen);
    if (!k)
        return 0;

    const char *p = k + claimlen;
    const char *end = decoded + decoded_len;
    while (p < end && isspace((unsigned char)*p))
        p++;
    if (p >= end || *p != ':')
        return 0;
    p++;
    while (p < end && isspace((unsigned char)*p))
        p++;
    if (p >= end || !isdigit((unsigned char)*p))
        return 0;

    uint32_t v = 0;
    while (p < end && isdigit((unsigned char)*p))
    {
        uint32_t d = (uint32_t)(*p - '0');
        if (v > 429496729U || (v == 429496729U && d > 5U))
            return 0;
        v = v * 10U + d;
        p++;
    }
    return v;
}

#endif // POSTGRESTCLIENT_H
#ifndef POSTGRESTCLIENT_H
#define POSTGRESTCLIENT_H
#include <ArduinoJson.h>
#include "WiFiClient.h"
#include <cstring>

// need to decode base64 encoded JWT tokens
#define BASE64_URL
#include <base64.hpp>

#ifndef MAX_JWT_LENGTH
#define MAX_JWT_LENGTH 8192
#endif

#define ERROR_NOT_SIGNED_IN "Not signed in"

uint32_t jwt_get_claim_u32_scan(const char *jwt, const char *claim); // see implementation below

class PostgrestClient
{
public:
    // Assuming the following Neon URLs, replace with your own as neeeded
    // NEON_AUTH_URL    "https://ep-steep-wind-refactored.neonauth.c-2.eu-central-1.aws.neon.tech/neondb/auth"
    // NEON_DATA_API_URL "https://ep-steep-wind-refactored.apirest.c-2.eu-central-1.aws.neon.tech/neondb/rest/v1/"
    // we need to decompose the URL into host and endpoint path for the WiFiClient as follows:
    // authHost: "ep-steep-wind-refactored.neonauth.c-2.eu-central-1.aws.neon.tech"
    // authPath: "/neondb/auth"
    // apiHost: "ep-steep-wind-refactored.apirest.c-2.eu-central-1.aws.neon.tech"
    // apiPath: "/neondb/rest/v1"
    // all paths must start with a leading '/' and end without a trailing '/'
    PostgrestClient(WiFiClient &client, const char *authHost, const char *authPath, const char *apiHost, const char *apiPath)
        : _client(client), _authHost(authHost), _authPath(authPath), _apiHost(apiHost), _apiPath(apiPath),
          _email(nullptr), _password(nullptr), _isSignedIn(false),
          _tokenExpiry(0), _internalTimeIat(0)
    {
        request.clear();
        response.clear();
    }

    /**
     * @brief Sign up a new user with name, email and password
     *
     * @param name User's full name
     * @param email User's email address
     * @param password User's password
     * @param timeout Maximum time in milliseconds to wait for response
     *
     * @return nullptr on success, error message in case of failure
     * Note: it is recommended to to the sign up NOT from the arduino device.
     * Better is to do it manually using curl or from a secure backend server.
     * Curl example:
     *
     * curl -i -X POST \
     * "<NEON_AUTH_URL>/email-otp/verify-email" \
     * -H "Content-Type: application/json" \
     * -H "Accept: application/json" \
     * -H "Origin: https://example.com" \
     * -d '{
     * "email": "you@your.domain",
     * "password": "your_very_secure_password",
     * "name": "Your Name"
     * }'
     *
     */
    const char *signUp(const char *name, const char *email, const char *password, unsigned long timeout = 20000)
    {
        // Prepare request JSON using provided name
        request.clear();
        request["email"] = email;
        request["password"] = password;
        request["name"] = name;

        const char *err = postJsonAuth("/sign-up/email", timeout);
        if (err)
            return err;

        // Validate response contains user with correct email and name
        JsonObject user = response["user"].as<JsonObject>();
        if (user.isNull())
            return "no user in response";
        const char *resEmail = user["email"];
        const char *resName = user["name"];
        if (!resEmail || !resName)
            return "user missing email or name";
        if (strcmp(resEmail, email) != 0)
            return "email mismatch";
        if (strcmp(resName, name) != 0)
            return "name mismatch";

        request.clear();
        response.clear();

        return nullptr;
    }

    /**
     * @brief Verify email using OTP code sent to the user's email address
     *
     * @param email User's email address
     * @param otp One-time password code received via email
     * @param timeout Maximum time in milliseconds to wait for response
     *
     * @return nullptr on success, error message in case of failure
     * Note: it is recommended to to the email verification NOT from the arduino device.
     * Better is to do it manually using curl or from a secure backend server.
     * Curl example:
     * curl -i -X POST \
     * "<NEON_AUTH_URL>/email-otp/verify-email" \
     * -H "Content-Type: application/json" \
     * -H "Accept: application/json" \
     * -H "Origin: https://example.com" \
     * -d '{
     * "email": "peter@familie-bendel.de",
     * "otp": "293185"
     * }'
     */
    const char *verifyEmail(const char *email, const char *otp, unsigned long timeout = 20000)
    {
        // Prepare request JSON
        request.clear();
        request["email"] = email;
        request["otp"] = otp;

        const char *err = postJsonAuth("/email-otp/verify-email", timeout, true);
        if (err)
            return err;

        // Validate response: check status true and user.email matches and emailVerified==true
        bool ok = response["status"].as<bool>();
        JsonObject user = response["user"].as<JsonObject>();
        if (!ok)
            return "verification status false";
        if (user.isNull())
            return "no user in response";
        const char *resEmail = user["email"];
        bool emailVerified = user["emailVerified"].as<bool>();
        if (!resEmail)
            return "user missing email";
        if (strcmp(resEmail, email) != 0)
            return "email mismatch";
        if (!emailVerified)
            return "email not verified";

        request.clear();
        response.clear();

        return nullptr;
    }

    /**
     * @brief Sign in an existing user with email and password
     *
     * @param email
     * @param password
     * @return const char* 0 or error message
     */
    const char *signIn(const char *email, const char *password)
    {
        // Store for re-signin
        _email = email;
        _password = password;

        // First request: POST /sign-in/email to obtain session token (and Set-Cookie)
        request.clear();
        response.clear();
        request["email"] = email;
        request["password"] = password;
        const char *err = postJsonAuth("/sign-in/email", 20000, true);
        if (err)
            return err;

        // session token may be present in Set-Cookie header captured into _sessionCookie
        if (_sessionCookie[0] == '\0')
            return "no session token in sign-in response";

        request.clear();
        response.clear();

        // Serial.println("Phase 1 completed. Session cookie:");
        // Serial.println(_sessionCookie);

        // Second request: GET /get-session with Cookie header to retrieve JWT from header
        const char *err2 = getSessionJWTWithCookie(20000);
        if (err2)
            return err2;

        request.clear();
        response.clear();

        return nullptr;
    }

    // Print current JWT and its timestamps to Serial for debugging
    void printJwt()
    {
        Serial.print("JWT: ");
        if (_jwtBuffer[0])
            Serial.println(_jwtBuffer);
        else
            Serial.println("<none>");
        Serial.print("token lifetime (s): ");
        Serial.println(_tokenExpiry);
        Serial.print("local time when token issued: ");
        Serial.println(_internalTimeIat / 1000U);
        Serial.print("current local time: ");
        Serial.println(millis() / 1000U);
        Serial.print("token expires in (s): ");
        Serial.println(_tokenExpiry - (millis() - _internalTimeIat) / 1000U);
    }

    JsonDocument &getJsonRequest()
    {
        return request;
    }

    JsonDocument &getJsonResult()
    {
        return response;
    }

    // query the given route and return results in getJsonResult()
    // all routes must start with a leading '/'
    // route can be like
    // retrieve all items: "/item"
    // retrieve all people younger than 13: "/people?age=lt.13"
    // see https://docs.postgrest.org/en/v14/references/api/tables_views.html
    const char *doGet(const char *route, unsigned long timeout = 20000)
    {
        if (!_isSignedIn)
            return ERROR_NOT_SIGNED_IN;
        const char *error = refreshTokenIfNeeded();

        if (error)
            return error;
        response.clear();
        // Perform GET request to _apiUrl + route with Authorization: Bear
        error = invokeDataAPI("GET", route, timeout, true);
        request.clear();
        if (error)
            return error;

        return nullptr;
    }

    // insert tuples
    // post the given route with payload from getJsonRequest() and return results in getJsonResult()
    // all routes must start with a leading '/'
    // route can be like
    // insert new item: "/item"
    // see https://docs.postgrest.org/en/v14/references/api/tables_views.html
    const char *doPost(const char *route, unsigned long timeout = 20000)
    {
        if (!_isSignedIn)
            return ERROR_NOT_SIGNED_IN;
        const char *error = refreshTokenIfNeeded();
        if (error)
            return error;
        // Perform POST request to _apiUrl + route with Authorization: Bear
        error = invokeDataAPI("POST", route, timeout, false);
        request.clear();
        if (error)
            return error;

        return nullptr;
    }

    // update tuples
    // patch the given route with payload from getJsonRequest() and return results in getJsonResult()
    // all routes must start with a leading '/'
    // route can be like
    // update item with id=5: "/item?id=eq.5"
    const char *doPatch(const char *route, unsigned long timeout = 20000)
    {
        if (!_isSignedIn)
            return ERROR_NOT_SIGNED_IN;
        const char *error = refreshTokenIfNeeded();
        if (error)
            return error;
        // Perform PATCH request to _apiUrl + route with Authorization: Bear
        error = invokeDataAPI("PATCH", route, timeout, false);
        request.clear();
        if (error)
            return error;

        return nullptr;
    }

    // delete tuples
    // delete the given route and return results in getJsonResult()
    // all routes must start with a leading '/'
    // route can be like
    // delete item with id=5: "/item?id=eq.5"
    const char *doDelete(const char *route, unsigned long timeout = 20000)
    {
        if (!_isSignedIn)
            return ERROR_NOT_SIGNED_IN;
        const char *error = refreshTokenIfNeeded();
        if (error)
            return error;
        // Perform PATCH request to _apiUrl + route with Authorization: Bear
        error = invokeDataAPI("DELETE", route, timeout, false);
        request.clear();
        if (error)
            return error;

        return nullptr;
    }

private:
    // Helper to POST the current `request` JsonDocument to the auth path + `pathSuffix`.
    // On success the parsed JSON response is stored in `response` and nullptr is returned.
    // On error an error message (pointer to `_status` or static string) is returned.
    const char *postJsonAuth(const char *pathSuffix, unsigned long timeout, bool setCookie = false)
    {
        // Connect to auth host
        if (!_client.connect(_authHost, 443))
        {
            return "cannot connect to auth host over Wifi";
        }

        // Send HTTP request headers (print path parts directly)
        _client.print("POST ");
        _client.print(_authPath);
        _client.print(pathSuffix);
        _client.println(" HTTP/1.1");
        _client.print("Host: ");
        _client.println(_authHost);
        _client.println("Content-Type: application/json");
        _client.println("Accept: application/json");
        _client.println("Origin: https://example.com");
        _client.print("Content-Length: ");
        size_t length = measureJson(request);
        _client.print(length);
        _client.print("\r\n\r\n");

        size_t written = serializeJson(request, _client);
        if (written != length)
        {
            _client.stop();
            return "payload serialization error";
        }
        _client.flush();

        // wait for response
        unsigned long ms = millis();
        while (!_client.available() && millis() - ms < timeout)
        {
            delay(0);
        }

        if (!_client.available())
        {
            _client.stop();
            return "request timed out";
        }

        size_t bytes_read = _client.readBytesUntil('\n', _status, sizeof(_status) - 1);
        _status[bytes_read] = 0;
        // Serial.println("HTTP status line:");
        // Serial.println(_status); // TODO remove for production
        int status_code = 0;
        if (bytes_read >= 12)
            sscanf(_status + 9, "%3d", &status_code);
        if (status_code < 200 || status_code >= 300)
        {
            _client.stop();
            return _status;
        }
        if (setCookie)
        {

            // Reset session cookie buffer and reuse it as a header line buffer
            _sessionCookie[0] = '\0';

            // Read headers line by line directly into _sessionCookie to avoid extra stack buffers.
            // Stop early once we've captured the session cookie so we don't consume the JSON body.
            while (true)
            {
                size_t hlen = _client.readBytesUntil('\n', _sessionCookie, sizeof(_sessionCookie) - 1);
                if (hlen == 0)
                    break;
                _sessionCookie[hlen] = 0; // includes possible '\r'
                // remove for production
                // Serial.println(_sessionCookie);
                // blank line -> end of headers
                if (hlen == 1 && _sessionCookie[0] == '\r')
                    break;
                const char *p = _sessionCookie;
                while (*p == ' ' || *p == '\t')
                    p++;
                // Check for Set-Cookie header
                if (strncmp(p, "Set-Cookie:", 11) == 0 || strncmp(p, "set-cookie:", 11) == 0)
                {
                    const char *val = p + 11;
                    while (*val == ' ' || *val == '\t')
                        val++;
                    // look for session cookie name
                    const char *found = strstr(val, "__Secure-neon-auth.session_token=");
                    if (found)
                    {
                        const char *cookie_start = found + strlen("__Secure-neon-auth.session_token=");
                        // copy until ';' or CR
                        const char *end = cookie_start;
                        while (*end && *end != ';' && *end != '\r' && *end != '\n')
                            end++;
                        size_t clen = (size_t)(end - cookie_start);
                        if (clen >= sizeof(_sessionCookie))
                            clen = sizeof(_sessionCookie) - 1;
                        // Move the cookie value to the start of the buffer
                        memmove(_sessionCookie, cookie_start, clen);
                        _sessionCookie[clen] = 0;
                        // remove for production
                        // Serial.println("Found session cookie:");
                        // Serial.println(_sessionCookie);
                        // stop reading more header lines now; remaining headers will be skipped below
                        break;
                    }
                }
            }
        }

        // skip remaining headers
        char endOfHeaders[] = "\r\n\r\n";
        if (!_client.find(endOfHeaders))
        {
            _client.stop();
            return "Invalid response";
        }

        // read json, remove for production
        // size_t hlen = _client.readBytesUntil('\n', _sessionCookie, sizeof(_sessionCookie) - 1);
        // if (hlen != 0)
        // {
        //     _sessionCookie[hlen] = 0; // includes possible '\r'
        //     // TODO remove for production
        //     Serial.println("JSON response:");
        //     Serial.println(_sessionCookie);
        // }

        // Parse JSON response into `response` member
        DeserializationError err = deserializeJson(response, _client);
        if (err)
        {
            _client.stop();
            return err.c_str();
        }
        _client.stop();
        return nullptr;
    }

    // helper to send a request to the data API host
    const char *invokeDataAPI(const char *verb, const char *pathSuffix, unsigned long timeout = 20000, bool expectJsonResult = false)
    {
        // Connect to auth host
        if (!_client.connect(_apiHost, 443))
        {
            return "cannot connect to data api host over Wifi";
        }

        // Send HTTP request headers (print path parts directly)
        _client.print(verb);
        _client.print(" ");
        _client.print(_apiPath);
        _client.print(pathSuffix);
        _client.println(" HTTP/1.1");
        _client.print("Host: ");
        _client.println(_apiHost);
        _client.println("Content-Type: application/json");
        // _client.println("Accept: application/json");
        // -H "Authorization: Bearer ${JWT}"
        _client.print("Authorization: Bearer ");
        _client.println(_jwtBuffer);

        if (strncmp(verb, "GET", 3) != 0)
        {
            _client.print("Content-Length: ");
            size_t length = measureJson(request);
            _client.print(length);
            _client.print("\r\n\r\n");

            size_t written = serializeJson(request, _client);
            if (written != length)
            {
                _client.stop();
                return "payload serialization error";
            }
        }
        else
        {
            // For GET requests we must terminate headers with an empty line so the server
            // knows the request is complete. For non-GET requests the Content-Length
            // and the body (with a following CRLFCRLF) are written above.
            _client.print("\r\n");
        }
        _client.flush();

        // wait for response
        unsigned long ms = millis();
        while (!_client.available() && millis() - ms < timeout)
        {
            delay(0);
        }

        if (!_client.available())
        {
            _client.stop();
            return "request timed out";
        }

        size_t bytes_read = _client.readBytesUntil('\n', _status, sizeof(_status) - 1);
        _status[bytes_read] = 0;
        // Serial.println("HTTP status line:");
        // Serial.println(_status); // TODO remove for production
        int status_code = 0;
        if (bytes_read >= 12)
            sscanf(_status + 9, "%3d", &status_code);
        if (status_code < 200 || status_code >= 300)
        {
            _client.stop();
            return _status;
        }

        if (expectJsonResult)
        {
            // skip remaining headers
            char endOfHeaders[] = "\r\n\r\n";
            if (!_client.find(endOfHeaders))
            {
                _client.stop();
                return "Invalid response";
            }
            // Parse JSON response into `response` member
            DeserializationError err = deserializeJson(response, _client);
            if (err)
            {
                _client.stop();
                return err.c_str();
            }
        }
        _client.stop();
        return nullptr;
    }

    // Helper: perform GET /get-session with given session cookie value, parse JWT and store it.
    const char *getSessionJWTWithCookie(unsigned long timeout)
    {
        if (_sessionCookie[0] == '\0')
            return "empty session token";

        if (!_client.connect(_authHost, 443))
        {
            return "cannot connect to auth host over Wifi";
        }

        _client.print("GET ");
        _client.print(_authPath);
        _client.print("/get-session");
        _client.println(" HTTP/1.1");
        _client.print("Host: ");
        _client.println(_authHost);
        _client.println("Accept: application/json");
        _client.println("Origin: https://example.com");
        _client.print("Cookie: __Secure-neon-auth.session_token=");
        _client.println(_sessionCookie);
        _client.print("\r\n");
        _client.flush();

        unsigned long ms = millis();
        while (!_client.available() && millis() - ms < timeout)
        {
            delay(0);
        }
        if (!_client.available())
        {
            _client.stop();
            return "get-session timed out";
        }

        // Read status line
        size_t bytes_read = _client.readBytesUntil('\n', _status, sizeof(_status) - 1);
        _status[bytes_read] = 0;
        int status_code = 0;
        if (bytes_read >= 12)
            sscanf(_status + 9, "%3d", &status_code);
        if (status_code < 200 || status_code >= 300)
        {
            _client.stop();
            return _status;
        }

        // Read headers line by line into the large _jwtBuffer and capture set-auth-jwt header if present
        bool haveJwt = false;
        while (true)
        {
            size_t hlen = _client.readBytesUntil('\n', _jwtBuffer, sizeof(_jwtBuffer) - 1);
            if (hlen == 0)
                break;
            _jwtBuffer[hlen] = 0; // includes possible '\r'
            // blank line (only CR) -> end of headers
            if (hlen == 1 && _jwtBuffer[0] == '\r')
                break;
            // check for set-auth-jwt header (case-insensitive check for common case)
            const char *p = _jwtBuffer;
            while (*p == ' ' || *p == '\t')
                p++;
            if (strncmp(p, "set-auth-jwt:", 13) == 0 || strncmp(p, "Set-Auth-Jwt:", 13) == 0)
            {
                // Serial.println("Found set-auth-jwt header"); // remove for production
                // Serial.println(_jwtBuffer);                  // remove for production
                const char *val = p + 13;
                while (*val == ' ' || *val == '\t')
                    val++;
                // trim trailing CR if present
                size_t vlen = strnlen(val, sizeof(_jwtBuffer));
                if (vlen && val[vlen - 1] == '\r')
                    vlen--;
                if (vlen >= sizeof(_jwtBuffer))
                    vlen = sizeof(_jwtBuffer) - 1;
                // Move the JWT value to the start of the buffer (it may already be there)
                memmove(_jwtBuffer, val, vlen);
                _jwtBuffer[vlen] = 0;
                // Serial.println("Captured JWT:"); // remove for production
                // Serial.println(_jwtBuffer);      // remove for production
                haveJwt = true;
                break;
            }
        }
        // skip remaining headers
        char endOfHeaders[] = "\r\n\r\n";
        if (!_client.find(endOfHeaders))
        {
            _client.stop();
            return "Invalid response";
        }

        // Now read and parse JSON body into response
        DeserializationError derr = deserializeJson(response, _client);
        if (derr)
        {
            _client.stop();
            return derr.c_str();
        }
        _client.stop();

        if (!haveJwt)
        {
            return "no jwt in get-session response";
        }

        // Store iat and exp and internal time (pass quoted claim names)
        uint32_t tokenIat = jwt_get_claim_u32_scan(_jwtBuffer, "\"iat\"");
        uint32_t tokenExpiry = jwt_get_claim_u32_scan(_jwtBuffer, "\"exp\"");
        _tokenExpiry = tokenExpiry - tokenIat;
        _internalTimeIat = millis();
        _isSignedIn = true;

        return nullptr;
    }

    // Validate current JWT expiry and re-signin if necessary.
    // Returns nullptr on success, or an error message on failure.
    const char *refreshTokenIfNeeded()
    {
        if (!_isSignedIn)
            return ERROR_NOT_SIGNED_IN;

        // If we don't have an expiry, attempt to sign in again if credentials are available
        if (_tokenExpiry == 0)
        {
            if (!_email || !_password)
                return "no credentials to refresh token";
            return signIn(_email, _password);
        }

        // Compute current time based on stored iat and elapsed millis
        unsigned long elapsed_ms = 0;
        if (millis() >= _internalTimeIat)
            elapsed_ms = millis() - _internalTimeIat;
        else
            elapsed_ms = 0; // protect against wrap (very unlikely)

        // If token expires within the next 60 seconds, refresh it
        if (elapsed_ms / 1000U + 60U >= _tokenExpiry)
        {
            if (!_email || !_password)
                return "no credentials to refresh token";
            return signIn(_email, _password);
        }

        return nullptr;
    }

private:
    WiFiClient &_client;
    const char *_authHost;
    const char *_authPath;
    const char *_apiHost;
    const char *_apiPath;
    const char *_email;    // need to remember for re-signin
    const char *_password; // need to remember for re-signin
    bool _isSignedIn;
    // status buffer for HTTP status messages (avoid returning stack pointer)
    char _status[64];
    // session cookie captured from Set-Cookie header
    char _sessionCookie[MAX_JWT_LENGTH];
    // JWT token and expiry time
    uint32_t _tokenExpiry;     // token lifetime in seconds
    uint32_t _internalTimeIat; // millis() at time token was issued
    char _jwtBuffer[MAX_JWT_LENGTH];

    // payload for requests and responses - one at a time
    JsonDocument request;
    JsonDocument response;
};

static const char *
find_char_n(const char *s, size_t n, char c)
{
    const void *p = memchr((const void *)s, (unsigned char)c, n);
    return (const char *)p; // nullptr if not found
}

static const char *find_sub_n(const char *s, size_t n, const char *sub, size_t sublen)
{
    if (!s || !sub || sublen == 0 || n < sublen)
        return nullptr;
    for (size_t i = 0; i + sublen <= n; i++)
    {
        if (memcmp(s + i, sub, sublen) == 0)
            return s + i;
    }
    return nullptr;
}

// Generic parser for numeric JWT claim values in the payload (middle segment).
// Looks for the JSON key "<claim>" and parses its unsigned integer value.
// Returns 0 if missing/invalid/overflow.
uint32_t jwt_get_claim_u32_scan(const char *jwt, const char *claim)
{
    if (!jwt || !claim)
        return 0;

    // Bound overall JWT length
    size_t jwt_len = strnlen(jwt, MAX_JWT_LENGTH + 1);
    if (jwt_len == 0 || jwt_len > MAX_JWT_LENGTH)
        return 0;

    // locate payload between the two dots
    const char *dot1 = find_char_n(jwt, jwt_len, '.');
    if (!dot1)
        return 0;
    size_t after_dot1_n = (size_t)(jwt + jwt_len - (dot1 + 1));
    const char *dot2 = find_char_n(dot1 + 1, after_dot1_n, '.');
    if (!dot2)
        return 0;

    const char *payload = dot1 + 1;
    size_t b64_len = (size_t)(dot2 - payload);
    if (b64_len == 0 || b64_len > MAX_JWT_LENGTH)
        return 0;

    // Base64URL padding
    size_t pad = (4 - (b64_len % 4)) % 4;
    size_t b64_padded_len = b64_len + pad;
    size_t decoded_max = (b64_padded_len / 4) * 3;

    char *b64 = (char *)alloca(b64_padded_len + 1);
    if (!b64)
        return 0;
    memcpy(b64, payload, b64_len);
    for (size_t i = 0; i < pad; i++)
        b64[b64_len + i] = '=';
    b64[b64_padded_len] = '\0';

    char *decoded = (char *)alloca(decoded_max + 1);
    if (!decoded)
        return 0;
    unsigned int decoded_len = decode_base64((unsigned char *)b64, (unsigned char *)decoded);
    if (decoded_len == 0 || decoded_len > decoded_max)
        return 0;
    decoded[decoded_len] = '\0';

    // Expect 'claim' to already be the quoted key, e.g. "iat" or "exp"
    size_t claimlen = strnlen(claim, decoded_len);
    if (claimlen == 0)
        return 0;
    const char *k = find_sub_n(decoded, decoded_len, claim, claimlen);
    if (!k)
        return 0;

    const char *p = k + claimlen;
    const char *end = decoded + decoded_len;
    while (p < end && isspace((unsigned char)*p))
        p++;
    if (p >= end || *p != ':')
        return 0;
    p++;
    while (p < end && isspace((unsigned char)*p))
        p++;
    if (p >= end || !isdigit((unsigned char)*p))
        return 0;

    uint32_t v = 0;
    while (p < end && isdigit((unsigned char)*p))
    {
        uint32_t d = (uint32_t)(*p - '0');
        if (v > 429496729U || (v == 429496729U && d > 5U))
            return 0;
        v = v * 10U + d;
        p++;
    }
    return v;
}

#endif // POSTGRESTCLIENT_H
