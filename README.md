# OSCulate

- [OSCulate](#osculate)
  - [About](#about)
  - [Supported Devices](#supported-devices)
  - [Why does this exist?](#why-does-this-exist)
  - [The OSC Protocol](#the-osc-protocol)
  - [OSC Communication](#osc-communication)
    - [OSC over Serial](#osc-over-serial)
    - [OSC over UDP](#osc-over-udp)
    - [OSC over TCP](#osc-over-tcp)
    - [Networking](#networking)
    - [DHCP and Fallback IP Addressing](#dhcp-and-fallback-ip-addressing)
    - [Console Discovery](#console-discovery)
  - [Future plans](#future-plans)

## About

OSCulate is a c++ program written for the sole purpose of creating an EOS keyboard that communicates over OSC, rather than
via .
At the moment, it only support keyboards, but I plan to extend support to buttons and other sorts of input devices.

## Supported Devices

- Teensy 4.0
- ETC EOS Family Software v3.1+

If your device or firmware is not on here, please see the [Future Plans] section for more information

## Why does this exist?

As an ETC Nomad user, I wanted to make my ETCNomad keyboard, based on Josh Spurgers writeup [here](http://spurgers.design/2020-03-05-001.html), a bit more robust in how it communicates with ETCNomad.
Currently, the Cherry SPOS keyboards have a few downsides when used as an EOS Keyboard.
In order to program the keyboard, each key can be either a single key or a key macro.

Unfortunately for us, the key macros send the entire key series at once, and do not work with long pressing and holding down the key.
For example, the EOS keyboard shortcut for the data key is `ctrl D`.
When setting a key on the Cherry SPOS to use the key macro of `ctrl D`, the cherry board sends key down events for both ctrl and D, but then immediately following sends key up events for ctrl and D, without ever waiting for the trigger key to be released.

In order to fix this, and ensure that this fix could work on _any_ computer without needing to install specific software, I knew I needed to process the commands before they even went to the console.

I also wanted to improve the communication, as communicating like a keyboard means that pressing whatever key on said keyboard goes to the focused program.
This is not ideal, as pressing live while having the wrong program selected results in sending f1 to a program that isn't EOS. This is far from ideal, as we want our EOS keyboard to be functional all of the time.

Thankfully, EOS has support for this, by using the OSC protocol to communicate.

## The OSC Protocol

EOS family software supports all of the forms of OSC protocols: UDP, TCP, and even OSC over USB serial.

In order to make our input more robust, we want to communicate over USB or TCP.

By using OSC over a safe protocol, we can establish that every key we press will be registered by EOS, regardless of what program we currently have selected, or if we're even nearby. By supporting TCP, we can technically use our keyboard from any ethernet port on the network, effectively making a remote programming station. Now all we need is a display...

## OSC Communication

### OSC over Serial

Not implemented yet.

### OSC over UDP

Technically implemented, but not recommended.

### OSC over TCP

OSC also supports being sent over a TCP connection, with a similar format to a UDP packet. Both OSC 1.0 and OSC 1.1 are supported, with the option currently set in [config.h](./src/config.h).

### Networking

In order to further ensure that OSCulate can be robust without needing to be a pain point of configuration or other issues with programming, OSCulate does a few things about the network to ensure that it can be most usable.

Due to simplicity, this library is currently using the undocumented APIs that exist for the [aRFR mobile apps](https://www.etcconnect.com/WorkArea/DownloadAsset.aspx?id=10737502822).

### DHCP and Fallback IP Addressing

On boot, and any time OSCulate is connected to a network, it will first attempt to receive an IP from a DHCP server. If it fails to receive an IP within about 15 seconds, it will fallback to a hardcoded IP address on the subnet 10.101.0.0, with a subnet mask of 255.255.0.0. This is the reccommended subnet configuration as suggested by [EOS themselves](https://support.etcconnect.com/ETC/Networking/General/ETC_Network_IP_Addresses), which should be supported by all networks, unless they've been changed by an installer.

### Console Discovery

Once the network interface is operational and has an IP address (either by DHCP or by static fallback), we attempt to find any consoles existing on the network with the Visible to Remotes setting enabled.

EOS has quite a few undocumented portions of code, however, with this functional on eos 2.x and 3.x, it is safe to say that it is unchanging for a long, long, time. That said, I am aware this may not always function on newer versions, and so we fall back to a Fallback IP when we cannot find an IP.

## Future plans

Note: not all of these ideas below will come to fruitition.

- process discovery requests
- process commands we receive from EOS
- version detection of the eos console.
  - Support staging_mode vs scroll_lock for the same key.
  - This adds support for EOS 2.9
- teensy 4.0, (build flag to remove networking related code)
- fallback to OSC Serial
- SLP protocol support to determine what computers are running EOS,
  - this is functional without using the undocumented /etc/discovery/request function listening on port 3034
  - unlike the visible to remotes setting, this does not turn off as its necessary for multiconsole
- support for additional ports
  - 3032 is the default OSC port, it runs either 1.0 or 1.1 and can be disabled if OSC is disabled.
  - 3036 is running an OSC TCP v1.0 server any time that allow remotes is enabled.
  - 3037 is the official third party OSC port as of version 3.1+
  - 3033 is simply a connection test port

[^hardware_macros]: support for hardware macros is not implemented as this does not extend well when moving to a different input mechanism. The goal of OSCulate is to make a third party keyboard have a robust input, similar to a dedicated faceplate or programming wing. It is not to give it additional features that a console does not have. Not yet, anyways.
