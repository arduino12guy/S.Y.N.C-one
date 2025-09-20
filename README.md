# S.Y.N.C-one
A pocket sized device with which you communicate with other using ESP-NOW protocol.
# S.Y.N.C. (Social Yielded Network Communicator)
# connections
| esp32 | oled |
| --- | ---- |
| GND | GND |
| 3.3v | VCC |
| D22 | SCL |
| D21 | SDA |

| esp32 | led |
| --- | ---- |
| GND | GND |
| D2 | out |

| esp32 | button |
| --- | ---- |
| GND | GND |
| D5 | out |

| esp32 | button |
| --- | ---- |
| GND | GND |
| D4 | out |

| esp32 | buzzer |
| --- | ---- |
| GND | GND |
| D15 | out |

| esp32 | joystick |
| --- | ---- |
| GND | GND |
| 3.3v | VCC |
| D35 | X-axis |
| D34 | Y-axys |

>[!Note]
>connecting led at pin 2 is not necessary since built-in led is already connected to it.

# Preview (screen)
currently there are **3** main screens and **3** supporting screens.
## Loading screen
