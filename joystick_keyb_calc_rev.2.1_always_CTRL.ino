//scritto da: Alessandro Mammino
//mail: krafen885@hotmail.com
//project link: https://www.thingiverse.com/thing:7285652
//ultima: la più libera possibile, ognuno faccia quello che vuole. GNU GPL

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <math.h>
#include <deque>
#include <string.h>
#include <stdlib.h>
#include <string> <iostream> 

#include <Bounce2.h>

#include <USB.h> 
#include <USBHIDKeyboard.h>
USBHIDKeyboard Keyboard;

#include <USBHIDMouse.h>
USBHIDMouse Mouse;

#include <Keypad.h>

#include <algorithm>

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

//JOYSTICK SETUP
#define analogDeadBand 35
#define sensitivityScroll 30000 //il CONTRARIO della sensitivity (DEFAULT 15000)
#define fixSensScrol 20
#define fixSensArrow 20
#define sensitivityArrow 35000
#define singleScroll 1
#define WINDOW_SIZE 5     // Dimensione della finestra di lettura

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
String numBak1 = "w";
String numBak2 = "w";
String numBak3 = "w";

//VARIABILI PER CALCOLATRICE
int stackSizeVar = 30;
#define MAX_STACK_SIZE stackSizeVar //era 10. aumentato a 30!
std::deque<double> stack;
std::string currentInput = "";  // Buffer per il numero corrente
bool isScientific = false;  // Flag per la gestione della notazione scientifica

double lastPush = 0.0;

char buffer[50]; //buffer di stampa a schermo
char lastCalcNum[50]; //buffer di stampa su keypad


//INIZIO SETUP

void setup() {
  // Inizializzazione Serial Monitor
  Serial.begin(115200);
  
  // ######################## INIZIALIZZAZIONE MOUSE 1 ##########################
  pinMode(vertJoyPIN, INPUT);  // set button select pin as input
  pinMode(horJoyPIN, INPUT);  // set button select pin as input
  pinMode(buttonJoyPIN, INPUT_PULLUP);  // set button select pin as input

  // ######################## INIZIALIZZAZIONE SCHERMO ##########################

  // Inizializzazione I2C
  Wire.begin(47, 21); // SDA, SCL
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
  USB.begin();

  // ######################## INIZIALIZZAZIONE ????? ##########################
  lastActionTimer = millis();
}

// ######################## MAIN LOOP ##########################

