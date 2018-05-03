/*
 * Server Module
 * 
 * The microcontroller reads the values of multiple sensors via analog or digital pins
 * and displays the recorded values on a web page using an Arduino Ethernet Shield.
 * If there is no ethernet connection, the data is stored in the SD Card on the ethernet
 * shield.
 * 
 * Microcontroller:
 * 
 * ** Arduino Mega ADK R3
 * ** Arduino Ethernet Shield 2
 * 
 * Sensors
 * 
 * ** Water Temperature Sensor: Waterproof DS18B20
 * ** Liquid Flow Sensor: Liquid Flow Meter - Plastic 1/2" NPS Threaded
 * ** Water Level Sensor: 
 * ** Dissolved Oxygen Sensor: Atlas Scientific sensor
 * ** pH Sensor: Atlas Scientific sensor
 * ** Electrical Conductivity Sensor: Homemade EC SEnsor
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

// libraries
#include <SPI.h>                        // library for seriable communication
#include <OneWire.h>                    // library for temperature and ec sensor
#include <Ethernet2.h>                  // Ethernet library for ethernet shield 2
#include <EthernetUdp2.h>               // library for udp messages
#include <SD.h>                         // read/write from/to sd card
#include <Wire.h>                       // Enable I2C (PH and DO2 sensors)


#define MAX_NUM 10
// ---------------------------------------------------------------------------------------------------------

#define DO2_address 97                  // default I2C ID number for EZO D.O. Circuit
#define pH_address 99                   // default I2C ID number for EZO pH circuit


// Module identifier
String MODULE_NUMBER = "A.0";

int numModules = 1;

// MAC address for the controller
byte mac[] = {
  0x90, 0xA2, 0xDA, 0x11, 0x19, 0x19
};

//byte mac[] = {
//  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
//};

int packetSize = 0;

//array structure to hold sensor data
// modInfo{modules (mod_ID,mod_IP,mod_Port)}
String modInfo[MAX_NUM][3];
// modData{modules{Sensors{value,status,minValue,maxValue}}}
String modData[MAX_NUM][6][4];
/* Ethernet  variables */
// static ip address
IPAddress ip(129,21,61,177);

EthernetServer server(80);                          // server variable for website

EthernetClient emailClient;                         // ethernet client for email

String emailBuffer = "";                            // body content of the email

int emailFlag = 0;

char emailServer[] = "mail.smtp2go.com";            // SMTP server to send email

int emailPort = 2525;                               // port for the email server

EthernetUDP serverUDP;                              // server variable for udp communication

unsigned int localPort = 8080;                      // local port for UDP

char udpBuffer[100];                                // buffer to hold incoming packet
// -------------------------------------------------------------------------------------------------------------------

/* Variables for water temperature sensor */
//int ONE_WIRE_BUS_TEMP_SENSOR = A4;                  // digital pin for temperature sensor
OneWire tempWire(A4);                                 // Setup a OneWire instance for the temperature sensor

float tempSensor = 0.0;                             // variable to hold sensor data
double maxTemp = 85;                                // maximum solution temperature for the system
double minTemp = 77;                                // minimum solution temperature for the system
String tempStatus = "OK";                                 // current status of the temperature

/* Variables for EC Sensor */
//float CalibrationEC = 3;                            // EC value of calibration solution in s/cm
double maxEC = 2300;                                // maximum limit for the E.C.
double minEC = 1500;                                // minimum limit for the E.C.
String ecStatus = "OK";                                   // current status of the E.C.
int ECPin = A2;                                     // Analog pin for E.C. data line
int ECPwr = A1;                                     // Analog pin to provide power to E.C. sensor

// The value below is dependent on the chemical solution
float TempCoef = 0.019;                             // 0.019 is considered the standard for plant nutrients

float KValue = 0.07;

//int RA = 1025;
//int RB = 25; // Resistance of powering pins
float EC = 0;
float EC25 = 0;
double ecSensor = 0.0;
float raw = 0;
float Vin = 5;
float Vdrop = 0;
float RC = 0;
float buf = 0;
float PPMconversion = 0.5;                          // converting to ppm (US)

/* Variables for the Dissolved Oxygen sensor */
float DO2Sensor = 0.0;                              // variable to hold the sensor data
double maxDO2 = 8.5;                                // maximum dissolved oxygen in the system
double minDO2 = 7;                                  // minimum dissolved oxygen in the system
String do2Status ="OK";                                  // current status of D.O.

