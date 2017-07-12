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
int submenu = 0;
int inmenu = 0;

int current_rate = 0;
int current_step_count = 0;
float current_pos = 0.0;

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

void validateState() {
  num_rates = min(max(num_rates, 1), 10);
  for (int i=0;i<10;i++) {
    rates[i] = min(max(rates[i], 1), 500);
    rot_per_rate[i] = min(max(rot_per_rate[i], 1.0), 500.0);
  }
  transition_time = min(max(transition_time, 0.0), 10.0);
  bright_stop = min(max(bright_stop, 0), 100);
  bright_run = min(max(bright_run, 0), 100);
  min_speed = min(max(min_speed, 10.0), 40.0);
  randomize_min = max(randomize_min, 0.0);
  randomize_max = min(randomize_max, 100.0);
  randomize_min = min(randomize_min, randomize_max);
  randomize_max = max(randomize_min, randomize_max);
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
  validateState();
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


char *ftoa(char *a, double f, int precision)
{
 long p[] = {0,10,100,1000,10000,100000,1000000,10000000,100000000};
 
 char *ret = a;
 long heiltal = (long)f;
 itoa(heiltal, a, 10);
 while (*a != '\0') a++;
 *a++ = '.';
 long desimal = abs((long)((f - heiltal) * p[precision]));
 itoa(desimal, a, 10);
 return ret;
}

void clearLCD() {
  for (int i=0;i<4;i++) {
    lcd.setCursor(0,i);
    lcd.print("                    ");
  }
}

void updateLCD() {
  int old_menu = menu_mode;
  int old_submenu = submenu;
  lcd.home(); 
  lcd.noBlink();
  if (!inmenu) {
    up = false;
    down = false;
    lcd.print("System Status:");
    lcd.setCursor(0, 1);
    lcd.print("NORMAL");
    lcd.setCursor(0, 2);
    sprintf(line_buffer, "%08d", enc.read());
    lcd.print(line_buffer);
    return;
  }
  if ((up | down) & !edit) {
    clearLCD();
  }
  switch (menu_mode) {
    case Enumrates:
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          num_rates++;
        }
        if (down) {
          num_rates--;
        }
        if ((up | down) & edit) {
          validateState();
          saveEEPROM();
        }
        lcd.setCursor(0, 1);
        sprintf(line_buffer, "%02d", num_rates);
        lcd.print(line_buffer);
      } else if (up | down) {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Erates;
          submenu = 0;
        } else if (down) {
          menu_mode = Eminspeed;
        }
      } else {
        lcd.print("Number of rates:");
        lcd.setCursor(0, 1);
        sprintf(line_buffer, "%02d", num_rates);
        lcd.print(line_buffer);
      }
      
      break;
    case Erates:
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          rates[submenu]++;
        }
        if (down) {
          rates[submenu]--;
        }
        if ((up | down) & edit) {
          validateState();
          saveEEPROM();
        }
        lcd.setCursor(0, 1);
        sprintf(line_buffer, "%02d", rates[submenu]);
        lcd.print(line_buffer);
      } else if (up | down) {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          if (submenu >= num_rates-1) {
            menu_mode = Erotperrate;
            submenu = 0;
          } else {
            submenu++;
          }
        } else if (down) {
          if (submenu <= 0) {
            menu_mode = Enumrates;
          } else {
            submenu--;
          }
        }
      } else {
        sprintf(line_buffer, "Rate %02d:", submenu);
        lcd.print(line_buffer);
        lcd.setCursor(0, 1);
        sprintf(line_buffer, "%02d", rates[submenu]);
        lcd.print(line_buffer);
      }

      break;
    case Erotperrate:
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          rot_per_rate[submenu] = rot_per_rate[submenu] + 0.1;
        }
        if (down) {
          rot_per_rate[submenu] = rot_per_rate[submenu] - 0.1;
        }
        if ((up | down) & edit) {
          validateState();
          saveEEPROM();
        }
        lcd.setCursor(0, 1);
        ftoa(line_buffer, rot_per_rate[submenu], 1);
        lcd.print("     ");
        lcd.setCursor(0, 1);
        lcd.print(line_buffer);
      } else if (up | down) {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          if (submenu >= num_rates-1) {
            menu_mode = Erandomizeorder;
            submenu = 0;
          } else {
            submenu++;
          }
        } else if (down) {
          if (submenu <= 0) {
            menu_mode = Erates;
            submenu = num_rates-1;
          } else {
            submenu--;
          }
        }
      } else {
        sprintf(line_buffer, "Rev for rate %02d:", submenu);
        lcd.print(line_buffer);
        lcd.setCursor(0, 1);
        ftoa(line_buffer, rot_per_rate[submenu], 1);
        lcd.print("     ");
        lcd.setCursor(0, 1);
        lcd.print(line_buffer);
      }

      break;
    case Erandomizeorder:
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          randomize_order = !randomize_order;
        }
        if (down) {
          randomize_order = !randomize_order;
        }
        if ((up | down) & edit) {
          validateState();
          saveEEPROM();
        }
        lcd.setCursor(0,1);
        if (randomize_order) {
          lcd.print("Yes");
        } else {
          lcd.print("No ");
        }
      } else if (up | down) {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Erandomizeduration;
        } else if (down) {
          menu_mode = Erotperrate;
          submenu = num_rates-1;
        }
      } else {
        lcd.print("Randomize order?:");
        lcd.setCursor(0,1);
        if (randomize_order) {
          lcd.print("Yes");
        } else {
          lcd.print("No ");
        }
      }

      break;
    case Erandomizeduration:
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          randomize_duration = !randomize_duration;
        }
        if (down) {
          randomize_duration = !randomize_duration;
        }
        if ((up | down) & edit) {
          validateState();
          saveEEPROM();
        }
        lcd.setCursor(0, 1);
        if (randomize_duration) {
          lcd.print("Yes");
        } else {
          lcd.print("No ");
        }
      } else if (up | down) {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Erandomizemin;
        } else if (down) {
          menu_mode = Erandomizeorder;
        }
      } else {
        lcd.print("Randomize duration?:");
        lcd.setCursor(0, 1);
        if (randomize_duration) {
          lcd.print("Yes");
        } else {
          lcd.print("No ");
        }
      }

      break;
    case Erandomizemin:
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          randomize_min = randomize_min + 0.1;
        }
        if (down) {
          randomize_min = randomize_min - 0.1;
        }
        if ((up | down) & edit) {
          validateState();
          saveEEPROM();
        }
        lcd.setCursor(0, 1);
        ftoa(line_buffer, randomize_min, 1);
        lcd.print("     ");
        lcd.setCursor(0, 1);
        lcd.print(line_buffer);
      } else if (up | down) {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Erandomizemax;
        } else if (down) {
          menu_mode = Erandomizeduration;
        }
      } else {
        lcd.print("Min Rand Time:");
        lcd.setCursor(0, 1);
        ftoa(line_buffer, randomize_min, 1);
        lcd.print("     ");
        lcd.setCursor(0, 1);
        lcd.print(line_buffer);
      }

      break;
    case Erandomizemax:
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          randomize_max = randomize_max + 0.1;
        }
        if (down) {
          randomize_max = randomize_max - 0.1;
        }
        if ((up | down) & edit) {
          validateState();
          saveEEPROM();
        }
        lcd.setCursor(0, 1);
        ftoa(line_buffer, randomize_max, 1);
        lcd.print("     ");
        lcd.setCursor(0, 1);
        lcd.print(line_buffer);
      } else if (up | down) {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Etransitiontime;
        } else if (down) {
          menu_mode = Erandomizemin;
        }
      } else {
        lcd.print("Max Rand Time:");
        lcd.setCursor(0, 1);
        ftoa(line_buffer, randomize_max, 1);
        lcd.print("     ");
        lcd.setCursor(0, 1);
        lcd.print(line_buffer);
      }
      
      break;
    case Etransitiontime:

      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          transition_time = transition_time + 0.1;
        }
        if (down) {
          transition_time = transition_time - 0.1;
        }
        if ((up | down) & edit) {
          validateState();
          saveEEPROM();
        }
        lcd.setCursor(0, 1);
        ftoa(line_buffer, transition_time, 1);
        lcd.print("     ");
        lcd.setCursor(0, 1);
        lcd.print(line_buffer);
      } else if (up | down) {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Ebrightstop;
        } else if (down) {
          menu_mode = Erandomizemax;
        }
      } else {
        lcd.print("Transition Time:");
        lcd.setCursor(0, 1);
        ftoa(line_buffer, transition_time, 1);
        lcd.print("     ");
        lcd.setCursor(0, 1);
        lcd.print(line_buffer);
      }
      
      break;
    case Ebrightstop:

      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          bright_stop++;
        }
        if (down) {
          bright_stop--;
        }
        if ((up | down) & edit) {
          validateState();
          saveEEPROM();
        }
        lcd.setCursor(0, 1);
        sprintf(line_buffer, "%02d", bright_stop);
        lcd.print(line_buffer);
      } else if (up | down) {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Ebrightrun;
        } else if (down) {
          menu_mode = Etransitiontime;
        }
      } else {
        lcd.print("Idle Brightness:");
        lcd.setCursor(0, 1);
        sprintf(line_buffer, "%02d", bright_stop);
        lcd.print(line_buffer);
      }
      
      break;
    case Ebrightrun:

      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          bright_run++;
        }
        if (down) {
          bright_run--;
        }
        if ((up | down) & edit) {
          validateState();
          saveEEPROM();
        }
        lcd.setCursor(0, 1);
        sprintf(line_buffer, "%02d", bright_run);
        lcd.print(line_buffer);
      } else if (up | down) {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Eminspeed;
        } else if (down) {
          menu_mode = Ebrightstop;
        }
      } else {
        lcd.print("Brightness:");
        lcd.setCursor(0, 1);
        sprintf(line_buffer, "%02d", bright_run);
        lcd.print(line_buffer);
      }
      
      break;
    case Eminspeed:
      if (edit) {
        lcd.setCursor(0, 3);
        lcd.print("EDIT");
        if (up) {
          min_speed = min_speed + 0.1;
        }
        if (down) {
          min_speed = min_speed - 0.1;
        }
        if ((up | down) & edit) {
          validateState();
          saveEEPROM();
        }
        lcd.setCursor(0, 1);
        ftoa(line_buffer, min_speed, 1);
        lcd.print("     ");
        lcd.setCursor(0, 1);
        lcd.print(line_buffer);
      } else if (up | down) {
        lcd.setCursor(0, 3);
        lcd.print("    ");
        if (up) {
          menu_mode = Enumrates;
        } else if (down) {
          menu_mode = Ebrightrun;
        }
      } else {
        lcd.print("Min Speed:");
        lcd.setCursor(0, 1);
        ftoa(line_buffer, min_speed, 1);
        lcd.print("     ");
        lcd.setCursor(0, 1);
        lcd.print(line_buffer);
      }
      
      break;
    default:
      break;
  }
  up = false;
  down = false;
  if ((menu_mode != old_menu) | old_submenu != submenu) {
    updateLCD();
  }
}

