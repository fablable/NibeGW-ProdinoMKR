/**
 * Copyright (c) 2010-2019 Contributors to the openHAB project
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * ----------------------------------------------------------------------------
 *
 *  Author: pauli.anttila@gmail.com
 *
 *
 *  2.11.2013   v1.00   Initial version.
 *  3.11.2013   v1.01
 *  27.6.2014   v1.02   Fixed compile error and added Ethernet initialization delay.
 *  29.6.2015   v2.00   Bidirectional support.
 *  18.2.2017   v3.00   Redesigned.
 *
 *  This repo https://github.com/fablable/NibeGW-ProdinoMKR by fablable@uamm.de is derived from
 *  https://github.com/openhab/openhab-addons/tree/master/bundles/org.openhab.binding.nibeheatpump/contrib/NibeGW/Arduino/NibeGW
 *
 *  28.12.2019  v3.50   added support for ProDino MKR Zero Ethernet, for library setup see:
 *                      https://kmpelectronics.eu/tutorials-examples/prodino-mkr-versions-tutorial/
 *                      modified Ethernet handling to prevent Nibe from entering error state
 *                      if network connection fails
 *  29.12.2019  v3.51   added initialization for UDP message port
 *                      added options to send debug printouts via serial and/or UDP port
 *                      debug: send array printout in single UDP packet
 */

// ######### CONFIGURATION #######################

#define VERSION                 "3.51"

// Enable if you use ProDiNo board
//#define PRODINO_BOARD
// Enable if you use ProDino MKR Zero
#define PRODINO_MKR0
// Enable if ENC28J60 LAN module is used
//#define TRANSPORT_ETH_ENC28J60

// Enable debug printouts sent via serial port and/or via UDP
// listen printouts e.g. via netcat (nc -l -u 50000)
//#define ENABLE_DEBUG_SER
//#define ENABLE_DEBUG_UDP
#define VERBOSE_LEVEL           3

#if defined ENABLE_DEBUG_SER || defined ENABLE_DEBUG_UDP
#define ENABLE_DEBUG
#endif

#ifdef PRODINO_MKR0
#define BOARD_NAME              "NibeGW running on ProDino MKR Zero"
#else
#define BOARD_NAME              "Arduino NibeGW"
#endif
#define BOARD_MAC               { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }
#define BOARD_IP                { 192, 168, 1, 50 }
#define GATEWAY_IP              { 192, 168, 1, 1 }
#define NETWORK_MASK            { 255, 255, 255, 0 }
#define INCOMING_PORT_READCMDS  TARGET_PORT
#define INCOMING_PORT_WRITECMDS 10000

#define TARGET_IP               192, 168, 1, 19
#define TARGET_PORT             9999
#define TARGER_DEBUG_PORT       50000

// Delay before initialize ethernet on startup in seconds
#define ETH_INIT_DELAY          10

// Used serial port and direction change pin for RS-485 port
#if defined PRODINO_BOARD || defined PRODINO_MKR0
#define RS485_PORT              Serial1
#define RS485_DIRECTION_PIN     3
#else
#define RS485_PORT              Serial
#define RS485_DIRECTION_PIN     2
#endif

#define ACK_MODBUS40            true
#define ACK_SMS40               false
#define ACK_RMU40               false
#define SEND_ACK                true

// ######### INCLUDES #######################

#ifdef TRANSPORT_ETH_ENC28J60
#include <UIPEthernet.h>
#else
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#endif

#ifdef PRODINO_BOARD
#include "KmpDinoEthernet.h"
#include "KMPCommon.h"
#elif defined PRODINO_MKR0
#include "KMPProDinoMKRZero.h"
#include "KMPCommon.h"
#endif

#ifdef PRODINO_MKR0
// ### todo: Watchdog
// possible candidate:
// https://github.com/javos65/WDTZero
#else
#include <avr/wdt.h>
#endif

#include "NibeGw.h"

// ######### VARIABLES #######################

