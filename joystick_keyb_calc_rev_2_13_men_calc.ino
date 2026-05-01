//scritto da: Alessandro Mammino
//mail: krafen885@hotmail.com
//ultima: la più libera possibile, ognuno faccia quello che vuole. GNU GPL

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <math.h>
#include <deque>
#include <string.h>
#include <stdlib.h>
#include <string> 
//#include <iostream> 

#include <Bounce2.h>

#include <USB.h> 
#include <USBCDC.h>
USBCDC USBSerial;
#include <USBHIDKeyboard.h>
USBHIDKeyboard Keyboard;

#include <USBHIDMouse.h>
USBHIDMouse Mouse;

#include <Keypad.h>

#include <algorithm>
#include <Preferences.h>

Preferences prefs;

// Configurazione I2C e dimensioni del display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128
#define OLED_RESET -1 // Usato per i display senza pin di reset fisico
Adafruit_SH1107 display = Adafruit_SH1107(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// KEYPAD MATRIX CHARACTERISTICS
#define ROWS 6
#define COLS 4

char hexaKeys[ROWS][COLS] = {
  {'A','B','C','D'},
  {'E','F','G','H'},
  {'I','J','K','L'},
  {'M','N','O','P'},
  {'Q','R','S','T'},
  {'U','V','W','X'},
};

byte rowPins[ROWS] = {15 , 16 , 17 , 18 , 8 , 3}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {4 , 5 , 6 , 7}; //connect to the column pinouts of the keypad

Keypad kpad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

//JOYSTICK PINS
#define vertJoyPIN 1
#define horJoyPIN 2
#define buttonJoyPIN 40

//JOYSTICK SETUP  — ora tutte variabili, caricate da NVS nel setup()
#define WINDOW_SIZE 5     // Dimensione della finestra di lettura (strutturale, rimane #define)
#define singleScroll 1

// Valori di default — NVS sovrascrive questi al boot se esistono
int   cfg_deadBand        = 35;
long  cfg_sensitivityScrl = 30000;  // CONTRARIO della sensitivity
int   cfg_fixSensScrol    = 20;
long  cfg_screensaverMs   = 30000;  // ms inattività → screensaver
int   cfg_screensaverFps  = 100;    // ms per frame screensaver
bool  cfg_calcDegrees     = true;   // true = gradi, false = radianti
int   cfg_scrollMult      = 1;      // moltiplicatore scroll rotella (1-10)
int   cfg_screensaverType = 0;      // 0 = matrix, 1 = grafici CPU/GPU temp

// Instantiate a Bounce object
Bounce debouncer = Bounce();

//VARIABILI
String jState = "COMMA mode";
String nState = "";

bool joyConf = false;
bool screenUpdate = true;
bool clickDisplay = false;
bool clrScr = false;
bool SscreenUpdate = false;
bool scrollScrExit = false;
bool displayDirty = false;

int vertZero, horZero; // Stores the initial value of each axis, usually around 512
int vertRawValue, horRawValue;
int scrollPeriod;

int long lastVertScroll = 0, lastHorScroll = 0;
int long keyStrCount = 0, ScrollCount = 0;
int long printTimerEnd = 0, lastActionTimer = 0;

int readingsHor[WINDOW_SIZE];
int readingsVert[WINDOW_SIZE];

int mouseClickFlag = 0;
int keyFuncState = 1;
int prevKeyFstate = 0;
char numBak1[20] = "w";
char numBak2[20] = "w";
char numBak3[20] = "w";

//VARIABILI PER CALCOLATRICE
const int stackSizeVar = 30;
#define MAX_STACK_SIZE stackSizeVar //era 10. aumentato a 30!
std::deque<double> stack;
std::string currentInput = "";  // Buffer per il numero corrente
bool isScientific = false;  // Flag per la gestione della notazione scientifica

double lastPush = 0.0;

char buffer[50]; //buffer di stampa a schermo
char lastCalcNum[50]; //buffer di stampa su keypad

// ── DATI CPU/GPU ricevuti via Serial dal PC ──────────────────────────────────
// -1 = nessun dato disponibile (niente PC connesso o timeout)
int pcCpuPct  = -1;
int pcCpuTemp = -1;
int pcGpuPct  = -1;
int pcGpuTemp = -1;
int pcRamPct  = -1;   // RAM %
int pcNetMbps = -1;   // velocità rete (Mbps, parte intera)

#define PC_SERIAL_TIMEOUT 6000  // ms senza pacchetti → nascondi valori

unsigned long lastPcPacket = 0; // timestamp ultimo pacchetto ricevuto

// ── CONFIG MENU ──────────────────────────────────────────────────────────────
// keyFuncState == 4  → menu configurazione attivo
int  cfgMenuIndex   = 0;    // voce selezionata
bool cfgMenuDirty   = true; // richiede ridisegno
bool dHoldConsumed  = false; // evita re-trigger continuo HOLD su tasto D

// ── CALC FUNCTION MENU ───────────────────────────────────────────────────────
// keyFuncState == 5  → menu funzioni calcolatrice (HOLD shift in calc mode)
int  calcFnMenuIndex  = 0;
bool calcFnMenuDirty  = true;
bool aHoldConsumed    = false;  // evita re-trigger HOLD su tasto A

struct CalcFnEntry {
  const char* label;   // nome mostrato a schermo
  const char* desc;    // descrizione breve (arity)
  char        opChar;  // char passato a performOperationFromKey
};

static const CalcFnEntry CALC_FN_MENU[] = {
  { "log10",   "log10(x)",      'L' },
  { "ln",      "ln(x)",         'l' },
  { "exp",     "e^x",           'E' },
  { "log2",    "log2(x)",       'A' },
  { "hypot",   "sqrt(a^2+b^2)", 'h' },
  { "atan2",   "atan2(y,x)",    'o' },
  { "floor",   "floor(x)",      'f' },
  { "ceil",    "ceil(x)",       'k' },
  { "abs",     "|x|",           'B' },
  { "mod",     "a mod b",       'M' },
  { "round",   "round(x)",      'R' },
  { "deg>rad", "x*pi/180",      'd' },
  { "rad>deg", "x*180/pi",      'D' },
  { "lcm",     "lcm(a,b)",      'K' },
  { "mean",    "media stack",   'V' },
};
const int CALC_FN_MENU_COUNT = (int)(sizeof(CALC_FN_MENU) / sizeof(CALC_FN_MENU[0]));

struct CfgEntry {
  const char* label;
  enum Type { INT_T, LONG_T, BOOL_T } type;
  void* ptr;
  long  minVal, maxVal, step;
};

CfgEntry cfgEntries[] = {
  //  label        tipo               ptr                    min     max    step
  { "Deadband",  CfgEntry::INT_T,  &cfg_deadBand,           5,    200,      1 },
  { "ScrolS",    CfgEntry::LONG_T, &cfg_sensitivityScrl,  5000,  80000,  2500 },
  { "ScrolF",    CfgEntry::INT_T,  &cfg_fixSensScrol,        1,    100,      1 },
  { "SaverSec",  CfgEntry::LONG_T, &cfg_screensaverMs,    5000, 300000,  2500 },
  { "SaverFPS",  CfgEntry::INT_T,  &cfg_screensaverFps,     50,    500,     25 },
  { "CalcDeg",   CfgEntry::BOOL_T, &cfg_calcDegrees,         0,      1,      1 },
  { "ScrolMul",  CfgEntry::INT_T,  &cfg_scrollMult,          1,     10,      1 },
  { "SaverTyp", CfgEntry::INT_T,  &cfg_screensaverType,      0,      1,      1 },
};
const int CFG_ENTRIES_COUNT = (int)(sizeof(cfgEntries) / sizeof(cfgEntries[0]));

long cfgGetLong(int idx) {
  switch (cfgEntries[idx].type) {
    case CfgEntry::INT_T:  return (long)(*(int*)cfgEntries[idx].ptr);
    case CfgEntry::LONG_T: return *(long*)cfgEntries[idx].ptr;
    case CfgEntry::BOOL_T: return (long)(*(bool*)cfgEntries[idx].ptr);
  }
  return 0;
}

void cfgSetLong(int idx, long val) {
  val = constrain(val, cfgEntries[idx].minVal, cfgEntries[idx].maxVal);
  switch (cfgEntries[idx].type) {
    case CfgEntry::INT_T:  *(int*)cfgEntries[idx].ptr  = (int)val;  break;
    case CfgEntry::LONG_T: *(long*)cfgEntries[idx].ptr = val;        break;
    case CfgEntry::BOOL_T: *(bool*)cfgEntries[idx].ptr = (bool)val;  break;
  }
}

void cfgLoad() {
  prefs.begin("cfg", true);
  cfg_deadBand        = prefs.getInt ("deadBand",  cfg_deadBand);
  cfg_sensitivityScrl = prefs.getLong("sensScrL",  cfg_sensitivityScrl);
  cfg_fixSensScrol    = prefs.getInt ("fixScrol",  cfg_fixSensScrol);
  cfg_screensaverMs   = prefs.getLong("saverMs",   cfg_screensaverMs);
  cfg_screensaverFps  = prefs.getInt ("saverFps",  cfg_screensaverFps);
  cfg_calcDegrees     = prefs.getBool("calcDeg",   cfg_calcDegrees);
  cfg_scrollMult      = prefs.getInt ("scrolMul",  cfg_scrollMult);
  cfg_screensaverType = prefs.getInt ("saverTyp",  cfg_screensaverType);
  joyConf             = prefs.getBool("joyConf",   false);
  prefs.end();
}

void cfgSave() {
  prefs.begin("cfg", false);
  prefs.putInt ("deadBand", cfg_deadBand);
  prefs.putLong("sensScrL", cfg_sensitivityScrl);
  prefs.putInt ("fixScrol", cfg_fixSensScrol);
  prefs.putLong("saverMs",  cfg_screensaverMs);
  prefs.putInt ("saverFps", cfg_screensaverFps);
  prefs.putBool("calcDeg",  cfg_calcDegrees);
  prefs.putInt ("scrolMul", cfg_scrollMult);
  prefs.putInt ("saverTyp", cfg_screensaverType);
  prefs.putBool("joyConf",  joyConf);
  prefs.end();
}
// ─────────────────────────────────────────────────────────────────────────────

// parser stringa: "CPU:45 CPUT:72 GPU:30 GPUT:65 RAM:60 NET:12.3\n"
void readPcSerial() {
  static char lineBuf[96];
  static int  linePos = 0;

  // timeout: se non arriva niente da abbastanza tempo, azzera a -1
  if (lastPcPacket > 0 && (millis() - lastPcPacket) > PC_SERIAL_TIMEOUT) {
    pcCpuPct = pcCpuTemp = pcGpuPct = pcGpuTemp = pcRamPct = pcNetMbps = -1;
    lastPcPacket = 0;
  }

  while (USBSerial.available()) {
    char c = USBSerial.read();
    if (c == '\n' || linePos >= 94) {
      lineBuf[linePos] = '\0';
      linePos = 0;
      char *p;
      p = strstr(lineBuf, "CPU:");
      if (p)  pcCpuPct  = atoi(p + 4);
      p = strstr(lineBuf, "CPUT:");
      if (p)  pcCpuTemp = atoi(p + 5);
      p = strstr(lineBuf, "GPU:");
      if (p)  pcGpuPct  = atoi(p + 4);
      p = strstr(lineBuf, "GPUT:");
      if (p)  pcGpuTemp = atoi(p + 5);
      p = strstr(lineBuf, "RAM:");
      if (p)  pcRamPct  = atoi(p + 4);
      p = strstr(lineBuf, "NET:");
      if (p)  pcNetMbps = (int)atof(p + 4);  // tronca decimali
      lastPcPacket = millis();
    } else {
      lineBuf[linePos++] = c;
    }
  }
}

// stampa un valore int o "-" se è -1
void printPcVal(int val) {
  if (val < 0) display.print("-");
  else         display.print(val);
}


//INIZIO SETUP

void setup() {
  // USB nativa ESP32-S3: registra CDC, poi USB.begin()
  USBSerial.begin(115200);
  USB.begin();
  Serial.begin(115200);
  
  // ######################## INIZIALIZZAZIONE MOUSE 1 ##########################
  pinMode(vertJoyPIN, INPUT);  // set button select pin as input
  pinMode(horJoyPIN, INPUT);  // set button select pin as input
  pinMode(buttonJoyPIN, INPUT_PULLUP);  // set button select pin as input

  // ######################## INIZIALIZZAZIONE SCHERMO ##########################

  // Inizializzazione I2C
  Wire.begin(47, 21); // SDA, SCL
  Wire.setClock(1000000);  // 1 MHz (Fast Mode Plus) — supportato da SH1107
  // Inizializzazione del display

  display.begin(0x3C);
  display.setRotation(3);
  // Configurazione del display
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("Go home,");
  display.println("go in the mountains");
  display.println("  ");
  display.setTextSize(1);
  display.println("do not touch joystick for any reason!!");
  display.display();
  display.setTextSize(1);

  delay(1000);


  // SETUP CALCOLATRICE
  stack.clear();  // Inizializza lo stack

  // ######################## INIZIALIZZAZIONE MOUSE 2 ##########################

  // After setting up the button, setup the Bounce instance :
  debouncer.attach(buttonJoyPIN);
  debouncer.interval(5); // interval in ms
  
  for (int i = 0; i <= 100; i += 5) {
        display.fillRect(10, 100, i, 10, 1);
        display.display();
        delay(200);
  }

  delay(1000);  // short delay to let outputs settle
  cfgLoad();   // carica configurazione da NVS
  vertZero = analogRead(vertJoyPIN);  // lettura del valore zero per il joystick orizzontale. non muovere lo stick all'accensione
  horZero = analogRead(horJoyPIN);  // lettura del valore zero per il joystick verticale. non muovere lo stick all'accensione
  Serial.println("vertZero");
  Serial.println(vertZero);
  Serial.println("horZero");
  Serial.println(horZero);
  Mouse.begin();
  display.clearDisplay();
  display.display();

  for (int i = 0; i < WINDOW_SIZE; i++) readingsHor[i] = 0;
  for (int i = 0; i < WINDOW_SIZE; i++) readingsVert[i] = 0;
  // ######################## INIZIALIZZAZIONE TASTIERA ##########################
  Keyboard.begin();
  Keyboard.releaseAll();

  // ######################## INIZIALIZZAZIONE ????? ##########################
  lastActionTimer = millis();
}

// ######################## MAIN LOOP ##########################

void loop() {

  readPcSerial();
  keyaquire();
  joymouse();
  printloop();

}

// ######################## FUNZIONI ##########################
// ######################## FUNZIONI ##########################
// ######################## FUNZIONI ##########################

// ######################## KEYPAD INPUT ##########################

void keyaquire() {
  if (kpad.getKeys())
    {
      for (int i = 0; i < LIST_MAX; i++) // Scan the whole key list.
      {
        if ( kpad.key[i].stateChanged )   // Only find keys that have changed state.
        {
          //###########################ALLOCAZIONE MACRO E TASTI#########################
          char tasto = (char)kpad.key[i].kchar;
                  
        //CONTATORE CLICK: aumento il contatore solo nel caso di case: PRESSED
            switch (kpad.key[i].kstate) {
              case PRESSED:
			        strCountIncr();
              lastActionTimer = millis();
              //DEBUG PER ASSEGNAZIONE TASTI
              Serial.println(tasto);
              break;
              case RELEASED:
              break;
              case IDLE:
              break;
              case HOLD:
              break;		  
        }
        //fine della parentesi contatore

          //inizio con la definizione dei vari pulsanti	
          if (tasto == 'A')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                aHoldConsumed = false;
                if (keyFuncState == 0){Mouse.press(MOUSE_MIDDLE);numpad_print("mid mouse");}
                if (keyFuncState == 1){Keyboard.press(KEY_BACKSPACE);numpad_print("backspc");}
                if (keyFuncState == 2){shiftSwitchCalc();break;}
                if (keyFuncState == 3){shiftSwitchCalc();break;}
                break;
              case RELEASED:
                aHoldConsumed = false;
                if (keyFuncState == 0){Mouse.release(MOUSE_MIDDLE);}
                if (keyFuncState == 1){Keyboard.release(KEY_BACKSPACE);}
                break;
              case IDLE:
                break;
              case HOLD:
                if (!aHoldConsumed) {
                  aHoldConsumed = true;
                  if (keyFuncState == 2 || keyFuncState == 3) {
                    // apri menu funzioni calcolatrice
                    // NON toccare prevKeyFstate: conserva il valore pre-calcolatrice (0 o 1)
                    keyFuncState     = 5;
                    calcFnMenuIndex  = 0;
                    calcFnMenuDirty  = true;
                    lastActionTimer  = millis();
                  } else if (keyFuncState == 5) {
                    // esci dal menu senza eseguire nulla → torna in calc
                    keyFuncState = 2;
                    displayDirty = true;
                  }
                }
                break;
            }
          }

          if (tasto == 'B')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){
                Keyboard.print("line");
                delay(50);
                Keyboard.press(176);  
                delay(50);
                Keyboard.releaseAll();
                numpad_print("line");
                }
                if (keyFuncState == 1){Keyboard.press('^');numpad_print("^");}
                if (keyFuncState == 2){handleKey('^');}
                if (keyFuncState == 3){
                  handleKey('r');
                  shiftSwitchCalc();
                }
                break;
              case RELEASED:
                if (keyFuncState == 0){}
                if (keyFuncState == 1){Keyboard.release('^');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }

          if (tasto == 'C')
          {
            switch (kpad.key[i].kstate) {
              case PRESSED:
                funcSwitchCalc();
                break;
              case RELEASED:
                break;
              case IDLE:
                break;
              case HOLD:
                //Serial.println(keyFuncState);
                if (keyFuncState == 2 || keyFuncState == 3){
                funcSwitchCalc();
                Keyboard.press(KEY_F5);
                Keyboard.release(KEY_F5);
                SscreenUpdate = false;
                screenUpdate = false;
                numpad_print("F5");
                }
                break;
            }
          }
          
          if (tasto == 'D')
          {
            switch (kpad.key[i].kstate) {
              case PRESSED:
                dHoldConsumed = false;
                funcSwitch1();
                break;
              case RELEASED:
                dHoldConsumed = false;
                break;
              case IDLE:
                break;
              case HOLD:
                if (!dHoldConsumed) {
                  dHoldConsumed = true;
                  if (keyFuncState != 4) {
                    prevKeyFstate   = keyFuncState;
                    keyFuncState    = 4;
                    cfgMenuIndex    = 0;
                    cfgMenuDirty    = true;
                    lastActionTimer = millis();
                  } else {
                    cfgSave();
                    keyFuncState  = prevKeyFstate;
                    screenUpdate  = true;
                    displayDirty  = true;
                  }
                }
                break;
            }
          }
        
          if (tasto == 'E')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){handleKey('1');handleKey('+');numpad_print("next sagoma");}
                if (keyFuncState == 1){Keyboard.press('=');numpad_print("=");}
                if (keyFuncState == 2){handleKey('e');}
                if (keyFuncState == 3){
                  rotateStack();
                  displayDirty = true;
                  shiftSwitchCalc();
                  }
                break;
              case RELEASED:
                if (keyFuncState == 0){}
                if (keyFuncState == 1){Keyboard.release('=');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }
          
          if (tasto == 'F')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){
				        Keyboard.print("dimlinear"); //powerquote di AUTOCAD
                delay(50);
                Keyboard.press(176);  
                delay(50);
                Keyboard.releaseAll();  
                numpad_print("quotare");  
                }
                if (keyFuncState == 1){Keyboard.press('(');numpad_print("(");}
                if (keyFuncState == 2){handleKey('C');}
                break;
              case RELEASED:
                if (keyFuncState == 0){}
                if (keyFuncState == 1){Keyboard.release('(');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }

          if (tasto == 'G')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){
 				        Keyboard.print("extend"); //estendi in AUTOCAD
                delay(50);
                Keyboard.press(176);  
                delay(50);
                Keyboard.releaseAll();
                numpad_print("extend");
                }
                if (keyFuncState == 1){Keyboard.press(')');numpad_print(")");}
                if (keyFuncState == 2){handleKey('D');}
                if (keyFuncState == 3){
                  handleKey('x');
                  shiftSwitchCalc();
                  }
                break;
              case RELEASED:
                if (keyFuncState == 0){}
                if (keyFuncState == 1){Keyboard.release(')');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }

          if (tasto == 'H')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){Keyboard.press(KEY_F8);numpad_print("F8");}
                if (keyFuncState == 1){Keyboard.press('*');numpad_print("*");}
                if (keyFuncState == 2){handleKey('*');}
                break;
              case RELEASED:
                if (keyFuncState == 0){Keyboard.release(KEY_F8);}
                if (keyFuncState == 1){Keyboard.release('*');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }

          if (tasto == 'I')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){
                Keyboard.print("circle");
                delay(50);
                Keyboard.press(176);  
                delay(50);
                Keyboard.releaseAll();
                numpad_print("cicle");
                }
                if (keyFuncState == 1){Keyboard.press('7');numpad_print("7");}
                if (keyFuncState == 2){handleKey('7');}
                break;
              case RELEASED:
                if (keyFuncState == 0){}
                if (keyFuncState == 1){Keyboard.release('7');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }
          
          if (tasto == 'J')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){
				        Keyboard.print("trim"); //taglia in AUTOCAD. per accorciare le linee
                delay(50);
                Keyboard.press(176);  
                delay(50);
                Keyboard.releaseAll();
                numpad_print("trim");
                }
                if (keyFuncState == 1){Keyboard.press('8');numpad_print("8");}
                if (keyFuncState == 2){handleKey('8');}
                break;
              case RELEASED:
                if (keyFuncState == 0){}
                if (keyFuncState == 1){Keyboard.release('8');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }

          if (tasto == 'K')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){
				        Keyboard.press(KEY_LEFT_CTRL);
                Keyboard.press('c'); //C COPIA
                Keyboard.releaseAll();
                numpad_print("ctr + C");
                }
                if (keyFuncState == 1){Keyboard.press('9');numpad_print("9");}
                if (keyFuncState == 2){handleKey('9');}
                break;
              case RELEASED:
                if (keyFuncState == 0){}
                if (keyFuncState == 1){Keyboard.release('9');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }

          if (tasto == 'L')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){
                Keyboard.press(KEY_LEFT_CTRL);
                Keyboard.press('v'); //V INCOLLA
                Keyboard.releaseAll();
                numpad_print("ctr + V");
                }
                if (keyFuncState == 1){Keyboard.press('/');numpad_print("/");}
                if (keyFuncState == 2){handleKey('/');}
                if (keyFuncState == 3){
                  handleKey('z');
                  shiftSwitchCalc();
                }
                break;
              case RELEASED:
                if (keyFuncState == 0){}
                if (keyFuncState == 1){Keyboard.release('/');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }

          if (tasto == 'M')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){Keyboard.press(KEY_LEFT_SHIFT);numpad_print("shift");}
                if (keyFuncState == 1){Keyboard.press('4');numpad_print("4");}
                if (keyFuncState == 2){handleKey('4');}
                if (keyFuncState == 3){
                  handleKey('y');
                  shiftSwitchCalc();
                }
                break;
              case RELEASED:
                if (keyFuncState == 0){Keyboard.release(KEY_LEFT_SHIFT);}
                if (keyFuncState == 1){Keyboard.release('4');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }
          
          if (tasto == 'N')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){
                Keyboard.press(KEY_LEFT_CTRL);
                Keyboard.press('z'); //Z AZIONE PRECEDENTE
                Keyboard.releaseAll();
                numpad_print("ctr + Z");
                }
                if (keyFuncState == 1){Keyboard.press('5');numpad_print("5");}
                if (keyFuncState == 2){handleKey('5');}
                if (keyFuncState == 3){
                  handleKey('u');
                  shiftSwitchCalc();
                }
                break;
              case RELEASED:
                if (keyFuncState == 0){}
                if (keyFuncState == 1){Keyboard.release('5');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }

          if (tasto == 'O')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){Keyboard.press(KEY_BACKSPACE);numpad_print("backsp");}
                if (keyFuncState == 1){Keyboard.press('6');numpad_print("6");}
                if (keyFuncState == 2){handleKey('6');}
                if (keyFuncState == 3){
                  handleKey('i');
                  shiftSwitchCalc();
                }
                break;
              case RELEASED:
                if (keyFuncState == 0){Keyboard.release(KEY_BACKSPACE);}
                if (keyFuncState == 1){Keyboard.release('6');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }

          if (tasto == 'P')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){Keyboard.print("sagoma");numpad_print("sagoma");}
                if (keyFuncState == 1){Keyboard.press('-');numpad_print("-");}
                if (keyFuncState == 2){handleKey('-');}
                break;
              case RELEASED:
                if (keyFuncState == 0){}
                if (keyFuncState == 1){Keyboard.release('-');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }

          if (tasto == 'Q')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){
                Keyboard.print("rotate");
                delay(50);
                Keyboard.press(176);  
                delay(50);
                Keyboard.releaseAll();
                numpad_print("rotate");
                }
                if (keyFuncState == 1){Keyboard.press('1');numpad_print("1");}
                if (keyFuncState == 2){handleKey('1');}
                if (keyFuncState == 3){
                  handleKey('s');
                  shiftSwitchCalc();
                }
                break;
              case RELEASED:
                if (keyFuncState == 0){}
                if (keyFuncState == 1){Keyboard.release('1');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }
          
          if (tasto == 'R')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){
                Keyboard.print("mirror"); //comando specchio autocad
                delay(50);
                Keyboard.press(176);  
                delay(50);
                Keyboard.releaseAll();
                numpad_print("mirror");
                }
                if (keyFuncState == 1){Keyboard.press('2');numpad_print("2");}
                if (keyFuncState == 2){handleKey('2');}
                if (keyFuncState == 3){
                  handleKey('c');
                  shiftSwitchCalc();
                }
                break;
              case RELEASED:
                if (keyFuncState == 0){}
                if (keyFuncState == 1){Keyboard.release('2');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }

          if (tasto == 'S')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){
                Keyboard.print("scale");
                delay(50);
                Keyboard.press(176);  
                delay(50);
                Keyboard.releaseAll();
                numpad_print("scale");
                }
                if (keyFuncState == 1){Keyboard.press('3');numpad_print("3");}
                if (keyFuncState == 2){handleKey('3');}
                if (keyFuncState == 3){
                  handleKey('t');
                  shiftSwitchCalc();
                }
                break;
              case RELEASED:
                if (keyFuncState == 0){}
                if (keyFuncState == 1){Keyboard.release('3');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }

          if (tasto == 'T')
          {
            switch (kpad.key[i].kstate) {
              case PRESSED:
                if (keyFuncState == 0){
                  if (stack.size() >= 4) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%d_a%d%s%03d_sa_%s",
                      (int)stack[0],
                      (int)stack[1],
                      "a_gn",
                      (int)stack[2],
                      numberToLetters((int)stack[3]).c_str());
                    Keyboard.print(buf);
                    for (int j = 0; buf[j]; j++) {
                      buf[j] = toupper(buf[j]);
                    }
                    numpad_print(buf);
                  } else {
                    numpad_print("err: <4 el");
                  }
                }
                if (keyFuncState == 1){Keyboard.press('+');numpad_print("+");}
                if (keyFuncState == 2){handleKey('+');}
                if (keyFuncState == 3){
                  replaceCharInArray(lastCalcNum, ',', '.');
                  Keyboard.print(lastCalcNum);
                  shiftSwitchCalc();
                }
                break;
              case RELEASED:
                if (keyFuncState == 0){}
                if (keyFuncState == 1){Keyboard.release('+');}
                break;
              case IDLE:
                break;
              case HOLD:
                if (keyFuncState == 0 || keyFuncState == 1)
                {
                  Keyboard.press(KEY_LEFT_CTRL);
                  Keyboard.press('q');
                  Keyboard.releaseAll();
                  numpad_print("ctr + Q");
                }
                break;
            }
          }

          if (tasto == 'U')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){Keyboard.press(KEY_LEFT_CTRL);numpad_print("ctrl");}
                if (keyFuncState == 1){Keyboard.press('0');numpad_print("0");}
                if (keyFuncState == 2){handleKey('0');}
                if (keyFuncState == 3){
                  handleKey('P');
                  shiftSwitchCalc();
                }
                break;
              case RELEASED:
                if (keyFuncState == 0){Keyboard.release(KEY_LEFT_CTRL);}
                if (keyFuncState == 1){Keyboard.release('0');}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }
          
          if (tasto == 'V')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0)
                {
                  Keyboard.print("movecopy");
                  delay(50);
                  Keyboard.press(176);  //CAMBIA TAB
                  delay(50);
                  Keyboard.releaseAll();
                  numpad_print("movecopy");
                }
                if (keyFuncState == 1){
                  Keyboard.press(mapStateToChar(joyConf));

                  char c = mapStateToChar(joyConf);
                  char buf[2] = { c, '\0' };
                  numpad_print(buf);
                  }
                if (keyFuncState == 2){handleKey('.');}
                if (keyFuncState == 3){
                  handleKey('G');
                  shiftSwitchCalc();
                }
                break;
              case RELEASED:
                if (keyFuncState == 0){}
                if (keyFuncState == 1){Keyboard.release(mapStateToChar(joyConf));}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }

          if (tasto == 'W')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){
                  Keyboard.print("move");
                  delay(50);
                  Keyboard.press(176);  
                  delay(50);
                  Keyboard.releaseAll();
                  numpad_print("move");
                }
                if (keyFuncState == 1){Keyboard.press(KEY_LEFT_CTRL);
                numpad_print("ctrl");}
                if (keyFuncState == 2){handleKey('n');}
                if (keyFuncState == 3){
                  handleKey('N');
                  shiftSwitchCalc();
                }
              break;
              case RELEASED:
                if (keyFuncState == 0){}
                if (keyFuncState == 1){Keyboard.release(KEY_LEFT_CTRL);}
              break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }

          if (tasto == 'X')
          {
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 4){
                  cfgSave();
                  keyFuncState = prevKeyFstate;
                  screenUpdate = true;
                  displayDirty = true;
                  break;
                }
                if (keyFuncState == 5){
                  // esegui funzione selezionata e torna in calc
                  handleKey(CALC_FN_MENU[calcFnMenuIndex].opChar);
                  keyFuncState = 2;
                  displayDirty = true;
                  break;
                }
                if (keyFuncState == 0){Keyboard.press(KEY_RETURN);numpad_print("enter");}
                if (keyFuncState == 1){Keyboard.press(KEY_RETURN);numpad_print("enter");}
                if (keyFuncState == 2){handleKey('=');}
                if (keyFuncState == 3){
                  replaceCharInArray(lastCalcNum, '.', ',');
                  Keyboard.print(lastCalcNum);
                  shiftSwitchCalc();
                  }
                break;
              case RELEASED:
                if (keyFuncState == 0){Keyboard.release(KEY_RETURN);}
                if (keyFuncState == 1){Keyboard.release(KEY_RETURN);}
                break;
              case IDLE:
                break;
              case HOLD:
                break;
            }
          }

          
        // restituisco tasto come output DEBUG
        }
      }
    }
}

