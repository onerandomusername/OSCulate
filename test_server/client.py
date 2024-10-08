from pythonosc.tcp_client import SimpleTCPClient
from time import sleep

ip = "127.0.0.1"
port = 6379

sleep(2)
print("Sending messages to server...")
client = SimpleTCPClient(ip, port, mode="1.0")  # Create client

client.send_message(
    "/some/address", [1, 2.0, "hello"]
)  # Send message with int, float and string
client.send_message("/keypress/h", 1.0)  # Send float message
print("send press h...")
sleep(2.500)
client.send_message("/keypress/h", 0.0)  # Send float message
print("send release h...")
