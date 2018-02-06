/*
 * Durgin Family Farms - Hydroponics Control System
 * 
 * Read the values of the analog/digital input pins
 * Display the values on a web server using an Arduino Ethernet Shield
 * If there is no internet connection then the data is stored on the SD Card. 
 * If an error occurs and there is ethernet connection then an alert email is
 * sent to the user detailing which error occured during the operation.
 * 
 * Microcontroller:
 * 
 * ** Arduino Mega ADK R3
 * 
 * ** Arduino Ethernet Shield
 * 
 * Sensors:
 * 
 * ** Water Temperature Sensor: Waterproof DS18B20 
 * *** Libraries required: OneWire.h, DallasTemperature.h
 * 
 *  ** Water Flow Sensor: Liquid Flow Meter - Platic 1/2" NPS Threaded
 *  *** No additional libraries required
 * 
 * Pin Setup:
 * 
 * Date Created: 11/1/17
 * Revision: 1.2
 */
 
/* Libraries */
#include <SPI.h>                  // Serial Port library
#include <Ethernet.h>             // Ethernet Library
#include <SD.h>                   // SD card library
#include <OneWire.h>              // library for sensor
#include <DallasTemperature.h>    // Library for temperature sensor

/* Digital Sensors Pin */
#define ONE_WIRE_BUS_TEMP_SENSOR 6        // temperature sensor digital pin
#define FLOWSENSORPIN 2                   // flow sensor digital pin
#define ECHOPIN 8
#define TRIGPIN 10

// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ   90
// size of buffer that stores a line of text for the LCD + null terminator
#define LIMIT_BUF_SZ   17


// Module number
String ModuleNumber = "A";

// sd card chipSelect
const int chipSelect = 4;


// MAC address and IP address for the controller
// IP address will be dependent on the local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

// static ip address
// assign an IP address for the controller:
byte ip[] = {129,21,63,177};

// SMTP server to send email
char emailServer[] = "mail.smtp2go.com";
// port for the email server
int emailPort = 2525;
// body content of the email
String emailBuffer;

// Initialize the Ethernet server library
// with the IP address and port
EthernetServer server(80);

// Ethernet client for email
EthernetClient emailClient;

// File for SD card
File logFile;

char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
char req_index = 0;              // index into HTTP_req buffer
char Temp_upper_limit[LIMIT_BUF_SZ] = {0};// buffer to save upper limit of water temperature
char Temp_lower_limit[LIMIT_BUF_SZ] = {0};// buffer to save lower limit of water temperature

// number of seconds
int numSeconds = 0;

// number of minutes
int numMins = 0;

// number of hours
int numHours = 0;

// maximum number of hours
int maxNumberHours = 2;


/* Water Temperature Sensor */
// Setup a OneWire instance for the temperature sensor
OneWire TempSensor(ONE_WIRE_BUS_TEMP_SENSOR);

// Pass OneWire refrence to Dallas Temperature
DallasTemperature tempSensor(&TempSensor);

float WaterTemp;
double avgWaterTemp = 0.0;
double maxWaterTemp = 0.0;
double minWaterTemp = 0.0;
String WaterTempStatus = "Normal";

/* Variables for EC Sensor */
float CalibrationEC = 2.9; // EC value of calibration solution in s/cm
double avgEC = 0.0;
double maxEC = 0.0;
double minEC = 0.0;
String ecStatus = "Normal";

// The value below is dependent on the chemical solution
float TempCoef = 0.019;   // 0.019 is considered the standard for plant nutrients

float StartTemp = 0;
float FinishTemp = 0;
float KValue = 0;
int ECPin = A0;
int ECPwr = A4;
int RA = 1000;
int RB = 25; // Resistance of powering pins
float EC = 0;
float EC25 = 0;
double ecSensor = 0.0;

float raw = 0;
float Vin = 5;
float Vdrop = 0;
float RC = 0;
float buf = 0;

// converting to ppm (US)
float PPMconversion = 0.5;


/* Water Flow Meter */
// count how many pulses
volatile uint16_t flow_pulses = 0;
//track the state of the flow pin
volatile uint16_t flow_LastState;
//time between pulses
volatile uint32_t flowratetimer = 0;
// flow rate
volatile float WaterFlow = 0.0;

