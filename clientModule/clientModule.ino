/*
 *  Client Module
 *  
 *  The microcontroller read the values of multiple sensors via analog and digital pins
 *  and sends the recorded value to a main server module using UDP. If there is no ethernet
 *  connection the data is stored in a text on the SD card.
 *  
 * Pin Setup:
 * Water Temperature
 *            Power     Analog pin A3
 *            Data      Analog pin A4
 *            Ground    Analog pin A5
 *            
 * Water Flow:          
 *            Power     Arduino 5V pin
 *            Data      Analog pin A0
 *            Ground    Arduino GND pin
 *            
 * Water Level:        
 *             Power    Analog pin A12
 *             Echo     Analog pin A14
 *             Trigger  Analog pin A13             
 *             Ground   Analog pin A15
 *             
 * Dissolved Oxygen     BNC connection to tentacle shield mini
 * 
 * pH                   BNC connection to ten
 * 
 * Electrical Conductivity
 *              Power   Analog pin A1
 *              Data    Analog pin A2
 *              Ground  Arduino pin GND pin 
 */

/*
 * @author: Jerry Affricot
 */
/* ---------------------------------- Libraries ------------------------------------------------- */

#include <SPI.h>                                // Serial library
#include <OneWire.h>                            // One Wire library
#include <Ethernet2.h>                          // Ethernet library
#include <EthernetUdp2.h>                       // UDP library
#include <SD.h>                                 // SD card library
#include <Wire.h>                               // EZO library

/* ---------------------------------- Varialbles ------------------------------------------------- */
#define DO2_address 97                          // default I2C ID number for EZO D.O. Circuit
#define pH_address 99                           // default I2C ID number for EZO pH Circuit


String MODULE_NUMBER = "A.1";                           // module ID
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
char udpBuffer[100];                            // character array for udp message
char udpResponse[25];                           // character array for udp response
unsigned int serverPort = 8080;                 // SET TO SERVER UDP PORT
IPAddress serverIP(129,21,63,177);              // SET TO SERVER IP ADDRESS
IPAddress ip(129,21,63,177);                    // SET TO UNIQUE IP ADDRESS

EthernetUDP clientUDP;                          // ethernet udp
unsigned int localPort = 8081;                  // SET TO UNIQUE PORT NUMBER

char emailServer[] = "mail.smtp2go.com";        // SMTP server to send email

EthernetClient emailClient;                         // ethernet client for email

int emailPort = 2525;                           // port for the email server

String emailBuffer = "";                        // email Buffer to hold email message

int emailFlag = 0;                              // 0 - email has been sent successfully

// Temperature
int ONE_WIRE_BUS_TEMP_SENSOR = A4;
OneWire tempWire(ONE_WIRE_BUS_TEMP_SENSOR);     // Setup a OneWire instance for the temperature sensor
float tempSensor = 0.0;                         // variable to hold sensor data
double minTemp = 77;                            // minimum solution temperature for the system
double maxTemp = 85;                            // maximum solution temperature for the system
String tempStatus = "OK";                             // variable to hold sensor status: 0 -> under, 1 -> ok, 2 -> over


// Electrical Conductivity
float ecSensor = 0.0;                           // variable to hold sensor data
double minEC = 1500;                            // minimum ec
double maxEC = 2300;                            // maximum ec
String ecStatus = "OK";                               // current status of the E.C.
int ECPin = A2;                                 // Analog pin for E.C. data line
int ECPwr = A1;                                 // Analog pin to provide power to E.C. sensor
float raw = 0;
float Vin = 5;
float Vdrop = 0;
float TempCoef = 0.019;                         // 0.019 is considered the standard for plant nutrients
int RA = 1025;                                  // pull up resistor
int RB = 25;                                    // Resistance of powering pins
float RC = 0;                                   // measured resistance of the water
float EC = 0;
float EC25 = 0;
float PPMconversion = 0.5;                      // converting to ppm (US)
float KValue = 0.07;

// Dissolved Oxygen
float DO2Sensor = 0.0;                           // variable to hold sensor data
double minDO2 = 7;                               // minimum dissolved oxygen
double maxDO2 = 8.5;                             // maximum dissolved oxygen
String do2Status = "OK";

// pH
float phSensor = 0.0;                           // variable to hold sensor data
double minPH = 5.5;                             // minimum pH of the solution
double maxPH = 6.5;                             // maximum pH of the solution
String phStatus = "OK";

