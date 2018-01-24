/*
 * Durgin Family Farms - Hydroponics Strawberries Monitoring System
 * 
 * The microcontroller reads the values of multiple sensors via analog or digital pins
 * and displays the recorded values on a web page using an Arduino Ethernet Shield.
 * If there is no ethernet connection, the data is stored in the SD Card on the ethernet
 * shield.
 * 
 * Microcontroller:
 * 
 * ** Arduino Mega ADK R3
 * ** Arduino Ethernet Shield
 * 
 * Sensors
 * 
 * ** Water Temperature Sensor: Waterproof DS18B20
 * ** Water Flow Sensor: Liquid Flow Meter - Plastic 1/2" NPS Threaded
 * ** Water Level Sensor: 
 * ** Dissolved Oxygen Sensor:
 * ** pH Sensor:
 * ** Electrical Conductivity Sensor: Homemade EC SEnsor
 * 
 * Pin Setup:
 * Water Temperature:   digital pin 6
 * Water Flow:          digital pin 2
 * Water Level:         Analog pin 10
 *             Echo     digital pin 8
 *             Trigger  digital pin 10             
 * Dissolved Oxygen     none
 * pH                   none
 * EC                   Analog pin 0                
 *            EC PWR    Analog pin 4
 */

// libraries
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Ethernet.h>
#include <SD.h>
#include <Wire.h>                       // Enable I2C


#define DO2_address 97                  // default I2C ID number for EZO D.O. Circuit
#define pH_address 99
/* Digital Pin declaration */
#define ONE_WIRE_BUS_TEMP_SENSOR 6      // digital pin for temperature sensor
#define FLOW_SENSOR_PIN 2               // digital pin for flow sensor
#define ECHO_PIN 8                      // digital pin for water level: echo
#define TRIGGER_PIN 10                  // digital pin for water level: trigger

// Module identifier
String MODULE_NUMBER = "A";

// MAC address for the controller
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

// static ip address
byte ip[] = {129,21,63,177};

// number of hours
int numHours = 0;

/*
 * Variables for water temperature sensor
 */
// Setup a OneWire instance for the temperature sensor
OneWire TempSensor(ONE_WIRE_BUS_TEMP_SENSOR);

// Pass OneWire reference to Dallas Temperature
DallasTemperature temperature_sensor(&TempSensor);

float WaterTempSensor = 0.0;                // variable to hold sensor data
double MaxWaterTemp = 88;                   // maximum water temperature for the system
double MinWaterTemp = 77;                   // minimum water temperature for the system

/*
 * Variable for water flow sensor
 */
volatile uint16_t flow_pulses = 0;          // number of pulses
volatile uint16_t flow_LastState;           // track the state of the flow pin
volatile uint32_t flowratetimer = 0;        // time between pulses
volatile float WaterFlowSensor = 0.0;       // variable to hold sensor data
double MaxWaterFlow = 30;                   // maximum water flow in the system
double MinWaterFlow = 1;                    // minimum water flow in the system

/*
 * Variables for water level sensor
 */
int WaterLevelPin = A10;                    // analog pin for water level sensor
double WaterLevelSensor = 0.0;              // variable to hold sensor data
double MaxWaterLevel;                       // maximum water level in the system
double MinWaterLevel;                       // minimum water level in the system

/*
 * Variables for the Dissolved Oxygen sensor
 */
float DO2Sensor = 0.0;                      // variable to hold the sensor data
double MaxDO2;                              // maximum dissolved oxygen in the system
double MinDO2;                              // minimum dissolved oxygen in the system

/*
 * Variables for pH sensor
 */
float phSensor = 0.0;                       // variable to hold the sensor data
double MaxpH;                               // maximum pH in the system
double MinpH;                               // minimum pH in the system

/*
 * Variables for homemade EC sensor
 */
float CalibrationEC = 2.9;                  // EC value of calibration solution in s/cm
float TempCoef = 0.019;                     //0.019 is considered the standard for plan nutrients
float KValue = 0;
int ECSensorPin = A0;                       // analog pin for ec sensor
int ECSensorPWR = A4;                       // analog pin to power the ec sensor
int RA = 1000;                              // pull-up resistor for the ec sensor
int RB = 25;                                // Resistance of powering pins
float EC = 0;
float EC25 = 0;
float ECSensor = 0.0;                       // variable to hold sensor data
float raw = 0;
float ECVin = 5;
float ECVdrop = 0;
float RC = 0;
float ECbuf = 0;
float PPMconversion = 0.5;                  // converting to ppm (US)





