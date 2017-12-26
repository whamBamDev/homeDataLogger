#include "debug.h"

Debug::Debug (char *logFilename, bool logToConsole) {
   _logFlename = logFilename;
   _logToConsole = logToConsole;
  if(_logToConsole) {
    Serial.begin(115200);   //start serial port for WIFI
  }
}

void Debug::printf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  logMessage(format, args);
  va_end(args);
}

void Debug::printf(const __FlashStringHelper *format, ...) {
  va_list args;
  va_start(args, format);
  logMessage((const char *) format, args);
  va_end(args);
}


void Debug::printfln(const char *format, ...) {
  va_list args;
  va_start(args, format);
  logMessage(format, args);
  va_end(args);

  Serial.write('\r');
  Serial.write('\n');
}

void Debug::printfln(const __FlashStringHelper *format, ...) {
  va_list args;
  va_start(args, format);
  logMessage( (const char *) format, args);
  va_end(args);

  Serial.write('\r');
  Serial.write('\n');
}

void Debug::logMessage(const char *format, va_list args) {
    static char line[80];
   if( _logEnabled || _logToConsole) {
    int len = vsnprintf_P(line, sizeof(line), (const char *) format, args);
    for (char *p = &line[0]; *p; p++) {
        if (*p == '\n') {
            Serial.write('\r');
        }
        Serial.write(*p);
    }
    if (len >= sizeof(line)) {
      Serial.write('$');
    }
  }
}

void Debug::startLogging(void) {
  _logEnabled = true;
}

void Debug::stopLogging(void) {
  _logEnabled = false;
}

