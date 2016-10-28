/* vim: set filetype=cpp ts=2 et sw=2 ai : */
#define HTTP_SERVER
#define OTA
#define SYSLOG
#define THINGSPEAK
#include <time.h>
#include <ESP8266WiFi.h>
#include <stdio.h>
#ifdef HTTP_SERVER
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#endif
#ifdef OTA
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#endif
#ifdef SYSLOG
#include <WiFiUdp.h>
#endif
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RCSwitch.h>
#include <Ticker.h>
#include <IPAddress.h>
#include <config.h>
//#include <Timezone.h>
#define RCTRANSMITTER_GPIO  13
#define RCRECEIVER_GPIO     14
#define BUTTONSWITCH_GPIO   0

#define MAX_WAIT 800
#define MAX_CHANGES 80
#define WANTED_PULSE 461

#define DOUBLE_CLICK 700 //Time between two clicks


#define DBG_OUTPUT_PORT Serial


static int16_t rawTemp[2];
static uint8_t humidity;
volatile uint32_t lastTime;
uint32_t Temp_Timer[2]; 
uint32_t click_timer;
static time_t temp_timestamp[2];
static time_t now_t;
//TimeChangeRule usCDT = {"CDT", Second, Sun, Mar, 2, -300};  //UTC - 5 hours
//TimeChangeRule usCST = {"CST", First, Sun, Nov, 2, -360};   //UTC - 6 hours
//Timezone tz(usCDT,usCST);

uint16_t seconds_sincelast[2], lastConnectionTime;
static uint8_t i,j, value[5];
String inputString = "", myIPAddress;
uint8 TSFailAttemp = 0;
enum RX_STATE {
  sync_high,
  pool_sync_low,
  pool_data_high,
  pool_data_low,
  get_data_high,
  get_data_low,
} rx_state = sync_high;

volatile struct bitField {
  bool     tempAvail:      1;
  bool     pooltempAvail:  1;
  bool     BackLight :     1;
  bool     DisplayLine:    1;
  bool     DoubleClick:    1;
  bool     userClick:      1;
  bool     sysLogging:     1;
  bool     collectingMode: 1;
  bool     poolTS:         1;
  bool     weatherTS:      1;
  bool     TSlastConnected:1;
  bool     TSRecord:       1;
} Data;
LiquidCrystal_I2C lcd(0x27, 16, 2); // set the LCD address to 0x27 for a 16 chars and 2 line display
RCSwitch mySwitch = RCSwitch();

Ticker LCDBacklight;

#ifdef SYSLOG
WiFiUDP udp;
IPAddress syslog_server(192,168,0,8);
#endif
#ifdef THINGSPEAK
WiFiClient client;
// ThingSpeak Settings
char thingSpeakAddress[] = "api.thingspeak.com";
String WeatherWriteAPIKey = "5BML9TTANNP4DEYJ";
String PoolWriteAPIKey = "OUARCO4P8AEL38FO";
void ThingSpeakLoop();
void updateThingSpeak(String tsData, String Key);
#endif


#ifdef HTTP_SERVER
ESP8266WebServer server(80);
//holds the current upload
File fsUploadFile;
#endif

//************************************Function Prototype************************/
void nobacklight();
void pauseSensor();
void resumeSensor();

ICACHE_RAM_ATTR void handleInterrupt(void);
ICACHE_RAM_ATTR void userInterrupt(void);
bool crc(uint8_t row[], int cols);
void KitchenLightToggle();
void printWeather(bool sysLog, uint8_t *p_val = NULL);
void printPool(bool sysLog, uint8_t *p_val = NULL);
void transition(RX_STATE);
class qArray 
{
  public:
    qArray()
    {
      items = head = tail = 0;
      contents = (uint16_t*) malloc (sizeof(uint16_t)  * initialSize);
      if (contents == NULL)
        Serial.println("QUEUE: insufficient memory to initialize queue.");
      size = initialSize;
    }

    ~qArray()
    {
      free(contents);
      contents = NULL;
    }
    void reset() {
      uint16_t *tcontents = (uint16_t *) realloc(contents, 32);
      if(tcontents == NULL) 
        Serial.println("Reallocate FAIL!");
      else {
        contents = tcontents;
        size = initialSize;
        items=head=tail= 0;
      }
    }

