#include <WS2812FX.h>
#include <TimeLib.h>

extern const char index_html[];
extern const char main_js[];

// QUICKFIX...See https://github.com/esp8266/Arduino/issues/263
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

#define LED_PIN 2                       // 0 = GPIO0, 2=GPIO2 = D4 on the Wemos D1 Mini
#define LED_COUNT 105

#define WIFI_TIMEOUT 30000              // checks WiFi every ...ms. Reset after this time, if WiFi cannot reconnect.
#define HTTP_PORT 80

#define DEFAULT_COLOR 0xFFFFFF  // 0xFF5900
#define DEFAULT_BRIGHTNESS 128 // 0..255
#define DEFAULT_SPEED 9000 // 0..65535
#define DEFAULT_LOOP_SPEED 10000 
#define DEFAULT_MODE FX_MODE_STATIC

unsigned long auto_last_change = 0;
unsigned long last_wifi_check_time = 0;
unsigned long loopSpeed = DEFAULT_LOOP_SPEED;
String modes = "";
uint8_t myModes[] = {}; // *** optionally create a custom list of effect/mode numbers
boolean auto_cycle = false;

WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

char command[30];
char b[2];
char rep[400];

String rr;
String lastArg;
String lastCommand;

void setup(){

  rr = ESP.getResetInfo();
      
  Serial.begin(74880);
  delay(500);
  Serial.println("\n\nStarting...");

  modes.reserve(5000);
  modes_setup();

  Serial.println("WS2812FX setup");
  ws2812fx.init();
  ws2812fx.setMode(DEFAULT_MODE);
  ws2812fx.setColor(DEFAULT_COLOR);
  ws2812fx.setSpeed(DEFAULT_SPEED);
  ws2812fx.setBrightness(DEFAULT_BRIGHTNESS);
  ws2812fx.start();
 
  Serial.println("ready!");
}

void loop() {
  
  ws2812fx.service();

  if (Serial.available()) {
    char c = Serial.read();
    if ((c=='\n') || (c=='\r')){      
      if (strlen(command)>0) {
        if (command[0]=='{') {
          memmove(command, command+1, strlen(command+1) + 1);            
          char c = command[0];
          memmove(command, command+1, strlen(command+1) + 1);
          if (command[strlen(command)-1]=='}') {
            command[strlen(command)-1]='\0';
            handle_set(c,command);  
          } else {
            Serial.println("Command does not end with '}'");
          }
        } else {
          Serial.println("Command does not start with '{'");
        }
      } else {
        Serial.println("No command captured to process.");   
      }
      memset(command,'\0',sizeof(command));
    } else {
      b[0]=c;
      b[1]='\0';
      if (strlen(command)<sizeof(command)) {
        strcat(command,b);
      } else {
        Serial.print("Error: Buffer overrun! Resetting. ");
        Serial.println(command);
        strcpy(command,"");
      }
    }    
  }

  unsigned long now = millis();

  if (auto_cycle && (now - auto_last_change > loopSpeed)) { // cycle effect mode every 10 seconds
    uint8_t next_mode = (ws2812fx.getMode() + 1) % ws2812fx.getModeCount();
//    if (sizeof(myModes) > 0) { // if custom list of modes exists
//      for (uint8_t i=0; i < sizeof(myModes); i++) {
//        if (myModes[i] == ws2812fx.getMode()) {
//          next_mode = ((i + 1) < sizeof(myModes)) ? myModes[i + 1] : myModes[0];
//          break;
//        }
//      }
//    }
    ws2812fx.setMode(next_mode);
//    Serial.print("mode is "); Serial.println(ws2812fx.getModeName(ws2812fx.getMode()));
    auto_last_change = now;
    handle_report();
  }

  now = millis();
  
  if (!auto_cycle && (now - auto_last_change > (loopSpeed+5000))) { // cycle effect mode every 10 seconds
    auto_last_change = now;
    handle_report();  
  }
}

/*
 * Build <li> string for all modes.
 */
void modes_setup() {
  modes = "";
  uint8_t num_modes = sizeof(myModes) > 0 ? sizeof(myModes) : ws2812fx.getModeCount();
  for(uint8_t i=0; i < num_modes; i++) {
    uint8_t m = sizeof(myModes) > 0 ? myModes[i] : i;
    modes += "<li><a href='#'>";
    modes += ws2812fx.getModeName(m);
    modes += "</a></li>";
  }
}

