#pragma once

#ifndef OSC_BASE_h
#define OSC_BASE_h

#include "SLIPEncodedTCP.h"
#include "config.h"
#include <OSCMessage.h>

// The prefix for the OSC address that we will send to the console.
const char addressPrefix[] = "/eos/key/";

enum class OSCVersion {
  PacketLength,
  SLIP,
};

class Connection {
public:
  Connection(OSCVersion version) : _oscVersion(version) {}

  virtual bool connectToConsole() = 0;
  virtual void disconnectFromConsole() = 0;
  virtual bool isConnected() = 0;
  virtual void send(OSCMessage &msg) = 0;

  OSCVersion getOSCVersion() { return _oscVersion; };

  virtual void Task() = 0;

  virtual ~Connection() = default;

protected:
  void setOSCVersion(OSCVersion version) { _oscVersion = version; };

private:
  OSCVersion _oscVersion;
};

class OSCClient {
public:
  OSCClient(Connection &connection);
  void send(OSCMessage &msg);
  // void send(OSCBundle &bundle);
  // shortcut to send a message for a specific key
  void sendEosKey(const char key[], bool isDown);
  OSCVersion getOSCVersion() { return connection.getOSCVersion(); };
  bool connectToConsole() { return connection.connectToConsole(); };
  void disconnectFromConsole() { connection.disconnectFromConsole(); };
  bool isConnected() { return connection.isConnected(); };

  void Task();

private:
  Connection &connection;
}; // class OSCClient

void sendOSCviaPacketLength(OSCMessage &msg, Stream &transport);

// void sendOSCviaSLIP(OSCMessage &msg, SLIPEncodedSerial &transport);
void sendOSCviaSLIP(OSCMessage &msg, SLIPEncodedTCP &transport);

#endif // OSC_BASE_h
