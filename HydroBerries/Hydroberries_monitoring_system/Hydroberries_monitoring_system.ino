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
 * ** Arduino Ethernet Shield 2
 * 
 * Sensors
 * 
 * ** Water Temperature Sensor: Waterproof DS18B20
 * ** Liquid Flow Sensor: Liquid Flow Meter - Plastic 1/2" NPS Threaded
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
#include <SPI.h>                        // library for seriable communication
#include <OneWire.h>                    // library for temperature and ec sensor
#include <DallasTemperature.h>          // library for temperature sensor
#include <Ethernet2.h>                  // Ethernet library for ethernet shield 2
#include <EthernetUdp2.h>               // library for udp messages
#include <SD.h>                         // read/write from/to sd card
#include <Wire.h>                       // Enable I2C (PH and DO2 sensors)


#define DO2_address 97                  // default I2C ID number for EZO D.O. Circuit
#define pH_address 99
/* Digital Pin declaration */
#define ONE_WIRE_BUS_TEMP_SENSOR 9      // digital pin for temperature sensor
#define LIQUIDFLOWPIN 52               // digital pin for flow sensor
#define ECHO_PIN  12                    // digital pin for water level: echo
#define TRIGGER_PIN 11                  // digital pin for water level: trigger

int MAX_TIME = 2;

// Module identifier
String MODULE_NUMBER = "A";

int numModules = 1;

// MAC address for the controller
byte mac[] = {
  0x90, 0xA2, 0xDA, 0x11, 0x19, 0x19
};

// static ip address
IPAddress ip(129,21,63,177);

EthernetClient emailClient;                         // ethernet client for email

String emailBuffer;                                 // body content of the email

char emailServer[] = "mail.smtp2go.com";            // SMTP server to send email

int emailPort = 2525;                               // port for the email server

File moduleConfig;

float numMinutes = 0;                               // number of minutes since the controller started

const int chipSelect = 4;

EthernetUDP ServerUdp;                              // server variable for udp communication

unsigned int localPort = 8080;                      // local port for UDP

EthernetServer server(80);                          // server variable for website

char udpBuffer[UDP_TX_PACKET_MAX_SIZE];             // buffer to hold incoming packet

/* Variables for water temperature sensor */
OneWire TempSensor(ONE_WIRE_BUS_TEMP_SENSOR);       // Setup a OneWire instance for the temperature sensor

DallasTemperature temperature_sensor(&TempSensor);  // Pass OneWire reference to Dallas Temperature

float LiquidTempSensor = 0.0;                       // variable to hold sensor data
double MaxLiquidTemp = 88;                          // maximum solution temperature for the system
double MinLiquidTemp = 77;                          // minimum solution temperature for the system
String tempStatus = "within limits" ;               // current status of the temperature
int tempPWR = 6;
int tempGND = 3;

/* Variable for water flow sensor */
volatile uint16_t flow_pulses = 0;                  // number of pulses
volatile uint16_t flowPrevState;                    // track the state of the flow pin
volatile uint32_t flowratetimer = 0;                // time between pulses
volatile float LiquidFlowSensor = 0.0;              // variable to hold sensor data
double MaxLiquidFlow = 30;                          // maximum water flow in the system
double MinLiquidFlow = 1;                           // minimum water flow in the system
String flowStatus = "within limits";                // current status of the flow


/* Variables for water level sensor */

double LiquidLevelSensor = 0.0;                     // variable to hold sensor data
double MaxLiquidLevel;                              // maximum water level in the system
double MinLiquidLevel;                              // minimum water level in the system
String levelStatus = "within limits";               // current status of the level
int levelPWR = 10;
int levelGND = 13;
/* Variables for the Dissolved Oxygen sensor */
float DO2Sensor = 0.0;                              // variable to hold the sensor data
double MaxDO2;                                      // maximum dissolved oxygen in the system
double MinDO2;                                      // minimum dissolved oxygen in the system
String do2Status = "within limits";                 // current status of D.O.

/* Variables for pH sensor */
float phSensor = 0.0;                               // variable to hold the sensor data
double MaxpH;                                       // maximum pH in the system
double MinpH;                                       // minimum pH in the system
String phStatus = "within limits";                  // current status of the pH

