#include <Wire.h>
#include <stdint.h>
#include "RTClib.h"
#include <avr/sleep.h>
#include <avr/wdt.h>

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

#define TEST_TIMING
// #define DO_SERIAL_LOGGING

#ifndef TEST_TIMING
// const uint32_t WAKE_UP_TIME_SECONDS = 23400; // 6:30 am
// const uint32_t FADE_DURATION_SECONDS = 1800; // half an hour
// const uint32_t STAY_ON_DURATION_SECONDS = 7200; // two hours
const uint32_t WAKE_UP_TIME_SECONDS = 73980;
const uint32_t FADE_DURATION_SECONDS = 1800; // half an hour
const uint32_t STAY_ON_DURATION_SECONDS = 7200; // two hours
#else
uint32_t WAKE_UP_TIME_SECONDS = 0;
const uint32_t FADE_DURATION_SECONDS = 30;
const uint32_t STAY_ON_DURATION_SECONDS = 15;
#endif

RTC_DS1307 RTC;

void set_time() {
    DateTime today(2016, 10, 23, 23, 31, 0);
    RTC.adjust(today);
}

bool isDST(const DateTime &dt_utc)
{
    // January, february, and december are out.
    if (dt_utc.month() < 3 || dt_utc.month() > 11) {
        return false;
    }
    
    // April to October are in
    if (dt_utc.month() > 3 && dt_utc.month() < 11) {
        return true;
    }

    int previousSunday = (int)dt_utc.day() - (int)dt_utc.dayOfTheWeek();
    
    // In march, we are DST if our previous sunday was after the 8th.
    if (dt_utc.month() == 3) {
        if (dt_utc.dayOfTheWeek() == 0 && dt_utc.day() >= 8 && dt_utc.day() <= 14) {
            // Today is DST day

            return dt_utc.hour() >= 6; // DST changes at 2 am local time, aka 6 UTC
        } else {
            // Previous Sunday was DST day
            return previousSunday >= 8;
        }
    }
    
    // In november we must be before the first sunday to be dst.
    // That means the previous sunday must be before the 1st.

    if (dt_utc.dayOfTheWeek() == 0 && dt_utc.day() < 8) {
        // Today is DST day
        return dt_utc.hour() < 5; // DST changes at 2 am local time (with DST), aka 5 UTC
    }
    
    return previousSunday < 1;
}

DateTime toLocalTime(const DateTime &dt_utc) {
    if (isDST(dt_utc)) {
        return DateTime(dt_utc.unixtime() - (3 * 60 * 60));
    } else {
        return DateTime(dt_utc.unixtime() - (4 * 60 * 60));
    }
}

uint32_t secondsSinceMidnight(const DateTime &probablyShouldPassLocalTime) {
    return ((uint32_t)(probablyShouldPassLocalTime.hour()) * 60 + probablyShouldPassLocalTime.minute()) * 60 + probablyShouldPassLocalTime.second();
}

/////////////////////////////
// Testing and diagnostics //
/////////////////////////////

void print_date(const DateTime &dt) {
    Serial.print(dt.year(), DEC);
    Serial.print('/');
    Serial.print(dt.month(), DEC);
    Serial.print('/');
    Serial.print(dt.day(), DEC);
    Serial.print(' ');
    Serial.print(dt.hour(), DEC);
    Serial.print(':');
    Serial.print(dt.minute(), DEC);
    Serial.print(':');
    Serial.print(dt.second(), DEC);
}

