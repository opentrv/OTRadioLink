#define __ASSERT_USE_STDERR
#include <assert.h>
#include <Arduino.h>

/**
 * 

/**
 * @brief   Print information about the fail to serial and abort the program.
 *          Prints "***Failed <function name> with <test expression> at line <line number>"
 */
void __assert(const char *__func, const char *__file, int __lineno, const char *__sexp) {
    Serial.print(F("***FAILED "));
    Serial.print(__func);
    Serial.print(F(" with "));
    Serial.print(__sexp);
    Serial.print(F(", at line "));
    Serial.println(__lineno);
    Serial.flush();
    abort();
}

namespace OTUnitTest {
    void begin(int baud) {
        if (!Serial) Serial.begin(baud);
        Serial.println(F("\n\n+++Beginning Tests+++\n"));
    }
    
    void end() {
        Serial.println(F("\n+++All tests passed+++\n\n"));
        delay(2000);
    }

}