/* Variables for homemade EC sensor */
float CalibrationEC = 2.9;                          // EC value of calibration solution in s/cm
float TempCoef = 0.019;                             // 0.019 is considered the standard for plan nutrients
float KValue = 0;                                   // calibration variable
int ECSensorPin = A0;                               // analog pin for ec sensor
int ECSensorPWR = A4;                               // analog pin to power the ec sensor
int RA = 1000;                                      // pull-up resistor for the ec sensor
int RB = 25;                                        // Resistance of powering pins
float EC = 0;
float EC25 = 0;
float ECSensor = 0.0;                               // variable to hold sensor data
float raw = 0;
float ECVin = 5;                                    // input voltage
float ECVdrop = 0;                                  // voltage drop
float RC = 0;                                       // resistance of solution
float ECbuf = 0;                                    // buffer to hold ec values
float PPMconversion = 0.5;                          // converting to ppm (US)
String ecStatus = "within limits";                  // current status of ec sensor



/* variable for sensor testing */
bool testing = true;                                // Enable/Disable ethernet connection
bool mult_client = false;                           // Enable/Disable udp communication
int measNum = 0;


/*
 * Interrupt service routine
 * calculate the flow rate of the system
 */
SIGNAL(TIMER0_COMPA_vect){
  uint8_t currState = digitalRead(LIQUIDFLOWPIN);
  float liters;

  numMinutes += 0.001;
  if (currState == flowPrevState){
    flowratetimer++;
    return;                                             // nothing changed
  } /* if () */

  if (currState == HIGH){                               // low to high transition
    flow_pulses++;
  }/* if () */

  flowPrevState = currState;
  LiquidFlowSensor = 1000.0;
  LiquidFlowSensor /= flowratetimer;                    // in Hz
  liters = flow_pulses / (7.5*60);
  LiquidFlowSensor = LiquidFlowSensor* liters;
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
void InitSensors(){
  /* Setup Liquid temperature sensor */
  pinMode(tempPWR,OUTPUT);                // set the pin as output
  pinMode(tempGND,OUTPUT);                // set the pin as output
  digitalWrite(tempGND,LOW);              
  digitalWrite(tempPWR, HIGH);
  temperature_sensor.begin();             // initialize temperature sensor
  delay(100);                             // wait for 100 ms

  /* Setup Liquid level sensor */ 
  pinMode(levelGND, OUTPUT);
  digitalWrite(levelGND, LOW);
  pinMode(levelPWR, OUTPUT);
  digitalWrite(levelPWR, HIGH); 
  pinMode(ECHO_PIN, INPUT);
  pinMode(TRIGGER_PIN, OUTPUT);
  digitalWrite(ECHO_PIN,HIGH);  
  delay(100);                             // wait for 100 ms

  /* Setup EC Sensor */
  pinMode(ECSensorPin, INPUT);
  pinMode(ECSensorPWR, OUTPUT);
  RA = RA + RB;
  delay(1000);
  
  /* Setup Liquid flow sensor */
  pinMode(LIQUIDFLOWPIN, INPUT);                  // set flow pin as input
  digitalWrite(LIQUIDFLOWPIN, HIGH);              // set the flow pin high
  flowPrevState = digitalRead(LIQUIDFLOWPIN);     
  useInterrupt(true);                             // initialize interrupt function
  delay(100);
  
  
}/* InitSensors() */


/*
 * Calibrate the electrical conductivity sensor
 */
boolean setKValue(){
  int lcv = 1;            // loop counter variable
  float StartTemp = 0;    // Starting temperature of the solution
  float FinishTemp = 0;   // Ending temperature of the solution
  ECbuf = 0;
  bool calibrated = false;
  

  temperature_sensor.requestTemperatures();             // get the current temperature of the solution
  delay(1000);
  temperature_sensor.requestTemperatures();             // get the current temperature of the solution
  StartTemp = temperature_sensor.getTempCByIndex(0);    // starting temperature
  Serial.print("Starting Temperature: ");
  Serial.println(StartTemp);

  while (lcv <= 10){
    digitalWrite(ECSensorPWR,HIGH);
    raw = analogRead(ECSensorPin);    // first reading will be low
    raw = analogRead(ECSensorPin);
    digitalWrite(ECSensorPWR, LOW);
    ECbuf = ECbuf + raw;
    lcv++;                            // increade loop counter variable
    delay(5000);                      // wait for 5 seconds
  }/*while()*/
  raw = (ECbuf/10);

  temperature_sensor.requestTemperatures();             // get the current temperature of the solution
  FinishTemp = temperature_sensor.getTempCByIndex(0);   // final temperature of the solution
  Serial.print("Final Temperature: ");
  Serial.println(FinishTemp);

  EC = CalibrationEC * (1 + (TempCoef * ( FinishTemp - 25.0)));

  ECVdrop = (((ECVin) * (raw)) / 1024.0);
  RC = (ECVdrop * RA) / (ECVin - ECVdrop);
  RC = RC - RB;
  KValue = 1000 / (RC*EC);

  if ((StartTemp > (FinishTemp*0.9)) && (StartTemp < (FinishTemp*1.1))){
    calibrated = true;
  } else {
    calibrated = false;
  }/*if()*/
  return calibrated;
}/* setKValue() */

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
  Serial.println("intializing SD card");

  if (!SD.begin(chipSelect)){   // See if the card is present and can be intialized
    Serial.println("Card failed, or not present");
    return;
  }/*if()*/
  File dataFile = SD.open("logdata.txt",FILE_WRITE);
  dataFile.print("Module Number: ");
  dataFile.println(MODULE_NUMBER);
  dataFile.println("Measurement,Temperature(F),Level(cm),Flow(liters/sec),EC(ppm),D.O.(g/liters),pH");
  dataFile.close();
  Serial.println("SD card initialized");
  InitSensors();                // initializes sensors

  Wire.begin();                 // enable I2C port


  // Calibrate the EC sensor
  if (setKValue()== true){
    Serial.println("EC Calibration successful");                // Calibration successful
  } else {
    Serial.println("EC calibration failed");                    // calibration failed
  }/* if() */  
}/*setup()*/