void test_dst() {
    for (int i = 1; i <= 12; i++)
    {
        {
            DateTime dt(2016, i, 1, 10, 0, 0);
            Serial.print("10 am UTC start-month: ");
            print_date(toLocalTime(dt));
            Serial.println();
        }
        {
            DateTime dt(2016, i, 15, 10, 0, 0);
            Serial.print("10 am UTC mid-month: ");
            print_date(toLocalTime(dt));
            Serial.println();
        }
        {
            DateTime dt(2016, i, 27, 10, 0, 0);
            Serial.print("10 am UTC late-month: ");
            print_date(toLocalTime(dt));
            Serial.println();
        }
    }
  
    {
        DateTime dt(2016, 3, 13, 5, 50, 0);
        Serial.print("Standard time, morning of DST change: ");
        print_date(toLocalTime(dt));
        Serial.println();
    }
  
    {
        DateTime dt(2016, 3, 13, 6, 0, 0);
        Serial.print("Daylight time, morning of DST change: ");
        print_date(toLocalTime(dt));
        Serial.println();
    }
  
    {
        DateTime dt(2016, 11, 6, 4, 50, 0);
        Serial.print("Daylight time, morning of fall DST change: ");
        print_date(toLocalTime(dt));
        Serial.println();
    }
  
    {
        DateTime dt(2016, 11, 6, 5, 10, 0);
        Serial.print("Standard time, morning of fall DST change: ");
        print_date(toLocalTime(dt));
        Serial.println();
    }
}

void test_lighting() {
    for(uint16_t i = 0; i < 128; i++) {
        uint16_t value = MIN(round(float(i) * i / 64), 255);

        Serial.println(value, DEC);
        analogWrite(11, value);
        delay(50);
    }
}

///////////////////////

// GND to activate
const uint8_t disp_digit0 = A1;
const uint8_t disp_digit1 = A0;
const uint8_t disp_digit2 = A2;
const uint8_t disp_digit3 = A3;

const uint8_t disp_digits[] = { disp_digit0, disp_digit1, disp_digit2, disp_digit3, };

// 5V to activate
const uint8_t disp_top = 2;
const uint8_t disp_upperleft = 3;
const uint8_t disp_upperright = 4;
const uint8_t disp_middle = 6;
const uint8_t disp_lowerright = 7;
const uint8_t disp_dot = 8;
const uint8_t disp_bottom = 9;
const uint8_t disp_lowerleft = 10;

const uint8_t disp_segs[] = { disp_top, disp_upperleft, disp_upperright, disp_middle, disp_lowerright, disp_dot, disp_bottom, disp_lowerleft };

// mask: use table above, top (msb) to lowerleft (lsb)
const uint8_t segs_0 = 0b11101011;
const uint8_t segs_1 = 0b00101000;
const uint8_t segs_2 = 0b10110011;
const uint8_t segs_3 = 0b10111010;
const uint8_t segs_4 = 0b01111000;
const uint8_t segs_5 = 0b11011010;
const uint8_t segs_6 = 0b11011011;
const uint8_t segs_7 = 0b10101000;
const uint8_t segs_8 = 0b11111011;
const uint8_t segs_9 = 0b11111010;

const uint8_t segs[] = { segs_0, segs_1, segs_2, segs_3, segs_4, segs_5, segs_6, segs_7, segs_8, segs_9 };

const uint8_t squelch_button_pin = A7;
const uint8_t alarm_onoff_toggle_pin = 13;

