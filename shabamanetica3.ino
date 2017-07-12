#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <Encoder.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);  

#define KNOB_A 3
#define KNOB_B 2
#define KNOB_S 8
Encoder knob(KNOB_A, KNOB_B);

#define ENC_A A1
#define ENC_B A2
Encoder enc(ENC_A, ENC_B);

int num_rates = 1;
int rates[10];
float rot_per_rate[10];
byte randomize_order = 0;
byte randomize_duration = 0;
float randomize_min = 0;
float randomize_max = 2;
float transition_time = 1;
int bright_stop = 10;
int bright_run = 10;
float min_speed = 10;

long knob_pos = 0;
int last_button_state = 1;
int edit = 0;
int up = 0;
int down = 0;
int debug = 0;
char line_buffer[20];

void saveEEPROM() {
  EEPROM.put(0, num_rates);
  for (int i=0;i<10;i++) {
    EEPROM.put(2+i*2, rates[i]);
    EEPROM.put(22+i*4, rot_per_rate[i]);
  }
  EEPROM.put(62, randomize_order);
  EEPROM.put(63, randomize_duration);
  EEPROM.put(64, randomize_min);
  EEPROM.put(68, randomize_max);
  EEPROM.put(72, transition_time);
  EEPROM.put(76, bright_stop);
  EEPROM.put(78, bright_run);
  EEPROM.put(80, min_speed);
}

void loadEEPROM() {
  EEPROM.get(0, num_rates);
  for (int i=0;i<10;i++) {
    EEPROM.get(2+i*2, rates[i]);
    EEPROM.get(22+i*4, rot_per_rate[i]);
  }
  EEPROM.get(62, randomize_order);
  EEPROM.get(63, randomize_duration);
  EEPROM.get(64, randomize_min);
  EEPROM.get(68, randomize_max);
  EEPROM.get(72, transition_time);
  EEPROM.get(76, bright_stop);
  EEPROM.get(78, bright_run);
  EEPROM.get(80, min_speed);
}

typedef enum Mode {
  Enumrates,
  Erates,
  Erotperrate,
  Erandomizeorder,
  Erandomizeduration,
  Erandomizemin,
  Erandomizemax,
  Etransitiontime,
  Ebrightstop,
  Ebrightrun,
  Eminspeed
};
volatile Mode menu_mode = Enumrates;

void clearLCD() {
  for (int i=0;i<4;i++) {
    lcd.setCursor(0,i);
    lcd.print("                    ");
  }
}