/*
 * Read the temperature of the Liquid several times and return the average
 */
void getLiquidTemperature(){
  LiquidTempSensor = 0.0;                        // Initialize temperature sensor to zero.
  // read the temperature 10 times
  for (int lcv = 0; lcv < 10; lcv++){
    temperature_sensor.requestTemperatures();
    LiquidTempSensor += DallasTemperature::toFahrenheit(temperature_sensor.getTempCByIndex(0));
    delay(1000);
  }/* for() */
  
  LiquidTempSensor = LiquidTempSensor / 10;       // take average of the values

  delay(2000);         // wait for 2s
} /* gerLiquidTemperature() */

/*
 * Read the Liquid level of the tank
 */
void getLiquidLevel(){
  LiquidLevelSensor = 0.0;                       // Initialize Liquid level to zero
  float Level = 0.0;                            // variable to hold Liquid level value

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
    LiquidLevelSensor += (Level/58);
  }/*for()*/

  LiquidLevelSensor /= 10;                     // take average
}/* getLiquidLevel() */

/*
 * read electrical conductivity of solution in the tank
 */
void getEC(){
  ECSensor = 0.0;
  float ECtemp = 0;
  float temp = 0;
  for (int lcv=0; lcv<10; lcv++){
    temperature_sensor.requestTemperatures();   // current temperature
    temp = temperature_sensor.getTempCByIndex(0);
    digitalWrite(ECSensorPWR,HIGH);
    raw = analogRead(ECSensorPin);    // first reading will be low
    raw = analogRead(ECSensorPin);
    digitalWrite(ECSensorPWR, LOW);
    
    // Convert to EC
    ECVdrop = (ECVin*raw)/1024.0;
    RC = (ECVdrop*RA)/ (ECVin-ECVdrop);
    RC = RC - RB;
    EC = 1000/(RC*KValue);

    // Compensating for temperature
    EC25 = EC/(1+TempCoef*(temp - 25.0));
    ECtemp += EC25 * PPMconversion * 1000;

    delay(1000);                                    // wait for 1 second
  }/* for() */
  ECSensor = ECtemp / 10;
  delay(100);                                       // wait for 100 ms
}/* getEC() */

/*
 * 
 */
void getLiquidFlow(){
  delay(10000);       // wait for 10 seconds
}/* getLiquidFlow() */

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

  delay(5000);                                // wait for 5 seconds
} /* getPH() */


/*
 * Main function of the monitoring system
 */
