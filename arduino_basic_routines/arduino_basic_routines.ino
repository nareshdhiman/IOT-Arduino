/*
   Basic routines for building programs for Linkit One and Arduino boards.

   Program contains following basic rountines:
   display - Displays the text on supported 128x64 oled display screen.
   console - Prints the text on serial monitor.
   readConfig - Reads configuration from config file on flash.
   wifiInit - Initializes Wifi for communicating with Internet.
   mqttInit - Initializes MQTT for publishing and subscribing for events.
   .
   Author: Naresh Dhiman
   .
   Bugs/Enhancments, I like to address:-
   (1) Reconnect Wifi if Wifi gets disconnnected -- FIXED
   (2) Reconnect MQTT if MQTT server gets disconnected -- FIXED
   (3) Support dotted IP Address config for MQTT server -- FIXED
   (4) Display buffer contains httpworld -- ToDO
   (5) Validate MQTT Server IP address -- ToDO

*/

#define SERIAL_BAUD_RATE 9600

/* Display routine initialization
    Uses Seeed OLED library from https://github.com/Seeed-Studio/OLED_Display_128X64
*/
#include <Wire.h>
#include <SeeedOLED.h>
#define BLK_CLS 0         // represents Black Background & Clear Screen
#define BLK_NO_CLS 1      // represents Black Background & No Clear Screen
#define WHT_CLS 2         // represents White Background & Clear Screen
#define WHT_NO_CLS 3      // represents White Background & No Clear Screen
#define OLED_WIDTH 16     // represents Maximum width of OLED Screen
#define OLED_HEIGHT 8      // represents Maximum height og OLED Screen

/* Console routine initialization*/
#define CRLF 1
#define NO_CRLF 0

/* WIFI routine initialization*/
#include <LWiFi.h>
#include <LWiFiClient.h>

/* MQTT routine initialization
    Uses PubSubClient library from https://github.com/knolleary/pubsubclient
*/
#include <PubSubClient.h>

/* File Config */
#include <LFlash.h>
#include <LStorage.h>
#define CONFIG_FILE "arduino_basic_routines.config"

char buff[256];
char* toCArray(String str) {
  str.toCharArray(buff, 256);
  return buff;
}

//-----display routine -------S//
char display_init = 0;
char x_pos = 0;
char y_pos = 0;
// unsigned char ctrl is 8bits and they represent:-
// bit 1 if not set clears screen; bit 2 if not set uses normal display (black background) else inverted display (white background);
void display(unsigned char ctrl, const String str) {
  if (display_init == 0) {
    Wire.begin();
    SeeedOled.init();
    SeeedOled.setPageMode();
    SeeedOled.setHorizontalMode();
    display_init = 1;
    console("Display initialized.", CRLF);
  }
  // Clear the screen and set x and y positions to zero if bit 1 is not set
  if (!(ctrl & 1)) {
    SeeedOled.clearDisplay();
    x_pos = 0;
    y_pos = 0;
  }
  // bit 2 decides display mode
  if (ctrl & 2) {
    SeeedOled.setInverseDisplay();
  } else {
    SeeedOled.setNormalDisplay();
  }
  // Calculate x,y positions according to the str length and new line chars in string
  // Oled screen size is 8 lines x 16 chars
  int len = str.length();
  int i = 0;
  String d_str;
  while (i < len) {
    if (d_str.length() + x_pos < OLED_WIDTH) {
      if (str.charAt(i) == '\n') {
        oledDisplay(d_str);
        x_pos = 0;
        if (++y_pos >= OLED_HEIGHT) y_pos = 0;
        d_str = "";
      } else {
        d_str += str.charAt(i);
      }
      //i++;
    } else {
      oledDisplay(d_str);
      x_pos += d_str.length();
      if (x_pos >= OLED_WIDTH) {
        x_pos = 0;
        if (++y_pos >= OLED_HEIGHT) y_pos = 0;
      }
      d_str = "";
    }
    i++;
  }
  if (sizeof(d_str) > 0) oledDisplay(d_str);
  d_str = "";
}

