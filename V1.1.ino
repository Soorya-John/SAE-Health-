#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <MAX30105.h>
#include <Adafruit_ADXL345_U.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

MAX30105 particleSensor;
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// === Pins ===
#define TEMP_SENSOR_PIN A0
#define BATTERY_VOLTAGE_PIN A1
#define BUZZER_PIN 9
#define BUTTON_UP_PIN 2
#define BUTTON_SELECT_PIN 3

// === States ===
int menuIndex = 0;
int totalMenus = 4;
int vitalsPage = 0;

float prevAccelMag = 0;
int stepCount = 0;
unsigned long lastStepTime = 0;
unsigned long lastInteraction = 0;
bool isSleeping = false;

// === Graph Buffer ===
#define GRAPH_WIDTH 128
#define GRAPH_HEIGHT 30
uint8_t irBuffer[GRAPH_WIDTH] = {0};
int graphIndex = 0;

// === Menu Items ===
const char* menuItems[] = {"Vitals", "Steps", "Battery", "About"};

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) showError("MAX3010x not found");
  particleSensor.setup();

  if (!accel.begin()) showError("ADXL345 not found");
  accel.setRange(ADXL345_RANGE_2_G);

  splashScreen();
}

void loop() {
  handleSleep();

  if (isSleeping) {
    if (buttonPressed(BUTTON_UP_PIN) || buttonPressed(BUTTON_SELECT_PIN)) {
      wakeUp();
    }
    delay(200);
    return;
  }

  handleMenuNavigation();

  switch (menuIndex) {
    case 0: showVitals(); break;
    case 1: showSteps(); break;
    case 2: showBattery(); break;
    case 3: showAbout(); break;
  }

  delay(200);
}

// === Menu Navigation ===
void handleMenuNavigation() {
  if (buttonPressed(BUTTON_UP_PIN)) {
    menuIndex = (menuIndex + 1) % totalMenus;
    tone(BUZZER_PIN, 1000, 50);
    resetSleepTimer();
  }

  if (menuIndex == 0 && buttonPressed(BUTTON_SELECT_PIN)) {
    vitalsPage = (vitalsPage + 1) % 2;
    tone(BUZZER_PIN, 1200, 50);
    resetSleepTimer();
  }
}

bool buttonPressed(int pin) {
  static bool prev[10] = {true}; // indexed by pin
  bool current = digitalRead(pin);
  bool state = (!current && prev[pin]);
  prev[pin] = current;
  return state;
}

void resetSleepTimer() {
  lastInteraction = millis();
}

void handleSleep() {
  if (millis() - lastInteraction > 30000 && !isSleeping) {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    isSleeping = true;
    tone(BUZZER_PIN, 200, 50);
  }
}

void wakeUp() {
  display.ssd1306_command(SSD1306_DISPLAYON);
  isSleeping = false;
  resetSleepTimer();
  tone(BUZZER_PIN, 800, 100);
}

// === Vitals with graph ===
void showVitals() {
  long ir = particleSensor.getIR();
  float tempC = (analogRead(TEMP_SENSOR_PIN) * 3.3 / 1023.0 - 0.5) * 100;

  irBuffer[graphIndex] = map(ir, 5000, 100000, 0, GRAPH_HEIGHT);
  graphIndex = (graphIndex + 1) % GRAPH_WIDTH;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("> Vitals");

  if (vitalsPage == 0) {
    display.setCursor(0, 10);
    display.println("Pulse (IR) Wave:");
    for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
      int x1 = i;
      int y1 = SCREEN_HEIGHT - irBuffer[(graphIndex + i) % GRAPH_WIDTH] - 1;
      int x2 = i + 1;
      int y2 = SCREEN_HEIGHT - irBuffer[(graphIndex + i + 1) % GRAPH_WIDTH] - 1;
      display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
    }
  } else {
    display.setCursor(0, 16);
    display.print("IR: "); display.println(ir);
    display.print("Temp: "); display.print(tempC); display.println(" C");
  }

  display.display();
}

// === Steps ===
void showSteps() {
  sensors_event_t event;
  accel.getEvent(&event);
  float mag = sqrt(pow(event.acceleration.x, 2) +
                   pow(event.acceleration.y, 2) +
                   pow(event.acceleration.z, 2));

  if (abs(mag - prevAccelMag) > 1.2 && millis() - lastStepTime > 300) {
    stepCount++;
    lastStepTime = millis();
    tone(BUZZER_PIN, 800, 20);
  }
  prevAccelMag = mag;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println("> Steps");
  display.setTextSize(2);
  display.print("Steps: ");
  display.println(stepCount);
  display.display();
}

// === Battery ===
void showBattery() {
  int raw = analogRead(BATTERY_VOLTAGE_PIN);
  float vBat = raw * 3.3 * 2.0 / 1023.0;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println("> Battery");
  display.setTextSize(2);
  display.print(vBat, 2);
  display.println(" V");
  display.display();
}

// === About ===
void showAbout() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("> About");
  display.println("HealthTracker v2.0");
  display.println("RP2040 | MAX30102");
  display.println("Temp, Steps, Batt");
  display.display();
}

// === UI Helpers ===
void splashScreen() {
  display.clearDisplay();
  display.setCursor(15, 20);
  display.setTextSize(2);
  display.println("HealthTrack");
  display.setTextSize(1);
  display.setCursor(35, 50);
  display.println("Initializing...");
  display.display();
  delay(2000);
}

void showError(const char* msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("ERROR:");
  display.println(msg);
  display.display();
  while (true);
}
