// Copyright (c) 2019 Sho Kuroda <krdlab@gmail.com>
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <Arduino.h>
#include <base64.h>
#include <mbedtls/sha1.h>
#include <esp_log.h>
#include <sstream>
#include "websocket.hpp"


#define LOG_TAG "websocket"

#define CRLF    "\r\n"
#define CR      '\r'
#define LF      '\n'
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

const String HTTP_STATUS_LINE_101 = "http/1.1 101 ";
const String HTTP_UPGRADE_WEBSOCKET = "upgrade: websocket";
const String HTTP_CONNECTION_UPGRADE = "connection: upgrade";
const String HTTP_HEADER_SEC_WEBSOCKET_ACCEPT = "sec-websocket-accept: ";

#define WS_FIN      0x80
#define WS_MASK     0x80
#define WS_SIZE16   126
#define WS_SIZE64   127


static String hash(const String& content)
{
  mbedtls_sha1_context context;
  const int DIGEST_LEN = 20;
  uint8_t digest[DIGEST_LEN];

  mbedtls_sha1_init(&context);
  mbedtls_sha1_starts(&context);
  mbedtls_sha1_update(&context, (const uint8_t*) content.c_str(), content.length());
  mbedtls_sha1_finish(&context, digest);

  char res[DIGEST_LEN + 1];
  for (int i = 0; i < DIGEST_LEN; ++i) {
    res[i] = (char) digest[i];
  }
  res[DIGEST_LEN] = '\0';
  return String(res);
}

static String base64_encode(const String& s)
{
  return base64::encode(s);
}

static void generateMask(uint8_t (&mask)[4])
{
  mask[0] = random(256);
  mask[1] = random(256);
  mask[2] = random(256);
  mask[3] = random(256);
}


WebSocketConnectResult WebSocketClient::connect(const char* host, const uint16_t port, const char* path, const char* protocol)
{
  if (client.connect(host, port)) {
    ESP_LOGD(LOG_TAG, "client.connect: connected");
    if (handshake(host, path, protocol == NULL ? "" : protocol)) {
      ESP_LOGD(LOG_TAG, "handshake: completed");
      return WebSocketConnectResult::Success;
    } else {
      ESP_LOGD(LOG_TAG, "handshake: failed");
      if (client.connected()) {
        client.stop();
      }
      return WebSocketConnectResult::HandshakeFailure;
    }
  } else{
    ESP_LOGD(LOG_TAG, "client.connect: failed");
    return WebSocketConnectResult::ConnectFailure;
  }
}

struct ParsedResponse
{
  bool is101 = false;
  bool hasUpgrade = false;
  bool hasConnection = false;
  String serverKey;

  bool isValid() const
  {
    return is101
        && hasUpgrade
        && hasConnection
        && serverKey.length() > 0;
  }
  const String& getServerKey() const { return serverKey; }
  const std::string toString() const { // NOTE: for debugging
    std::stringstream ss;
    ss << "ParsedResponse { "
        << "is101 = " << std::boolalpha << is101
        << ", "
        << "hasUpgrade = " << std::boolalpha << hasUpgrade
        << ", "
        << "hasConnection = " << std::boolalpha << hasConnection
        << ", "
        << "serverKey = " << serverKey.c_str()
        << " }";
    return ss.str();
  }
};

bool WebSocketClient::handshake(const String& host, const String& path, const String& protocol)
{
  randomSeed(analogRead(0));

  const String key = generateKey();
  ESP_LOGD(LOG_TAG, "generateKey: %s", key.c_str());
  requestUpgrade(path, host, key, protocol);
  waitForResponse();

  ParsedResponse response;
  if (!parseResponse(response)) {
    ESP_LOGD(LOG_TAG, "invalid response");
    return false;
  }

  const String& serverKey = response.getServerKey();
  const String calcKey = calculateServerKey(key);
  ESP_LOGD(LOG_TAG, "serverKey (recv): %s", serverKey.c_str());
  ESP_LOGD(LOG_TAG, "serverKey (calc): %s", calcKey.c_str());
  return serverKey.equals(calcKey);
}

String WebSocketClient::generateKey()
{
  const int LEN = 16;
  char key[LEN + 1];
  for (int i = 0; i < LEN; ++i) {
    key[i] = random(256);
  }
  key[LEN] = '\0';
  return base64_encode(String(key));
}

String WebSocketClient::calculateServerKey(const String& clientKey)
{
  String calcKey = hash(clientKey + WS_GUID); // FIXME: error check
  return base64_encode(calcKey);
}

bool WebSocketClient::requestUpgrade(const String& path, const String& host, const String& key, const String& protocol)
{
  if (!client.connected()) {
    return false;
  }
  client.printf("GET %s HTTP/1.1%s", path.c_str(), CRLF);
  client.printf("Upgrade: websocket%s", CRLF);
  client.printf("Connection: Upgrade%s", CRLF);
  client.printf("Host: %s%s", host.c_str(), CRLF);
  client.printf("Sec-WebSocket-Key: %s%s", key.c_str(), CRLF);
  if (protocol.length() > 0) {
    client.printf("Sec-WebSocket-Protocol: %s%s", protocol.c_str(), CRLF);
  }
  client.printf("Sec-WebSocket-Version: 13%s", CRLF);
  client.print(CRLF);
}

void WebSocketClient::waitForResponse()
{
  while (client.connected() && !client.available()) {
    delay(100);
  }
}

String WebSocketClient::readHttpLine()
{
  String line = client.readStringUntil(LF);
  return line.substring(0, line.length() - 1 /* len(CR) */);
}

