
#include "network.h"
#include "SLIPEncodedTCP.h"
#include "config.h"
#include "osc_base.h"
#include "ulog.h"
#include <Arduino.h>
#include <OSCBundle.h>
#include <OSCMessage.h>
#include <QNEthernet.h>
#include <set>

const std::string_view discoveryReplyAddress =
    std::string_view("/etc/discovery/reply");

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
  transport.flush();
  if (transport.status() != ESTABLISHED) {
    ULOG_DEBUG("Transport status: %i", transport.status());
    ULOG_WARNING("Aborting transport and recreating.");
    transport.abort();
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
    if (DEST_IP == INADDR_NONE) {
      ULOG_INFO("IP set to NULL, trying to get it from the network.");
      if (!getLXConsoleIP()) {
        return false;
      }
    }
    ULOG_INFO("Connecting to LX console at: %u.%u.%u.%u", DEST_IP[0],
              DEST_IP[1], DEST_IP[2], DEST_IP[3]);
    if (!transport.connect(DEST_IP, outPort)) {
      return false;
    }
    transport.setConnectionTimeout(600);
    transport.setTimeout(600);
    ULOG_INFO("Connected to LX console.");
    networkStateChanged = true;
  }
  return true;
};

void TCPConnection::disconnectFromConsole() {
  if (transport.connected()) {
    transport.abort();
    ULOG_INFO("Disconnected from LX console.");
    networkStateChanged = true;
  }
};

// time since last connection attempt
elapsedMillis sinceLastConnectAttempt;
uint16_t sinceLastGatheredConsoles = 0;

// TCPConnection
// EthernetClient tcp = EthernetClient();
// SLIPEncodedTCP slip(tcp);

/// @brief Get the IP address of an L console from the network.
/// @return true if an IP address was found, false otherwise.
/// this BLOCKING method is a contained routine for getting the IP address
/// from the network.
bool getLXConsoleIP() {
  ULOG_INFO("Looking for consoles");
  EthernetUDP udpClient = EthernetUDP();
  EthernetUDP udpServer = EthernetUDP(10);
  udpServer.begin(3035);

  std::set<IPAddress> foundIPs;

  for (int i = 0; i < 3; i++) {
    udpClient.beginPacket(IPAddress(255, 255, 255, 255), 3034);
    // the content of the packet must be exactly this for eos detection
    // undocumented protocol that we don't really have documentation for
    unsigned char bytes[] = {
        0x2f, 0x65, 0x74, 0x63, 0x2f, 0x64, 0x69, 0x73, 0x63, 0x6f, 0x76,
        0x65, 0x72, 0x79, 0x2f, 0x72, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74,
        0x0,  0x0,  0x2c, 0x69, 0x73, 0x0,  0x0,  0x0,  0xb,  0xdb, 0x52,
        0x46, 0x52, 0x20, 0x4f, 0x53, 0x43, 0x20, 0x44, 0x69, 0x73, 0x63,
        0x6f, 0x76, 0x65, 0x72, 0x79, 0x0,  0x0,  0x0};
    udpClient.write(bytes, sizeof(bytes));
    udpClient.endPacket();
    ULOG_INFO("Sent discovery request to broadcast: %u/3", i + 1);

    elapsedMillis serverTimeout = 0;

    while (serverTimeout < 5000) {
      OSCMessage bundleIN;
      int size;
      if ((size = udpServer.parsePacket()) > 0) {

        while (size--) {
          char s = udpServer.read();
          bundleIN.fill(s);
        };
        OSCMessage &msg = bundleIN;
        if (!bundleIN.hasError()) {
        } else if (!msg.hasError()) {
          ULOG_INFO("Bundle was corrupted, but raw message was good.");
        } else {
          ULOG_INFO("Message is also bad");
          while (udpServer.available()) {
            udpServer.read();
          }
          bundleIN.empty();
          continue;
        }

        if (discoveryReplyAddress != msg.getAddress()) {
          // ignore the reply because it wasn't sent to the reply address
          ULOG_INFO("We got a a reply but it was some other address!");
          continue;
        }
        // the format of this message is supposed to be isssTs but who really
        // knows lol
        for (int j = 0; j < msg.size(); j++) {
          ULOG_TRACE("Message type: %c", msg.getType(j));
          if (msg.isString(j)) {
            char buffer[512];
            int wrote = msg.getString(j, buffer, 512);
            if (buffer == CONSOLE_NAME) {
              // return straight up if we found the console we're looking for
              ULOG_INFO("You have found the console you're looking for");
              IPAddress remoteIP = udpServer.remoteIP();
              DEST_IP = remoteIP;
              ULOG_INFO("Using console at: %u.%u.%u.%u", DEST_IP[0], DEST_IP[1],
                        DEST_IP[2], DEST_IP[3]);
              return true;
            }
            ULOG_TRACE("We parsed a string: %s", buffer);
            ULOG_TRACE("It was at position: %u", j);
          };
        };

        IPAddress remoteIP = udpServer.remoteIP();
        ULOG_INFO("A console responded at IP: %u.%u.%u.%u", remoteIP[0],
                  remoteIP[1], remoteIP[2], remoteIP[3]);
        foundIPs.insert(remoteIP);
      };
    };
  };

  if (foundIPs.size() == 0) {
    ULOG_INFO("No consoles found.");
    return false;
  }

  if (foundIPs.size() > 1) {
    ULOG_INFO("Multiple consoles found, using first responded console.");
  }

  DEST_IP = *foundIPs.begin();
  ULOG_INFO("Using console at: %u.%u.%u.%u", DEST_IP[0], DEST_IP[1], DEST_IP[2],
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
    ULOG_ERROR("ERROR: Failed to start Ethernet");
    return false;
  }

  if (!Ethernet.waitForLink(fallbackWaitTime)) {
    ULOG_ERROR("Ethernet link is not up");
    return false;
  }

  if (!Ethernet.waitForLocalIP(fallbackWaitTime)) {
    ULOG_WARNING("Failed to get IP address, trying static");
    if (Ethernet.begin(staticIP, staticSubnetMask, INADDR_NONE))
      ULOG_INFO("Set a static IP address.");
    else
      ULOG_ERROR("Failed to get an IP address.");
  }

  ULOG_INFO("Ethernet started");
  return !!Ethernet.localIP();
};

