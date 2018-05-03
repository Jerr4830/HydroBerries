#include <Ethernet.h>
#include <SPI.h>


// MAC address from Ethernet shield sticker under board
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 20);   // IP address, may need to change depending on network
EthernetServer server(80);       // create a server at port 80

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  Ethernet.begin(mac);

  server.begin();
  Serial.print("Ethernet IP: ");
  Serial.println(Ethernet.localIP());

}

void loop() {
  // put your main code here, to run repeatedly:
  EthernetClient client = server.available();

  if (client){
    Serial.println("Connection successful");
    while(client.connected()){
      
      if (client.available()){
        char c = client.read();
        Serial.print(c);
        if (c =='\n'){
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: keep-alive");
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println("<script>");
          client.println("function sendVal(){");
          client.println("var str1 = '';");
          client.println("var str2 = '';");
          client.println("nocache = '&nocache=' + Math.random() * 1000000;");
          client.println("var xhttp = new XMLHttpRequest();");          
          client.println("str1 = document.getElementById('name').value;");
          client.println("str2 = document.getElementById('phone').value;");
          client.println("xhttp.open('GET','ajax_inputs' + str1 + str2 + nocache, true);");
          client.println("xhttp.send(null);}");
          client.println("</script>");
          
          client.println("<input type='text' id='name' />");
          client.println("<input type='text' id='phone' />");
          client.println("<button type='button' onclick='sendVal()'>send</button>");
          client.println("</html>"); 
          break;         
        }
      }
    }
    delay(1);
    client.stop();
  }
  delay(100);

}
