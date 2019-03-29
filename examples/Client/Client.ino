// Copyright (c) 2019 Sho Kuroda <krdlab@gmail.com>
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <WiFi.h>
#include <WiFiClient.h>
#include <websocket.hpp>

const char* ssid      = "WiFi SSID";
const char* password  = "WiFi password";
const char* host      = "Server IP address";
const uint16_t port   = 3000;
const char* path      = "/";

WiFiClient wifi;
WebSocketClient client(wifi);

void setup()
{
  beginSerial();
  connectWiFi(ssid, password);

  if (client.connect(host, port, path)) {
    Serial.printf("Client connected to %s:%d.\n", host, port);
  } else {
    Serial.printf("Client failed connecting to %s:%d.\n", host, port);
  }
}

void loop()
{
  if (client.connected()) {
    demo(client);
  } else {
    Serial.println("Client disconnected.");
  }
  delay(1000);
}

void beginSerial()
{
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
}

void connectWiFi(const char* ssid, const char* password)
{
  Serial.print("Connecting to "); Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected.");
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
}

void demo(WebSocketClient& client)
{
  while (client.connected()) {
    Payload text;
    text.print("あいうえお");
    if (!client.write(text, WS_OPCODE_TEXT)) {
      Serial.println("Failed to write text data");
      return;
    }

    Payload binary;
    binary.write(0x01);
    binary.write(0x02);
    binary.write(0x34);
    if (!client.write(binary, WS_OPCODE_BINARY)) {
      Serial.println("Failed to write binary data");
      return;
    }

    client.waitForResponse();

    while (readAndPrint());

    delay(3000);
  }
}

bool readAndPrint()
{
  Payload recv;
  uint8_t opcode;
  if (!client.read(recv, opcode)) {
    return false;
  }
  print(opcode, recv);
  return true;
}

void print(const uint8_t opcode, Payload& p)
{
  switch (opcode) {
  case WS_OPCODE_TEXT:
    printText(p);
    break;
  case WS_OPCODE_BINARY:
    printBinary(p);
    break;
  default:
    printAny(opcode, p);
  }
}

void printText(Payload& p)
{
  Serial.printf("opcode %2x: ", WS_OPCODE_TEXT);
  Serial.println(p.readString());
}

void printBinary(Payload& p)
{
  printAny(WS_OPCODE_BINARY, p);
}

void printAny(const uint8_t opcode, Payload& p)
{
  Serial.printf("opcode %2x: ", opcode);
  while (p.available()) {
    Serial.printf("%02x ", p.read());
  }
  Serial.println();
}