bool WebSocketClient::parseResponse(ParsedResponse& parsed) // TODO: use HTTPClient
{
  String line;
  while ((line = readHttpLine()) != "") {
    ESP_LOGD(LOG_TAG, "HTTP: %s", line.c_str());

    String lower(line);
    lower.toLowerCase();
    if (lower.startsWith(HTTP_STATUS_LINE_101)) {
      parsed.is101 = true;
    }
    if (lower.startsWith(HTTP_UPGRADE_WEBSOCKET)) {
      parsed.hasUpgrade = true;
    }
    if (lower.startsWith(HTTP_CONNECTION_UPGRADE)) {
      parsed.hasConnection = true;
    }
    if (lower.startsWith(HTTP_HEADER_SEC_WEBSOCKET_ACCEPT)) {
      parsed.serverKey = line.substring(HTTP_HEADER_SEC_WEBSOCKET_ACCEPT.length());
    }
  }
  ESP_LOGD(LOG_TAG, "parsed: %s", parsed.toString().c_str());
  return parsed.isValid();
}

WebSocketReadResult WebSocketClient::read(Stream& data, uint8_t& opcode)
{
  if (!client.available()) {
    ESP_LOGD(LOG_TAG, "read: !client.available");
    return WebSocketReadResult::NotAvailable;
  }

  const uint8_t frameType = readByte();
  if (!(frameType & WS_FIN)) { // if FIN = 0
    ESP_LOGD(LOG_TAG, "read: !(frameType & WS_FIN)");
    return WebSocketReadResult::NotSupported; // TODO: multi frames
  }

  bool hasMask;
  size_t length;
  const uint8_t maskAndLength = readByte();
  if (maskAndLength & WS_MASK) {
    hasMask = true;
    length = maskAndLength & ~WS_MASK;
  } else {
    hasMask = false;
    length = maskAndLength;
  }

  if (length == WS_SIZE16) {
    length = readByte() << 8;
    length |= readByte();
  } else if (length == WS_SIZE64) {
    ESP_LOGD(LOG_TAG, "read: length == WS_SIZE64");
    return WebSocketReadResult::NotSupported; // TODO: too large for ESP32
  }

  uint8_t mask[4];
  if (hasMask && !readMask(mask)) {
    ESP_LOGD(LOG_TAG, "read: hasMask && !readMask(mask)");
    return WebSocketReadResult::InvalidFrame;
  }

  opcode = frameType & ~WS_FIN;

  for (int i = 0; i < length; ++i) {
    uint8_t b = readByte();
    if (hasMask) {
      b ^= mask[i % 4];
    }
    data.write(b);
  }
  return WebSocketReadResult::Success;
}

bool WebSocketClient::readMask(uint8_t (&mask)[4])
{
  if (client.available() < 4) {
    return false;
  }
  mask[0] = readByte();
  mask[1] = readByte();
  mask[2] = readByte();
  mask[3] = readByte();
  return true;
}

int WebSocketClient::readByte(const bool wait)  // TODO: expected<int, ReadError>
{
  while (wait && !client.available()) {
    delay(10);
  }
  return client.read();
}

void WebSocketClient::close()
{
  if (closed) {
    return;
  }

  client.write((uint8_t) (WS_FIN | WS_OPCODE_CLOSE));
  client.write((uint8_t) 0x00);
  client.flush();
  // TODO
  closed = true;

  delay(10);
  client.stop();
}

class ClientTxBuffer {
private:
  Client& client;
  static const size_t BUFFER_SIZE = 1360; // https://github.com/espressif/arduino-esp32/blob/fc737e08c617d27414ebf1a574f7ebdf9a968f38/libraries/WiFi/src/WiFiClient.cpp#L387
  uint8_t buffer[BUFFER_SIZE];
  size_t index;

public:
  ClientTxBuffer(Client& client)
    : client(client)
    , index(0)
  {}
  ~ClientTxBuffer()
  {
    flush();
  }

  void write(const uint8_t value)
  {
    buffer[index++] = value;
    if (index == BUFFER_SIZE) {
      flush();
    } else if (index > BUFFER_SIZE) {
      ESP_LOGE(LOG_TAG, "index out of bounds: %d (max = %d)", index, BUFFER_SIZE);
    }
  }

  void write(const uint8_t data[], const size_t length)
  {
    for (size_t i = 0; i < length ; ++i) {
      write(data[i]);
    }
  }

  void flush()
  {
    if (index > 0) {
      client.write(buffer, index);
      index = 0;
    }
  }
};

WebSocketWriteResult WebSocketClient::write(Stream& data, const uint8_t opcode)
{
  if (!client.connected()) {
    return WebSocketWriteResult::NotAvailable;
  }

  ClientTxBuffer buffer(client);
  buffer.write(opcode | WS_FIN);

  const size_t length = data.available();
  if (length < 126) {
    buffer.write((uint8_t) length | WS_MASK);
  } else if (length < (uint16_t)(-1 & 0xFFFF)) {
    buffer.write(WS_SIZE16 | WS_MASK);
    buffer.write((uint8_t)(length >> 8));
    buffer.write((uint8_t)(length & 0xFF));
  } else {
    return WebSocketWriteResult::NotSupported;
  }

  uint8_t mask[4];
  generateMask(mask);
  buffer.write(mask, 4);

  for (int i = 0; i < length; ++i) {
    buffer.write(data.read() ^ mask[i % 4]);
  }
  buffer.flush();
  return WebSocketWriteResult::Success;
}
