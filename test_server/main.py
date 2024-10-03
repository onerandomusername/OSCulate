"""Small example OSC server

This program listens to several addresses, and prints some information about
received packets.
"""

import argparse

from pythonosc.dispatcher import Dispatcher
from pythonosc import osc_server
import ahk

AHK = ahk.AHK()

AHK.set_send_mode('Input')

ENABLE_AUTOHOTKEY: bool = False


def send_keypress(ip_addr: tuple[str, int], osc_addr: str, value: float, *args):
    key = osc_addr.removeprefix("/keypress/")
    if not key:
        if value:
            print("No key specified")
        return

    if key == "LGUI":
        key = "LWin"
    elif key == "RGUI":
        key = "RWin"
    if value == 1.0:
        if ENABLE_AUTOHOTKEY:
            if key == "CapsLock":
                AHK.set_capslock_state(
                    "On" if not AHK.key_state("CapsLock", mode="T") else "Off"
                )

            else:
                AHK.key_down(key)
        print("Key down: ", key)
    else:
        if ENABLE_AUTOHOTKEY and key != "CapsLock":
            AHK.key_up(key)
        print("Key up: ", key)


def print_log(ip_addr, osc_addr, *args):
    print("[{0[0]}:{0[1]} {1}] ~ {2}".format(ip_addr, osc_addr, args))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ip", default="0.0.0.0", help="The ip to listen on")
    parser.add_argument("--port", type=int, default=6379, help="The port to listen on")
    parser.add_argument("--autohotkey", action="store_true", help="Enable AutoHotkey")
    args = parser.parse_args()

    ENABLE_AUTOHOTKEY = args.autohotkey or False

    dispatcher = Dispatcher()
    dispatcher.map("/keypress/*", send_keypress, needs_reply_address=True)
    dispatcher.set_default_handler(print_log, needs_reply_address=True)

    server = osc_server.ThreadingOSCUDPServer((args.ip, args.port), dispatcher)
    print("Serving on {0}:{1}".format(*server.server_address))
    server.serve_forever()