// level
int TRIGGER_PIN = A13;                              //
int ECHO_PIN = A14;                                 //
int levelGND = A15;                                 //
int levelPWR = A12;                                 //
float levelSensor = 0.0;                        // variable to hold sensor data
double minLevel = 15;                          // minimum gal. in the tank
double maxLevel = 40;                         // maximum gal. in the tank
String levelStatus = "OK";


// Flow
int LIQUIDFLOWPIN = A0;                         // digital pin for flow sensor
volatile uint16_t flow_pulses = 0;              // number of pulses
volatile uint16_t flowPrevState;                // track the state of the flow pin
volatile uint32_t flowratetimer = 0;            // time between pulses
volatile float flowrate;
float flowSensor = 0.0;                         // variable to hold sensor data
double minFlow = 0.0;                           // minimum water flow
double maxFlow = 0.0;                           // maximum water flow
int flowStatus = 0;

String dataString = "";                             // formatting string to write data to SD card

int doTime = 2400;

int sTime = 1800;

int phTime = 3000;

int ecTime = 28800;

int dataSend = 0;

int timeout = 0;

int measNum = 0;

int waitVal = 0;
/* ---------------------------------- Functions ------------------------------------------------- */

/*
 * Interrupt service routine
 * calculate the flow rate of the system
 */
SIGNAL(TIMER0_COMPA_vect){
  uint8_t currState = digitalRead(LIQUIDFLOWPIN);

  if (currState == flowPrevState){
    flowratetimer++;
    return;                                             // nothing changed
  } /* if () */

  if (currState == HIGH){                               // low to high transition
    flow_pulses++;
  }/* if () */

  flowPrevState = currState;
  flowrate = 1000.0;
  flowrate /= flowratetimer;            // in Hz  
  flowratetimer = 0;
} /* SIGNAL() */

/*
 * Interrupt function for flow rate sensor
 * Interrupts the board every 1 ms
 */
void useInterrupt(boolean val){
  if (val){   // if true
    // Timer 0 is already used for millis() - interrupt somewhere else
    // and call the function above
    OCR0A = 0XAF;
    TIMSK0  |= _BV(OCIE0A);    
  } else {
    TIMSK0 &= ~_BV(OCIE0A);
  } /* if() */
}/* useInterrupt() */

/*
 * Initialize the sensors for the monitoring system
 */
void initSensors(){
  // temperature sensor
  pinMode(A3,OUTPUT);                             // temperature power pin
  digitalWrite(A5,LOW);                           // set output pin to ground
  pinMode(A5,OUTPUT);                             // temperature ground pin  
  digitalWrite(A3,HIGH);                          // set output pin to 5V 

  delay(1000);                                    // wait for 1000 ms

  // EC sensor
  pinMode(ECPin,INPUT);                           // EC sensor data pin
  pinMode(ECPwr,OUTPUT);                          // EC sensor power pin

  delay(1000);                                    // wait for 1000 ms
  
  // D.O. and pH sensor
  Wire.begin();                                   // enable I2C port

  delay(1000);                                    // wait for 1000 ms
  
  /* Initialize & Setup water level sensor sensor */
  pinMode(levelGND,OUTPUT);                       // level sensor ground pin
  digitalWrite(levelGND,LOW);                     // set output pin to ground
  pinMode(levelPWR,OUTPUT);                       // level sensor power pin
  digitalWrite(levelPWR,HIGH);                    // set output pin to 5V
  pinMode(ECHO_PIN, INPUT);                       // level sensor echo pin
  pinMode(TRIGGER_PIN, OUTPUT);                   // level sensor trigger pin
  digitalWrite(ECHO_PIN,HIGH);                    // set output pin to 5V

  delay(1000);                                    // wait for 1000 ms

  /* Initialize & Setup Liquid flow sensor */
  pinMode(LIQUIDFLOWPIN, INPUT);                  // set flow pin as input  
  digitalWrite(LIQUIDFLOWPIN, HIGH);              // set the flow pin high
  flowPrevState = digitalRead(LIQUIDFLOWPIN);     
  useInterrupt(true);                             // initialize interrupt function

  delay(1000);                                    // wait for 1000 ms

  pinMode(A9,OUTPUT);
  digitalWrite(A9,LOW);
  pinMode(A10,OUTPUT);
  digitalWrite(A10,LOW);  
}/* InitSensors() */

/*
 * Setup the monitoring system:
 *  -> open serial communication (debugging)
 *  -> Initialize the SD card
 *  -> Start the Ethernet connection
 *  -> Start the Server
 *  -> Start UDP server
 *  -> Initialize the sensors
 */
