#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

int LIGHT_PIN = 21;    // Output 3.3V at ~40mA max
int AUDIO_PIN = 25;    // PWM output for audio amplifier dac1,  25 and 26 are the only GPIO pins with built-in DAC

const int PWM_FREQUENCY = 1500;   // PWM carrier frequency in Hz — stands in for real analog 
                                   // audio through the PAM8403 amp
const int PWM_RESOLUTION = 8;     // 8-bit resolution = duty cycle values 0-255

unsigned long activationTime = 0;
bool isActive = false;
const unsigned long AUTO_OFF_DURATION = 10000   // ms before auto shutoff after trigger 
                                                 // (note: not currently checked anywhere below)

// Called automatically by the ESP-NOW stack when a packet arrives from a registered peer.
// Runs in callback context, so it's kept short/fast rather than doing heavy work here.
void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len) {
  uint8_t command = incomingData[0];   // single byte command: 1 = trigger, 0 = end
 
  Serial.print("\ndata received from ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", recv_info->src_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
 
  if (command == 1) {
    activationTime = millis();
    isActive = true;
    Serial.println("trigger received");
   
    digitalWrite(LIGHT_PIN, HIGH);
    ledcWrite(AUDIO_PIN, 128);   // ~50% duty cycle tone
    Serial.println("sound playing");
    
  } else if (command == 0) {
    isActive = false;
    Serial.println("end received");
   
    digitalWrite(LIGHT_PIN, LOW);
    ledcWrite(AUDIO_PIN, 0);
    
    Serial.println("deactivated");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);   // give Serial time to stabilize before printing
 
  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, LOW);
  ledcAttach(AUDIO_PIN, PWM_FREQUENCY, PWM_RESOLUTION); // newer ESP32 core API — 
                                                          // channel assignment handled internally
  ledcWrite(AUDIO_PIN, 0);
  Serial.println("Audio init on 25");
 
  WiFi.mode(WIFI_STA);   // ESP-NOW requires station mode, even without joining a network
  WiFi.disconnect();
  delay(100);
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());   // print once to get the address the transmitter needs 
                                        // hardcoded into its receiverMAC[] array
  uint8_t mac[6];
  WiFi.macAddress(mac);  
 
  if (esp_now_init() != ESP_OK) {
    Serial.println("init failed");
    return;
  }
  Serial.println("initialized");
  esp_now_register_recv_cb(OnDataRecv);   // register the callback for incoming ESP-NOW packets
 
  Serial.println("ready for trigger");
}

void loop() {
  continue;   
  }