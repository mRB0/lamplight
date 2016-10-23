#include <Wire.h>
#include <stdint.h>
#include "RTClib.h"
#include <avr/sleep.h>
#include <avr/wdt.h>

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

#define TEST_TIMING

#ifndef TEST_TIMING
const uint32_t WAKE_UP_TIME_SECONDS = 23400; // 6:30 am
const uint32_t FADE_DURATION_SECONDS = 1800; // half an hour
const uint32_t STAY_ON_DURATION_SECONDS = 7200; // two hours
#else
uint32_t WAKE_UP_TIME_SECONDS = 0;
const uint32_t FADE_DURATION_SECONDS = 30;
const uint32_t STAY_ON_DURATION_SECONDS = 15;
#endif

RTC_DS1307 RTC;

void set_time() {
    DateTime today(2016, 3, 25, 19, 6, 30);
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

void setup () {
    // Clear the watchdog
    // MCUSR &= ~_BV(WDRF);
    // wdt_disable();
    // wdt_reset();

    Serial.begin(57600);
    Wire.begin();
    RTC.begin();

    
#ifdef TEST_TIMING
    WAKE_UP_TIME_SECONDS = secondsSinceMidnight(toLocalTime(RTC.now())) + 5;
#endif
    
    pinMode(2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(2), button, FALLING);

    TCCR1A = 0x00;
    TCCR1B = 0x02;
    TIMSK1 = (1<<TOIE1); // use the overflow interrupt only

    // Serial.println("Reset; enable watchdog");
    
    // wdt_reset();
    // wdt_enable(WDTO_1S);
}

static bool squelched = true;

static volatile bool _buttonPressed = false;
static volatile bool _timerFired = false;

// NOTE: this is an ISR
void button() {
    _buttonPressed = true;
}


void setLightBrightness(uint32_t value, uint32_t scale) {
    // scale the value to 0..256 first
    
    uint32_t rescaledValue = (uint32_t)value * 256 / scale;
    
    // square it to compensate for light's actual brightness curve
    // TODO: is square the right function?
    
    uint8_t analogValue = MIN(lroundf(float(rescaledValue) * rescaledValue / 256), 255);

    Serial.print("Set brightness: ");
    Serial.println(analogValue, DEC);
    analogWrite(11, analogValue);
}

void updateBrightness() {
    DateTime now = toLocalTime(RTC.now());
    print_date(now);
    Serial.println();

    uint32_t timeOfDay = secondsSinceMidnight(now);

    if (timeOfDay == WAKE_UP_TIME_SECONDS) {
        squelched = false;
    }

    if (squelched) {
        setLightBrightness(0, 1);
        return;
    }

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
        
    } else {

        // not squelched, turn on (by user request)
        setLightBrightness(1, 1);
        
    }
    
}

static bool anything_fired(void) {
    return _timerFired || _buttonPressed;
}

void loop () {
    static uint8_t ledTestCounter = 0;
    static uint8_t buttonPressCounter = 0;
    
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
        bool buttonPressed = _buttonPressed;
        _timerFired = false;
        _buttonPressed = false;
        
        sei();

        
        if (buttonPressed) {
            buttonPressCounter = 1;
        }

        if (timerFired) {
            //wdt_reset();

            if (ledTestCounter == 0) {
                updateBrightness();
            }
            ledTestCounter = (ledTestCounter + 1) % 16;

            if (buttonPressCounter != 0) {
                // debounce
                buttonPressCounter = (buttonPressCounter + 1) % 3;
                if (buttonPressCounter == 0) {
                    uint8_t buttonState = digitalRead(2);

                    if (buttonState == 0) {
                        squelched = !squelched;
                        updateBrightness();
                    }
                }
            }
        }
    }
}

ISR(TIMER1_OVF_vect) {
    _timerFired = true;
}