// The media access control (ethernet hardware) address for the shield
byte mac[] = BOARD_MAC;

//The IP address for the shield
byte ip[] = BOARD_IP;

//The IP address of the gateway
byte gw[] = GATEWAY_IP;

//The network mask
byte mask[] = NETWORK_MASK;

boolean ethernetInitialized = false;

// Target IP address and port where Nibe UDP packets are send
IPAddress targetIp(TARGET_IP);
EthernetUDP udp;
EthernetUDP udp4readCmnds;
EthernetUDP udp4writeCmnds;

NibeGw nibegw(&RS485_PORT, RS485_DIRECTION_PIN);

// ######### DEBUG #######################

#define DEBUG_BUFFER_SIZE       200

#ifdef ENABLE_DEBUG
#define DEBUG_PRINT(level, message) if (verbose >= level) { debugPrint(message); }
#define DEBUG_PRINTDATA(level, message, data) if (verbose >= level) { sprintf(debugBuf, message, data); debugPrint(debugBuf); }
#define DEBUG_PRINTARRAY(level, data, len) if (verbose >= level) { int i; for (i = 0; i < len; i++) { sprintf(debugBuf+i+i, "%02X", data[i]); } sprintf(debugBuf+i+i, "\n"); debugPrint(debugBuf); }
#else
#define DEBUG_PRINT(level, message)
#define DEBUG_PRINTDATA(level, message, data)
#define DEBUG_PRINTARRAY(level, data, len)
#endif

#ifdef ENABLE_DEBUG
char verbose = VERBOSE_LEVEL;
char debugBuf[DEBUG_BUFFER_SIZE];

void debugPrint(char* data)
{
#ifdef ENABLE_DEBUG_SER
  Serial.print(data);
#endif
#ifdef ENABLE_DEBUG_UDP
  if (ethernetInitialized)
  {
    udp.beginPacket(targetIp, TARGER_DEBUG_PORT);
    udp.write(data);
    udp.endPacket();
  }
#endif
}
#endif

// ######### SETUP #######################

void setup()
{
  // Start watchdog
#ifdef PRODINO_MKR0
  // ### todo: Watchdog
#else
  wdt_enable (WDTO_2S);
#endif

  nibegw.setCallback(nibeCallbackMsgReceived, nibeCallbackTokenReceived);
  nibegw.setAckModbus40Address(ACK_MODBUS40);
  nibegw.setAckSms40Address(ACK_SMS40);
  nibegw.setAckRmu40Address(ACK_RMU40);
  nibegw.setSendAcknowledge(SEND_ACK);

#ifdef ENABLE_NIBE_DEBUG
  nibegw.setDebugCallback(nibeDebugCallback);
  nibegw.setVerboseLevel(VERBOSE_LEVEL);
#endif

#ifdef PRODINO_BOARD
  DinoInit();
  Serial.begin(115200, SERIAL_8N1);
#elif defined PRODINO_MKR0
#ifdef ENABLE_DEBUG_SER
  while(!Serial) ;
#endif
  KMPProDinoMKRZero.init(ProDino_MKR_Zero_Ethernet);
#endif

  DEBUG_PRINTDATA(0, "%s ", BOARD_NAME);
  DEBUG_PRINTDATA(0, "version %s\n", VERSION);
  DEBUG_PRINT(0, "Started\n");
}

// ######### MAIN LOOP #######################

