#pragma once

#include <chrono>
#include <stdint.h>

using timestamp_t = std::chrono::system_clock::time_point;

class Frame
{
public:
    enum Digit : int
    {
        D0,
        D1,
        D2,
        D3,
        D4,
        D5,
        D6,
        D7,
        D8,
        D9,
        DEmpty,
        DL,
    };

    struct Unit
    {
        enum class Prefix
        {
            Nano,
            Micro,
            Milli,
            None,
            Kilo,
            Mega,
        } prefix;

        enum class Base
        {
            Volt,
            Ohm,
            Farad,
            Hertz,
            Percent,
            Celsius,
            Ampere,
        } base;
    };

    timestamp_t timestamp{};
    bool AUTO{};
    bool DC{};
    bool AC{};
    bool isNegative{};
    uint8_t digitThousand{};
    uint8_t digitHundred{};
    uint8_t digitTen{};
    uint8_t digitSingle{};
    bool decimalPoint1{};
    bool decimalPoint2{};
    bool decimalPoint3{};
    bool diode{};
    bool kilo{};
    bool nano{};
    bool micro{};
    bool beep{};
    bool mega{};
    bool percent{};
    bool milli{};
    bool hold{};
    bool rel{};
    bool ohm{};
    bool farad{};
    bool battery{};
    bool hertz{};
    bool volt{};
    bool ampere{};
    bool celsius{};
    bool milliVolt{};

    Frame(uint8_t buf[14], const timestamp_t& ts);

    inline Digit getDigitThousandVal() const { return digitToVal(digitThousand); }
    inline Digit getDigitHundredVal()  const { return digitToVal(digitHundred); }
    inline Digit getDigitTenVal()      const { return digitToVal(digitTen); }
    inline Digit getDigitSingleVal()   const { return digitToVal(digitSingle); }

    static char digitToChar(Digit digit);

    float getFloatVal() const;
    float getFloatValOrZero() const;

    Unit getUnit() const;

    std::string getUnitStr() const;

private:
    static Digit digitToVal(uint8_t digit);
};