    //ICACHE_RAM_ATTR void t_resize (const int s) {
    void t_resize (const int s) {
      uint16_t *tcontents = (uint16_t *) realloc(contents, (sizeof (uint16_t) * s));
      if(tcontents == NULL)
        Serial.println("Reallocate FAIL!");
      else {
        contents = tcontents;
        size = s;
      }
    }

    ICACHE_RAM_ATTR void enqueue(const uint16_t i) 
    {
      if (items==size) //Max
        t_resize(size << 1 );
      else if (size == 8192 && items == 0 )
        reset();
      contents[tail++] = i;
      if (tail == size) tail = 0;
      items++;
    }

    uint16_t dequeue()
    {
      uint16_t item = contents[head++];
      items--;
      if (head==size) head = 0;
      return item;
    }
    bool isEmpty() const
    {
      return items == 0;
    }
    int fsize() const
    {
      return size;
    }
    void shrink()
    {
       if (items == 0 && size != initialSize) t_resize(initialSize);
    }
  private:
    static const int initialSize = 16;
    uint16_t *contents;
    int size, items,head, tail;

};
qArray delta_queue;
class debugger 
{
  public:
    void setLevel(uint8_t loglevel) {
      _loglevel = loglevel;
    }

    debugger(uint8_t loglevel) {
      setLevel(loglevel);
    }

    debugger(void) {
      _loglevel = 0;
    }
    void log(const char *msg) {
      if(_loglevel== 1)
        _printMsg(msg);
    }

    void warn(const char* msg) {
      if(_loglevel == 4) {
        DBG_OUTPUT_PORT.print("WARN: ");
        _printMsg(msg);
      }
    }


  private:
    void _printMsg(const char *msg) {
      DBG_OUTPUT_PORT.print(msg);
    }
    uint8 _loglevel;
};

debugger DBG;

void nobacklight() {
  lcd.noBacklight();
}

void KitchenLightToggle() {
  pauseSensor();
  mySwitch.setPulseLength(350);
  mySwitch.send(0x73935C, 24);
  lcd.setCursor(0, Data.DisplayLine);
  Data.DisplayLine = !Data.DisplayLine;
  lcd.print("Kitchen Light   ");
  resumeSensor();
}

void LivingRoomLightSwitch(bool onOff) {
  pauseSensor();
  mySwitch.setPulseLength(600);
  lcd.setCursor(0,0); 
  lcd.print("LivingRoom Light");
  lcd.setCursor(0,1);
  if(onOff) {
    lcd.print("     On         ");
    mySwitch.send(0x6908,16);

  } else {
    lcd.print("     Off        ");
    mySwitch.send(0x6A08,16);
  }
  Data.DisplayLine = !Data.DisplayLine;
  resumeSensor();
}

void LivingRoomFanSwitch(bool onOff) {
  pauseSensor();
  mySwitch.setPulseLength(600);
  lcd.setCursor(0,0); 
  lcd.print("LivingRoom Fan  ");
  lcd.setCursor(0,1);
  if(onOff) {
    lcd.print("     On         ");
    mySwitch.send(0x6888,16);
  } else {
    lcd.print("     Off        ");
    mySwitch.send(0x6848,16);
  }
  resumeSensor();
}
//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