/* Variables for pH sensor */
float phSensor = 0.0;                               // variable to hold the sensor data
double maxPH = 7;                                   // maximum pH in the system
double minPH = 4.8;                                 // minimum pH in the system
String phStatus = "OK";                                   // current status of the pH

/* Variable for water flow sensor */
int LIQUIDFLOWPIN = A0;                             // digital pin for flow sensor
volatile uint16_t flow_pulses = 0;                  // number of pulses
volatile uint16_t flowPrevState;                    // track the state of the flow pin
volatile uint32_t flowratetimer = 0;                // time between pulses
volatile float flowSensor = 0.0;                    // variable to hold sensor data
double maxFlow = 1;                                 // maximum water flow in the system
double minFlow = 0.5;                               // minimum water flow in the system
String flowStatus = "OK";                                 // current status of the flow
volatile float flowrate;

/* Variables for water level sensor */
int TRIGGER_PIN = A13;                              //
int ECHO_PIN = A14;                                 //
int levelGND = A15;                                 //
int levelPWR = A12;                                 //
double levelSensor = 0.0;                           // variable to hold sensor data
double minLevel = 30;                                // minimum water level in the system
double maxLevel = 50;                               // maximum water level (tank height)
String levelStatus = "OK";                          // current status of the level



/* variable for sensor testing */
bool testing = true;                                // Enable/Disable ethernet connection
bool mult_client = false;                           // Enable/Disable udp communication
int measNum = 0;

String dataString = "";                             // formatting string to write data to SD card

int doTime = 2400;

int sTime = 30;

int phTime = 3000;

int ecTime = 3600;

int timeout = 0;

String labels[] = {"Temperature ","Electrical Conductivity ","Dissolved Oxygen ","pH ","Level ","Flow "};

/*--------------------------------------------------------------------------Functions--------------------------------------------------------------------------*/

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
  pinMode(A5,OUTPUT);                             // temperature ground pin  
  digitalWrite(A5,LOW);                           // set output pin to ground
  digitalWrite(A3,HIGH);                          // set output pin to 5V 

//  delay(1000);                                    // wait for 1000 ms

  // EC sensor
  pinMode(ECPin,INPUT);                           // EC sensor data pin
  pinMode(ECPwr,OUTPUT);                          // EC sensor power pin

  delay(1000);                                    // wait for 1000 ms
  
  // D.O. and pH sensor
  Wire.begin();                                   // enable I2C port

//  delay(1000);                                    // wait for 1000 ms
  
  /* Initialize & Setup water level sensor sensor */
  pinMode(levelGND,OUTPUT);                       // level sensor ground pin
  digitalWrite(levelGND,LOW);                     // set output pin to ground
  pinMode(levelPWR,OUTPUT);                       // level sensor power pin
  digitalWrite(levelPWR,HIGH);                    // set output pin to 5V
  pinMode(ECHO_PIN, INPUT);                       // level sensor echo pin
  pinMode(TRIGGER_PIN, OUTPUT);                   // level sensor trigger pin
  digitalWrite(ECHO_PIN,HIGH);                    // set output pin to 5V

//  delay(1000);                                    // wait for 1000 ms

  /* Initialize & Setup Liquid flow sensor */
  pinMode(LIQUIDFLOWPIN, INPUT);                  // set flow pin as input  
  digitalWrite(LIQUIDFLOWPIN, HIGH);              // set the flow pin high
  flowPrevState = digitalRead(LIQUIDFLOWPIN);     
  useInterrupt(true);                             // initialize interrupt function

//  delay(1000);                                    // wait for 1000 ms

  pinMode(A9,OUTPUT);                             // relay ground input signal
  digitalWrite(A9,LOW);                           // set output signal to ground
  pinMode(A10,OUTPUT);                            // relay 5V input signal
  digitalWrite(A10,LOW);                          // set output signal to ground
  
}/* InitSensors() */

/*
 * Initialize data array to empty strings
 */
void initArray(){  

  modInfo[0][0] = "A.0";
  modInfo[0][2] = String(Ethernet.localIP());
  modInfo[0][3] = String(localPort);  

  // initialize array to empty string
  for (int mod =0; mod < MAX_NUM; mod++){
    for (int ss =0; ss < 6; ss++){
      for (int dt=0; dt <4; dt++){
        modData[mod][ss][dt] = "";
      }/* for(dt)*/
    }/* for(ss)*/
  }/*for(mod)*/
}/* initArray()*/

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
  Serial.begin(9600);                               // enable serial port
  Serial.println("intializing SD card");
  if (!SD.begin(4)){                                // See if the card is present and can be intialized
    Serial.println("Card failed, or not present");  // SD card could not be initialized  
    return;                                         // end setup operations
  }/*if()*/

  Serial.println("SD card initialized");            

  if (!SD.exists("index.htm")){                     // check if webfile exists
    Serial.println("ERROR: can't find home.html");  // file does not exist
    return;                                         // end setup operations
  }
  Serial.println("Found index.html file");

  Ethernet.begin(mac);

  if (Ethernet.begin(mac)==0){
    Serial.println("Failed to configure Ethernet using DHCP");
    Ethernet.begin(mac,ip);
  }
  server.begin();                                   // start server
  Serial.println("Web Server is at ");
  Serial.println(Ethernet.localIP());
  delay(1000);
  
