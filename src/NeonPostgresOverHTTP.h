// NeonPostgresOverHTTP example - https://github.com/neondatabase-labs/NeonPostgresOverHTTP
// Copyright © 2025, Peter Bendel and neondatabase labs (neon.tech)
// MIT License

#ifndef NEONPOSTGRESOVERTTPPROXYCLIENT_H
#define NEONPOSTGRESOVERHTTPPROXYCLIENT_H
#include <ArduinoJson.h>
#include "WiFiClient.h"
#include <cstring>

class NeonPostgresOverHTTPProxyClient {
public:

  /**
     * @brief Constructs an database client that connects over Wifi to a Neon database (https://neon.tech)
     *        @see https://github.com/neondatabase/neon/tree/main/proxy 
     *        Instead of a Neon database you can use any other PostgreSQL database and run the Neon Proxy 
     *        yourself to convert between the SQL over HTTP and the postgres wire protocol.
     *        For details see https://github.com/neondatabase/serverless/blob/main/DEPLOY.md
     * 
     * @param client A Wifi client that supports SSL, for example WifiNINA.h WiFiSSLClient or WiFiBearSSLClient
     *               The caller must connect the Wifi client before invoking SQL statements:
     * @example
     * ```cpp
     * while (status != WL_CONNECTED) {
     *   Serial.print("Attempting to connect to SSID: ");
     *   Serial.println(ssid);
     *   // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
     *  status = WiFi.begin(ssid, pass);
     * 
     *   // wait 10 seconds for connection:
     *   delay(10000);
     * }
     * ```
     * @param neonPostgresConnectionString A connection string that connects to the Neon database, for example
     *                                     "postgresql://neondb_owner:password@ep-hungry-bird-4242.eu-central-1.aws.neon.tech/neondb?sslmode=require".
     *                                     This pointer/buffer must be valid for the complete lifetime of the NeonPostgresOverHTTPProxyClient, it is NOT 
     *                                     copied
     * @param neonProxy The hostname of the neon proxy. Usually it can be easily derived from the connection string by extracting the hostname
     *                  as "api.<hostname>" for example "api.eu-central-1.aws.neon.tech".
     *                  This pointer/buffer must be valid for the complete lifetime of the NeonPostgresOverHTTPProxyClient, it is NOT 
     *                  copied
     * @param proxyPort Proxy listening port. Usually 443 for https.
     */
  NeonPostgresOverHTTPProxyClient(WiFiClient& client, const char* neonPostgresConnectionString, const char* neonProxy, int proxyPort = 443)
    : client(client), connstr(neonPostgresConnectionString), proxy(neonProxy), proxyPort(proxyPort) {
    request.clear();
    response.clear();
    txnRequest.clear();
    txnResponse.clear();
    request["params"].to<JsonArray>();
    txnRequest["queries"].to<JsonArray>();
  }

  /**
     * @brief Specify the sql statement you want to execute. 
     *        The statement text can use parameter markers.
     *        If you use parameter markers you must also supply the parameter values before executing the
     *        statement.
     * @example
     * ```cpp
     * client.setQuery("INSERT INTO T1 (C1) VALUES ($1::int)");
     * JsonArray params = client.getParams();
     * while (moreWorkToDo) {
     *    params.clear();
     *    params.add(42);
     *    // execute statement and handle result
     *    ...
     * }
     * ```
     * 
     * @param query query SQL statement text (INSERT, UPDATE, DELETE,...) optionally with parameter markers.
     */
  void setQuery(const char* query) {
    request["query"] = query;
  }

  /**
     * @brief Get the parameter array to clear and set the parameter values for the next statement 
     *        execution.
     *      
     * @example
     * ```cpp
     * JsonArray params = client.getParams();
     * while (moreWorkToDo) {
     *    params.clear();
     *    params.add(42);
     *    // execute statement and handle result
     *    ...
     * }
     * ```
     * 
     * @param query query SQL statement text (INSERT, UPDATE, DELETE,...) optionally with parameter markers.
     */
  JsonArray getParams() {
    return request["params"].to<JsonArray>();
  }

  /**
    * Connect to proxy, send the SQL statement set with setQuery() and parse the result.
    * 
    * @param timeout maximum time in milliseconds to wait for response
    *
    * @return nullptr on success, error message in case of failure
    */
  const char* execute(unsigned long timeout = 20000) {
    return executeInternal(request, response, timeout);
  }

