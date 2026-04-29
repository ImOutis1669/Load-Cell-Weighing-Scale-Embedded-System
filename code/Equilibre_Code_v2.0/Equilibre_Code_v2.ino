#include <HX711.h>
#include <Arduino.h>
#include <rgb_lcd.h>

/*
    Project Name: L'Equilibre
    Author: John Pritchard
    Date: 24/03/2025
    Version: 1.9
    
    Description: 
      This code allows the user to weigh themselves and other items. This is done by taking the data from the load cells
      and converting it into the user's desired unit measurement (kilograms, grams, pounds, stone).
      The user has two buttons - one that can Tare the scale (reset reading to 0) and one that can 
      change the measuremnt units. 
      
      2.0 (27/03/2025): Hard coded the weight of the plate out. Not ideal, however it is functional.
      1.9 (24/03/2025): Added Check Battery Level method that displays a warning when value drops below a certain threshold. Added red backgrounds for the warnings. General organisation improvements of the code.
      1.8 (22/03/2025): Added Channel Change - allowing all 4 load cells to be read by alternating between Channel A to Channel B. Changed LCD Screen, so different library used.
      1.7 (21/03/2025): Added debounce on button interrupts to prevent false triggering. Fixed the issue where units button would sometimes trigger both Unit and Tare ISR.
      1.6 (29/02/2025):	Added overload warning when the measured value exceeds 190lbs.
      1.5 (29/02/2025):	Improved formatting of scale. Fixed bug where the "Tare Applied" message wouldn't show if used in an ISR. Used a flag variable instead. 
      1.4 (28/02/2025): Got interrupts working. Problem caused by lack of libraries. Need to work on formatting of LCD.
      1.3 (20/02/2025): Implemented grams measurement to allow users to use the scales in more scenarios - such as weighing food (as per the brief).
      1.2 (15/02/2025): Implemented start screen.
      1.1 (14/02/2025): Added and refined main functionality of scale and buttons. The buttons could be further enhanced using interrupts.
      
    Hardware:
      - ATmega328 Arduino Uno Board
      - 2 Push Buttons (Pin 2 and 3)
      - 2 HX711 - scale_1 (Pin 6 and Pin 7) and scale_2 (Pin 4 and Pin 5)
      - 4 Load Cells (2 for each HX711)
      
*/

rgb_lcd lcd;

//Define HX711 connections
const int LOADCELL_DOUT_PIN_1 = 5;
const int LOADCELL_SCK_PIN_1 = 4;
const int LOADCELL_DOUT_PIN_2 = 7;  
const int LOADCELL_SCK_PIN_2 = 6;   

//Button Pins 
const int TARE_BUTTON = 2;  //TARE Button
const int UNIT_BUTTON = 3;  //Unit Change Button

//Battery pin
const int VIN_PIN = A0;

//Create HX711 instances
HX711 scale_1;
HX711 scale_2;

//Variables to store initial weight
float initialWeight1 = 0;
float initialWeight2 = 0;

volatile int unitMode = 0;  //0 = kg, 1 = grams, 2 = lb, 3 = stone

volatile bool tareFlag = false;  //Flag to trigger tare function
bool batteryFlag = true;

//Calibration factors for each load cell - could be more accurate
const int calibrationFactor1A = 436.0181964;  
const int calibrationFactor2A = 340.3532245;  
const int calibrationFactor1B = 137.4632058;  
const int calibrationFactor2B = 296.5721167;  


// Conversion Constants
const float KG_TO_GRAMS = 1000;
const float KG_TO_LB = 2.20462;
const float KG_TO_STONE = 0.157473;

String unitStr = "";       //Stores the current unit
int decPlaces = 2;         //Decimal places for display (can be changed based on situation)

volatile unsigned long lastInterruptTime = 0;  
const unsigned long debounceDelay = 200; 

//Gain values for channels.
int channel_A = 64;
int channel_B = 32;

float division = 1;

//Colours
byte red[3] = {255, 0, 0};
byte white[3] = {255, 255, 255};