void setup() {
  Serial.begin(9600);               // enable serial port

  Serial.println("Initializing SD card");
  if (!SD.begin(4)){        // check if a sd card is present and can be initialized
    Serial.println("Card failed or not present");
    return;    
  }/* if(SD.begin) */

  Serial.println("SD Card initialized");

  if (Ethernet.begin(mac)==0){
    Serial.println("Failed to configure Ethernet using DHCP");
    Ethernet.begin(mac,ip);
  }
  clientUDP.begin(localPort);
  initSensors();          // initialize sensors

  delay(30000);                                   // wait for 30 seconds
  randomSeed(1000);

}



void loop() {

  // get sensor data
  if (ecTime == 28800){           // every 8 hours
    getEC();                      // get EC data
    ecTime = 0;                   // reset ec counter
    delay(1000);                  // wait for 1000 ms
  }/* if (ecTime) */

  if (doTime == 2400){            // every 40 mins
    getDissolvedOxygen();         // get d.o. value
    doTime = 0;                   // reset oxygen counter
    delay(500);                   // wait for 500 ms
  }/* if (doTime) */

  if (phTime == 3000){            // every 50 mins
    getPH();                      // get pH value
    phTime = 0;                   // reset pH counter
    delay(500);                   // wait for 500 ms
  }/* if (phTime)*/

  if (sTime == 1800){               // every 30 mins
    getTemp();
    getLevel();
    getFlow();
//    dataString += measNum;
    dataString += ",";
    

//    logData(measNum);
//    measNum += 30;
    sTime = 0;    
  }/* if() */

  dataString = tempSensor;
  dataString += ",";    
  dataString += ecSensor;
  dataString += ",";    
  dataString +=  DO2Sensor;
  dataString += ",";    
  dataString += phSensor;
  dataString += ",";
  dataString += levelSensor;  
  dataString += ",";         
  dataString += flowSensor;
  
  Serial.println(dataString);

  // write data to file
  if (timeout == 93600){          // every 24 hours
    logData(measNum);
  }

  // send UDP message to user
  if (waitVal == dataSend){
    sendData();
    dataSend = random(0,30);
    dataSend = dataSend * 60;
  }

  delay(1000);                    //wait for one second
  // increment timers
  timeout++;
  ecTime++;
  phTime++;
  doTime++;
  sTime++;
  waitVal++; 

}/* loop() */

/*
 * Read the temperature of the Liquid several times and return the average
 */
