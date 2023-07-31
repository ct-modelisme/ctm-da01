/**
 * CT-Modelisme (ct-modelisme.fr) - Decodeur Audio 01 (CTM-DA01.ino)
 * 
 * Requires NmraDcc library from mrrwa (https://github.com/mrrwa/NmraDcc)
 * Requires DFPlayerMini library from DFRobot (https://github.com/DFRobot/DFRobotDFPlayerMini)
 * 
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 * 
 * 
 * Cyrille TOULET <cyrille.toulet@linux.com>
 * Sat 21 Aug 12:38:54 CEST 2021
 */


// ----------------------------------------- Configuration -------------------------------------------- //

// Libraries
#include <NmraDcc.h>                                            // DCC library
#include "SoftwareSerial.h"                                     // Serial used by DFPlayer
#include "DFRobotDFPlayerMini.h"                                // DFPlayerMini audio driver


// Constants
#define BTN_MODE_A_PIN A0                                       // The pin connected to "MODE A" jumpers
#define BTN_MODE_B_PIN A1                                       // The pin connected to "MODE B" jumpers
#define BTN_MODE_C_PIN A2                                       // The pin connected to "MODE C" jumpers
#define VOL_TRIM_PIN   A7                                       // The pin connected to volume trim pot
#define DCC_SIGNAL_PIN 2                                        // The pin connected to opto-isolator
#define BTN_A1_PIN     3                                        // The pin connected to button "A1"
#define BTN_A2_PIN     4                                        // The pin connected to button "A2"
#define BTN_B1_PIN     5                                        // The pin connected to button "B1"
#define BTN_B2_PIN     6                                        // The pin connected to button "B2"
#define BTN_C1_PIN     7                                        // The pin connected to button "C1"
#define BTN_C2_PIN     8                                        // The pin connected to button "C2"
#define BTN_RAND_PIN   9                                        // The pin connected to button "RAND"
#define PLAYER_RX_PIN  10                                       // The pin connected to serial rx of DFPlayer
#define PLAYER_TX_PIN  11                                       // The pin connected to serial tx of DFPlayer

#define DECODER_VERSION 1                                       // The decoder version used by NmraDcc lib
#define FACTORY_DCC_ADDRESS 3                                   // The default DCC address for this decoder


// Structures
struct CV                                                       // The CV structure
{
  uint16_t addr;                                                // Address of CV
  uint8_t value;                                                // Value of CV
};


// Globals
SoftwareSerial playerSerial(PLAYER_RX_PIN, PLAYER_TX_PIN);      // The audio player serial
DFRobotDFPlayerMini player;                                     // The audio player instance
NmraDcc dcc;                                                    // The DCC instance

CV factoryCVs[] =                                               // The factory default CVs
{
  // The CV Below defines the Short DCC Address
  {CV_MULTIFUNCTION_PRIMARY_ADDRESS, FACTORY_DCC_ADDRESS},      // CV 1

  // These two CVs define the Long DCC Address
  {CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB, 192},                 // CV 17
  {CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB, FACTORY_DCC_ADDRESS}, // CV 18

  // Use long address 28/128 Speed Steps
  {CV_29_CONFIG, CV29_EXT_ADDRESSING|CV29_F0_LOCATION},         // CV 29
};

uint8_t factoryCVIndex = 0;                                     // The factory CV current index

int playedLoopingSong = 0;



// ------------------------------------- Common functions --------------------------------------------- //

/**
 * Software arduino reset
 */
void (*resetArduino) (void) = 0;



// -------------------------------------- Audio functions --------------------------------------------- //

/**
 * Compute DFPlayer volume from trim pot value
 * @return: The computed volume between 0 and 30
 */
int
getVolume()
{
  return min(analogRead(VOL_TRIM_PIN)/28, 840);
}


/**
 * Play a file stored in folder "01"
 * @param id: The ID of file to play
 */
void 
playSimpleSong(int id)
{
  Serial.print("Play simple sound ");
  Serial.println(id);

  player.disableLoop();
  player.stop();
  player.playFolder(1, id);

  delay(200);
}


/**
 * Play a file stored in folder "02" in loop mode
 * @param id: The ID of file to play
 */
