#ifndef RESTARTER
#define RESTARTER

#include <string.h>
#include <ArduinoHttpClient.h>

Hashtable<String, String> extractHeaders(HttpClient*  client);
String* processSetCookieHeader(String* header);

// If defined, assume a serial port has been configured and we should log
#define USE_SERIAL 1

#ifdef USE_SERIAL
#define P P
#define PL PL
#else
#define P (void)0
#define PL (void)0
#endif


/**
 * Get the index page, processing the headers and returning the require cookie if 
 * succesfull (null on error)
 */
String* processIndexPage(HttpClient* client) {
  client->get("/");
  int statusCode = client->responseStatusCode();
  if (statusCode != 200) {
    P("Unable to fetch index. statusCode=");
    PL(statusCode);
    return NULL;
  }
  Hashtable<String, String> headers = extractHeaders(client);
  String response = client->responseBody();

  String* cookie = processSetCookieHeader(headers.get("Set-Cookie"));
  headers.clear();

  return cookie;
}

/**
 * Post to the init page, including the required cookie.
 */
bool processInitPage(HttpClient* client, String* cookie) {
  if (cookie == NULL) {
    return false;
  }
  client->beginRequest();
  client->sendHeader("frkrouter", cookie->c_str());
  client->post("/cgi-bin/init_page.cgi", "application/json", "{\"command\": \"load\"}");
  client->endRequest();

  int statusCode = client->responseStatusCode();
  if (statusCode != 200) {
    P("Unable to post to init page. statusCode=");
    PL(statusCode);
    return false;
  }

  return true;
}

bool processLoginPage(HttpClient* client, String* cookie) {
  if (cookie == NULL) {
    return false;
  }
  client->beginRequest();
  client->sendHeader("frkrouter", cookie->c_str());
  client->post("/cgi-bin/login.cgi", "application/json", "{\"command\": \"log_in\", \"params\": {\"password\": \"ytsejaM8p\"}}");
  client->endRequest();

  int statusCode = client->responseStatusCode();
  if (statusCode != 200) {
    P("Unable to post to login page. statusCode=");
    PL(statusCode);
    return false;
  }

  // Make sure we have "Success" in the result body
  String response = client->responseBody();
  return response.indexOf("Success") != -1;
}

bool processSettingsUpdate(HttpClient* client, String* cookie) {
  client->beginRequest();
  client->sendHeader("frkrouter", cookie->c_str());
  client->post("/cgi-bin/settings.advanced_router-lan_settings.cgi", "application/json", "{\"command\":\"save\",\"params\":null,\"data\":{\"ip_addr1\":\"0\",\"ip_addr2\":\"1\",\"subnet_mask\":\"255.255.255.0\",\"vpn_passthrough\":\"on\",\"dhcp_server\":\"on\",\"dhcp_ip_st1\":\"0\",\"dhcp_ip_st2\":\"100\",\"dhcp_ip_ed1\":\"0\",\"dhcp_ip_ed2\":\"254\",\"dhcp_lease_time\":\"7200\",\"dns_manual_mode\":\"off\",\"dns_addr1\":\"\",\"dns_addr2\":\"\",\"upnp\":\"off\",\"out_of_service_notification\":\"off\",\"nat_timeout\":\"200\"}}");
  client->endRequest();

  int statusCode = client->responseStatusCode();
  if (statusCode != 200) {
    P("Unable to post to update page. statusCode=");
    PL(statusCode);
    return false;
  }

  // Make sure we have "OK" in the result body
  String response = client->responseBody();
  return response.indexOf("OK") != -1;
}

/**
 * Parse the Set-Cookie value
 */
String* processSetCookieHeader(String* header) {
  if (header == NULL) {
    return NULL;
  }

  String* cookieName = new String("");
  String* cookieValue = new String("");
  
  int eqIndex = header->indexOf('=');
  int semiIndex = header->indexOf(';');
  
  if (eqIndex != -1) {
      cookieName = new String(header->substring(0, eqIndex));
      if (semiIndex != -1) {
          cookieValue = new String(header->substring(eqIndex + 1, semiIndex));
      } else {
          cookieValue = new String(header->substring(eqIndex + 1));
      }
  }

  // String attributes = (semiIndex != -1) ? header->substring(semiIndex + 1) : "";

  // PL("Cookie Name: " + cookieName);
  // PL("Cookie Value: " + cookieValue);
  // PL("Attributes: " + attributes);
  // free(header);

  return cookieValue;
}

/*
  Return the headers from the HTTP call
*/
Hashtable<String, String> extractHeaders(HttpClient*  client) {
  Hashtable<String, String> headers;
  while (client->headerAvailable()) {
    headers.put(client->readHeaderName(), client->readHeaderValue());
  }

  return headers;
}

#endif 