double avgWaterFlow = 0.0;
double maxWaterFlow = 0.0;
double minWaterFlow = 0.0;
String WaterFlowStatus = "Normal";


bool eth_connection = true;

/* Dissolved Oxygen Sensor */
double oxygenSensor = 0.0;
double avgDO2 = 0.0;
double maxDO2 = 0.0;
double minDO2 = 0.0;
String oxygenStatus = "Normal";

/* Water Level Sensor */
int levelPin = A10;
double WaterLevel = 25.5;
double avgWaterLevel = 0.0;
double maxWaterLevel = 0.0;
double minWaterLevel = 0.0;
String WaterLevelStatus = "Normal";

/* pH Sensor */
int phPin = A8;
double phSensor = 0;
double avgPH = 0.0;
double maxPH = 0.0;
double minPH = 0.0;
String phStatus = "Normal";

// debugging
String dataDebug = "";

bool debug = true;

/* -----------------------------------
 *            SYSTEM SETUP
 * -----------------------------------
 */


/* Interrupt is called once a millisecond */
SIGNAL(TIMER0_COMPA_vect) {
  uint8_t xx = digitalRead(FLOWSENSORPIN);

  numSeconds++;
  if (numSeconds == 60000){
    numSeconds = 0;
    numMins++;
    if (numMins == 60){
      numMins = 0;
      numHours++;
    }
  }  

  if (xx == flow_LastState){
    flowratetimer++;
    return; // nothing changed
  }

  if (xx == HIGH){
    // low to high transition
    flow_pulses++;
  }

  flow_LastState = xx;
  WaterFlow = 1000.0;
  WaterFlow /= flowratetimer; // in Hz
  flowratetimer = 0;
}

/* 
 * Interrupt function for flow rate sensor
 * Interrrupts the board every 1 ms
*/
void useInterrupt(boolean v){
  if (v) {
    // Timer) is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0XAF;
    TIMSK0 |= _BV(OCIE0A);  
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
  }
}

/* 
 *  Setup sensors for Arduino board
 */
void InitSensors(){  
  /* Setup Temperature Sensor */
  Serial.println("Setting up Temperature Sensor...");
  tempSensor.begin();
  delay(100);

  /* Setup Water Level Sensor */
  Serial.println("Setting up Level Sensor...");
  pinMode(levelPin, INPUT);

  

  /* Setup EC Sensor */
  Serial.println("Setting up EC Sensor...");
  pinMode(ECPin, INPUT);
  pinMode(ECPwr, OUTPUT);
 // pinMode(ECGnd, OUTPUT);
 // digitalWrite(ECGnd,LOW);
  delay(1000);
  RA = RA + RB;

  /* Setup flow sensor */
  Serial.println("Setting up Flow meter...");
  pinMode(FLOWSENSORPIN, INPUT);
  digitalWrite(FLOWSENSORPIN, HIGH);
  flow_LastState = digitalRead(FLOWSENSORPIN);
  useInterrupt(true);
  delay(100);

  /* Water Level */
  pinMode(ECHOPIN, INPUT);
  pinMode(TRIGPIN, OUTPUT);
  
  digitalWrite(ECHOPIN, HIGH);  
}

/* 
 * Calibrate EC sensor 
 */
void setKValue(){  
  int lcv = 1;        // LOOP COUNTER VARIABLE
  buf = 0;
  
  tempSensor.requestTemperatures();
  StartTemp = tempSensor.getTempCByIndex(0);

  while (lcv <= 10){
    digitalWrite(ECPwr, HIGH);
    raw = analogRead(ECPin);  // first reading will be low
    raw = analogRead(ECPin);
    digitalWrite(ECPwr, LOW);
    buf = buf + raw;
    lcv++;
    delay(5000);
  }
  raw = (buf/10);

  tempSensor.requestTemperatures();
  FinishTemp = tempSensor.getTempCByIndex(0);

  EC = CalibrationEC*(1+(TempCoef*(FinishTemp-25.0)));

  Vdrop = (((Vin)* (raw))/1024.0);
  RC = (Vdrop*RA)/ (Vin-Vdrop);
  RC = RC - RB;
  KValue = 1000/(RC*EC);

  if (StartTemp == FinishTemp){    
    Serial.println("EC Calibration Successful");
  } else {    
    Serial.println("EC Calibration Failed");
  }
}

