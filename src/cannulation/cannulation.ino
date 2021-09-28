/*
  canulation.ino
  
  Biola Summer Engineering Internship 2021 Project: Automated Canulation of Mouse Aorta
  This program controls a display, a light source, and three solenoid valves using button controls.

  Copywrite Daniel Dirksen 2021. All rights reserved.
  
*/

// dependencies
#include "Adafruit_ILI9341.h"

#define SEC_PER_MIN 1 // Number of seconds in a minute. Can be reduced for faster debugging.


// tft screen pins
#define PIN_CS 0
#define PIN_RESET 2
#define PIN_DC 3
#define PIN_MOSI 4
#define PIN_SCLK 5
#define PIN_MISO 6

// other pins
#define PIN_DOWN 7
#define PIN_ENTER 8
#define PIN_UP 9
#define PIN_SOLN1 10
#define PIN_SOLN2 11
#define PIN_RECYCLE 12
#define PIN_BUZZER 13
//#define PIN_BUZZER 1

// enum for event type
#define EVT_NONE 0
#define EVT_UP 1
#define EVT_ENTER 2
#define EVT_DOWN 3

#define NUM_MENUS 8 // size of menu list. blank manus will not be used.
#define NUM_ITEMS 8 // maximum number of items allowed.

// enum for types of buttons
#define TYP_NONE 0
#define TYP_NAV 1
#define TYP_CHECK 2
#define TYP_SLIDER 3

// enum for the state of the menu and program. these should be in order.
#define MNU_LOADING 0
#define MNU_READY 1
#define MNU_OPTIONS 2
#define MNU_OPTIONS_SAVED 3
#define MNU_RUNNING 4
#define MNU_PAUSED 5
#define MNU_FINISHED 6

struct Item {
  String name;
  int8_t type;
  int8_t targetMenu;
  int8_t *target;
};

struct Menu {
  String name;
  struct Item items[NUM_ITEMS];
};

struct Options {
  uint8_t recycle1;
  uint8_t recycle2;
  uint8_t duration1;
  uint8_t duration2;
};

struct State {
  int8_t menu;
  int8_t cursor;
  int8_t inputting; 
  int progress; // test progress in seconds
  struct Options options;
};

// do not replace or move this data.
struct State state = {MNU_LOADING, 1, false, 0, {0, 0, 10, 10}};

Adafruit_ILI9341 tft = Adafruit_ILI9341(PIN_CS, PIN_DC, PIN_MOSI, PIN_SCLK, PIN_RESET, PIN_MISO);
int8_t event = EVT_NONE;

const String arrowString = "->";
const String xString = "x";
String valHolder = "   ";

const struct Menu menus[NUM_MENUS] = {
  {"Loading...", {}},
  {"Ready.", {
    {"Start", TYP_NAV, MNU_RUNNING},
    {"Options", TYP_NAV, MNU_OPTIONS}
  }},
  {"Options:", {
    {"Back", TYP_NAV, MNU_READY},
    {"Recycle Soln 1", TYP_CHECK, 0, &(state.options.recycle1)},
    {"Recycle Soln 2", TYP_CHECK, 0, &(state.options.recycle2)},
    {"Soln 1 time (mins)", TYP_SLIDER, 0, &(state.options.duration1)},
    {"Soln 2 time (mins)", TYP_SLIDER, 0, &(state.options.duration2)},
    {"Save Options", TYP_NAV, MNU_OPTIONS_SAVED}
  }},
  {"Coming Soon.", {
    {"Back", TYP_NAV, MNU_OPTIONS}
  }},
  {"Running...", {
    {"Pause", TYP_NAV, MNU_PAUSED},
    {"Stop", TYP_NAV, MNU_READY}
  }},
  {"Paused.", {
    {"Resume", TYP_NAV, MNU_RUNNING},
    {"Stop", TYP_NAV, MNU_READY}
  }},
  {"Finished.", {
    {"Main Menu", TYP_NAV, MNU_READY}
  }},
  {}
};

void beep(int8_t len = 1) {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(len);
  digitalWrite(PIN_BUZZER, LOW);
}

// txt sizes:
// 1: 6x8 px
// 2: 12x16 px
// 3: 18x24 px
// 4: 24x32 px
void text(const String &txt, int x = 0, int y = 0, int8_t txtSize = 1, int8_t color = ILI9341_BLACK) {
  tft.setCursor(x, y);
  tft.setTextColor(color);
  tft.setTextSize(txtSize);
  tft.print(txt);
}

void eraseText(String txt, int x = 0, int y = 0, int8_t txtSize = 1, int8_t color = ILI9341_WHITE) {
  tft.fillRect(x, y, 6*txtSize * txt.length(), 8*txtSize, ILI9341_WHITE);
}