void loop() {
  EthernetClient client = server.available();                    // listen for incoming clients
  int timing = 0;
  File file;
  String strData;
  
  getLiquidTemperature();                                        // get temperature of the solution
  getLiquidLevel();                                              // get level of solution in the tank
  getLiquidFlow();
  getDissolvedOxygen();                                          // level of D.O. in the solution
  getPH();
  getEC();

  Serial.print("Liquid Temperature: ");
  Serial.println(LiquidTempSensor);

  Serial.print("Liquid Level: ");
  Serial.println(LiquidLevelSensor);

  Serial.print("EC: ");
  Serial.println(ECSensor);

  Serial.print("Liquid Flow: ");
  Serial.println(LiquidFlowSensor);

  Serial.print("DO2: ");
  Serial.println(DO2Sensor);
  Serial.print("pH: ");
  Serial.println(phSensor);  
  
  if (true){                                   // get data every 30 mins    
    
    if (testing == true){                                                // write data to sd card
      measNum++;
      logData(measNum);            
    } else {                                                     // display data on website
      moduleConfig = SD.open("module_settings.txt");
      
      // try connection to server
      // timeout after 30 seconds
      while (!client){
        delay(1);
        timing++;
        if (timing > 30000){
          break;
        }/*if()*/
        client = server.available();
      }/*while()*/

      if (client){
        bool blankLine = true;
        while(client.connected()){
          Serial.println("Connection Successful");
          if (client.available()){
            char clientResponse = client.read();
            Serial.println(clientResponse);

            //HTTP request has ended
            if (clientResponse == '\n' && blankLine){
              // send a standar http response header
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/html");
              client.println("Refresh: keep-alive");              // keep the system connected to the website
              client.println();

              /* display sensor data on the website */
              client.println("<!DOCTYPE html>");
              client.println("<html lang='en'>");

              /* head */
              client.println("<head>");
              client.println("<title>Hydroponic Monitoring System</title>");
              client.println("<meta charset='utf-8'>");
              client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
            
              /* CSS */
              client.println("<style>");
              client.println("* { box-sizing: border-box;}");
              client.println("body { margin: 0;}");
              client.println(".header{ background-color: black; padding: 20px; text-align: center; color: white;}");
              client.print(" a { border: 1px solid lightseagreen; padding: 5px; color: lightseagreen: text-decoration;");
              client.print(" text-align: center; border-radius: 3px;}");
              /* create 2 equal columns that floats next to each other */
              client.println(".colum{ float: left; width: 50%; padding: 15px;}");
              /* clear floats after the columns */
              client.println(".row::after{ content:''; display:table; clear:both;}");
              /* Responsive layout - makes the column stack on top of each other instead of next to each other */
              client.println("@media (max-width:600px){ .column{ float: left; width: 100%; } }");
              client.println("</style>");

              client.println("</head>");

              /* body */
              client.println("<body>");
              client.println("<div class='header'>");
              client.println("<h1>Durgin Family Farms - Hydroponic Monitoring System</h1>");
              client.println("</div>");
              client.println("<div class='data'>");

              /* sensor data from module A */
              client.println("div class='modules'>");
              client.println("<label>");
              client.println("Module ");
              client.println(MODULE_NUMBER);
              client.println("</label>");
              
              // header row
              client.println("<table>");
              client.println("<tr>");
              client.println("<td><b>Sensor</b></td>");
              client.println("<td><b>Value</b></td>");
              client.println("<td><b>Units</b></td>");
              client.println("<td><b>Status</b></td>");
              client.println("</tr>");

              /* sensor values */
              /* Module A */

              // temperature
              client.println("<tr>");              
              client.println("<td><b>Temperature</b></td>");
              client.print("<td>");
              client.print(LiquidTempSensor);
              client.print("</td>");
              
              client.println("<td>");
              client.println("&#8457");
              client.println("</td>");
              
              client.println("<td>");
              client.println(tempStatus);
              client.println("</td>");
              client.println("</tr>");

              // Dissolved Oxygen
              client.println("<tr>");              
              client.println("<td><b>Dissolved Oxygen</b></td>");
              client.print("<td>");
              client.print(DO2Sensor);
              client.print("</td>");
              
              client.println("<td>");
              client.println("mg/L");
              client.println("</td>");
              
              client.println("<td>");
              client.println(do2Status);
              client.println("</td>");
              client.println("</tr>");

              // pH
              client.println("<tr>");              
              client.println("<td><b>pH</b></td>");
              client.print("<td>");
              client.print(phSensor);
              client.print("</td>");
              
              client.println("<td>");
              client.println("ppm");
              client.println("</td>");
              
              client.println("<td>");
              client.println(phStatus);
              client.println("</td>");
              client.println("</tr>");

              // Level
              client.println("<tr>");              
              client.println("<td><b>Level</b></td>");
              client.print("<td>");
              client.print(LiquidLevelSensor);
              client.print("</td>");
              
              client.println("<td>");
              client.println("cm");
              client.println("</td>");
              
              client.println("<td>");
              client.println(levelStatus);
              client.println("</td>");
              client.println("</tr>");

              // EC
              client.println("<tr>");              
              client.println("<td><b>Electrical Conductivity</b></td>");
              client.print("<td>");
              client.print(ECSensor);
              client.print("</td>");
              
              client.println("<td>");
              client.println("ppm");
              client.println("</td>");
              
              client.println("<td>");
              client.println(ecStatus);
              client.println("</td>");
              client.println("</tr>");

              // flow
              client.println("<tr>");              
              client.println("<td><b>Flow</b></td>");
              client.print("<td>");
              client.print(LiquidFlowSensor);
              client.print("</td>");
              
              client.println("<td>");
              client.println("liters/sec");
              client.println("</td>");
              
              client.println("<td>");
              client.println(flowStatus);
              client.println("</td>");
              client.println("</tr>");

              client.println("</table>");
              client.println("</div>");

              /* get data from the other modules */
              for (int lcv=0; lcv<numModules; lcv++){                     // for each module
                client.println("<div class='modules'>");
                client.println("<table>");
                for(int ilcv=0; ilcv<6;ilcv++){                           // for each sensor
                  client.println("<tr>");
                  for(int i=0; i<4; i++){                                 // for each row
                    client.println("<td>");
//                    client.println(module_data[lcv][ilcv][i]);
                    client.println("</td>");
                  }/* for() */
                  client.println("</tr>");
                }/* for() */
                client.println("</table>");
                client.println("</div>");
              }/* for() */
              
            }/* if() */
          }/*if()*/
        }/*while()*/
      }/*if()*/
      
    }/* if() */
    numMinutes = 0;
  }/* if() */
  for (int i=0; i < 1800; i++){
    delay(1000);
  }
//  delay(2000);
}/*loop()*/