// ######################## MOUSE JOYSTICK ##########################

void joymouse() {
  //Update the Bounce instance :
  debouncer.update();
  //read raw values
  for (int i = 0; i < WINDOW_SIZE; i++) readingsVert[i] = (int)analogRead(vertJoyPIN);

  // Ordina i valori per trovare il mediano
  int sorted[WINDOW_SIZE];
  memcpy(sorted, readingsVert, sizeof(readingsVert));
  for (int i = 0; i < WINDOW_SIZE - 1; i++) {
    for (int j = i + 1; j < WINDOW_SIZE; j++) {
      if (sorted[i] > sorted[j]) {
        int temp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = temp;
      }
    }
  }

  vertRawValue = sorted[WINDOW_SIZE / 2]; // Ottieni il mediano

  for (int i = 0; i < WINDOW_SIZE; i++) readingsHor[i] = (int)analogRead(horJoyPIN);
  
  // Ordina i valori per trovare il mediano
  memcpy(sorted, readingsHor, sizeof(readingsHor));
  for (int i = 0; i < WINDOW_SIZE - 1; i++) {
    for (int j = i + 1; j < WINDOW_SIZE; j++) {
      if (sorted[i] > sorted[j]) {
        int temp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = temp;
      }
    }
  }
  
  horRawValue = sorted[WINDOW_SIZE / 2];

  //Serial.print(vertRawValue);
  //Serial.println(horRawValue);
  //rilevo se è stato cliccato il pulsante centrale del joystick.
  if (debouncer.rose())
  {
    joyConf = !joyConf;
    lastActionTimer = millis();
    //Serial.println("funz");
    if (joyConf == 0)
    {
      jState = "COMMA mode";
      screenUpdate = true;
    }
    if (joyConf == 1)
    {
      jState = "dot mode";
      screenUpdate = true;
    }
    cfgSave();  // salva dot/comma in NVS
    //Serial.println(joyConf);
  }

  // ── CONFIG MENU: navigazione con joystick ────────────────────────────────────
  if (keyFuncState == 4) {
    static unsigned long lastJoyNav = 0;
    if (millis() - lastJoyNav >= 300) {
      if (abs(vertRawValue - vertZero) > cfg_deadBand) {
        lastJoyNav = millis();
        if ((vertRawValue - vertZero) < 0) {
          cfgMenuIndex = (cfgMenuIndex + 1) % CFG_ENTRIES_COUNT;
        } else {
          cfgMenuIndex = (cfgMenuIndex - 1 + CFG_ENTRIES_COUNT) % CFG_ENTRIES_COUNT;
        }
        cfgMenuDirty = true;
        lastActionTimer = millis();
      }
      if (abs(horRawValue - horZero) > cfg_deadBand) {
        lastJoyNav = millis();
        long val = cfgGetLong(cfgMenuIndex);
        if ((horRawValue - horZero) > 0) val += cfgEntries[cfgMenuIndex].step;
        else                              val -= cfgEntries[cfgMenuIndex].step;
        cfgSetLong(cfgMenuIndex, val);
        cfgMenuDirty = true;
        lastActionTimer = millis();
      }
    }
    // click joystick -> conferma + salva
    if (debouncer.rose()) {
      cfgSave();
      cfgMenuDirty = true;
      lastActionTimer = millis();
    }
    return;  // blocca scroll mouse mentre nel menu
  }
  // ─────────────────────────────────────────────────────────────────────────────

  // ── CALC FUNCTION MENU: navigazione con joystick ─────────────────────────────
  if (keyFuncState == 5) {
    static unsigned long lastJoyNavFn = 0;
    if (millis() - lastJoyNavFn >= 300) {
      if (abs(vertRawValue - vertZero) > cfg_deadBand) {
        lastJoyNavFn = millis();
        if ((vertRawValue - vertZero) < 0) {
          calcFnMenuIndex = (calcFnMenuIndex + 1) % CALC_FN_MENU_COUNT;
        } else {
          calcFnMenuIndex = (calcFnMenuIndex - 1 + CALC_FN_MENU_COUNT) % CALC_FN_MENU_COUNT;
        }
        calcFnMenuDirty = true;
        lastActionTimer = millis();
      }
      if (abs(horRawValue - horZero) > cfg_deadBand) {
        lastJoyNavFn = millis();
        int step = 4;  // salta di una pagina (4 voci visibili)
        if ((horRawValue - horZero) > 0) {
          calcFnMenuIndex = (calcFnMenuIndex + step) % CALC_FN_MENU_COUNT;
        } else {
          calcFnMenuIndex = (calcFnMenuIndex - step + CALC_FN_MENU_COUNT) % CALC_FN_MENU_COUNT;
        }
        calcFnMenuDirty = true;
        lastActionTimer = millis();
      }
    }
    // click joystick -> esegui funzione selezionata e torna in calc
    if (debouncer.rose()) {
      handleKey(CALC_FN_MENU[calcFnMenuIndex].opChar);
      keyFuncState = 2;
      displayDirty = true;
      lastActionTimer = millis();
    }
    return;  // blocca scroll mouse mentre nel menu
  }
  // ─────────────────────────────────────────────────────────────────────────────

  //############################GESTIONE ASSE VERTICALE############################
  if (abs(vertRawValue - vertZero) > cfg_deadBand)
  {
    //joystick horizz mosso in modo ponderato
    scrollScrExit = true;
    //if (!joyConf)
    //{
      scrollPeriod = cfg_fixSensScrol + abs(cfg_sensitivityScrl / (vertRawValue - vertZero));
      if ((millis() - lastVertScroll) >= scrollPeriod)                            //#
      {
        if ((vertRawValue - vertZero) > 0)                                        //#
        {
          Mouse.move(0, 0, +singleScroll * cfg_scrollMult);
          ScrollCount = ScrollCount + singleScroll;
        }
        else
        {
          Mouse.move(0, 0, -singleScroll * cfg_scrollMult);
          ScrollCount = ScrollCount + singleScroll;
        }
        lastVertScroll = millis();
        //lastActionTimer = millis();
        //Serial.print("vert");
        //Serial.println(vertRawValue);
      }
    //}
    //else
    // {
    //   //frecce
    //   scrollPeriod = fixSensArrow + abs(sensitivityArrow / (vertRawValue - vertZero));
    //   if ((millis() - lastVertScroll) >= scrollPeriod)                            //#
    //   {
    //     if ((vertRawValue - vertZero) > 0)                                        //#
    //     {
    //       Keyboard.press(KEY_UP_ARROW);
    //       Keyboard.release(KEY_UP_ARROW);
    //     }
    //     else
    //     {
    //       Keyboard.press(KEY_DOWN_ARROW);
    //       Keyboard.release(KEY_DOWN_ARROW);
    //     }
    //     lastVertScroll = millis();
    //     //lastActionTimer = millis();
    //     Serial.print("vert");
    //     Serial.println(vertRawValue);

    //   }
    // }

  }

  //##############################################################################

  //############################GESTIONE ASSE ORIZZONTALE#########################
  if (abs(horRawValue - horZero) > cfg_deadBand)
  {
    scrollScrExit = true;
    //joystick horizz mosso in modo ponderato
    // if (!joyConf)
    // {
      scrollPeriod = cfg_fixSensScrol + abs(cfg_sensitivityScrl / (horRawValue - horZero));
      if ((millis() - lastHorScroll) >= scrollPeriod)                             //#
      {
        if ((horRawValue - horZero) > 0)                                        //#
        {
          Mouse.move(0, 0, -singleScroll*3 * cfg_scrollMult);
          ScrollCount = ScrollCount + singleScroll*3;
        }
        else
        {
          Mouse.move(0, 0, +singleScroll*3 * cfg_scrollMult);
          ScrollCount = ScrollCount + singleScroll*3;
        }
        lastHorScroll = millis();
        //lastActionTimer = millis();
        //Serial.print("hor");
        //Serial.println(horRawValue);
      }
    // }
    // else
    // {
    //   scrollPeriod = fixSensArrow + abs(sensitivityArrow / (horRawValue - horZero));
    //   if ((millis() - lastHorScroll) >= scrollPeriod)                            //#
    //   {
    //     if ((horRawValue - horZero) > 0)                                        //#
    //     {
    //       Keyboard.press(KEY_RIGHT_ARROW);
    //       Keyboard.release(KEY_RIGHT_ARROW);
    //     }
    //     else
    //     {
    //       Keyboard.press(KEY_LEFT_ARROW);
    //       Keyboard.release(KEY_LEFT_ARROW);
    //     }
    //     lastHorScroll = millis();
    //     Serial.print("hor");
    //     Serial.println(horRawValue);
    //     //lastActionTimer = millis();
    //   }

    // }
  }
}