void setupNetworking() {
  Ethernet.onLinkState([](bool state) {
    if (state) {
      ULOG_INFO("[Ethernet] Link ON");
    } else {
      ULOG_INFO("[Ethernet] Link OFF");
      gotIP = false;
      if (client.isConnected()) {
        client.disconnectFromConsole();
        ULOG_WARNING("[Ethernet] Aborted TCP Connection");
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
      ULOG_INFO("[Ethernet] Address changed:");
      ip = Ethernet.localIP();
      ULOG_INFO("    Local IP     = %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
      ULOG_INFO(Ethernet.isDHCPActive() ? "    DHCP" : "    Static IP");
      ip = Ethernet.subnetMask();
      ULOG_INFO("    Subnet mask  = %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
      ip = Ethernet.broadcastIP();
      ULOG_INFO("    Broadcast IP = %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
      ip = Ethernet.gatewayIP();
      ULOG_INFO("    Gateway      = %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
      ip = Ethernet.dnsServerIP();
      ULOG_INFO("    DNS          = %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    } else {
      ULOG_INFO("[Ethernet] Address changed: No IP");
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

      // look for another console if its been longer than 15 seconds since we've
      // connected
      if (sinceLastGatheredConsoles == 0)
        sinceLastGatheredConsoles = millis();

      bool foundConsole = false;
      if (sinceLastGatheredConsoles != 0 &&
          (millis() - sinceLastConnectAttempt) > 15000) {
        foundConsole = getLXConsoleIP();
      }

      if (!client.connectToConsole()) {
        ULOG_ERROR("Failed to connect to LX Console at %u.%u.%u.%u", DEST_IP[0],
                   DEST_IP[1], DEST_IP[2], DEST_IP[3]);
      }
    }
  } else {
    if (sinceLastGatheredConsoles != 0) {
      sinceLastConnectAttempt = 0;
    }
  }

  client.Task();
};
