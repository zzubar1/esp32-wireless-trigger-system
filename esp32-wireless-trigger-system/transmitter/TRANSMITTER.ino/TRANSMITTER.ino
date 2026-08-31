#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>

const char* ap_ssid = "ESP32Transmitter";      // name of the Wi-Fi network this ESP32 creates
const char* ap_password = "Password";           // password for that network

// Receiver ESP32's MAC address, hardcoded because ESP-NOW has no dynamic peer 
// discovery — obtained by printing WiFi.macAddress() on the receiver board once
uint8_t receiverMAC[] = {0x78, 0x1C, 0x3C, 0xE3, 0xAA, 0xF8}; // This mac address needs to be manually assigned in the code

WebServer server(80);           // HTTP server on the standard web port, serves the control page
bool lastSendSuccess = false;   // set asynchronously by OnDataSent() after each esp_now_send() call

// Raw HTML/CSS/JS for the control page, stored in flash (PROGMEM) instead of RAM 
// since it's static and doesn't need to live in the ESP32's limited SRAM
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>esp32 control</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      margin-top: 50px;
    }
    .button {
      background-color: green;
      border: none;
      color: white;
      padding: 15px 32px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 16px;
      margin: 4px 2px;
      cursor: pointer;
      border-radius: 8px;
    }
    .status {
      margin-top: 20px;
      font-size: 1.2em;
      color: #333;
    }
    .button2 {
      background-color: red;
      border: none;
      color: white;
      padding: 15px 32px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 16px;
      margin: 4px 2px;
      cursor: pointer;
      border-radius: 8px;
    }
  </style>
</head>
<body>
  <h1>send signal to esp32</h1>
  <p class="status" id="status-message">ready to send</p>
  <button class="button" onclick="sendTrigger()">TRIGGER ESP32</button>
  <button class="button2" onclick="endFunction()">END</button>
  <script>
    // Hits the /trigger HTTP endpoint on this ESP32, which relays a command 
    // to the receiver board over ESP-NOW
    function sendTrigger() {
      const statusElement = document.getElementById('status-message');
      statusElement.textContent = "sending trigger";
      fetch('/trigger', { method: 'GET' })
        .then(response => {
          if (response.ok) {
            return response.text();
          }
          throw new Error('Failed');
        })
        .then(data => {
          statusElement.textContent = "trigger sent";
        })
        .catch(error => {
          statusElement.textContent = "failed to send";
        });
    }

    // Hits the /end HTTP endpoint to relay a "stop" command to the receiver
    function endFunction() {
      const statusElement = document.getElementById('status-message');
      statusElement.textContent = "Sending END";

      fetch('/end', { method: 'GET' })
        .then(response => {
          if (response.ok) {
            return response.text();
          }
          throw new Error('Failed');
        })
        .then(data => {
          statusElement.textContent = "END sent";
        })
        .catch(error => {
          statusElement.textContent = "failed END";
        });
    }
  </script>
</body>
</html>
)rawliteral";


// Callback registered with ESP-NOW, fires automatically after esp_now_send() 
// completes (send is async, so this is how we find out if it actually worked)
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("success");
    lastSendSuccess = true;
  } else {
    Serial.println("failed");
    lastSendSuccess = false;
  }
}
void setup() {
  Serial.begin(115200);
  delay(1000);                    // give Serial time to stabilize before printing
  WiFi.mode(WIFI_AP_STA);         // AP mode to host the control page, STA mode needed for ESP-NOW
  WiFi.softAP(ap_ssid, ap_password);
  delay(500);

  Serial.print("SSID: ");
  Serial.println(ap_ssid);
  Serial.print("IP Address ");
  Serial.println(WiFi.softAPIP());   // connect to this IP from your phone's browser to load the control page
  Serial.print("MAC Address ");
  Serial.println(WiFi.softAPmacAddress());   // this is the AP's own MAC — not the one used for 
                                              // ESP-NOW pairing, that comes from the receiver board
  if (esp_now_init() != ESP_OK) {
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  Serial.println("send callback registered");
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;        // encryption disabled — simplicity for a local trusted network,
                                    // not something you'd want on an untrusted/public network

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("failed to add");
    return;
  }
  Serial.println("Receiver added");
  Serial.print("Receiver MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", receiverMAC[i]);
    // just echoing back the hardcoded receiverMAC[] value as a sanity check at boot,
    // not reading it from the receiver board
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  server.on("/", HTTP_GET, []() {
    Serial.println("\nweb requested");
    server.send(200, "text/html", htmlPage);
  });
  server.on("/trigger", HTTP_GET, []() {
    Serial.println("trigger request from http");
    lastSendSuccess = false; 
    uint8_t command = 1;            // 1 = trigger/activate
    Serial.print("sending command");
    Serial.println(command);
    esp_err_t result = esp_now_send(receiverMAC, &command, sizeof(command));
    if (result == ESP_OK) {
      Serial.println("ESP_OK");
    } else {
      Serial.printf("ERROR %d\n", result);
    }
    delay(100);   // brief wait so OnDataSent's async callback has time to set lastSendSuccess
                  // before we check it below
   
    if (result == ESP_OK && lastSendSuccess) {
      Serial.println("trigger sent");
      server.send(200, "text/plain", "OK");
    } else {
      Serial.println("failed to send");
      server.send(500, "text/plain", "FAILED");
    }
  });

  server.on("/end", HTTP_GET, []() {
    Serial.println("end request");
   
    lastSendSuccess = false;  
    uint8_t command = 0;            // 0 = end/deactivate
   
    Serial.print("sending command");
    Serial.println(command);
   
    esp_err_t result = esp_now_send(receiverMAC, &command, sizeof(command));
   
    if (result == ESP_OK) {
      Serial.println("ESP_OK");
    } else {
      Serial.printf("ERROR %d\n", result);
    }
    delay(100);   // same wait as /trigger, giving the async send callback time to fire
 
    if (result == ESP_OK && lastSendSuccess) {
      Serial.println("end sent");
      server.send(200, "text/plain", "OK");
    } else {
      Serial.println("failed end");
      server.send(500, "text/plain", "FAILED");
    }
  });
  server.begin();
  Serial.println("Web started");
}

void loop() {
  server.handleClient();   // must be called repeatedly to process incoming HTTP requests
}