#include "type_conv.h"

char* itoa(int val, char* str) {
    char* ptr = str;
    char* ptr1 = str;
    char temp;
    int is_negative = 0;

    if (val == 0) {
        str[0] = '0';
        return str;
    }
    if (val < 0) {
        is_negative = 1;
        val = -val;
    }
    while (val != 0) {
        int digit = val % 10;
        *ptr++ = digit + '0';
        val /= 10;
    }
    if (is_negative) {
        *ptr++ = '-';
    }
    *ptr-- = '\0';
    while (ptr1 < ptr) {
        temp = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = temp;
    }
    return str;
}

char* htoa(uint32_t val, char* str) {
    char* ptr = str;
    char* ptr1 = str;
    char temp;

    if (val == 0) {
        str[0] = '0';
        str[1] = '\0';
        return str;
    }
    while (val != 0) {
        uint32_t digit = val % 16;

        if (digit < 10) {
            *ptr++ = digit + '0';
        }
        else {
            *ptr++ = digit - 10 + 'A';
        }
        val /= 16;
    }
    *ptr-- = '\0';

    while (ptr1 < ptr) {
        temp = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = temp;
    }
    return str;
}

char* htoan(uint32_t val, char* str, int width) {
    char buf[32];
    htoa(val, buf);

    int len = 0;
    while (buf[len] != '\0') len++;

    int pad = width - len;
    int i = 0;

    while (pad-- > 0) {
        str[i++] = '0';
    }

    for (int j = 0; j < len; j++) {
        str[i++] = buf[j];
    }

    str[i] = '\0';

    return str;
}

int atoi(const char* str) {
    int result = 0;
    int sign = 1;

    while (*str == ' ') str++;

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }

    return result * sign;
}

uint8_t bcd_to_bin(uint8_t val) {
    return ((val / 16) * 10) + (val & 0x0F);
}