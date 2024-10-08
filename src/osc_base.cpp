#include "osc_base.h"
#include "SLIPEncodedTCP.h"
#include "config.h"

/// @brief Send an OSC message over the network using OSC v1.0 over TCP.
/// @param msg The OSCMessage to send.
void sendOSCviaPacketLength(OSCMessage &msg, Stream &transport) {
  uint8_t buffer[4];
  const auto len = msg.bytes();
  // make the first four bytes the count of len
  buffer[0] = (len >> 24) & 0xFF;
  buffer[1] = (len >> 16) & 0xFF;
  buffer[2] = (len >> 8) & 0xFF;
  buffer[3] = len & 0xFF;
  transport.write(buffer, 4);
  msg.send(transport);
  transport.flush();
}
/// @brief Send an OSC message over the network using OSC v1.1 over TCP.
/// @param msg The OSCMessage to send.
// void sendOSCviaSLIP(OSCMessage &msg, SLIPEncodedSerial &transport) {
//   transport.beginPacket();
//   msg.send(transport);
//   transport.endPacket();
// }

/// @brief Send an OSC message over the network using OSC v1.1 over TCP.
/// @param msg The OSCMessage to send.
void sendOSCviaSLIP(OSCMessage &msg, SLIPEncodedTCP &transport) {
  transport.beginPacket();
  msg.send(transport);
  transport.endPacket();
}

OSCClient::OSCClient(Connection &connection) : connection(connection) {}

/// @brief Send the provided OSC message to the console.
/// @param msg the OSCMessage to send.
void OSCClient::send(OSCMessage &msg) { connection.send(msg); }

/// @brief Send the Eos key to the console over OSC.
/// @param key the key that was pressed. This should be the already formatted
/// Eos key, eg "at"
/// @param isDown Whether the key was just pressed down or just released.
void OSCClient::sendEosKey(const char key[], bool isDown) {
  Serial.print("Got key: ");
  Serial.println(key);
  std::string address(addressPrefix);
  address += key;
  OSCMessage msg(address.c_str());
  msg.add(isDown ? 1.0 : 0.0);
  Serial.printf("Address: %s\r\n", address.c_str());

  this->send(msg);

  msg.empty(); // free space occupied by message
}

void OSCClient::Task() { this->connection.Task(); }