/* setup Arduino board */
void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);

  Serial.println("Initializing System...\n\n");

  // Initialize sensors
  //Serial.println("Initializing Sensors...");
  InitSensors();

  // Calibrate EC Sensor
  Serial.println("Calibrating EC sensor...");
  setKValue();

  // Initialize SD Card
  Serial.println("Initializing SD Card...");
  
  if (!SD.begin(chipSelect)){   // See if the card is present and can be intialized
    Serial.println("Card failed, or not present");
    return;
  } else {
    Serial.println("SD Card initialized.");
  }
  
  
  Serial.println("setting up connection");
  // start the Ethernet connection and the server
  Ethernet.begin(mac);
   
  Serial.println("Starting server...");
  server.begin();
  Serial.print("Web Server is at ");
  Serial.println(Ethernet.localIP()); 
}

/*
 * -----------------------------------------------
 * ||||||||||||||||   MAIN LOOP   ||||||||||||||||
 * -----------------------------------------------
 */

/*
 * Continuously read and update sensor values
 * The sensor values are written to a file on the sd card if 
 * there is no internet connection, otherwise the values are
 * displayed on a website.
 * An email is sent to the user if the sensor(s) value(s)
 * is not within the specified limit.
 */
void loop() {
    
  // listen for incoming clients
  EthernetClient client = server.available();
  int timing = 0;
  emailBuffer = "The Module below has one or more errors\n\nModule Number: ";
  emailBuffer += String(ModuleNumber);
  emailBuffer += "\n\n";

  dataDebug = "Getting Measurement from Sensors";
  /* Get the measurement from the sensors */
  // water temperature
  GetWaterTemp();
  // Dissolved Oxygen
  //GetOxygen();
  // EC
  GetEC();
  // Water Flow  
  GetWaterFlow();
  // Water Level
  getWaterLevel();
  // pH  Sensor
  //getPH();

  // try connection to ethernet
  // timeout after 30 seconds.
  while(!client){
    delay(1);
    timing++;
    if (timing > 30000){
      break;
    }
  }
  
  
  // Display Sensor values on the website
  
  if (client){
    // a http request ends with a blank line
    boolean BlankLine = true;
    while(client.connected()){
      Serial.println("Connection Successful");
      if (client.available()){
        char clientResponse = client.read(); 
        Serial.println(clientResponse);
              
        // HTTP request has ended
        if (clientResponse == '\n' && BlankLine){
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          // keep the arduino connected to the internet
          client.println("Refresh: keep-alive");
          client.println();          
          DisplayData(client); 
          Serial.println("Website updated");
          break;                  
        }
      }
    }    
  } else{
    Serial.println("No connection");
    //logData();    
  }

  if (debug){
    Serial.println(dataDebug);
  }
  // wait for 1 second
  delay(1);
  
  //stopping client
  client.stop();

  dataDebug = "";
  emailBuffer = "";
  // wait for 10 seconds
  delay(1000); 

  if (numHours >= 2){
    logData();
  }
}


/* 
 * Read the water temperature sensor data
 */
void GetWaterTemp(){  
  // read temperature 5 times then takes average
  float temp = 0.0;
  for (int lcv = 0; lcv < 5; lcv++){
    temp += DallasTemperature::toFahrenheit(tempSensor.getTempCByIndex(0));
    delay(1000);
  }
  
  WaterTemp = temp/5;
  
  // Debugging
  dataDebug += "\r\nWater Temperature Sensor \r\n";
  dataDebug += "Value: ";
  dataDebug += String(WaterTemp);
  dataDebug += (" F \r\n");
  
  if (WaterTemp > 85){
    emailBuffer += "The water temperature is too hot (Water Temperature = ";
    emailBuffer += char(WaterTemp);
    emailBuffer += ")\n";
    WaterTempStatus = "Over the limit";
  } else if (WaterTemp < 77){
    emailBuffer += "The water temperature is too cold (Water Temperature = ";
    emailBuffer += char(WaterTemp);
    emailBuffer += ")\n";
    WaterTempStatus = "under the limit";
  } else {
    WaterTempStatus = "Normal";
  }

  dataDebug += "Status: ";
  dataDebug += WaterTempStatus;
  dataDebug += "\r\n\n";  

  // wait for 100 ms
  delay(100);
}