  /**
    * After execution for a query get the number of rows returned.
    * For a DML statement (INSERT, UPDATE, DELETE) this is the number of rows affected.
    *
    * @return number of rows returned or affected
    */
  int getRowCount() {
    return response["rowCount"].as<int>();
  }

  /**
    * After execution for a query get the rows returned.
    *
    * @return the rows returned as JsonArray
    */
  JsonArray getRows() {
    return response["rows"].as<JsonArray>();
  }

  JsonArray getFields() {
    return response["fields"].as<JsonArray>();
  }

  /** Get complete query result as ArduinoJson JsonDocument.
    * Mostly used for debugging, normally
    * getRows(), getFields() and getRowCount() should be used.
    */
  JsonDocument& getRawJsonResult() {
    return response;
  }

  /** Print complete query result as ArduinoJson JsonDocument.
    * Mostly used for debugging.
    * @example
    * ```cpp
    * sqlClient.printRawJsonResult(Serial);
    * ```
    */
  void printRawJsonResult(Print& print) {
    print.println();
    serializeJson(response, print);
    print.println();
  }

  // --------------------------------------------------------------------
  // same APIs as above with transaction support 
  // allows to run multiple statements in a single transaction atomically
  // --------------------------------------------------------------------

  /**
     * @brief Specify the sql statement you want to execute in a transaction.
     *        The statement text can use parameter markers.
     *        If you use parameter markers you must also supply the parameter values before executing the
     *        statement.
     * @example
     * ```cpp
     * sqlClient.startTransaction();
     * sqlClient.addQueryToTransaction("SELECT $1::int");
     * JsonArray params1 = sqlClient.getParamsForTransactionQuery(0);
     * params1.clear();
     * params1.add(100);
     * ```
     * @param query query SQL statement text (INSERT, UPDATE, DELETE,...) optionally with parameter markers.
     */
  void addQueryToTransaction(const char* query) {
    JsonObject newQuery = txnRequest["queries"].add<JsonObject>();
    newQuery["query"] = query;
    // Create the params array inside the new object
    newQuery["params"].to<JsonArray>();
  }

  /**
     * @brief Get the parameter array for a statement in a transaction to clear and 
     *        set the parameter values for the next statement execution.
     *      
     * @example
     * ```cpp
     * JsonArray params = client.getParamsForTransactionQuery(0);
     * params.clear();
     * params.add(42);
     * // execute transaction and handle result
     * ...
     * ```
     * 
     * @param query query SQL statement text (INSERT, UPDATE, DELETE,...) optionally with parameter markers.
     */
  JsonArray getParamsForTransactionQuery(size_t queryIndex) {
    if (queryIndex >= txnRequest["queries"].size()) {
      return JsonArray();
    }
    JsonObject query = txnRequest["queries"][queryIndex];
    return query["params"];
  }

  /**
   * Reset the transaction state in case you have run a transaction before.
   * Clears all queries and responses.
   */
  void startTransaction() {
    txnRequest.clear();
    txnResponse.clear();
    txnRequest["queries"].to<JsonArray>();
  }

  /**
    * Connect to proxy, send the SQL statements added to the transaction with
    * addQueryToTransaction() and parse the result.
    * 
    * @param timeout maximum time in milliseconds to wait for response
    *
    * @return nullptr on success, error message in case of failure
    */
  const char* executeTransaction(unsigned long timeout = 20000) {
    // activate the following lines for debugging
    // Serial.println();
    // serializeJson(txnRequest, Serial);
    // Serial.println();
    return executeInternal(txnRequest, txnResponse, timeout);
  }

  /**
    * After execution of a transaction for a query get the rows returned.
    *
    * @param queryIndex 0-based index of the query in the transaction
    * 
    * @return the rows returned as JsonArray
    */
  JsonArray getRowsForTransactionQuery(size_t queryIndex) {
    if (queryIndex >= txnResponse["results"].size()) {
      return JsonArray();
    }
    return txnResponse["results"][queryIndex]["rows"].as<JsonArray>();
  }

