#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BH1750.h>
#include <OneButton.h>
#include <Preferences.h> 


#define PIN_BATTERY 0
#define PIN_BTN_MENU 2   
#define PIN_BTN_CHANGE 3 


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


BH1750 lightMeter;


Preferences prefs;


OneButton btnMenu(PIN_BTN_MENU, true);
OneButton btnChange(PIN_BTN_CHANGE, true);


enum AppState { STATE_MAIN, STATE_MENU };
AppState currentState = STATE_MAIN;


const float dividerRatio = 2.0; 
int selectedIndex = 1; 
int lockMode = 1; 

float currentLux = 0;     
float accumulatedLux = 0; 
int sampleCount = 0;      
float currentEV = 0;


int settingsBrightness = 2; 
int settingsBatStyle = 0;   
int settingsSleepIdx = 5;   
int settingsEvShift = 0;   

unsigned long lastActivityTime = 0; 
unsigned long lastLuxUpdate = 0; 


const float fStops[] = { 0.7, 0.8, 0.9, 0.95, 1.0, 1.1, 1.2, 1.4, 1.6, 1.7, 1.8, 2.0, 2.2, 2.4, 2.5, 2.8, 3.2, 3.5, 4.0, 4.5, 4.8, 5.0, 5.6, 6.3, 6.7, 7.1, 8.0, 9.0, 9.5, 10, 11, 12.5, 13, 14, 16, 18, 19, 20, 22, 25, 29, 32, 36, 40, 45, 51, 57, 64 };
const int fStopCount = sizeof(fStops) / sizeof(fStops[0]);

const float shutterSpeeds[] = { 8000, 6400, 5000, 4000, 3200, 2500, 2000, 1600, 1250, 1000, 800, 640, 500, 400, 320, 250, 200, 160, 125, 100, 80, 60, 50, 40, 30, 25, 20, 15, 13, 10, 8, 6, 5, 4, 3, 2.5, 2, 1.6, 1.3, -1, -1.3, -1.6, -2, -2.5, -3, -4, -5, -6, -8, -10, -13, -15, -20, -25, -30 };
const int shutterCount = sizeof(shutterSpeeds) / sizeof(shutterSpeeds[0]);

const int isoValues[] = { 6, 12, 25, 50, 64, 80, 100, 125, 160, 200, 250, 320, 400, 500, 640, 800, 1000, 1250, 1600, 2000, 2500, 3200, 6400, 12800 };
const int isoCount = sizeof(isoValues) / sizeof(isoValues[0]);

const long sleepTimes[] = { 0, 3000, 5000, 10000, 30000, 60000, 180000, 300000, 600000, 900000, 1800000, 3600000 };
const char* sleepLabels[] = { "Never", "3 sec", "5 sec", "10 sec", "30 sec", "1 min", "3 min", "5 min", "10 min", "15 min", "30 min", "1 hour" };
const int sleepTimesCount = 12;

int isoIdx = 6;       
int apertureIdx = 22; 
int shutterIdx = 18;  

int menuIndex = 0; 
const int menuItemsCount = 5;



void setBrightness(int level) {
  int contrast = 0;
  switch(level) {
    case 0: contrast = 1; break;   
    case 1: contrast = 60; break;  
    case 2: contrast = 127; break; 
    case 3: contrast = 190; break; 
    case 4: contrast = 255; break; 
    default: contrast = 127; break;
  }
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(contrast);
}

void loadPreferences() {
  prefs.begin("meter", false);
  isoIdx = prefs.getInt("iso", 6);
  apertureIdx = prefs.getInt("aperture", 22);
  settingsBrightness = prefs.getInt("bright", 2);
  settingsBatStyle = prefs.getInt("batstyle", 0);
  settingsSleepIdx = prefs.getInt("sleep", 5); 
  settingsEvShift = prefs.getInt("evshift", 0); 
  setBrightness(settingsBrightness);
}

void savePreferences() {
  prefs.putInt("iso", isoIdx);
  prefs.putInt("aperture", apertureIdx);
  prefs.putInt("bright", settingsBrightness);
  prefs.putInt("batstyle", settingsBatStyle);
  prefs.putInt("sleep", settingsSleepIdx);
  prefs.putInt("evshift", settingsEvShift);
}

