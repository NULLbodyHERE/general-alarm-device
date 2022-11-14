/* GPAD_API.ino
  The program implements the main API of the General Purpose Alarm Device.

  Copyright (C) 2022 Robert Read

  This program includes free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  See the GNU Affero General Public License for more details.
  You should have received a copy of the GNU Affero General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  Change notes:
  20221024 Creation by Rob.
  20221028 Report Program Name in splash screen. Lee Erickson
*/

/* This is a work-in-progress but it has two purposes.
 * It essentially implements two APIs: An "abstract" API that is
 * intended to be unchanging and possibly implemented on a large
 * variety of hardware devices. That is, as the GPAD hardware
 * changes and evolves, it does not invalidate the use of this API.
 *
 * Secondly, it offers a fully robotic API; that is, it gives
 * complete access to all of the hardware currently on the board.
 * For example, the current hardware, labeled Prototype #1, offers
 * a simple "tone" buzzer. The abstract interface uses this as part
 * of an abstract command like "set alarm level to PANIC".
 * The robotic control allows you to specify the actual tone to be played.
 * The robotic inteface obviously changes as the hardware changes.
 *
 * Both APIs are useful in different situations. The abstract interface
 * is good for a medical device manufacturer that expects the alarming
 * device to change and evolve. The Robotic API is good for testing the
 * actual hardware, and for a hobbyist that wants to use the device for
 * more than simple alarms, such as for implementing a game.
 *
 * It is our intention that the API will be available both through the
 * serial port and through an SPI interface. Again, these two modes
 * serve different purposes. The SPI interface is good for tight
 * intergration with a safey critical devices. The serial port approach
 * is easier for testing and for a hobbyist to develop an approach,
 * whether they eventually intend to use the SPI interface or not.
 * -- rlr, Oct. 24, 2022
 */

#include "alarm_api.h"
#include "robot_api.h"
#include "gpad_utility.h"
#include "gpad_serial.h"



// #define VERSION 0.02             //Version of this software
#define BAUDRATE 115200
// #define BAUDRATE 57600
// Let's have a 10 minute time out to allow people to compose strings by hand, but not
// to leave data our there forever
#define SERIAL_TIMEOUT_MS 600000


// here is the abstract "state" of the machine,
// completely independent of hardware.
// This is very simple version of what is probably needed.
// For example, perhaps the abstract machine should know how long
// it has been in a given alarm state. At present, this
// is a "dumb" machine---the user of it is expected to do
// all alarm management.
extern const char *AlarmNames[];
// TODO --- consider packing all this into a single struct,
// as all are used together
extern AlarmLevel currentLevel;
extern bool currentlyMuted;
extern char AlarmMessageBuffer[128];


//Set LED wink parameters
const int HIGH_TIME_LED_MS = 800;    //time in milliseconds
const int LOW_TIME_LED_MS = 200;
unsigned long lastLEDtime_ms = 0;
unsigned long nextLEDchangee_ms = 100; //time in ms.


//For I2C Scan
#include <Wire.h>
/* //For LCD */
/* #include <LiquidCrystal_I2C.h> */
/* LiquidCrystal_I2C lcd(0x38, 20, 4); // set the LCD address to 0x27 for a 20 chars and 4 line display in Wokwi, and 0x38 for the physical GPAD board */
// To Debounce our standard button
#include <DailyStruggleButton.h>
DailyStruggleButton muteButton;

extern int LIGHT[];
extern int NUM_LIGHTS;


// a goal here is to remove this dependence from this file...
#include <LiquidCrystal_I2C.h>
extern LiquidCrystal_I2C lcd;

// Functions

void updateWink(void) {
  //Wink the built in LED
  const unsigned long m = millis();
  if (((m - lastLEDtime_ms) > nextLEDchangee_ms) || (m < lastLEDtime_ms)) {
    if (digitalRead(LED_BUILTIN) == LOW) {
      digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
      nextLEDchangee_ms = HIGH_TIME_LED_MS;
    } else {
      digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
      nextLEDchangee_ms = LOW_TIME_LED_MS;
    }
    lastLEDtime_ms = m;
  }//end of Wink
}


void setup() {
  //Lets make the LED high near the start of setup for visual clue
  pinMode(LED_BUILTIN, OUTPUT);      // set the LED pin mode
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)

  //Serial setup
  delay(100);
  Serial.begin(BAUDRATE);
  delay(100);                         //Wait before sending the first data to terminal

  Serial.setTimeout(SERIAL_TIMEOUT_MS);

  Wire.begin();

  lcd.init();
  Serial.println("Clear LCD");
  clearLCD();
  delay(100);
  Serial.println("Start LCD splash");
  splashLCD();
  Serial.println("EndLCD splash");

  Serial.println("Set up GPIO pins");
  pinMode(SWITCH_MUTE, INPUT_PULLUP);
  for (int i = 0; i < NUM_LIGHTS; i++) {
    Serial.println(LIGHT[i]);
    pinMode(LIGHT[i], OUTPUT);
  }

  muteButton.set(SWITCH_MUTE, myCallback);
  Serial.println("end set up GPIO pins");

  printInstructions(Serial);
  AlarmMessageBuffer[0] = '\0';

  digitalWrite(LED_BUILTIN, LOW);   // turn the LED off at end of setup
}// end of setup()

/* void printAlarmState() { */
/*   Serial.print("Muted: "); */
/*   Serial.println(currentlyMuted ? "YES" : "NO"); */
/*   Serial.print("LVL: "); */
/*   Serial.println(currentLevel); */
/*   if (strlen(AlarmMessageBuffer) == 0) { */
/*     Serial.println("No Message."); */
/*   } else { */
/*     Serial.print("Msg: "); */
/*     Serial.println(AlarmMessageBuffer); */
/*   } */
/* } */



// This operation is idempotent if there is no change in the abstract state.
/* void annunciateAlarmLevel() { */
/*   for(int i = 0; i < currentLevel; i++) { */
/*     digitalWrite(LIGHT[i],HIGH); */
/*   } */
/*   for(int i = currentLevel; i < NUM_LIGHTS; i++) { */
/*     digitalWrite(LIGHT[i],LOW); */
/*   } */
/*   if (!currentlyMuted) { */
/*     tone(TONE_PIN, BUZZER_LVL_FREQ_HZ[currentLevel],INF_DURATION); */
/*   } else { */
/*     noTone(TONE_PIN); */
/*   } */
/*   showStatusLCD(currentLevel,currentlyMuted,AlarmMessageBuffer); */
/* } */

void myCallback(byte buttonEvent){
  switch (buttonEvent){
    case onPress:
      // Do something...
      Serial.println("onPress");
      currentlyMuted = !currentlyMuted;
      annunciateAlarmLevel();
      printAlarmState(Serial);
      break;
  }
}


void loop() {
  updateWink(); //The builtin LED

  muteButton.poll();

  processSerial(Serial);
}
