constexpr uint8_t GetMon()
{
    return (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n')?0:
    (__DATE__[0] == 'F')?1:
    (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r')?2:
    (__DATE__[0] == 'A' && __DATE__[1] == 'p')?3:
    (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y')?4:
    (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n')?5:
    (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l')?6:
    (__DATE__[0] == 'A' && __DATE__[1] == 'u')?7:
    (__DATE__[0] == 'S')?8:
    (__DATE__[0] == 'O')?9:
    (__DATE__[0] == 'N')?10:
    (__DATE__[0] == 'D')?11:0;
}
constexpr uint8_t GetDay()
{
    return (uint8_t)((__DATE__[4] - '0') * 10 + (__DATE__[5] - '0'));
}
constexpr uint8_t GetYear()
{
    return (__DATE__[9] - '0') * 10 + (__DATE__[10] - '0');
}
constexpr uint8_t GetMin()
{
    return (__TIME__[3] - '0') * 10 + (__TIME__[4] - '0');
}
constexpr uint8_t GetH()
{
    return (__TIME__[0] - '0') * 10 + (__TIME__[1] - '0');
}
constexpr uint8_t GetS()
{
    return (__TIME__[6] - '0') * 10 + (__TIME__[7] - '0');
}

constexpr uint8_t GetDateByte()
{
    return GetYear() + GetMon() + GetDay();
}

constexpr uint8_t GetTimeByte()
{
    return ((((GetMin() * 60) + GetS()) % 256) >> 3) | ((GetH() % 8) << 5);
}

void GenArr(uint8_t seed, uint8_t *arr)
{
    memset(arr, 0xFF, 32);
    uint8_t dat = (1 << ((seed & 0b00000111))) ^ 0xFF;
    arr[seed >> 3] = dat;
}

void GenArrDate(uint8_t *arr)
{
    GenArr(GetDateByte(), arr);
}

void GenArrTime(uint8_t *arr)
{
    GenArr(GetTimeByte(), arr);
}