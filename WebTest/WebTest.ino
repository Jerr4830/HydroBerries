#include <SPI.h>                  // Serial Port library
#include <Ethernet.h>             // Ethernet Library
#include <SD.h>


// MAC address and IP address for the controller
// IP address will be dependent on the local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

// static ip address
// assign an IP address for the controller:
byte ip[] = {129,21,63,177};
// Initialize the Ethernet server library
// with the IP address and port
EthernetServer server(80);

File webFile;
char response[25] = {0};

/* setup Arduino board */
void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);

  Serial.println("Initializing SD card");

  if (!SD.begin(4)){
    Serial.println("ERROR: Could not initialize SD card");
    return;
  }

  Serial.println("SD card initialized");

  if (!SD.exists("HOME~1.HTM")){
    Serial.println("ERROR: home.html not found");
    return;
  }
  Serial.println("home.html found");
  Serial.println("setting up connection");
  // start the Ethernet connection and the server
  Ethernet.begin(mac);
   
  Serial.println("Starting server...");
  server.begin();
  Serial.print("Web Server is at ");
  Serial.println(Ethernet.localIP()); 
}

/*
 * Continuously read and update sensor values
 */
void loop() {
    
  // listen for incoming clients
  EthernetClient client = server.available();
  int lcv = 0;    
  
  
  // Display Sensor values on the website
  if (client){
    // a http request ends with a blank line
    boolean BlankLine = true;
    while(client.connected()){
      Serial.println("Connection Successful");
      if (client.available()){
        char clientResponse = client.read(); 

        if (lcv < 25){
          response[lcv] = clientResponse;
          Serial.print(clientResponse);
          lcv++;
        }
              
        // HTTP request has ended
        if (clientResponse == '\n' && BlankLine){
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          // refresh the page automatically every 60 sec
          client.println("Connection: keep-alive");  // the connection will be closed after completion of the response
          client.println();
          if (StrContains(response, "GET / ") || StrContains(response, "GET /home.html")){
            webFile = SD.open("HOME~1.HTM");
          }

          if (StrContains(response, "GET /add_module.html")){
            webFile = SD.open("ADD~1.HTM");
          }

          if (webFile){
            while(webFile.available()){
              client.write(webFile.read());
            }
            webFile.close();
          }
          lcv = 0;
          StrClear(response,25);
          Serial.println("Website updated");
          break;                 
        }
      }
    }
    //stopping client
    client.stop();   
  } else{
    Serial.println("No connection");
    //logData();    
  }
   // wait for 1 second
  delay(1);
  
  
  
  // wait for 10 seconds
  delay(1000);   
}

void StrClear(char *str, int len){
  for (int y=0; y < len; y++){
    str[y] = 0;
  }
}


int StrContains(char *str, char *sfind){
  int found = 0;
  int index = 0;
  int len = strlen(str);

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
  return 0;
}

