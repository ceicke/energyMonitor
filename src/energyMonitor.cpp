/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#line 1 "/Users/ceicke/Development/particle/energyMonitor/energyMonitor/src/energyMonitor.ino"
#include "Particle.h"

void setup();
void loop();
void findStartSequence();
void findStopSequence();
void findPowerSequence();
void findConsumptionSequence();
void publishMessage();
#line 3 "/Users/ceicke/Development/particle/energyMonitor/energyMonitor/src/energyMonitor.ino"
const int UART_RX_PIN = RX;
const int UART_TX_PIN = TX;
const unsigned long BAUD_RATE = 9600;

SerialLogHandler logHandler;

byte inByte; //byte to store the serial buffer
byte smlMessage[1000]; //byte to store the parsed message 
const byte startSequence[] = { 0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01 }; //start sequence of SML protocol
const byte stopSequence[]  = { 0x1B, 0x1B, 0x1B, 0x1B, 0x1A }; //end sequence of SML protocol
const byte powerSequence[] = { 0x07, 0x01, 0x00, 0x10, 0x07, 0x00, 0xFF, 0x01, 0x01, 0x62, 0x1B, 0x52, 0x00, 0x55 }; //sequence preceeding the current "Wirkleistung" value (4 Bytes)
const byte consumptionSequence[] = { 0x07, 0x01, 0x00, 0x01, 0x08, 0x00, 0xFF, 0x65, 0x00, 0x00, 0x01, 0x82, 0x01, 0x62, 0x1E, 0x52, 0xFF, 0x59 }; //sequence predeecing the current "Gesamtverbrauch" value (8 Bytes)
int smlIndex; //index counter within smlMessage array
int startIndex; //start index for start sequence search
int stopIndex; //start index for stop sequence search
int stage; //index to maneuver through cases
byte powerArray[4]; //array that holds the extracted 4 byte "Wirkleistung" value
byte consumption[8]; //array that holds the extracted 8 byte "Gesamtverbrauch" value
int currentpower; //variable to hold translated "Wirkleistung" value
int currentconsumption; //variable to hold translated "Gesamtverbrauch" value
float currentconsumptionkWh; //variable to calulate actual "Gesamtverbrauch" in kWh

// const byte startByte = 0xAA;
// const byte endByte = 0xFF;

// enum State {
//   WAIT_START,
//   WAIT_END
//   READ_DATA,
//   FIND_POWER_SEQUENCE,
//   PUBLISH
// };

// State currentState = WAIT_START;
// byte dataBuffer[2];
// byte dataCounter = 0;

void setup() {
  stage = 0;
  startIndex = 0;
  stopIndex = 0;
  Particle.variable("currentpower", currentpower);
  Particle.variable("totalconsumption", currentconsumption);
  Serial1.begin(BAUD_RATE);
  Log.info("setup complete");
}

// Example binary protocol:
// Start byte: 0xAA
// Payload: 2 bytes (big-endian integer)
// End byte: 0xFF

// void loop() {
//   if (Serial1.available()) {
//     byte incomingByte = Serial1.read();

//     switch (currentState) {
//       case WAIT_START:
//         if (incomingByte == startByte) {
//           currentState = READ_DATA;
//         }
//         break;

//       case READ_DATA:
//         dataBuffer[dataCounter++] = incomingByte;
//         if (dataCounter == 2) {
//           currentState = WAIT_END;
//         }
//         break;

//       case WAIT_END:
//         if (incomingByte == endByte) {
//           // Decode and process the data
//           int payload = (dataBuffer[0] << 8) | dataBuffer[1];
//           Particle.publish("uart_data", String(payload));
//         }
//         // Reset the state machine
//         dataCounter = 0;
//         currentState = WAIT_START;
//         break;
//     }
//   }
// }

//Credit - This codes is based on the work of user "rollercontainer" at https://www.photovoltaikforum.com/volkszaehler-org-f131/sml-protokoll-hilfe-gesucht-sml-gt-esp8266-gt-mqtt-t112216-s10.html
//For SML protocol see http://www.schatenseite.de/2016/05/30/smart-message-language-stromzahler-auslesen/
//Hardware used: https://www.msxfaq.de/sonst/bastelbude/smartmeter_d0_sml.htm