void oledDisplay(const String str) {
  SeeedOled.setTextXY(y_pos, x_pos);
  SeeedOled.putString(str.c_str());
  sprintf(buff, "Display => %s", str.c_str());
  console(buff, CRLF);
}
//-----display routine -------S//

//-----console routine -------S//
char console_init = 0;
//ctrl=0 - append mode => NO_CRLF, ctrl=1 - adds new line at the end => CRLF
void console(const char *str, unsigned char ctrl) {
  if (console_init == 0) {
    Serial.begin(SERIAL_BAUD_RATE);
    console_init = 1;
    Serial.println("Serial initialized.");
  }

  if (ctrl == CRLF) {
    Serial.println(str);
  } else {
    Serial.print(str);
  }
}
//-----console routine -------E//

//-----Flash read routine -------S//
LFile file;
String wifi_ap, wifi_password, wifi_auth;
String mqtt_server_ip, mqtt_server_port, mqtt_user, mqtt_password, mqtt_client_id, mqtt_topic;
// Parse line string into key name and value pair
void parseLine(String line, String& keyvalue, const String keyname) {
  line.trim();
  int index = line.indexOf('=');
  if (index != -1) {
    if (line.substring(0, index) == keyname) {
      keyvalue = line.substring(index + 1);
      keyvalue.trim();
    }
  }
}
// Validate key name and value pair
int validateKeyValue(const String keyvalue, const String keyname) {
  if (keyvalue.length() < 1) {
    sprintf(buff, "ERROR - invalid config value; %s=%s", keyname.c_str(), keyvalue.c_str());
    console(buff, CRLF);
    return -1;
  } else {
    sprintf(buff, "%s=%s", keyname.c_str(), keyvalue.c_str());
    console(buff, CRLF);
    return 0;
  }
}

int readConfig() {
  LFlash.begin();
  console("Reading configuration...", CRLF);
  file = LFlash.open(CONFIG_FILE, FILE_READ);
  if (file) {
    file.seek(0);
    String line;

    while (file.available()) {
      char c = file.read();

      if (c == '\n') {
        parseLine(line, wifi_ap, "WIFI_AP");
        parseLine(line, wifi_password, "WIFI_PASSWORD");
        parseLine(line, wifi_auth, "WIFI_AUTH");
        parseLine(line, mqtt_server_ip, "MQTT_SERVER_IP");
        parseLine(line, mqtt_server_port, "MQTT_SERVER_PORT");
        parseLine(line, mqtt_user, "MQTT_USER");
        parseLine(line, mqtt_password, "MQTT_PASSWORD");
        parseLine(line, mqtt_client_id, "MQTT_CLIENT_ID");
        parseLine(line, mqtt_topic, "MQTT_TOPIC");

        //reset the line
        line = "";
      } else {
        line += c;
      }
    }

    //validate
    if (validateKeyValue(wifi_ap, "WIFI_AP") == -1) return -1;
    if (validateKeyValue(wifi_password, "WIFI_PASSWORD") == -1) return -1;
    if (validateKeyValue(wifi_auth, "WIFI_AUTH") == -1) return -1;
    if (validateKeyValue(mqtt_server_ip, "MQTT_SERVER_IP") == -1) return -1;
    if (validateKeyValue(mqtt_server_port, "MQTT_SERVER_PORT") == -1) return -1;
    if (validateKeyValue(mqtt_user, "MQTT_USER") == -1) return -1;
    if (validateKeyValue(mqtt_password, "MQTT_PASSWORD") == -1) return -1;
    if (validateKeyValue(mqtt_client_id, "MQTT_CLIENT_ID") == -1) return -1;
    if (validateKeyValue(mqtt_topic, "MQTT_TOPIC") == -1) return -1;

    console("Reading configuration...done", CRLF);
  } else {
    console("ERROR - File can't be opened - ", NO_CRLF);
    console(CONFIG_FILE, CRLF);
  }
}
//-----Flash read routine -------E//

