#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Preferences.h>

// ==== OLED DISPLAY SETUP ====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==== JOYSTICK & BUTTON SETUP ====
#define JOYSTICK_X_PIN  34
#define JOYSTICK_Y_PIN  35
#define JOYSTICK_SW_PIN 27
#define button          5
#define led             2
#define toggleButton    4
#define buzzer          15 // Buzzer pin added

// ==== KEYBOARD LAYOUT ====
const int KEY_ROWS = 4;
const int KEY_COLS = 10;
const int KEY_WIDTH = 11;
const int KEY_HEIGHT = 11;
const int KEY_PADDING = 1;
const int START_X = 3;
const int START_Y = 18;
const char keyboard[KEY_ROWS][KEY_COLS] = {
    {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'},
    {'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P'},
    {'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '<'},
    {'Z', 'X', 'C', 'V', 'B', 'N', 'M', '-', '_', ' '}
};

// ==== VARIABLES ====
int joystickX = 0;
int joystickY = 0;
bool joystickBtn = false;
bool lastJoystickBtnState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

int cursorX = 0;
int cursorY = 0;
String typedText = "";
String userName = "User"; // Default username
String currentInput = "";
bool buzzerOn = false; // Variable to track buzzer state
bool ledOn = false; // New variable to track LED state
bool isScreenOn = true; // New variable to track screen state
bool sleepModeOn = true; // New variable to toggle screen sleep mode
bool colorMode = false; // false for Dark Mode, true for Light Mode
unsigned long lastActivityTime = 0; // New variable to track inactivity

// Define inactivity timeout (e.g., 30 seconds = 30000 ms)
const unsigned long INACTIVITY_TIMEOUT_MS = 30000;

// Calibration values (determined during splash screen)
int JOYSTICK_X_CENTER = 2048;
int JOYSTICK_Y_CENTER = 2048;
const int THRESHOLD = 1000;  // Minimum deviation from center to trigger movement

// ==== ENUM FOR SCREEN STATE ====
enum ScreenState {
  KEYBOARD_SCREEN,
  MESSAGES_SCREEN,
  PROFILE_SCREEN,
  SETTINGS_SCREEN,
  MESSAGE_LIMIT_SCREEN // New screen for changing message limit
};
ScreenState currentScreen = KEYBOARD_SCREEN;
int settingsCursorY = 0; // Cursor position for settings screen
int settingsScrollY = 0; // New variable for vertical scrolling

const int SETTINGS_OPTIONS_COUNT = 6; // Total number of settings options
const int VISIBLE_OPTIONS_COUNT = 4; // Number of options to display on the screen at once
int MAX_MESSAGES = 4; // Maximum number of messages to keep in history
String messageHistory[20]; // Array to store messages (size is max possible limit)
int messageCount = 0; // Current number of messages

// ==== ESP-NOW PEER ====
esp_now_peer_info_t peerInfo;

// ==== PREFERENCES OBJECT ====
Preferences preferences;

// ==== PREFERENCE FUNCTIONS ====
void saveUsername() {
  preferences.begin("user-data", false);
  preferences.putString("username", userName);
  preferences.end();
}

void loadUsername() {
  preferences.begin("user-data", true);
  String savedName = preferences.getString("username", "User");
  preferences.end();
  userName = savedName;
}

void saveBuzzerState() {
  preferences.begin("user-data", false);
  preferences.putBool("buzzer", buzzerOn);
  preferences.end();
}

void loadBuzzerState() {
  preferences.begin("user-data", true);
  buzzerOn = preferences.getBool("buzzer", false);
  preferences.end();
}

void saveSleepModeState() {
  preferences.begin("user-data", false);
  preferences.putBool("sleep_mode", sleepModeOn);
  preferences.end();
}

void loadSleepModeState() {
  preferences.begin("user-data", true);
  sleepModeOn = preferences.getBool("sleep_mode", true);
  preferences.end();
}

void saveLedState() {
  preferences.begin("user-data", false);
  preferences.putBool("led", ledOn);
  preferences.end();
}

void loadLedState() {
  preferences.begin("user-data", true);
  ledOn = preferences.getBool("led", false); // Default to OFF
  preferences.end();
}

void saveMessageLimit() {
  preferences.begin("user-data", false);
  preferences.putInt("msg_limit", MAX_MESSAGES);
  preferences.end();
}

void loadMessageLimit() {
  preferences.begin("user-data", true);
  MAX_MESSAGES = preferences.getInt("msg_limit", 4); // Default to 4
  preferences.end();
}

void saveColorMode() {
  preferences.begin("user-data", false);
  preferences.putBool("color_mode", colorMode);
  preferences.end();
}

void loadColorMode() {
  preferences.begin("user-data", true);
  colorMode = preferences.getBool("color_mode", false); // Default to Dark Mode
  preferences.end();
}

// ==== AUDIO FUNCTIONS ====
void beep() {
  if (buzzerOn) {
    digitalWrite(buzzer, HIGH);
    delay(50);
    digitalWrite(buzzer, LOW);
  }
}

// ==== DISPLAY FUNCTIONS ====
void drawTypedText(String textToDraw) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    int textPixelWidth = textToDraw.length() * 6;  // Approximate width per character
    int scrollOffset = 0;
    if (textPixelWidth > SCREEN_WIDTH) {
        scrollOffset = textPixelWidth - SCREEN_WIDTH;
    }

    display.setCursor(-scrollOffset, 0);  // Scroll left if needed
    display.print(textToDraw);
}

void drawKeyboardLayout() {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    for (int y = 0; y < KEY_ROWS; y++) {
        for (int x = 0; x < KEY_COLS; x++) {
            int keyX = START_X + x * (KEY_WIDTH + KEY_PADDING);
            int keyY = START_Y + y * (KEY_HEIGHT + KEY_PADDING);
            display.drawRect(keyX, keyY, KEY_WIDTH, KEY_HEIGHT, SSD1306_WHITE);
            display.setCursor(keyX + 3, keyY + 3);
            display.print(keyboard[y][x]);
        }
    }
}

void drawCursor() {
    int keyX = START_X + cursorX * (KEY_WIDTH + KEY_PADDING);
    int keyY = START_Y + cursorY * (KEY_HEIGHT + KEY_PADDING);
    display.fillRect(keyX, keyY, KEY_WIDTH, KEY_HEIGHT, SSD1306_INVERSE);
    display.setCursor(keyX + 3, keyY + 3);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.print(keyboard[cursorY][cursorX]);
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
}

void drawSettingsScreen() {
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("SETTINGS");
    display.drawFastHLine(0, 15, SCREEN_WIDTH, SSD1306_WHITE);

    display.setTextSize(1);
    // Array of settings options
    String settingsOptions[] = {
      "Edit Username",
      "Buzzer",
      "Sleep Mode",
      "LED",
      "Max Messages",
      "Light Mode"
    };

    // Array of settings statuses
    String settingsStatus[] = {
      userName,
      String(buzzerOn ? "ON" : "OFF"),
      String(sleepModeOn ? "ON" : "OFF"),
      String(ledOn ? "ON" : "OFF"),
      String(MAX_MESSAGES),
      String(colorMode ? "ON" : "OFF")
    };
    
    // Display only the visible options based on the scroll position
    for (int i = 0; i < VISIBLE_OPTIONS_COUNT; i++) {
        int optionIndex = settingsScrollY + i;
        if (optionIndex < 0 || optionIndex >= SETTINGS_OPTIONS_COUNT) {
            continue; // Skip if out of bounds
        }
        
        int optionY = 18 + i * 11;
        
        // Draw the cursor for the selected option
        if (settingsCursorY == optionIndex) {
            display.fillRect(0, optionY, SCREEN_WIDTH, 11, colorMode ? SSD1306_BLACK : SSD1306_WHITE);
            display.setTextColor(colorMode ? SSD1306_WHITE : SSD1306_BLACK);
            display.setCursor(5, optionY + 2);
            display.print(settingsOptions[optionIndex]);
            display.setCursor(95, optionY + 2);
            display.print(settingsStatus[optionIndex]);
        } else {
            display.fillRect(0, optionY, SCREEN_WIDTH, 11, colorMode ? SSD1306_WHITE : SSD1306_BLACK);
            display.setTextColor(colorMode ? SSD1306_WHITE : SSD1306_BLACK);
            display.setCursor(5, optionY + 2);
            display.print(settingsOptions[optionIndex]);
            display.setCursor(95, optionY + 2);
            display.print(settingsStatus[optionIndex]);
        }
    }
}

void drawMessagesScreen() {
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("INBOX");
    display.drawFastHLine(0, 15, SCREEN_WIDTH, SSD1306_WHITE);

    display.setTextSize(1);
    const int startY = 18;
    const int lineHeight = 10;
    
    // Display all messages in a simple list
    for (int i = 0; i < messageCount; i++) {
        int yPos = startY + (i * lineHeight); 
        display.setCursor(0, yPos);
        display.print(messageHistory[i]);
    }
}

void drawMessageLimitScreen() {
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("MSG LIMIT");
    display.drawFastHLine(0, 15, SCREEN_WIDTH, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("Current Limit:");
    
    display.setTextSize(3);
    display.setCursor(SCREEN_WIDTH/2 - 10, SCREEN_HEIGHT/2 - 10);
    display.print(MAX_MESSAGES);
    
    display.setTextSize(1);
    display.setCursor(0, 50);
    display.println("Use joystick to change");
    display.setCursor(0, 60);
    display.println("Press button to save");
}

void updateDisplay() {
    display.clearDisplay();
    display.invertDisplay(colorMode); // Invert the entire display based on the colorMode
    
    switch(currentScreen) {
        case KEYBOARD_SCREEN:
            drawTypedText(typedText);
            drawKeyboardLayout();
            drawCursor();
            break;
        case MESSAGES_SCREEN:
            drawMessagesScreen();
            break;
        case PROFILE_SCREEN:
            drawTypedText(currentInput);
            drawKeyboardLayout();
            drawCursor();
            break;
        case SETTINGS_SCREEN:
            drawSettingsScreen();
            break;
        case MESSAGE_LIMIT_SCREEN:
            drawMessageLimitScreen();
            break;
    }
    display.display();
}

void moveCursor(int dx, int dy) {
    cursorX = (cursorX + dx + KEY_COLS) % KEY_COLS;
    cursorY = (cursorY + dy + KEY_ROWS) % KEY_ROWS;
}

void selectCharacter() {
    char selectedChar = keyboard[cursorY][cursorX];
    if (selectedChar == '<') {
        if (currentInput.length() > 0) {
            currentInput.remove(currentInput.length() - 1);
        }
    } else {
        if (currentInput.length() < 100) {
            currentInput += selectedChar;
        }
    }
    if (currentScreen == KEYBOARD_SCREEN) {
      typedText = currentInput;
    }
}

// ==== SPLASH SCREEN WITH CALIBRATION ====
void showSplashAndCalibrate() {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(4, 10);
    display.println("S.Y.N.C");
    display.setTextSize(1);
    display.setCursor(90, 18);
    display.println("one");
    display.setCursor(5, 35);
    display.println("Hold joystick still");
    display.setCursor(5, 45);
    display.println("Calibrating...");
    display.display();
    delay(1500);

    long xSum = 0, ySum = 0;
    const int samples = 10;
    for (int i = 0; i < samples; i++) {
        xSum += analogRead(JOYSTICK_X_PIN);
        ySum += analogRead(JOYSTICK_Y_PIN);
        delay(50);
    }

    JOYSTICK_X_CENTER = xSum / samples;
    JOYSTICK_Y_CENTER = ySum / samples;

    display.clearDisplay();
    display.setCursor(30, 25);
    display.setTextSize(2);
    display.println("Done!");
    display.setTextSize(1);    
    display.display();
    delay(1000);
}

// ==== SETUP ====
void setup() {
    Serial.begin(115200);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        while (1);
    }

    showSplashAndCalibrate();
    loadUsername(); // Load the saved username at startup
    loadBuzzerState(); // Load the saved buzzer state
    loadSleepModeState(); // Load the saved sleep mode state
    loadLedState(); // Load the saved LED state
    loadMessageLimit(); // Load the saved message limit
    loadColorMode(); // Load the saved color mode

    pinMode(JOYSTICK_SW_PIN, INPUT_PULLUP);
    pinMode(button, INPUT_PULLUP);
    pinMode(toggleButton, INPUT_PULLUP);
    pinMode(led, OUTPUT);
    pinMode(buzzer, OUTPUT); // Buzzer pin as output
    digitalWrite(buzzer, LOW); // Ensure buzzer is off at startup
    digitalWrite(led, ledOn ? HIGH : LOW); // Set the LED based on the saved state

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return;
    }

    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }

    esp_now_register_recv_cb(onDataRecv);
    
    // Set the initial activity time
    lastActivityTime = millis();
}

