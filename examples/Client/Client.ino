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

  WebSocketConnectResult res;
  if ((res = client.connect(host, port, path)) == WebSocketConnectResult::Success) {
    Serial.printf("Client connected to %s:%d.\n", host, port);
  } else {
    Serial.printf("Client failed connecting to %s:%d (reason = %d)\n", host, port, res);
  }
}

void loop()
{
  if (client.connected()) {
    demo(client);
    delay(3000);
  } else {
    Serial.println("Client disconnected.");
    while (true);
  }
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
    if (client.write(text, WS_OPCODE_TEXT) != WebSocketWriteResult::Success) {
      Serial.println("Failed to write text data");
      return;
    }

    Payload binary;
    binary.write(0x01);
    binary.write(0x02);
    binary.write(0x34);
    if (client.write(binary, WS_OPCODE_BINARY) != WebSocketWriteResult::Success) {
      Serial.println("Failed to write binary data");
      return;
    }

    client.waitForResponse();

    while (handleResponse(client));

    delay(3000);
  }
}

bool handleResponse(WebSocketClient& client)
{
  Payload recv;
  uint8_t opcode;
  if (client.read(recv, opcode) != WebSocketReadResult::Success) {
    return false;
  }
  doAction(client, opcode, recv);
  return true;
}

void doAction(WebSocketClient& clinet, const uint8_t opcode, Payload& p)
{
  switch (opcode) {
  case WS_OPCODE_TEXT:
    printText(p);
    break;
  case WS_OPCODE_BINARY:
    printBinary(p);
    break;
  case WS_OPCODE_PING:
    if (0 < p.available()) {
      const size_t size = p.available();
      uint8_t tmp[size];
      p.readBytes(tmp, size);
      printBytes(opcode, tmp, size);
      Payload res(tmp, p.available());
      client.pong(res);
    } else {
      printAny(opcode, p);
      client.pong();
    }
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

void printBytes(const uint8_t opcode, const uint8_t* data, const size_t size)
{
  Serial.printf("opcode %2x: ", opcode);
  for (int i = 0; i < size; ++i) {
    Serial.printf("%02x ", data[i]);
  }
  Serial.println();
}
