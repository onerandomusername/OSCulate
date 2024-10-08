#pragma once

#ifndef Network_h
#define Network_h

#include "SLIPEncodedSerial.h"
#include "SLIPEncodedTCP.h"
#include "config.h"
#include <Arduino.h>
#include <OSCBundle.h>
#include <OSCMessage.h>
#include <QNEthernet.h>
#include <list>
#include <string>

using namespace qindesign::network;

bool networkStateChanged = false;

// The prefix for the OSC address that we will send to the console.
const char addressPrefix[] = "/eos/key/";

// time since last connection attempt
elapsedMillis sinceLastConnectAttempt;

// whether we have an IP address or not
bool gotIP = false;

// TCPConnection
EthernetClient tcp = EthernetClient();
SLIPEncodedTCP slip(tcp);

/// @brief Send an OSC message over the network using OSC v1.0 over TCP.
/// @param msg The OSCMessage to send.
void sendOSCviaPacketLength(OSCMessage &msg) {
  uint8_t buffer[4];
  const auto len = msg.bytes();
  // make the first four bytes the count of len
  buffer[0] = (len >> 24) & 0xFF;
  buffer[1] = (len >> 16) & 0xFF;
  buffer[2] = (len >> 8) & 0xFF;
  buffer[3] = len & 0xFF;
  tcp.write(buffer, 4);
  msg.send(tcp);
  tcp.flush();
}

/// @brief Send an OSC message over the network using OSC v1.1 over TCP.
/// @param msg The OSCMessage to send.
void sendOSCviaSLIP(OSCMessage &msg) {
  slip.beginPacket();
  msg.send(slip);
  slip.endPacket();
}

/// @brief Send the Eos key to the console over OSC.
/// @param key the key that was pressed. This should be the already formatted
/// Eos key, eg "at"
/// @param isDown Whether the key was just pressed down or just released.
void sendRemoteCommand(const char key[], bool isDown) {
  Serial.print("Got key: ");
  Serial.println(key);
  std::string address(addressPrefix);
  address += key;
  OSCMessage msg(address.c_str());
  msg.add(isDown ? 1.0 : 0.0);
  Serial.printf("Address: %s\r\n", address.c_str());

  if (oscversion == OSCVersion::SLIP) {
    sendOSCviaSLIP(msg);
  } else {
    sendOSCviaPacketLength(msg);
  }

  msg.empty(); // free space occupied by message
}

/// @brief Get the IP address of an L console from the network.
/// @return true if an IP address was found, false otherwise.
/// this BLOCKING method is a contained routine for getting the IP address from
/// the network.
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
}

/// @brief Connect to the LX console over TCP.
/// @return Whether the connection was successful.
bool connectToLXConsole() {
  if (!tcp.connectionId()) {
    Serial.print("Connecting to LX console at: ");
    Serial.printf(" %u.%u.%u.%u\r\n", DEST_IP[0], DEST_IP[1], DEST_IP[2],
                  DEST_IP[3]);
    if (!tcp.connect(DEST_IP, outPort)) {
      return false;
    }
    tcp.setConnectionTimeout(600);
    Serial.println("Connected to LX console.");
    networkStateChanged = true;
  }
  return true;
}

void setupNetworking() {
  Ethernet.onLinkState([](bool state) {
    if (state) {
      Serial.println("[Ethernet] Link ON");
    } else {
      Serial.println("[Ethernet] Link OFF");
      gotIP = false;
      if (tcp) {
        tcp.abort();
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

  getEthernetIPFromNetwork();

  getLXConsoleIP();
  connectToLXConsole();
}

void checkNetwork() {
  if (!gotIP) {
    getEthernetIPFromNetwork();
  }
  if (!tcp.connectionId()) {
    networkStateChanged = true;
    if (sinceLastConnectAttempt > TCPConnectionCheckTime) {
      sinceLastConnectAttempt = 0;
      if (!connectToLXConsole()) {
        Serial.println("Failed to connect to LX Console");
      }
    }
  }

  if (tcp.available()) {
    while (tcp.available()) {
      tcp.read();
    }
  }
};

#endif // Network_h