void setup () {
#ifdef DO_SERIAL_LOGGING
    Serial.begin(57600);
#endif
    
    Wire.begin();
    RTC.begin();

#ifdef TEST_TIMING
    WAKE_UP_TIME_SECONDS = secondsSinceMidnight(toLocalTime(RTC.now())) + 5;
#endif
    
    // disabled: analog-only pin // pinMode(squelch_button_pin, INPUT_PULLUP); // squelch button
    pinMode(alarm_onoff_toggle_pin, INPUT_PULLUP); // enable/disable toggle (forces squelch when low)

    TCCR1A = 0x01;
    TCCR1B = 0x03;
    TIMSK1 = (1<<TOIE1); // use the overflow interrupt only

    // LED clock display
    digitalWrite(disp_top, LOW);
    pinMode(disp_top, OUTPUT);
    digitalWrite(disp_upperleft, LOW);
    pinMode(disp_upperleft, OUTPUT);
    digitalWrite(disp_upperright, LOW);
    pinMode(disp_upperright, OUTPUT);
    digitalWrite(disp_middle, LOW);
    pinMode(disp_middle, OUTPUT);
    digitalWrite(disp_lowerright, LOW);
    pinMode(disp_lowerright, OUTPUT);
    digitalWrite(disp_dot, LOW);
    pinMode(disp_dot, OUTPUT);
    digitalWrite(disp_bottom, LOW);
    pinMode(disp_bottom, OUTPUT);
    digitalWrite(disp_lowerleft, LOW);
    pinMode(disp_lowerleft, OUTPUT);
    
    digitalWrite(disp_digit0, LOW);
    pinMode(disp_digit0, OUTPUT);
    digitalWrite(disp_digit1, LOW);
    pinMode(disp_digit1, OUTPUT);
    digitalWrite(disp_digit2, LOW);
    pinMode(disp_digit2, OUTPUT);
    digitalWrite(disp_digit3, LOW);
    pinMode(disp_digit3, OUTPUT);
}

static bool squelched = true;
static bool userLightOn = false;

static volatile bool _timerFired = false;
static volatile uint8_t _clock_digits[] = { 0x80, 0x80, 0x80, 0x80 };

void setLightBrightness(uint32_t value, uint32_t scale) {
    // scale the value to 0..256 first
    
    uint32_t rescaledValue = (uint32_t)value * 256 / scale;
    
    // square it to compensate for light's actual brightness curve
    // TODO: is square the right function?
    
    uint8_t analogValue = MIN(lroundf(float(rescaledValue) * rescaledValue / 256), 255);

#ifdef DO_SERIAL_LOGGING
    Serial.print("Set brightness: ");
    Serial.println(analogValue, DEC);
#endif
    
    analogWrite(11, analogValue);
}

void updateClockDisplay(DateTime &when) {
    _clock_digits[0] = when.hour() / 10;
    _clock_digits[1] = (when.hour() % 10) | 0x80;
    _clock_digits[2] = when.minute() / 10;
    _clock_digits[3] = when.minute() % 10;
};

void updateBrightness() {
    DateTime now = toLocalTime(RTC.now());
    updateClockDisplay(now);
    uint32_t timeOfDay = secondsSinceMidnight(now);
    
#ifdef DO_SERIAL_LOGGING
    Serial.print("Current date in local time: ");
    print_date(now);
    Serial.println();
    Serial.print("Time in seconds since midnight: ");
    Serial.print(timeOfDay, DEC);
    Serial.println();
#endif
    
    if (userLightOn) {
        setLightBrightness(1, 1);
        return;
    }

    
    if (timeOfDay == WAKE_UP_TIME_SECONDS) {
        squelched = false;
    }

    if (digitalRead(3) == 0) {
        squelched = true;
    }

    
    if (squelched) {
        
        setLightBrightness(0, 1);
        
    } else {
        
        const uint32_t FADE_TIME_SECONDS = WAKE_UP_TIME_SECONDS + FADE_DURATION_SECONDS;
        const uint32_t STAY_ON_TIME_SECONDS = WAKE_UP_TIME_SECONDS + FADE_DURATION_SECONDS + STAY_ON_DURATION_SECONDS;

        if (timeOfDay >= WAKE_UP_TIME_SECONDS &&
            timeOfDay < FADE_TIME_SECONDS) {
        
            setLightBrightness(timeOfDay - WAKE_UP_TIME_SECONDS, FADE_DURATION_SECONDS);
        
        } else if (timeOfDay >= FADE_TIME_SECONDS &&
                   timeOfDay < STAY_ON_TIME_SECONDS) {

            setLightBrightness(1, 1);

        } else if (timeOfDay == STAY_ON_TIME_SECONDS) {
        
            squelched = true;
            setLightBrightness(0, 1);
        
        }
    }
    
}