void goToSleep() {
  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  savePreferences();
  esp_deep_sleep_enable_gpio_wakeup((1ULL << PIN_BTN_MENU) | (1ULL << PIN_BTN_CHANGE), ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();
}

float getShutterTime(float val) {
  if (val < 0) return abs(val);
  return 1.0 / val;
}

int roundToTen(float val) {
  if (val < 10) return (int)val; 
  return (int)((val + 5.0) / 10.0) * 10;
}

String getEvShiftString() {
  if (settingsEvShift == 0) return "0.0";
  float val = settingsEvShift / 3.0;
  String s = (val > 0) ? "+" : "";
  s += String(val, 1);
  return s;
}



void clickMenu() {
  lastActivityTime = millis(); 
  if (currentState == STATE_MAIN) {
    if (selectedIndex == 1) selectedIndex = 0;      
    else if (selectedIndex == 0) selectedIndex = 2; 
    else selectedIndex = 1;                         
    calculateExposure(); 
    drawMainScreen();
  } else {
    menuIndex = (menuIndex + 1) % menuItemsCount;
    drawMenuScreen();
  }
}

void longPressMenu() {
  lastActivityTime = millis();
  if (currentState == STATE_MAIN) {
    currentState = STATE_MENU;
    menuIndex = 0;
    drawMenuScreen();
  } else {
    currentState = STATE_MAIN;
    savePreferences(); 
    calculateExposure();
    drawMainScreen();
  }
}

void clickChange() {
  lastActivityTime = millis();
  if (currentState == STATE_MAIN) {
    if (selectedIndex == 0) isoIdx = (isoIdx + 1) % isoCount;
    else if (selectedIndex == 1) { apertureIdx = (apertureIdx + 1) % fStopCount; lockMode = 1; }
    else if (selectedIndex == 2) { shutterIdx = (shutterIdx + 1) % shutterCount; lockMode = 2; }
    calculateExposure();
    drawMainScreen();
  } else {
    switch(menuIndex) {
      case 0: 
        settingsEvShift++;
        if (settingsEvShift > 9) settingsEvShift = -9;
        break;
      case 1: settingsBrightness = (settingsBrightness + 1) % 5; setBrightness(settingsBrightness); break;
      case 2: settingsBatStyle = (settingsBatStyle + 1) % 3; break;
      case 3: settingsSleepIdx = (settingsSleepIdx + 1) % sleepTimesCount; break;
      case 4: currentState = STATE_MAIN; savePreferences(); break;
    }
    drawMenuScreen();
  }
}

void setup() {
  Wire.begin(8, 9);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) for(;;);
  
  pinMode(PIN_BTN_MENU, INPUT_PULLUP);
  pinMode(PIN_BTN_CHANGE, INPUT_PULLUP);
  
  btnMenu.attachClick(clickMenu);
  btnMenu.attachLongPressStart(longPressMenu);
  btnMenu.setPressTicks(2000); 
  btnChange.attachClick(clickChange);

  loadPreferences();
  
  selectedIndex = 1; 
  lockMode = 1;      
  
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  display.setTextColor(SSD1306_WHITE);
  
  lastActivityTime = millis();
  lastLuxUpdate = millis();
}

void calculateExposure() {
  if (currentLux <= 0) return;
  float ev100 = log10(currentLux / 2.5) / log10(2.0);
  float shiftedEV100 = ev100 - (settingsEvShift / 3.0);
  currentEV = shiftedEV100 + (log10((float)isoValues[isoIdx] / 100.0) / log10(2.0));

  if (lockMode == 1) { 
    // Prior: Aperture
    float f = fStops[apertureIdx];
    float t_target = (f * f) / pow(2.0, currentEV);
    
    float minDiff = 1000000;
    int bestIdx = 0;
    for (int i = 0; i < shutterCount; i++) {
      float t_array = getShutterTime(shutterSpeeds[i]);
      float diff = abs(t_array - t_target);
      if (diff < minDiff) { minDiff = diff; bestIdx = i; }
    }
    shutterIdx = bestIdx;
  } else {
    // Prior: Shutter
    float t_seconds = getShutterTime(shutterSpeeds[shutterIdx]);
    float f_target = sqrt(t_seconds * pow(2.0, currentEV));
    
    float minDiff = 100;
    int bestIdx = 0;
    for (int i = 0; i < fStopCount; i++) {
      float diff = abs(fStops[i] - f_target);
      if (diff < minDiff) { minDiff = diff; bestIdx = i; }
    }
    apertureIdx = bestIdx;
  }
}

void drawBattery(int x, int y) {
  float sum = 0; 
  for(int i=0; i<3; i++) sum += analogRead(PIN_BATTERY);
  float voltage = (sum/3.0 * 3.3 / 4095.0) * dividerRatio;
  int percent = map(constrain(voltage * 100, 330, 420), 330, 420, 0, 100);
  
  if (settingsBatStyle == 1) {
    display.setCursor(x - 10, y); 
    display.print(percent); display.print("%");
  } else if (settingsBatStyle == 2) {
    display.setCursor(x - 18, y); 
    display.print(voltage, 2); display.print("V");
  } else {
    display.drawRect(x, y, 16, 8, SSD1306_WHITE);
    display.fillRect(x + 16, y + 2, 2, 4, SSD1306_WHITE);
    int w = map(percent, 0, 100, 0, 12);
    if(w > 0) display.fillRect(x+2, y+2, w, 4, SSD1306_WHITE);
  }
}