void 
playLoopingSong(int id)
{
  player.stop();
  player.enableLoop();
  
  if(id == playedLoopingSong)
  {
    Serial.print("Stop looping sound ");
    Serial.println(id);
    
    playedLoopingSong = 0;
  }
  else
  {
    Serial.print("Play looping sound ");
    Serial.println(id);

    playedLoopingSong = id;
    player.playFolder(2, id);
  }

  delay(200);
}


/**
 * Play a file randomly choose in folder "03"
 */
void 
playRandomSong()
{
  int randomCount = player.readFileCountsInFolder(3);
  int id = random(1, randomCount + 1);

  Serial.print("Play random sound ");
  Serial.println(id);

  player.disableLoop();
  player.stop();
  player.playFolder(3, id);

  delay(200);
}



// ---------------------------------------- DCC functions --------------------------------------------- //

/**
 * Reset CVs to factory defaults
 */
void
notifyCVResetFactoryDefault()
{
  Serial.println("Factory reset requested");
  
  // Make factoryCVIndex non-zero and equal to num CV's to be reset 
  // to flag to the loop() function that a reset to Factory Defaults needs to be done
  factoryCVIndex = sizeof(factoryCVs) / sizeof(CV);
};


/**
 * Apply factory reset if requested by notifyCVResetFactoryDefault (CV 7 and 8 to "255")
 */
void
applyFactoryReset()
{
  if(factoryCVIndex && dcc.isSetCVReady())
  {
    // Decrement first as initially it is the size of the array 
    factoryCVIndex--; 
    
    dcc.setCV(
      factoryCVs[factoryCVIndex].addr, 
      factoryCVs[factoryCVIndex].value
    );

    if(factoryCVIndex == 0)
    {
      Serial.println("All CVs reset at factory defaults. Reboot in progress...");
      delay(200);
      resetArduino();
    }
  }
}


/**
 * Notify CV ack
 */
void
notifyCVAck()
{
  digitalWrite(LED_BUILTIN, HIGH);
  delay(8);
  digitalWrite(LED_BUILTIN, LOW);
}


/**
 * Notify a change to a CV
 */
void
notifyCVChange(uint16_t cv, uint8_t value)
{
  Serial.print("New value for CV ");
  Serial.print(cv);
  Serial.print(": ");
  Serial.println(value);

  if(dcc.isSetCVReady())
  {
    dcc.setCV(cv, value);

    Serial.print("CV ");
    Serial.print(cv);
    Serial.print(" changed to ");
    Serial.println(value);
  }
}


/**
 * Notify DCC functions (F0 to F28) according to following map:
 *  - F0 = Stop played sounds
 *  
 *  - F1-F10 = Play simple sounds 1 to 10
 *  - F11-F20 = Play loop sounds 1 to 10
 *  - F21 = Play random sound
 *  
 *  - F27 = Turn down the volume
 *  - F28 = Mute (on/off)
 *  
 * @param addr: The DCC address
 * @param type: The DCC address type (short/long)
 * @param group: The functions group
 * @param state: The functions state
 */
