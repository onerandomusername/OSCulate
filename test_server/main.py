"""Small example OSC server

This program listens to several addresses, and prints some information about
received packets.
"""

import argparse
import atexit

import ahk
import ahk.keys
from pythonosc import osc_tcp_server
from pythonosc.dispatcher import Dispatcher

AHK = ahk.AHK()

AHK.set_send_mode("Input")

ENABLE_AUTOHOTKEY: bool = False

KEYS_PRESSED = set()


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
                KEYS_PRESSED.add(key)
                AHK.key_down(key)
        print("Key down: ", key)
    else:
        if ENABLE_AUTOHOTKEY and key != "CapsLock":
            AHK.key_up(key)
            KEYS_PRESSED.discard(key)

        print("Key up: ", key)


def print_log(ip_addr, osc_addr, *args):
    print("[{0[0]}:{0[1]} {1}] ~ {2}".format(ip_addr, osc_addr, args))


@atexit.register
def at_exit():
    for key in KEYS_PRESSED:
        AHK.key_up(key)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ip", default="0.0.0.0", help="The ip to listen on")
    parser.add_argument("--port", type=int, default=6379, help="The port to listen on")
    parser.add_argument("--autohotkey", action="store_true", help="Enable AutoHotkey")
    parser.add_argument(
        "--slip",
        action="store_const",
        const="1.1",
        default="1.0",
        help="Which version of OSC to use, provide to use 1.1",
    )
    args = parser.parse_args()

    ENABLE_AUTOHOTKEY = args.autohotkey or False

    dispatcher = Dispatcher()
    dispatcher.map("/keypress/*", send_keypress, needs_reply_address=True)
    dispatcher.set_default_handler(print_log, needs_reply_address=True)

    server = osc_tcp_server.ThreadingOSCTCPServer(
        (args.ip, args.port),
        dispatcher,
        mode=args.slip,
    )
    print("Serving on {0}:{1}".format(*server.server_address))
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("Server closed")