/*
 * Read the dissolved oxygen sensor value
 */
void GetOxygen(){

  // read the oxygen value five times then average
  oxygenSensor = 0;
  // (update code)

  // Debugging
  dataDebug += "Dissolved Oxygen Sensor \r\n";
  dataDebug += "Value: ";
  dataDebug += String(oxygenSensor);
  dataDebug += (" mg/L \r\n");

  if (oxygenSensor > 2000){
    oxygenStatus = "Over the limit";
    emailBuffer += "The amount of dissolved oxygen in the nutrient solution is over the limit (Dissolved Oxygent = ";
    emailBuffer += char(oxygenSensor);
    emailBuffer += ")\n";
  } else if (oxygenSensor < 100){
    oxygenStatus = "Under the limit";
    emailBuffer += "EC value is over the limit (Electrical Conductivity (EC) = ";
    emailBuffer += char(oxygenSensor);
    emailBuffer += ")\n";
  } else {
    oxygenStatus = "Normal";
  }

  dataDebug += "Status: ";
  dataDebug += oxygenStatus;
  dataDebug += "\r\n\n";
  
  //wait for 100 ms
  delay(100);
}

/*
 * Get EC value
 */
void GetEC(){
  tempSensor.requestTemperatures();  
  float temp  = tempSensor.getTempCByIndex(0); 

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

  // Debugging
  dataDebug += "Electrical Conductivity (EC) Sensor \n";
  dataDebug += "Value: ";
  dataDebug += String(ecSensor);
  dataDebug += (" ppm \r\n");

  // Check if EC is within Limits
  if (ecSensor > 2000){
    ecStatus = "Over the limit";
    emailBuffer += "EC value is over the limit (Electrical Conductivity (EC) = ";
    emailBuffer += char(ecSensor);
    emailBuffer += ")\n";
  } else if (ecSensor < 100){
    ecStatus = "Under the limit";
    emailBuffer += "EC value is over the limit (Electrical Conductivity (EC) = ";
    emailBuffer += char(ecSensor);
    emailBuffer += ")\n";
  } else {
    ecStatus = "Normal";
  }

  dataDebug += "Status: ";
  dataDebug += ecStatus;
  dataDebug += "\r\n\n";
  

  //wait for 100 ms
  delay(100);
}

/*
 * Get water flow
 */
void GetWaterFlow(){
  WaterFlow = flow_pulses / (7.5*60);

  // Debugging
  dataDebug += "Water Flow Sensor: \r\n";
  dataDebug += "Value: ";
  dataDebug += String(WaterFlow);
  dataDebug += (" L/s  \n");

  // Check if water flow is within limits
  if (WaterFlow > 5){
    WaterFlowStatus = "Over the limit";
    emailBuffer += "The water flow is over the limit (Water flow = ";
    emailBuffer += char(WaterFlow);
    emailBuffer += ")\n";
  } else if (WaterFlow < 0){
    WaterFlowStatus = "Under the limit";
    emailBuffer += "EC value is under the limit (Water flow = ";
    emailBuffer += char(WaterFlow);
    emailBuffer += ")\n";
  } else {
    WaterFlowStatus = "Normal";
  }

  dataDebug += "Status: ";
  dataDebug += WaterFlowStatus;
  dataDebug += "\r\n\n ";  

  //wait for 100 ms
  delay(100);
}