void handle_report() {
  sprintf(rep,"{ \"color\": %d, \"mode\": %d, \"modeName\": \"%s\", \"brightness\":%d, \"speed\": %d, \"autocycle\": \"%s\", \"loopSpeed\": %d, \"time\": \"%d-%d-%d %d:%d:%d\", \"lastCommand\":\"%s\", \"lastArg\":\"%s\", \"resetReason\": \"%s\"}",
    ws2812fx.getColor(), ws2812fx.getMode(), ws2812fx.getModeName(ws2812fx.getMode()), 
    ws2812fx.getBrightness(), ws2812fx.getSpeed(), auto_cycle ? "on" : "off", loopSpeed, 
    year(), month(), day(), hour(), minute(), second(), lastCommand.c_str(), lastArg.c_str(), rr.c_str());
  Serial.println(rep);    
}

void handle_set(char command, char* arg) {

//  Serial.print("handle_set. Command:");
//  Serial.print(command);
//  Serial.print(" Arg:");
//  Serial.println(arg);

  lastCommand = String(command);
  lastArg = String(arg);
  
  if (command == 'r') {
    handle_report();
  }
  
  if (command == 'c') {
    uint32_t tmp = (uint32_t) strtol(arg, NULL, 10);
    if (tmp >= 0x000000 && tmp <= 0xFFFFFF) {
      ws2812fx.setColor(tmp);
//      Serial.print("Color is "); 
//      Serial.println(ws2812fx.getModeName(ws2812fx.getMode()));
      handle_report();
    }
  }

  if (command == 't') {
    uint32_t tmp = (uint32_t) strtol(arg, NULL, 10);
//    Serial.print("Setting time:");
//    Serial.println(tmp);
    setTime(tmp);
//      Serial.print("Time is "); 
//      Serial.println();
    handle_report();
  }

  if (command == 'l') {
    uint32_t tmp = (uint32_t) strtol(arg, NULL, 10);
    if (tmp >= 0 && tmp <= 999999) {
      loopSpeed = tmp;
//      Serial.print("Color is "); 
//      Serial.println(ws2812fx.getModeName(ws2812fx.getMode()));
      handle_report();
    }
  }

  if (command == 'm') {
    uint8_t tmp = (uint8_t) strtol(arg, NULL, 10);
    ws2812fx.setMode(tmp % ws2812fx.getModeCount());
//    Serial.print("Mode is "); 
//    Serial.println(ws2812fx.getModeName(ws2812fx.getMode()));
    handle_report();
  }

  if (command == 'b') {
    if (arg[0] == '-') {
      ws2812fx.setBrightness(ws2812fx.getBrightness() * 0.8);
    } else if (arg[0] == '+') {
      ws2812fx.setBrightness(ws2812fx.getBrightness() * 1.2);    
    } else if (arg[0] == ' ') {
      ws2812fx.setBrightness(min(max(ws2812fx.getBrightness(), 5) * 1.2, 255));
    } else { // set brightness directly
      uint8_t tmp = (uint8_t) strtol(arg, NULL, 10);
      ws2812fx.setBrightness(tmp);
    }
//    Serial.print("Brightness is "); 
//    Serial.println(ws2812fx.getBrightness());
    handle_report();
  }

  if (command == 's') {
    if (arg[0] == '-') {
      ws2812fx.setSpeed(max(ws2812fx.getSpeed(), 5) * 1.2);
    } else if (arg[1] == '+') {
      ws2812fx.setSpeed(max(ws2812fx.getSpeed(), 5) * 0.8);
    } else if (arg[1] == ' ') {
      ws2812fx.setSpeed(ws2812fx.getSpeed() * 0.8);
    } else {
      uint16_t tmp = (uint16_t) strtol(arg, NULL, 10);
      ws2812fx.setSpeed(tmp);
    }
//    Serial.print("Speed is "); 
//    Serial.println(ws2812fx.getSpeed());
    handle_report();
  }

  if (command == 'a') {
    if (arg[0] == '-') {
      auto_cycle = false;
    } else if (arg[0] == '+') {
      auto_cycle = true;
      auto_last_change = millis();
    } else {
      auto_cycle = true;
      auto_last_change = millis();
    }
//    Serial.print("AutoCyle is "); 
//    Serial.println(auto_cycle ? "on" : "off");    
    handle_report();
  }    
}
