/*
 emonTx Shield 4 x CT + Voltage example

 An example sketch for the emontx Arduino shield module for
 CT and AC voltage sample electricity monitoring. Enables real power and Vrms calculations.

 Part of the openenergymonitor.org project
 Licence: GNU GPL V3

 Authors: Glyn Hudson, Trystan Lea
 Builds upon Ciseco SRFSPI library and Arduino

 emonTx documentation: 	http://openenergymonitor.org/emon/modules/emontxshield/
 emonTx firmware code explination: http://openenergymonitor.org/emon/modules/emontx/firmware
 emonTx calibration instructions: http://openenergymonitor.org/emon/modules/emontx/firmware/calibration

 THIS SKETCH REQUIRES:

 Libraries in the standard arduino libraries folder:
	- SRFSPI		https://github.com/CisecoPlc/SRFSPI
	- EmonLib		https://github.com/openenergymonitor/EmonLib.git

 Other files in project directory (should appear in the arduino tabs above)
	- emontx_lib.ino

*/

#include <OneWire.h>
#include <DallasTemperature.h>

#define FILTERSETTLETIME 10000                                          //  Time (ms) to allow the filters to settle before sending data

const int CT1 = 1;
const int CT2 = 0;                                                      // Set to 0 to disable
const int CT3 = 0;
const int CT4 = 0;

#include <SPI.h>
#include <SRFSPI.h>
#define READVCC_CALIBRATION_CONST 1097280L
#include "EmonLib.h"
EnergyMonitor ct1,ct2,ct3, ct4;                                              // Create  instances for each CT channel

typedef struct {
  int power1;
  int power2;
  int power3;
  int power4;
  float temp1;
  float temp2;
  int Vrms;
} PayloadTX;

PayloadTX emontx;
uint8_t PANID[2];

const int LEDpin = 9;                                                   // On-board emonTx LED 

#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress caseThermometer;
DeviceAddress remoteThermometer;

boolean settled = false;

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