//  serverUDP.begin(localPort);                       // start UDP server

  initArray();                                      // initialize arrays
  
  initSensors();                                    // initializes sensors

  Serial.println("Initialization done.");
  
  delay(10000);                                      // wait for 10 s  

}/*setup()*/


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
  
  delay(1000);                                             // wait for 1s
  
  return TemperatureSum;
}/* getTemp() */

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

  levelSensor /= 1;                     // take average
//  levelSensor = 46 - levelSensor;       // height of the tank is ~46 cm  
  
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
  
}/* getFlow() */

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
  
  delay(10000);                                // wait for 10 seconds
} /* getPH() */

/*
 * Get EC value
 */
void getEC(){
     
  float temp  = getTemp();                // read the temperature of the solution
  delay(1000);                            // wait for 1000 ms
  temp = getTemp();                       // read temperature of the water

  // Estimate resistance of liquid
  digitalWrite(ECPwr,HIGH);
  raw = analogRead(ECPin);
  raw = analogRead(ECPin);    // First reading will be low
  digitalWrite(ECPwr,LOW);

  // Converts to EC
  Vdrop = (Vin*raw)/1024.0;
  RC = (Vdrop*1025)/(Vin-Vdrop);
  RC = RC - 25;
  EC = 1000/(RC*KValue);

  // Compensating for Temperature
  EC25 = EC/(1+TempCoef*(temp - 25.0));
  ecSensor = (EC25) * (PPMconversion*1000);  
    
  delay(10000);                            // wait for 10 seconds
}/* getEC() */


/*
 * Main function of the monitoring system
 */
