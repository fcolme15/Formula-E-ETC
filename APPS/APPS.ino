// [TODO]
// Add check for enable signal from master controller before outputting to I2C
// Add code to set a digital pin high whenever brake pedal is pressed (brake light activation for master controller)
// Out-of-range values from APPS to Arduino & between Arduino and motor controller - T.4.2.10 & T.4.3.4
// Test calibration in assembly
// Add check to calibration numbers before writing to EEPROM
// Add check to only allow calibration while accel < 10 (creates issues if throttle stuck > 10, maybe only while moving? or allow when accel > 10 and break engaged?)
// Improve Readibility: Add more comments, but variable names
// Need to make sure that upon calibration exit, output is 0, so that the car doesn't unexpectedly take off 


// #include <EEPROM.h> // EEPROM LIBRARY
#include <Adafruit_MCP4725.h> // I2C DAC LIBRARY

#define analogPin5g A6  // 5V Gas Input
#define analogPin3g A7  // 3.3V Gas Input
#define analogWritePin A0

#define calibration_interrupt_pin 2 // Input Button, toggles the calibration state
#define calibration_indicator_pin 8 // Output LED, indicates if we are in a calibration state
#define brake_state_pin 6
#define implausibility_pin 3

#define RTD_pin 5

bool calibration_state = false;
bool brakes_engaged = true;

// Variables for storing percentages mapped from analog readings
int accel_pedal_travel; // was gasperc, combined accel percentages/2 for final pedal travel percentage
int brake_pedal_travel; // was brakeperc, same as above

// Defining variables & initializing them as zero for potentiometer readings and calculated percentages
int analog_in_5acc = 0; // was analog_5g
int analog_in_3acc = 0;
int percent_5g = 0;
int percent_3g = 0;

bool pedal_implausibility = false; // pedal_implausibility is the state that occurs when the position of two matching pedals do not agree
bool temp_pedal_implausibility = false; // temporary variable needed to keep track of time implausibility has occurred for
bool brake_implausibility = false; // brake_implausibility is the state that occurs when brake is engaged and accel > 25%
unsigned long implausibility_timer = 0;

// variable init for pedal calibration mins/maxs
int accel_5v_max = 1000;
int accel_5v_min = 500;
int accel_3v_max = 1000;
int accel_3v_min = 300;

unsigned long debounce_millis = 0; // move this to static variable in calibration_toggle_state func

Adafruit_MCP4725 dac; // create I2C DAC object


// setup()
// Before we enter the loop() below, this setup() function is run once
void setup() {
  Serial.begin(115200);
  dac.begin(0x62); // initialize I2C DAC at Addr: 0x62

  pinMode(analogPin5g, INPUT);
  pinMode(analogPin3g, INPUT);
  // pinMode(gas_pin, OUTPUT);

  pinMode(calibration_interrupt_pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(calibration_interrupt_pin), calibration_toggle_state, FALLING);

  pinMode(brake_state_pin, INPUT_PULLUP);
  pinMode(RTD_pin, INPUT);

  pinMode(calibration_indicator_pin, OUTPUT);
  pinMode(implausibility_pin, OUTPUT);

  pinMode(analogWritePin, OUTPUT);

  analogWriteResolution(12); // unecessary for I2C DAC
  analogReadResolution(12);
  //read_eeprom(); // read eeprom values and overwrite min/maxes
}


// loop()
// Main driver code, as long as the Arduino is on this code loops forever
void loop() {
  digitalWrite(calibration_indicator_pin, calibration_state);
  digitalWrite(implausibility_pin, pedal_implausibility || brake_implausibility);
  
  if (calibration_state) {
    calibrate_pedals();
    //write_to_eeprom();
  }

  if (get_accel_pedal_travel()) {
    pedal_implausibility = false;
    temp_pedal_implausibility = false;
  } else {
    if (temp_pedal_implausibility = false) {
      temp_pedal_implausibility = true;
      implausibility_timer = millis();
    } else {
      if (millis() - implausibility_timer > 200) {
        pedal_implausibility = true;
      }
    }
  }

  get_brake_pedal_travel();
  check_brake_implausibility();

  if (!pedal_implausibility && !brake_implausibility) {
    // write_accel_value();
    write_accel_value_i2c();
  } else if (digitalRead(RTD_pin)){
    dac.setVoltage(0, false);
  }

  print_state();
  if (brakes_engaged) {
    Serial.println("Brakes engaged");
  }
}

void check_brake_implausibility() {
  if (brakes_engaged && accel_pedal_travel > 5) {
    if (accel_pedal_travel > 25) {
      brake_implausibility = true;
    }
  } else if (accel_pedal_travel < 6) {
    brake_implausibility = false;
  }
}


// void read_eeprom() // read min/max values from eeprom; 
// {
//   accel_5v_max = 16 * EEPROM.read(0);
//   accel_5v_min = 16 * EEPROM.read(1);
//   accel_3v_max = 16 * EEPROM.read(2);
//   accel_3v_min = 16 * EEPROM.read(3);
//   Serial.println("EEPROM READ");
// }