float getTemp(){
  //returns the temperature from one DS18S20 in DEG Celsius

  byte data[12];
  byte addr[8];

  if ( !tempWire.search(addr)) {
      //no more sensors on chain, reset search
      tempWire.reset_search();
      return -1000;
  }

  if ( OneWire::crc8( addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return -1000;
  }

  if ( addr[0] != 0x10 && addr[0] != 0x28) {
      Serial.print("Device is not recognized");
      return -1000;
  }

  tempWire.reset();
  tempWire.select(addr);
  tempWire.write(0x44,1);                               // start conversion, with parasite power on at the end

  byte present = tempWire.reset();
  tempWire.select(addr);    
  tempWire.write(0xBE);                                 // Read Scratchpad

  for (int i = 0; i < 9; i++) {                           // we need 9 bytes
    data[i] = tempWire.read();
  }

  tempWire.reset_search();

  byte MSB = data[1];
  byte LSB = data[0];

  float tempRead = ((MSB << 8) | LSB);                    //using two's compliment
  float TemperatureSum = tempRead / 16;

  tempSensor = (TemperatureSum * 18 + 5)/10 + 32;

  // update status of the temperature sensor
  if (tempSensor > maxTemp){
    tempStatus = "2";    
  } else if (tempSensor < minTemp){
    tempStatus = "0";    
  } else {
    tempStatus = "1";    
  }/* if () */  

  delay(500);                                             // wait for 500 ms
  
  return TemperatureSum;
}/* getTemp() */


/*
 * Get EC value
 */
void getEC(){
     
  float temp  = getTemp();                // read the temperature of the solution

  delay(100);
  temp = getTemp();

  // Estimate resistance of liquid
  digitalWrite(ECPwr,HIGH);
  raw = analogRead(ECPin);
  raw = analogRead(ECPin);    // First reading will be low
  digitalWrite(ECPwr,LOW);

  // Converts to EC
  Vdrop = (Vin*raw)/1024.0;
  RC = (Vdrop*RA)/(Vin-Vdrop);
  RC = RC - RB;
  EC = 1000/(RC*KValue);

  // Compensating for Temperature
  EC25 = EC/(1+TempCoef*(temp - 25.0));
  ecSensor = (EC25) * (PPMconversion*1000);

  // check if ec value is within limits
  if (ecSensor > maxEC){
    ecStatus = "2";
  } else if (ecSensor < minEC){
    ecStatus = "0";
  } else {
    ecStatus = "1";
  } /* if(ecSensor) */

  //wait for 100 ms
  delay(100);
}/* getEC() */

/*
 * Read the amount of dissolved oxygen in the solution
 */
void getDissolvedOxygen(){
  byte lcv = 0;                               // counter variable used for DO2_data array
  int code = 0;                               // hold the I2C response code
  char DO_data[20];                           // 20 byte character array to hold incoming data from the D.O. circuit
  byte in_char = 0;                           // 1 byte buffer to hold incoming data from the D.O. circuit
  int errorCode = 0;

  Wire.beginTransmission(DO2_address);        // call the circuit by its number.
  Wire.write('r');
  Wire.endTransmission();                     // end the I2C data transmission

  delay(600);                                 // delay to take a reading

  Wire.requestFrom(DO2_address,20,1);         // call the circuit and request 20 bytes
  code = Wire.read();                         // the first byte is the response code

  errorCode = code;

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
  }/*switch()*/                                
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

  if (DO2Sensor > maxDO2){
    do2Status = "2";
  } else if (DO2Sensor < minDO2){
    do2Status = "0";    
  } else {
    do2Status = "1";
  }
  
  delay(10000);                                // wait for 10 seconds
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
  }/*if()*/
  return result;                        // return the value
}/* parse_string() */

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
  }/*switch()*/
  
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

  if (phSensor > maxPH){
    phStatus = "2";    
  } else if (phSensor < minPH){
    phStatus = "0";
  } else {
    phStatus = "1";
  }
  
  delay(10000);                                // wait for 10 seconds
} /* getPH() */

/*
 * Read the Liquid level of the tank
 */
void getLevel(){
  levelSensor = 0.0;                       // Initialize Liquid level to zero
  float Level = 0.0;                            // variable to hold Liquid level value 
  

  for (int lcv = 0; lcv < 1; lcv++){
    //Set the trigger pin to low for 2 uS
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);

    // Set the trigger high for 10uS for ranging
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);

    // Send the trigger pin low again
    digitalWrite(TRIGGER_PIN, LOW);

    Level = pulseIn(ECHO_PIN, HIGH, 26000);
    levelSensor += (Level/58);
    delay(500);
  }/*for()*/

  levelSensor /= 10;                     // take average

  if (levelSensor < minLevel){
    levelStatus = "0";
  } else {
    levelStatus = "1";
  }
  
  delay(1000);                                    // wait for 1000 ms
}/* getLevel() */

/*
 * read the current flow of the system
 */
void getFlow(){
//  flowSensor = 0;
  flow_pulses = 0;
  flowrate = 0;
  delay(10000);       // wait for 10 seconds
  flowSensor = 0.26 * (flowrate/7.5);  

  if (flowSensor > maxFlow){
    flowStatus = "2";
  } else if (flowSensor < minFlow){
    flowStatus = "0";
  } else {
    flowStatus = "1";
  }
  
}/* getFlow() */


void sendData(){
  String dataString = "";
  char ack[10];
  int lcv = 0;
  int packetSize = 0;
  
  dataString += MODULE_NUMBER;                                  // module ID
  dataString += ",";                                        // separator

  dataString += tempSensor;                                 // temperature value
  dataString += ",";                                        // separator
  dataString += tempStatus;                                 // temperature status
  dataString += ",";                                        // separator
  
  dataString += ecSensor;                                   // E.C. value
  dataString += ",";                                        // separator
  dataString += ecStatus;                                   // E.C. status
  dataString += ",";                                        // separator

  dataString += DO2Sensor;                                  // D.O. value
  dataString += ",";                                        // separator
  dataString += do2Status;                                  // D.O. status
  dataString += ",";                                        // separator

  dataString += phSensor;                                   // pH value
  dataString += ",";                                        // separator
  dataString += phStatus;                                   // pH status
  dataString += ",";                                        // separator

  dataString += levelSensor;                                // Level value
  dataString += ",";                                        // separator
  dataString += levelStatus;                                // Level Status
  dataString += ",";                                        // separator

  dataString += flowSensor;                                 // Flow value
  dataString += ",";                                        // separator
  dataString += flowStatus;                                 // Flow Status
  dataString += ",";                                        // separator

  dataString.toCharArray(udpBuffer,100);     // convert string to Array

  clientUDP.beginPacket(serverIP,serverPort);
  clientUDP.write(udpBuffer);
  clientUDP.endPacket();

  packetSize = clientUDP.parsePacket();
  while (!packetSize){
    delay(500);
    lcv++;
    if (lcv >= 300){
      break;
    }/* if(lcv) */
    clientUDP.beginPacket(serverIP,serverPort);
    clientUDP.write(udpBuffer);
    clientUDP.endPacket();
    packetSize = clientUDP.parsePacket();
  }/* while(packetSize) */

  if (packetSize){
    clientUDP.read(udpResponse,25);    
  }
}/* sendData()*/