void getWaterLevel(){
  // Set the trigger pin to low for 2uS
  digitalWrite(TRIGPIN, LOW);
  delayMicroseconds(2);

  // Send a 10uS high to trigger ranging
  digitalWrite(TRIGPIN, HIGH);
  delayMicroseconds(10);

  // Send pin low again
  digitalWrite(TRIGPIN, LOW);
  
  WaterLevel = pulseIn(ECHOPIN, HIGH,26000);
  WaterLevel = WaterLevel/58;
  

  // Debugging
  dataDebug += "Water Level Sensor \r\n";
  dataDebug += "Status: ";
  dataDebug += String(WaterLevel);
  dataDebug += (" cm \r\n");

  // Level
  if (WaterLevel > 100){
    WaterLevelStatus = "Over the limit";
    emailBuffer += "EC value is over the limit (Electrical Conductivity (EC) = ";
    emailBuffer += char(WaterLevel);
    emailBuffer += ")\n";
  } else if (WaterLevel < 5){
    WaterLevelStatus = "Under the limit";
    emailBuffer += "EC value is over the limit (Electrical Conductivity (EC) = ";
    emailBuffer += char(WaterLevel);
    //emailBuffer += ")\n";
  } else {
    WaterLevelStatus = "Normal";
  }

  dataDebug += "Status: ";
  dataDebug += WaterLevelStatus;
  dataDebug += "\r\n\n";
  

  //wait for 100 ms
  delay(10000);
  
}

/*
 * Get ph Sensor value
 */
void getPH(){
  phSensor = 0;

  // Debugging
  dataDebug += "pH Sensor \r\n";
  dataDebug += "Value: ";
  dataDebug += String(phSensor);
  dataDebug += ("\r\n");

  // Check if pH is within values
  if (phSensor > 10){
    phStatus = "Over the limit";
    emailBuffer += "EC value is over the limit (Electrical Conductivity (EC) = ";
    emailBuffer += char(phSensor);
    emailBuffer += ")\n";
  } else if (phSensor < 3){
    phStatus = "Under the limit";
    emailBuffer += "EC value is over the limit (Electrical Conductivity (EC) = ";
    emailBuffer += char(phSensor);
    emailBuffer += ")\n";
  } else {
    phStatus = "Normal";
  }

  dataDebug += "Status: ";
  dataDebug += phStatus;
  dataDebug += "\r\n\n";
  

  //wait for 100 ms
  delay(100);
  
}

/*
 * Alert user via email if sensor values are outside limits
*/
void AlertUser(){
  if (sendEmail()){
    Serial.println(F("Email sent"));
  } else {
    Serial.println(F("Email Failed"));
  }
}

/*
 * Send email to user using a SMTP server
 */

 byte sendEmail(){
  byte thisByte = 0;
  byte respCode;

  if (emailClient.connect(emailServer, emailPort) == 1){
    Serial.println(F("connected"));
  } else {
    Serial.println(F("connection failed"));
    return 0;
  }

  if (!emailRcv()){
    return 0;
  }

  // Auth login
  Serial.println(F("Sending auth login"));
  emailClient.println("auth login");
  if (!emailRcv()){
    return 0;
  }
  
  // username
  Serial.println(F("Sending user"));
  // change to base64 encoded user
  emailClient.println("amFzcGVlZHg3ODVAZ21haWwuY29t");
  
  if (!emailRcv()){
    return 0;
  }

  //password
  Serial.println(F("Sending password"));
  // base64 encode user
  emailClient.println("QXJkdWlub0VtYWlsVGVzdGluZw==");

  if (!emailRcv()){
    return 0;
  }

  // Sender's email
  Serial.println("Sending From");
  emailClient.println("MAIL From: <jasspeedx785@gmail.com>");

  if (!emailRcv()){
    return 0;
  }

  // recipient address
  Serial.println("Sending To");
  emailClient.println("RCPT To: <jra8788@rit.edu>");

  if (!emailRcv()){
    return 0;
  }

  // DATA
  Serial.println("Data");
  emailClient.println("DATA");

  // Sending the email
  Serial.println(F("Sending email"));
  
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
  }

  Serial.println(F("Sending QUIT"));
  
  emailClient.println("QUIT");
  
  if (!emailRcv()){
    return 0;
  }
  // disconnect from server
  emailClient.stop();

  Serial.println(F("disconnected"));

  return 1;
}

