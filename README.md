# NibeGW-ProdinoMKR

## UDP Gateway for Nibe heat pumps

This repository is derived from
[NibeGW project](https://github.com/openhab/openhab-addons/tree/2.5.x/bundles/org.openhab.binding.nibeheatpump/contrib/NibeGW/Arduino/NibeGW)
(OpenHAB).

While the original project was intended to run on 8-bit Arduino, this port has been implemented on a 32-bit ARM Cortex M0+ platform:

### ProDino MKR Zero Ethernet

    * ATSAMD21G18 MCU
    * W5500 Ethernet Controller
    * RS-485 interface
    * 4 relays
    * 4 opto inputs
    * wide supply range 8..30 VDC
    * DIN rail mountable enclosure

This 32-bit hardware can also be programmed using the Arduino IDE by installing support for the Arduino MKR GSM 1400 platform.