void 
notifyDccFunc(uint16_t addr, DCC_ADDR_TYPE type, FN_GROUP group, uint8_t state)
{
  if(addr != ((type == DCC_ADDR_SHORT)? 
    dcc.getCV(CV_MULTIFUNCTION_PRIMARY_ADDRESS): 
    ((dcc.getCV(CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB) - 192) << 8) + dcc.getCV(CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB)
  ))
    return;


  switch(group)
  {
    case FN_0_4:
      // Only process F0 in this packet if we're not in Speed Step 14 Mode
      if(dcc.getCV(CV_29_CONFIG) & CV29_F0_LOCATION)
        if(state & FN_BIT_00)
        {
          player.disableLoop();
          player.stop();
        }

      if(state & FN_BIT_01)
        playSimpleSong(1);
      if(state & FN_BIT_02)
        playSimpleSong(2);
      if(state & FN_BIT_03)
        playSimpleSong(3);
      if(state & FN_BIT_04)
        playSimpleSong(4);
      break;
    
    case FN_5_8:
      if(state & FN_BIT_05)
        playSimpleSong(5);
      if(state & FN_BIT_06)
        playSimpleSong(6);
      if(state & FN_BIT_07)
        playSimpleSong(7);
      if(state & FN_BIT_08)
        playSimpleSong(8);
      break;
    
    case FN_9_12:
      if(state & FN_BIT_09)
        playSimpleSong(9);
      if(state & FN_BIT_10)
        playSimpleSong(10);
        
      if(state & FN_BIT_11)
        playLoopingSong(1);
      if(state & FN_BIT_12)
        playLoopingSong(2);
      break;

    case FN_13_20:
      if(state & FN_BIT_13)
        playLoopingSong(3);
      if(state & FN_BIT_14)
        playLoopingSong(4);
      if(state & FN_BIT_15)
        playLoopingSong(5);
      if(state & FN_BIT_16)
        playLoopingSong(6);
      if(state & FN_BIT_17)
        playLoopingSong(7);
      if(state & FN_BIT_18)
        playLoopingSong(8);
      if(state & FN_BIT_19)
        playLoopingSong(9);
      if(state & FN_BIT_20)
        playLoopingSong(10);
      break;
  
    case FN_21_28:
      if(state & FN_BIT_21)
      {
        playRandomSong();
      }
      if(state & FN_BIT_28)
      {
        player.volume(0);
        Serial.println("Muted");
      }
      else if(state & FN_BIT_27)
      {
        player.volume((int) getVolume() / 1.5);
        Serial.println("Lower volume");
      }
      else
        player.volume(getVolume());
      break;  
  }
}



// -------------------------------------- Buttons functions ------------------------------------------- //


/**
 * 
 */
void
processButtons()
{
  int modeA = digitalRead(BTN_MODE_A_PIN);
  int modeB = digitalRead(BTN_MODE_B_PIN);
  int modeC = digitalRead(BTN_MODE_C_PIN);

  if(!digitalRead(BTN_RAND_PIN))
  {
    playRandomSong();
  }
  else if(!digitalRead(BTN_A1_PIN))
  {
    if(modeA)
      playSimpleSong(1);
    else
      playLoopingSong(1);

    // Software anti-rebound
    do delay(200); while(!digitalRead(BTN_A1_PIN));
  }
  else if(!digitalRead(BTN_A2_PIN))
  {
    if(modeA)
      playSimpleSong(2);
    else
      playLoopingSong(2);

    // Software anti-rebound
    do delay(200); while(!digitalRead(BTN_A2_PIN));
  }
  else if(!digitalRead(BTN_B1_PIN))
  {
    if(modeA && modeB)
      playSimpleSong(3);
    else if(modeA && !modeB)
      playLoopingSong(1);
    else if(!modeA && modeB)
      playSimpleSong(1);
    else if(!modeA && !modeB)
      playLoopingSong(3);

    // Software anti-rebound
    do delay(200); while(!digitalRead(BTN_B1_PIN));
  }
  else if(!digitalRead(BTN_B2_PIN))
  {
    if(modeA && modeB)
      playSimpleSong(4);
    else if(modeA && !modeB)
      playLoopingSong(2);
    else if(!modeA && modeB)
      playSimpleSong(2);
    else if(!modeA && !modeB)
      playLoopingSong(4);

    // Software anti-rebound
    do delay(200); while(!digitalRead(BTN_B2_PIN));
  }
  else if(!digitalRead(BTN_C1_PIN))
  {
    if(modeA)
    {
      if(modeB && modeC)
        playSimpleSong(5);
      else if(modeB && !modeC)
        playLoopingSong(1);
      else if(!modeB && modeC)
        playSimpleSong(3);
      else if(!modeB && !modeC)
        playLoopingSong(3);
    }
    else
    {
      if(modeB && modeC)
        playSimpleSong(3);
      else if(modeB && !modeC)
        playLoopingSong(3);
      else if(!modeB && modeC)
        playSimpleSong(1);
      else if(!modeB && !modeC)
        playLoopingSong(5);
    }

    // Software anti-rebound
    do delay(200); while(!digitalRead(BTN_C1_PIN));
  }
  else if(!digitalRead(BTN_C2_PIN))
  {
    if(modeA)
    {
      if(modeB && modeC)
        playSimpleSong(6);
      else if(modeB && !modeC)
        playLoopingSong(2);
      else if(!modeB && modeC)
        playSimpleSong(4);
      else if(!modeB && !modeC)
        playLoopingSong(4);
    }
    else
    {
      if(modeB && modeC)
        playSimpleSong(4);
      else if(modeB && !modeC)
        playLoopingSong(4);
      else if(!modeB && modeC)
        playSimpleSong(2);
      else if(!modeB && !modeC)
        playLoopingSong(6);
    }

    // Software anti-rebound
    do delay(200); while(!digitalRead(BTN_C2_PIN));
  }
}