/*
 * strContains:
 *            returns: 1 - if the string is found
 */
int strContains(char *str, char *sfind){
  int found = 0;
  int index = 0;
  int len =strlen(str);

  if (strlen(sfind) > len){
    return 0;
  }

  while (index < len){
    if (str[index] == sfind[found]){
      found++;
      if (strlen(sfind) == found){
        return 1;
      }
    } else {
      found = 0;
    }
    index++;
  }
}/* strContains() */


/*
 * Sends a detailed email to the user if an error occurs
 * Returns:
 *          0 - if the email was sent successful
 *          1 - if sending the email was failed
 */
void checkStatus(){
  emailBuffer = "Module ID: ";
  emailBuffer += MODULE_NUMBER;
  emailBuffer += "\n Current Status of Sensors \n ";  
  String temp = "";

  // update status of the temperature sensor
  if (tempSensor > maxTemp){    
    emailBuffer += "ALERT: The water temperature ";
    emailBuffer += tempSensor;
    emailBuffer += " F is currently over the limit\n\n";   
  } else if (tempSensor < minTemp){
    tempStatus = "under the limit";    
    emailBuffer += "ALERT: The water temperature ";
    emailBuffer += tempSensor;
    emailBuffer += " F is currently under the limit\n\n";
  } else {}/* if() */ 

  // check if ec value is within limits
  if (ecSensor > maxEC){    
    emailBuffer += "ALERT: EC value ";
    emailBuffer += ecSensor;
    emailBuffer += " ppm is currently over the limit\n\n"; 
  } else if (ecSensor < minEC){    
    emailBuffer += "ALERT: EC value ";
    emailBuffer += ecSensor;
    emailBuffer += " ppm is currently under the limit\n\n"; 
  } else { } /* if(ecSensor) */

  if (DO2Sensor > maxDO2){
    emailBuffer += "ALERT: Dissolved Oxygen value ";
    emailBuffer += DO2Sensor;
    emailBuffer += " mg/L is currently over the limit\n\n"; 
  } else if (DO2Sensor < minDO2){    
    emailBuffer += "ALERT: Dissolved Oxygen value ";
    emailBuffer += DO2Sensor;
    emailBuffer += " mg/L is currently under the limit\n\n";     
  } else {}

  if (phSensor > maxPH){    
    emailBuffer += "ALERT: pH value ";
    emailBuffer += phSensor;
    emailBuffer += " is currently over the limit\n\n";
  } else if (phSensor < minPH){    
    emailBuffer += "ALERT: pH value ";
    emailBuffer += phSensor;
    emailBuffer += " is currently under the limit\n\n";
  } else { }

  if (levelSensor < minLevel){    
    emailBuffer += "ALERT: The water in the tank is very low\n\n ";    
  } else if ((levelSensor > minLevel) && (levelSensor <= (minLevel*1.15))){    
    emailBuffer += "WARNING: The water in the tank is getting low\n\n ";    
  } else { }

  if (flowSensor < minFlow){   
    emailBuffer += "ALERT: Flow value ";
    emailBuffer += flowSensor;
    emailBuffer += " gal/min is currently under the limit\n\n";    
  } else { }/* if (flowSensor) */

  
  // Send email to user
  if (sendEmail() ==  1){
    emailFlag = 0;    
  } else {
    emailFlag = 1;
  }/* if()*/
  
  if (levelStatus.equals("under the limit")){
    digitalWrite(A10,HIGH);                                     // turn off water pump and heaters
  } else {
    digitalWrite(A10,LOW);                                      // leave pump and heaters running
  }/*if (levelStatus)*/
  
} /* updateStatus()*/


