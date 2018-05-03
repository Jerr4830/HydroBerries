#include <SPI.h>
#include <SD.h>

File dataFile;
File root;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  Serial.println("initializing SD card");

  if (!SD.begin(4)){
    Serial.println("initialization failed");
    return;
  }

  Serial.println("Initialization successful");
  delay(500);

  dataFile = SD.open("index.htm", FILE_WRITE);
  dataFile.close();
  

  delay(500);
  root = SD.open("/");
  printDirectory(root, 0);
}

void loop() {
  // put your main code here, to run repeatedly:

}

void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}