void drawMainScreen() {
  display.clearDisplay();


  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("lux"); display.print((int)currentLux);
  display.setCursor(60, 0);
  display.print("EV"); display.print((int)currentEV);
  
  drawBattery(110, 0);


  int yCenter = 20;
  int xPadding = 12;


  if(selectedIndex == 2) { display.setCursor(0, yCenter + 4); display.print(">"); }
  display.setTextSize(2);
  display.setCursor(xPadding, yCenter);
  display.print("T ");
  float sVal = shutterSpeeds[shutterIdx];
  if (sVal > 0) { display.print("1/"); display.print((int)sVal); }
  else { display.print(abs(sVal), 1); display.print("\""); }


  if(selectedIndex == 1) { display.setTextSize(1); display.setCursor(0, yCenter + 26); display.print(">"); }
  display.setTextSize(2);
  display.setCursor(xPadding, yCenter + 22);
  display.print("F ");
  display.print(fStops[apertureIdx], 1); 


  display.setTextSize(1);
  int isoVal = isoValues[isoIdx];
  

  int charCount = 4; if(isoVal<100) charCount+=2; else if(isoVal<1000) charCount+=3; else if(isoVal<10000) charCount+=4; else charCount+=5;
  int xPos = 128 - (charCount * 6);
  

  if (settingsEvShift != 0) {
    String shiftStr = getEvShiftString();

    int shiftX = 128 - (shiftStr.length() * 6);
    display.setCursor(shiftX, 46); 
    display.print(shiftStr);
  }


  display.setCursor(xPos, 56);
  if(selectedIndex == 0) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.print("ISO "); display.print(isoVal);
  display.setTextColor(SSD1306_WHITE);

  display.display();
}

void drawMenuScreen() {
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setCursor(20, 0);
  display.print("--- SETTINGS ---");
  
  int yBase = 15;
  int lineH = 10; 
  int textOffset = 12; 


  display.setCursor(0, yBase);
  if(menuIndex == 0) display.print(">"); 
  display.setCursor(textOffset, yBase);
  display.print("Calib EV: ");
  display.print(getEvShiftString());


  display.setCursor(0, yBase + lineH);
  if(menuIndex == 1) display.print(">"); 
  display.setCursor(textOffset, yBase + lineH);
  display.print("Bright: ");
  display.print(settingsBrightness + 1);


  display.setCursor(0, yBase + lineH*2);
  if(menuIndex == 2) display.print(">"); 
  display.setCursor(textOffset, yBase + lineH*2);
  display.print("Bat Ind: ");
  if(settingsBatStyle == 0) display.print("Icon");
  else if(settingsBatStyle == 1) display.print("%");
  else display.print("Volt");


  display.setCursor(0, yBase + lineH*3);
  if(menuIndex == 3) display.print(">"); 
  display.setCursor(textOffset, yBase + lineH*3);
  display.print("Sleep: ");
  display.print(sleepLabels[settingsSleepIdx]);


  display.setCursor(0, yBase + lineH*4);
  if(menuIndex == 4) display.print(">"); 
  display.setCursor(textOffset, yBase + lineH*4);
  display.print("EXIT / BACK");

  display.display();
}

void loop() {
  btnMenu.tick();
  btnChange.tick();
  
  unsigned long currentMillis = millis();

  long sleepDuration = sleepTimes[settingsSleepIdx];
  if (currentState == STATE_MAIN && sleepDuration > 0 && (currentMillis - lastActivityTime > sleepDuration)) {
    goToSleep();
  }
  
  if (currentState == STATE_MENU && (currentMillis - lastActivityTime > 15000)) {
    currentState = STATE_MAIN; 
    savePreferences();
    drawMainScreen();
  }

  static unsigned long lastSampleTime = 0;
  if (currentMillis - lastSampleTime > 100) { 
    float val = lightMeter.readLightLevel();
    if (val >= 0) {
      accumulatedLux += val;
      sampleCount++;
    }
    lastSampleTime = currentMillis;
  }

  if (currentState == STATE_MAIN) {
    if (currentMillis - lastLuxUpdate > 1000) {
      if (sampleCount > 0) {
        float avgLux = accumulatedLux / sampleCount;
        currentLux = roundToTen(avgLux);
        accumulatedLux = 0;
        sampleCount = 0;
        calculateExposure();
        drawMainScreen();
      }
      lastLuxUpdate = currentMillis;
    }
  } else {
    static unsigned long lastMenuUpdate = 0;
    if (currentMillis - lastMenuUpdate > 100) {
       drawMenuScreen();
       lastMenuUpdate = currentMillis;
    }
  }
}
