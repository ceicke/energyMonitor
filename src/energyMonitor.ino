#include "Particle.h"

const int UART_RX_PIN = RX;
const int UART_TX_PIN = TX;
const unsigned long BAUD_RATE = 9600;
const int DELAY = 5000;

SerialLogHandler logHandler;

// byte to store the serial buffer
byte inByte;
// byte to store the parsed message
byte smlMessage[1000];
// start sequence of SML protocol
const byte startSequence[] = { 0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01 };
// end sequence of SML protocol
const byte stopSequence[]  = { 0x1B, 0x1B, 0x1B, 0x1B, 0x1A };
// sequence preceeding the current "Wirkleistung" value (4 Bytes)
const byte powerSequence[] = { 0x07, 0x01, 0x00, 0x10, 0x07, 0x00, 0xFF, 0x01, 0x01, 0x62, 0x1B, 0x52, 0x00, 0x55 };
// sequence predeecing the current "Gesamtverbrauch" value (8 Bytes)
const byte consumptionSequence[] = { 0x07, 0x01, 0x00, 0x01, 0x08, 0x00, 0xFF, 0x65, 0x00, 0x00, 0x01, 0x82, 0x01, 0x62, 0x1E, 0x52, 0xFF, 0x59 };
// index counter within smlMessage array
int smlIndex;
// start index for start sequence search
int startIndex;
// start index for stop sequence search
int stopIndex;
// index to maneuver through cases
int currentState;
// array that holds the extracted 4 byte "Wirkleistung" value
byte powerArray[4];
// array that holds the extracted 8 byte "Gesamtverbrauch" value
byte consumption[8];
// variable to hold translated "Wirkleistung" value
int currentpower;
// variable to hold translated "Gesamtverbrauch" value
int currentconsumption;
// variable to calulate actual "Gesamtverbrauch" in kWh
int currentconsumptionkWh;

// introduce a watchdog timer that resets the device after 30s
void setupWatchdog() {
  Watchdog.init(WatchdogConfiguration().timeout(30s));
  Watchdog.onExpired([]() {
    System.reset();
  });
  Watchdog.start();
}

void setup() {
  // initial states
  currentState = 0;
  startIndex = 0;
  stopIndex = 0;

  // define our cloud variables
  Particle.variable("currentpower", currentpower);
  // Particle.variable("totalconsumption", currentconsumption);

  // setup watchdog
  setupWatchdog();
  
  // start listening on the wire
  Serial1.begin(BAUD_RATE);

  // cloud reset function
  Particle.function("cloudReset", cloudReset);
}

// the cloud reset function that simply resets the device
int cloudReset(String command) {
  System.reset(RESET_NO_WAIT);  
  return 0;
}

// for SML protocol see http://www.schatenseite.de/2016/05/30/smart-message-language-stromzahler-auslesen/

void loop() {
    
  switch(currentState) {
    case 0:
      // look for start sequence
      findStartSequence();
      break;
    case 1:
      // look for stop sequence
      findStopSequence();
      break;
    case 2:
      // find and extract power sequence
      findPowerSequence();
      break;
    case 3:
      // find and extract consumption sequence
      findConsumptionSequence();
      break;
    case 4:
      // publish our findings to the cloud
      publishMessage();
      // tell watchdog we are alive
      Watchdog.refresh();
      // getting the data only every 5 seconds or so is good enough for us
      delay(DELAY);
      break;
  }
}

void serialDebug() {
  // debug to just see if there is something on the wire
  currentState = 5;
  if(Serial1.available() > 0) {
    inByte = Serial1.read();
    Log.info("%02X", inByte);
  }
}

void findStartSequence() {
  if (Serial1.available()) {
    inByte = Serial1.read();
    if(inByte == startSequence[startIndex]) {
      // set smlMessage element at position 0,1,2 to inByte value
      smlMessage[startIndex] = inByte;
      startIndex = (startIndex + 1) % 1000;
      // all start sequence values have been identified
      if(startIndex == sizeof(startSequence)) {
        // go to next case
        currentState = 1;
        // set start index to last position to avoid rerunning the first numbers in end sequence search
        smlIndex = startIndex;
        startIndex = 0;
      }
    } else {
      startIndex = 0;
    }
  }
}



void findStopSequence() {
  if (Serial1.available()) {
    inByte = Serial1.read();
    smlMessage[smlIndex] = inByte;
    smlIndex++;

    if(inByte == stopSequence[stopIndex]) {
      stopIndex++;
      if(stopIndex == sizeof(stopSequence)) {
        currentState = 2;
        stopIndex = 0;
      }
    } else {
      stopIndex = 0;
    }
  }
}

void findPowerSequence() {
  // temp variable to store loop search data
  byte temp;
  // start at position 0 of exctracted SML message
  startIndex = 0;
  
  // for as long there are element in the exctracted SML message
  for(unsigned int x = 0; x < sizeof(smlMessage); x++) { 
    // set temp variable to 0,1,2 element in extracted SML message
    temp = smlMessage[x];
    // compare with power sequence
    if(temp == powerSequence[startIndex]) {
      startIndex = (startIndex + 1) % 1000;
       // if complete sequence is found
      if(startIndex == sizeof(powerSequence)) {
        // read the next 4 bytes (the actual power value)
        for(int y = 0; y < 4; y++) {
          // store into power array
          powerArray[y] = smlMessage[x+y+1];
        }
        // go to next state
        currentState = 4;
        startIndex = 0;
      }
    } else {
      startIndex = 0;
    }
  }
  // merge 4 bytes into single variable to calculate power value
  currentpower = (powerArray[0] << 24 | powerArray[1] << 16 | powerArray[2] << 8 | powerArray[3]); 
}


void findConsumptionSequence() {
  byte temp;
 
  startIndex = 0;
  for(unsigned int x = 0; x < sizeof(smlMessage); x++) {
    temp = smlMessage[x];
    if(temp == consumptionSequence[startIndex]) {
      startIndex = (startIndex + 1) % 1000;
      if(startIndex == sizeof(consumptionSequence)) {
        for(int y = 0; y < 8; y++) {
          consumption[y] = smlMessage[x+y+1];
        }
        currentState = 4;
        startIndex = 0;
      }
    } else {
      startIndex = 0;
    }
  }

  // combine and turn 8 bytes into one variable
  currentconsumption = (consumption[0] << 56 | consumption[1] << 48 | consumption[2] << 40 | consumption[32] | consumption[4] << 24 | consumption[5] << 16 | consumption[6] << 8 | consumption[7]);
  // 10.000 impulses per kWh
  currentconsumptionkWh = currentconsumption / 10000;
}


void publishMessage() {
  Particle.publish("currentpower", String(currentpower));
  // Particle.publish("totalconsumption", String(currentconsumptionkWh));
  
  Log.info("currentpower: %d", currentpower);
  //Log.info("totalconsumption: %d", currentconsumptionkWh);

  // clear the buffers
  memset(smlMessage, 0, sizeof(smlMessage));
  memset(powerArray, 0, sizeof(powerArray));
  memset(consumption, 0, sizeof(consumption));

  smlIndex = 0;
  // start over
  currentState = 0;
}