float threshold = 20.0;     //If battery percentage goes below this number, a warning is displayed (Can be changed)

void setup() {
  //Initialise LCD screen
  Serial.begin(57600);
  lcd.begin(16, 2);

  //Initialise buttons and attach interrupts
  pinMode(TARE_BUTTON, INPUT_PULLUP);
  pinMode(UNIT_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TARE_BUTTON), ISR_TareSet, FALLING);
  attachInterrupt(digitalPinToInterrupt(UNIT_BUTTON), ISR_Unit, FALLING);

  start_screen();
  initialize_HX711_Modules();

  channel_change(channel_A);                  //Calibrate and tare scales in Channel A
  scale_1.set_scale(calibrationFactor1A);
  scale_2.set_scale(calibrationFactor2B);
  scale_1.tare();
  scale_2.tare();

  channel_change(channel_B);                  //Calibrate and tare scales in Channel B
  scale_1.set_scale(calibrationFactor1B);
  scale_2.set_scale(calibrationFactor2B);
  scale_1.tare();
  scale_2.tare();

  initialWeight1 = 0;  
  initialWeight2 = 0;

  lcd.clear();
  delay(1000);
}


void loop() { 
  if (tareFlag) { 
    tareFunc();
    tareFlag = false;  //Reset flag
  }

  check_batteryLevel();    

  channel_change(channel_A);                //Get Channel A readings
  float channelA1 = measure_scale1();
  float channelA2 = measure_scale2();

  channel_change(channel_B);                //Get Channel B readings. Output scaled since Channel B has less gain
  float channelB1 = measure_scale1() * 2;   
  float channelB2 = measure_scale2() * 2;   

  float totalWeight = (channelA1 + channelA2 + channelB1 + channelB2);       //Total all readings from both HX711 and both Channels
 
  float displayWeight = totalWeight;                //Convert to selected unit based on unitMode value
  if (unitMode == 0) {
      unitStr = "kg";
      displayWeight = displayWeight - 15.2;         //Removes weight of plate in relevant unit
      division = 1;
  } else if (unitMode == 1) {
      displayWeight = (totalWeight * KG_TO_GRAMS) - (15.2 * KG_TO_GRAMS);
      unitStr = "g";
      division = KG_TO_GRAMS;
  } else if (unitMode == 2) {
      displayWeight = (totalWeight * KG_TO_LB) - (15.2 * KG_TO_LB);
      unitStr = "lb";
      division = KG_TO_LB;
  } else if (unitMode == 3) {
      displayWeight = (totalWeight * KG_TO_STONE) - (15.2 * KG_TO_STONE);
      unitStr = "st";
      division = KG_TO_STONE;
  }

  if (displayWeight/division > 250){			
    overloadWarning();		  		                //When weight in kg exceeds 190, warning is displayed
  } else {

    if (displayWeight < 0) displayWeight = 0;        //If displayWeight < 0, it becomes 0
    displayWeight = displayWeight / 2;
    lcd.clear();    
    lcd.setRGB(white[0], white[1], white[2]);
    lcd.setCursor(0, 0);
    lcd.print("Total: ");
    lcd.setCursor(0, 1);
    lcd.print(displayWeight, decPlaces);		    //Prints display weight to 2dp (Can be changed to ndp at the top)
    lcd.print(" " + unitStr + " ");
  } 
}


// Functions ----------------------------------

void start_screen() {
  delay(500);
  lcd.setCursor(2, 0);
  lcd.print("L'Equilibre");
  delay(1000);
  lcd.setCursor(7, 1);
  lcd.print(":)");
  delay(1500);
}

void initialize_HX711_Modules(){
  //Initialize HX711 A and B
  scale_1.begin(LOADCELL_DOUT_PIN_1, LOADCELL_SCK_PIN_1);
  scale_2.begin(LOADCELL_DOUT_PIN_2, LOADCELL_SCK_PIN_2);

  //Makes sure BOTH HX711 are connected.
  while (!(scale_1.is_ready() || scale_2.is_ready())) {
	  lcd.clear();
	  delay(500);
    lcd.print("~Connect HX711!~");
    delay(500);
  }
  //If both are connected, suitable message is given.
  lcd.clear();
  lcd.print("HX711 Ready!");
  delay(200);
}