/*
 * Verify that data was sent to emai server 
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
      Serial.println(F("\r\nTimeout"));
      return 0;
    }
  }

  responseCode = emailClient.peek();

  while(emailClient.available()){
    thisByte = emailClient.read();
    Serial.write(thisByte);
  }

  if (responseCode >='4'){
    emailFail();
    return 0;
  }
  return 1;
}

/*
 * Failed to send email 
 */
void emailFail(){
  byte thisByte = 0;
  int loopCount = 0;

  // stop sending the email
  emailClient.println(F("QUIT"));

  while(!emailClient.available()){
    delay(1);
    loopCount++;

    // if nothing is received  for 30 seconds, timeout
    if (loopCount > 30000){
      emailClient.stop();
      Serial.println(F("\r\n Timeout"));
      return;
    }
  }

  while(emailClient.available()){
    thisByte = emailClient.read();
    Serial.write(thisByte);
  }

  //disconnect from the server
  emailClient.stop();

  Serial.println(F("disconnected"));
}


/*
 * Display recorded sensor data on a website.
 * Display the data for each monitoring system
 * in a table format.
 */
void DisplayData(EthernetClient client){
    
  /* display sensor data on website */
  client.println("<!DOCTYPE html>");
  client.println("<html lang='en'>");

  /* head */
  client.println("<head>");
  client.println("<title> Hydroberries System </title>");
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

  /*client.println("<script>");
  client.println();
  client.println("Kvalue = '';");
  client.println();
  client.println("function SendValue()");
  client.println("{");
  client.println("nocache = '&nocache=' + Math.random() * 1000000;");
  client.println("var request = new XMLHttpRequest();");
  client.println("Kvalue = '&txt=' + document.getElementById('txt_form').form_text.value + '&end=end';");
  client.println("request.open('GET', 'ajax_inputs'+ + Kvalue + nocache, true);");
  client.println("request.send(null)");
  client.println("}");
  client.println("</script>");*/
  client.println("</head>");
  /* body */
  client.println("<body>");
  //header
  client.println("<div class='header'>");
  client.println("<h1> Hydroponic Strawberries Monitoring System</h1>");
  /*client.println("<form id='txt_form'>");
  client.println("<input type='text' />");
  client.println("</form>");
  client.println("<input type='submit' value='Send K Value' onclick='SendValue()' />");*/
  client.print("Total number of modules: ");
  client.println(NumOfModules);
  client.println("</div>");
  // Modules
  client.println("<div class='row'>");
  client.println("<div class='column'>");
  /// Module Number
  client.print("<h2> Module: ");
  client.print("A");
  client.print("</h2>");
  client.println("<br />");
  /// Monitoring parameters
  client.println("<table>");
  //// row 1
  client.println("<tr>");
  ///// headers
  client.println("<th> Nutrients </th>");
  client.println("<th> Value </th>");
  client.println("<th> Units </th>");
  client.println("<th> Status </th>");
  client.println("</tr>");
  // Display data read from sensors
  int nLCV = 0;
  //// Water Temperature
  client.println("<tr>");
  ///// label
  client.println("<td>");
  client.print("<b>");
  client.print("Water Temperature");  
  client.print("</b>");
  client.println("</td>");
  ///// sensor value
  client.println("<td>");
  client.println(WaterTemp);
  client.println("</td>");
  ///// units
  client.println("<td>");
  client.println("&#8457");
  client.println("</td>");
  ///// status
  client.println("<td>");
  client.println(WaterTempStatus);
  client.println("</td>");
  client.println("</tr>");
  /* Water Flow */
  client.println("<tr>");
  ///// label
  client.println("<td>");
  client.print("<b>");
  client.print("Water Flow");  
  client.print("</b>");
  client.println("</td>");
  ///// Sensor Value
  client.println("<td>");
  client.println(WaterFlow);
  client.println("</td>");
  ///// Unit
  client.println("<td>");
  client.println("liters/sec");
  client.println("</td>");
  ///// Status
  client.println("<td>");
  client.println(WaterFlowStatus);
  client.println("</td>");
  client.println("</tr>");
  /* Conductivity */
  client.println("<tr>");
  ///// label
  client.println("<td>");
  client.print("<b>");
  client.print("Electrical Conductivity (EC)");  
  client.print("</b>");
  client.println("</td>");
  //Sensor Value
  client.println("<td>");
  client.println(ecSensor);
  client.println("</td>");
  ///// Units
  client.println("<td>");
  client.println("ppm");
  client.println("</td>");
  ///// Status
  client.println("<td>");
  client.println(ecStatus);
  client.println("</td>");
  client.println("</tr>");
  /* Dissolved Oxygen */
  client.println("<tr>");
  // label
  client.println("<td>");
  client.print("<b>");
  client.print("Dissolved Oxygen");  
  client.print("</b>");
  client.println("</td>");
  // Sensor Value
  client.println("<td>");
  client.println(oxygenSensor);
  client.println("</td>");
  // Unit
  client.println("<td>");
  client.println("ppm");
  client.println("</td>");
  //Status
  client.println("<td>");
  client.println(oxygenStatus);
  client.println("</td>");
  client.println("</tr>");

  /* Water Level */
  client.println("<tr>");
  // label
  client.println("<td>");
  client.print("<b>");
  client.print("Water Level");  
  client.print("</b>");
  client.println("</td>");
  // Sensor Value
  client.println("<td>");
  client.println(WaterLevel);
  client.println("</td>");
  // Unit
  client.println("<td>");
  client.println("cm");
  client.println("</td>");
  //Status
  client.println("<td>");
  client.println(WaterLevelStatus);
  client.println("</td>");
  client.println("</tr>");

  /* pH */
  client.println("<tr>");
  // label
  client.println("<td>");
  client.print("<b>");
  client.print("pH");  
  client.print("</b>");
  client.println("</td>");
  // Sensor Value
  client.println("<td>");
  client.println(phSensor);
  client.println("</td>");
  // Unit
  client.println("<td>");
  client.println("---");
  client.println("</td>");
  //Status
  client.println("<td>");
  client.println(phStatus);
  client.println("</td>");
  client.println("</tr>");
  
  client.println("</table>");
  client.println("</div>");
  client.println("</div>");
  client.println("</body>");
  client.println("</html>");  
}