// ######################## FUNZIONI DI MENÙ ##########################

// funzione cambio mode tra numpad e software.. per ora
void funcSwitch1()
{
  if (keyFuncState>=1)
  {
    keyFuncState = 0;
    screenUpdate = true;
  }
  else 
  {
    ++keyFuncState;
    screenUpdate = true;
  }
}

// funzione attivazione calcolatrice
void funcSwitchCalc()
{
  if (keyFuncState == 2 || keyFuncState == 3 || keyFuncState == 5)
  {
    // uscita dalla calcolatrice (anche da shift attivo o menu funzioni)
    keyFuncState = prevKeyFstate;
    screenUpdate = true;
  }
  else
  {
    prevKeyFstate = keyFuncState;
    keyFuncState = 2;
    screenUpdate = false;
    displayDirty = true;
  }
}

void shiftSwitchCalc()
{
  if (keyFuncState == 2)
  {
    keyFuncState = 3;
    display.setCursor(20,50);
    display.setTextSize(3);
    display.println("shift");
    display.display();
  }
  else if (keyFuncState == 3)
  {
    keyFuncState = 2;
    displayDirty = true;
  }
}

// funzione incremento variabile conta click
void strCountIncr()
{
  ++keyStrCount;
}

// funzione inizializzazione timer stampa contatore click
void printTimer()
{
  screenUpdate = true;
  clickDisplay = true;
  printTimerEnd = millis() + 3000;
}

