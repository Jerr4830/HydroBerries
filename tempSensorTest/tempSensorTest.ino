/*
 * Water Temperature Sensor test
 * DS18B20
 */

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS_TEMP 6

OneWire oneWireTemp(ONE_WIRE_BUS_TEMP);

DallasTemperature sensors(&oneWireTemp);
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Testing Temp Sensor");
  sensors.begin();
  

}

void loop() {
  // put your main code here, to run repeatedly:
  
  Serial.println("Getting temperature measurement");
  sensors.requestTemperatures();
  Serial.println("Done");
  Serial.print("Temp: ");
  float temp = sensors.getTempCByIndex(0);
  Serial.print(temp);
  Serial.print(" C");
  Serial.print("     ");
  Serial.print(DallasTemperature::toFahrenheit(temp));
  Serial.println();

  delay(2000);

}
