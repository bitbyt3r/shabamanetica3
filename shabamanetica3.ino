#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <Encoder.h>
#include <avr/io.h>
#include <avr/interrupt.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);  

#define KNOB_A 3
#define KNOB_B 2
#define KNOB_S 8
Encoder knob(KNOB_A, KNOB_B);

#define ENC_A A1
#define ENC_B A2

#define LED 7
#define STATUS 9

#define MAX_RATES 25
int num_rates = 1;
int rates[MAX_RATES];
float rot_per_rate[MAX_RATES];
byte randomize_order = 0;
byte randomize_duration = 0;
float randomize_min = 0;
float randomize_max = 2;
float transition_time = 1;
volatile int bright_stop = 10;
volatile int bright_run = 10;
float min_speed = 10;
float rpm = 0;

long knob_pos = 0;
int last_button_state = 1;
int edit = 0;
int up = 0;
int down = 0;
int debug = 0;
char line_buffer[20];
int submenu = 0;
int inmenu = 0;

#define STEPS 2400
volatile int stopped = 0;
volatile int current_rate = 0;
volatile int current_step_count = 0;
volatile float current_pos = 0.0;
volatile int step_counter = 0;
volatile long current_random_duration = 1;
volatile int idle = 0;
volatile long last_millis = 0;
volatile long step_time = 0;
int last_pin_state = 0;

void saveEEPROM() {
  EEPROM.put(0, num_rates);
  for (int i=0;i<MAX_RATES;i++) {
    EEPROM.put(2+i*2, rates[i]);
    EEPROM.put(2*MAX_RATES+2+i*4, rot_per_rate[i]);
  }
  EEPROM.put(6*MAX_RATES+2, randomize_order);
  EEPROM.put(6*MAX_RATES+3, randomize_duration);
  EEPROM.put(6*MAX_RATES+4, randomize_min);
  EEPROM.put(6*MAX_RATES+8, randomize_max);
  EEPROM.put(6*MAX_RATES+12, transition_time);
  EEPROM.put(6*MAX_RATES+16, bright_stop);
  EEPROM.put(6*MAX_RATES+18, bright_run);
  EEPROM.put(6*MAX_RATES+20, min_speed);
}

void validateState() {
  num_rates = min(max(num_rates, 1), MAX_RATES);
  for (int i=0;i<MAX_RATES;i++) {
    rates[i] = min(max(rates[i], 1), 500);
    rot_per_rate[i] = min(max(rot_per_rate[i], 1.0), 500.0);
  }
  transition_time = min(max(transition_time, 0.0), 10.0);
  bright_stop = min(max(bright_stop, 2), 98);
  bright_run = min(max(bright_run, 2), 98);
  min_speed = min(max(min_speed, 1.0), 100.0);
  randomize_min = max(randomize_min, 0.0);
  randomize_max = min(randomize_max, 100.0);
  randomize_min = min(randomize_min, randomize_max);
  randomize_max = max(randomize_min, randomize_max);
}