#ifdef HTTP_SERVER
String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload(){
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    DBG_OUTPUT_PORT.print("handleFileUpload Name: "); DBG_OUTPUT_PORT.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
    DBG_OUTPUT_PORT.print("handleFileUpload Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

void handleFileDelete(){
  if(server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  DBG_OUTPUT_PORT.println("handleFileDelete: " + path);
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate(){
  if(server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  DBG_OUTPUT_PORT.println("handleFileCreate: " + path);
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if(file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if(!server.hasArg("dir")) {server.send(500, "text/plain", "BAD ARGS"); return;}

  String path = server.arg("dir");
  DBG_OUTPUT_PORT.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, "text/json", output);
}
#endif

void setup() {
  // put your setup code here, to run once:
  DBG_OUTPUT_PORT.begin(115200);

  lcd.begin();
  lcd.backlight();
  pinMode(BUTTONSWITCH_GPIO, INPUT_PULLUP);

#ifdef HTTP_SERVER
  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {    
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n\r", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    DBG_OUTPUT_PORT.printf("\n");
  }
#endif

  // Setup Wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WiFiSSID, WiFiPSK);
  i = 120; //One minute timeout for DHCP
  DBG_OUTPUT_PORT.println("Connecting");
  lcd.setCursor(0,0);
  lcd.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    DBG_OUTPUT_PORT.print(".");
    lcd.print(".");
    if (--i == 0) {
      DBG_OUTPUT_PORT.println("Connection Fail");
      lcd.print("Connection Fail");
      ESP.restart();
      break;
    }
  }
  IPAddress myip = WiFi.localIP();
  myIPAddress = String(myip[0]) + "." +String(myip[1]) + "." + String(myip[2]) + "." + String(myip[3]);
  /*
     if ( i!=0 ) {
     lcd.setCursor(0,0);  
     lcd.print("IP Address      ");
     lcd.setCursor(0,1);
     lcd.print(myIPAddress + " ");
     DBG_OUTPUT_PORT.print("Connected to ");
     DBG_OUTPUT_PORT.println(WiFi.localIP());
     }
   */

  //Get NTP time
  configTime( 0, 0, "pool.ntp.org", "time.nist.gov"); 
  while (!(time(&now_t))) {
    Serial.print(".");
    delay(1000);
  }
  char buffer[80] = "ESP Reset time ";
  //temp_timestamp[0] = tz.toLocal(temp_timestamp[0]);
  strcat(buffer, ctime(&now_t));
  Serial.println(buffer);
#ifdef SYSLOG
  udp.beginPacket(syslog_server, 514);
  udp.write(buffer, strlen(buffer));
  udp.endPacket();
#endif
#ifdef HTTP_SERVER
  MDNS.begin(host);
  DBG_OUTPUT_PORT.print("Open http://");
  DBG_OUTPUT_PORT.print(host);
  DBG_OUTPUT_PORT.println(".local/edit to see the file browser");

#ifdef OTA
  ArduinoOTA.onStart([]() {
      Serial.println("OTA Start");
      pauseSensor();
      });
  ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
      });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      });
  ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
      });
  ArduinoOTA.begin();
#endif

  //SERVER INIT
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //load editor
  server.on("/edit", HTTP_GET, [](){
      if(!handleFileRead("/edit.htm")) server.send(404, "text/plain", "FileNotFound");
      });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([](){
      if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
      });

  //get heap status, analog input value and all GPIO statuses in one json call
  server.on("/all", HTTP_GET, [](){
      String json = "{";
      json += "\"heap\":"+String(ESP.getFreeHeap());
      json += ", \"analog\":"+String(analogRead(A0));
      json += ", \"gpio\":"+String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
      json += "}";
      server.send(200, "text/json", json);
      json = String();
      });
  server.on("/pool", HTTP_GET, []() {
      if(Data.pooltempAvail) {
      String json = "{";
      json += "\"RawTemp\":" + String(rawTemp[1]);
      json += ", \"Time\":" + String(temp_timestamp[1]);
      json += "}";
      server.send(200, "text/json", json);
      json = String();
      } else {
      server.send(500, "text/plain",  "DATA NOT AVAILABLE");
      }
      });
  server.on("/weather", HTTP_GET, []() {
      if(Data.tempAvail) {
      String json = "{";
      json += "\"RawTemp\":" + String(rawTemp[0]);
      json += ", \"Humidity\":" + String(humidity);
      json += ", \"Time\":" + String(temp_timestamp[0]);
      json += "}";
      server.send(200, "text/json", json);
      json = String();
      } else {
      server.send(500, "text/plain",  "DATA NOT AVAILABLE");
      }
      });
  server.on("/switch", HTTP_GET, []() {
      bool failed = false;
      if(server.hasArg("type") && server.hasArg("status")) {
      switch (server.arg("type").toInt()) {
      case 0:
      KitchenLightToggle();
      break;
      case 1:
      LivingRoomLightSwitch(server.arg("status").toInt());
      break;
      case 2:
      LivingRoomFanSwitch(server.arg("status").toInt());
      break;
      default:
      failed = true;
      break;
      }
      } else {
      failed = true;
      }
      if(failed) {
        server.send(500,"text/plain", "BAD ARGS");
      } else {
        server.send(200,"text/plain", "");
      }
  });
  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");