// ==== MAIN LOOP ====
void loop() {
  // Check for inactivity and turn off the screen
  if (sleepModeOn && isScreenOn && (millis() - lastActivityTime > INACTIVITY_TIMEOUT_MS)) {
    Serial.println("Inactivity detected, turning screen off.");
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    isScreenOn = false;
  }
  
  // Joystick Button (debounced)
  bool currentJoystickBtn = (digitalRead(JOYSTICK_SW_PIN) == LOW);
  if (currentJoystickBtn != lastJoystickBtnState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (currentJoystickBtn != joystickBtn) {
      joystickBtn = currentJoystickBtn;
      if (joystickBtn) {
        // If the screen is off, turn it on and then process the action
        if (!isScreenOn) {
          display.ssd1306_command(SSD1306_DISPLAYON);
          isScreenOn = true;
        }
        lastActivityTime = millis(); // Update activity time
        if (currentScreen == KEYBOARD_SCREEN || currentScreen == PROFILE_SCREEN) {
          selectCharacter();
        } else if (currentScreen == SETTINGS_SCREEN) {
          // Process selection in settings menu
          if (settingsCursorY == 0) { // Edit Username selected
            currentScreen = PROFILE_SCREEN;
            currentInput = userName;
          } else if (settingsCursorY == 1) { // Toggle Buzzer selected
            buzzerOn = !buzzerOn;
            if (buzzerOn) {
              beep();
            } else {
              digitalWrite(buzzer, LOW);
            }
            saveBuzzerState();
          } else if (settingsCursorY == 2) { // Toggle Sleep Mode selected
            sleepModeOn = !sleepModeOn;
            saveSleepModeState();
          } else if (settingsCursorY == 3) { // Toggle LED selected
            ledOn = !ledOn;
            if (ledOn) {
                digitalWrite(led, HIGH);
                delay(150);
                digitalWrite(led, LOW);
            } else {
                digitalWrite(led, LOW);
            }
            saveLedState();
          } else if (settingsCursorY == 4) { // Change Max Messages
            currentScreen = MESSAGE_LIMIT_SCREEN;
          } else if (settingsCursorY == 5) { // Toggle Light/Dark Mode
            colorMode = !colorMode;
            saveColorMode();
          }
        } else if (currentScreen == MESSAGE_LIMIT_SCREEN) {
            saveMessageLimit();
            currentScreen = SETTINGS_SCREEN;
        }
      }
    }
  }
  lastJoystickBtnState = currentJoystickBtn;
  
  // Read joystick values
  joystickX = analogRead(JOYSTICK_X_PIN);
  joystickY = analogRead(JOYSTICK_Y_PIN);

  // Joystick movement handling based on current screen
  if (currentScreen == KEYBOARD_SCREEN || currentScreen == PROFILE_SCREEN) {
    if (joystickX > JOYSTICK_X_CENTER + THRESHOLD || joystickX < JOYSTICK_X_CENTER - THRESHOLD || joystickY > JOYSTICK_Y_CENTER + THRESHOLD || joystickY < JOYSTICK_Y_CENTER - THRESHOLD) {
        if (!isScreenOn) {
          display.ssd1306_command(SSD1306_DISPLAYON);
          isScreenOn = true;
        }
        lastActivityTime = millis();
        if (joystickX > JOYSTICK_X_CENTER + THRESHOLD) {
            moveCursor(1, 0);
        } else if (joystickX < JOYSTICK_X_CENTER - THRESHOLD) {
            moveCursor(-1, 0);
        }
        if (joystickY > JOYSTICK_Y_CENTER + THRESHOLD) {
            moveCursor(0, 1);
        } else if (joystickY < JOYSTICK_Y_CENTER - THRESHOLD) {
            moveCursor(0, -1);
        }
        delay(150);
    }
  } else if (currentScreen == SETTINGS_SCREEN) {
    if (joystickY > JOYSTICK_Y_CENTER + THRESHOLD) {
        if (!isScreenOn) {
          display.ssd1306_command(SSD1306_DISPLAYON);
          isScreenOn = true;
        }
        lastActivityTime = millis();
        settingsCursorY++;
        if (settingsCursorY >= SETTINGS_OPTIONS_COUNT) {
            settingsCursorY = 0; // Wrap around to the top
        }
        // Update scroll position if the cursor moves off-screen
        if (settingsCursorY >= settingsScrollY + VISIBLE_OPTIONS_COUNT) {
            settingsScrollY++;
        }
        delay(200);
    } else if (joystickY < JOYSTICK_Y_CENTER - THRESHOLD) {
        if (!isScreenOn) {
          display.ssd1306_command(SSD1306_DISPLAYON);
          isScreenOn = true;
        }
        lastActivityTime = millis();
        settingsCursorY--;
        if (settingsCursorY < 0) {
            settingsCursorY = SETTINGS_OPTIONS_COUNT - 1; // Wrap around to the bottom
        }
        // Update scroll position if the cursor moves off-screen
        if (settingsCursorY < settingsScrollY) {
            settingsScrollY--;
        }
        delay(200);
    }
  } else if (currentScreen == MESSAGE_LIMIT_SCREEN) {
      if (joystickY > JOYSTICK_Y_CENTER + THRESHOLD) {
          if (!isScreenOn) {
            display.ssd1306_command(SSD1306_DISPLAYON);
            isScreenOn = true;
          }
          lastActivityTime = millis();
          MAX_MESSAGES++;
          if (MAX_MESSAGES > 20) MAX_MESSAGES = 20; // Cap at 20 to avoid memory issues
          delay(200);
      } else if (joystickY < JOYSTICK_Y_CENTER - THRESHOLD) {
          if (!isScreenOn) {
            display.ssd1306_command(SSD1306_DISPLAYON);
            isScreenOn = true;
          }
          lastActivityTime = millis();
          MAX_MESSAGES--;
          if (MAX_MESSAGES < 1) MAX_MESSAGES = 1; // Minimum limit is 1
          delay(200);
      }
  }

  // Toggle between screens
  if (digitalRead(toggleButton) == LOW) {
    if (!isScreenOn) {
      display.ssd1306_command(SSD1306_DISPLAYON);
      isScreenOn = true;
    }
    lastActivityTime = millis();
    beep(); // Beep when screen is switched
    switch (currentScreen) {
      case KEYBOARD_SCREEN:
        currentScreen = MESSAGES_SCREEN;
        break;
      case MESSAGES_SCREEN:
        currentScreen = SETTINGS_SCREEN;
        break;
      case SETTINGS_SCREEN:
        currentScreen = KEYBOARD_SCREEN;
        break;
      case PROFILE_SCREEN:
        currentScreen = SETTINGS_SCREEN;
        break;
      case MESSAGE_LIMIT_SCREEN:
        currentScreen = SETTINGS_SCREEN;
        break;
    }
    delay(300);
  }

  // Send message or perform action based on the 'button'
  if (digitalRead(button) == LOW) {
    if (!isScreenOn) {
      display.ssd1306_command(SSD1306_DISPLAYON);
      isScreenOn = true;
    }
    lastActivityTime = millis();
    if (currentScreen == KEYBOARD_SCREEN) {
      if (typedText.length() > 0) {
        String fullMessage = "From " + userName + ": " + typedText;
        Serial.println("Sending: " + fullMessage);
        esp_now_send(peerInfo.peer_addr, (uint8_t *)fullMessage.c_str(), fullMessage.length());
        if (ledOn) {
            digitalWrite(led, HIGH);
        }
        beep(); // Beep when message is sent
        delay(500);
        digitalWrite(led, LOW);
        typedText = "";
        currentInput = "";
      }
    } else if (currentScreen == PROFILE_SCREEN) {
      userName = currentInput;
      saveUsername(); 
      currentScreen = SETTINGS_SCREEN; // Return to settings after saving
      currentInput = "";
    } else if (currentScreen == MESSAGE_LIMIT_SCREEN) {
      saveMessageLimit();
      currentScreen = SETTINGS_SCREEN;
    }
    delay(300); // Debounce delay for the main button
  }

  // Update the display only if the screen is on
  if (isScreenOn) {
    updateDisplay();
  }
}

// ==== ESP-NOW RECEIVER CALLBACK ====
void onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  // If the screen is off, turn it on and switch to messages screen
  if (!isScreenOn) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    isScreenOn = true;
    currentScreen = MESSAGES_SCREEN; // Launch messages screen
  }
  lastActivityTime = millis();
  
  if (ledOn) {
    digitalWrite(led, HIGH);
    beep(); // Beep when message is received
    delay(500);
    digitalWrite(led, LOW);
  } else {
    beep();
  }

  String message = String((char*)incomingData);
  // Add new message to history
  if (messageCount < MAX_MESSAGES) {
      messageHistory[messageCount] = message;
      messageCount++;
  } else {
      // Shift messages to the left
      for (int i = 0; i < MAX_MESSAGES - 1; i++) {
          messageHistory[i] = messageHistory[i + 1];
      }
      messageHistory[MAX_MESSAGES - 1] = message; // Add the new message at the end
  }
  Serial.println("Received message: " + message);
}