void loadEEPROM() {
  EEPROM.get(0, num_rates);
  for (int i=0;i<MAX_RATES;i++) {
    EEPROM.get(2+i*2, rates[i]);
    EEPROM.get(2*MAX_RATES+2+i*4, rot_per_rate[i]);
  }
  EEPROM.get(6*MAX_RATES+2, randomize_order);
  EEPROM.get(6*MAX_RATES+3, randomize_duration);
  EEPROM.get(6*MAX_RATES+4, randomize_min);
  EEPROM.get(6*MAX_RATES+8, randomize_max);
  EEPROM.get(6*MAX_RATES+12, transition_time);
  EEPROM.get(6*MAX_RATES+16, bright_stop);
  EEPROM.get(6*MAX_RATES+18, bright_run);
  EEPROM.get(6*MAX_RATES+20, min_speed);
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
    lcd.print("System Status");
    lcd.setCursor(0, 1);
    sprintf(line_buffer, "Rate: %02d (%03d)", current_rate, rates[current_rate]);
    lcd.print(line_buffer);
    lcd.setCursor(0, 2);
    lcd.print("RPM: ");
    ftoa(line_buffer, rpm, 2);
    lcd.print(line_buffer);
    long max_steps = 0;
    if (randomize_duration) {
      max_steps = STEPS * current_random_duration;
    } else {
      max_steps = STEPS * rot_per_rate[current_rate];
    }
    lcd.setCursor(0,3);
    if (idle) {
      sprintf(line_buffer, "IDLE    %05d/%05d", step_counter, max_steps);
    } else {
      sprintf(line_buffer, "RUNNING %05d/%05d ", step_counter, max_steps);
    }
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
        lcd.print("00.Number of rates:");
        lcd.setCursor(0, 1);
        sprintf(line_buffer, "%02d", num_rates);
        lcd.print(line_buffer);
        lcd.setCursor(0, 3);
        lcd.print("    ");
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
        sprintf(line_buffer, "01.Rate %02d:", submenu);
        lcd.print(line_buffer);
        lcd.setCursor(0, 1);
        sprintf(line_buffer, "%02d", rates[submenu]);
        lcd.print(line_buffer);
        lcd.setCursor(0, 3);
        lcd.print("    ");
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
        sprintf(line_buffer, "02.Rev for rate %02d:", submenu);
        lcd.print(line_buffer);
        lcd.setCursor(0, 1);
        ftoa(line_buffer, rot_per_rate[submenu], 1);
        lcd.print("     ");
        lcd.setCursor(0, 1);
        lcd.print(line_buffer);
        lcd.setCursor(0, 3);
        lcd.print("    ");
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
        lcd.print("03.Random order?:");
        lcd.setCursor(0,1);
        if (randomize_order) {
          lcd.print("Yes");
        } else {
          lcd.print("No ");
        }
        lcd.setCursor(0, 3);
        lcd.print("    ");
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
        lcd.print("04.Random duration?:");
        lcd.setCursor(0, 1);
        if (randomize_duration) {
          lcd.print("Yes");
        } else {
          lcd.print("No ");
        }
        lcd.setCursor(0, 3);
        lcd.print("    ");
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
        lcd.print("05.Min Rand Time:");
        lcd.setCursor(0, 1);
        ftoa(line_buffer, randomize_min, 1);
        lcd.print("     ");
        lcd.setCursor(0, 1);
        lcd.print(line_buffer);
        lcd.setCursor(0, 3);
        lcd.print("    ");
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
        lcd.print("06.Max Rand Time:");
        lcd.setCursor(0, 1);
        ftoa(line_buffer, randomize_max, 1);
        lcd.print("     ");
        lcd.setCursor(0, 1);
        lcd.print(line_buffer);
        lcd.setCursor(0, 3);
        lcd.print("    ");
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
        lcd.print("07.Transition Time:");
        lcd.setCursor(0, 1);
        ftoa(line_buffer, transition_time, 1);
        lcd.print("     ");
        lcd.setCursor(0, 1);
        lcd.print(line_buffer);
        lcd.setCursor(0, 3);
        lcd.print("    ");
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
        lcd.print("08.Idle Brightness:");
        lcd.setCursor(0, 1);
        sprintf(line_buffer, "%02d", bright_stop);
        lcd.print(line_buffer);
        lcd.setCursor(0, 3);
        lcd.print("    ");
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
        lcd.print("09.Brightness:");
        lcd.setCursor(0, 1);
        sprintf(line_buffer, "%02d", bright_run);
        lcd.print(line_buffer);
        lcd.setCursor(0, 3);
        lcd.print("    ");
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
        lcd.print("10.Min Speed:");
        lcd.setCursor(0, 1);
        ftoa(line_buffer, min_speed, 1);
        lcd.print("     ");
        lcd.setCursor(0, 1);
        lcd.print(line_buffer);
        lcd.setCursor(0, 3);
        lcd.print("    ");
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
  DDRC &= ~(1 << DDC1);
  PORTC |= (1 << PORTC1);
  DDRC &= ~(1 << DDC2);
  PORTC |= (1 << PORTC2);

  TCCR1A = 0;
  TCCR1B = 0;

  TIMSK1 |= (1 << OCIE1A);
  TCCR1B |= (1 << WGM12) | (1 << CS10);
  
  pinMode(LED, OUTPUT);
  pinMode(STATUS, OUTPUT);
  digitalWrite(STATUS, HIGH);
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  digitalWrite(LED, LOW);
  pinMode(KNOB_S, INPUT_PULLUP);
  loadEEPROM();
  lcd.begin();
  updateLCD();
}

int refresh_timer = 0;

void loop() {
  stopped++;
  if (stopped > 500) {
    digitalWrite(LED, LOW);
    stopped = 0;
    idle = 1;
  }
  if (inmenu) {
    refresh_timer = refresh_timer % 10000;
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
  rpm = 60000000 / ((last_millis - step_time) * STEPS);
  if (rpm > min_speed) {
    idle = 0;
  } else {
    idle = 1;
  }
  int start = millis();
  while (start <= millis()) {
    int new_pin_state = digitalRead(ENC_A);
    if(last_pin_state != new_pin_state) {
      last_pin_state = new_pin_state;
      tick();
    }
  }
}

void tick() {
  step_time = last_millis;
  last_millis = micros();
  stopped = 0;
  current_step_count++;
  step_counter++;
  if ((step_counter >= (STEPS * rot_per_rate[current_rate])) | (randomize_duration & (step_counter >= (STEPS * current_random_duration)))) {
    step_counter = 0;
    if (randomize_duration) {
      current_random_duration = random(randomize_min, randomize_max);
    }
    if (randomize_order) {
      current_rate = current_rate + random(num_rates);
    }
    current_rate++;
    current_rate = current_rate % num_rates;
  }
  int steps = STEPS / rates[current_rate];
  if (current_step_count >= steps) {
    current_step_count = 0;
    if (!idle) {
      digitalWrite(LED, HIGH);
      OCR1AH = (bright_run * 655) / 256;
      OCR1AL = (bright_run * 655) % 256;
      TCNT1 = 0;
    }
  }
}

ISR (TIMER1_COMPA_vect) {
  if (idle) {
    if (digitalRead(LED)) {
      digitalWrite(LED, LOW);
      long temp = 65535 - (bright_stop * 655);
      OCR1AH = temp / 256;
      OCR1AL = temp % 256;
    } else {
      digitalWrite(LED, HIGH);
      long temp = bright_stop * 655;
      OCR1AH = temp / 256;
      OCR1AL = temp % 256;
    }
  } else {
    digitalWrite(LED, LOW);
  }
}