void loop() {
  // debug to just see if there is something on the wire
  // if(true) {
  //   stage = 5;
  //   if(Serial1.available() > 0) {
  //     inByte = Serial1.read();
  //     Log.info("%02X", inByte);
  //   }
  // }
  switch (stage) {
    case 0:
      findStartSequence(); // look for start sequence
      break;
    case 1:
      findStopSequence(); // look for stop sequence
      break;
    case 2:
      findPowerSequence(); //look for power sequence and extract
      break;
    case 3:
      findConsumptionSequence(); //look for consumption sequence and exctract
      break;
    case 4:
      publishMessage(); // do something with the result
      break;
  }
}


void findStartSequence() {
  if (Serial1.available()) {
    inByte = Serial1.read();
    if (inByte == startSequence[startIndex]) {
       // set smlMessage element at position 0,1,2 to inByte value
      smlMessage[startIndex] = inByte;
      startIndex =  (startIndex +1) %1000;
       // all start sequence values have been identified
      if (startIndex == sizeof(startSequence)) {
        stage = 1; // go to next case
        smlIndex = startIndex; // set start index to last position to avoid rerunning the first numbers in end sequence search
        startIndex = 0;
      }
    } else {
      startIndex = 0;
    }
  }
}



void findStopSequence() {
  if (Serial1.available())
  {
    inByte = Serial1.read();
    smlMessage[smlIndex] = inByte;
    smlIndex++;

    if (inByte == stopSequence[stopIndex])
    {
      stopIndex++;
      if (stopIndex == sizeof(stopSequence))
      {
        stage = 2;
        stopIndex = 0;
      }
    }
    else {
      stopIndex = 0;
    }
  }
}

void findPowerSequence() {
  byte temp; //temp variable to store loop search data
 startIndex = 0; //start at position 0 of exctracted SML message
 
  for(int x = 0; x < sizeof(smlMessage); x++){ //for as long there are element in the exctracted SML message 
    temp = smlMessage[x]; //set temp variable to 0,1,2 element in extracted SML message
    if (temp == powerSequence[startIndex]) //compare with power sequence
    {
      startIndex++;
      if (startIndex == sizeof(powerSequence)) //in complete sequence is found
      {
        for(int y = 0; y< 4; y++){ //read the next 4 bytes (the actual power value)
          powerArray[y] = smlMessage[x+y+1]; //store into power array
        }
        stage = 3; // go to stage 3
        startIndex = 0;
      }
    }
    else {
      startIndex = 0;
    }
  }
   currentpower = (powerArray[0] << 24 | powerArray[1] << 16 | powerArray[2] << 8 | powerArray[3]); //merge 4 bytes into single variable to calculate power value
}


void findConsumptionSequence() {
  byte temp;
 
  startIndex = 0;
  for(unsigned int x = 0; x < sizeof(smlMessage); x++){
    temp = smlMessage[x];
    if (temp == consumptionSequence[startIndex])
    {
      startIndex++;
      if (startIndex == sizeof(consumptionSequence))
      {
        for(int y = 0; y< 8; y++){
          //hier muss für die folgenden 8 Bytes hoch gezählt werden
          consumption[y] = smlMessage[x+y+1];
        }
        stage = 4;
        startIndex = 0;
      }
    }
    else {
      startIndex = 0;
    }
  }

   currentconsumption = (consumption[0] << 56 | consumption[1] << 48 | consumption[2] << 40 | consumption[32] | consumption[4] << 24 | consumption[5] << 16 | consumption[6] << 8 | consumption[7]); //combine and turn 8 bytes into one variable
   currentconsumptionkWh = currentconsumption/10000; // 10.000 impulses per kWh
}


void publishMessage() {
  Particle.publish("currentpower", String(currentpower));
  Particle.publish("totalconsumption", String(currentconsumptionkWh));
  
  Log.info("currentpower: %d", currentpower);
  Log.info("totalconsumption: %d", currentconsumptionkWh);

  // clear the buffers
  memset(smlMessage, 0, sizeof(smlMessage));
  memset(powerArray, 0, sizeof(powerArray));
  memset(consumption, 0, sizeof(consumption));
  // reset case
  smlIndex = 0;
  stage = 0; // start over
}