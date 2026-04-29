#include <Arduino.h>  
#include <HX711.h>
#include <LiquidCrystal.h>

/*
    Project Name: L'Equilibre
    Author: John Pritchard
    Date: 29/02/2025
    Version: 1.5
    
    Description: 
      This code allows the user to weigh themselves and other items. This is done by taking the data from the load cells
      and converting it into the user's desired unit measurement (kilograms, grams, pounds, stone).
      The user has two buttons - one that can Tare the scale (reset reading to 0) and one that can 
      change the measuremnt units.
      
      1.5 (29/02/2025):	Improved formatting of scale. Fixed bug where the "Tare Applied" message wouldn't show if used in an ISR. Used a flag variable instead. 
      1.4 (28/02/2025): Got interrupts working. Problem caused by lack of libraries. Need to work on formatting of LCD.
      1.3 (20/02/2025): Implemented grams measurement to allow users to use the scales in more scenarios - such as weighing food (as per the brief).
      1.2 (15/02/2025): Implemented start screen.
      1.1 (14/02/2025): Added and refined main functionality of scale and buttons. The buttons could be further enhanced using interrupts.
      
    Hardware:
      - ATmega328 Arduino Uno Board
      - 2 Push Buttons (Pin 2 and 3)
      - 2 HX711 - scale_A (Pin 12 and Pin 13) and scale_B (Pin 4 and Pin 5)
      - 4 Load Cells (2 for each HX711)
      
    
*/


// Initialize LCD (RS, E, D4, D5, D6, D7)
LiquidCrystal lcd(6, 7, 8, 9, 10, 11);

// Define HX711 connections for two modules
const int LOADCELL_DOUT_PIN_A = 12;  
const int LOADCELL_SCK_PIN_A = 13;   
const int LOADCELL_DOUT_PIN_B = 4;  
const int LOADCELL_SCK_PIN_B = 5;   

const int TARE_BUTTON = 2;  // Button to reset scale (TARE)
const int UNIT_BUTTON = 3;  // Button to switch units

// Create HX711 instances
HX711 scale_A;
HX711 scale_B;

// Variables to store initial weight
float initialWeightA = 0;
float initialWeightB = 0;
volatile int unitMode = 0; 		// 0 = kg, 1 = grams, 2 = lb, 3 = stone

const float calibrationFactorA = 1;  	// 1 for now until calibrated (raw weight value / real known weight)
const float calibrationFactorB = 1;  

// Conversion constants
const float KG_TO_GRAMS = 1000;		//1kg = 1000g 	(not really necessary icl as its an easy conversion but it just made sense to put here).
const float KG_TO_LB = 2.20462;  	// 1 kg = 2.20462 lb
const float KG_TO_STONE = 0.157473;  	// 1 kg = 0.157473 stone


String unitStr = "";		// string that stores the unit that the system is on.
int decPlaces;			// num of decimal places for measurement. Changes depending on units used.

boolean tareFlag = false; 	//governs when the tare function activates. Swiched true by ISR_TareSet.


void setup() {
  Serial.begin(57600);	//set Baud rate
  lcd.begin(16, 2);

  // Set up the buttons - NOTE: INPUT_PULLUP enables pullup resistor to ensure the read value from the buttons are either HIGH or LOW.
  pinMode(TARE_BUTTON, INPUT_PULLUP);
  pinMode(UNIT_BUTTON, INPUT_PULLUP);			
  attachInterrupt(digitalPinToInterrupt(TARE_BUTTON), ISR_TareSet, FALLING);
  attachInterrupt(digitalPinToInterrupt(UNIT_BUTTON), ISR_Unit, FALLING);
    
  // calls relevant methods for initalization.
  start_screen();
  initialize_HX711_Modules();

  // Apply calibration once known (NEED TO CALCULATE THIS DURING PROTOTYPING BY USING A KNOWN WEIGHT).
  scale_A.set_scale(calibrationFactorA);
  scale_B.set_scale(calibrationFactorB);

  // get initial weight of both scales and convert to kg.
  initialWeightA = scale_A.get_units(30) / 1000.0;
  initialWeightB = scale_B.get_units(30) / 1000.0;

  // Tare the scales to remove offset.
  scale_A.tare();
  scale_B.tare();

  delay(200);
  lcd.clear();
}


