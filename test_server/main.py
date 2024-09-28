"""Small example OSC server

This program listens to several addresses, and prints some information about
received packets.
"""

import argparse

from pythonosc.dispatcher import Dispatcher
from pythonosc import osc_server
import ahk

AHK = ahk.AHK()


def send_keypress(ip_addr: tuple[str, int], osc_addr: str, value: float, *args):
    key = osc_addr.removeprefix("/keypress/")
    if value == 1.0:
        AHK.key_down(key)
        print("Key down: ", key)
    else:
        AHK.key_up(key)
        print("Key up: ", key)


def print_log(ip_addr, osc_addr, *args):
    print("[{0[0]}:{0[1]} {1}] ~ {2}".format(ip_addr, osc_addr, args))

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ip", default="0.0.0.0", help="The ip to listen on")
    parser.add_argument("--port", type=int, default=6379, help="The port to listen on")
    args = parser.parse_args()

    dispatcher = Dispatcher()
    dispatcher.map("/keypress/*", send_keypress, needs_reply_address=True)
    dispatcher.set_default_handler(print_log, needs_reply_address=True)

    server = osc_server.ThreadingOSCUDPServer((args.ip, args.port), dispatcher)
    print("Serving on {0}:{1}".format(*server.server_address))
    server.serve_forever()