  /**
    * After execution for a set of queries get the number of rows returned.
    * For a DML statement (INSERT, UPDATE, DELETE) this is the number of rows affected.
    *
    * @param queryIndex 0-based index of the query in the transaction
    * 
    * @return number of rows returned or affected
    */
  int getRowCountForTransactionQuery(size_t queryIndex) {
    if (queryIndex >= txnResponse["results"].size()) {
      return -1;
    }
    return txnResponse["results"][queryIndex]["rowCount"].as<int>();
  }

  JsonArray getFieldsForTransactionQuery(size_t queryIndex) {
    if (queryIndex >= txnResponse["results"].size()) {
      return JsonArray();
    }
    return txnResponse["results"][queryIndex]["fields"].as<JsonArray>();
  }

  /** Get complete transaction result as ArduinoJson JsonDocument.
    * Mostly used for debugging, normally
    * getRowsForTransactionQuery(), getFieldsForTransactionQuery() and getRowCountForTransactionQuery() should be used.
    */
  JsonDocument& getRawJsonResultForTransaction() {
    return txnResponse;
  }

  /** Print complete transaction result as ArduinoJson JsonDocument.
    * Mostly used for debugging.
    * @example
    * ```cpp
    * sqlClient.printRawJsonResultForTransaction(Serial);
    * ```
    */
  void printRawJsonResultForTransaction(Print& print) {
    print.println();
    serializeJson(txnResponse, print);
    print.println();
  }

private:

  /**
    * Connect to proxy, send the SQL statement(s) parse the result.
    * 
    * @param src JsonDocument containing the queries
    * @param dst JsonDocument to store the result
    * @param timeout maximum time in milliseconds to wait for response
    *
    * @return nullptr on success, error message in case of failure
    */
  const char* executeInternal(JsonDocument& src, JsonDocument& dst, unsigned long timeout = 20000) {
    if (client.connect(proxy, proxyPort)) {
      client.println("POST /sql HTTP/1.1");
      client.print("Host: ");
      client.println(proxy);
      client.print("Neon-Connection-String: ");
      client.println(connstr);
      client.println("Content-Type: application/json");
      client.print("Content-Length: ");
      size_t length = measureJson(src);
      client.print(length);
      // Send payload after second new-line
      client.print("\r\n\r\n");  // double new line between headers and payload

      size_t written = serializeJson(src, client);
      if (written != length) {
        // Serial.print("Writing ");
        // Serial.print(length);
        // Serial.println(" chars:\n");
        // serializeJson(request, Serial);
        // Serial.print("\nserializeJson written: ");
        // Serial.println(written);
        client.stop();
        return "payload serialization error";
      }
      client.flush();
      // waiting for response
      unsigned long ms = millis();
      while (!client.available() && millis() - ms < timeout) {
        delay(0);
      }
      if (client.available()) {
        // Check HTTP status
        size_t bytes_read = client.readBytesUntil('\r', status, sizeof(status) - 1);
        status[bytes_read] = 0;
        // It should be "HTTP/1.0 200 OK" or "HTTP/1.1 200 OK" or HTTP/1.1 400 Bad Request
        int status_code;
        sscanf(status + 9, "%3d", &status_code);
        if (status_code < 200 || (status_code >= 300 && status_code < 400) || status_code > 400) {
          client.stop();
          return status;
        }
        // Skip HTTP headers
        char endOfHeaders[] = "\r\n\r\n";
        if (!client.find(endOfHeaders)) {
          client.stop();
          return "Invalid response";
        }

        DeserializationError err = deserializeJson(dst, client);
        if (err) {
          client.stop();
          return err.c_str();
        }
        // else {
        //   Serial.println();
        //   serializeJson(response, Serial);
        //   Serial.println();
        // }
        client.stop();
        const char* errormessage = dst["message"];
        if (errormessage != nullptr) {
          return errormessage;
        }
      } else {
        client.stop();
        return "query timed out";
      }
    } else {
      // client.stop();
      return "cannot connect to proxy over Wifi";
    }
    return nullptr;
  }

  JsonDocument request;
  JsonDocument response;
  JsonDocument txnRequest;
  JsonDocument txnResponse;
  WiFiClient& client;
  const char* connstr;
  const char* proxy;
  const int proxyPort;
  char status[32];
};

#endif /* NeonPostgresOverHTTPProxyClient_H */