void setup()
{
  uint8_t rxbuf[64];
  char msg[48];
  unsigned int i, retries = 10;
  Serial.begin(9600);
   //while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only

  Serial.println("emonTX Shield CT1234 with SRF Voltage example");
  Serial.println("OpenEnergyMonitor.org");

  if (CT1) ct1.current(1, 60.606);                                     // Setup emonTX CT channel (ADC input, calibration)
  if (CT2) ct2.current(2, 60.606);                                     // Calibration factor = CT ratio / burden resistance
  if (CT3) ct3.current(3, 60.606);                                     // emonTx Shield Calibration factor = (100A / 0.05A) / 33 Ohms
  if (CT4) ct4.current(4, 60.606);

  if (CT1) ct1.voltage(0, 259.4, 1.7);                                // ct.voltageTX(ADC input, calibration, phase_shift) - make sure to select correct calibration for AC-AC adapter  http://openenergymonitor.org/emon/modules/emontx/firmware/calibration. Default set for Ideal Power adapter
  if (CT2) ct2.voltage(0, 259.4, 1.7);                                // 268.97 for the UK adapter, 260 for the Euro and 130 for the US.
  if (CT3) ct3.voltage(0, 259.4, 1.7);
  if (CT4) ct4.voltage(0, 259.4, 1.7);

  SRF.init(10);
  PANID[0] = 'P';
  PANID[1] = 'M';

  pinMode(LEDpin, OUTPUT);                                              // Setup indicator LED
  digitalWrite(LEDpin, HIGH);
  while(retries--) {
    i = 0;
    delay(1100);
    SRF.write((uint8_t *)"+++", 3);
    delay(1000);
    while(!SRF.available() && i < 1000) {
      delay(10);
      i++;
    }
    i = 0;
    while(SRF.available()) {
      rxbuf[i % 64] = SRF.read();
      i++;
    }
    if ((i == 3) && rxbuf[0] == 'O' && rxbuf[1] == 'K') {
      //Serial.println("Got OK back successfully");
      SRF.write((uint8_t *)"ATMY\r", 5);
      i = 0;
      while(!SRF.available() && i < 1000) {
        delay(10);
        i++;
      }
      i = 0;
      while(SRF.available()) {
        rxbuf[i % 64] = SRF.read();
        i++;
        //Serial.write(rxbuf[i-1]);
        //if (rxbuf[i-1] == '\r') Serial.write('\n');
      }
      if ((i == 6) && rxbuf[3] == 'O' && rxbuf[4] == 'K') {
        snprintf(msg, 48, "ATMY returned OK and set PANID to '%c%c'", rxbuf[0], rxbuf[1]);
        Serial.println(msg);
        PANID[0] = rxbuf[0];
        PANID[1] = rxbuf[1];
        retries = 0;
      } else {
        snprintf(msg, 48, "Got back i = %d", i);
        Serial.println(msg);
      }
    } else {
      snprintf(msg, 48, "Got back i = %d and response '%c' '%c'", i, rxbuf[0], rxbuf[1]);
      Serial.println(msg);
    }
    SRF.write((uint8_t *)"ATDN\r", 5);
    delay(100);
    i = 0;
    while(SRF.available()) {
      rxbuf[i % 64] = SRF.read();
      i++;
    }
  }

  sensors.begin();
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");
  if (!sensors.getAddress(caseThermometer, 0)) Serial.println("Unable to find address for Device 0");
  Serial.print("Device 0 Address: ");
  printAddress(caseThermometer);
  Serial.println();
  // set the resolution to 11 bits (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(caseThermometer, 11);
  if (!sensors.getAddress(remoteThermometer, 1)) Serial.println("Unable to find address for Device 1");
  Serial.print("Device 1 Address: ");
  printAddress(remoteThermometer);
  Serial.println();
  // set the resolution to 11 bits (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(remoteThermometer, 11);

  //Serial.print("Device 0 Resolution: ");
  //Serial.print(sensors.getResolution(insideThermometer), DEC);
  //Serial.println();
}

void loop()
{
  if (CT1) {
    ct1.calcVI(20,2000);                                                  // Calculate all. No.of crossings, time-out
    emontx.power1 = ct1.realPower;
    Serial.print(emontx.power1);
  }

  emontx.Vrms = ct1.Vrms*100;                                            // AC Mains rms voltage

  if (CT2) {
    ct2.calcVI(20,2000);                                                  // Calculate all. No.of crossings, time-out
    emontx.power2 = ct2.realPower;
    Serial.print(" "); Serial.print(emontx.power2);
  }

  if (CT3) {
    ct3.calcVI(20,2000);                                                  // Calculate all. No.of crossings, time-out
    emontx.power3 = ct3.realPower;
    Serial.print(" "); Serial.print(emontx.power3);
  }

   if (CT4) {
     ct4.calcVI(20,2000);                                                  // Calculate all. No.of crossings, time-out
    emontx.power4 = ct4.realPower;
    Serial.print(" "); Serial.print(emontx.power4);
  }

  Serial.print(" "); Serial.print(ct1.Vrms);

  Serial.println(); delay(100);

  // because millis() returns to zero after 50 days !
  if (!settled && millis() > FILTERSETTLETIME) settled = true;

  if (settled)                                                            // send data only after filters have settled
  {
    send_rf_data();                                                       // *SEND RF DATA* - see emontx_lib
    sensors.requestTemperatures();
    delay(1000);
    emontx.temp1 = sensors.getTempC(caseThermometer);
    emontx.temp2 = sensors.getTempC(remoteThermometer);
    Serial.print("Temperature measured is ");
    Serial.println(emontx.temp1);
    send_temp_data();
    digitalWrite(LEDpin, HIGH); delay(2); digitalWrite(LEDpin, LOW);      // flash LED
    delay(1000);                                                          // delay between readings in ms
  }
}