void loop() {
  dataString = "";  
  int lcv = 0;                                        // loop counter variable

//  packetSize = serverUDP.parsePacket();                       // check if data has been sent to the server
//
//  if (packetSize){                                            // if a packet has been received    
//    serverUDP.read(udpBuffer,100);                            // read UDP data into buffer
//    getModData(serverUDP.remoteIP(), serverUDP.remotePort()); // format data received
//    Serial.println("received");                               // print received to terminal
//  }/* if (packetSize) */
  
  
  // get sensor data
  if (ecTime == 3600){           // every 8 hours
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

  if (sTime == 30){               // every 30 mins
    getTemp();                    // get the temperature
    getLevel();                   // get the water level
    getFlow();                    // get the water flow
    sTime = 0;                    // reset the counter
//    Serial.println(dataString);
  }/* if() */
  
  /* save data to data string */
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

  // write data to file
  if (timeout == 93600){          // every 24 hours
    logData(measNum);
  } 
  Serial.println(dataString); 
  

  //add data to modData array
  checkStatus();
  
  modData[0][0][0] = tempSensor;                              // temperature value
  modData[0][0][0] += " &#8457;";                             // temperature units
  modData[0][0][1] = tempStatus;                              // temperature status
  modData[0][0][2] = minTemp;                                 // temperature minimum limit
  modData[0][0][2] += " &#8457;";                             // temperature units
  modData[0][0][3] = maxTemp;                                 // temperature maximum limit
  modData[0][0][3] += " &#8457;";                             // temperature units

  modData[0][1][0] = ecSensor;                                // E.C. value
  modData[0][1][0] += " ppm";                                 // E.C. units
  modData[0][1][1] = ecStatus;                                // E.C. status
  modData[0][1][2] = minEC;                                   // E.C. minimum limit
  modData[0][1][2] += " ppm";                                 // E.C. units
  modData[0][1][3] = maxEC;                                   // E.C. maximum limit
  modData[0][1][3] += " ppm";                                 // E.C. units

  modData[0][2][0] = DO2Sensor;                               // D.O. value
  modData[0][2][0] += " mg/L";                                // D.O. units
  modData[0][2][1] = do2Status;                              // D.O. status
  modData[0][2][2] = minDO2;                                 // D.O. minimum limit
  modData[0][2][2] += " mg/L";                                // D.O. units
  modData[0][2][3] = maxDO2;                                  // D.O. maximum limit
  modData[0][2][3] += " mg/L";                                // D.O. units

  modData[0][3][0] = phSensor;                                // pH value
  modData[0][3][1] = phStatus;                                // pH status
  modData[0][3][2] = minPH;                                   // pH minimum limit
  modData[0][3][3] = maxPH;                                   // pH maximum limit

  modData[0][4][0] = levelSensor;                             // level value
  modData[0][4][0] += " cm";                                // level units
  modData[0][4][1] = levelStatus;                             // level status
  modData[0][4][2] = minLevel;                                // level minimum limit
  modData[0][4][2] += " cm";                                // level units
  modData[0][4][3] = maxLevel;                                // level maximum limit
  modData[0][4][3] += " cm";                                // level units

  modData[0][5][0] = flowSensor;                              // flow value
  modData[0][5][0] += " gal/min";                             // flow units
  modData[0][5][1] = flowStatus;                              // flow status
  modData[0][5][2] = minFlow;                                 // flow minimum value
  modData[0][5][2] += " gal/min";                             // flow units
  modData[0][5][3] = maxFlow;                                 // flow maximum value
  modData[0][5][3] += " gal/min";                             // flow units 
  

  // update website data
  EthernetClient client = server.available();                 // listen for incoming clients  
  if (client){                                                // if the server is available
    Serial.println("Connection Successful");                // 
    while (client.connected()){                               // client is connected      
      
      if (client.available()){                                // client is avaliable
        char c = client.read();                               //  request from web page
        
        Serial.print(c);                                      // print request to terminal
        
        if (c == '\n'){                                       // end of HTTP request
          client.println("HTTP/1.1 200 OK");                  // print HTTP header
          client.println("Content-Type: text/html");          //
          client.println("Refresh: 30");              // keep the system connected to the website
          client.println();                                   //
          displayData(client);                                // display sensor data to website        
          break;                                              // end the loop
        }/* if()*/
      }/* if() */
    }/* while() */
    delay(1);                                                 //
    client.stop();                                            // stop connection to server
    Serial.println("client disconnected");                    //
  } else {                                                    
    Serial.println("no connection");                          //
  }      
  delay(1000);                                                // wait for 1 second

  // increment timers
  timeout++;
  ecTime++;
  phTime++;
  doTime++;
  sTime++;

}/*loop()*/

/*
 * Update website data
 */
void displayData(EthernetClient client){  
  // open html file
  File webFile = SD.open("index.htm");
  // read the file
  while (webFile.available()){
    client.write(webFile.read());
  }/* while (webFile) */  
  // close the file after reading it
  webFile.close();   

  // write sensor data to 
  for (int lcv=0; lcv < numModules; lcv++){                            // for each modules

    client.println("<div class='module_data'><h2>Module ID: ");    
    client.print(modInfo[lcv][0]);
    client.print("</h2><table>");
    client.println("<tr><th><b>Sensor</b></th><th><b>Value</b></th><th><b>Status</b></th><th><b>Lower Limit</b></th><th><b>Upper Limit</b></th></tr>");

    /* write sensor data */
    for (int yy =0; yy < 6; yy++){                                        // for each sensor
      client.println("<tr><td><b>");      
      client.println(labels[yy]);
      client.println("</b></td>");
      for (int xx=0; xx <4; xx++){                                        // for each field
        client.println("<td>");
        client.println(modData[lcv][yy][xx]);        
        client.println("</td>");
      } /* for(xx) */
      client.println("</tr>");
    }/* for(yy) */
    client.println("</table></div>");
  }/* for(lcv) */

  client.println("</body></html>");
}/* displayData()*/



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
    tempStatus = "over the limit";
    emailBuffer += "ALERT: The water temperature ";
    emailBuffer += tempSensor;
    emailBuffer += " F is currently over the limit\n\n";   
  } else if (tempSensor < minTemp){
    tempStatus = "under the limit";    
    emailBuffer += "ALERT: The water temperature ";
    emailBuffer += tempSensor;
    emailBuffer += " F is currently under the limit\n\n";
  } else {
    tempStatus = "OK";    
  }/* if() */ 

  // check if ec value is within limits
  if (ecSensor > maxEC){
    ecStatus = "over the limit";
    emailBuffer += "ALERT: EC value ";
    emailBuffer += ecSensor;
    emailBuffer += " ppm is currently over the limit\n\n"; 
  } else if (ecSensor < minEC){
    ecStatus = "under the limit";
    emailBuffer += "ALERT: EC value ";
    emailBuffer += ecSensor;
    emailBuffer += " ppm is currently under the limit\n\n"; 
  } else {
    ecStatus = "OK";
  } /* if(ecSensor) */

  if (DO2Sensor > maxDO2){                    
    do2Status = "Over the limit";
    emailBuffer += "ALERT: Dissolved Oxygen value ";
    emailBuffer += DO2Sensor;
    emailBuffer += " mg/L is currently over the limit\n\n"; 
  } else if (DO2Sensor < minDO2){
    do2Status = "under the limit";
    emailBuffer += "ALERT: Dissolved Oxygen value ";
    emailBuffer += DO2Sensor;
    emailBuffer += " mg/L is currently under the limit\n\n";     
  } else {
    do2Status = "OK";
  }

  if (phSensor > maxPH){
    phStatus = "over the limit";
    emailBuffer += "ALERT: pH value ";
    emailBuffer += phSensor;
    emailBuffer += " is currently over the limit\n\n";
  } else if (phSensor < minPH){
    phStatus = "under the limit";
    emailBuffer += "ALERT: pH value ";
    emailBuffer += phSensor;
    emailBuffer += " is currently under the limit\n\n";
  } else {
    phStatus = "OK";
  }

  if (levelSensor < minLevel){
    levelStatus = "Over the limit";
    emailBuffer += "ALERT: The water in the tank is very low\n\n ";    
  } else if ((levelSensor > minLevel) && (levelSensor <= (minLevel*1.15))){
    levelStatus = "OK";
    emailBuffer += "WARNING: The water in the tank is getting low\n\n ";    
  }
  else {
    levelStatus = "OK";
    
  }

  if (flowSensor < minFlow){
    flowStatus = "under the limit";
    emailBuffer += "ALERT: Flow value ";
    emailBuffer += flowSensor;
    emailBuffer += " gal/min is currently under the limit\n\n";    
  } else {
    flowStatus = "OK";
  }/* if (flowSensor) */

  
  // Send email to user
