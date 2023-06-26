#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

//HTTPS Includes
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>

//HTTPS variables
using namespace httpsserver;
SSLCert* cert;
HTTPSServer* secureServer;
void handleSetGpio(HTTPRequest* req, HTTPResponse* res);
void handleGetStatus(HTTPRequest* req, HTTPResponse* res);
void handleRoot(HTTPRequest* req, HTTPResponse* res);

// AP info
const char* AP_SSID = "Alkoteket_Brain";
const char* AP_PASSWORD = "12345678";

bool secureServerStarted = false;

// WiFi info
String networkSSID;
String networkPassword;
bool connectToWiFi = false;
bool clearWiFiCredentials = false;

// Preferences
Preferences preferences;

const int gpioPins[] = { 32, 33, 27 };

unsigned long startTimes[] = { 0, 0, 0 };
int durations[] = { 0, 0, 0 };
bool isHigh[] = { false, false, false };

// AsyncWebServer server(80);
AsyncWebServer serverAP(80);  // Server for AP mode

void setup() {
  Serial.begin(115200);
  preferences.begin("wifi", false);

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET,POST");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

  // Try to connect with saved credentials
  networkSSID = preferences.getString("ssid", "");
  networkPassword = preferences.getString("password", "");
  delay(1000);
  if (networkSSID != "" && networkPassword != "") {
    Serial.println("Credentials found.");
    connectToWiFi = true;  // Set flag for checking in loop()
  } else {
    // If no credentials saved, start AP
    Serial.println("No credentials found.");
    startAP();
  }
}

void loop() {
  // If we need to connect to a WiFi network
  if (connectToWiFi) {
    connectToWiFi = false;  // Reset the flag

    // HTTPS code
    Serial.println("Creating a new self-signed certificate.");
    Serial.println("This may take up to a minute, so be patient ;-)");
    cert = new SSLCert();
    int createCertResult = createSelfSignedCert(
      *cert,
      KEYSIZE_1024,
      "CN=myesp32.local,O=FancyCompany,C=DE",
      "20190101000000",
      "20300101000000");
    if (createCertResult != 0) {
      Serial.printf("Cerating certificate failed. Error Code = 0x%02X, check SSLCert.hpp for details", createCertResult);
      while (true) delay(500);
    }
    Serial.println("Creating the certificate was successful");
    secureServer = new HTTPSServer(cert);
    Serial.println("Setting up WiFi");

    WiFi.mode(WIFI_STA);
    WiFi.begin(networkSSID.c_str(), networkPassword.c_str());
    unsigned long startTime = millis();
    Serial.println("Attempting to connect to " + networkSSID);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      if (millis() - startTime > 20000) {
        Serial.println("");
        Serial.println("Failed to connect.");
        // If failed to connect, start the AP again
        startAP();
        break;
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi (" + networkSSID + ") connected successfully.");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      // Save credentials
      preferences.putString("ssid", networkSSID);
      preferences.putString("password", networkPassword);
      startSTAServer();
      secureServerStarted = true;
    }
  }
  if (secureServerStarted){
    secureServer->loop();
  }
  // If clear command received from Serial
  if (clearWiFiCredentials) {
    clearWiFiCredentials = false;  // Reset the flag
    preferences.remove("ssid");
    preferences.remove("password");
    Serial.println("WiFi credentials cleared.");
    Serial.println("Restarting...");
    ESP.restart();
  }

  // Check for clear command from Serial
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "clear") {
      clearWiFiCredentials = true;
    }
  }

  for (int i = 0; i < 6; i++) {
    if (isHigh[i] && (millis() - startTimes[i] >= durations[i])) {
      digitalWrite(gpioPins[i], LOW);
      isHigh[i] = false;
    }
  }
}

void startAP() {
  Serial.println("Starting Access Point.");
  // secureServer->stop();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  setupAPServer();
}

void setupAPServer() {
  // Serve the form
  serverAP.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    String html = "<html><head>";
    html += "<style>";
    html += "body { background-color: #f2f2f2; font-family: Arial, sans-serif; padding: 20px; }";
    html += "form { background-color: white; border-radius: 5px; padding: 20px; max-width: 500px; margin: 0 auto; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); }";
    html += "input[type='text'], input[type='password'] { width: 100%; padding: 10px; margin: 10px 0; border-radius: 5px; border: 1px solid #ddd; }";
    html += "input[type='submit'] { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; }";
    html += "input[type='submit']:hover { background-color: #45a049; }";
    html += "</style>";
    html += "</head><body>";
    html += "<form method='POST' action='/set_network'>";
    html += "<h2>Network Settings</h2>";
    html += "SSID: <input type='text' name='ssid'><br>";
    html += "Password: <input type='password' name='password'><br>";
    html += "<input type='submit' value='Submit'>";
    html += "</form>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  serverAP.on("/set_network", HTTP_POST, [](AsyncWebServerRequest* request) {
    int params = request->params();
    for (int i = 0; i < params; i++) {
      AsyncWebParameter* p = request->getParam(i);
      if (p->name() == "ssid") {
        networkSSID = p->value();
      } else if (p->name() == "password") {
        networkPassword = p->value();
      }
    }
    request->send(200, "text/plain", "Network settings received. Attempting to connect...");
    Serial.println("Network settings received.");
    connectToWiFi = true;
  });

  serverAP.begin();
  Serial.println("AP server started.");
}