void updateLCD() {
  lcd.home(); 
  lcd.noBlink();
  switch (menu_mode) {
    case Enumrates:
      sprintf(line_buffer, "Num rates: %02d", num_rates);
      lcd.print(line_buffer);
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          num_rates++;
        }
        if (down) {
          num_rates--;
        }
        num_rates = min(max(1, num_rates), 10);
      } else {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Erates;
        } else if (down) {
          menu_mode = Eminspeed;
        }
      }
      break;
    case Erates:
      sprintf(line_buffer, "Rate 1: %02d", num_rates);
      lcd.print(line_buffer);
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          num_rates++;
        }
        if (down) {
          num_rates--;
        }
        num_rates = min(max(1, num_rates), 10);
      } else {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Erotperrate;
        } else if (down) {
          menu_mode = Enumrates;
        }
      }
      break;
    case Erotperrate:
      sprintf(line_buffer, "Rev per rate: %02d", num_rates);
      lcd.print(line_buffer);
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          num_rates++;
        }
        if (down) {
          num_rates--;
        }
        num_rates = min(max(1, num_rates), 10);
      } else {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Erandomizeorder;
        } else if (down) {
          menu_mode = Erates;
        }
      }
      break;
    case Erandomizeorder:
      sprintf(line_buffer, "Random order: %02d", num_rates);
      lcd.print(line_buffer);
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          num_rates++;
        }
        if (down) {
          num_rates--;
        }
        num_rates = min(max(1, num_rates), 10);
      } else {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Erandomizeduration;
        } else if (down) {
          menu_mode = Erotperrate;
        }
      }
      break;
    case Erandomizeduration:
      sprintf(line_buffer, "Random Time: %02d", num_rates);
      lcd.print(line_buffer);
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          num_rates++;
        }
        if (down) {
          num_rates--;
        }
        num_rates = min(max(1, num_rates), 10);
      } else {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Erandomizemin;
        } else if (down) {
          menu_mode = Erandomizeorder;
        }
      }
      break;
    case Erandomizemin:
      sprintf(line_buffer, "Min Rand Time: %02d", num_rates);
      lcd.print(line_buffer);
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          num_rates++;
        }
        if (down) {
          num_rates--;
        }
        num_rates = min(max(1, num_rates), 10);
      } else {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Erandomizemax;
        } else if (down) {
          menu_mode = Erandomizeduration;
        }
      }
      break;
    case Erandomizemax:
      sprintf(line_buffer, "Max Rand Time: %02d", num_rates);
      lcd.print(line_buffer);
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          num_rates++;
        }
        if (down) {
          num_rates--;
        }
        num_rates = min(max(1, num_rates), 10);
      } else {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Etransitiontime;
        } else if (down) {
          menu_mode = Erandomizemin;
        }
      }
      break;
    case Etransitiontime:
      sprintf(line_buffer, "Transition Time: %02d", num_rates);
      lcd.print(line_buffer);
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          num_rates++;
        }
        if (down) {
          num_rates--;
        }
        num_rates = min(max(1, num_rates), 10);
      } else {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Ebrightstop;
        } else if (down) {
          menu_mode = Erandomizemax;
        }
      }
      break;
    case Ebrightstop:
      sprintf(line_buffer, "Idle Brightness: %02d", num_rates);
      lcd.print(line_buffer);
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          num_rates++;
        }
        if (down) {
          num_rates--;
        }
        num_rates = min(max(1, num_rates), 10);
      } else {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Ebrightrun;
        } else if (down) {
          menu_mode = Etransitiontime;
        }
      }
      break;
    case Ebrightrun:
      sprintf(line_buffer, "Brightness: %02d", num_rates);
      lcd.print(line_buffer);
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          num_rates++;
        }
        if (down) {
          num_rates--;
        }
        num_rates = min(max(1, num_rates), 10);
      } else {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Eminspeed;
        } else if (down) {
          menu_mode = Ebrightstop;
        }
      }
      break;
    case Eminspeed:
      sprintf(line_buffer, "Minimum Speed: %.1f", min_speed);
      lcd.print(line_buffer);
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          min_speed = min_speed + 0.1;
        }
        if (down) {
          min_speed = min_speed - 0.1;
        }
        min_speed = min(max(10, num_rates), 100);
      } else {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Enumrates;
        } else if (down) {
          menu_mode = Ebrightrun;
        }
      }
      break;
    default:
      break;
  }
  if ((up | down) & edit) {
    saveEEPROM();
  }
  up = false;
  down = false;
}

void setup() {
  pinMode(KNOB_S, INPUT_PULLUP);
  loadEEPROM();
  lcd.begin();
  updateLCD();
}

int refresh_timer = 0;

void loop() {
  refresh_timer = refresh_timer % 10;
  int button_state = digitalRead(KNOB_S);
  if (button_state != last_button_state) {
    if (digitalRead(KNOB_S)) {
      edit = !edit;
    }
  }
  last_button_state = button_state;

  long new_pos = knob.read();
  long diff = knob_pos - new_pos;
  if (diff > 3) {
    up = true;
    updateLCD();
    knob_pos = new_pos;
  } else if (diff < -3) {
    down = true;
    updateLCD();
    knob_pos = new_pos;
  }

  if ((up | down) & !edit) {
    clearLCD();
  }
  
  if (!refresh_timer) {
    updateLCD();
  }
  refresh_timer++;
  delay(1);
}
