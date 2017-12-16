#include <SPI.h>
#include <Ethernet.h>

// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ   90
// size of buffer that stores a line of text for the LCD + null terminator
#define LCD_BUF_SZ   17

// MAC address from Ethernet shield sticker under board
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 20);   // IP address, may need to change depending on network
EthernetServer server(80);       // create a server at port 80

char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
char req_index = 0;              // index into HTTP_req buffer
char lcd_buf_1[LCD_BUF_SZ] = {0};// buffer to save LCD line 1 text to
char lcd_buf_2[LCD_BUF_SZ] = {0};// buffer to save LCD line 2 text to


void setup()
{
    // disable Ethernet chip
    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);
    
    Serial.begin(9600);       // for debugging
        
    

    Ethernet.begin(mac);  // initialize Ethernet device
    server.begin();           // start to listen for clients

    Serial.println(Ethernet.localIP());
}

void loop()
{
    EthernetClient client = server.available();  // try to get client

    if (client) {  // got client?
        boolean currentLineIsBlank = true;
        while (client.connected()) {
            if (client.available()) {   // client data available to read
                char c = client.read(); // read 1 byte (character) from client
                // limit the size of the stored received HTTP request
                // buffer first part of HTTP request in HTTP_req array (string)
                // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
                if (req_index < (REQ_BUF_SZ - 1)) {
                    HTTP_req[req_index] = c;          // save HTTP request character
                    req_index++;
                }
                // last line of client request is blank and ends with \n
                // respond to client only after last line received
                if (c == '\n' && currentLineIsBlank) {
                    // send a standard http response header
                    client.println("HTTP/1.1 200 OK");
                    // remainder of header follows below, depending on if
                    // web page or XML page is requested
                    // Ajax request - send XML file
                    if (StrContains(HTTP_req, "ajax_inputs")) {
                        // send rest of HTTP header
                        client.println("Content-Type: text/xml");
                        client.println("Connection: keep-alive");
                        client.println();

                        Serial.println("Here");

                        // print the received text to the LCD if found
                        if (GetLcdText(lcd_buf_1, lcd_buf_2, LCD_BUF_SZ)) {
                          // lcd_buf_1 and lcd_buf_2 now contain the text from
                          // the web page
                          // write the received text to the LCD
                                                    
                          Serial.println(lcd_buf_1);
                          
                          Serial.println(lcd_buf_2);
                        }
                    }
                    else {  // web page request
                        // send rest of HTTP header
                        client.println("Content-Type: text/html");
                        client.println("Connection: keep-alive");
                        client.println();
                        client.println("<!DOCTYPE html>");
                        client.println("<html lang='en'>");
                        client.println("<head>");
                        client.println("<script>");
                        client.println("strLine1 = '';");
                        client.println("strLine2 = '';");
                        client.println("function SendText() {");
                        client.println("nocache = '&nocache=' + Math.random() * 1000000;");
                        client.println("var request = new XMLHttpRequest();");
                        client.println("strLine1 = '&L1=' + document.getElementById('txt_form').line1LCD.value;");
                        client.println("strLine2 = '&L2=' + document.getElementById('txt_form').line2LCD.value;");
                        client.println("request.open('GET', 'ajax_inputs' + strLine1 + strLine2 + nocache, true);");
                        client.println("request.send(null);");
                        client.println("}");
                        client.println("</script>");
                        client.println("</head>");
                        client.println("<body onload='GetArduinoIO()'");
                        client.println("<p><b>Enter text to send to Arduino LCD:</b></p>");
                        client.println("<form id='txt_form' name='frmText'>");
                        client.println("<label>Line 1: <input type='text' name='line1LCD' size='16' maxlength='16' /></label><br /><br />");
                        client.println("<label>Line 2: <input type='text' name='line2LCD' size='16' maxlength='16' /></label>");
                        client.println("</form>");
                        client.println("<br />");
                        client.println("<input type='submit' value='Send Text' onclick='SendText()' />");
                        client.println("</body>");
                        client.println("</html>");

                        Serial.println("lcd_buf_1");
                          
                        Serial.println(lcd_buf_1);
                        Serial.println("lcd_buf_2");
                          
                        Serial.println(lcd_buf_2);
                    }
                    
                    // reset buffer index and all buffer elements to 0
                    req_index = 0;
                    StrClear(HTTP_req, REQ_BUF_SZ);
                    break;
                }
                // every line of text received from the client ends with \r\n
                if (c == '\n') {
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    // a text character was received from client
                    currentLineIsBlank = false;
                }
            } // end if (client.available())
        } // end while (client.connected())
        delay(1);      // give the web browser time to receive the data
        client.stop(); // close the connection
    } // end if (client)
}

// get the two strings for the LCD from the incoming HTTP GET request
boolean GetLcdText(char *line1, char *line2, int len)
{
  boolean got_text = false;    // text received flag
  char *str_begin;             // pointer to start of text
  char *str_end;               // pointer to end of text
  int str_len = 0;
  int txt_index = 0;
  char *current_line;

  current_line = line1;

  // get pointer to the beginning of the text
  str_begin = strstr(HTTP_req, "&L1=");

  for (int j = 0; j < 2; j++) { // do for 2 lines of text
    if (str_begin != NULL) {
      str_begin = strstr(str_begin, "=");  // skip to the =
      str_begin += 1;                      // skip over the =
      str_end = strstr(str_begin, "&");

      if (str_end != NULL) {
        str_end[0] = 0;  // terminate the string
        str_len = strlen(str_begin);

        // copy the string to the buffer and replace %20 with space ' '
        for (int i = 0; i < str_len; i++) {
          if (str_begin[i] != '%') {
            if (str_begin[i] == 0) {
              // end of string
              break;
            }
            else {
              current_line[txt_index++] = str_begin[i];
              if (txt_index >= (len - 1)) {
                // keep the output string within bounds
                break;
              }
            }
          }
          else {
            // replace %20 with a space
            if ((str_begin[i + 1] == '2') && (str_begin[i + 2] == '0')) {
              current_line[txt_index++] = ' ';
              i += 2;
              if (txt_index >= (len - 1)) {
                // keep the output string within bounds
                break;
              }
            }
          }
        } // end for i loop
        // terminate the string
        current_line[txt_index] = 0;
        if (j == 0) {
          // got first line of text, now get second line
          str_begin = strstr(&str_end[1], "L2=");
          current_line = line2;
          txt_index = 0;
        }
        got_text = true;
      }
    }
  } // end for j loop

  return got_text;
}

// sets every element of str to 0 (clears array)
void StrClear(char *str, char length)
{
    for (int i = 0; i < length; i++) {
        str[i] = 0;
    }
}

// searches for the string sfind in the string str
// returns 1 if string found
// returns 0 if string not found
char StrContains(char *str, char *sfind)
{
    char found = 0;
    char index = 0;
    char len;

    len = strlen(str);
    
    if (strlen(sfind) > len) {
        return 0;
    }
    while (index < len) {
        if (str[index] == sfind[found]) {
            found++;
            if (strlen(sfind) == found) {
                return 1;
            }
        }
        else {
            found = 0;
        }
        index++;
    }

    return 0;
}
