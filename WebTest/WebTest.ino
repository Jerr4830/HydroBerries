#include <SPI.h>                  // Serial Port library
#include <Ethernet.h>             // Ethernet Library

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
// Initialize the Ethernet server library
// with the IP address and port
EthernetServer server(80);




/* setup Arduino board */
void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);

  
    
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
          // refresh the page automatically every 60 sec
          client.println("Connection: keep-alive");  // the connection will be closed after completion of the response
          client.println("Refresh: 5");
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
   // wait for 1 second
  delay(1);
  
  //stopping client
  client.stop();
  
  // wait for 10 seconds
  delay(1000);  
  
  
}

/* Display Sensor Data */
void DisplayData(EthernetClient client){
   
  
  /* display sensor data on website */
  client.println("<!DOCTYPE html>");
  client.println("<html>");

  

  
  /* body */
  client.println("<body>");
  
  
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
  client.println("80");
  client.println("</td>");
  ///// units
  client.println("<td>");
  client.println("F");
  client.println("</td>");
  ///// status
  client.println("<td>");
  client.println("Normal");
  client.println("</td>");
  client.println("</tr>");
  
  

  
  
  
  client.println("</table>");
  
  client.println("</body>");
  client.println("</html>");  
}
