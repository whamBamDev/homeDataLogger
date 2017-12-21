#include "debug.h"

Debug::Debug (char *logFilename) {
   _logFlename = logFilename;
}

void Debug::printf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  logMessage(format, args);
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

void Debug::logMessage(const char *format, va_list args) {
  static char line[80];
  int len = vsnprintf(line, sizeof(line), format, args);
  for (char *p = &line[0]; *p; p++) {
      if (*p == '\n') {
          Serial.write('\r');
      }
      Serial.write(*p);
  }
  if (len >= sizeof(line))
      Serial.write('$');
}

void Debug::startLogging(void) {
  _log = true;
}

void Debug::stopLogging(void) {
  _log = false;
}



