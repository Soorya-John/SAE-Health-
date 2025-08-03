#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <MAX30105.h>                     // For heart rate / SpO2 sensor
#include <Adafruit_ADXL345_U.h>           // For 3-axis accelerometer

// === OLED Display Setup ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// === Sensors ===
MAX30105 particleSensor;                  // MAX30105 IR sensor
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);  // Accelerometer

// === Pins ===
#define TEMP_SENSOR_PIN A0
#define BATTERY_VOLTAGE_PIN A1
#define BUZZER_PIN 9
#define BUTTON_UP_PIN 2
#define BUTTON_SELECT_PIN 3

// === State Variables ===
int menuIndex = 0;                        // Current menu selected
int totalMenus = 4;                       // Total number of menu items
int vitalsPage = 0;                       // Toggle between vitals graph and values

float prevAccelMag = 0;                   // Previous acceleration magnitude
int stepCount = 0;                        // Number of detected steps
unsigned long lastStepTime = 0;           // Timestamp of last detected step
unsigned long lastInteraction = 0;        // Last time a button was pressed
bool isSleeping = false;                  // Whether display is off/sleeping

// === IR Graph Buffer ===
#define GRAPH_WIDTH 128
#define GRAPH_HEIGHT 30
uint8_t irBuffer[GRAPH_WIDTH] = {0};      // Stores recent IR values for pulse graph
int graphIndex = 0;                       // Index for circular graph buffer

// === Menu Items ===
const char* menuItems[] = {"Vitals", "Steps", "Battery", "About"};

void setup() {
  Serial.begin(115200);

  // Setup pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);

  // Initialize OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  // Initialize MAX3010x sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD))
    showError("MAX3010x not found");
  particleSensor.setup();

  // Initialize accelerometer
  if (!accel.begin())
    showError("ADXL345 not found");
  accel.setRange(ADXL345_RANGE_2_G);

  splashScreen();  // Show boot screen
}

void loop() {
  handleSleep();  // Manage display sleep

  if (isSleeping) {
    // Wake up on any button press
    if (buttonPressed(BUTTON_UP_PIN) || buttonPressed(BUTTON_SELECT_PIN)) {
      wakeUp();
    }
    delay(200);
    return;
  }

  handleMenuNavigation();  // Handle menu navigation and page toggling

  // Render current menu
  switch (menuIndex) {
    case 0: showVitals(); break;
    case 1: showSteps(); break;
    case 2: showBattery(); break;
    case 3: showAbout(); break;
  }

  delay(200);
}

// === Menu and Button Handling ===
void handleMenuNavigation() {
  if (buttonPressed(BUTTON_UP_PIN)) {
    menuIndex = (menuIndex + 1) % totalMenus;  // Cycle through menu items
    tone(BUZZER_PIN, 1000, 50);  // Feedback tone
    resetSleepTimer();
  }

  // If on Vitals menu and SELECT is pressed, toggle between graph and data
  if (menuIndex == 0 && buttonPressed(BUTTON_SELECT_PIN)) {
    vitalsPage = (vitalsPage + 1) % 2;
    tone(BUZZER_PIN, 1200, 50);
    resetSleepTimer();
  }
}

// Detect falling edge button press
bool buttonPressed(int pin) {
  static bool prev[10] = {true}; // Debounce: store previous states
  bool current = digitalRead(pin);
  bool state = (!current && prev[pin]);  // Detect falling edge
  prev[pin] = current;
  return state;
}

// Reset inactivity timer
void resetSleepTimer() {
  lastInteraction = millis();
}

// Turn off screen after 30 seconds of inactivity
void handleSleep() {
  if (millis() - lastInteraction > 30000 && !isSleeping) {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    isSleeping = true;
    tone(BUZZER_PIN, 200, 50);
  }
}

// Wake display on button press
void wakeUp() {
  display.ssd1306_command(SSD1306_DISPLAYON);
  isSleeping = false;
  resetSleepTimer();
  tone(BUZZER_PIN, 800, 100);
}

// === Vitals Page ===
void showVitals() {
  long ir = particleSensor.getIR();  // IR value from MAX3010x
  float tempC = (analogRead(TEMP_SENSOR_PIN) * 3.3 / 1023.0 - 0.5) * 100;

  // Store IR value for graph
  irBuffer[graphIndex] = map(ir, 5000, 100000, 0, GRAPH_HEIGHT);
  graphIndex = (graphIndex + 1) % GRAPH_WIDTH;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("> Vitals");

  if (vitalsPage == 0) {
    // Plot IR waveform graph
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
    // Show raw IR and temperature
    display.setCursor(0, 16);
    display.print("IR: "); display.println(ir);
    display.print("Temp: "); display.print(tempC); display.println(" C");
  }

  display.display();
}

// === Steps Page ===
void showSteps() {
  sensors_event_t event;
  accel.getEvent(&event);

  // Calculate total acceleration magnitude
  float mag = sqrt(pow(event.acceleration.x, 2) +
                   pow(event.acceleration.y, 2) +
                   pow(event.acceleration.z, 2));

  // Simple step detection based on peak threshold and debounce
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

// === Battery Page ===
void showBattery() {
  int raw = analogRead(BATTERY_VOLTAGE_PIN);
  float vBat = raw * 3.3 * 2.0 / 1023.0;  // Assuming 1:1 voltage divider

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println("> Battery");
  display.setTextSize(2);
  display.print(vBat, 2);
  display.println(" V");
  display.display();
}

// === About Page ===
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

// === Splash Screen ===
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

// === Error Display and Halt ===
void showError(const char* msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("ERROR:");
  display.println(msg);
  display.display();
  while (true); // Halt execution
}