//-----WiFi routine -------S//
LWiFiClient c;
char wifi_init = 0;
void wifiInit() {
  if (wifi_init == 0) {
    LWiFi.begin();
    wifi_init = 1;
  }

  if (!c.connected()) {
    console("Connecting to WiFi...", NO_CRLF);
    LWiFiEncryption enc;
    if (wifi_auth.equalsIgnoreCase("LWIFI_WPA")) enc = LWIFI_WPA;
    if (wifi_auth.equalsIgnoreCase("LWIFI_OPEN")) enc = LWIFI_OPEN;
    if (wifi_auth.equalsIgnoreCase("LWIFI_WEP")) enc = LWIFI_WEP;
    while (0 == LWiFi.connect(wifi_ap.c_str(), LWiFiLoginInfo(enc, wifi_password)))
    {
      delay(1000);
    }
    IPAddress ip = LWiFi.localIP();
    sprintf(buff, "Connected. LocalIP=%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    console(buff, CRLF);

  }
}
//-----WiFi routine -------E//

//-----MQTT routine -------S//
PubSubClient client(c); //client(WiFiClient)
char mqtt_init = 0;

void callback(char* topic, byte * payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) {
    //sprintf(buff, "%c", (char)payload[i]);
    //console(buff, NO_CRLF);
    msg += (char)payload[i];
  }

  sprintf(buff, "Message arrived on [%s] => %s", topic, msg.c_str());
  console(buff, CRLF);
  //display on screen for testing
  display(BLK_CLS, msg.c_str());
}

void mqttInit() {
  if (mqtt_init == 0) {
    int p_dotindex = -1; String octet[4];

    //Split IP address into octets
    int dotindex = mqtt_server_ip.indexOf(".");
    octet[0] = mqtt_server_ip.substring(0, dotindex);

    p_dotindex = dotindex + 1;
    dotindex = mqtt_server_ip.indexOf('.', p_dotindex);
    octet[1] = mqtt_server_ip.substring(p_dotindex, dotindex);

    p_dotindex = dotindex + 1;
    dotindex = mqtt_server_ip.indexOf('.', p_dotindex);
    octet[2] = mqtt_server_ip.substring(p_dotindex, dotindex);

    p_dotindex = dotindex + 1;
    dotindex = mqtt_server_ip.indexOf('.', p_dotindex);
    octet[3] = mqtt_server_ip.substring(p_dotindex, dotindex);

    sprintf(buff, "Parsed MQTT_SERVER_IP=%s %s %s %s", (octet[0]).c_str(), (octet[1]).c_str(), (octet[2]).c_str(), (octet[3]).c_str());
    console(buff, CRLF);
    IPAddress server(octet[0].toInt(), octet[1].toInt(), octet[2].toInt(), octet[3].toInt());
    client.setServer(server, mqtt_server_port.toInt());
    client.setCallback(callback);
    mqtt_init = 1;
  }

  // Loop until we're reconnected
  while (!client.connected()) {
    console("Attempting MQTT connection...", NO_CRLF);
    // Attempt to connect
    if (client.connect(mqtt_client_id.c_str(), mqtt_user.c_str(), mqtt_password.c_str())) {
      console("connected", CRLF);
      client.subscribe(mqtt_topic.c_str());
    } else {
      sprintf(buff, "failed, rc=%d, try again in 5 seconds", client.state());
      console(buff, CRLF);
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
int mqttPublish(const char* topic, const char* msg) {
  if (client.connected()) {
    client.publish(topic, msg);
    return 0;
  } else
    return -1;
}
int mqttSubscribe(const char* topic) {
  if (client.connected()) {
    client.subscribe(topic);
    return 0;
  } else
    return -1;
}
int mqttPoll() {
  if (client.connected()) {
    client.loop();
    return 0;
  } else
    return -1;
}
//-----MQTT routine -------E//

/*---------------------------------------
  ---- MAIN setup and loop functions ----
  ---------------------------------------
*/
int config_error = 0;
void setup() {
  config_error = readConfig();
  if (config_error != 0) return;

  wifiInit();
  mqttInit();
}

void loop() {
  if (config_error == -1) return;

  /*WiFi not available - start again*/
  wifiInit();


  /*

     Write your code here

  */

  /*MQTT poll and reconnect if not connected*/
  if (mqttPoll() == -1) {
    mqttInit();
  }
}