/*
 * Initialize the sensors for the monitoring system
 */
void InitSensors(){
  /* Setup Water temperature sensor */
  temperature_sensor.begin();             // initialize temperature sensor
  delay(100);                             // wait for 100 ms

  /* Setup water level sensor */
  pinMode(WaterLevelPin, INPUT);          // set analog pin as input
  delay(100);                             // wait for 100 ms

  /* Setup EC Sensor */
  pinMode(ECSensorPin, INPUT);
  pinMode(ECSensorPWR, OUTPUT);
  RA = RA + RB;
  delay(1000);

  
  
}/* InitSensors() */


/*
 * Calibrate the electrical conductivity sensor
 */
void setKValue(){
  int lcv = 1;            // loop counter variable
  float StartTemp = 0;    // Starting temperature of the solution
  float FinishTemp = 0;   // Ending temperature of the solution
  ECbuf = 0;
  

  temperature_sensor.requestTemperatures();             // get the current temperature of the solution
  StartTemp = temperature_sensor.getTempCByIndex(0);    // starting temperature

  while (lcv <= 10){
    digitalWrite(ECSensorPWR,HIGH);
    raw = analogRead(ECSensorPin);    // first reading will be low
    raw = analogRead(ECSensorPin);
    digitalWrite(ECSensorPWR, LOW);
    ECbuf = ECbuf + raw;
    lcv++;                            // increade loop counter variable
    delay(5000);                      // wait for 5 seconds
  }
  raw = (ECbuf/10);

  temperature_sensor.requestTemperatures();             // get the current temperature of the solution
  FinishTemp = temperature_sensor.getTempCByIndex(0);   // final temperature of the solution

  EC = CalibrationEC * (1 + (TempCoef * ( FinishTemp - 25.0)));

  ECVdrop = (((ECVin) * (raw)) / 1024.0);
  RC = (ECVdrop * RA) / (ECVin - ECVdrop);
  RC = RC - RB;
  KValue = 1000 / (RC*EC);

  if (StartTemp == FinishTemp){
    
  } else {
    
  }
  
}

/*
 * Setup the monitoring system:
 *          -> Open serial communication
 *          -> Initialize the sensors
 *          -> Calibrate the EC Sensor
 *          -> Initialize the SD card
 *          -> Setup the ethernet connection
 *          -> Start the server connection
 */
void setup() {
  Serial.begin(9600);           // enable serial port

  Wire.begin();                 // enable I2C port

  InitSensors();                // initializes sensors
  

}

/*
 * Read the temperature of the water several times and return the average
 */
void getWaterTemperature(){
  WaterTempSensor = 0.0;                        // Initialize temperature sensor to zero.
  // read the temperature 10 times
  for (int lcv = 0; lcv < 10; lcv++){
    temperature_sensor.requestTemperatures();
    WaterTempSensor += DallasTemperature::toFahrenheit(temperature_sensor.getTempCByIndex(0));
    delay(1000);
  }/* for() */

  WaterTempSensor = WaterTempSensor / 10;       // take average of the values

  delay(100);         // wait for 100 ms
} /* gerWaterTemperature() */

/*
 * Read the water level of the tank
 */
void getWaterLevel(){
  WaterLevelSensor = 0.0;                       // Initialize water level to zero
  float Level = 0.0;                            // variable to hold water level value

  for (int lcv = 0; lcv < 10; lcv++){
    //Set the trigger pin to low for 2 uS
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);

    // Set the trigger high for 10uS for ranging
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);

    // Send the trigger pin low again
    digitalWrite(TRIGGER_PIN, LOW);

    Level = pulseIn(ECHO_PIN, HIGH, 26000);
    WaterLevelSensor += (Level/58);
  }

  WaterLevelSensor /= 10;                     // take average
}/* getWaterLevel() */

/*
 * read electrical conductivity of solution in the tank
 */
void getEC(){
  ECSensor = 0.0;
  float temp = 0;
  for (int lcv=0; lcv<10; lcv++){
    temperature_sensor.requestTemperatures();   // current temperature
    //temp = 
  }
}

/*
 * Read the amount of dissolved oxygen in the solution
 */