// void write_to_eeprom() // write min/max values to eeprom; todo: find a way to store full 12bit number
// {
//   EEPROM.write(0, (accel_5v_max/16));
//   EEPROM.write(1, (accel_5v_min/16));
//   EEPROM.write(2, (accel_3v_max/16));
//   EEPROM.write(3, (accel_3v_min/16));
//   Serial.println("EEPROM UPDATED");
// }


// write_accel_value_i2c()
// double check constrain values; replace map with a slope function and test difference in speed; 
// add check to see if setVoltage function returns false
void write_accel_value_i2c() {
  int output = constrain(map(accel_pedal_travel, 0, 100, 820, 3280),820, 3280);

  if(accel_pedal_travel < 10)
  {
    dac.setVoltage(0, false);
  } else {
    dac.setVoltage((uint16_t)output, false);
  }

  //dac.setVoltage((uint16_t)output, false);
}


// write_accel_value()
// transform pedal travel % to usable DAC 12-bit value
void write_accel_value() {
  int output = constrain(map(accel_pedal_travel, 0, 100, 0, 4080),0, 4080);
  analogWrite(analogWritePin, output);
}


// calibration_toggle_state()
// add check to make sure calibration mode cant be entered at throttle > 10, 
// need way to reset throttle calibration if stuck at > 10
void calibration_toggle_state() {
  // debounce button input and toggle calibration state
  if ((millis() - debounce_millis) > 500 ) { //&& !digitalRead(brake_state_pin) taking out for testing lol
    calibration_state = !calibration_state;
  }

  debounce_millis = millis();
}


// calibrate_pedals()
// set min/max pedal vals based on measured travel, 
// need to add checks to ensure min/max vals have a reasonable difference
void calibrate_pedals() {
  accel_5v_max = 50;//analogRead(analogPin5g);
  accel_5v_min = 50000;//accel_5v_max;
  accel_3v_max = 50;//analogRead(analogPin3g);
  accel_3v_min = 50000;//accel_3v_max;

  int accel_5v;
  int accel_3v;

  while (calibration_state) {
    accel_5v = analogRead(analogPin5g);
    accel_3v = analogRead(analogPin3g);

    char s [50];
    sprintf(s, "Calibration | 5V %i, 3V %i", accel_5v, accel_3v);
    Serial.println(s);

    if (accel_5v > accel_5v_max) accel_5v_max = accel_5v;
    if (accel_5v < accel_5v_min) accel_5v_min = accel_5v;
    if (accel_3v > accel_3v_max) accel_3v_max = accel_3v;
    if (accel_3v < accel_3v_min) accel_3v_min = accel_3v;
  }
}


// print_state()
// log variables to console
void print_state() {
  Serial.print("Gas 5V: ");
  Serial.print(percent_5g);
  Serial.print("%  Analog:");
  Serial.print(analog_in_5acc);
  Serial.print("  Gas 3.3V: ");
  Serial.print(percent_3g);
  Serial.print("%  Analog:");
  Serial.print(analog_in_3acc);

  Serial.print("Combined Accel Pedal Travel: ");
  Serial.println(accel_pedal_travel); 

  Serial.print("5V Max/Min: ");
  Serial.print(accel_5v_max);
  Serial.print(" ");
  Serial.print(accel_5v_min);
  Serial.print("  3.3v Max/Min: ");
  Serial.print(accel_3v_max);
  Serial.print(" ");
  Serial.print(accel_3v_min);

  Serial.print("     left:");
  Serial.print(percent_3g);
  
  Serial.print("     right: ");
  Serial.println(percent_5g);

  delay(100);
}


// get_accel_pedal_travel()
// read both pedal values, convert to percentages and compare, return false if implaus occurs
bool get_accel_pedal_travel() {
  bool return_state = false;

  analog_in_5acc = analogRead(analogPin5g);  // 5V Gas INPUT
  analog_in_3acc = analogRead(analogPin3g);  // 3.3V Gas INPUT
  if (analog_in_5acc > accel_5v_max) accel_5v_max = analog_in_5acc;
  if (analog_in_5acc < accel_5v_min) accel_5v_min = analog_in_5acc;
  if (analog_in_3acc > accel_3v_max) accel_3v_max = analog_in_3acc;
  if (analog_in_3acc < accel_3v_min) accel_3v_min = analog_in_3acc;
  percent_5g = constrain(map(analog_in_5acc, accel_5v_min, accel_5v_max, 0, 100), 0, 100);
  percent_3g = constrain(map(analog_in_3acc, accel_3v_min, accel_3v_max, 0, 100), 0, 100);

  // Gas pedal implausibility check
  if (abs(percent_5g - percent_3g) <  10) {
    return_state = true;
    accel_pedal_travel = constrain(((percent_5g + percent_3g) / 2), 0, 100);  // Since no implausibility, taking the average of the two percentages
  } else {
    return_state = false;
  }

  return return_state;
}

// get_brake_pedal_travel()
// read both pedal values, convert to percentages and compare, return false if implaus occurs
void get_brake_pedal_travel() {
  brakes_engaged = !digitalRead(brake_state_pin);
}