static bool anything_fired(void) {
    return _timerFired;
}

static int analogReadAsDigital(uint8_t pin) {
    return analogRead(pin) > 80;
}

class DebouncedButton {
public:
    DebouncedButton(uint8_t _pin) : pin(_pin), readFunction(digitalRead) { }
    DebouncedButton(uint8_t _pin, int (*_readFunction)(uint8_t)) : pin(_pin), readFunction(_readFunction) { }

    // -1 = went low, 0 = did not change, +1 = went high
    int8_t didChange() {
        uint8_t value = readFunction(this->pin);

        if (!value && state <= -state_threshold) {
            return 0;
        }
        
        if (value && state >= state_threshold) {
            return 0;
        }
        
        if (!value) {
            if (state > 0) {
                state = 0;
            }
            state--;
            if (state == -state_threshold) {
                return -1;
            }
        }
        else if (value) {
            if (state < 0) {
                state = 0;
            }
            state++;
            if (state == state_threshold) {
                return 1;
            }
        }
        
        return 0;
    }
    
private:
    uint8_t pin;
    int (*readFunction)(uint8_t);
    int8_t state = 0;
    const int8_t state_threshold = 3;
};


void loop () {
    static uint8_t actionCounter = 0;

    analogReadAsDigital(squelch_button_pin);
    
    DebouncedButton squelchButton(squelch_button_pin, analogReadAsDigital);
    
    for(;;) {
        // Watch for interrupts, and sleep if nothing has fired.
        cli();
        while(!anything_fired()) {
            set_sleep_mode(SLEEP_MODE_IDLE);
            sleep_enable();
            // It's safe to enable interrupts (sei) immediately before
            // sleeping.  Interrupts can't fire until after the
            // following instruction has executed, so there's no race
            // condition between re-enabling interrupts and sleeping.
            sei();
            sleep_cpu();
            sleep_disable();
            cli();
        }

        bool timerFired = _timerFired;
        _timerFired = false;
        
        sei();

        
        if (timerFired) {
            if (actionCounter == 0) {
                updateBrightness();
                
            }
            actionCounter = (actionCounter + 1) % 16;

            if (squelchButton.didChange() == -1) {
                if (!squelched) {
                    squelched = true;
                } else {
                    userLightOn = !userLightOn;
                }
                updateBrightness();
            }
        }
    }
}

// fire only every n interrupts, not on every single interrupt
uint8_t const _timer1_fire_counter_max = 12;
uint8_t _timer1_fire_counter = 0;

// digits is a 4-array of digits (0 .. 9), msb set => dot on
void writeDigits(volatile uint8_t *digits) {
    uint8_t masks[4] = { 0, 0, 0, 0 };
    
    for(uint8_t i = 0; i < 4; i++) {
        uint8_t dotmask = (digits[i] & 0x80) ? 0b100 : 0;
        
        masks[i] = segs[digits[i] & 0x0f] | dotmask;
    }

    for(uint8_t seg = 0; seg < 8; seg++) {

        digitalWrite(disp_segs[seg], LOW);
        
        for(uint8_t digit = 0; digit < 4; digit++) {
            if (masks[digit] & (1 << (7 - seg))) {                
                digitalWrite(disp_digits[digit], HIGH);
            } else {
                digitalWrite(disp_digits[digit], LOW);
            }
        }
        
        for(uint8_t digit = 0; digit < 4; digit++) {
            digitalWrite(disp_digits[digit], LOW);
        }

        digitalWrite(disp_segs[seg], HIGH);
    }
}

ISR(TIMER1_OVF_vect) {
    writeDigits(_clock_digits);

    _timer1_fire_counter++;
    if (_timer1_fire_counter >= _timer1_fire_counter_max) {
        _timer1_fire_counter = 0;
        _timerFired = true;
    }
}