/*
 * Send an email using a SMTP server
 */
byte sendEmail(){
  byte thisByte = 0;
  byte respCode;

  // Connect to email server
  if (emailClient.connect(emailServer, emailPort) == 1){
    Serial.println(("connected"));
  } else {
    Serial.println(("connection failed"));
    return 0;
  }/*if()*/

  if (!emailRcv()){
    return 0;
  }/*if()*/

  // Auth login
  Serial.println(("Sending auth login"));
  emailClient.println("auth login");
  if (!emailRcv()){
    return 0;
  }/*if()*/
  
  // username
  Serial.println(("Sending user"));
  // change to base64 encoded user
  emailClient.println("amFzcGVlZHg3ODVAZ21haWwuY29t");
  
  if (!emailRcv()){
    return 0;
  }/*if()*/

  //password
  Serial.println(("Sending password"));
  // base64 encode password
  emailClient.println("QXJkdWlub0VtYWlsVGVzdGluZw==");

  if (!emailRcv()){
    return 0;
  }/*if()*/

  // Sender's email
  Serial.println("Sending From");
  emailClient.println("MAIL From: <jasspeedx785@gmail.com>");

  if (!emailRcv()){
    return 0;
  }/*if()*/

  // recipient address
  Serial.println("Sending To");
  emailClient.println("RCPT To: <jra8788@rit.edu>");

  if (!emailRcv()){
    return 0;
  }/*if()*/

  // DATA
  Serial.println("Data");
  emailClient.println("DATA");

  // Sending the email
  Serial.println(("Sending email"));
  
  // recipuent's address
  emailClient.println("To: <jra8788@rit.edu>");
  
  // Sender's address
  emailClient.println("From: <jaspeedx785@gmail.com>");

  // mail subject
  emailClient.println("Subject: Arduino Email Test");

  // email body
  emailClient.println(emailBuffer);

  
  emailClient.println(".");

  if (!emailRcv()){
    return 0;
  }/*if()*/

  Serial.println(("Sending QUIT"));
  
  emailClient.println("QUIT");
  
  if (!emailRcv()){
    return 0;
  }/*if()*/
  // disconnect from server
  emailClient.stop();

  Serial.println(("disconnected"));

  return 1;
}/* sendEmail()*/

/*
 * Verify that data was sent to email server 
 */
byte emailRcv(){
  byte responseCode;
  byte thisByte;
  int loopCount = 0;

  // if no internet connection
  while (!emailClient.available()){
    delay(1);
    loopCount++;

    // if nothing received for 30 seconds, timeout
    if(loopCount > 30000){
      emailClient.stop();
      Serial.println(("\r\nTimeout"));
      return 0;
    }/*if()*/
  }/*while()*/

  responseCode = emailClient.peek();

  while(emailClient.available()){
    thisByte = emailClient.read();
    Serial.write(thisByte);
  }/*while()*/

  if (responseCode >='4'){
    emailFail();
    return 0;
  }/*if()*/
  return 1;
}/*emailRcv()*/

/*
 * Failed to send email 
 */
void emailFail(){
  byte thisByte = 0;
  int loopCount = 0;

  // stop sending the email
  emailClient.println(("QUIT"));

  while(!emailClient.available()){
    delay(1);
    loopCount++;

    // if nothing is received  for 30 seconds, timeout
    if (loopCount > 30000){
      emailClient.stop();
      Serial.println(("\r\n Timeout"));
      return;
    }/*if()*/
  }/*while()*/

  while(emailClient.available()){
    thisByte = emailClient.read();
    Serial.write(thisByte);
  }/*while()*/

  //disconnect from the server
  emailClient.stop();

  Serial.println(("disconnected"));
}/*emailFail()*/

/*
 * Write the sensor data to a text file
 * Returns
 *        0 - if the data was succesfully written to the file
 *        1 - if the file was not found
 */
int logData(int measNum){  
  File dataFile = SD.open("logdata.txt", FILE_WRITE);  

  // if the file is available write to it
  if (dataFile) { 
    if (measNum == 0){
      dataFile.println("Time(min),Temperature(F),E.C. (ppm),D.O.(mg/L),pH,Flow(gpm)");  
    }
    dataFile.println(dataString);    
    dataFile.close();                                         // close the file    
    Serial.println("file opened"); 
    return 0; 
  } else {            // if the file isn't open return error
    Serial.println("error opening file");
    return 1;
  }/* if() */
  
}/* logData() */

