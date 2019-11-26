#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

typedef struct {
  union {
    struct {
      uint8_t bit0:1; // 0x01 (lsb = least significant bit)
      uint8_t bit1:1; // 0x02
      uint8_t bit2:1; // 0x04
      uint8_t bit3:1; // 0x08
      uint8_t bit4:1; // 0x10
      uint8_t bit5:1; // 0x20
      uint8_t bit6:1; // 0x40
      uint8_t bit7:1; // 0x80 (msb = most significant bit)
    };
    uint8_t byte;
  };
} Registers;

int main(int argc, char* argv[]) {
  Registers registers;
  registers.byte = 0x00;
  registers.bit1 = 1; // 1 << 1 = 0x02
  registers.bit7 = 1; // 1 << 7 = 0x80
  printf("0x%02" PRIX8 "\n", registers.byte);
  return 0;
}