void printTimer2()
{
  printTimerEnd = millis() + 3000;
  SscreenUpdate = true;
} 

// ######################## STAMPA A SCHERMO ##########################

void printloop() {
  // timer per tornare alla schermata principale dopo numpad_print
  if (millis() > printTimerEnd && SscreenUpdate) {
    clickDisplay  = false;
    screenUpdate  = true;
    SscreenUpdate = false;
  }

  // refresh automatico 1 secondo in mode 0/1, solo se lo screensaver non è attivo
  static unsigned long lastAutoRefresh = 0;
  bool screensaverActive = (millis() - lastActionTimer > (unsigned long)cfg_screensaverMs);
  if ((keyFuncState == 0 || keyFuncState == 1) && !SscreenUpdate && !screensaverActive) {
    if (millis() - lastAutoRefresh >= 1000) {
      lastAutoRefresh = millis();
      screenUpdate = true;
    }
  }

  // refresh schermata principale (mode 0/1)
  if (screenUpdate && (keyFuncState == 0 || keyFuncState == 1)) {
    joystatPrt();
    screenUpdate = false;
  }

  // ← NUOVO: refresh calcolatrice, solo se qualcosa è cambiato
  if (displayDirty && (keyFuncState == 2 || keyFuncState == 3)) {
    printStack();
    displayDirty = false;
  }

  // menu configurazione — ridisegna sempre per animare il testo scorrevole
  if (keyFuncState == 4) {
    printConfigMenu();
    cfgMenuDirty = false;
  }

  // menu funzioni calcolatrice — ridisegna sempre per animare il testo scorrevole
  if (keyFuncState == 5) {
    printCalcFnMenu();
    calcFnMenuDirty = false;
  }

  screensaver();
}