void getDissolvedOxygen(){
  byte lcv = 0;                               // counter variable used for DO2_data array
  int code = 0;                               // hold the I2C response code
  char DO_data[20];                           // 20 byte character array to hold incoming data from the D.O. circuit
  byte in_char = 0;                           // 1 byte buffer to hold incoming data from the D.O. circuit
  

  Wire.beginTransmission(DO2_address);        // call the circuit by its number.
  Wire.write('r');
  Wire.endTransmission();                     // end the I2C data transmission

  delay(600);                                 // delay to take a reading

  Wire.requestFrom(DO2_address,20,1);         // call the circuit and request 20 bytes
  code = Wire.read();                         // the first byte is the response code

  switch (code) {                             // Switch case based on what the response code is
    case 1:                                   // decimal 1
      Serial.println("Sucess");               // command was successful
      break;                                  // exits the switch case

    case 2:                                   //decimal 2
      Serial.println("Failed");               // command has failed
      break;                                  // exits the switch case

    case 254:                                 // decimal 254
      Serial.println("Pending");              // command has not yet finished calculating
      break;                                  // exits the switch case

    case 255:                                 // decimal 255
      Serial.println("No data");              // No further data to send
      break;    
  }
  delay(600);                                 
  while (Wire.available()) {                  // are there bytes to receive
    in_char = Wire.read();                    // receive a byte.
    DO_data[lcv] = in_char;                   // append the byte to the array
    lcv++;                                    // increment the counter
    if (in_char == 0){                        // if a null command has been sent
      lcv = 0;                                // reset the counter to 0
      Wire.endTransmission();                 // end the I2C transmission
      break;                                  // exit the while loop
    } /* if() */
  } /* while () */

  if (isDigit(DO_data[0])){
    DO2Sensor = parse_string(DO_data);        // If the first char is a number we know it is a DO reading, lets parse the DO reading
  } else {
    DO2Sensor = atof(DO_data);                // Set the data to DO2 variable    
  } /* if() */

  delay(5000);                                // wait for 5 seconds
} /* getDissolvedOxygen() */

/*
 * this function will break us the CSV string into its 2 individual parts
 */
float parse_string(char *data){
  byte flag = 0;                        // variable to indicate a "," was found in the string array
  byte lcv = 0;                         // loop counter variable
  char *DO;                             // char pointer used in string parsing
  char *sat;                            // char pointer used in string parsing
  float result = 0;                     // float value of the data required

  for (lcv = 0; lcv < 20; lcv++){       // step through the array
    if (data[lcv] == ','){              // check if "," is present
      flag = 1;                         // if present set the flag to 1
    } /* if() */
  } /* for() */

  if (flag != 1) {                      // if no "," was found
    result = atof(data);                // convert to float
  }/* if() */

  if (flag == 1){                       // if a "," was found
    DO = strtok(data,",");              // parse the string at each comma
    sat = strtok(NULL, ",");            // parse the string at each comma
    result =  atof(DO);                 // convert to float
  }
  return result;                        // return the value
}

/*
 * Read the pH of the solution
 */
void getPH(){
  byte lcv = 0;                               // counter variable used for PH_data array
  int code = 0;                               // hold the I2C response code
  char PH_data[20];                           // 20 byte character array to hold incoming data from the pH circuit
  byte in_char = 0;                           // 1 byte buffer to hold incoming data from the pH circuit
  

  Wire.beginTransmission(pH_address);         // call the circuit by its number.
  Wire.write('r');
  Wire.endTransmission();                     // end the I2C data transmission

  delay(900);                                 // delay to take a reading

  Wire.requestFrom(pH_address,20,1);          // call the circuit and request 20 bytes
  code = Wire.read();                         // the first byte is the response code

  switch (code) {                             // Switch case based on what the response code is
    case 1:                                   // decimal 1
      Serial.println("Sucess");               // command was successful
      break;                                  // exits the switch case

    case 2:                                   //decimal 2
      Serial.println("Failed");               // command has failed
      break;                                  // exits the switch case

    case 254:                                 // decimal 254
      Serial.println("Pending");              // command has not yet finished calculating
      break;                                  // exits the switch case

    case 255:                                 // decimal 255
      Serial.println("No data");              // No further data to send
      break;    
  }
  
  while (Wire.available()) {                  // are there bytes to receive
    in_char = Wire.read();                    // receive a byte.
    PH_data[lcv] = in_char;                   // append the byte to the array
    lcv++;                                    // increment the counter
    if (in_char == 0){                        // if a null command has been sent
      lcv = 0;                                // reset the counter to 0
      Wire.endTransmission();                 // end the I2C transmission
      break;                                  // exit the while loop
    } /* if() */
  } /* while () */

  phSensor = atof(PH_data);                   // convert ascii to float

  delay(5000);                                // wait for 5 seconds
} /* getPH() */


/*
 * Main function of the monitoring system
 */
void loop() {
  // put your main code here, to run repeatedly:

}