bool pressed(int8_t buttonPin) {
  return digitalRead(buttonPin) == LOW;
}

void drawItem(const struct Item& item, int8_t index) {
  int checkX = 10;
  int textX = checkX + 15;
  int valX = textX + 120;
  int y = 30 + 10 * index;
  tft.fillRect(checkX, y, 152, 8, ILI9341_WHITE);
  if (state.cursor == index) {
    tft.fillRect(checkX, y, 8, 8, ILI9341_BLUE);
  }
  if (item.type != TYP_NONE) {
    int8_t color = state.cursor == index ? (state.inputting ? ILI9341_GREEN : ILI9341_BLUE) : ILI9341_BLACK;
    text(item.name, textX, y, 1, color);
    switch (item.type) {
      case TYP_NAV:
        text(arrowString, valX, y, 1, color);
        break;
      case TYP_CHECK:
        tft.drawRect(valX, y, 7, 7, ILI9341_BLACK);
        if (*(item.target)) {
          //Serial.println("X");
          text(xString, valX + 1, y - 1, 1, color);
        }
        break;
      case TYP_SLIDER:

        // due to memory limits, this is a fancy way of printing the item value
        int value = *(item.target);
        valHolder.setCharAt(0, '0' + value / 100);
        valHolder.setCharAt(1, '0' + (value / 10) % 100);
        valHolder.setCharAt(2, '0' + value % 10);
        //Serial.println(valHolder);
        text(valHolder, valX, y);
        break;
    }
  }
}

void drawMenu(const struct Menu& menu) {
  tft.fillRect(5, 5, 6*25 + 10, 8*NUM_ITEMS + 45, ILI9341_WHITE);
  tft.drawRect(5, 5, 6*25 + 10, 8*NUM_ITEMS + 45, ILI9341_BLACK);
  text(menu.name, 10, 10, 2, ILI9341_BLUE);
  for (int8_t i = 0; i < NUM_ITEMS; i ++) {
    drawItem(menu.items[i], i);
  }
}

void changeMenu(int8_t newMenu) {
  state.menu = newMenu;
  state.cursor = 0;
  drawMenu(menus[newMenu]);
}

void getEvent() {
  static int8_t lastEvent = EVT_NONE;
  int8_t newEvent = EVT_NONE;
  if (pressed(PIN_UP)) {
    newEvent = EVT_UP;
  } else if (pressed(PIN_ENTER)) {
    newEvent = EVT_ENTER;
  } else if (pressed(PIN_DOWN)) {
    newEvent = EVT_DOWN;
  }
  if (
    (lastEvent == EVT_NONE && newEvent != EVT_NONE) ||
    ((newEvent == EVT_UP || newEvent == EVT_DOWN) && state.inputting)
  ) {
    event = newEvent;
    beep();
  } else {
    event = EVT_NONE;
  }
  lastEvent = newEvent;
}

void handleEnter() {
  const struct Item &item = menus[state.menu].items[state.cursor];
  //Serial.print("Item Type: ");
  //Serial.println(item.type);
  switch (item.type) {
    case TYP_NONE:
      break;
    case TYP_NAV:
      //Serial.print("Target menu: ");
      //Serial.println(item.targetMenu);
      changeMenu(item.targetMenu);
      break;
    case TYP_CHECK:
      *(item.target) = (*(item.target) == 0) ? 1 : 0;
      drawItem(item, state.cursor);
      break;
    case TYP_SLIDER:
      state.inputting = !state.inputting;
      drawItem(item, state.cursor);
      break;
    default:
      text("ERROR: 2");
  }
}

void handleEvent() {
  int8_t previousCursor = state.cursor;
  int8_t nextCursor;
  const struct Item &item = menus[state.menu].items[state.cursor];
  switch (event) {
    case EVT_UP:
      //Serial.println("UP");
      if (state.inputting) {
        uint8_t *slider = (item.target);
        *(slider) = *(slider) + 1;
      } else {
        state.cursor = max(state.cursor - 1, 0);
        drawItem(menus[state.menu].items[state.cursor], state.cursor);
      }
      drawItem(item, previousCursor);
      break;
    case EVT_DOWN:
      //Serial.println("DOWN");
      if (state.inputting) {
        uint8_t *slider = (item.target);
        *(slider) = *(slider) - 1;
      } else {
        state.cursor = min(state.cursor + 1, NUM_ITEMS - 1);
        drawItem(menus[state.menu].items[state.cursor], state.cursor);
      }
      drawItem(item, previousCursor);
      break;
    case EVT_ENTER:
      //Serial.println("ENTER");
      handleEnter();
      break;
  }
}

unsigned long long int previousCount = 0;