void setup() {
  pinMode(KNOB_S, INPUT_PULLUP);
  loadEEPROM();
  lcd.begin();
  updateLCD();
}

int refresh_timer = 0;

void loop() {
  if (inmenu) {
    refresh_timer = refresh_timer % 5000;
  } else {
    refresh_timer = refresh_timer % 500;
  }
  int button_state = digitalRead(KNOB_S);
  if (button_state != last_button_state) {
    if (digitalRead(KNOB_S)) {
      if (inmenu) {
        edit = !edit;
        up = false;
        down = false;
      } else {
        clearLCD();
      }
      refresh_timer = 1;
      inmenu = 1;
      updateLCD();
    }
  }
  last_button_state = button_state;

  long new_pos = knob.read();
  long diff = knob_pos - new_pos;
  if (diff < -3) {
    if (inmenu) {
      up = true;
    } else {
      clearLCD();
    }
    inmenu = 1;
    refresh_timer = 1;
    updateLCD();
    knob_pos = new_pos;
  } else if (diff > 3) {
    if (inmenu) {
      down = true;
    } else {
      clearLCD();
    }
    inmenu = 1;
    refresh_timer = 1;
    updateLCD();
    knob_pos = new_pos;
  }
  
  if (!refresh_timer) {
    if (inmenu) {
      clearLCD();
      if (!edit) {
        inmenu = 0;
      }
    }
    updateLCD();
  }
  refresh_timer++;
  delay(1);
}
