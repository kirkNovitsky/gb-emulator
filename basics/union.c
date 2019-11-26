#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

typedef struct {
  union {
    struct {
      uint8_t l;
      uint8_t h;
    };
    uint16_t hl;
  };
} Registers;

int main(int argc, char* argv[]) {
  Registers registers;
  registers.h = 0x12;
  registers.l = 0x34;
  printf("0x%04" PRIX16 "\n", registers.hl);
  return 0;
}