/*
 * Write Sensor values in a text file 
 */
void logData(){
  File dataFile = SD.open("datalog.txt",FILE_WRITE);
  
  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println("Sensor Measurements");
    dataFile.println(ModuleNumber);
    
    dataFile.println("Water Temperature");
    dataFile.print("Value: ");
    dataFile.print(WaterTemp);
    dataFile.println(" F");
    dataFile.print("Status: ");
    dataFile.println(WaterTempStatus);
    dataFile.println();
    dataFile.println("Water Flow");
    dataFile.print("Value: ");
    dataFile.print(WaterFlow);
    dataFile.println(" liters/sec");
    dataFile.print("Status: ");
    dataFile.println(WaterFlowStatus);
    dataFile.println();
    dataFile.println("Water Level");
    dataFile.print("Value: ");
    dataFile.print(WaterLevel);
    dataFile.println(" cm");
    dataFile.print("Status: ");
    dataFile.println(WaterLevelStatus);
    dataFile.println();
    dataFile.println("Electrical Conductivity (EC)");
    dataFile.print("Value: ");
    dataFile.print(ecSensor);
    dataFile.println(" ppm");
    dataFile.print("Status: ");
    dataFile.println(ecStatus);
    dataFile.println();
    dataFile.println("Dissolved Oxygen (DO2)");
    dataFile.print("Value: ");
    dataFile.print(oxygenSensor);
    dataFile.println(" F");
    dataFile.print("Status: ");
    dataFile.println(oxygenStatus);
    dataFile.println();
    dataFile.println("pH");
    dataFile.print("Value: ");
    dataFile.print(phSensor);
    dataFile.println();
    dataFile.print("Status: ");
    dataFile.println(phStatus);
    dataFile.println();
    dataFile.println();
    dataFile.close();
  } else { // if the file isn't open pop up an error
    Serial.println("Error opening datalog.txt");
  }
  
}

