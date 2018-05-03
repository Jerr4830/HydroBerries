#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 1, 177);

unsigned int localPort = 8888;

// buffers for receiving and sending data
char packetBuffer[100];  //buffer to hold incoming packet,
char  ReplyBuffer[] = "received";       // a string to send back

EthernetUDP serverUDP;

EthernetServer server(80);

String ModuleNumber = "";
String temperature = "0.00";
String tempStatus = "";
String level = "0.00";
String levelStatus = "";
String flow = "0.00";
String flowStatus = "";
String conductivity = "0.00";
String ecStatus = "";
String oxygen = "0.00";
String doStatus = "";
String ph = "0.00";
String phStatus = "";

int packetSize;


void setup() {
  // put your setup code here, to run once:
  Ethernet.begin(mac);
  server.begin();
  serverUDP.begin(localPort);
  Serial.begin(9600);

  Serial.print("Server address: ");
  Serial.println(Ethernet.localIP());

}

void loop() {
  // put your main code here, to run repeatedly:
  int lcv = 0; 
  boolean datarcv = false;
  
  while (datarcv == false){
    packetSize = serverUDP.parsePacket();

    if (packetSize){
      Serial.println("received");
      serverUDP.read(packetBuffer, 100);
      serverUDP.beginPacket(serverUDP.remoteIP(), serverUDP.remotePort());
      serverUDP.write(ReplyBuffer);
      serverUDP.endPacket();
      datarcv = true;
      Serial.println(packetBuffer);
    }/* if()*/
    
  }/* while() */
  parseMsg();
  EthernetClient client = server.available();
  while (!client){
    if (lcv == 60){
      Serial.println("timeout");
      break;
    }
    client = server.available();
    lcv++;
  }

  if (client){
    while(client.connected()){
      Serial.println("connected");
      if (client.available()){
        char c = client.read();

        if (c == '\n'){
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");                              
          client.println("Refresh: keep-alive");  
          client.println();

          /* display sensor data on the website */          
          client.println("<html>");

          /* head */
          client.println("<head><title>Hydroponic Monitoring System</title>");          
          client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
          client.println("</head>");

          /* body */
          client.println("<body>");
          client.println("<div style='text-align:center'>");
          client.println("<h1>Durgin Family Farms - Hydroponic Monitoring System</h1>");
          client.println("</div><div>");

          /* sensor data from module A */
          client.println("<div><label>Module Number: ");
          client.println(ModuleNumber);
          client.println("</label>");
              
          // header row
          client.println("<table><tr><td><b>Sensor</b></td><td><b>Value</b></td>");
          client.println("<td><b>Units</b></td><td><b>Status</b></td></tr>");

          /* sensor values */
          /* Module A */
          // temperature
          client.println("<tr><td><b>Temperature</b></td><td>");
          client.print(temperature);
          client.print("</td><td>&#8457;</td><td>");
          client.print(tempStatus);
          
          client.println("</td></tr><tr><td><b>Dissolved Oxygen</b></td><td>");
          client.print(oxygen);
          client.print("</td><td>mg/L</td><td>");
          client.println(doStatus);
          
          client.println("</td></tr><tr><td><b>pH</b></td><td>");
          client.print(ph);
          client.print("</td><td></td><td>");
          client.println(phStatus);
          
          client.println("</td></tr><tr><td><b>Level</b></td><td>");
          client.print(level);
          client.print("</td><td>cm</td><td>");
          client.println(levelStatus);
          client.println("</td></tr><tr><td><b>Electrical Conductivity</b></td><td>");
          client.print(conductivity);
          client.print("</td><td>ppm</td><td>");
          client.println(ecStatus);
          
          client.println("</td></tr><tr><td><b>Flow</b></td><td>");
          client.print(flow);
          client.print("</td><td>liters/sec</td><td>");
          client.println(flowStatus);
          
          client.println("</td></tr></table></div>");
          client.println("</body></html>");
          break;
        }
      }
    }
  }
  delay(10);
  client.stop();
  Ethernet.maintain();

  
  

}


void parseMsg(){
  int plc = 0;
  String dt = "";
  for (int lcv=0; lcv < packetSize; lcv++){
    if (packetBuffer[lcv] == ','){
      if (plc == 0){
        ModuleNumber = dt;
      } else if (plc == 1){
        temperature = dt;              
      } else if (plc == 2){
        tempStatus = dt;
      } else if (plc == 3){
        level = dt;
      } else if (plc == 4){
        levelStatus = dt;
      } else if (plc == 5){
        flow = dt;
      } else if (plc == 6){
        flowStatus = dt;
      } else if (plc == 7){
        conductivity = dt;
      } else if (plc == 8){
        ecStatus = dt;
      } else if (plc == 9){
        oxygen = dt;
      } else if (plc == 10){
        doStatus = dt;
      } else if (plc == 11){
        ph = dt;               
      } else {
        phStatus = dt;
      }
      dt = ""; 
      plc++;
    } else {
      dt += packetBuffer[lcv];
    }
  }
}