// stampa a schermo nelle modalità base
void joystatPrt()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0); 
  display.println("DOT/COM mode");
  display.setCursor(15,10);
  display.println(jState);
  display.setCursor(0,35);
  display.println("KEYpad mode"); 
  
  switch (keyFuncState)
  {
    case 0:
      nState = "mAcr";
      break;
    case 1:
      nState = "Nump";
      break;
  }
  display.setCursor(0,45);
  display.setTextSize(3);
  display.println(nState);
  
  // inserire stampa del contatore di click:
  // voglio che si attivi una volta schiacciato il tasto F8 in HOLD.
  // deve stampare il numero con un titoletto e poi sparire dopo 5 secondi.
 
  display.setTextSize(1);
  display.setCursor(0,96);
  display.print("But: ");  display.println(keyStrCount);
  display.setCursor(0,104);
  display.print("Scr: ");  display.println(ScrollCount);
  // riga CPU / GPU
  display.setCursor(0,112);
  display.print("C:"); printPcVal(pcCpuPct);
  if (pcCpuPct >= 0) display.print("%");
  display.print(" ");
  printPcVal(pcCpuTemp);
  if (pcCpuTemp >= 0) display.print("C");
  display.setCursor(64,112);
  display.print("G:"); printPcVal(pcGpuPct);
  if (pcGpuPct >= 0) display.print("%");
  display.print(" ");
  printPcVal(pcGpuTemp);
  if (pcGpuTemp >= 0) display.print("C");
  // riga RAM / NET
  display.setCursor(0,120);
  display.print("R:"); printPcVal(pcRamPct);
  if (pcRamPct >= 0) display.print("%");
  display.setCursor(64,120);
  display.print("N:"); printPcVal(pcNetMbps);
  if (pcNetMbps >= 0) display.print("M");
  display.display();
  // effettivo comando di output a schermo
}

// funzione stampa numero digitato
void numpad_print(const char* numb)
{
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.print(numb);
  //Serial.println(numb);
  display.setCursor(0,35);
  display.setTextSize(1);
  display.println("old");  
  display.setCursor(0,46);
  display.setTextSize(1);
  display.println(numBak1);
  display.setCursor(0,62);
  display.setTextSize(1);
  display.println(numBak2);
  display.setCursor(0,78);
  display.setTextSize(1);
  display.println(numBak3);
  display.setTextSize(1);
  display.setCursor(0,96);
  display.print("But: ");  display.println(keyStrCount);
  display.setCursor(0,104);
  display.print("Scr: ");  display.println(ScrollCount);
  // riga CPU / GPU
  display.setCursor(0,112);
  display.print("C:"); printPcVal(pcCpuPct);
  if (pcCpuPct >= 0) display.print("%");
  display.print(" ");
  printPcVal(pcCpuTemp);
  if (pcCpuTemp >= 0) display.print("C");
  display.setCursor(64,112);
  display.print("G:"); printPcVal(pcGpuPct);
  if (pcGpuPct >= 0) display.print("%");
  display.print(" ");
  printPcVal(pcGpuTemp);
  if (pcGpuTemp >= 0) display.print("C");
  // riga RAM / NET
  display.setCursor(0,120);
  display.print("R:"); printPcVal(pcRamPct);
  if (pcRamPct >= 0) display.print("%");
  display.setCursor(64,120);
  display.print("N:"); printPcVal(pcNetMbps);
  if (pcNetMbps >= 0) display.print("M");
  display.display();
  strncpy(numBak3, numBak2, sizeof(numBak3));
  strncpy(numBak2, numBak1, sizeof(numBak2));
  strncpy(numBak1, numb,    sizeof(numBak1));
  printTimer2();
}

// ############ SCREENSAVER MATRIX ###########

