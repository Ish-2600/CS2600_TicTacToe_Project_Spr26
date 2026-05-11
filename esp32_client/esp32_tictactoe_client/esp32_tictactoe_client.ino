#include <WiFi.h>
#include <PubSubClient.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#define SDA 13
#define SCL 14

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* MQTT_SERVER = "ishserver.duckdns.org";
const int MQTT_PORT = 1883;

const char* GAME_ID = "ismael";
char playerRole = 'x';

LiquidCrystal_I2C lcd(0x27, 16, 2);

WiFiClient espClient;
PubSubClient mqttClient(espClient);

char keys[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[4] = {27, 26, 25, 33};
byte colPins[4] = {15, 21, 22, 23};

Keypad myKeypad = Keypad(makeKeymap(keys), rowPins, colPins, 4, 4);

void showMessage(const char* line1, const char* line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void buildTopic(char* topic, const char* suffix) {
  sprintf(topic, "ttt/%s/%s", GAME_ID, suffix);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char message[64];

  if (length >= sizeof(message)) {
    length = sizeof(message) - 1;
  }

  for (unsigned int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }

  message[length] = '\0';

  Serial.print("MQTT topic: ");
  Serial.println(topic);
  Serial.print("MQTT message: ");
  Serial.println(message);

  if (strstr(topic, "/game/status")) {
    showMessage("Status:", message);
  } else if (strstr(topic, "/game/available")) {
    showMessage("Available:", message);
  } else if (strstr(topic, "/game/board")) {
    showMessage("Board:", message);
  }
}

void connectWiFi() {
  showMessage("Connecting WiFi", WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  showMessage("WiFi connected", WiFi.localIP().toString().c_str());
  delay(1000);
}

void connectMQTT() {
  char clientId[40];
  char topic[64];

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  while (!mqttClient.connected()) {
    showMessage("Connecting MQTT", MQTT_SERVER);

    sprintf(clientId, "esp32_ttt_%lu", millis());

    if (mqttClient.connect(clientId)) {
      Serial.println("MQTT connected");

      buildTopic(topic, "game/status");
      mqttClient.subscribe(topic);

      buildTopic(topic, "game/board");
      mqttClient.subscribe(topic);

      buildTopic(topic, "game/available");
      mqttClient.subscribe(topic);

      showMessage("MQTT connected", "Ready");
      delay(1000);
    } else {
      Serial.print("MQTT failed, rc=");
      Serial.println(mqttClient.state());
      showMessage("MQTT failed", "Retrying...");
      delay(2000);
    }
  }
}

void publishMove(char move) {
  char topic[64];
  char message[2];
  char line2[16];

  sprintf(topic, "ttt/%s/player/%c/move", GAME_ID, playerRole);

  message[0] = move;
  message[1] = '\0';

  mqttClient.publish(topic, message);

  sprintf(line2, "Move %c as %c", move, playerRole);
  showMessage("Sent move", line2);

  Serial.print("Published move ");
  Serial.print(move);
  Serial.print(" to ");
  Serial.println(topic);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(SDA, SCL);

  if (!i2CAddrTest(0x27)) {
    lcd = LiquidCrystal_I2C(0x3F, 16, 2);
  }

  lcd.init();
  lcd.backlight();

  showMessage("ESP32 TTT", "Starting...");
  delay(1000);

  connectWiFi();
  connectMQTT();

  showMessage("Player: X", "Press 1-9 move");
}

void loop() {
  if (!mqttClient.connected()) {
    connectMQTT();
  }

  mqttClient.loop();

  char keyPressed = myKeypad.getKey();

  if (keyPressed) {
    Serial.print("Key: ");
    Serial.println(keyPressed);

    if (keyPressed >= '1' && keyPressed <= '9') {
      publishMove(keyPressed);
    } else if (keyPressed == 'A') {
      playerRole = 'x';
      showMessage("Player set to", "X");
    } else if (keyPressed == 'B') {
      playerRole = 'o';
      showMessage("Player set to", "O");
    } else if (keyPressed == 'C') {
      showMessage("Refreshing", "MQTT status");
      connectMQTT();
    } else if (keyPressed == 'D') {
      showMessage("Player:", playerRole == 'x' ? "X" : "O");
    }
  }
}

bool i2CAddrTest(uint8_t addr) {
  Wire.beginTransmission(addr);

  if (Wire.endTransmission() == 0) {
    return true;
  }

  return false;
}