#endif

  //delta_queue.setPrinter(DBG_OUTPUT_PORT);
  DBG_OUTPUT_PORT.println("Attaching 433 Sensor to GPIO Interrupt");
  lastConnectionTime = click_timer = Temp_Timer[0] = Temp_Timer[1] = lastTime = micros(); //establish baseline time
  mySwitch.enableTransmit(RCTRANSMITTER_GPIO);
  Data.poolTS = false;
  Data.weatherTS = false;
  Data.tempAvail = false;
  Data.pooltempAvail = false;
  Data.BackLight = true;
  Data.userClick = false;
  Data.DisplayLine = 0;
  Data.BackLight = true;
  Data.sysLogging = true;
  Data.collectingMode = false;
  Data.TSlastConnected = false;
  Data.TSRecord = true;
  LCDBacklight.attach(5, nobacklight); 
  transition(sync_high);
  // client.connect(thingSpeakAddress, 80); //Establish a connection to thingspeak
  attachInterrupt(RCRECEIVER_GPIO, handleInterrupt, CHANGE);
  attachInterrupt(BUTTONSWITCH_GPIO,  userInterrupt, RISING);
} //end of setup()

void transition(RX_STATE tmp) {
  switch (tmp) {
    case sync_high:
      i = 0;
      j = 0;
      value[0] = 0;
      Data.collectingMode = false;
      rx_state = sync_high;
      break;
    default:
      break;
  }
}