void screensaverMatrix() {
  static int  columnY[21];
  static int  speed[21];
  static bool initialized = false;
  static unsigned long lastFrameTime = 0;

  static int  infoY    = 0;
  static int  infoVDir = 1;
  static unsigned long lastInfoMove = 0;
  #define INFO_MOVE_INTERVAL 120
  #define INFO_BLOCK_H 32

  if (!initialized) {
    for (int i = 0; i < 21; i++) {
      columnY[i] = random(-40, 0);
      speed[i]   = random(1, 4);
    }
    initialized = true;
  }

  if (millis() - lastFrameTime < (unsigned long)cfg_screensaverFps) return;
  lastFrameTime = millis();

  unsigned long now = millis();
  if (now - lastInfoMove >= INFO_MOVE_INTERVAL) {
    lastInfoMove = now;
    infoY += infoVDir;
    if (infoY >= SCREEN_HEIGHT - INFO_BLOCK_H) { infoY = SCREEN_HEIGHT - INFO_BLOCK_H; infoVDir = -1; }
    if (infoY <= 0)                             { infoY = 0;                            infoVDir =  1; }
  }

  display.clearDisplay();

  for (int col = 0; col < 21; col++) {
    int x = col * 6;
    int y = columnY[col];

    char ch;
    if      (keyFuncState == 0) ch = 'm';
    else if (keyFuncState == 1) ch = '0' + random(0, 10);
    else                        ch = "calc"[random(0, 4)];

    display.setTextSize(1);

    if (y >= 0 && (y < infoY || y >= infoY + INFO_BLOCK_H)) {
      display.setCursor(x, y);
      display.write(ch);
    }

    for (int trail = 1; trail <= 3; trail++) {
      int ty = y - trail * 8;
      if (ty >= 0 && (ty < infoY || ty >= infoY + INFO_BLOCK_H)) {
        display.setCursor(x, ty);
        if      (trail == 1) display.write(ch);
        else if (trail == 2) display.write('.');
        else                 display.write(' ');
      }
    }

    columnY[col] += speed[col] * 2;
    if (columnY[col] > SCREEN_HEIGHT) {
      columnY[col] = random(-20, 0);
      speed[col]   = random(1, 4);
    }
  }

  display.setTextSize(1);
  display.setCursor(0, infoY);
  display.print("But: ");  display.println(keyStrCount);
  display.setCursor(0, infoY + 8);
  display.print("Scr: ");  display.println(ScrollCount);
  display.setCursor(0, infoY + 16);
  display.print("C:"); printPcVal(pcCpuPct);
  if (pcCpuPct >= 0) display.print("%");
  display.print(" ");
  printPcVal(pcCpuTemp);
  if (pcCpuTemp >= 0) display.print("C");
  display.setCursor(64, infoY + 16);
  display.print("G:"); printPcVal(pcGpuPct);
  if (pcGpuPct >= 0) display.print("%");
  display.print(" ");
  printPcVal(pcGpuTemp);
  if (pcGpuTemp >= 0) display.print("C");
  display.setCursor(0, infoY + 24);
  display.print("R:"); printPcVal(pcRamPct);
  if (pcRamPct >= 0) display.print("%");
  display.setCursor(64, infoY + 24);
  display.print("N:"); printPcVal(pcNetMbps);
  if (pcNetMbps >= 0) display.print("M");

  display.display();
}

// ############ SCREENSAVER GRAFICI CPU/GPU ###########

#define TEMP_HISTORY 128
static int  tempCpuHistory[TEMP_HISTORY];
static int  tempGpuHistory[TEMP_HISTORY];
static int  tempHistIdx  = 0;
static bool tempHistInit = false;
static unsigned long lastTempSample = 0;
#define TEMP_SAMPLE_INTERVAL 2000  // campiona ogni 2 s

void screensaverGraph() {
  static unsigned long lastFrameTime = 0;

  // inizializzazione buffer al primo accesso
  if (!tempHistInit) {
    for (int i = 0; i < TEMP_HISTORY; i++) { tempCpuHistory[i] = 0; tempGpuHistory[i] = 0; }
    tempHistInit = true;
  }

  // campionamento temperatura indipendente dal frame rate
  if (millis() - lastTempSample >= TEMP_SAMPLE_INTERVAL) {
    lastTempSample = millis();
    tempCpuHistory[tempHistIdx] = (pcCpuTemp >= 0) ? pcCpuTemp : 0;
    tempGpuHistory[tempHistIdx] = (pcGpuTemp >= 0) ? pcGpuTemp : 0;
    tempHistIdx = (tempHistIdx + 1) % TEMP_HISTORY;
  }

  // frame rate
  if (millis() - lastFrameTime < (unsigned long)cfg_screensaverFps) return;
  lastFrameTime = millis();

  display.clearDisplay();
  display.setTextSize(1);

  // ── layout display 128×128 ──────────────────────────────────────────────────
  // y=0..7    header modalità pad
  // y=8       linea separatrice
  // y=9..54   grafico CPU  (plotH=45, base=54)
  // y=55      linea separatrice centrale
  // y=56..101 grafico GPU  (plotH=45, base=101)
  // y=102     linea separatrice footer
  // y=110     riga unica "C:72C 45%  G:65C 30%"  (8px, finisce a y=117)

  const int plotH   = 45;
  const int maxT    = 110;
  const int cpuBase = 54;
  const int gpuBase = 101;

  // ── header modalità ─────────────────────────────────────────────────────────
  const char* modeName;
  switch (keyFuncState) {
    case 0:  modeName = "MACRO";  break;
    case 1:  modeName = "NUMPAD"; break;
    case 2:
    case 3:  modeName = "CALC";   break;
    default: modeName = "---";    break;
  }
  display.setCursor(0, 0);
  display.print("[ ");
  display.print(modeName);
  display.print(" ]");
  display.drawFastHLine(0, 8, 128, SH110X_WHITE);

  // ── etichette laterali CPU / GPU ─────────────────────────────────────────────
  display.setCursor(0, 10);  display.print("CPU");
  display.setCursor(0, 57);  display.print("GPU");

  // ── linee di riferimento tratteggiate 50°C e 80°C ───────────────────────────
  int ref50cpu = cpuBase - (int)((long)50 * plotH / maxT);
  int ref80cpu = cpuBase - (int)((long)80 * plotH / maxT);
  int ref50gpu = gpuBase - (int)((long)50 * plotH / maxT);
  int ref80gpu = gpuBase - (int)((long)80 * plotH / maxT);
  for (int x = 20; x < 128; x += 4) {
    display.drawPixel(x, ref50cpu, SH110X_WHITE);
    display.drawPixel(x, ref80cpu, SH110X_WHITE);
    display.drawPixel(x, ref50gpu, SH110X_WHITE);
    display.drawPixel(x, ref80gpu, SH110X_WHITE);
  }
  display.setCursor(0, ref80cpu - 1);  display.print("80");
  display.setCursor(0, ref50cpu - 1);  display.print("50");
  display.setCursor(0, ref80gpu - 1);  display.print("80");
  display.setCursor(0, ref50gpu - 1);  display.print("50");

  // ── grafici (x 20..127 = 108px) ─────────────────────────────────────────────
  const int graphX0 = 20;
  const int graphW  = 108;
  int prevCpuY = -1, prevGpuY = -1;

  for (int xi = 0; xi < graphW; xi++) {
    int sampleIdx = (tempHistIdx + (int)((long)xi * TEMP_HISTORY / graphW)) % TEMP_HISTORY;
    int cpuT = constrain(tempCpuHistory[sampleIdx], 0, maxT);
    int gpuT = constrain(tempGpuHistory[sampleIdx], 0, maxT);
    int cpuY = cpuBase - (int)((long)cpuT * plotH / maxT);
    int gpuY = gpuBase - (int)((long)gpuT * plotH / maxT);
    int x    = graphX0 + xi;

    if (prevCpuY >= 0) display.drawLine(x - 1, prevCpuY, x, cpuY, SH110X_WHITE);
    else               display.drawPixel(x, cpuY, SH110X_WHITE);
    if (prevGpuY >= 0) display.drawLine(x - 1, prevGpuY, x, gpuY, SH110X_WHITE);
    else               display.drawPixel(x, gpuY, SH110X_WHITE);

    prevCpuY = cpuY;
    prevGpuY = gpuY;
  }

  // ── separatori ───────────────────────────────────────────────────────────────
  display.drawFastHLine(0,  55, 128, SH110X_WHITE);   // tra CPU e GPU
  display.drawFastHLine(0, 102, 128, SH110X_WHITE);   // sopra footer

  // ── footer: riga unica "C:72C 45%  G:65C 30%" ───────────────────────────────
  display.setCursor(0, 110);
  display.print("C:");
  if (pcCpuTemp >= 0) { display.print(pcCpuTemp); display.print("C "); }
  else display.print("-- ");
  if (pcCpuPct >= 0) { display.print(pcCpuPct); display.print("%"); }
  else display.print("--%");
  display.print("  G:");
  if (pcGpuTemp >= 0) { display.print(pcGpuTemp); display.print("C "); }
  else display.print("-- ");
  if (pcGpuPct >= 0) { display.print(pcGpuPct); display.print("%"); }
  else display.print("--%");

  display.display();
}

// ############ SCREENSAVER DISPATCHER ###########

void screensaver() {
  // check timeout inattività — esco subito se ancora attivi
  if (millis() - lastActionTimer <= (unsigned long)cfg_screensaverMs) return;

  // non attivare screensaver mentre si è in un menu
  if (keyFuncState == 4 || keyFuncState == 5) return;

  // uscita dallo screensaver via joystick
  if (scrollScrExit) {
    scrollScrExit   = false;
    lastActionTimer = millis();
    if (keyFuncState == 2 || keyFuncState == 3) { displayDirty = true; return; }
    screenUpdate = true;
    return;
  }

  // dispatch sul tipo selezionato
  if (cfg_screensaverType == 0) {
    screensaverMatrix();
  } else {
    screensaverGraph();
  }
}


