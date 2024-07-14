#include "Frame.h"
#include <cmath>
#include <bitset>
#include <cassert>
#include <iostream>

Frame::Frame(uint8_t buf[14], const timestamp_t& ts)
{
    timestamp       = ts;

    AUTO            = (buf[0] & (1 << 1)) >> 1;
    DC              = (buf[0] & (1 << 2)) >> 2;
    AC              = (buf[0] & (1 << 3)) >> 3;
    digitThousand   = ((buf[1] & 7) << 4) | (buf[2] & 15);
    digitHundred    = ((buf[3] & 7) << 4) | (buf[4] & 15);
    digitTen        = ((buf[5] & 7) << 4) | (buf[6] & 15);
    digitSingle     = ((buf[7] & 7) << 4) | (buf[8] & 15);

    isNegative      = (buf[1] & (1 << 3)) >> 3;
    decimalPoint1   = (buf[3] & (1 << 3)) >> 3;
    decimalPoint2   = (buf[5] & (1 << 3)) >> 3;
    decimalPoint3   = (buf[7] & (1 << 3)) >> 3;

    diode           = (buf[9] & (1 << 0)) >> 0;
    kilo            = (buf[9] & (1 << 1)) >> 1;
    nano            = (buf[9] & (1 << 2)) >> 2;
    micro           = (buf[9] & (1 << 3)) >> 3;

    beep            = (buf[10] & (1 << 0)) >> 0;
    mega            = (buf[10] & (1 << 1)) >> 1;
    percent         = (buf[10] & (1 << 2)) >> 2;
    milli           = (buf[10] & (1 << 3)) >> 3;

    hold            = (buf[11] & (1 << 0)) >> 0;
    rel             = (buf[11] & (1 << 1)) >> 1;
    ohm             = (buf[11] & (1 << 2)) >> 2;
    farad           = (buf[11] & (1 << 3)) >> 3;

    battery         = (buf[12] & (1 << 0)) >> 0;
    hertz           = (buf[12] & (1 << 1)) >> 1;
    volt            = (buf[12] & (1 << 2)) >> 2;
    ampere          = (buf[12] & (1 << 3)) >> 3;

    celsius         = (buf[13] & (1 << 1)) >> 1;
    milliVolt       = (buf[13] & (1 << 2)) >> 2;
}

char Frame::digitToChar(Digit digit)
{
    if (digit == DEmpty)
        return ' ';
    if (digit == DL)
        return 'L';
    return digit + '0';
}

float Frame::getFloatVal() const
{
    if (getDigitThousandVal() > D9
        || getDigitHundredVal() > D9
        || getDigitTenVal() > D9
        || getDigitSingleVal() > D9)
        return NAN;

    float result = getDigitSingleVal() + getDigitTenVal()*10
        + getDigitHundredVal()*100 + getDigitThousandVal()*1000;
    if (isNegative)
        result = -result;

    if (decimalPoint3)
        result /= 10;
    if (decimalPoint2)
        result /= 100;
    if (decimalPoint1)
        result /= 1000;

    return result;
}

float Frame::getFloatValOrZero() const
{
    const float result = getFloatVal();
    return std::isnan(result) ? 0 : result;
}

Frame::Unit Frame::getUnit() const
{
    Unit result{};

    if (milliVolt)
    {
        result.prefix = Unit::Prefix::Milli;
        result.base = Unit::Base::Volt;
    }
    else
    {
        if (kilo)
            result.prefix = Unit::Prefix::Kilo;
        else if (nano)
            result.prefix = Unit::Prefix::Nano;
        else if (micro)
            result.prefix = Unit::Prefix::Micro;
        else if (mega)
            result.prefix = Unit::Prefix::Mega;
        else if (milli)
            result.prefix = Unit::Prefix::Milli;
        else
            result.prefix = Unit::Prefix::None;

        if (percent)
            result.base = Unit::Base::Percent;
        else if (ohm)
            result.base = Unit::Base::Ohm;
        else if (farad)
            result.base = Unit::Base::Farad;
        else if (hertz)
            result.base = Unit::Base::Hertz;
        else if (volt)
            result.base = Unit::Base::Volt;
        else if (ampere)
            result.base = Unit::Base::Ampere;
        else if (celsius)
            result.base = Unit::Base::Celsius;
        else
            assert(false);
    }

    return result;
}

std::string Frame::getUnitStr() const
{
    Unit unit = getUnit();
    std::string result;
    switch (unit.prefix)
    {
        case Unit::Prefix::Nano:
            result = "n";
            break;
        case Unit::Prefix::Micro:
            result = "\u00B5";
            break;
        case Unit::Prefix::Milli:
            result = "m";
            break;
        case Unit::Prefix::None:
            break;
        case Unit::Prefix::Kilo:
            result = "k";
            break;
        case Unit::Prefix::Mega:
            result = "M";
            break;
    }

    switch (unit.base)
    {
        case Unit::Base::Volt:
            result += "V";
            break;
        case Unit::Base::Ohm:
            result += "\u03A9";
            break;
        case Unit::Base::Farad:
            result += "F";
            break;
        case Unit::Base::Hertz:
            result += "Hz";
            break;
        case Unit::Base::Percent:
            result += "%";
            break;
        case Unit::Base::Celsius:
            result += "\u00B0C";
            break;
        case Unit::Base::Ampere:
            result += "A";
            break;
    }

    return result;
}

Frame::Digit Frame::digitToVal(uint8_t digit)
{
    switch (digit)
    {
    case 0b01111101: return D0;
    case 0b00000101: return D1;
    case 0b01011011: return D2;
    case 0b00011111: return D3;
    case 0b00100111: return D4;
    case 0b00111110: return D5;
    case 0b01111110: return D6;
    case 0b00010101: return D7;
    case 0b01111111: return D8;
    case 0b00111111: return D9;
    case 0b00000000: return DEmpty;
    case 0b01101000: return DL;
    default:
#ifndef NDEBUG
        std::cerr << "Invalid digit value: " << std::bitset<8>(digit) << '\n';
#endif
        assert(false);
        return DEmpty;
    }
}