void checkRCTransmitter() {
  while (!delta_queue.isEmpty()) { // We got data
    uint16_t delta = delta_queue.dequeue();
    switch(rx_state) {
      case sync_high:
        if (8500 < delta && delta < 9000) {
          //	DBG.log("S %d %d\n", (delta - 8500), (9000 - delta));
          DBG.log("S");
          rx_state = get_data_high;
        } else if (460 < delta && delta < 670 ) {
          //DBG.log("S+"); //Too noisy
          rx_state = pool_sync_low;
        }
        break;
      case pool_sync_low:
        if (3200 < delta && delta < 4200 ) {
          rx_state = pool_data_high;
          DBG.log("#");
          Data.collectingMode = true;
        } else if (8500 < delta && delta < 9000) {
          rx_state = get_data_high;
        } else {
          rx_state=sync_high;
        }
        break;
      case pool_data_high: 
        if (460 < delta && delta < 926) {
          rx_state = pool_data_low;
        } else {
          transition(sync_high);
        }
        break;
      case pool_data_low:
        if ( delta > 529 && delta < 2125 ) {
          value[j] = value[j] << 1;
          if (delta < 1100 ) { //0
            DBG.log("0");
            value[j] = value[j] & 0b11111110;
          } else {
            DBG.log("1");
            value[j] = value[j] | 0b00000001;
          }
          i++;
          rx_state = pool_data_high;
          if (i == 8 ) {
            DBG.log(" ");
            j++;
            if ( j > 10) {
              DBG_OUTPUT_PORT.printf("Error: we got more than 5 bytes!\n");
              j=0;
            }
            value[j] = 0;
            i=0;
          }
        } else {
          if (j== 4 && i == 4) {
            Data.pooltempAvail = true;
            uint32_t temp_t = micros();
            //temp_timestamp[1] = tz.toLocal(time(nullptr));
            temp_timestamp[1] = time(NULL);
            seconds_sincelast[1] = (temp_t - Temp_Timer[1]) / 1000000;
            rawTemp[1] =  (((int16_t) value[1] & 0x1) << 8) | (int16_t) value[2];
            if (seconds_sincelast[1] != 0 ) {
              printPool(true, value);
            }
            Temp_Timer[1] = temp_t;
          }
          if (3200 < delta && delta < 4200) {
            DBG.log("S+");
            i=0;
            j=0;
            rx_state = pool_data_high;
          } else {
            DBG.log("|");
            //DBG.log(delta);
            DBG.log("\n\r");
            //rx_state = sync_high;
            transition(sync_high);
          }
        }
        break;
      case get_data_high: 
        if ( 460 < delta && delta < 670 ) {
          rx_state = get_data_low;
        } else {
          DBG.log("^");
          //DBG.log(delta);
          //rx_state = sync_high;
          transition(sync_high);
        }
        break;
      case get_data_low:
        if ( delta > 900 && delta < 2010 ) {
          value[j] = value[j] << 1;
          if (delta < 1100 ) { //0
            DBG.log("0");
            value[j] = value[j] & 0b11111110;
          } else {
            DBG.log("1");
            value[j] = value[j] | 0b00000001;
          }
          i++;
          rx_state = get_data_high;
          if (i == 8 ) {
            DBG.log(" ");
            j++;
            if ( j > 5) {
              DBG_OUTPUT_PORT.printf("Error: we got more than 5 bytes!\n");
              j=0;
            } else if (j == 5) {
              if (crc(value,5)) {
                Data.tempAvail = true;
                uint32_t temp_t = micros();
                //Grab a timestamp;
                //temp_timestamp[0] = tz.toLocal(time(nullptr));
                temp_timestamp[0] = time(NULL);
                seconds_sincelast[0] = (temp_t - Temp_Timer[0])/1000000;
                rawTemp[0] =  (((int16_t) value[1] & 0x1) << 8) | (int16_t) value[2];
                humidity = value[3];
                if (seconds_sincelast[0] != 0 ) {
                  printWeather(true, value);
                }
                lcd.setCursor(0, Data.DisplayLine);
                Data.DisplayLine = !Data.DisplayLine;
                lcd.print((float) (value[2] * 9.0 /50.0) + 32.0);
                lcd.print("F ");
                lcd.print(value[3]);
                lcd.print("%");
                //rx_state = sync_high;
                transition(sync_high);
                Temp_Timer[0] = temp_t;
              } else {
                DBG_OUTPUT_PORT.println("Bad Checksum");
              }
              j=0;
            }
            value[j] = 0;
            i=0;
          }
        } else {
          if (45000 < delta) {
            DBG.log("E");
          } else if (8500 < delta && delta < 11100) {
            DBG.log("S");
          } else {
            DBG.log("|");
          }
          transition(sync_high);
          //rx_state = sync_high;
        }
        break;
      default:
        DBG_OUTPUT_PORT.println("Error state");
        break;
    }
    yield();
  } //End of While
}
void checkButtonTransmitter() {
  if (Data.userClick) {
    DBG_OUTPUT_PORT.println("User Click!");
    lcd.backlight();
    LCDBacklight.attach(5, nobacklight); 
    // Double click
    if ((millis() - click_timer) < DOUBLE_CLICK ) {
      Data.DoubleClick = true;
      lcd.setCursor(0,Data.DisplayLine);
      Data.DisplayLine = !Data.DisplayLine;
      lcd.print("User Click");
      KitchenLightToggle();
    } else {
      click_timer =  millis();
    }
    Data.userClick = false;
  }
}
void serialEvent1() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    //Serial.write(inChar);
    // add it to the inputString:
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n' || inChar == '\r') {
      if (inputString == "reset") {
        Serial.println("User Request Reset");
        delay(3000); //Give 3 seconds
        ESP.restart();
      } else if (inputString == "ip") {
        Serial.println(WiFi.localIP());
      } else if (inputString == "uptime") {
        Serial.println(WiFi.localIP());
      } else if (inputString == "heap") {
        Serial.println(ESP.getFreeHeap());
   //   } else if (inputString == "reset") {
   //     Serial.println(delta_queue.reset());
      } else if (inputString == "size") {
        Serial.println(delta_queue.fsize());
      } else if (inputString == "getFlashChipRealSize") {
        Serial.println(ESP.getFlashChipRealSize());
      } else if (inputString == "getFlashChipId") {
        Serial.println(ESP.getFlashChipId());
      } else if (inputString == "getBootVersion") {
        Serial.println(ESP.getBootVersion());
      } else if (inputString == "getSdkVersion") {
        Serial.println(ESP.getSdkVersion());
      } else if (inputString == "getChipId") {
        Serial.println(ESP.getChipId());
      } else if (inputString == "ts off") {
        Data.TSRecord = false;
      } else if (inputString == "ts on") {
        Data.TSRecord = true;
      } else if (inputString == "debug on") {
        Serial.println("Debug on");
        DBG.setLevel(1);
      } else if (inputString == "debug off") {
        Serial.println("Debug off");
        DBG.setLevel(0);
      } else if (inputString == "pool") {
        printPool(false);
      } else if (inputString == "weather") {
        printWeather(false);
      } else if (inputString == "date") {
        //time_t tmp = tz.toLocal(time(nullptr));
        now_t = time(NULL);
        Serial.println(ctime(&now_t));
      } else if (inputString == "log on") {
        Data.sysLogging = true;
        Serial.println("Logging to syslog");
      } else if (inputString == "log off") {
        Data.sysLogging = true;
        Serial.println("Logging to syslog off");
      } else {
        Serial.print("Command \'");
        Serial.print(inputString);
        Serial.println("\' not supported");
      }
      inputString = ""; //reset inputString
    } else {
      inputString += inChar;
    }
  }
}