void calcState() {
  switch (state.menu) {
    case MNU_LOADING:
      pinMode(PIN_UP, INPUT_PULLUP);
      pinMode(PIN_ENTER, INPUT_PULLUP);
      pinMode(PIN_DOWN, INPUT_PULLUP);
      
      pinMode(PIN_SOLN1, OUTPUT);
      pinMode(PIN_SOLN2, OUTPUT);
      pinMode(PIN_RECYCLE, OUTPUT);
      pinMode(PIN_BUZZER, OUTPUT);
      
      Serial.begin(9600);
      tft.begin();
      tft.setRotation(1);
      text("Loading...", 10, tft.height() - 20);
      tft.fillScreen(ILI9341_RED);
      changeMenu(MNU_READY);
      break;
    case MNU_READY:
      state.progress = 0;
      getEvent();
      handleEvent();

      // if state has switched to running, prepare the progress bar.
      if (state.menu == MNU_RUNNING) {
        #define BAR_START 180
        #define BAR_WIDTH 100
        tft.fillRect(BAR_START - 4, 5, BAR_WIDTH + 10, BAR_WIDTH, ILI9341_WHITE);
        tft.drawRect(BAR_START - 5, 4, BAR_WIDTH + 12, BAR_WIDTH + 2, ILI9341_BLACK);
        text("Progress:", BAR_START, 10);
        tft.drawRect(BAR_START, 19, BAR_WIDTH + 2, 10, ILI9341_BLACK);
        previousCount = millis();
      }
      break;
    case MNU_OPTIONS:
      getEvent();
      handleEvent();
      break;
    case MNU_OPTIONS_SAVED:
      getEvent();
      handleEvent();
      break;
    case MNU_PAUSED:
      //Serial.println("Paused.");
      getEvent();
      handleEvent();
      break;
    case MNU_FINISHED:
      getEvent();
      handleEvent();
      break;
    case MNU_RUNNING:
      int totalDuration = state.options.duration1 + state.options.duration1;
      if (state.progress / SEC_PER_MIN >= totalDuration) {
        changeMenu(MNU_FINISHED);
      } else {
        int timeSincePrevious = millis() - previousCount;
        //Serial.println(timeSincePrevious);
        if (timeSincePrevious < 0 || timeSincePrevious >= 1000) {
          previousCount = millis();
          state.progress ++;
          tft.fillRect(BAR_START + 1, 20, BAR_WIDTH * state.progress / SEC_PER_MIN / totalDuration, 8, ILI9341_GREEN);
          int x = BAR_START;
          int x2 = BAR_START + 80;
          bool soln1 = state.progress / SEC_PER_MIN < state.options.duration1;
          bool recycling = soln1 ? state.options.recycle1 : state.options.recycle2;
          tft.fillRect(x, 30, x2-x + 20, 40, ILI9341_WHITE);
          text("Mins Elapsed:", x, 30);   text(String(state.progress / SEC_PER_MIN), x2, 30);
          text("Mins Left:", x, 40);      text(String(totalDuration - state.progress / SEC_PER_MIN), x2, 40);
          text("Solution:", x, 50);       text(soln1 ? "1" : "2", x2, 50);
          text("Recycling:", x, 60);      text(recycling ? "Yes" : "No", x2, 60);
        }
      }
      
      
      getEvent();
      handleEvent();
      break;
    default:
      text("ERROR: 3");
      break;
  }
  //Serial.print("Menu: ");
  //Serial.println(state.menu);
  //Serial.print("Event: ");
  //Serial.println(event);
}

void writePinsFromState() {
  if (state.menu == MNU_RUNNING) {
    if (state.progress / SEC_PER_MIN < state.options.duration1) {
      digitalWrite(PIN_SOLN1, HIGH);
      digitalWrite(PIN_SOLN2, LOW);
      digitalWrite(PIN_RECYCLE, state.options.recycle1 ? HIGH : LOW);
    } else {
      digitalWrite(PIN_SOLN1, LOW);
      digitalWrite(PIN_SOLN2, HIGH);
      digitalWrite(PIN_RECYCLE, state.options.recycle2 ? HIGH : LOW);
    }
    
  } else {
    digitalWrite(PIN_SOLN1, LOW);
    digitalWrite(PIN_SOLN2, LOW);
    digitalWrite(PIN_RECYCLE, LOW);
  }
}

// this is the tradional one-time-only arduino function which runs at the start of the program
void setup() {
  //valHolder.reserve(8);
}

// function is called over and over again for the rest of the program.
// It usually does nothing, unless a clock cycle is needed, in which case it runs through the state and render cycle
void loop() {
  calcState();
  writePinsFromState();
  delay(10);
}
