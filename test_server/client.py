from pythonosc.udp_client import SimpleUDPClient
from time import sleep

ip = "127.0.0.1"
port = 5005

sleep(2)
print('Sending messages to server...')
client = SimpleUDPClient(ip, port)  # Create client

client.send_message(
    "/some/address", [1, 2.0, "hello"]
)  # Send message with int, float and string
client.send_message("/keypress/h", 1.0)  # Send float message
print('send press h...')
sleep(2.500)
client.send_message("/keypress/h", 0.0)  # Send float message
print('send release h...')