void loop() {
  // put your main code here, to run repeatedly:
#ifdef HTTP_SERVER
  server.handleClient();
#endif
  checkRCTransmitter();
  checkButtonTransmitter();
#ifdef OTA
  ArduinoOTA.handle();
#endif
  serialEvent1();
#ifdef THINGSPEAK
  ThingSpeakLoop();
#endif

}

ICACHE_RAM_ATTR void userInterrupt() {
  Data.userClick = true;
}

char* formatTime( time_t *now) 
{
  char *cTime = (char*) malloc(25);
  if (cTime) {
    tm *t_time = localtime(now);
    snprintf(cTime, 25, "%04d-%02d-%02dT%02d:%02d:%02d-0%d:00" \
        , t_time->tm_year +1900 \
        , t_time->tm_mon + 1 \
        , t_time->tm_mday \
        , t_time->tm_hour \
        , t_time->tm_min \
        , t_time->tm_sec \
        , (t_time->tm_isdst) ? 5 : 6 );
  //  printf("%s\n" , cTime);
     return cTime;
  } else {
    DBG_OUTPUT_PORT.println("Can't allocate memory!");
    return NULL;
  }
}

void printPool(bool sysLog, uint8_t *p_val) {
  char buffer[120] = "";
  char holder[10] = "";
  static int16_t lastTemp;
  strcat(buffer, "Pool Temperature: " );
  dtostrf((float) rawTemp[1] / 10.0, 5, 1, holder);
  strncat(buffer, holder, 120);
  strncat(buffer, "C ",120);
  dtostrf((float) (rawTemp[1] * 9.0 / 50.0) + 32.0, 6,1,holder);
  strncat(buffer,holder,120);
  strncat(buffer, "F",120);
  if (p_val != NULL) {
    strncat(buffer, " 0x", 120);
    for(uint8 i = 0; i < 5; i++) {
      snprintf(holder, 120, "%02X", p_val[i]);
      strncat(buffer,holder,120);
    }
  }
  if(lastTemp != rawTemp[1]) {
    Data.poolTS = true;
    DBG_OUTPUT_PORT.print("*");
  }
  char *tmp_time = ctime(&temp_timestamp[1]);
  tmp_time[strlen(tmp_time) -1 ] = 0;
  DBG_OUTPUT_PORT.print(tmp_time);
  DBG_OUTPUT_PORT.printf(" Seconds ago: %3d ", seconds_sincelast[1]);
  DBG_OUTPUT_PORT.println(buffer);
#ifdef SYSLOG
  if (Data.sysLogging && sysLog && Data.poolTS) {
    char *tmp_str = formatTime(&temp_timestamp[1]);
    String syslogHeader = "<14>1 " \
                           +  String(tmp_str) \
                           + " " \
                           + myIPAddress \
                           + " ESP8266 - POOL - " \
                           + String(buffer);
    DBG.log(syslogHeader.c_str());
    udp.beginPacket(syslog_server, 514);
    udp.write(syslogHeader.c_str(), syslogHeader.length() );
    free(tmp_str);
    udp.endPacket();
  }
#endif
  lastTemp = rawTemp[1];
}

