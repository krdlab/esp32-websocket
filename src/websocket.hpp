// Copyright (c) 2019 Sho Kuroda <krdlab@gmail.com>
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#ifndef WEBSOCKET_CLIENT_HPP_
#define WEBSOCKET_CLIENT_HPP_

#include <Client.h>
#include <StreamString.h>

#define WS_OPCODE_CONTINUATION  0x00
#define WS_OPCODE_TEXT    0x01
#define WS_OPCODE_BINARY  0x02
// NOTE: 0x03 - 0x07 are reserved for further non-control frames
#define WS_OPCODE_CLOSE   0x08
#define WS_OPCODE_PING    0x09
#define WS_OPCODE_PONG    0x0a
// NOTE: 0x0B - 0x0F are reserved for further control frames


class Payload : public StreamString // FIXME: NOT String
{
public:
  void purge() { invalidate(); }
};

struct ParsedResponse;

enum class WebSocketConnectResult
{
  Success,
  ConnectFailure,
  HandshakeFailure,
};
enum class WebSocketReadResult
{
  Success,
  NotAvailable,
  InvalidFrame,
  ReadTimeout,
  NotSupported,
};
enum class WebSocketWriteResult
{
  Success,
  NotAvailable,
  NotSupported,
};


class WebSocketClient
{
private:
  Client& client;
  bool closed;

public:
  WebSocketClient(Client& client)
    : client(client)
    , closed(false)
  {}
  ~WebSocketClient()
  {
    close();
  }

  WebSocketConnectResult connect(const char* host, const uint16_t port, const char* path, const char* protocol = NULL);
  void close();
  WebSocketWriteResult write(Stream& data, const uint8_t opcode);
  WebSocketReadResult read(Stream& data, uint8_t& opcode);

  bool connected() const { return client.connected(); }
  int available() { return client.available(); }
  void waitForAvailable() { waitForResponse(); }
  void waitForResponse(); // TODO: timeout

private:
  bool handshake(const String& host, const String& path, const String& protocol);
  String generateKey();
  String calculateServerKey(const String& clientKey);

  bool requestUpgrade(const String& path, const String& host, const String& key, const String& protocol);
  String readHttpLine();
  bool parseResponse(ParsedResponse& parsed);

  int readByte(const bool wait = true);
  bool readMask(uint8_t (&mask)[4]);
};

#endif