// ######################## FUNZIONI CALCOLATRICE ####################################
// gestione dei tasti di input da keypad
// e traduzione in relative funzioni
void handleKey(char key) {
  if (key >= '0' && key <= '9') {
    // Aggiungi la cifra al buffer del numero corrente
    currentInput += key;
    Serial.print("Numero corrente: ");
    Serial.println(currentInput.c_str());
    displayDirty = true;
  }
  else if (key == 'n') {
    // Gestisci il simbolo meno per i numeri negativi o per l'esponente
    if (currentInput.empty() || currentInput[currentInput.size() - 1] == 'e') {
      // Aggiungi il meno all'inizio di un numero o dopo 'e'
      currentInput += '-';
      Serial.print("Numero corrente (negativo): ");
      Serial.println(currentInput.c_str());
      displayDirty = true;
    }
  }
  else if (key == '.') {
    // Gestisci il punto decimale
    if (currentInput.find('.') == std::string::npos) {  // Verifica se c'è già un punto
      currentInput += '.';
      Serial.print("Numero corrente: ");
      Serial.println(currentInput.c_str());
      displayDirty = true;
    }
  }
  else if (key == 'e') {
    // Gestisci la notazione scientifica (esponente)
    if (!isScientific && !currentInput.empty()) {
      currentInput += 'e';  // Aggiungi il carattere 'e'
      isScientific = true;   // Attiva la modalità scientifica
      Serial.print("Notazione scientifica attivata: ");
      Serial.println(currentInput.c_str());
      displayDirty = true;
    }
  }
  else if (key == '=') {
    // Quando premi "=", aggiungi il numero allo stack
    if (!currentInput.empty()) {
      if (isScientific) {
        // Gestione della notazione scientifica: calcola la base * 10^esponente
        size_t ePos = currentInput.find('e');
        if (ePos != std::string::npos) {
          std::string baseStr = currentInput.substr(0, ePos);
          std::string expStr = currentInput.substr(ePos + 1);
          double baseValue = atof(baseStr.c_str());
          int expValue = atoi(expStr.c_str());
          double result = baseValue * pow(10, expValue);  // Calcola la notazione scientifica
          pushToStack(result);
          lastPush = result;
        }
        } else {
        double result = atof(currentInput.c_str());
        pushToStack(result);  // Converte la stringa in double e la aggiunge allo stack
        lastPush = result;
        }
        //currentInput = "";  // Reset del buffer EDIT INCLUSO NEL PUSH TO STACK
        isScientific = false;  // Disattiva la modalità scientifica
    }
    else {
        pushToStack(lastPush);
    }
  } 
  else if (key == 'C') {
    // Pulisce lo stack
    clearStack();
    displayDirty = true;
  }
  else if (key == 'D') {
    if(currentInput.length() > 0) {
      int lastIndex = currentInput.length() - 1;
      currentInput.erase(lastIndex);
    }
    displayDirty = true;
  }
  else 
  {
    // FACCIO ENTER
    if (!currentInput.empty()) {
      if (isScientific) {
        // Gestione della notazione scientifica: calcola la base * 10^esponente
        size_t ePos = currentInput.find('e');
        if (ePos != std::string::npos) {
          std::string baseStr = currentInput.substr(0, ePos);
          std::string expStr = currentInput.substr(ePos + 1);
          double baseValue = atof(baseStr.c_str());
          int expValue = atoi(expStr.c_str());
          double result = baseValue * pow(10, expValue);  // Calcola la notazione scientifica
          pushToStack(result);
        }
      } else {
        pushToStack(atof(currentInput.c_str()));  // Converte la stringa in double e la aggiunge allo stack
      }
      //currentInput = "";  // Reset del buffer EDIT, INCLUSO NEL PUSH TO STACK
      isScientific = false;  // Disattiva la modalità scientifica
    }
    // Gestisci operazioni (+, -, *, /)
    performOperationFromKey(key);
    Serial.println(key);
  }
  //controllo finale per baco cancellazione esponenziale
  if(currentInput.find('e') == std::string::npos) {
    isScientific = false;
  } 
}

// manda i dati nello stack
void pushToStack(double value) {
  if (stack.size() >= MAX_STACK_SIZE) {
    stack.pop_front();  // Rimuove il valore più vecchio se lo stack è pieno
  }
  stack.push_back(value);
  // Serial.print("Stack aggiornato: ");
  // TEST, CANCELLO IL CURRENT INPUT PER EVITARE CONFUSIONE
  currentInput = "" ;
  displayDirty = true;
}

// stampa a schermo dello stack
void printStack() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println(currentInput.c_str());
  display.setTextSize(1);
  display.print(cfg_calcDegrees ? "deg" : "rad");
  display.print(" - St.p.");
  display.print(stack.size());
  display.print(" of ");
  display.print(stackSizeVar);
  display.println(":");

  for (int i = stack.size() - 1; i >= 0; i--) {
    
    int nInteri = 0;
    int nDecimali = 0;
    int nCifre = 8;
    countDigits(stack[i], nInteri,nDecimali);
    int nDecPrt = nCifre - nInteri;
    if (fabs(stack[i]) >= 10000000) {
      snprintf(buffer, sizeof(buffer), "%.*e", 6, stack[i]);
    }
    else if (fabs(stack[i]) < 0.000001) {
      snprintf(buffer, sizeof(buffer), "%.*e", 4, stack[i]);
    }
    else {
      snprintf(buffer, sizeof(buffer), "%.*f", nDecPrt, stack[i]);
    }

    if (i == stack.size() - 1) {
      display.setTextSize(2);
      //qua di seguito carico il buffer per l'output a keypad con molti decimali
      if (fabs(stack[i]) >= 10000000) {
          snprintf(lastCalcNum, sizeof(lastCalcNum), "%.*e", 10, stack[i]);
        }
        else if (fabs(stack[i]) < 0.000001) {
          snprintf(lastCalcNum, sizeof(lastCalcNum), "%.*e", 10, stack[i]);
        }
        else {
        snprintf(lastCalcNum, sizeof(lastCalcNum), "%.*f", 10, stack[i]);
      }
    }
    display.setTextSize(2);
    display.println(buffer);    

  }
  Serial.println();
  display.display();
}

// Funzione per calcolare le cifre della parte intera e le cifre significative decimali
void countDigits(double number, int &integerDigits, int &decimalDigits) {
  integerDigits = 0;
  decimalDigits = 0;

  // Converti il numero in valore assoluto
  number = fabs(number);

  // Calcola le cifre totali della parte intera
  long long intPart = (long long)number; // Parte intera
  if (intPart == 0) {
    integerDigits = 1; // Per numeri come 0 o frazioni pure (es. 0.001)
  } else {
    integerDigits = (int)log10(intPart) + 1;
  }

  // Calcola le cifre significative decimali (max 4)
  double decimalPart = number - intPart;
  int count = 0;
  while (decimalPart > 0.0 && count < 4) {
    decimalPart *= 10;
    int currentDigit = (int)decimalPart % 10;
    if (currentDigit != 0 || decimalDigits > 0) { // Conta solo cifre significative
      decimalDigits++;
    }
    decimalPart -= (int)decimalPart;
    count++;
  }
}

// funzione effettiva di calcolo
// ── Tipi ──────────────────────────────────────────────────────────────────
// Arity: 0 = costante (nessun pop), 1 = unaria, 2 = binaria
enum class Arity { CONSTANT = 0, UNARY = 1, BINARY = 2 };

struct OpInfo {
  Arity arity;
  double (*fn)(double a, double b);  // b ignorato se arity < 2
};

// ── Tabella delle operazioni ───────────────────────────────────────────────
// Per aggiungere una nuova operazione: UNA sola riga qui, nient'altro.
static const std::unordered_map<char, OpInfo> OPS = {
  // binarie
  { '+', { Arity::BINARY,   [](double a, double b){ return a + b; } } },
  { '-', { Arity::BINARY,   [](double a, double b){ return a - b; } } },
  { '*', { Arity::BINARY,   [](double a, double b){ return a * b; } } },
  { '/', { Arity::BINARY,   [](double a, double b){ return b != 0 ? a / b : INFINITY; } } },
  { '^', { Arity::BINARY,   [](double a, double b){ return pow(a, b); } } },

  // unarie trigonometriche (input in gradi)
  { 'c', { Arity::UNARY,    [](double a, double){ return cos(cfg_calcDegrees ? DEG_TO_RAD * a : a); } } },
  { 's', { Arity::UNARY,    [](double a, double){ return sin(cfg_calcDegrees ? DEG_TO_RAD * a : a); } } },
  { 't', { Arity::UNARY,    [](double a, double){ return tan(cfg_calcDegrees ? DEG_TO_RAD * a : a); } } },  // ← bug corretto
  { 'y', { Arity::UNARY,    [](double a, double){ return cfg_calcDegrees ? RAD_TO_DEG * asin(a) : asin(a); } } },
  { 'u', { Arity::UNARY,    [](double a, double){ return cfg_calcDegrees ? RAD_TO_DEG * acos(a) : acos(a); } } },
  { 'i', { Arity::UNARY,    [](double a, double){ return cfg_calcDegrees ? RAD_TO_DEG * atan(a) : atan(a); } } },

  // unarie varie
  { 'r', { Arity::UNARY,    [](double a, double){ return sqrt(a); } } },
  { 'z', { Arity::UNARY,    [](double a, double){ return 1.0 / a; } } },
  { 'N', { Arity::UNARY,    [](double a, double){ return -a; } } },

  // costanti (nessun pop dallo stack)
  { 'P', { Arity::CONSTANT, [](double, double){ return M_PI; } } },
  { 'G', { Arity::CONSTANT, [](double, double){ return 9.80665; } } },

  // ── funzioni avanzate (menu shift) ────────────────────────────────────────
  { 'L', { Arity::UNARY,    [](double a, double){ return log10(a); } } },          // log10
  { 'l', { Arity::UNARY,    [](double a, double){ return log(a); } } },            // ln
  { 'E', { Arity::UNARY,    [](double a, double){ return exp(a); } } },            // e^x
  { 'A', { Arity::UNARY,    [](double a, double){ return log2(a); } } },           // log2
  { 'h', { Arity::BINARY,   [](double a, double b){ return hypot(a, b); } } },     // √(a²+b²)
  { 'o', { Arity::BINARY,   [](double a, double b){                               // atan2(y,x)
              double r = atan2(a, b);
              return cfg_calcDegrees ? RAD_TO_DEG * r : r; } } },
  { 'f', { Arity::UNARY,    [](double a, double){ return floor(a); } } },          // floor
  { 'k', { Arity::UNARY,    [](double a, double){ return ceil(a); } } },           // ceil
  { 'B', { Arity::UNARY,    [](double a, double){ return fabs(a); } } },           // |x|
  { 'M', { Arity::BINARY,   [](double a, double b){ return b != 0 ? fmod(a, b) : INFINITY; } } }, // mod
  { 'R', { Arity::UNARY,    [](double a, double){ return round(a); } } },          // round
  { 'd', { Arity::UNARY,    [](double a, double){ return a * DEG_TO_RAD; } } },    // deg→rad
  { 'D', { Arity::UNARY,    [](double a, double){ return a * RAD_TO_DEG; } } },    // rad→deg
  { 'K', { Arity::BINARY,   [](double a, double b){                               // lcm
              long ia = (long)fabs(a), ib = (long)fabs(b);
              if (ia == 0 || ib == 0) return 0.0;
              long g = ia, tmp = ib;
              while (tmp) { long t = tmp; tmp = g % tmp; g = t; }
              return (double)(ia / g * ib); } } },
};