void printWeather(bool sysLog, uint8_t *p_val) {
  char buffer[120] = "";
  char holder[10] = "";
  static int16_t lastTemp;
  static uint8_t lastHum;
  strcat(buffer, "Weather Temperature: " );
  dtostrf((float) rawTemp[0] / 10.0, 5, 1, holder);
  strcat(buffer, holder);
  strcat(buffer, "C ");
  dtostrf((float) (rawTemp[0] * 9.0 / 50.0) + 32.0, 6,1,holder);
  strcat(buffer,holder);
  strcat(buffer, "F Humidity: ");
  sprintf(holder, "%d%%", humidity);
  strcat(buffer,holder);
  if (p_val != NULL) {
    strncat(buffer, " 0x", 120);
    for(uint8 i = 0; i < 5; i++) {
      snprintf(holder, 120, "%02X", p_val[i]);
      strncat(buffer,holder,120);
    }
  }
  if(lastTemp != rawTemp[0] || lastHum != humidity) {
    Data.weatherTS = true;
    DBG_OUTPUT_PORT.print("*");
  }
  char *tmp_time = ctime(&temp_timestamp[0]);
  tmp_time[strlen(tmp_time) -1 ] = 0;
  DBG_OUTPUT_PORT.print(tmp_time);
  DBG_OUTPUT_PORT.printf(" Seconds ago: %3d ", seconds_sincelast[0]);
  DBG_OUTPUT_PORT.println(buffer);
#ifdef SYSLOG
  if (Data.sysLogging && sysLog && Data.weatherTS) {
    char *tmp_str = formatTime(&temp_timestamp[0]);
    String syslogHeader = "<14>1 " \
                           +  String(tmp_str) \
                           + " " \
                           + myIPAddress \
                           + " ESP8266 - WEATHER - " \
                           + String(buffer);
    udp.beginPacket(syslog_server, 514);
    udp.write(syslogHeader.c_str(), syslogHeader.length());
    udp.endPacket();
    free(tmp_str);
  }
#endif
  lastTemp = rawTemp[0];
  lastHum = humidity;

}

void ICACHE_RAM_ATTR handleInterrupt() {
  int32_t time = micros();
  uint16_t delta  = time - lastTime;
  if (delta > WANTED_PULSE){
    delta_queue.enqueue(delta);
    lastTime = time;
  } else if(!Data.collectingMode ){
    lastTime = time;
  }
}

bool crc(uint8_t row[], int cols) {
  // sum of first n-1 bytes modulo 256 should equal nth byte
  cols -= 1; // last byte is CRC
  int sum = 0;
  uint8_t i;
  for (i = 0; i < cols; i++) {
    sum += row[i];
  }    
  if (sum != 0 && sum % 256 == row[cols]) {
    return true;
  } else {
    return false;
  }
}
#ifdef THINGSPEAK
void ThingSpeakLoop() {
  if (!Data.TSRecord)
    return;
  if(client.available())
  {
    client.read();
  }
  // Data Available send to ThingSpeak
  if (Data.TSlastConnected && !client.connected()) {
    Data.TSlastConnected == false;
    Serial.println("...TS done");
    client.stop();
  }
  if (Data.poolTS) {
    Serial.print("Sending Pool data to TS");
    updateThingSpeak("field1="+ String((float)rawTemp[1]/10.0) + "&field2=" + String( (float)rawTemp[1] * 9.0/50.0 +32.0), PoolWriteAPIKey);
    Data.poolTS = false;


  }
  if (Data.weatherTS) {
    Serial.print("Sending Weather data to TS");
    updateThingSpeak("field1="+ String((float)rawTemp[0]/10.0) + "&field2=" +String( (float)rawTemp[0] * 9.0/50.0 +32.0) + "&field3="+String(humidity), WeatherWriteAPIKey); 
    Data.weatherTS = false;
  }
  Data.TSlastConnected = client.connected();
}
void resumeSensor() {
  attachInterrupt(RCRECEIVER_GPIO, handleInterrupt, CHANGE);
}
void pauseSensor() {
  detachInterrupt(RCRECEIVER_GPIO);
  transition(sync_high);
  delta_queue.reset();
}
void updateThingSpeak(String tsData, String writeAPIKey)
{
  //pauseSensor();
  if (client.connect(thingSpeakAddress,80))
  {         
    //Serial.println("Connected to ThingSpeak...");
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: "+writeAPIKey+"\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(tsData.length());
    client.print("\n\n");
    client.print(tsData);
    lastConnectionTime = millis();
  } else {
    Serial.print("Connecting to ThingSpeak failed!\n\r");
    TSFailAttemp++;
    if (TSFailAttemp >= 5 ) {
      
    }
  }
  //resumeSensor();
}

#endif
