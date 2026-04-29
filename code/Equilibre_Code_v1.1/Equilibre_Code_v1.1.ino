#include <Arduino.h>  
#include <HX711.h>
#include <LiquidCrystal.h>

/*
    Project Name: L'Equilibre
    Author: John Pritchard
    Date: 14/02/2025
    Version: 1.1
    
    Description: 
      This code allows the user to weigh themselves and other items. This is done by taking the data from the load cells
      and converting it into the user's desired unit measurement (kilograms, grams, pounds, stone).
      The user has two buttons - one that can Tare the scale (reset reading to 0) and one that can 
      change the measuremnt units.
      
      1.1 (14/02/2025): Added and refined main functionality of scale and buttons. The buttons could be further enhanced using interrupts.
      
    Hardware:
      - ATmega328 Arduino Uno Boards
      - 2 Push Buttons (Pin 12 and 13)
      - 2 HX711 - scale_A (Pin 2 and Pin 3) and scale_B (Pin 4 and Pin 5)
      - 4 Load Cells (2 for each HX711)
      
    
*/

// Initialize LCD (RS, E, D4, D5, D6, D7)
LiquidCrystal lcd(6, 7, 8, 9, 10, 11);

// Define HX711 connections for two modules
const int LOADCELL_DOUT_PIN_A = 2;  
const int LOADCELL_SCK_PIN_A = 3;   
const int LOADCELL_DOUT_PIN_B = 4;  
const int LOADCELL_SCK_PIN_B = 5;   

// Define Buttons
const int TARE_BUTTON = 12;  // Button to reset scale (TARE)
const int UNIT_BUTTON = 13;  // Button to switch units

// Create HX711 instances
HX711 scale_A;
HX711 scale_B;

// Variables to store initial weight
float initialWeightA = 0;
float initialWeightB = 0;
int unitMode = 0; 	// 0 = kg, 1 = lb, 2 = stone

const float calibrationFactorA = 1;  
const float calibrationFactorB = 1;  

// Conversion constants
const float KG_TO_LB = 2.20462;  // 1 kg = 2.20462 lb
const float KG_TO_STONE = 0.157473;  // 1 kg = 0.157473 stone

String unitStr = "";		// string that stores the unit that the system is on.

void setup() {
  Serial.begin(57600);	//set Baud rate
  lcd.begin(16, 2);
    
    
  // Set up the buttons - NOTE: INPUT_PULLUP enables pullup resistor to ensure the read value from the buttons are either HIGH or LOW.
  pinMode(TARE_BUTTON, INPUT_PULLUP);
  pinMode(UNIT_BUTTON, INPUT_PULLUP);			
    			
  initialize_HX711_Modules();
    
  // Apply calibration.
  scale_A.set_scale(calibrationFactorA);
  scale_B.set_scale(calibrationFactorB);

  // get initial weight of both scales and convert to kg.
  initialWeightA = scale_A.get_units(30) / 1000.0;
  initialWeightB = scale_B.get_units(30) / 1000.0;

  // Tare the scales to remove offset.
  scale_A.tare();
  scale_B.tare();

  delay(1000);
  lcd.clear();
}

void loop() {
  if (digitalRead(TARE_BUTTON) == LOW){
    TareFunction();
  }

  if (digitalRead(UNIT_BUTTON) == LOW){
		UnitCycle();
  }
 	
  // Read weight from load 2 cells each. Converts to kg for ease since all conversion constants are from kg to something else.
  float weightA = scale_A.get_units(20) / 1000.0;  
  float weightB = scale_B.get_units(20) / 1000.0;  

  float correctedWeightA = weightA + initialWeightA;
  float correctedWeightB = weightB + initialWeightB;
   
  float totalWeight = correctedWeightA + correctedWeightB;		// adds both scales for total weight.
    
  // Convert to selected unit
  float displayWeight = totalWeight;
  if (unitMode == 0) {
    unitStr = "kg";
  } else if (unitMode == 1) {
    displayWeight = totalWeight * KG_TO_LB;
    unitStr = "lb";
  } else if (unitMode == 2) {
    displayWeight = totalWeight * KG_TO_STONE;
    unitStr = "st";
  } 

  // Display weight on LCD.
  lcd.setCursor(0, 0);
  lcd.print("Total: ");
  lcd.setCursor(8, 0);
  lcd.print(displayWeight, 2);
  lcd.print(" " + unitStr + " ");

  delay(500); 
}

// Functions/Methods ----------------------------------------------------------------------------------------------------------------------------------

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
}


void TareFunction(){
	
  scale_A.tare();				// resets both scales.
  scale_B.tare();		
	
  initialWeightA = 0;			// sets initial weight to new weight (0 since Tare applied).
  initialWeightB = 0;
  
	// display message for suitable time
	lcd.clear();
	lcd.setCursor(2,0);
  lcd.print("Tare Applied");
  delay(750);
	lcd.clear();    
}

void UnitCycle(){
  unitMode = (unitMode + 1) % 3;  	/* This increments unitMode everytime it is called. 
						   The number stored in unitMode corresponds to a measurement (0 = kg, 1 = lbs, 2 = st).
						   "% 3" means find the remainer of unitValue divided by 3. When unitValue is 3 the remainder is 0.  
						   unitMode is then assigned this value, THEREFORE resetting it to 0 when it reaches 3. :) */
}


/*
  ___     .__/\___________            .__.__  ._____.                    ___     
 / _ \_/\ |  )/\_   _____/ ________ __|__|  | |__\_ |_________   ____   / _ \_/\ 
 \/ \___/ |  |  |    __)_ / ____/  |  \  |  | |  || __ \_  __ \_/ __ \  \/ \___/ 
          |  |__|        < <_|  |  |  /  |  |_|  || \_\ \  | \/\  ___/           
          |____/_______  /\__   |____/|__|____/__||___  /__|    \___  >          
                       \/    |__|                     \/            \/           		*/