// ── Funzione di esecuzione ─────────────────────────────────────────────────
void performOperationFromKey(char operation) {

  // caso speciale: 'x' = drop (non è un'operazione matematica)
  if (operation == 'x') {
    if (!stack.empty()) {
        stack.pop_back();
        displayDirty = true;
      }
    return;
  }

  // caso speciale: 'V' = media di tutti gli elementi dello stack
  if (operation == 'V') {
    if (stack.empty()) return;
    double sum = 0.0;
    int n = stack.size();
    for (double v : stack) sum += v;
    stack.clear();
    stack.push_back(sum / n);
    displayDirty = true;
    return;
  }

  auto it = OPS.find(operation);
  if (it == OPS.end()) {
    Serial.println("Operazione non valida");
    return;
  }

  const OpInfo& op = it->second;
  int needed = static_cast<int>(op.arity);

  if ((int)stack.size() < needed) {
    Serial.println("Errore: stack insufficiente");
    return;
  }

  double a = 0, b = 0;
  if (op.arity == Arity::BINARY) {
    b = stack.back(); stack.pop_back();
    a = stack.back(); stack.pop_back();
  } else if (op.arity == Arity::UNARY) {
    a = stack.back(); stack.pop_back();
  }
  // CONSTANT: nessun pop

  double result = op.fn(a, b);
  pushToStack(result);
  lastPush = result;
}

void clearStack() {
  stack.clear();
  Serial.println("Stack pulito");
}

// ######################## CONFIG MENU DISPLAY ##########################

void printConfigMenu() {
  // testo scorrevole header
  static const char hint[] = "  SU/GIU=voce  LR=valore  ENTER=salva+esci  HoldD=esci  ";
  static int  hintOffset = 0;
  static unsigned long lastHintScroll = 0;
  if (millis() - lastHintScroll >= 120) {
    lastHintScroll = millis();
    hintOffset++;
    int hintLen = strlen(hint);
    if (hintOffset >= hintLen) hintOffset = 0;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  // riga titolo fissa
  display.setCursor(0, 0);
  display.print("== CONFIG MENU ==");

  // riga scorrevole: stampa caratteri a partire da hintOffset, wrappa
  {
    int hintLen = strlen(hint);
    int x = 0;
    for (int ci = 0; ci < 21 && x < 128; ci++) {  // max 21 chars a 6px = 126px
      char ch = hint[(hintOffset + ci) % hintLen];
      display.setCursor(x, 9);
      display.write(ch);
      x += 6;
    }
  }

  display.drawFastHLine(0, 18, 128, SH110X_WHITE);

  int startIdx = cfgMenuIndex - 1;
  if (startIdx < 0) startIdx = 0;
  if (startIdx > CFG_ENTRIES_COUNT - 4) startIdx = max(0, CFG_ENTRIES_COUNT - 4);

  for (int i = startIdx; i < min(startIdx + 4, CFG_ENTRIES_COUNT); i++) {
    int y = 21 + (i - startIdx) * 26;
    bool selected = (i == cfgMenuIndex);

    if (selected) {
      display.fillRect(0, y - 1, 123, 21, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
    } else {
      display.setTextColor(SH110X_WHITE);
    }

    display.setCursor(2, y + 2);
    display.setTextSize(1);
    display.print(cfgEntries[i].label);
    display.print(": ");

    if (cfgEntries[i].type == CfgEntry::BOOL_T) {
      bool v = *(bool*)cfgEntries[i].ptr;
      display.print(v ? "DEG" : "RAD");
    } else {
      display.print(cfgGetLong(i));
    }

    // barra progresso per voci numeriche selezionate
    if (selected && cfgEntries[i].type != CfgEntry::BOOL_T) {
      long rng = cfgEntries[i].maxVal - cfgEntries[i].minVal;
      long cur = cfgGetLong(i) - cfgEntries[i].minVal;
      int barW = (int)((long)119 * cur / max(rng, 1L));
      barW = constrain(barW, 0, 119);
      display.drawRect(2, y + 17, 121, 3, SH110X_BLACK);
      display.fillRect(2, y + 17, barW, 3, SH110X_BLACK);
    }
    display.setTextColor(SH110X_WHITE);
  }

  // scrollbar laterale
  if (CFG_ENTRIES_COUNT > 4) {
    int sbH = max(4, 107 / CFG_ENTRIES_COUNT);
    int sbY = 20 + (int)((long)(107 - sbH) * cfgMenuIndex / (CFG_ENTRIES_COUNT - 1));
    display.fillRect(125, sbY, 3, sbH, SH110X_WHITE);
  }

  display.display();
}
// ─────────────────────────────────────────────────────────────────────────────

// ######################## CALC FUNCTION MENU DISPLAY ##########################

void printCalcFnMenu() {
  static const char hint[] = "  SU/GIU=voce  JOY/ENTER=esegui  HoldA=esci  ";
  static int  hintOffset = 0;
  static unsigned long lastHintScroll = 0;
  if (millis() - lastHintScroll >= 120) {
    lastHintScroll = millis();
    hintOffset++;
    int hintLen = strlen(hint);
    if (hintOffset >= hintLen) hintOffset = 0;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  // riga titolo fissa
  display.setCursor(0, 0);
  display.print("== FUNZIONI CALC ==");

  // riga scorrevole
  {
    int hintLen = strlen(hint);
    int x = 0;
    for (int ci = 0; ci < 21 && x < 128; ci++) {
      char ch = hint[(hintOffset + ci) % hintLen];
      display.setCursor(x, 9);
      display.write(ch);
      x += 6;
    }
  }

  display.drawFastHLine(0, 18, 128, SH110X_WHITE);

  int startIdx = calcFnMenuIndex - 1;
  if (startIdx < 0) startIdx = 0;
  if (startIdx > CALC_FN_MENU_COUNT - 4) startIdx = max(0, CALC_FN_MENU_COUNT - 4);

  for (int i = startIdx; i < min(startIdx + 4, CALC_FN_MENU_COUNT); i++) {
    int y = 21 + (i - startIdx) * 26;
    bool selected = (i == calcFnMenuIndex);

    if (selected) {
      display.fillRect(0, y - 1, 123, 21, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
    } else {
      display.setTextColor(SH110X_WHITE);
    }

    display.setCursor(2, y + 2);
    display.setTextSize(1);
    display.print(CALC_FN_MENU[i].label);

    // descrizione a destra
    display.setCursor(50, y + 2);
    display.print(CALC_FN_MENU[i].desc);

    display.setTextColor(SH110X_WHITE);
  }

  // scrollbar laterale
  if (CALC_FN_MENU_COUNT > 4) {
    int sbH = max(4, 107 / CALC_FN_MENU_COUNT);
    int sbY = 20 + (int)((long)(107 - sbH) * calcFnMenuIndex / (CALC_FN_MENU_COUNT - 1));
    display.fillRect(125, sbY, 3, sbH, SH110X_WHITE);
  }

  display.display();
}
// ─────────────────────────────────────────────────────────────────────────────

void replaceCharInArray(char* charArray, char searchChar, char replaceChar) {
  int i = 0;
  while (charArray[i] != '\0') {
    if (charArray[i] == searchChar) {
      charArray[i] = replaceChar;
    }
    i++;
  }
}

//FUNZIONE PER RUOTARE LO STACK
void rotateStack() {
  // Controlliamo se ci sono abbastanza elementi per la rotazione
  if (stack.size() < 2) {
    Serial.println("Errore: servono almeno due numeri nello stack per la rotazione");
    return;
  }

  // Ruota lo stack di una posizione
  // Il primo elemento diventa l'ultimo, tutti gli altri si spostano avanti
  std::rotate(stack.begin(), stack.begin() + 1, stack.end());
}


//FUNZIONE PER AVERE SCRIVERE LE SAGOME VELOCEMENTE
String numberToLetters(int val) {
  String result = "";

  do {
    int remainder = val % 26;
    result = char('a' + remainder) + result;
    val = val / 26 - 1;
  } while (val >= 0);

  return result;
}


//VIRGOLA O PUNTO?
char mapStateToChar(int stateValue) {
    if (stateValue == 0) return ',';
    else if (stateValue == 1) return '.';
    return '?';
}