/*
 * Sends a detailed email to the user if an error occurs
 * Returns:
 *          0 - if the email was sent successful
 *          1 - if sending the email was failed
 */
int alertUser(){
  int result;

  if (sendEmail()){
    result = 0;
  } else {
    result = 1;
  }/* if()*/

  return result;
} /* alertUser()*/


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
  Serial.println(F("Sending auth login"));
  emailClient.println("auth login");
  if (!emailRcv()){
    return 0;
  }/*if()*/
  
  // username
  Serial.println(F("Sending user"));
  // change to base64 encoded user
  emailClient.println("amFzcGVlZHg3ODVAZ21haWwuY29t");
  
  if (!emailRcv()){
    return 0;
  }/*if()*/

  //password
  Serial.println(F("Sending password"));
  // base64 encode user
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
  emailClient.println("Sent from Arduino board");

  
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
 * Read the sensor values from the other modules
 * After receiving the data, send a reply to client
 * module
 */
String getModuleData(){
  char replyBuffer[] = "received";

  

  
}/*getModuleData()*/


/*
 * Add/modify client information
 */
void addClient(EthernetClient client){
  /* display sensor data on website */
  client.println("<!DOCTYPE html>");
  client.println("<html lang='en'>");

  /* head */
  client.println("<head>");
  client.println("<title>&#9881; Settings</title>");
  client.println("<meta charset='utf-8'>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  client.println("</head>");
  client.println("<table>");
  client.println("<tr>");
  client.println("<td>Module Number</td>");
  client.println("<td>Module IP</td>");
  client.println("<td>Module PORT</td>");
  
  
  
}/* changeSettings() */


/*
 * Write the sensor data to a text file
 * Returns
 *        0 - if the data was succesfully written to the file
 *        1 - if the file was not found
 */
int logData(int measNum){
  int errorCode = 0;
  File dataFile = SD.open("logdata.txt",FILE_WRITE);
  

  // if the file is available write to it
  if (dataFile) {    
    dataFile.print(measNum);  
    dataFile.print(",");
    

    /* record measured data */
    // Temperature
    dataFile.print(LiquidTempSensor);
    dataFile.print(",");
    
    // Level    
    dataFile.print(LiquidLevelSensor);
    dataFile.print(",");

    // Flow
    dataFile.print(LiquidFlowSensor);
    dataFile.print(",");

    // EC    
    dataFile.print(ECSensor);
    dataFile.print(",");

    // Dissolved Oxygen    
    dataFile.print(DO2Sensor);
    dataFile.print(",");

    // pH    
    dataFile.print(phSensor);
    dataFile.println();
    dataFile.println();
    
    dataFile.close();                                         // close the file    
  } else {            // if the file isn't open return error
    errorCode = 1;
  }/* if() */

  return errorCode;
}/* logData() */

/*
 * Get the number of modules currently present
 */
int getNumOfModules(File configFile){
  char temp[3];
  char currChar;
  char prevChar;
  int lcv = 0;
  while (configFile.available()){
    currChar = configFile.read();
    if (currChar == '#'){
      currChar = configFile.read();
      while(currChar != ';'){        
        temp[lcv] = currChar;
        lcv++;
        currChar = configFile.read();
      }/*while()*/
    }/* if() */    
  }/*while()*/
  return String(temp).toInt();
}/* getNumOfModules() */