void loop()
{
#ifdef PRODINO_MKR0
  // ### todo: Watchdog
#else
  wdt_reset();
#endif

  long now = millis() / 1000;

  if (!nibegw.connected())
  {
    nibegw.connect();
  }
  else
  {
    do
    {
      nibegw.loop();
#ifdef TRANSPORT_ETH_ENC28J60
      Ethernet.maintain();
#endif
    } while (nibegw.messageStillOnProgress());
  }

  if (!ethernetInitialized)
  {
    if (now >= ETH_INIT_DELAY)
    {
      DEBUG_PRINT(1, "Initializing Ethernet\n");
      initializeEthernet();
#ifdef ENABLE_DEBUG
      DEBUG_PRINTDATA(0, "%s ", BOARD_NAME);
      DEBUG_PRINTDATA(0, "version %s\n", VERSION);
      sprintf(debugBuf, "MAC=%02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      DEBUG_PRINT(0, debugBuf);
      sprintf(debugBuf, "IP=%d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
      DEBUG_PRINT(0, debugBuf);
      sprintf(debugBuf, "GW=%d.%d.%d.%d\n", gw[0], gw[1], gw[2], gw[3]);
      DEBUG_PRINT(0, debugBuf);
      sprintf(debugBuf, "TARGET IP=%d.%d.%d.%d\n", TARGET_IP);
      DEBUG_PRINTDATA(0, "TARGET PORT=%d\n", BOARD_NAME);
      DEBUG_PRINTDATA(0, "ACK_MODBUS40=%s\n", ACK_MODBUS40 ? "true" : "false");
      DEBUG_PRINTDATA(0, "ACK_SMS40=%s\n", ACK_SMS40 ? "true" : "false");
      DEBUG_PRINTDATA(0, "ACK_RMU40=%s\n", ACK_RMU40 ? "true" : "false");
      DEBUG_PRINTDATA(0, "SEND_ACK=%s\n", SEND_ACK ? "true" : "false");
      DEBUG_PRINTDATA(0, "ETH_INIT_DELAY=%d\n", ETH_INIT_DELAY);
      DEBUG_PRINTDATA(0, "RS485_DIRECTION_PIN=%d\n", RS485_DIRECTION_PIN);
#endif
    }
  }
}

// ######### FUNCTIONS #######################

void initializeEthernet()
{
  Ethernet.begin(mac, ip, gw, mask);
  Ethernet.setRetransmissionCount(2);
  Ethernet.setRetransmissionTimeout(50);
  if(Ethernet.linkStatus() == LinkON) {
    ethernetInitialized = true;
    udp.begin(0);
    udp4readCmnds.begin(INCOMING_PORT_READCMDS);
    udp4writeCmnds.begin(INCOMING_PORT_WRITECMDS);
  }

}

void nibeCallbackMsgReceived(const byte* const data, int len)
{
  if (ethernetInitialized)
  {
    sendUdpPacket(data, len);
  }
}


int nibeCallbackTokenReceived(eTokenType token, byte* data)
{
  int len = 0;
  if (ethernetInitialized)
  {
    if (token == READ_TOKEN)
    {
      DEBUG_PRINT(2, "Read token received\n");
      int packetSize = udp4readCmnds.parsePacket();
      if (packetSize) {
        len = udp4readCmnds.read(data, packetSize);
#ifdef TRANSPORT_ETH_ENC28J60
        udp4readCmnds.flush();
        udp4readCmnds.stop();
        udp4readCmnds.begin(INCOMING_PORT_READCMDS);
#endif
      }
    }
    else if (token == WRITE_TOKEN)
    {
      DEBUG_PRINT(2, "Write token received\n");
      int packetSize = udp4writeCmnds.parsePacket();
      if (packetSize) {
        len = udp4writeCmnds.read(data, packetSize);
#ifdef TRANSPORT_ETH_ENC28J60
        udp4writeCmnds.flush();
        udp4writeCmnds.stop();
        udp4writeCmnds.begin(INCOMING_PORT_WRITECMDS);
#endif
      }
    }
  }
  return len;
}

void nibeDebugCallback(byte verbose, char* data)
{
  DEBUG_PRINT(verbose, data);
}

void sendUdpPacket(const byte * const data, int len)
{
  DEBUG_PRINTDATA(2, "Sending UDP packet, len=%d\n", len);
  DEBUG_PRINTARRAY(2, data, len)

  udp.beginPacket(targetIp, TARGET_PORT);
  udp.write(data, len);
  udp.endPacket();
}
