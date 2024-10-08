#pragma once

#ifndef Network_h34
#define Network_h34

#include "SLIPEncodedTCP.h"
#include "config.h"
#include "osc_base.h"
#include <Arduino.h>
#include <OSCBundle.h>
#include <OSCMessage.h>
#include <QNEthernet.h>
#include <list>
#include <string>

using namespace qindesign::network;

inline bool networkStateChanged = false;
inline bool gotIP = false;

bool getLXConsoleIP();
bool getEthernetIPFromNetwork();
void setupNetworking();
void checkNetwork();

class TCPConnection : public Connection {

public:
  TCPConnection(OSCVersion version);
  TCPConnection(EthernetClient eth, OSCVersion version);
  bool connectToConsole();
  void disconnectFromConsole();
  bool isConnected() { return transport.connected(); };
  void send(OSCMessage &msg);

  void Task();

private:
  EthernetClient transport;
  SLIPEncodedTCP slip;

}; // class TCPConnection

inline TCPConnection conn(OSCVersion::PacketLength);
inline OSCClient client(conn);

#endif // Network_h