void startSTAServer() {
  serverAP.end();  // Stop the AP server

  //HTTPS Code
  ResourceNode* nodeSetGpio = new ResourceNode("/set_gpio", "POST", &handleSetGpio);
  ResourceNode* nodeGetStatus = new ResourceNode("/get_status", "GET", &handleGetStatus);
  ResourceNode* nodeRoot = new ResourceNode("/", "GET", &handleRoot);

  secureServer->registerNode(nodeSetGpio);
  secureServer->registerNode(nodeGetStatus);
  secureServer->registerNode(nodeRoot);

  Serial.println("Starting server...");
  secureServer->start();
  if (secureServer->isRunning()) {
    Serial.println("Server ready.");
  }
}

//HTTPS Functions

void handleSetGpio(HTTPRequest* req, HTTPResponse* res) {
  res->setHeader("Access-Control-Allow-Methods", "HEAD,GET,POST,DELETE,PUT,OPTIONS");
  res->setHeader("Access-Control-Allow-Origin", "*");
  res->setHeader("Access-Control-Allow-Headers", "*");

  // Get access to the parameters.
  ResourceParameters* params = req->getParams();
  ResourceParameters* params2 = req->getHTTPHeaders();
  params2->

  for(auto it = params->beginQueryParameters(); it != params->endQueryParameters(); ++it) {
    Serial.println((*it).first.c_str());
    Serial.println((*it).second.c_str());
  }

  Serial.println(params->getQueryParameterCount());

  for(auto it = params->beginQueryParameters(); it != params->endQueryParameters(); ++it) {
    auto parameter_key = atoi((*it).first.c_str());
    auto parameter_value = atoi((*it).second.c_str());

    if (parameter_key >= 1 && parameter_key <= 3 && parameter_value > 0) {
      Serial.println("hejjjjJ");
      int index = parameter_key - 1;
      int pin = gpioPins[index];
      pinMode(pin, OUTPUT);
      digitalWrite(pin, HIGH);
      startTimes[index] = millis();
      durations[index] = (parameter_value * 1000) / 0.766;
      isHigh[index] = true;
    }
  }

  res->setStatusCode(200);
  res->setHeader("Content-Type", "application/json");
  res->println("GPIO set");
}

void handleGetStatus(HTTPRequest* req, HTTPResponse* res) {
  res->setHeader("Access-Control-Allow-Methods", "HEAD,GET,POST,DELETE,PUT,OPTIONS");
  res->setHeader("Access-Control-Allow-Origin", "*");
  res->setHeader("Access-Control-Allow-Headers", "*");
  String json = "{";
  for (int i = 0; i < 3; i++) {
    json += "\"gpio" + String(i + 1) + "\":\"" + String(isHigh[i] ? "HIGH" : "LOW") + "\"";
    if (i < 2) json += ",";
  }
  json += "}";

  res->setStatusCode(200);
  res->setHeader("Content-Type", "application/json");
  res->println(json);
}

void handleRoot(HTTPRequest* req, HTTPResponse* res) {
  // We will deliver an HTML page
  res->setHeader("Content-Type", "text/html");

  // Write the response page
  res->println("<!DOCTYPE html>");
  res->println("<html>");
  res->println("<head><title>Hello World!</title></head>");
  res->println("<style>.info{font-style:italic}</style>");
  res->println("<body>");

  res->println("<h1>Query Parameters</h1>");
  res->println("<p class=\"info\">The parameters after the question mark in your URL.</p>");

  // Show a form to select a color to colorize the faces
  // We pass the selection as get parameter "shades" to this very same page,
  // so we can evaluate it below
  res->println("<form method=\"GET\" action=\"/\">Show me faces in shades of ");
  res->println("<select name=\"shades\">");
  res->println("<option value=\"red\">red</option>");
  res->println("<option value=\"green\">green</option>");
  res->println("<option value=\"blue\">blue</option>");
  res->println("<option value=\"yellow\">yellow</option>");
  res->println("<option value=\"cyan\">cyan</option>");
  res->println("<option value=\"magenta\">magenta</option>");
  res->println("<option value=\"rainbow\">rainbow</option>");
  res->println("</select>");
  res->println("<button type=\"submit\">Go!</button>");
  res->println("</form>");
  res->println("<p>You'll find another demo <a href=\"/queryparams?a=42&b&c=13&a=hello\">here</a>.</p>");

  // Link to the path parameter demo
  res->println("<h1>Path Parameters</h1>");
  res->println("<p class=\"info\">The parameters derived from placeholders in your path, like /foo/bar.</p>");
  res->println("<p>You'll find the demo <a href=\"/urlparam/foo/bar\">here</a>.</p>");

  res->println("</body>");
  res->println("</html>");
}