//  if (sendEmail() ==  1){
//    emailFlag = 0;    
//  } else {
//    emailFlag = 1;
//  }/* if()*/
  
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


/*
 * Format data received 
 */
void getModData(IPAddress ip, int port){
  /* find the index of the  data */
  int index = -1;
  String dt = "";  
  int cnt = 0;
  String modPort = String(port);
  String tmp[25];

  String modMAC = "";  

  /* check if it is a new client module */
  for (int lcv=0; lcv < numModules; lcv++){
    if (modInfo[lcv][2].equals(modPort)){
      index = lcv;
    }
  }  

  /* format received data */
  for (int lcv = 0; lcv < packetSize; lcv++){
    if (udpBuffer[lcv] == ','){
      tmp[cnt] = dt;
      cnt++; 
      dt = "";     
    }else {
      dt += udpBuffer[lcv];
    }/* if() */
  }/* for() */

  int lcv = 1;  
  if (index == -1){                                 // if new module
    if (numModules < MAX_NUM){                      // if number of modules has not been exceeded
      modInfo[numModules][0] = tmp[0];                // insert MAC address
      modInfo[numModules][1] = String(ip);            // insert ip address
      modInfo[numModules][2] = String(port);          // insert port
      modInfo[numModules][3] = "24";                  // add timeout value

      for (int ss=0; ss<6; ss++){                   // for each sensor
        for (int val = 0; val < 4; val++){          // for each field
          if (val == 1){
            if (tmp[lcv].equals("0")){
              modData[numModules][ss][val] = "under limit";
            } else if (tmp[lcv].equals("1")){
              modData[numModules][ss][val] = "OK";
            } else {
              modData[numModules][ss][val] = "over limit";
            }
          } else {
            modData[numModules][ss][val] = tmp[lcv];  // insert data
          }/* if(val)*/          
          lcv++;                                    // increment index
        }/*for (val) */
      }/*for (ss) */
      numModules++;                                 // increment number of modules
    }/* if *numModules) */    
  } else {                                          // update data
    for (int ss=0; ss<6; ss++){                     // for each sensor
      for (int val = 0; val < 4; val++){            // for each field
        modData[index][ss][val] = tmp[lcv];         // insert data
        lcv++;                                      // increment index
      }/* for(val) */
    }/* for (ss)  */
  }/* if (index)*/  

  /* send ack message */
  serverUDP.beginPacket(ip,port);
  serverUDP.write("ack");
  serverUDP.endPacket();  
}/* getModData */

