#include <Wire.h>
#include <stdint.h>
#include "RTClib.h"

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
    Serial.begin(57600);
    Wire.begin();
    RTC.begin();

#ifdef TEST_TIMING
    WAKE_UP_TIME_SECONDS = secondsSinceMidnight(toLocalTime(RTC.now())) + 5;
#endif
    
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

volatile static bool squelched = false;

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
        
    } else {
        squelched = true;
        setLightBrightness(0, 1);
    }
    
}

void loop () {
    updateBrightness();
    delay(500);
}
