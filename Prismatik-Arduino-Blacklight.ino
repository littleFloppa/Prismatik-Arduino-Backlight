#include "FastLED.h"

#define NUM_LEDS 113
#define DATA_PIN 6
#define BUTTON_PIN1 10
#define BUTTON_PIN2 11
#define POT_PIN 5  // Pin for the potentiometer

// Baudrate for serial communication
#define serialRate 256000

// Define the states
enum Mode {
  ADALIGHT_MODE,
  SOLID_BLUE_MODE,
  SOLID_GREEN_MODE,
  SOLID_RED_MODE,
  RAINBOW_CYCLE_MODE,
  BLUE_TO_RED_GRADIENT_MODE // Added blue-to-red gradient mode
};

// Adalight protocol
uint8_t prefix[] = {'A', 'd', 'a'}, hi, lo, chk, i;

// Initialize LED array
CRGB leds[NUM_LEDS];

Mode currentState = ADALIGHT_MODE; // Start at ADALIGHT_MODE

// Blue to Red Gradient variables
float ratio = 0.0;
bool isReversing = false;
int colorChangeDelay = 20; // Speed of transition
int colorA[3] = {0, 0, 255};  // Blue
int colorB[3] = {255, 0, 0};  // Red

// Heartbeat variables
unsigned long lastHeartbeatTime = 0;
#define HEARTBEAT_INTERVAL 1000

void setup() {
  // Set up LED strip
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

  // Initial RGB flash sequence
  FastLED.showColor(CRGB(255, 0, 0));
  delay(500);
  FastLED.showColor(CRGB(0, 255, 0));
  delay(500);
  FastLED.showColor(CRGB(0, 0, 255));
  delay(500);
  FastLED.showColor(CRGB(0, 0, 0));

  Serial.begin(serialRate);

  // Set up button pins
  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
}

void loop() {
  // Read potentiometer value (0 to 1023)
  int potValue = analogRead(POT_PIN);

  // Map the potentiometer value to a brightness value (0 to 255)
  int brightness = map(potValue, 0, 1023, 0, 255);

  // Set the global brightness for the LED strip
  FastLED.setBrightness(brightness);

  int forwardButtonState = digitalRead(BUTTON_PIN1);
  int backwardButtonState = digitalRead(BUTTON_PIN2);

  // Debugging button states
  Serial.println(forwardButtonState);
  Serial.println(backwardButtonState);

  // Check button inputs to switch modes
  if (forwardButtonState == LOW) {
    currentState = static_cast<Mode>((static_cast<int>(currentState) + 1) % (BLUE_TO_RED_GRADIENT_MODE + 1));
    delay(300); // Debounce delay
  }

  if (backwardButtonState == HIGH) {
    currentState = static_cast<Mode>((static_cast<int>(currentState) - 1 + (BLUE_TO_RED_GRADIENT_MODE + 1)) % (BLUE_TO_RED_GRADIENT_MODE + 1));
    delay(300); // Debounce delay
  }

  // Execute the current mode
  switch (currentState) {
    case ADALIGHT_MODE:
      runAdalightMode();
      break;

    case SOLID_BLUE_MODE:
      runSolidBlueMode();
      break;

    case SOLID_GREEN_MODE:
      runSolidGreenMode();
      break;

    case SOLID_RED_MODE:
      runSolidRedMode();
      break;

    case RAINBOW_CYCLE_MODE:
      runRainbowCycleMode();
      break;

    case BLUE_TO_RED_GRADIENT_MODE:
      runBlueToRedGradientMode(); // Call the new blue-to-red gradient mode
      break;
  }

  // Check if it's time to send a heartbeat
  unsigned long currentTime = millis();
  if (currentTime - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
    sendHeartbeat(); // Send heartbeat signal to keep communication alive
    lastHeartbeatTime = currentTime; // Update the last heartbeat time
  }
}

void sendHeartbeat() {
  Serial.write(0x00); // Send a dummy byte (heartbeat signal) to keep communication alive
  Serial.flush(); // Ensure the data is transmitted immediately
}

void runAdalightMode() {
  // Wait for the first byte of the Magic Word
  for (i = 0; i < sizeof prefix; ++i) {
    waitLoop:
    while (!Serial.available());
    if (prefix[i] == Serial.read()) continue;
    i = 0;
    goto waitLoop;
  }

  // Hi, Lo, Checksum
  while (!Serial.available());
  hi = Serial.read();
  while (!Serial.available());
  lo = Serial.read();
  while (!Serial.available());
  chk = Serial.read();

  if (chk != (hi ^ lo ^ 0x55)) {
    i = 0;
    goto waitLoop;
  }

  memset(leds, 0, NUM_LEDS * sizeof(struct CRGB));
  
  // Read the pixel data
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    byte r, g, b;
    while (!Serial.available());
    r = Serial.read();
    while (!Serial.available());
    g = Serial.read();
    while (!Serial.available());
    b = Serial.read();
    leds[i].r = r;
    leds[i].g = g;
    leds[i].b = b;
  }

  // Update LEDs with new data
  FastLED.show();
}

void runSolidBlueMode() {
  // Set all LEDs to solid blue
  FastLED.showColor(CRGB(0, 0, 255));
}

void runSolidGreenMode() {
  // Set all LEDs to solid green
  FastLED.showColor(CRGB(0, 255, 0));
}

void runSolidRedMode() {
  // Set all LEDs to solid red
  FastLED.showColor(CRGB(255, 0, 0));
}

void runRainbowCycleMode() {
  // Rainbow cycle mode: slowly cycle through the colors of the rainbow
  static uint8_t hue = 0; // Start at red
  
  // Update the color of each LED
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(hue + (i * 255 / NUM_LEDS), 255, 255);
  }

  // Increment hue to slowly transition the colors
  hue++;

  // Show the updated LEDs
  FastLED.show();

  // Slow down the color transition
  delay(20);
}

void runBlueToRedGradientMode() {
  // Calculate color transition based on ratio
  ratio += (isReversing ? -0.02 : 0.02); // Increment or decrement ratio based on direction
  if (ratio <= 0.0) {
    ratio = 0.0; // Prevent underflow
    isReversing = false; // Change direction to forward
  } else if (ratio >= 1.0) {
    ratio = 1.0; // Prevent overflow
    isReversing = true; // Change direction to reverse
  }

  // Calculate the color components based on the ratio
  int r = colorA[0] + (colorB[0] - colorA[0]) * ratio;
  int g = colorA[1] + (colorB[1] - colorA[1]) * ratio;
  int b = colorA[2] + (colorB[2] - colorA[2]) * ratio;

  // Update the strip with the calculated color
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(r, g, b); // Set each LED to the calculated color
  }
  FastLED.show(); // Update the strip to reflect changes

  // Add a delay for the color cycling speed
  delay(50); // Control speed of color transition
}