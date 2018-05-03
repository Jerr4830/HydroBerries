#include <SPI.h>
#include <OneWire.h>
#include <SD.h>

#define TEMP_SENSOR_PIN A4

OneWire tempSensor(TEMP_SENSOR_PIN);

const int chipSelect = 4;
float temperature;
int num = 0;
/* Variables for EC Sensor */
float CalibrationEC = 3; // EC value of calibration solution in s/cm
double avgEC = 0.0;
double maxEC = 0.0;
double minEC = 0.0;
String ecStatus = "Normal";

// The value below is dependent on the chemical solution
float TempCoef = 0.019;   // 0.019 is considered the standard for plant nutrients

float StartTemp = 0;
float FinishTemp = 0;
float KValue = 0.07;
int ECPin = A0;
int ECPwr = A4;
int RA = 1025;
int RB = 25; // Resistance of powering pins
float EC = 0;
float EC25 = 0;
double ecSensor = 0.0;
boolean ecval = true;

String dataString = "";


float raw = 0;
float Vin = 5;
float Vdrop = 0;
float RC = 0;
float buf = 0;

float startEC = 0;
float currPPM = 710;

// converting to ppm (US)
float PPMconversion = 0.5;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

//  if (!SD.begin(chipSelect)){   // See if the card is present and can be intialized
//    Serial.println("Card failed, or not present");
//    return;
//  }/*if()*/
  
  pinMode(A3,OUTPUT);
  digitalWrite(A3,HIGH);
  pinMode(A5,OUTPUT);
  digitalWrite(A5,LOW);
  
  pinMode(ECPin,INPUT);
  pinMode(ECPwr,OUTPUT);

//  delay(2000);

//  setKValue();  
//  Serial.print("K value: ");
//  Serial.println(KValue);
//  delay(1000);
//  initEC();

}



/* 
 * Calibrate EC sensor 
 */
//void setKValue(){  
//  int lcv = 1;        // LOOP COUNTER VARIABLE
//  buf = 0;
//  
//  
//  StartTemp = getTemp(); 
//  delay(1000); 
//  StartTemp = getTemp();
//  Serial.println(StartTemp);
//
//  while (lcv <= 10){
//    digitalWrite(ECPwr, HIGH);
//    raw = analogRead(ECPin);  // first reading will be low
//    raw = analogRead(ECPin);
//    digitalWrite(ECPwr, LOW);
//    buf = buf + raw;
//    lcv++;
//    delay(5000);
//  }
//  raw = (buf/10);
//
//  
//  FinishTemp = getTemp();
//  Serial.println(FinishTemp);
//
//  EC = CalibrationEC*(1+(TempCoef*(FinishTemp-25.0)));
//
//  Vdrop = (((Vin)* (raw))/1024.0);
//  RC = (Vdrop*RA)/ (Vin-Vdrop);
//  RC = RC - RB ;
//  KValue = 1000/(RC*EC);
//
//  if (StartTemp == FinishTemp){    
//    Serial.println("EC Calibration Successful");
//  } else {    
//    Serial.println("EC Calibration Failed");
//  }
//}

/*
 * Get EC value
 */
void getEC(){
   
  float temp  = getTemp(); 

  temp  = getTemp();

  
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
//  Serial.print("Vdrop: ");
//  Serial.println(Vdrop);
  // Compensating for Temperature
  EC25 = EC/(1+TempCoef*(temp - 25.0));
  ecSensor = (EC25) * (PPMconversion*1000);

//  Serial.print("EC ");
//  Serial.print(EC25);
//  Serial.println(" mS/cm"); 

  //wait for 100 ms
  delay(100);
}


/*
 * Read the temperature of the Liquid several times and return the average
 */
float getTemp(){
  //returns the temperature from one DS18S20 in DEG Celsius

  byte data[12];
  byte addr[8];

  if ( !tempSensor.search(addr)) {
      //no more sensors on chain, reset search
      tempSensor.reset_search();
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

  tempSensor.reset();
  tempSensor.select(addr);
  tempSensor.write(0x44,1); // start conversion, with parasite power on at the end

  byte present = tempSensor.reset();
  tempSensor.select(addr);    
  tempSensor.write(0xBE); // Read Scratchpad

  for (int i = 0; i < 9; i++) { // we need 9 bytes
    data[i] = tempSensor.read();
  }

  tempSensor.reset_search();

  byte MSB = data[1];
  byte LSB = data[0];

  float tempRead = ((MSB << 8) | LSB); //using two's compliment
  float TemperatureSum = tempRead / 16;

  temperature = (TemperatureSum * 18 + 5)/10 + 32;

  return TemperatureSum;
}


/*
 * Read the conductivity of the solution
 */
void loop() {
//  File dataFile = SD.open("ecdata.txt",FILE_WRITE);
    
  
  getEC();
  dataString = num;
  dataString += ",";
  dataString = ecSensor;
  Serial.println(dataString);

//  if (dataFile){
//    if (num == 0){
//      dataFile.println("Time(min), Temperature(F), E.C.(ppm)");
//    }
//    dataFile.println(dataString);
//    dataFile.close();
//    Serial.println("File opened");
//  } else {
//    Serial.println("File not opened");
//}   
  

  
  
  delay(10000);
  num += 10;
}