// ---------------------------------------- Main programm --------------------------------------------- //

/** 
 * Arduino setup
 */
void setup() {  
  Serial.begin(115200);
  playerSerial.begin(9600);
  
  Serial.println("\n\n------");
  Serial.println("Initializing microcontroller...");
  Serial.println(" - Serial communication initialized");

  pinMode(BTN_MODE_A_PIN, INPUT_PULLUP);
  pinMode(BTN_MODE_B_PIN, INPUT_PULLUP);
  pinMode(BTN_MODE_C_PIN, INPUT_PULLUP);
  pinMode(BTN_A1_PIN, INPUT_PULLUP);
  pinMode(BTN_A2_PIN, INPUT_PULLUP);
  pinMode(BTN_B1_PIN, INPUT_PULLUP);
  pinMode(BTN_B2_PIN, INPUT_PULLUP);
  pinMode(BTN_C1_PIN, INPUT_PULLUP);
  pinMode(BTN_C2_PIN, INPUT_PULLUP);
  pinMode(BTN_RAND_PIN, INPUT_PULLUP);

  Serial.println(" - Inputs/Outputs initialized");
  Serial.println("\nInitializing audio driver...");
  
  if(!player.begin(playerSerial))
  {
    Serial.println(" - Error: unable to initialize driver. Is SD card inserted?\n");
    while(true);
  }

  Serial.println(" - MP3-TF-16P driver loaded");

  player.setTimeOut(500);
  player.EQ(DFPLAYER_EQ_NORMAL);
  player.volume(getVolume());

  Serial.println(" - MP3-TF-16P driver configured");
  Serial.println("\nInitializing DCC decoder...");

  dcc.pin(0, DCC_SIGNAL_PIN, 0);
  dcc.init(MAN_ID_DIY, DECODER_VERSION, FLAGS_AUTO_FACTORY_DEFAULT, 0);

  Serial.print(" - Short DCC address: ");
  Serial.println(dcc.getCV(CV_MULTIFUNCTION_PRIMARY_ADDRESS));
  Serial.print(" - Long DCC address: ");
  Serial.println(((dcc.getCV(CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB) - 192) << 8) + dcc.getCV(CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB));

  Serial.println("\nInitializing completed!");
  Serial.println("CT-Modelisme - Decodeur Audio 01");
  Serial.println("Firmware v1.1");
  Serial.println("Please visit https://ct-modelisme.fr/");
  Serial.println("------\n");
}



/** 
 * Arduino main loop
 */
void loop() {
  // Process DCC packets
  dcc.process();

  // Call factory reset subroutine
  applyFactoryReset();

  // Buttons
  processButtons();

  // Parse audio player messages
  if(player.available())
  {
    switch(player.readType())
    {
      case TimeOut:
        Serial.println("Time out!");
        break;
      case WrongStack:
        Serial.println("Stack wrong!");
        break;
      case DFPlayerCardInserted:
        Serial.println("Card inserted!");
        break;
      case DFPlayerCardRemoved:
        Serial.println("Card removed!");
        break;
      case DFPlayerCardOnline:
        Serial.println("Card online!");
        break;
      case DFPlayerError:
        Serial.print("DFPlayer error: ");
        switch (player.read()) {
          case Busy:
            Serial.println(F("card not found"));
            break;
          case FileMismatch:
            Serial.println(F("cannot find file"));
            break;
          default:
            break;
        }
        break;
      default:
        break;
    }
  }
}