void loop() {

  if (tareFlag) {		// when the ISR sets the flag to true, the tare function is called. 
    tareFunc();
  }

  // Read weight from load 2 cells each. Converts to kg for ease since all conversion constants are from kg to something else.
  float weightA = initialWeightA + (scale_A.get_units(20) / 1000.0);  
  float weightB = initialWeightB + (scale_B.get_units(20) / 1000.0);  

  float totalWeight = weightA + weightB;		// adds both scales for total weight.


  // Convert to selected unit
  float displayWeight = totalWeight;
  if (unitMode == 0) {
      unitStr = "kg";
      decPlaces = 2;
  } else if (unitMode == 1) {
      displayWeight = totalWeight * KG_TO_GRAMS;
      unitStr = "g";
      decPlaces = 0;
  } else if (unitMode == 2) {
      displayWeight = totalWeight * KG_TO_LB;
      unitStr = "lb";
       decPlaces = 2;
  } else if (unitMode == 3) {
      displayWeight = totalWeight * KG_TO_STONE;
      unitStr = "st";
      decPlaces = 2;
  }

  // Display weight on LCD.
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Total: ");
  lcd.setCursor(0, 1);
  lcd.print(displayWeight, decPlaces);		//prints display weight to decPlaces decimal places
  lcd.print(" " + unitStr + " ");

  delay(500);
}


// Functions/Methods ----------

void start_screen(){
  delay(500);
  lcd.setCursor(2,0);
  lcd.print("L'Equillibre");
  delay(1000);
  lcd.setCursor(7,1);
  lcd.print(":)");
  delay(1000);
 }

void initialize_HX711_Modules(){

  // Initialize HX711 A and B
  scale_A.begin(LOADCELL_DOUT_PIN_A, LOADCELL_SCK_PIN_A);
  scale_B.begin(LOADCELL_DOUT_PIN_B, LOADCELL_SCK_PIN_B);

  // makes sure BOTH HX711 are connected. If not, program will not run and error message given.
  while (!(scale_A.is_ready() || scale_B.is_ready())) {
    lcd.clear();
    delay(500);
    lcd.print("~Connect HX711!~");
    delay(500);
  }
  // if both are connected, suitable message is given.
  lcd.clear();
  lcd.print("HX711 Ready!");
  delay(200);
}

void tareFunc(){
  tareFlag = false;
  
  // resets both scales.
  scale_A.tare();				
  scale_B.tare();		

  // sets initial weight to new weight (0 since Tare applied).
  initialWeightA = 0;			
  initialWeightB = 0;

  // display message for suitable time
  lcd.clear();
  lcd.setCursor(2,0);
  lcd.print("Tare Applied");
  delay(2000);
  lcd.clear();
}


// Interrupts ----------

void ISR_TareSet(){
  tareFlag = true;
}


void ISR_Unit(){
  unitMode = (unitMode + 1) % 4;  	/* This increments unitMode everytime it is called. 
					   The number stored in unitMode corresponds to a measurement (0 = grams, 1 = kg, 2 = lbs, 3 = st).
					   "% 4" means find the remainer of unitValue divided by 4. When unitValue is 4 the remainder is 0.  
					   unitMode is then assigned this value, THEREFORE resetting it to 0 when it reaches 4. :) */				
}


/*
  ___     .__/\___________            .__.__  ._____.                    ___     
 / _ \_/\ |  )/\_   _____/ ________ __|__|  | |__\_ |_________   ____   / _ \_/\ 
 \/ \___/ |  |  |    __)_ / ____/  |  \  |  | |  || __ \_  __ \_/ __ \  \/ \___/ 
          |  |__|        < <_|  |  |  /  |  |_|  || \_\ \  | \/\  ___/           
          |____/_______  /\__   |____/|__|____/__||___  /__|    \___  >          
                       \/    |__|                     \/            \/           		*/

		       
		       