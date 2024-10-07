# OSCulate

- [OSCulate](#osculate)
  - [About](#about)
  - [Supported Devices](#supported-devices)
  - [Backstory](#backstory)
  - [The OSC Protocol](#the-osc-protocol)
  - [OSC Communication](#osc-communication)
    - [OSC over Serial](#osc-over-serial)
    - [OSC over UDP](#osc-over-udp)
    - [OSC over TCP](#osc-over-tcp)
  - [Networking](#networking)
    - [DHCP and Fallback IP Addressing](#dhcp-and-fallback-ip-addressing)
    - [Console Discovery](#console-discovery)
  - [Advanced](#advanced)
    - [Usage of Undocumented Eos Features](#usage-of-undocumented-eos-features)
      - [Usage of Mobile Apps API](#usage-of-mobile-apps-api)
      - [Usage of console discovery](#usage-of-console-discovery)
  - [Future Plans](#future-plans)
  - [Copyright Disclosure](#copyright-disclosure)

## About

OSCulate is a C++ program written for the sole purpose of creating an ETCnomad keyboard that communicates over OSC, rather than over USB HID.
At the moment, OSCulate only supports keyboards, but I plan to extend support to buttons and other sorts of input devices.

## Supported Devices

- Teensy 4.1
- ETC Eos Family Software v3.1+

If your device or firmware is not yet on here, please see the [Future Plans](#future-plans) section for more information.

## Backstory

As an ETCnomad user, I wanted to make my ETCNomad keyboard, based on Josh Spurgers' writeup [here](http://spurgers.design/2020-03-05-001.html), a bit more robust in how it communicates with ETCNomad.
Currently, the Cherry SPOS keyboards have a few downsides when used as an Eos Keyboard.
In order to program the keyboard, each key can be either a single key or a key macro.

Unfortunately for us, the key macros send the entire key series at once, and do not work with long pressing and holding down the key.
For example, the Eos keyboard shortcut for the data key is `ctrl D`.
When setting a key on the Cherry SPOS to use the key macro of `ctrl D`, the cherry board sends key down events for both ctrl and D, but then immediately following sends key up events for ctrl and D, without ever waiting for the trigger key to be released.

In order to fix this, and ensure that this fix could work on _any_ computer without needing to install specific software, I knew I needed to process the commands before they even went to the console.

I also wanted to improve the communication, as communicating like a keyboard means that pressing whatever key on said keyboard goes to the focused program. We could even implement hardware macros if we wanted![^hardware_macros]
This is not ideal, as pressing live while having the wrong program selected results in sending f1 to a program that isn't Eos. This is far from ideal, as we want our Eos keyboard to be functional all of the time.

Thankfully, Eos has support for this, by using the OSC protocol to communicate.

## The OSC Protocol

Eos family software supports all of the forms of OSC protocols: UDP, TCP, and even OSC over USB Serial.

In order to make our input more robust, we want to communicate over USB or TCP. Unlike USB or TCP, UDP is not reliant as it does not contain native assurances our messages have arrived. With TCP, we have built in retransmission support for our commands, in the event they were dropped by the network.

## OSC Communication

### OSC over Serial

Not implemented.

### OSC over UDP

While you can technically use OSC with TCP, this library does implement direct support for sending OSC commands with UDP as it is considered a footgun.

### OSC over TCP

OSC supports being sent over a TCP connection, with a similar format to a UDP packet. Both OSC v1.0 and OSC v1.1 are supported, with the option currently set in [config.h](./src/config.h).

## Networking

In order to further ensure that OSCulate can be robust without needing to be a pain point of configuration or other issues with programming, OSCulate attempts to make no assumptions about the network or console environments it is working on.

### DHCP and Fallback IP Addressing

On boot, and any time OSCulate is connected to a network, it will first attempt to receive an IP from a DHCP server. If it fails to receive an IP within about 15 seconds, it will fallback to a hardcoded IP address on the subnet 10.101.0.0, with a subnet mask of 255.255.0.0. This is the reccommended subnet configuration as suggested by [Eos themselves](https://support.etcconnect.com/ETC/Networking/General/ETC_Network_IP_Addresses), which should be supported by all networks, unless they've been changed by an installer.

### Console Discovery

Once the network interface is operational and has an IP address (either by DHCP or by static fallback), we attempt to find any consoles existing on the network with the Visible to Remotes setting enabled.

Eos has quite a few undocumented portions of code, however, with this functional on Eos 2.x and 3.x, it is safe to say that it is unchanging for a long, long, time. That said, I am aware this may not always function on newer versions, and so we fall back to a Fallback IP when we cannot find an IP.

Due to simplicity, this library is currently using the undocumented APIs that exist for the [aRFR mobile apps](https://www.etcconnect.com/WorkArea/DownloadAsset.aspx?id=10737502822).

A more detailed write-up is accessible in the Advanced section, under [Usage of console discovery](#usage-of-console-discovery)

## Advanced

### Usage of Undocumented Eos Features

The two usages of undocumented features implemented as of now are:

- use of the mobile apps OSC server
- use of the undocumented Eos OSC discovery server, which among other things, exists for the official mobile apps to automatically show existing consoles on the network. It is only functional when Visible to Remotes (Visible to Mobile Apps) setting is turned on.

Both of these undocumented features can be bypassed with official means, but the negatives would far outweigh the positives.

#### Usage of Mobile Apps API

The mobile app clients for Eos connect to an undocumented OSC server running at port 3036, which is running version 1.0 for backwards compatibility. It is always accepting connections, but only processes OSC commands if the Allow Remotes setting is turned on in the console.

This does the following:

- Pros:
  - Always a v1.0 OSC TCP server when enabled
  - There is only one toggle in the console for this connection
  - the same behaviour on version 2.9 and 3.x
- Cons:
  - Undocumented server

There are two main options we can switch to, though both also have their pros and cons.

- **3032**
  - Pros:
    - Official recommendation
    - Lots of customization options
  - Cons:
    - Requires the user to set settings in their console, or in the code here
    - Can be any port at all
    - not always be available
    - can be either OSC v1.0 or v1.1
    - Requires several settings to be enabled
- **3037**
  - Pros:
    - When enabled, always a v1.1 OSC TCP server
    - There is only one toggle for this server
    - Same settings control this enpoint as 3036
  - Cons:
    - Only supported since Eos 3.1+

This library wants to be the most functional as it can be. As such, it currently uses port 3036 as that has the longest backwards compatability, and easiest to develop for while supporting both Eos 2.9 and 3.X. A future version may add support for attempting port 3037 connections before falling back to port 3036.

#### Usage of console discovery

ETC consoles run a listener for a discovery request, sent over an UDP OSC request, for the visible to remotes option. If it is enabled, the listener will also respond.

However, this API can be considered somewhat stable and public, as the Luminosus Eos Edition used the [same API](https://github.com/ETCLabs/LuminosusEosEdition/blob/fb9fe30d285812312e93cf2678c62155f50e2f07/src/eos_specific/OSCDiscovery.h#L12-L35).

## Future Plans

Note: not all of these ideas below will come to fruitition.

- process discovery replies
- process commands we receive from Eos
- version detection of the Eos console.
  - Support staging_mode vs scroll_lock for the same key.
  - This adds support for Eos 2.9
- teensy 4.0, (build flag to remove networking related code)
- fallback to OSC Serial
- SLP protocol support to determine what computers are running Eos,
  - this is functional without using the undocumented /etc/discovery/request function listening on port 3034
  - unlike the visible to remotes setting, this does not turn off as its necessary for multiconsole
- support for additional ports
  - see [Usage of Mobile Apps API](#usage-of-mobile-apps-api) for more information

## Copyright Disclosure

Electronic Theatre Controls has trademarks on the following:

- Eos
- ETCNomad
- ETC

Usage of their trademark does not signify an endorsement, or partnership, or relation of any kind.
This is not official ETC software.

Trademark and patent info: [etcconnect.com/IP](https://etcconnect.com/IP)

[^hardware_macros]: support for hardware macros is not implemented as this does not extend well when moving to a different input mechanism. The goal of OSCulate is to make a third party keyboard have a robust input, similar to a dedicated faceplate or programming wing. It is not to give it additional features that a console does not have. Not yet, anyways.
