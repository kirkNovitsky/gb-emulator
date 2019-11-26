#include <inttypes.h>
#include <stdio.h>

int main(int argc, char* argv[]) {

  // Strings literals (text between doublequotes) are concatenated
  printf("1. AAA "   "55"   " CCC"   "\n");

  // Also works for macros, which are just search & replace before compilation
#define MACRO "55"
  printf("2. AAA " MACRO " CCC\n");

  // "%" marks a formatting option for a paramter; X is upper-case hex
  printf("3. AAA %X CCC\n", 0x55);

  // inttypes.h has macros for stdint.h (`uint8_t` = PRIu8 / PRIx8 / PRIX8)
  printf("4. <" PRIX8 ">\n");

  // As these are string literals, we can place them in the code, after "%"
  printf("5. AAA %" PRIX8 " CCC\n", 0x55);

  // We can even use formatting (0 = pad with zero; 4 = number of digits)
  printf("6. AAA %04" PRIX8 " CCC\n", 0x55);

  return 0;
}
