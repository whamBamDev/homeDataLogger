
#ifndef _DEBUG_LIB_H_
#define _DEBUG_LIB_H_

#include <stdio.h>

#include <SD.h>
#include <Wire.h>

class Debug {
public:
    Debug (char *logFilename, bool logToConsole);

    void printf(const char *format, ...);
    void printf(const __FlashStringHelper *format, ...);
    void printfln(const char *format, ...);
    void printfln(const __FlashStringHelper *format, ...);

    void startLogging( void);
    void stopLogging( void);

private:
    char * _logFlename;
    bool _logEnabled = false;
    bool _logToConsole = false;

    void logMessage(const char *format, va_list args);
};

#endif // _DEBUG_LIB_H_

