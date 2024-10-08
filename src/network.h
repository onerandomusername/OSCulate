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

bool networkStateChanged = false;

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

TCPConnection::TCPConnection(OSCVersion version = OSCVersion::SLIP)
    : transport(), slip(transport) {
  setOSCVersion(version);
  transport = EthernetClient();
  this->slip = SLIPEncodedTCP(transport);
}

TCPConnection::TCPConnection(EthernetClient eth,
                             OSCVersion version = OSCVersion::SLIP)
    : transport(eth), slip(transport) {
  this->slip = SLIPEncodedTCP(eth);
  setOSCVersion(version);
}

void TCPConnection::send(OSCMessage &msg) {
  this->Task();
  if (getOSCVersion() == OSCVersion::SLIP) {
    sendOSCviaSLIP(msg, slip);
  } else {
    sendOSCviaPacketLength(msg, transport);
  }
}

void TCPConnection::Task() {
  if (transport.available()) {
    while (transport.available()) {
      transport.read();
    }
  }
};

/// @brief Connect to the LX console over TCP.
/// @return Whether the connection was successful.
bool TCPConnection::connectToConsole() {
  if (!transport.connectionId()) {
    Serial.print("Connecting to LX console at: ");
    Serial.printf(" %u.%u.%u.%u\r\n", DEST_IP[0], DEST_IP[1], DEST_IP[2],
                  DEST_IP[3]);
    if (!transport.connect(DEST_IP, outPort)) {
      return false;
    }
    transport.setConnectionTimeout(600);
    Serial.println("Connected to LX console.");
    networkStateChanged = true;
  }
  return true;
};

void TCPConnection::disconnectFromConsole() {
  if (transport.connected()) {
    transport.abort();
    Serial.println("Disconnected from LX console.");
    networkStateChanged = true;
  }
};

TCPConnection conn(OSCVersion::PacketLength);
OSCClient client(conn);

// time since last connection attempt
elapsedMillis sinceLastConnectAttempt;

// whether we have an IP address or not
bool gotIP = false;

// TCPConnection
// EthernetClient tcp = EthernetClient();
// SLIPEncodedTCP slip(tcp);

/// @brief Get the IP address of an L console from the network.
/// @return true if an IP address was found, false otherwise.
/// this BLOCKING method is a contained routine for getting the IP address
/// from the network.
bool getLXConsoleIP() {
  Serial.println("Looking for consoles");
  EthernetUDP udp = EthernetUDP();
  udp.begin(3035);

  udp.beginPacket(IPAddress(255, 255, 255, 255), 3034);
  // the content of the packet must be exactly this for eos detection
  // undocumented protocol that we don't really have documentation for
  unsigned char bytes[] = {0x2f, 0x65, 0x74, 0x63, 0x2f, 0x64, 0x69, 0x73, 0x63,
                           0x6f, 0x76, 0x65, 0x72, 0x79, 0x2f, 0x72, 0x65, 0x71,
                           0x75, 0x65, 0x73, 0x74, 0x0,  0x0,  0x2c, 0x69, 0x73,
                           0x0,  0x0,  0x0,  0xb,  0xdb, 0x52, 0x46, 0x52, 0x20,
                           0x4f, 0x53, 0x43, 0x20, 0x44, 0x69, 0x73, 0x63, 0x6f,
                           0x76, 0x65, 0x72, 0x79, 0x0,  0x0,  0x0};
  udp.write(bytes, sizeof(bytes));
  udp.endPacket();
  Serial.println("Sent discovery request to broadcast.");

  std::list<IPAddress> foundIPs;
  // we'll timeout if we don't get any replies
  elapsedMillis timeout = 0;
  OSCBundle bundleIN;
  int size;
  while (timeout < 5000) {
    if ((size = udp.parsePacket()) > 0) {
      Serial.println("Found a console. Maybe. I don't know anymore.");
      while (size--)
        bundleIN.fill(udp.read());

      if (!bundleIN.hasError())
        // honestly too tired to do this right
        // we should parse the entire response
        // but frankly for now i'm assuming if they reply they're a console
        // this does not support a priorty console which is the next goal.
        foundIPs.push_back(udp.remoteIP());
    }
  };

  if (foundIPs.size() == 0) {
    Serial.println("No consoles found");
    return false;
  }

  if (foundIPs.size() > 1) {
    Serial.println("Multiple consoles found, using first");
  }

  DEST_IP = foundIPs.front();
  Serial.print("Found console at: ");
  Serial.printf(" %u.%u.%u.%u\r\n", DEST_IP[0], DEST_IP[1], DEST_IP[2],
                DEST_IP[3]);

  return true;
}

/// @brief Start Ethernet and secure an IP from DHCP or fallback to our
/// preconfigured Static IP.
/// @return true if networking is properly started and we have an IP address,
/// false otherwise.
bool getEthernetIPFromNetwork() {
  if (!!Ethernet.localIP() && Ethernet.linkState()) {
    return false;
  }
  if (!Ethernet.begin()) {
    Serial.println("ERROR: Failed to start Ethernet");
    return false;
  }

  if (!Ethernet.waitForLink(fallbackWaitTime)) {
    Serial.println("Ethernet link is not up");
    return false;
  }

  if (!Ethernet.waitForLocalIP(fallbackWaitTime)) {
    Serial.println("Failed to get IP address, trying static");
    Ethernet.begin(staticIP, staticSubnetMask, INADDR_NONE);
  }

  Serial.println("Ethernet started");
  return !!Ethernet.localIP();
};

void setupNetworking() {
  Ethernet.onLinkState([](bool state) {
    if (state) {
      Serial.println("[Ethernet] Link ON");
    } else {
      Serial.println("[Ethernet] Link OFF");
      gotIP = false;
      if (client.isConnected()) {
        client.disconnectFromConsole();
        Serial.println("[Ethernet] Aborted TCP Connection");
      }
    }
    networkStateChanged = true;
  });

  // Watch for address changes
  // It will take a little time to get an IP address, so watch for it
  Ethernet.onAddressChanged([]() {
    IPAddress ip = Ethernet.localIP();
    bool hasIP = (ip != INADDR_NONE);
    if (hasIP) {
      gotIP = true;
      Serial.printf("[Ethernet] Address changed:\r\n");
      ip = Ethernet.localIP();
      Serial.printf("    Local IP     = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2],
                    ip[3]);
      Serial.println(Ethernet.isDHCPActive() ? "    DHCP" : "    Static IP");
      ip = Ethernet.subnetMask();
      Serial.printf("    Subnet mask  = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2],
                    ip[3]);
      ip = Ethernet.broadcastIP();
      Serial.printf("    Broadcast IP = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2],
                    ip[3]);
      ip = Ethernet.gatewayIP();
      Serial.printf("    Gateway      = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2],
                    ip[3]);
      ip = Ethernet.dnsServerIP();
      Serial.printf("    DNS          = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2],
                    ip[3]);
    } else {
      Serial.println("[Ethernet] Address changed: No IP");
      gotIP = false;
    }
    networkStateChanged = true;
  });

  Ethernet.setHostname(HOSTNAME);
}

void checkNetwork() {
  if (!gotIP) {
    getEthernetIPFromNetwork();
  }
  if (!client.isConnected()) {
    networkStateChanged = true;
    if (sinceLastConnectAttempt > TCPConnectionCheckTime) {
      sinceLastConnectAttempt = 0;
      if (!client.connectToConsole()) {
        Serial.println("Failed to connect to LX Console");
      }
    }
  }

  client.Task();
};

#endif // Network_h