void loop() {

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
                if (keyFuncState == 0){Mouse.press(MOUSE_MIDDLE);numpad_print("mid mouse");}
                if (keyFuncState == 1){Keyboard.press(KEY_BACKSPACE);numpad_print("backspc");}
                if (keyFuncState == 2){shiftSwitchCalc();break;}
                if (keyFuncState == 3){shiftSwitchCalc();break;}
                break;
              case RELEASED:
                if (keyFuncState == 0){Mouse.release(MOUSE_MIDDLE);}
                if (keyFuncState == 1){Keyboard.release(KEY_BACKSPACE);}
                break;
              case IDLE:
                break;
              case HOLD:
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
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 3){
                  shiftSwitchCalc();
                }
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
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                funcSwitch1(); 
                break;
              case RELEASED:
                break;
              case IDLE:
                break;
              case HOLD:
                printTimer();
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
            switch (kpad.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
              case PRESSED:
                if (keyFuncState == 0){
                  char buf[32]; // Assicurati che sia abbastanza grande
                  sprintf(buf, "%d_a%d%s%03d_sa_%s",
                    (int)stack[0],
                    (int)stack[1],
                    "a_gn",
                    (int)stack[2],
                    numberToLetters((int)stack[3]));
                  Keyboard.print(buf);
                  for (int i = 0; buf[i]; i++) {
                      buf[i] = toupper(buf[i]);
                    }
                  numpad_print(buf);
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
                  Keyboard.press('q'); //soliworks upd
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
    //Serial.println(joyConf);
  }

  //############################GESTIONE ASSE VERTICALE############################
  if (abs(vertRawValue - vertZero) > analogDeadBand)
  {
    //joystick horizz mosso in modo ponderato
    scrollScrExit = true;
    //if (!joyConf)
    //{
      scrollPeriod = fixSensScrol + abs(sensitivityScroll / (vertRawValue - vertZero));
      if ((millis() - lastVertScroll) >= scrollPeriod)                            //#
      {
        if ((vertRawValue - vertZero) > 0)                                        //#
        {
          Mouse.move(0, 0, +singleScroll);
          ScrollCount = ScrollCount + singleScroll;
        }
        else
        {
          Mouse.move(0, 0, -singleScroll);
          ScrollCount = ScrollCount + singleScroll;
        }
        lastVertScroll = millis();
        //lastActionTimer = millis();
        Serial.print("vert");
        Serial.println(vertRawValue);
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
  if (abs(horRawValue - horZero) > analogDeadBand)
  {
    scrollScrExit = true;
    //joystick horizz mosso in modo ponderato
    // if (!joyConf)
    // {
      scrollPeriod = fixSensScrol + abs(sensitivityScroll / (horRawValue - horZero));
      if ((millis() - lastHorScroll) >= scrollPeriod)                             //#
      {
        if ((horRawValue - horZero) > 0)                                        //#
        {
          Mouse.move(0, 0, -singleScroll*3);
          ScrollCount = ScrollCount + singleScroll*3;
        }
        else
        {
          Mouse.move(0, 0, +singleScroll*3);
          ScrollCount = ScrollCount + singleScroll*3;
        }
        lastHorScroll = millis();
        //lastActionTimer = millis();
        Serial.print("hor");
        Serial.println(horRawValue);
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
  if (keyFuncState != 2)
  {
    prevKeyFstate = keyFuncState;
    keyFuncState = 2;
    screenUpdate = false;

    printStack();

  }
  else 
  {
    keyFuncState = prevKeyFstate;
    screenUpdate = true;
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
    printStack();
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
  if (millis() > printTimerEnd and SscreenUpdate)
  {
	clickDisplay = false;
	screenUpdate = true;
  // Serial.println(screenUpdate);
  }
  
  if (screenUpdate)
  {
    if (keyFuncState == 1 or keyFuncState == 0)
    {
    joystatPrt();
    SscreenUpdate = false;
    }
  }
  
  screenUpdate = false;
  
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
  display.setCursor(0,100);
  display.print("Buttons: ");
  //display.setCursor(0,110);
  display.println(keyStrCount);
  display.print("Scrolls: ");
  display.println(ScrollCount);
  display.display();
  // effettivo comando di output a schermo
  display.display();
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
  display.setCursor(0,100);
  display.print("Buttons: ");
  //display.setCursor(0,110);
  display.println(keyStrCount);
  display.print("Scrols: ");
  display.println(ScrollCount);
  display.display();
  numBak3 = numBak2;
  numBak2 = numBak1;
  numBak1 = numb;
  printTimer2();
}

// ############ SCREENSAVER ###########

void screensaver() {
  static int columnY[21];     // posizione verticale testa di colonna
  static int speed[21];       // velocità di caduta di ogni colonna
  static bool initialized = false;
  static unsigned long lastFrameTime = 0; // per temporizzare con millis()


  // Variabili statiche per movimento orizzontale info
  static int infoX = 64;      // posizione iniziale
  static int infoDir = 1;     // direzione (1 = destra, -1 = sinistra)
  static unsigned long lastInfoMove = 0;
  static int infoInterval = 100; // intervallo in ms per movimento info (più alto = più lento)

  // inizializzazione
  if (!initialized) {
    for (int i = 0; i < 21; i++) {
      columnY[i] = random(-40, 0);
      speed[i] = random(1, 4); // velocità 1..3
    }
    initialized = true;
  }

  // se non sono passati 30 secondi dall'ultima azione, esco
  if (millis() - lastActionTimer <= 30000) return;

  if (scrollScrExit) {
      scrollScrExit = false;
      lastActionTimer = millis();
      if (keyFuncState == 2 || keyFuncState == 3) {
        printStack();
        return;
      }
      screenUpdate = true;
      return;
  }

  // disegna solo ogni 50 ms (senza bloccare)
  if (millis() - lastFrameTime >= 100) {
    lastFrameTime = millis();

    display.clearDisplay();

    // Disegna le colonne
    for (int col = 0; col < 21; col++) {
      int x = col * 6;
      int y = columnY[col];

      // carattere in base a keyFuncState
      char ch;
      if (keyFuncState == 0) ch = 'm';
      if (keyFuncState == 1) ch = '0' + random(0, 10);
      if (keyFuncState == 2 || keyFuncState == 3) ch = "calc"[random(0, 4)];

      // testa colonna
      display.setTextSize(1);
      display.setCursor(x, y);
      display.write(ch);

      // scia
      for (int trail = 1; trail <= 3; trail++) {
        int ty = y - trail * 8;
        if (ty >= 0 && ty < SCREEN_HEIGHT) {
          display.setCursor(x, ty);
          if (trail == 1) display.write(ch);
          else if (trail == 2) display.write('.');
          else display.write(' ');
        }
      }

      // aggiorna posizione
      columnY[col] += speed[col] * 2;
      if (columnY[col] > SCREEN_HEIGHT) {
        columnY[col] = random(-20, 0);
        speed[col] = random(1, 4);
      }
    }
    // --- Dentro la parte di disegno dello screensaver ---
    unsigned long now = millis();

    // Movimento orizzontale BUt/Scr più lento del Matrix
    if (now - lastInfoMove >= infoInterval) {
        lastInfoMove = now;
        infoX += infoDir;
        if (infoX > 64) infoDir = -1; // limite destro a metà schermo
        if (infoX < 0) infoDir = 1;   // limite sinistro
    }

    // Disegno BUt e Scr nella nuova posizione orizzontale
    display.setTextSize(1);
    display.setCursor(infoX, SCREEN_HEIGHT - 16); 
    display.print("BUt: ");
    display.println(keyStrCount);

    display.setCursor(infoX, SCREEN_HEIGHT - 8);  
    display.print("Scr: ");
    display.println(ScrollCount);

    display.display();
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
    printStack();
  }
  else if (key == 'n') {
    // Gestisci il simbolo meno per i numeri negativi o per l'esponente
    if (currentInput.empty() || currentInput[currentInput.size() - 1] == 'e') {
      // Aggiungi il meno all'inizio di un numero o dopo 'e'
      currentInput += '-';
      Serial.print("Numero corrente (negativo): ");
      Serial.println(currentInput.c_str());
      printStack();
    }
  }
  else if (key == '.') {
    // Gestisci il punto decimale
    if (currentInput.find('.') == std::string::npos) {  // Verifica se c'è già un punto
      currentInput += '.';
      Serial.print("Numero corrente: ");
      Serial.println(currentInput.c_str());
      printStack();
    }
  }
  else if (key == 'e') {
    // Gestisci la notazione scientifica (esponente)
    if (!isScientific && !currentInput.empty()) {
      currentInput += 'e';  // Aggiungi il carattere 'e'
      isScientific = true;   // Attiva la modalità scientifica
      Serial.print("Notazione scientifica attivata: ");
      Serial.println(currentInput.c_str());
      printStack();
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
    printStack();
  }
  else if (key == 'D') {
    if(currentInput.length() > 0) {
      int lastIndex = currentInput.length() - 1;
      currentInput.erase(lastIndex);
    }
    printStack();
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
  printStack();
}

// stampa a schermo dello stack
void printStack() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println(currentInput.c_str());
  display.setTextSize(1);
  display.print("Stack pos ");
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
void performOperationFromKey(char operation) {

  double a;
  double b;
  double result;

  if (operation == 'c' || operation == 's' || operation == 't' || operation == 'y' || operation == 'u' || operation == 'i'|| operation == 'r' || operation == 'z' || operation == 'N') 
  {
    if (stack.size() < 1 && operation == 'N') {
      Serial.println("Errore: meno di due numeri nello stack");
      return;
    }
    a = stack.back(); stack.pop_back();
  }
  else if(operation == 'x' && stack.size() > 0)
  {
    stack.pop_back();
    printStack();
    return;
  }
  else if(operation != 'P' && operation != 'G') //caso particolare per output di Pi greco od altre costanti
  {
    if (stack.size() < 2) {
      Serial.println("Errore: meno di due numeri nello stack");
      return;
    }
    b = stack.back(); stack.pop_back();
    a = stack.back(); stack.pop_back();
  }

  switch (operation) {
    case '+':
      result = a + b;
      break;
    case '-':
      result = a - b;
      break;
    case '*':
      result = a * b;
      break;
    case '/':
      if (b != 0) {
        result = a / b;
        } else {
        result = INFINITY;
        }
      break;
    case '^':
      result = pow(a, b);
      break;
    case 'c':
      result = cos(DEG_TO_RAD * a);
      break;
    case 's':
      result = sin(DEG_TO_RAD * a);
      break;
    case 't':
      result = sin(DEG_TO_RAD * a);
      break;
    case 'y':
      result = RAD_TO_DEG * asin(a);
      break;
    case 'u':
      result = RAD_TO_DEG * acos(a);
      break;
    case 'i':
      result = RAD_TO_DEG * atan(a);
      break;
    case 'r':
      result = sqrt(a);
      break;
    case 'z':
      result = 1.00/a;
      break;
    case 'N':
      result = -a;
      break;
    case 'P':
      result = 3.1415926535;
      break;
    case 'G':
      result = 9.806650000;
      break;
    default:
      Serial.println("Operazione non valida");
      return;
  }

  pushToStack(result);  // Aggiungi il risultato allo stack
  lastPush = result;
}

void clearStack() {
  stack.clear();
  Serial.println("Stack pulito");
}

//FUNZIONE PER CAMBIARE PUNTO IN VIRGOLA

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
  
  // Opzionale: stampa dello stack dopo la rotazione
  printStack();
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
    if (stateValue == 0) {
        return ',';
    } else if (stateValue == 1) {
        return '.';
    }
    return '?'; // carattere di fallback
}