void channel_change(int gain){   //Channel A = 64, Channel B = 32
  scale_1.set_gain(gain);
  scale_2.set_gain(gain);
}

float measure_scale1(){
  float returnValue = initialWeight1 + (scale_1.get_units(1) / 1000.0);
  if (returnValue < 0) returnValue = 0;               
  return returnValue;
}

float measure_scale2(){
  float returnValue = initialWeight2 + (scale_2.get_units(1) / 1000.0);
  if (returnValue < 0) returnValue = 0;                                     //Prevent negative values
  return returnValue;
}

void overloadWarning() {
  lcd.clear();    
  lcd.setRGB(red[0], red[1], red[2]);
  lcd.setCursor(0, 0);
  lcd.print("Overload Warning");
  lcd.setCursor(1, 1);
  lcd.print("Max: 190 lbs");
  delay(1000);

  //Reset Display
  lcd.clear();
  lcd.setRGB(white[0], white[1], white[2]);
}

void check_batteryLevel(){
  int VIN_reading = analogRead(VIN_PIN);    //Reads voltage put into A0. 
  
  float scaledVoltage = (VIN_reading / 1023.0) * 5;     //1023 is mapped to reference voltage (5v)
  float actualBatteryVoltage = scaledVoltage * 2;       //*2 due to voltage divider
  float percentage = (actualBatteryVoltage/4.5) * 100;

  if ((percentage < threshold) && (batteryFlag == true)){
    lcd.setRGB(red[0], red[1], red[2]);
    lcd.clear();    
    lcd.setCursor(0, 0);
    lcd.print("Battery Low!");
    delay(5000);              //5 second delay to make sure the user is notified
    batteryFlag = false;      //Makes sure warning only happens once

    //Reset Display
    lcd.clear();
    lcd.setRGB(white[0], white[1], white[2]);
  }

  if (percentage >= 20){
    batteryFlag = true;       //Resets the warning if battery is charged/replaced
  }
}

void tareFunc() {
  //Tare both scales, reset initialWeight
  channel_change(channel_A);
  scale_1.tare();
  scale_2.tare();

  channel_change(channel_B);
  scale_1.tare();
  scale_2.tare();

  initialWeight1 = 0;
  initialWeight2 = 0;

  //Display this to the user
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("Tare Applied");
  delay(2000);
  lcd.clear();
}

// Interrupt Service Routines (ISRs) ----------

void ISR_TareSet() {
  unsigned long currentTime = millis();                         //Returns how long in ms the Arduino has been running
  if ((currentTime - lastInterruptTime) >= debounceDelay) {     //Debounce button to prevent two triggers

    tareFlag = true;                       //Set tare flag
    lastInterruptTime = currentTime;       //Update the last interrupt time
  }
}

void ISR_Unit() {
  unsigned long currentTime = millis();  
  if ((currentTime - lastInterruptTime) >= debounceDelay) {    //Debounce button to prevent two triggers

    unitMode = (unitMode + 1) % 4;        //Cycle through 0-3 (inclusive) - each number represents a unit
    lastInterruptTime = currentTime;      //Update the last interrupt time
  }
}


/*
  ___     .__/\___________            .__.__  ._____.                    ___     
 / _ \_/\ |  )/\_   _____/ ________ __|__|  | |__\_ |_________   ____   / _ \_/\ 
 \/ \___/ |  |  |    __)_ / ____/  |  \  |  | |  || __ \_  __ \_/ __ \  \/ \___/ 
          |  |__|        < <_|  |  |  /  |  |_|  || \_\ \  | \/\  ___/           
          |____/_______  /\__   |____/|__|____/__||___  /__|    \___  >          
                       \/    |__|                     \/            \/           		*/