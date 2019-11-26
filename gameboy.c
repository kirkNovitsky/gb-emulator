#define DEBUG 1
#define DISASSEMBLE 0



#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "gameboy.h"

GameboyInput gameboy_input;
uint8_t gameboy_framebuffer[GAMEBOY_SCREEN_WIDTH * GAMEBOY_SCREEN_HEIGHT];

size_t cartridge_rom_size = 0;
uint8_t* cartridge_rom_memory = NULL;


// Declare IO ports
#define JOYP 0xFF00
#define TIMA 0xFF05
#define TMA  0xFF06
#define TAC  0xFF07
#define IF   0xFF0F
#define NR10 0xFF10
#define NR11 0xFF11
#define NR12 0xFF12
#define NR14 0xFF14
#define NR21 0xFF16
#define NR22 0xFF17
#define NR24 0xFF19
#define NR30 0xFF1A
#define NR31 0xFF1B
#define NR32 0xFF1C
#define NR33 0xFF1E
#define NR41 0xFF20
#define NR42 0xFF21
#define NR43 0xFF22
#define NR44 0xFF23
#define NR50 0xFF24
#define NR51 0xFF25
#define NR52 0xFF26
#define LCDC 0xFF40
#define STAT 0xFF41
#define SCY  0xFF42
#define SCX  0xFF43
#define LY   0xFF44
#define LYC  0xFF45
#define DMA  0xFF46
#define BGP  0xFF47
#define OBP0 0xFF48
#define OBP1 0xFF49
#define WY   0xFF4A
#define WX   0xFF4B

static uint8_t io_ports[0x80];
static uint8_t ie;
static bool ime;

static uint8_t read_io8(uint16_t address) {
  assert((address >= 0xFF00) && (address <= 0xFF7F));
  int offset = address - 0xFF00;
  uint8_t v = io_ports[offset];
  
  if (address == JOYP) {
      
    // Clear inputs (1=not-pressed)
    v |= 0xF;
  
    // The eight gameboy buttons/direction keys are arranged in form of a 2x4 matrix.
    // Select either button or direction keys by writing to this register, then read-out bit 0-3.
    
    
    // Bit 5 - Select Button Keys      (0=Select)
    bool button_mode = !(v & (1 << 5));
 
    //  Bit 4 - Select Direction Keys   (0=Select)
    bool direction_mode = !(v & (1 << 4));
    
    // Check for mode conflicts
    
    assert(button_mode != direction_mode);     

    if (button_mode) {
      // Bit 3 - Start    (0=Pressed) (Read Only)
      if (gameboy_input.start) { v &= ~(1 << 3); }
      // Bit 2 - Select   (0=Pressed) (Read Only)
      if (gameboy_input.select) { v &= ~(1 << 2); }
      // Bit 1 - Button B (0=Pressed) (Read Only)
      if (gameboy_input.b) { v &= ~(1 << 1); }
      // Bit 0 - Button A (0=Pressed) (Read Only)
      if (gameboy_input.a) { v &= ~(1 << 0); }
    }
    
    if (direction_mode) {
      // Bit 3 - Input Down  (0=Pressed) (Read Only)
      if (gameboy_input.down) { v &= ~(1 << 3); }
      // Bit 2 - Input Up    (0=Pressed) (Read Only)
      if (gameboy_input.up) { v &= ~(1 << 2); }
      // Bit 1 - Input Left  (0=Pressed) (Read Only)
      if (gameboy_input.left) { v &= ~(1 << 1); }
      // Bit 0 - Input Right (0=Pressed) (Read Only)
      if (gameboy_input.right) { v &= ~(1 << 0); }
    }
    return v;
  }

  return v;
}

static uint8_t read_memory8(uint16_t address);
static void write_memory8(uint16_t address, uint8_t v);
static void write_io8(uint16_t address, uint8_t v) {
  assert((address >= 0xFF00) && (address <= 0xFF7F));
  int offset = address - 0xFF00;
  io_ports[offset] = v;
  
  if (address == DMA) {
  
    //timing - From pan docs
    // It takes 160 microseconds until the transfer has completed (80 microseconds in CGB Double Speed Mode), during this time the CPU can access only HRAM (memory at FF80-FFFE).
    // For this reason, the programmer must copy a short procedure into HRAM, and use this procedure to start the transfer from inside HRAM, and wait until the transfer has finished.
  
    // Source:      XX00-XX9F   ;XX in range from 00-F1h
    // Destination: FE00-FE9F
    uint16_t source = v * 0x100;
    uint16_t destination = 0xFE00;
    unsigned int size = 0xA0;
    while(size--) {
      uint8_t byte = read_memory8(source++);
      write_memory8(destination++, byte);
    }
    
  }

}

#define CARTRIDGE_RAM_BANKS 4

static uint8_t vram_memory[8 * 1024];
static uint8_t cartridge_ram_memory[8 * 1024 * CARTRIDGE_RAM_BANKS];
static uint8_t wram0_memory[4 * 1024];
static uint8_t wram1_memory[4 * 1024];
static uint8_t echo_memory[0x1E00];
static uint8_t oam_memory[0xA0];
static uint8_t hram_memory[0x80];



// MBC1
static bool ram_enable; // 0000-1FFF - RAM Enable (Write Only)
static uint8_t rom_bank_number; // 2000-3FFF - ROM Bank Number (Write Only)
static uint8_t rom_ram_bank_number; // 4000-5FFF - RAM Bank Number - or - Upper Bits of ROM Bank Number (Write Only)
static bool rom_ram_mode_select; // 6000-7FFF - ROM/RAM Mode Select (Write Only)

//FIXME: Should be MBC1
static unsigned int get_rom_bank_number(unsigned address) {

  if ((address >= 0x4000) && (address <= 0x7FFF)) {

    int bank_number;
    if (!rom_ram_mode_select) { // 00h = ROM Banking Mode (up to 8KByte RAM, 2MByte ROM) (default)
      // 7 bit ROM bank number
      return (rom_ram_bank_number << 5) | rom_bank_number;
    }

    // 01h = RAM Banking Mode (up to 32KByte RAM, 512KByte ROM)
    // 5 bit ROM bank number
    return rom_bank_number;

  }

  return 0x00;
}


//FIXME: Should be MBC1
static unsigned int get_ram_bank_number() {

  int bank_number;
  if (!rom_ram_mode_select) { // 00h = ROM Banking Mode (up to 8KByte RAM, 2MByte ROM) (default)
    return 0x00;
  }

  // 01h = RAM Banking Mode (up to 32KByte RAM, 512KByte ROM)
  return rom_ram_bank_number;
}

// Implements memory maps (except I/O ports)
static uint8_t* map_memory(uint16_t address) {

  // 0000-3FFF   16KB ROM Bank 00     (in cartridge, fixed at bank 00)
  if ((address >= 0x0000) && (address <= 0x3FFF)) {
    int offset = address - 0x0000;
    return &cartridge_rom_memory[offset];

  // 4000-7FFF   16KB ROM Bank 01..NN (in cartridge, switchable bank number)
  } else if ((address >= 0x4000) && (address <= 0x7FFF)) {
    int offset = address - 0x4000;
    int bank_base = get_rom_bank_number(address) * 0x4000;
    return &cartridge_rom_memory[bank_base + offset];

  // 8000-9FFF   8KB Video RAM (VRAM) (switchable bank 0-1 in CGB Mode)
  } else if ((address >= 0x8000) && (address <= 0x9FFF)) {
    int offset = address - 0x8000;

    
    //assert(false); // this is possibly only for game boy color -> have to double check
   //possibly come back to this part
    return &vram_memory[offset];

  // A000-BFFF   8KB External RAM     (in cartridge, switchable bank, if any)
  } else if ((address >= 0xA000) && (address <= 0xBFFF)) {
    int offset = address - 0xA000;
    int bank_base = get_ram_bank_number() * 0x2000;
    return &cartridge_ram_memory[bank_base + offset];

  // C000-CFFF   4KB Work RAM Bank 0 (WRAM)
  } else if ((address >= 0xC000) && (address <= 0xCFFF)) {
    int offset = address - 0xC000;
    return &wram0_memory[offset];

  // D000-DFFF   4KB Work RAM Bank 1 (WRAM)  (switchable bank 1-7 in CGB Mode)
  } else if ((address >= 0xD000) && (address <= 0xDFFF)) {
    int offset = address - 0xD000;

    
    //assert(false); // possibly come back to this

    return &wram1_memory[offset];

  // E000-FDFF   Same as C000-DDFF (ECHO)    (typically not used)
  } else if ((address >= 0xE000) && (address <= 0xFDFF)) {
    int offset = address - 0xE000;
    return &echo_memory[offset];

  // FE00-FE9F   Sprite Attribute Table (OAM)
  } else if ((address >= 0xFE00) && (address <= 0xFE9F)) {
    int offset = address - 0xFE00;
    return &oam_memory[offset];

  // FEA0-FEFF   Not Usable
  } else if ((address >= 0xFEA0) && (address <= 0xFEFF)) {
    // Already handled in read_/write_memory.
    assert(false);

  // FF00-FF7F   I/O Ports
  } else if ((address >= 0xFF00) && (address <= 0xFF7F)) {
    // Already handled in read_/write_memory.
    assert(false);

  // FF80-FFFE   High RAM (HRAM)
  } else if ((address >= 0xFF80) && (address <= 0xFFFE)) {
    int offset = address - 0xFF80;
    return &hram_memory[offset];

  // FFFF        Interrupt Enable Register
  } else if (address == 0xFFFF) {
     return &ie;

  } else {
    fprintf(stderr, "Unmapped memory address: 0x%04X\n", address);
    assert(false);
  }
  return NULL;
}

static uint8_t read_memory8(uint16_t address) {  
  if ((address >= 0xFEA0) && (address <= 0xFEFF)) {
    // this memory range is unused, if this comes up it is wrong
    return 0xFF;
  } else if ((address >= 0xFF00) && (address <= 0xFF7F)) {
    return read_io8(address);
  } else {
    uint8_t* memory = map_memory(address);
    return *memory;
  }
}

static void write_memory8(uint16_t address, uint8_t v) {

  if ((address >= 0x0000) && (address <= 0x1FFF)) { // MBC1: RAM Enable (Write Only)
    // From Pandocs:
    // Practically any value with 0Ah in the lower 4 bits enables RAM, and any other value disables RAM.
    ram_enable = ((v & 0xF) == 0xA);
  } else if ((address >= 0x2000) && (address <= 0x3FFF)) { // MBC1: ROM Bank Number (Write Only)
    // From Pandocs:
    // Writing to this address space selects the lower 5 bits of the ROM Bank Number (in range 01-1Fh).
    // When 00h is written, the MBC translates that to bank 01h [...].
    // The same happens for Bank 20h, 40h, and 60h. Any attempt to address these ROM Banks will select Bank 21h, 41h, and 61h instead.
    if (v == 0x00) { v == 0x01; }
    if (v == 0x20) { v == 0x21; }
    if (v == 0x40) { v == 0x41; }
    if (v == 0x60) { v == 0x61; }
    assert(v <= 0x1F);
    rom_bank_number = v;
  } else if ((address >= 0x4000) && (address <= 0x5FFF)) { // MBC1: RAM Bank Number - or - Upper Bits of ROM Bank Number (Write Only)
    assert(v <= 0x3); //a bit confused here, possibly come back to this 
    rom_ram_bank_number = v;
  } else if ((address >= 0x6000) && (address <= 0x7FFF)) { // MBC1: 6000-7FFF - ROM/RAM Mode Select (Write Only)
    assert((v == 0x00) || (v == 0x01));
    rom_ram_mode_select = v;
  } else if ((address >= 0xFEA0) && (address <= 0xFEFF)) {
    // unused memory range
  } else if ((address >= 0xFF00) && (address <= 0xFF7F)) { // IO Ports
    return write_io8(address, v);
  } else {
    uint8_t* memory = map_memory(address);
    *memory = v;
  }


}

static void write_memory16(uint16_t address, uint16_t value) {
  // Little endian: 0x1234 becomes (0x34, 0x12)  remember to re-study up on this
  write_memory8(address + 0, (value >> 0) & 0xFF);
  write_memory8(address + 1, (value >> 8) & 0xFF);
}

static uint16_t read_memory16(uint16_t address) {
  uint16_t value = 0x0000;
  value |= read_memory8(address + 0) << 0;
  value |= read_memory8(address + 1) << 8;
  return value;
}



typedef struct {

  // 3-0  -     -   -    Not used (always zero)
  uint8_t zero:4; // mask: 0x1 | 0x02 | 0x04 | 0x08

  // 4    cy    C   NC   Carry Flag
  uint8_t cy:1;   // mask: 0x10

  // 5    h     -   -    Half Carry Flag (BCD)
  uint8_t h:1;    // mask: 0x20

  // 6    n     -   -    Add/Sub-Flag (BCD)
  uint8_t n:1; // 0x40

  // 7    zf    Z   NZ   Zero Flag
  uint8_t zf:1;   // mask: 0x80

} Flags;

typedef struct {
  union { uint16_t af; struct { Flags f; uint8_t a; }; };
  union { uint16_t bc; struct { uint8_t c; uint8_t b; }; };
  union { uint16_t de; struct { uint8_t e; uint8_t d; }; };
  union { uint16_t hl; struct { uint8_t l; uint8_t h; }; };
  uint16_t sp;
  uint16_t pc;
} Registers;

static Registers cpu;


//                             0         1        2       3        4       5       6      7
uint16_t* registers16[4] = { &cpu.bc, &cpu.de, &cpu.hl, &cpu.sp };
uint8_t* registers8[8]   = { &cpu.b,  &cpu.c,  &cpu.d,  &cpu.e,  &cpu.h,  &cpu.l, NULL,  &cpu.a };

static uint16_t read_x16(uint8_t reg_index) {
  if (reg_index >= ARRAY_SIZE(registers16))  {
    assert(false);
    return 0x0000;
  }
  return *(registers16[reg_index]);
}

static void write_x16(uint8_t reg_index, uint16_t value) {
  if (reg_index >= ARRAY_SIZE(registers16))  {
    assert(false);
    return;
  }
  *(registers16[reg_index]) = value;
}

static uint8_t read_x8(uint8_t reg_index) {

  // Handle (HL)
  if (reg_index == 6) {
    return read_memory8(cpu.hl);
  }

  if (reg_index >= ARRAY_SIZE(registers8))  {
    assert(false);
    return 0x00;
  }
  return *(registers8[reg_index]);
}

static void write_x8(uint8_t reg_index, uint8_t value) {

  // Handle (HL)
  if (reg_index == 6) {
    write_memory8(cpu.hl, value);
    return;
  }

  if (reg_index >= ARRAY_SIZE(registers8))  {
    assert(false);
    return;
  }
  *(registers8[reg_index]) = value;
}




static void initialize_cpu() {

  // Initialize CPU registers
  cpu.af = 0x01B0;
  cpu.bc = 0x0013;
  cpu.de = 0x00D8;
  cpu.hl = 0x014D;
  cpu.sp = 0xFFFE;
  cpu.pc = 0x0100;

#if 1
  // CPU instruction test somehow ends up differently 
  cpu.af = 0x1180;
  cpu.bc = 0x0000;
  cpu.de = 0x0008;
  cpu.hl = 0x007C;
  cpu.sp = 0xFFFE;
  cpu.pc = 0x0100;
#endif

  // Init IO Ports
  write_io8(TIMA, 0x00);
  write_io8(TMA, 0x00);
  write_io8(TAC, 0x00);
  write_io8(NR10, 0x80);
  write_io8(NR11, 0xBF);
  write_io8(NR12, 0xF3);
  write_io8(NR14, 0xBF);
  write_io8(NR21, 0x3F);
  write_io8(NR22, 0x00);
  write_io8(NR24, 0xBF);
  write_io8(NR30, 0x7F);
  write_io8(NR31, 0xFF);
  write_io8(NR32, 0x9F);
  write_io8(NR33, 0xBF);
  write_io8(NR41, 0xFF);
  write_io8(NR42, 0x00);
  write_io8(NR43, 0x00);
  write_io8(NR30, 0xBF);
  write_io8(NR50, 0x77);
  write_io8(NR51, 0xF3);
  write_io8(NR52, 0xF1);
  write_io8(LCDC, 0x91);
  write_io8(SCY, 0x00);
  write_io8(SCX, 0x00);
  write_io8(LYC, 0x00);
  write_io8(BGP, 0xFC);
  write_io8(OBP0, 0xFF);
  write_io8(OBP1, 0xFF);
  write_io8(WY, 0x00);
  write_io8(WX, 0x00);
  ie = 0x00;

  ime = false;
}

static void disassemble();

//FIXME: Specific to MBC1
static void initialize_cartridge(const char* rom_file_path) {
    
  ram_enable = false;
  rom_bank_number = 0x00;
  rom_ram_bank_number = 0x00;
  rom_ram_mode_select = false;
  
  // Load ROM
  {
    FILE* f = fopen(rom_file_path, "rb");
    fseek(f, 0, SEEK_END);
    cartridge_rom_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    cartridge_rom_memory = malloc(cartridge_rom_size);
    fread(cartridge_rom_memory, 1, cartridge_rom_size, f);
    fclose(f);
    
    
#if DISASSEMBLE
    // Run a disassembler
    disassemble(); 
    exit(0);   
#endif
    
  }

  // Clear all memory
  memset(cartridge_ram_memory, 0x00, sizeof(cartridge_ram_memory)); //FIXME: Move into cartridge init
  
  // Find savegame (RAM file)
  //
  // zelda.hacks.gb => zelda.hacks.sav
  // zelda.gb => zelda.sav
  // zelda => zelda.sav
  //
  char* ram_extension = ".sav";
  char* ram_file_path = malloc(strlen(rom_file_path) + strlen(ram_extension) + 1);
  strcpy(ram_file_path, rom_file_path);
  char* dot = strrchr(ram_file_path, '.');
  if (dot != NULL) {
    *dot = '\0';
  }
  strcat(ram_file_path, ram_extension);
  printf("Expecting RAM at '%s'\n", ram_file_path);
  
  // Load RAM
  {
    FILE* f = fopen(ram_file_path, "rb");
    if (f != NULL) {
      int load_size = fread(cartridge_ram_memory, 1, sizeof(cartridge_ram_memory), f);
      fclose(f);
      printf("Savegame loaded (%d bytes)\n", load_size);
    }
  }
  free(ram_file_path);
}

bool gameboy_init(const char* rom_file_path) {
  printf("Loading '%s'\n", rom_file_path);

  // Clear gameboy_framebuffer to dark gray
  memset(gameboy_framebuffer, 0x33, sizeof(gameboy_framebuffer));

  // Initialize memory to safe values
  memset(vram_memory, 0x00, sizeof(vram_memory));
  memset(wram0_memory, 0x00, sizeof(wram0_memory));
  memset(wram1_memory, 0x00, sizeof(wram1_memory));
  memset(echo_memory, 0x00, sizeof(echo_memory));
  memset(oam_memory, 0x00, sizeof(oam_memory));
  memset(hram_memory, 0x00, sizeof(hram_memory));

  // Initialize CPU
  initialize_cpu();

  // Initialize cartridge
  initialize_cartridge(rom_file_path);

  // Return success
  return true;
}

static void push16(uint16_t value) {
  cpu.sp -= 2;
  write_memory16(cpu.sp, value);
}

static uint16_t pop16() {
  uint16_t value = read_memory16(cpu.sp);
  cpu.sp += 2;
  return value;
}

static void call(uint16_t address) {
  push16(cpu.pc);
  cpu.pc = address;
}

static uint8_t rr(uint8_t value) {
  bool carry = (value >> 0) & 1;
  value = value >> 1;
  if (cpu.f.cy) {
    value |= 0x80;
  }

  // Update CPU flags
  cpu.f.zf = (value == 0x00);
  cpu.f.n = 0;
  cpu.f.h = 0;
  cpu.f.cy = carry;

  return value;
}

static uint8_t rl(uint8_t value) {
  bool carry = (value >> 7) & 1;
  value = value << 1;
  if (cpu.f.cy) {
    value |= 0x01;
  }

  // Update CPU flags
  cpu.f.zf = (value == 0x00);
  cpu.f.n = 0;
  cpu.f.h = 0;
  cpu.f.cy = carry;

  return value;
}

static uint8_t rrc(uint8_t value) {
  bool carry = (value >> 0) & 1;
  value = value >> 1;
  if (carry) {
    value |= 0x80;
  }

  // Update CPU flags
  cpu.f.zf = (value == 0x00);
  cpu.f.n = 0;
  cpu.f.h = 0;
  cpu.f.cy = carry;

  return value;
}

static uint8_t rlc(uint8_t value) {
  bool carry = (value >> 7) & 1;
  value = value << 1;
  if (carry) {
    value |= 0x01;
  }

  // Update CPU flags
  cpu.f.zf = (value == 0x00);
  cpu.f.n = 0;
  cpu.f.h = 0;
  cpu.f.cy = carry;

  return value;
}

uint8_t and8(int a, int b) {
  uint8_t result = a & b;

  // Update CPU flags
  cpu.f.zf = (result == 0x00);
  cpu.f.n = 0;
  cpu.f.h = 1;
  cpu.f.cy = 0;

  return result;
}

uint8_t or8(int a, int b) {
  uint8_t result = a | b;

  // Update CPU flags
  cpu.f.zf = (result == 0x00);
  cpu.f.n = 0;
  cpu.f.h = 0;
  cpu.f.cy = 0;

  return result;
}

uint8_t xor8(int a, int b) {
  uint8_t result = a ^ b;

  // Update CPU flags
  cpu.f.zf = (result == 0x00);
  cpu.f.n = 0;
  cpu.f.h = 0;
  cpu.f.cy = 0;

  return result;
}

uint8_t add8(int a, int b, bool update_carry) {
  int result = a + b;
  int half_carry_result = (a & 0xF) + (b & 0xF);

  // Update CPU flags
  cpu.f.zf = ((uint8_t)result == 0x00);
  cpu.f.n = 0;
  cpu.f.h = (half_carry_result > 0xF);
  if (update_carry) {
    cpu.f.cy = (result > 0xFF);
  }

  return result;
}

uint8_t sub8(int a, int b, bool update_carry) {
  int result = a - b;
  int half_carry_result = (a & 0xF) - (b & 0xF);

  // Update CPU flags
  cpu.f.zf = ((uint8_t)result == 0x00);
  cpu.f.n = 1;
  cpu.f.h = (half_carry_result < 0x0);
  if (update_carry) {
    cpu.f.cy = (result < 0x00);
  }

  return result;
}

uint16_t add16(int a, int b) {
  int result = a + b;
  int half_carry_result = (a & 0xF00) + (b & 0xF00);

  // Update CPU flags
  //possibly wrong..
  cpu.f.n = 0;
  cpu.f.h = 0;
  cpu.f.cy = (result > 0xFFFF);

  return result;
}

static bool get_cc_result(uint8_t cc) {
  bool cc_result = false;
  switch(cc) {
  case 0: // NZ = not zero
    cc_result = !cpu.f.zf;
    break;
  case 1: // Z = zero
    cc_result = cpu.f.zf;
    break;
  case 2: // NC = not carry
    cc_result = !cpu.f.cy;
    break;
  case 3: // C = carry
    cc_result = cpu.f.cy;
    break;
  default:
    assert(false);
    break;
  }
  return cc_result;
}


const char* operands8[8] = { "b", "c", "d", "e", "h", "l", "[hl]", "a" };
const char* operands16[4] = { "bc", "de", "hl", "sp" };
const char* conditions[] = { "nz", "z",  "nc", "c" };

typedef struct {
  unsigned int length;
  unsigned int(*emulate)(uint8_t*); // unsigned int func(uint8_t*) {}
  void(*disassemble)(uint8_t*, char*); // void func(uint8_t*, char*) {}
} InstructionHandler;

#define DECODE_X8_X8() \
    uint8_t op2 = code[0] & 7; \
    const char* s_op2 = operands8[op2]; \
    uint8_t op1 = (code[0] >> 3) & 7; \
    const char* s_op1 = operands8[op1];

#define DECODE_X8() \
    uint8_t op1 = code[0] & 7; \
    const char* s_op1 = operands8[op1];

#define DECODE_A16() \
    uint16_t a16 =  (code[2] << 8) | code[1];

#define DECODE_X16() \
    uint8_t op1 = (code[0] >> 4) & 3; \
    const char* s_op1 = operands16[op1];

#define DECODE_X16_D16() \
    DECODE_X16() \
    uint16_t d16 = (code[2] << 8) | code[1];

#define DECODE_X8_X16() \
    DECODE_X8() \
    uint8_t op2 = (code[0] >> 4) & 3; \
    const char* s_op2 = operands16[op2];

#define DECODE_A8() \
    uint8_t a8 = code[1];

#define DECODE_D8() \
    uint8_t d8 = code[1];

#define DECODE_CC() \
  uint8_t cc = (code[0] >> 3) & 3;

#define DECODE_R8() \
  int8_t r8 = code[1];

#define DECODE_CC_R8() \
  DECODE_CC() \
  DECODE_R8()

#define DECODE_CC_X16() \
  DECODE_CC() \
  DECODE_X16()


static unsigned int emulate_nop(uint8_t* code) {
  return 4;
}

static void disassemble_nop(uint8_t* code, char* s) {
  sprintf(s, "nop ");
}

static unsigned int emulate_halt(uint8_t* code) {
  return 4;
}

static void disassemble_halt(uint8_t* code, char* s) {
  sprintf(s, "halt");
}

static unsigned int emulate_ld(uint8_t* code) {
  DECODE_X8_X8()
  uint8_t value = read_x8(op2);
  write_x8(op1, value);
  return 4; //FIXME: If operand (HL) is used, then 8 cycles
}

static void disassemble_ld(uint8_t* code, char* s) {
  DECODE_X8_X8()
  sprintf(s, "ld %s, %s", s_op1, s_op2);
}

static unsigned int emulate_ld_a16(uint8_t* code) {
  DECODE_A16()
  write_memory16(a16, cpu.sp);
  return 20;
}

static void disassemble_ld_a16(uint8_t* code, char* s) {
  DECODE_A16()
  sprintf(s, "ld [$%04X], sp", a16);
}

static unsigned int emulate_ld_x16_d16(uint8_t* code) {
  DECODE_X16_D16()
  write_x16(op1, d16);
  return 12;
}
static void disassemble_ld_x16_d16(uint8_t* code, char* s) {
  DECODE_X16_D16()
  sprintf(s, "ld %s, $%02X", s_op1, d16);
}

static unsigned int emulate_ldd(uint8_t* code) {
  write_memory8(cpu.hl, cpu.a);
  cpu.hl -= 1;
  return 8;
}

static void disassemble_ldd(uint8_t* code, char* s) {
  sprintf(s, "ld [hl-], a");
}

static unsigned int emulate_dec_x16(uint8_t* code) {
  DECODE_X16()
  uint16_t value = read_x16(op1);
  value -= 1;
  write_x16(op1, value);
  return 8;
}

static void disassemble_dec_x16(uint8_t* code, char* s) {
  DECODE_X16();
  sprintf(s, "dec %s", s_op1);
}

static unsigned int emulate_dec_x8(uint8_t* code) {
  DECODE_X8_X8()
  uint8_t value = read_x8(op1);
  value = sub8(value, 1, false);
  write_x8(op1, value);
  return 4;
}

static void disassemble_dec_x8(uint8_t* code, char* s) {
  DECODE_X8_X8();
  sprintf(s, "dec %s", s_op1);
}

static unsigned int emulate_inc_x16(uint8_t* code) {
  DECODE_X16()
  uint16_t value = read_x16(op1);
  value += 1;
  write_x16(op1, value);
  return 8;
}

static void disassemble_inc_x16(uint8_t* code, char* s) {
  DECODE_X16();
  sprintf(s, "inc %s", s_op1);
}

static unsigned int emulate_inc_x8(uint8_t* code) {
  DECODE_X8_X8()
  uint8_t value = read_x8(op1);
  value = add8(value, 1, false);
  write_x8(op1, value);
  return 4;
}

static void disassemble_inc_x8(uint8_t* code, char* s) {
  DECODE_X8_X8()
  sprintf(s, "inc %s", s_op1);
}

static unsigned int emulate_ldi(uint8_t* code) {
  write_memory8(cpu.hl, cpu.a);
  cpu.hl += 1;
  return 8;
}

static void disassemble_ldi(uint8_t* code, char*s) {
  sprintf(s, "ld [hl+], a");
}

static unsigned int emulate_ld_mem_02(uint8_t* code) {
  write_memory8(cpu.bc, cpu.a);
  return 8;
}

static void disassemble_ld_mem_02(uint8_t*code, char* s) {
  sprintf(s, "ld [bc], a");
}

static unsigned int emulate_ld_mem_12(uint8_t* code) {
  write_memory8(cpu.de, cpu.a);
  return 8;
}

static void disassemble_ld_mem_12(uint8_t* code, char* s) {
  sprintf(s, "ld [de], a");
}

static unsigned int emulate_ld_mem_0a(uint8_t* code) {
  cpu.a = read_memory16(cpu.bc);
  return 8;
}

static void disassemble_ld_mem_0a(uint8_t* code, char* s) {
  sprintf(s, "ld a, [bc]");
}

static unsigned int emulate_ld_mem_1a(uint8_t* code) {
  cpu.a = read_memory16(cpu.de);
  return 8;
}

static void disassemble_ld_mem_1a(uint8_t* code, char* s) {
  sprintf(s, "ld a, [de]");
}

static unsigned int emulate_ldi_2a(uint8_t* code) {
  cpu.a = read_memory8(cpu.hl);
  cpu.hl += 1;
  return 8;
}

static void disassemble_ldi_2a(uint8_t* code, char* s) {
  sprintf(s, "ld a, [hl+]");
}

static unsigned int emulate_ldd_3a(uint8_t* code) {
  cpu.a = read_memory8(cpu.hl);
  cpu.hl -= 1;
  return 8;
}

static void disassemble_ldd_3a(uint8_t* code, char* s) {
  sprintf(s, "ld a, [hl-]");
}

static unsigned int emulate_jr_cc_r8(uint8_t* code) {
  DECODE_CC_R8()
  if (get_cc_result(cc)) {
    cpu.pc += r8;
    return 12;
  }
  return 8;
}
static void disassemble_jr_cc_r8(uint8_t* code, char* s) {
  DECODE_CC_R8()
  sprintf(s, "jr %s, $%02X", conditions[cc], r8 & 0xFF);
}

static unsigned int emulate_jr_r8(uint8_t* code) {
  DECODE_R8()
  cpu.pc += r8;
  return 12;
}

static void disassemble_jr_r8(uint8_t* code, char* s) {
  DECODE_R8()
  
  if (r8 == 0x00) {
    sprintf(s, "jr , $%02X", r8 & 0xFF);
  } else {
    sprintf(s, "jr $%02X", r8 & 0xFF);
  }
}

static unsigned int emulate_jp_a16(uint8_t* code) {
  DECODE_A16()
  cpu.pc = a16;
  return 16;
}

static void disassemble_jp_a16(uint8_t* code, char* s) {
  DECODE_A16()
  sprintf(s, "jp $%X", a16);
}

static unsigned int emulate_jp_hl(uint8_t* code) {
  cpu.pc = cpu.hl;
  return 4;
}

static void disassemble_jp_hl(uint8_t* code, char* s) {
  sprintf(s, "jp hl"); //possibly incorrect
}

static unsigned int emulate_jp_cc(uint8_t* code) {
  DECODE_CC()
  DECODE_A16()
  if (get_cc_result(cc)) {
    cpu.pc = a16;
    return 16;
  } else {
    return 12;
  }
}

static void disassemble_jp_cc(uint8_t* code, char* s) {
  DECODE_CC()
  DECODE_A16()
  sprintf(s, "jp %s, $%X", conditions[cc], a16);
}

static unsigned int emulate_sub(uint8_t* code) {
  DECODE_X8()
  cpu.a = sub8(cpu.a, read_x8(op1), true);
  return 4;

}
static void disassemble_sub(uint8_t* code, char* s) {
  DECODE_X8()
  sprintf(s, "sub a, %s", s_op1);
}

static unsigned int emulate_sbc(uint8_t* code) {
  DECODE_X8()
  int carry = cpu.f.cy ? 1: 0;
  cpu.a = sub8(cpu.a, read_x8(op1) + carry, true);
  return 4;

}
static void disassemble_sbc(uint8_t* code, char* s) {
  DECODE_X8()
  sprintf(s, "sbc a, %s", s_op1);
}

static unsigned int emulate_add(uint8_t* code) {
  DECODE_X8()
  cpu.a = add8(cpu.a, read_x8(op1), true);
  return 4;

}
static void disassemble_add(uint8_t* code, char* s) {
  DECODE_X8()
  sprintf(s, "add a, %s", s_op1);
}

static unsigned int emulate_adc(uint8_t* code) {
  DECODE_X8()
  int carry = cpu.f.cy ? 1: 0;
  cpu.a = add8(cpu.a, read_x8(op1) + carry, true);
  return 4;
}

static void disassemble_adc(uint8_t* code, char* s) {
  DECODE_X8()
  sprintf(s, "add a, %s", s_op1);
}

static unsigned int emulate_adc_d8(uint8_t* code) {
  DECODE_D8()
  int carry = cpu.f.cy ? 1: 0;
  cpu.a = add8(cpu.a, d8 + carry, true);
  return 8;
}

static void disassemble_adc_d8(uint8_t* code, char* s) {
  DECODE_D8()
  sprintf(s, "adc $%02X", d8); 
}

static unsigned int emulate_sbc_d8(uint8_t* code) {
  DECODE_D8()
  int carry = cpu.f.cy ? 1: 0;
  cpu.a = sub8(cpu.a, d8 + carry, true);
  return 8;
}

static void disassemble_sbc_d8(uint8_t* code, char* s) {
  DECODE_D8()
  sprintf(s, "sbc $%02X", d8); 
}

static unsigned int emulate_rst(uint8_t* code) {
  //      hard coded here instead of using X8_X8 macro
  //       This would match http://www.z80.info/decoding.htm
  uint8_t target = ((code[0] >> 3) & 7) * 0x8;
  call(target);
  return 16;
}

static void disassemble_rst(uint8_t* code, char* s) {
  uint8_t target = ((code[0] >> 3) & 7) * 0x8;
  sprintf(s, "rst $%02X", target);
}

static unsigned int emulate_xor(uint8_t* code) {
  DECODE_X8()
  cpu.a = xor8(cpu.a, read_x8(op1));
  return 4;
}

static void disassemble_xor(uint8_t* code, char* s) {
  DECODE_X8()
  sprintf(s, "xor %s", s_op1);
}

static unsigned int emulate_and(uint8_t* code) {
  DECODE_X8()
  cpu.a = and8(cpu.a, read_x8(op1));
  return 4;
}

static void disassemble_and(uint8_t* code, char* s) {
  DECODE_X8()
  sprintf(s, "and %s", s_op1);
}

static unsigned int emulate_or(uint8_t* code) {
  DECODE_X8()
  cpu.a = or8(cpu.a, read_x8(op1));
  return 4;
}

static void disassemble_or(uint8_t* code, char* s) {
  DECODE_X8()
  sprintf(s, "or %s", s_op1);
}

static unsigned int emulate_e0_ldh(uint8_t* code) {
  DECODE_A8()
  write_memory8(0xFF00 + a8, cpu.a);
  return 12;
}

static void disassemble_e0_ldh(uint8_t* code, char* s) {
  DECODE_A8()
  sprintf(s, "ld [$FF%02X], a", a8);
}

static unsigned int emulate_f0_ldh(uint8_t* code) {
  DECODE_A8()
  cpu.a = read_memory8(0xFF00 + a8);
  return 12;
}

static void disassemble_f0_ldh(uint8_t* code, char* s) {
  DECODE_A8()
  sprintf(s, "ld a, [$FF%02X]", a8);
}

static unsigned int emulate_cp_d8(uint8_t* code) {
  DECODE_D8()
  sub8(cpu.a, d8, true);
  return 8;
}

static void disassemble_cp_d8(uint8_t* code, char* s) {
  DECODE_D8()
  sprintf(s, "cp $%02X", d8);
}

static unsigned int emulate_cp_x8(uint8_t* code) {
  DECODE_X8()
  uint8_t value = read_x8(op1);
  sub8(cpu.a, value, true);
  return 4;
}

static void disassemble_cp_x8(uint8_t* code, char* s) {
  DECODE_X8()
  sprintf(s, "cp %s", s_op1);
}

static unsigned int emulate_and_d8(uint8_t* code) {
  DECODE_D8()
  cpu.a = and8(cpu.a, d8);
  return 8;
}

static void disassemble_and_d8(uint8_t* code, char* s) {
  DECODE_D8()
  sprintf(s, "and $%02X", d8);
}

static unsigned int emulate_or_d8(uint8_t* code) {
  DECODE_D8()
  cpu.a = or8(cpu.a, d8);
  return 8;
}

static void disassemble_or_d8(uint8_t* code, char* s) {
  DECODE_D8()
  sprintf(s, "or $%02X", d8);
}


static unsigned int emulate_add_d8(uint8_t* code) {
  DECODE_D8()
  cpu.a = add8(cpu.a, d8, true);
  return 8;
}

static void disassemble_add_d8(uint8_t* code, char* s) {
  DECODE_D8()
  sprintf(s, "add $%02X", d8); 
}

static unsigned int emulate_add_sp(uint8_t* code) {
  DECODE_R8();

  uint16_t a = cpu.sp;
  uint16_t b = r8;
  
  cpu.sp = add16(a, b);

  return 16;
}

static void disassemble_add_sp(uint8_t* code, char* s) {
  DECODE_R8();
  sprintf(s, "add sp, $%02X", r8);
}

static unsigned int emulate_add_hl(uint8_t* code) {
  DECODE_X16();

  uint16_t a = cpu.hl;
  uint16_t b = read_x16(op1);
  
  cpu.hl = add16(a, b);

  return 8;
}

static void disassemble_add_hl(uint8_t* code, char* s) {
  DECODE_X16();
  sprintf(s, "add hl, %s", s_op1);
}

static unsigned int emulate_ld_f8(uint8_t* code){
  DECODE_R8()
  
  uint16_t a = cpu.sp;
  uint16_t b = r8;

  cpu.hl = add16(a, b);
  
  return 12;
}

static void disassemble_ld_f8(uint8_t* code, char* s){
  DECODE_R8()
  sprintf(s, "ld hl, sp+ $%02X", r8 & 0xFF);
}

static unsigned int emulate_sub_d8(uint8_t* code) {
  DECODE_D8()
  cpu.a = sub8(cpu.a, d8, true);
  return 8;
}

static void disassemble_sub_d8(uint8_t* code, char* s) {
  DECODE_D8()
  sprintf(s, "sub $%02X", d8);
}

static unsigned int emulate_xor_d8(uint8_t* code) {
  DECODE_D8()
  cpu.a = xor8(cpu.a, d8);
  return 8;
}

static void disassemble_xor_d8(uint8_t* code, char* s) {
  DECODE_D8()
  sprintf(s, "xor $%02X", d8);
}

#define DECODE_CB() \
  uint8_t operation = code[1]; \
  uint8_t operation_index = (operation >> 3) & 7; \
  uint8_t op1 = operation & 7; \
  const char* s_op1 = operands8[op1];

static unsigned int emulate_cb_prefix(uint8_t* code) {
    DECODE_CB()

    uint8_t value = read_x8(op1);

    if ((operation >= 0x00) && (operation <= 0x07)) {
      // RLC (Rotate Left Circular)

      value = rlc(value);

    } else if ((operation >= 0x08) && (operation <= 0x0F)) {
      // RRC (Rotate Right Circular)

      value = rrc(value);

    } else if ((operation >= 0x10) && (operation <= 0x17)) {
      // RL (Rotate Left through carry)

      value = rl(value);

    } else if ((operation >= 0x18) && (operation <= 0x1F)) {
      // RR (Rotate Right through carry)

      value = rr(value);

    } else if ((operation >= 0x20) && (operation <= 0x27)) {
      // SLA

      bool carry = (value >> 7) & 1;
      value = value << 1;

      // Update CPU flags
      cpu.f.zf = (value == 0x00);
      cpu.f.n = 0;
      cpu.f.h = 0;
      cpu.f.cy = carry;
    
    } else if ((operation >= 0x28) && (operation <= 0x2F)) {
      // SRA

      bool carry = (value >> 0) & 1;
      value = (value & 0x80) | (value >> 1);

      // Update CPU flags
      cpu.f.zf = (value == 0x00);
      cpu.f.n = 0;
      cpu.f.h = 0;
      cpu.f.cy = carry;

    } else if ((operation >= 0x30) && (operation <= 0x37)) {
      // SWAP

      uint8_t high_nibble = (value >> 4) & 0xF;
      uint8_t low_nibble  = (value >> 0) & 0xF;
      value = (low_nibble << 4) | high_nibble;

      // Update CPU flags
      cpu.f.zf = (value == 0x00);
      cpu.f.n = 0;
      cpu.f.h = 0;
      cpu.f.cy = 0;

    } else if ((operation >= 0x37) && (operation <= 0x3F)) {
      // SRL

      bool carry = (value >> 0) & 1;
      value = value >> 1;

      // Update CPU flags
      cpu.f.zf = (value == 0x00);
      cpu.f.n = 0;
      cpu.f.h = 0;
      cpu.f.cy = carry;

    } else if ((operation >= 0x40) && (operation <= 0x7F)) {
      // BIT
            
      bool bit = (value >> operation_index) & 1;

      // Update CPU flags
      cpu.f.zf = bit;
      cpu.f.n = 0;
      cpu.f.h = 1;
      
    } else if ((operation >= 0x80) && (operation <= 0xBF)) {
      // RES

      value &= ~(1 << operation_index);

    } else if ((operation >= 0xC0) && (operation <= 0xFF)) {
      // SET

      value |= 1 << operation_index;
    }

    write_x8(op1, value);
    return 8;
}

static void disassemble_cb_prefix(uint8_t* code, char* s) {
    DECODE_CB()
    const char* operations_0x_3x[] = { "rlc", "rrc", "rl", "rr", "sla", "sra", "swap", "srl" };
    if ((operation >= 0x00) && (operation <= 0x3F)) {
      sprintf(s, "%s %s", operations_0x_3x[operation_index], s_op1);
    } else if ((operation >= 0x40) && (operation <= 0x7F)) {
      sprintf(s, "bit %u, %s", operation_index, s_op1);
    } else if ((operation >= 0x80) && (operation <= 0xBF)) {
      sprintf(s, "res $%02X, %s", operation_index, s_op1);
    } else if ((operation >= 0xC0) && (operation <= 0xFF)) {
      sprintf(s, "set %u, %s", operation_index, s_op1);
    }
}

static unsigned int emulate_call(uint8_t* code) {
  DECODE_A16()
  call(a16);
  return 24;
}

static void disassemble_call(uint8_t* code, char* s) {
  DECODE_A16()
  sprintf(s, "call $%X", a16);
}

static unsigned int emulate_call_cc_a16(uint8_t* code) {
  DECODE_CC()
  DECODE_A16()
  if(get_cc_result(cc)) {
    call(a16);
    return 24;
  } else {
    return 12;
  }
}

static void disassemble_call_cc_a16(uint8_t* code, char* s) {
  DECODE_CC()
  DECODE_A16()
  sprintf(s, "call %s, $%04X", conditions[cc], a16);

}

static unsigned int emulate_push_x16(uint8_t* code) {
  DECODE_X16()
  uint16_t value = read_x16(op1);
  push16(value);
  return 16;
}

static void disassemble_push_x16(uint8_t* code, char* s) {
  DECODE_X16()
  sprintf(s, "push %s", s_op1);
}

static unsigned int emulate_push_af(uint8_t* code) {
  push16(cpu.af);
  return 16;
}

static void disassemble_push_af(uint8_t* code, char* s) {
  sprintf(s, "push af");
}

static unsigned int emulate_ret(uint8_t* code) {
  cpu.pc = pop16();
  return 16;
}

static void disassemble_ret(uint8_t* code, char* s) {
  sprintf(s, "ret "); 
}

static unsigned int emulate_ret_cc(uint8_t* code) {
  DECODE_CC()
  if (get_cc_result(cc)) {
    cpu.pc = pop16();
    return 20;
  } else {
    return 8;
  }
}

static unsigned int emulate_reti(uint8_t* code) {
  ime = true;
  cpu.pc = pop16();
  return 16;
}

static void disassemble_reti(uint8_t* code, char* s) {
  sprintf(s, "RETI");
}

static void disassemble_ret_cc(uint8_t* code, char* s) {
  DECODE_CC()
  sprintf(s, "ret %s", conditions[cc]);
}

static unsigned int emulate_pop_x16(uint8_t* code) {
  DECODE_X16()
  uint16_t value = pop16();
  write_x16(op1, value);
  return 12;
}

static void disassemble_pop_x16(uint8_t* code, char* s) {
  DECODE_X16()
  sprintf(s, "pop %s", s_op1);
}

static unsigned int emulate_pop_af(uint8_t* code) {
  cpu.af = pop16();
  return 12;
}

static void disassemble_pop_af(uint8_t* code, char* s) {
  sprintf(s, "pop af");
}

static unsigned int emulate_ld_x8_d8(uint8_t* code) {
  DECODE_D8()
  DECODE_X8_X8()
  write_x8(op1, d8);
  return 8;
}
static void disassemble_ld_x8_d8(uint8_t* code, char* s) {
  DECODE_D8()
  DECODE_X8_X8()  //using this macro for a test
  sprintf(s, "ld %s, $%02X", s_op1, d8);
}

static unsigned int emulate_ld_ea(uint8_t* code) {
  DECODE_A16()
  write_memory8(a16, cpu.a);
  return 16;
}

static void disassemble_ld_ea(uint8_t* code, char* s) {
  DECODE_A16()
  sprintf(s, "ld [$%X], a", a16);
}

static unsigned int emulate_ld_e2(uint8_t* code) {
  
  write_memory8(0xFF00 + cpu.c, cpu.a);
  return 8;
}

static void disassemble_ld_e2(uint8_t* code, char* s) {
  
  sprintf(s, "ld [c], a");
}

static unsigned int emulate_ld_f2(uint8_t* code) {
  
  cpu.a = read_memory8(0xFF00 + cpu.c);
  return 8;
}

static void disassemble_ld_f2(uint8_t* code, char* s) {
 
  sprintf(s, "ld a, [c]");
}

static unsigned int emulate_ld_fa(uint8_t* code) {
  DECODE_A16()
  cpu.a = read_memory8(a16);
  return 16;
}

static void disassemble_ld_fa(uint8_t* code, char* s) {
  DECODE_A16()
  sprintf(s, "ld a, [$%02X]", a16);
}

static unsigned int emulate_ld_f9(uint8_t* code) {
  cpu.sp = cpu.hl;
  return 8;
}

static void disassemble_ld_f9(uint8_t* code, char* s) {
  sprintf(s, "ld sp, hl");
}

static unsigned int emulate_ei(uint8_t* code) {
  ime = true;
  return 4;
}

static void disassemble_ei(uint8_t* code, char* s) {
  sprintf(s, "ei "); 
}

static unsigned int emulate_di(uint8_t* code) {
  ime = false;
  return 4;
}

static void disassemble_di(uint8_t* code, char* s) {
  sprintf(s, "di "); 
}

static unsigned int emulate_undefined(uint8_t* code) {
  cpu.pc -= 1;
  return 0;
}

static void disassemble_undefined(uint8_t* code, char* s) {
  sprintf(s, "UNDEFINED_%02X", code[0]);
}

static unsigned int emulate_rra(uint8_t* code) {
  cpu.a = rr(cpu.a);

  // Fixup the zero flag (rrc sets it according to result)
  cpu.f.zf = 0;

  return 4;
}

static void disassemble_rra(uint8_t* code, char* s) {
  sprintf(s, "rr a"); 
}

static unsigned int emulate_rrca(uint8_t* code) {
  cpu.a = rrc(cpu.a);

  // zero flag is set by rrc according to the result
  cpu.f.zf = 0;

  return 4;
}

static void disassemble_rrca(uint8_t* code, char* s) {
  sprintf(s, "rrc a"); 
}

static unsigned int emulate_rla(uint8_t* code) {
  cpu.a = rl(cpu.a);

  
  cpu.f.zf = 0;

  return 4;
}

static void disassemble_rla(uint8_t* code, char* s) {
  sprintf(s, "rl a"); 
}

static unsigned int emulate_rlca(uint8_t* code) {
  cpu.a = rlc(cpu.a);

 //setting 0 flag 
  cpu.f.zf = 0;

  return 4;
}

static void disassemble_rlca(uint8_t* code, char* s) {
  sprintf(s, "rlc a"); 
}

static unsigned int emulate_stop_0(uint8_t* code) {
  return 4;
}

static void disassemble_stop_0(uint8_t* code, char* s) {
  sprintf(s, "stop 0");
}


static unsigned int emulate_cpl(uint8_t* code) {
    
  cpu.a ^= 0xFF;
    
  // Update CPU flags
  cpu.f.n = 1;
  cpu.f.h = 1;
      
  return 4;
}

static void disassemble_cpl(uint8_t* code, char* s) {
  sprintf(s, "cpl");
}

static unsigned int emulate_scf(uint8_t* code) {
    
  // Update CPU flags
  cpu.f.n = 0;
  cpu.f.h = 0;
  cpu.f.cy = 1;
  
  return 4;   
}

static void disassemble_scf(uint8_t* code, char* s) {
  sprintf(s, "scf");
}

static unsigned int emulate_ccf(uint8_t* code) {

  // Update CPU flags
  cpu.f.n = 0;
  cpu.f.h = 0;
  cpu.f.cy = !cpu.f.cy;
  
  return 4;
}

static void disassemble_ccf(uint8_t* code, char* s) {
  sprintf(s, "ccf");
}

static unsigned int emulate_daa(uint8_t* code) {

  // do not understand this section very well
  if (cpu.f.n) {
    if (cpu.f.h)  { cpu.a += 0xFA; }
    if (cpu.f.cy) { cpu.a += 0xA0; }
  } else {
    int a = cpu.a;
    if ((a & 0x00F) > 0x09 || cpu.f.h) {
       a += 0x06;
    }
    if ((a & 0x1F0) > 0x90 || cpu.f.cy) {
      a += 0x60; 
      cpu.f.cy = 1;
    } else {
      cpu.f.cy = 0;
    }
    cpu.a = a;
  }
  cpu.f.h = 0;
  cpu.f.zf = (cpu.a != 0x00);
  
  return 4;
}

static void disassemble_daa(uint8_t* code, char* s) {
  sprintf(s, "daa");
}

#define INSTRUCTION_HANDLER(name, length) InstructionHandler handle_ ## name = { (length), emulate_ ## name, disassemble_ ## name }
INSTRUCTION_HANDLER(stop_0,      2);
INSTRUCTION_HANDLER(rra,         1);
INSTRUCTION_HANDLER(rla,         1);
INSTRUCTION_HANDLER(jr_cc_r8,    2);
INSTRUCTION_HANDLER(cp_d8,       2);
INSTRUCTION_HANDLER(cp_x8,       1);
INSTRUCTION_HANDLER(and_d8,      2);
INSTRUCTION_HANDLER(add_d8,      2);
INSTRUCTION_HANDLER(sub_d8,      2);
INSTRUCTION_HANDLER(or_d8,       2);
INSTRUCTION_HANDLER(xor_d8,      2);
INSTRUCTION_HANDLER(dec_x8,      1);
INSTRUCTION_HANDLER(dec_x16,     1);
INSTRUCTION_HANDLER(inc_x16,     1);
INSTRUCTION_HANDLER(inc_x8,      1);
INSTRUCTION_HANDLER(nop,         1);
INSTRUCTION_HANDLER(call,        3);
INSTRUCTION_HANDLER(call_cc_a16, 3);
INSTRUCTION_HANDLER(push_x16,    1);
INSTRUCTION_HANDLER(pop_x16,     1);
INSTRUCTION_HANDLER(push_af,     1);
INSTRUCTION_HANDLER(pop_af,      1);
INSTRUCTION_HANDLER(halt,        1);
INSTRUCTION_HANDLER(ld,          1);
INSTRUCTION_HANDLER(ldi,         1);
INSTRUCTION_HANDLER(ldd,         1);
INSTRUCTION_HANDLER(ld_mem_02,   1);
INSTRUCTION_HANDLER(ld_mem_12,   1);
INSTRUCTION_HANDLER(ld_mem_0a,   1);
INSTRUCTION_HANDLER(ld_mem_1a,   1);
INSTRUCTION_HANDLER(ldi_2a,      1);
INSTRUCTION_HANDLER(ldd_3a,      1);
INSTRUCTION_HANDLER(ld_x8_d8,    2);
INSTRUCTION_HANDLER(ld_e2,       1);
INSTRUCTION_HANDLER(ld_f2,       1);
INSTRUCTION_HANDLER(jp_a16,      3);
INSTRUCTION_HANDLER(jp_hl,       1);
INSTRUCTION_HANDLER(jp_cc,       3);
INSTRUCTION_HANDLER(jr_r8,       2);
INSTRUCTION_HANDLER(add_hl,      1);
INSTRUCTION_HANDLER(sub,         1);
INSTRUCTION_HANDLER(sbc,         1);
INSTRUCTION_HANDLER(add,         1);
INSTRUCTION_HANDLER(adc,         1);
INSTRUCTION_HANDLER(adc_d8,      2);
INSTRUCTION_HANDLER(xor,         1);
INSTRUCTION_HANDLER(sbc_d8,      2);
INSTRUCTION_HANDLER(or,          1);
INSTRUCTION_HANDLER(and,         1);
INSTRUCTION_HANDLER(e0_ldh,      2);
INSTRUCTION_HANDLER(ld_ea,       3);
INSTRUCTION_HANDLER(ld_fa,       3);
INSTRUCTION_HANDLER(ld_f8,       2);
INSTRUCTION_HANDLER(ld_f9,       1);
INSTRUCTION_HANDLER(f0_ldh,      2);
INSTRUCTION_HANDLER(cb_prefix,   2);
INSTRUCTION_HANDLER(ret,         1);
INSTRUCTION_HANDLER(ret_cc,      1);
INSTRUCTION_HANDLER(reti,        1);
INSTRUCTION_HANDLER(ld_x16_d16,  3);
INSTRUCTION_HANDLER(undefined,   1);
INSTRUCTION_HANDLER(ei,          1);
INSTRUCTION_HANDLER(di,          1);
INSTRUCTION_HANDLER(rst,         1);
INSTRUCTION_HANDLER(rrca,        1);
INSTRUCTION_HANDLER(rlca,        1);
INSTRUCTION_HANDLER(ld_a16,      3);
INSTRUCTION_HANDLER(add_sp,      2);
INSTRUCTION_HANDLER(cpl,         1);
INSTRUCTION_HANDLER(scf,         1);
INSTRUCTION_HANDLER(ccf,         1);
INSTRUCTION_HANDLER(daa,         1);

static InstructionHandler* cpu_decode(uint8_t opcode) {
  if (opcode == 0x00) {
    return &handle_nop;
  } else if ((opcode == 0x01) || (opcode == 0x11) || (opcode == 0x21) || (opcode == 0x31)) {
    return &handle_ld_x16_d16;
  } else if (opcode == 0x02) {
    return &handle_ld_mem_02;
  } else if ((opcode == 0x03) || (opcode == 0x13) || (opcode == 0x23) || (opcode == 0x33)) {
    return &handle_inc_x16;
  } else if ((opcode == 0x04) || (opcode == 0x14) || (opcode == 0x24) || (opcode == 0x34) || (opcode == 0x0C) || (opcode == 0x1C) || (opcode == 0x2C) || (opcode == 0x3C)) {
    return &handle_inc_x8;
  } else if ((opcode == 0x05) || (opcode == 0x15) || (opcode == 0x25) || (opcode == 0x35) || (opcode == 0x0D) || (opcode == 0x1D) || (opcode == 0x2D) || (opcode == 0x3D)) {
    return &handle_dec_x8;
  } else if ((opcode == 0x06) || (opcode == 0x16) || (opcode == 0x26) || (opcode == 0x36) || (opcode == 0x0E) || (opcode == 0x1E) || (opcode == 0x2E) || (opcode == 0x3E)) {
    return &handle_ld_x8_d8;      
  } else if (opcode == 0x07) {
    return &handle_rlca;
  } else if (opcode == 0x08) {
    return &handle_ld_a16;
  } else if ((opcode == 0x09) || (opcode == 0x19) || (opcode == 0x29) || (opcode == 0x39)) {
    return &handle_add_hl;
  } else if (opcode == 0x0A) {
    return &handle_ld_mem_0a;
  } else if ((opcode == 0x0B) || (opcode == 0x1B) || (opcode == 0x2B) || (opcode == 0x3B)) {
    return &handle_dec_x16;
  } else if (opcode == 0x0F) {
    return &handle_rrca;
  } else if (opcode == 0x10) {
    return &handle_stop_0;
  } else if (opcode == 0x12) {
    return &handle_ld_mem_12;
  } else if (opcode == 0x17) {
    return &handle_rla;
  } else if (opcode == 0x18) {
    return &handle_jr_r8;
  } else if (opcode == 0x1A) {
    return &handle_ld_mem_1a;
  } else if (opcode == 0x1F) {
    return &handle_rra;
  } else if ((opcode == 0x20) || (opcode == 0x28) || (opcode == 0x30) || (opcode == 0x38)) {
    return &handle_jr_cc_r8;
  } else if (opcode == 0x22) {
    return &handle_ldi;
  } else if (opcode == 0x27) {
    return &handle_daa;
  } else if (opcode == 0x2A) {
    return &handle_ldi_2a;
  } else if (opcode == 0x2F) {
    return &handle_cpl;
  } else if (opcode == 0x32) {
    return &handle_ldd;
  } else if (opcode == 0x37) {
    return &handle_scf;
  } else if (opcode == 0x3A) {
    return &handle_ldd_3a;
  } else if (opcode == 0x3F) {
    return &handle_ccf;
  } else if ((opcode >= 0x40) && (opcode <= 0x7F)) {
    if (opcode == 0x76) {
       return &handle_halt;
    } else {
      return &handle_ld;
    }
  } else if ((opcode >= 0x80) && (opcode <= 0x87)) {
    return &handle_add;
  } else if ((opcode >= 0x88) && (opcode <= 0x8F)) {
    return &handle_adc;
  } else if ((opcode >= 0x90) && (opcode <= 0x97)) {
    return &handle_sub;
  } else if ((opcode >= 0x98) && (opcode <= 0x9F)) {
    return &handle_sbc;
  } else if ((opcode >= 0xA8) && (opcode <= 0xAF)) {
    return &handle_xor;
  } else if ((opcode >= 0xA0) && (opcode <= 0xA7)) {
    return &handle_and;
  } else if ((opcode >= 0xB0) && (opcode <= 0xB7)) {
    return &handle_or;
  } else if ((opcode >= 0xB8) && (opcode <= 0xBF)) {
    return &handle_cp_x8;
  } else if ((opcode == 0xC0) || (opcode == 0xC8) ||(opcode == 0xD0) || (opcode == 0xD8)) {
    return &handle_ret_cc;
  } else if ((opcode == 0xC1) || (opcode == 0xD1) || (opcode == 0xE1)) {
    return &handle_pop_x16;
  } else if ((opcode == 0xC2) || (opcode == 0xCA) || (opcode == 0xD2) || (opcode == 0xDA)) {
    return &handle_jp_cc;
  } else if (opcode == 0xC3) {
    return &handle_jp_a16;
  } else if ((opcode == 0xC4) || (opcode == 0xCC) || (opcode == 0xD4) || (opcode == 0xDC)) {
    return &handle_call_cc_a16;
  } else if ((opcode == 0xC5) || (opcode == 0xD5) || (opcode == 0xE5)) {
    return &handle_push_x16;
  } else if (opcode == 0xC6) {
    return &handle_add_d8;
  } else if ((opcode == 0xC7) || (opcode == 0xD7) || (opcode == 0xE7) || (opcode == 0xF7) || (opcode == 0xCF) || (opcode == 0xDF) || (opcode == 0xEF) || (opcode == 0xFF)) {
    return &handle_rst;
  } else if (opcode == 0xC9) {
    return &handle_ret;
  } else if (opcode == 0xCB) {
    return &handle_cb_prefix;
  } else if (opcode == 0xCD) {
    return &handle_call;
  } else if (opcode == 0xCE) {
    return &handle_adc_d8;
  } else if ((opcode == 0xD3) || (opcode == 0xDB) || (opcode == 0xDD) || (opcode == 0xE3) || (opcode == 0xE4) || ((opcode >= 0xEB) && (opcode <=0xED)) || (opcode == 0xF4) || (opcode == 0xFC) ||(opcode == 0xFD)) {
    return &handle_undefined;
  } else if (opcode == 0xD6) {
    return &handle_sub_d8;
  } else if (opcode == 0xD9) {
    return &handle_reti;
  } else if (opcode == 0xDE) {
    return &handle_sbc_d8;
  } else if (opcode == 0xE0) {
    return &handle_e0_ldh;
  } else if (opcode == 0xE2) {
    return &handle_ld_e2;
  } else if (opcode == 0xE6) {
    return &handle_and_d8;
  } else if (opcode == 0xE8) {
    return &handle_add_sp;
  } else if (opcode == 0xE9) {
    return &handle_jp_hl;
  } else if (opcode == 0xEA) {
    return &handle_ld_ea;
  } else if (opcode == 0xEE) {
    return &handle_xor_d8;
  } else if (opcode == 0xF0) {
    return &handle_f0_ldh;
  } else if (opcode == 0xF1) {
    return &handle_pop_af;
  } else if (opcode == 0xF2) {
    return &handle_ld_f2;
  } else if (opcode == 0xF3) {
    return &handle_di;
  } else if (opcode == 0xF5) {
    return &handle_push_af;
  } else if (opcode == 0xF6) {
    return &handle_or_d8;
  } else if (opcode == 0xF8) {
    return &handle_ld_f8;
  } else if (opcode == 0xF9) {
    return &handle_ld_f9;
  } else if (opcode == 0xFA) {
    return &handle_ld_fa;
  } else if (opcode == 0xFB) {
    return &handle_ei;
  } else if (opcode == 0xFE) {
    return &handle_cp_d8;
  } else {
    fprintf(stderr, "Unknown instruction 0x%02X\n", opcode);
    assert(false);
  }
  return NULL;
}

#define INTERRUPTS_VBLANK    (1 << 0)
#define INTERRUPTS_LCDSTAT   (1 << 1)
#define INTERRUPTS_TIMER     (1 << 2)
#define INTERRUPTS_SERIAL    (1 << 3)
#define INTERRUPTS_JOYPAD    (1 << 4)

void invoke_interrupt(uint16_t address) {
  ime = false;
  call(address);
}


static int cpu_step(int mcycles) {

  while(mcycles > 0) {

    //FIXME: Make this part of register access
    cpu.f.zero = 0;

    if (ime)  {
 
      // If this interrupt is enabled AND it's also triggering now
      uint8_t _if = read_io8(IF);
      uint8_t irq = ie & _if;

      //interrupts

      if (irq & INTERRUPTS_VBLANK) {
        invoke_interrupt(0x40);
        _if &= ~INTERRUPTS_VBLANK;
      } else if (irq & INTERRUPTS_LCDSTAT) {
        invoke_interrupt(0x48);
        _if &= ~INTERRUPTS_LCDSTAT;
      } if (irq & INTERRUPTS_TIMER) {
        
        
        

        //rememebr to do this 
        //assert(false); // Test
        
        
        
        invoke_interrupt(0x50);
        _if &= ~INTERRUPTS_TIMER;
      } if (irq & INTERRUPTS_SERIAL) {
        assert(false); // Test .. not ready
        invoke_interrupt(0x58);
        _if &= ~INTERRUPTS_SERIAL;
      } if (irq & INTERRUPTS_JOYPAD) {          
        assert(false); // Test
        invoke_interrupt(0x60);
        _if &= ~INTERRUPTS_JOYPAD;
      } else {
        // No interrupt triggered, keep IME enabled
      }
   
      write_io8(IF, _if);
    }

#if 0
    // Debug markers
    if (cpu.pc == 0x3D0) {
      printf("IMPORTANT 3D0\n"); // grep IMPORT
    }
    static unsigned int step = 0;
    if (step % 100 == 0) {
      printf("Step %d\n", step);
    }
    step++;
#endif

#if DEBUG
    // Debug print the current CPU state
    printf("A: %02X ", cpu.a);
    printf("F: %02X ", cpu.f);
    printf("B: %02X ", cpu.b);
    printf("C: %02X ", cpu.c);
    printf("D: %02X ", cpu.d);
    printf("E: %02X ", cpu.e);
    printf("H: %02X ", cpu.h);
    printf("L: %02X ", cpu.l);
    printf("SP: %04X ", cpu.sp);
    unsigned int rom_bank = get_rom_bank_number(cpu.pc);
    printf("PC: %02X:%04X ", rom_bank, cpu.pc);
    printf("| ");
#endif

    // Get instruction
    uint8_t opcode = read_memory8(cpu.pc);

    // Figure out what instruction this is
    InstructionHandler* handler = cpu_decode(opcode);
    assert(handler != NULL);

    // Read rest of instruction
    uint8_t code[32];
    code[0] = opcode;
    for(int i = 1; i < handler->length; i++) {
      code[i] = read_memory8(cpu.pc + i);
    }

#if DEBUG
    // Print instruction bytes
    for(int i = 0; i < handler->length; i++) {
      printf("%02X", code[i]);
    }
    printf(": ");

    // Print disassembly
    char buffer[32];
    handler->disassemble(code, buffer);
    printf("%s", buffer);
    printf("\n");
    fflush(stdout);
#endif

    // Move PC first, so we don't have to adjust jmp etc.





    cpu.pc += handler->length;

    // Emulate instruction
    unsigned int cycles = handler->emulate(code);

    // Spend time
    mcycles -= cycles;
  }

  return mcycles;
}


static uint8_t u2_to_u8(unsigned int v) { 
  // ab => abababab
  // 00 => 00000000 = 0%
  // 01 => 01010101 = 33%
  // 10 => 10101010 = 66%
  // 11 => 11111111 = 100%
  assert(v <= 3);
  return (v << 6) | (v << 4) | (v << 2) | v;
}
static unsigned int u8_to_u2(uint8_t v) {
  return v >> 6;
}

uint8_t get_palette_color(uint16_t palette_register, unsigned int palette_index) {
 
  // Read palette and get color
  // Bit 7-6 - Shade for Color Number 3
  // Bit 5-4 - Shade for Color Number 2
  // Bit 3-2 - Shade for Color Number 1
  // Bit 1-0 - Shade for Color Number 0 
  assert(palette_index <= 3);
  uint8_t palette = read_io8(palette_register);
  uint8_t palette_color = (palette >> (2 * palette_index)) & 0x3;
  
  // Remap color (0 = white, 3 = black)
  return 3 - palette_color;

}
      
static void draw_tile_line(uint8_t* image, unsigned w, unsigned int h, int x, int y, uint16_t address, int __dx, int dy, uint16_t palette_register, bool flip_x, bool flip_y) {

  //FIXME: Unsupported    
  assert(__dx == 0);
    
  // Check tile bounds
  if (dy < 0) { return; }
  if (dy >= 8) { return; }

  // Handle mirroring
  int tile_y = flip_y ? (7 - dy) : dy;
    
  // Read tile row from memory
  unsigned int row_offset = tile_y * 2;
  uint8_t low_byte = read_memory8(address + row_offset + 0);
  uint8_t high_byte = read_memory8(address + row_offset + 1);
    
  for(unsigned int dx = 0; dx < 8; dx++) {
        
    // Handle mirroring
    int tile_x = flip_x ? dx : (7 - dx);
       
    // Construct palette index
    unsigned int low_bit = (low_byte >> tile_x) & 1;
    unsigned int high_bit = (high_byte >> tile_x) & 1;
    unsigned int palette_index = (high_bit << 1) | low_bit;
    
    // Skip color 0:  
    // - For background: already drawn
    // - For sprites: transparent
    if (palette_index == 0) {
      continue;
    }    
      
    // Get coordinates in image
    int image_x = x + dx;
    int image_y = y;
      
    // Check bounds
    if (image_x < 0) { continue; }
    if (image_y < 0) { continue; }
    if (image_x >= w) { continue; }
    if (image_y >= h) { continue; }
     
    // Lookup color from palette
    uint8_t color = get_palette_color(palette_register, palette_index);  
      
    // Write color to image buffer
    unsigned int image_pixel_index = image_y * w + image_x;
    image[image_pixel_index] = u2_to_u8(color);
      
  }
  
}

static void draw_tile(uint8_t* image, unsigned w, unsigned int h, int x, int y, uint16_t address, uint16_t palette_register, bool flip_x, bool flip_y) {
  // Each tile is sized 8x8 pixels and has a color depth of 4 colors/gray shades
  for(unsigned int dy = 0; dy < 8; dy++) {
    draw_tile_line(image, w, h, x, y + dy, address, 0, dy, palette_register, flip_x, flip_y);
  }
}

static void draw_background_line(uint8_t* image, unsigned int w, unsigned h, int x, int y, uint16_t map_address, bool bg_tiles, uint8_t dx, uint8_t dy) {
  unsigned int tile_row = dy / 8;
  unsigned int tile_line = dy % 8;
  
 for(unsigned int screen_x = 0; screen_x < 32*8; screen_x++) {  
     
    unsigned int tile_col = screen_x / 8;
                    
    // Read tile index from background map memory
    unsigned int block_index = tile_row * 32 + tile_col;
    int tile_index = read_memory8(map_address + block_index);
      
    // Use specified tiles
    unsigned int tile_address = bg_tiles ? 0x9000 : 0x8000;
    if (bg_tiles) {
      tile_index = (int8_t)tile_index;
    }

    
    //             image, w, h,                   x, y
    draw_tile_line(image, w, h, x + 8*tile_col - dx, y, tile_address + tile_index * 0x10, 0, tile_line, BGP, false, false);
    draw_tile_line(image, w, h, x + 8*tile_col + 32*8 - dx, y, tile_address + tile_index * 0x10, 0, tile_line, BGP, false, false);

  }
}

static void draw_sprites_line(uint8_t* image, unsigned int w, unsigned int h, int x, int y, int dy, bool background) {
  
  uint8_t lcdc = read_io8(LCDC);

  //  Bit 2 - OBJ (Sprite) Size              (0=8x8, 1=8x16)  
  bool tall_sprites = lcdc & (1 << 2);
  
  //   Bit 1 - OBJ (Sprite) Display Enable    (0=Off, 1=On)
  bool sprites_enabled = lcdc & (1 << 1);
  
  if (!sprites_enabled) {
    return;
  }
      
  for(unsigned int sprite_index = 0; sprite_index < 40; sprite_index++) {

    uint16_t sprite_address = 0xFE00 + sprite_index * 4;
    int sprite_y = read_memory8(sprite_address + 0) - 16;
    int sprite_x = read_memory8(sprite_address + 1) - 8;
    uint8_t tile_index = read_memory8(sprite_address + 2);
    uint8_t flags = read_memory8(sprite_address + 3);
      
    // Bit7   OBJ-to-BG Priority (0=OBJ Above BG, 1=OBJ Behind BG color 1-3)
    // (Used for both BG and Window. BG color 0 is always behind OBJ)
    bool sprite_background = flags & (1 << 7);
    if (sprite_background != background) {
      continue;
    } 
      
    // Bit4   Palette number  **Non CGB Mode Only** (0=OBP0, 1=OBP1)
    uint16_t palette_register = (flags & (1 << 4)) ? OBP1 : OBP0;
      
    // Bit5   X flip          (0=Normal, 1=Horizontally mirrored)
    bool flip_x = flags & (1 << 5);
      
    // Bit6   Y flip          (0=Normal, 1=Vertically mirrored)
    bool flip_y = flags & (1 << 6);

    //From pan docs: An offscreen value (X=0 or X>=168) hides the sprite, but the sprite still affects the priority ordering - a better way to hide a sprite is to set its Y-coordinate offscreen.
  

    // In 8x16 mode, the lower bit of the tile number is ignored. Ie. the upper 8x8 tile is "NN AND FEh", and the lower 8x8 tile is "NN OR 01h".
    uint16_t tile_address = 0x8000;
    if (tall_sprites) {
      assert(flip_y == false);
      draw_tile_line(image, w, h, x + sprite_x, y, tile_address + (tile_index & 0xFE) * 0x10, 0, dy - (sprite_y + 0), palette_register, flip_x, flip_y);
      draw_tile_line(image, w, h, x + sprite_x, y, tile_address + (tile_index | 0x01) * 0x10, 0, dy - (sprite_y + 8), palette_register, flip_x, flip_y);
    } else {
      draw_tile_line(image, w, h, x + sprite_x, y, tile_address + tile_index * 0x10, 0, dy - sprite_y, palette_register, flip_x, flip_y);
    }
  
  }
    
}
   
static void gameboy_step_once() {
    
  // CPU: 4.194304 MHz => /4 = 1.048576 megahertz; 1/f = 953.674316 nanoseconds
  // LCD: 59.73 Hz = refresh rate => 1/59.73 Hz = 16.7420057 milliseconds

  int wasted_cycles = 0;

  // LCD has 144 lines + 10 lines [vblank] = 154 lines
  // 16.7420057mns / 154 lines = 108.714323 microseconds
  // The LY can take on any value between 0 through 153. The values between 144 and 153 indicate the V-Blank period.
  for(uint8_t ly = 0; ly < 154; ly++) {
    
    // Update LCD Y controller
    write_io8(LY, ly);

    // Read current flags    
    uint8_t _if = read_io8(IF);
    
    // Get current line comparator
    uint8_t lyc = read_io8(LYC);
        
    // Handle STAT register
    uint8_t stat = read_io8(STAT);
    
    // Bit 2 - Coincidence Flag  (0:LYC<>LY, 1:LYC=LY) (Read Only)
    if (lyc != ly) {
      stat &= ~(1 << 2);
    } else {
      stat |= (1 << 2);
      
      // Bit 6 - LYC=LY Coincidence Interrupt (1=Enable) (Read/Write)
      if (stat & (1 << 6)) {
        write_io8(IF, _if | INTERRUPTS_LCDSTAT);
      }
      
    }
  
    // Reset mode flag to 0, so we can OR the actual mode onto it
    // Bit 1-0 - Mode Flag       (Mode 0-3, see below) (Read Only)
    //           0: During H-Blank
    //           1: During V-Blank
    //           2: During Searching OAM-RAM
    //           3: During Transfering Data to LCD Driver
    stat &= ~0x3;
  
    //FIXME: Add memory access restrictions
    // Mode 0: The LCD controller is in the H-Blank period and
    //         the CPU can access both the display RAM (8000h-9FFFh)
    //         and OAM (FE00h-FE9Fh)
          
    // Mode 1: The LCD contoller is in the V-Blank period (or the
    //         display is disabled) and the CPU can access both the
    //         display RAM (8000h-9FFFh) and OAM (FE00h-FE9Fh)

    //FIXME: Add memory access restrictions
    // Mode 2: The LCD controller is reading from OAM memory.
    //         The CPU <cannot> access OAM memory (FE00h-FE9Fh)
    //         during this period.

    //FIXME: Add memory access restrictions
    // Mode 3: The LCD controller is reading from both OAM and VRAM,
    //         The CPU <cannot> access OAM and VRAM during this period.
    //         CGB Mode: Cannot access Palette Data (FF69,FF6B) either.
          
    // Emulate CPU for 108.714323 microseconds
    // That'd be: (108.714323 microseconds) / (953.674316 nanoseconds) = 113.99523 ~ 114 M-cycles per line [456 T-Cyles]     
    if (ly < 144) {
        
        // Mode 2
        write_io8(STAT, stat | 2);
        // Bit 5 - Mode 2 OAM Interrupt         (1=Enable) (Read/Write)
        if (stat & (1 << 5)) {
          write_io8(IF, _if | INTERRUPTS_LCDSTAT);
        }
        wasted_cycles = cpu_step(80 + wasted_cycles);
        
        
        // Mode 3
        write_io8(STAT, stat | 3);
        wasted_cycles = cpu_step(172 + wasted_cycles);
        
        // Mode 0
        write_io8(STAT, stat | 0);
        // Bit 3 - Mode 0 H-Blank Interrupt     (1=Enable) (Read/Write)
        if (stat & (1 << 5)) {
          write_io8(IF, _if | INTERRUPTS_LCDSTAT);
        }
        wasted_cycles = cpu_step(204 + wasted_cycles);
        
        // Clear background to background color 0
        uint8_t color = get_palette_color(BGP, 0);
        memset(&gameboy_framebuffer[ly * GAMEBOY_SCREEN_WIDTH], u2_to_u8(color), GAMEBOY_SCREEN_WIDTH);
        
        // Draw background sprites
        draw_sprites_line(gameboy_framebuffer, GAMEBOY_SCREEN_WIDTH, GAMEBOY_SCREEN_HEIGHT, 0, ly, ly, true);
        
        // Draw background
        uint8_t lcdc = read_io8(LCDC);
       
        //  Bit 4 - BG & Window Tile Data Select   (0=8800-97FF, 1=8000-8FFF)
        bool bg_tiles = !(lcdc & (1 << 4));
        
        //  Bit 3 - BG Tile Map Display Select     (0=9800-9BFF, 1=9C00-9FFF)
        uint16_t map_address = (lcdc & (1 << 3)) ? 0x9C00 : 0x9800;
        
        uint8_t scx = read_io8(SCX);
        uint8_t scy = read_io8(SCY);
        draw_background_line(gameboy_framebuffer, GAMEBOY_SCREEN_WIDTH, GAMEBOY_SCREEN_HEIGHT, 0, ly, map_address, bg_tiles, scx, ly + scy);
        
        // Draw foreground sprites
        draw_sprites_line(gameboy_framebuffer, GAMEBOY_SCREEN_WIDTH, GAMEBOY_SCREEN_HEIGHT, 0, ly, ly, false);
        
        //FIXME: 
        //  Bit 6 - Window Tile Map Display Select (0=9800-9BFF, 1=9C00-9FFF)
        //  Bit 5 - Window Display Enable          (0=Off, 1=On)
        
    } else {
  
        // Mode 1
        write_io8(STAT, stat | 1);
        
        // Trigger vblank interrupt at start of first invisible line (line 144)
        if (ly == 144) {
          write_io8(IF, _if | INTERRUPTS_VBLANK);
          
          
          // Trigger optional LCDSTAT interrupt
          // Bit 4 - Mode 1 V-Blank Interrupt     (1=Enable) (Read/Write)
          if (stat & (1 << 4)) {
            write_io8(IF, _if | INTERRUPTS_LCDSTAT);
          }
        }
        wasted_cycles = cpu_step(456 + wasted_cycles);
    }

  }
  
}

static bool fast_mode = false;
void gameboy_step() {
  unsigned int frames = fast_mode ? 4 : 1;
  while(frames--) {
    gameboy_step_once();
  }
}



void gameboy_notify_exit() {

  //FIXME
}


static void disassemble() {

  uint16_t address = 0x0000;
    
  while(address <= 0x7FFF) {
    
    
    // 04C5:  E024<tab>ld [$FF24], a
    
    printf("%04X:  ", address);
    
    // Get instruction
    uint8_t opcode = read_memory8(address);

    // run the CPU instruction
    InstructionHandler* handler = cpu_decode(opcode);
    assert(handler != NULL);

    // Read rest of instruction
    uint8_t code[32];
    code[0] = opcode;
    for(int i = 1; i < handler->length; i++) {
      code[i] = read_memory8(address + i);
    }

    // Print instruction bytes
    for(int i = 0; i < handler->length; i++) {
      printf("%02X", code[i]);
    }
    printf("\t");

    // Print disassembly
    char buffer[32];
    handler->disassemble(code, buffer);
    printf("%s", buffer);
    printf("\n");
    fflush(stdout);

    // Go to next instruction
    address += handler->length;
  }    
}

static void export_image(const char* path, uint8_t* image, unsigned int w, unsigned int h) {
    
  // Open file for writing (overwrites if necessary!)
  FILE* f = fopen(path, "wb");
  assert(f != NULL);
  
  // Write header with magic, width, height and highest color index
  fprintf(f, "P2\n");
  fprintf(f, "%d %d\n", w, h);
  fprintf(f, "3\n");
    
  // Loop over all pixels
  for(unsigned int y = 0; y < h; y++) {
    for(unsigned int x = 0; x < w; x++) {
        
      // Read pixel from source image
      unsigned int pixel_index = y * w + x;
      uint8_t color_u8 = image[pixel_index];
      
      // Convert color
      unsigned int color_u2 = u8_to_u2(color_u8);
      
      // Write pixel to file
      fprintf(f, " %1d", color_u2);
    }

    // Mark end of line
    fprintf(f, "\n");
  }

  // Close the file
  fclose(f);
  
}

static void dump_tile(uint16_t address, uint16_t palette_register, const char* suffix) {
  uint8_t image[8 * 8];
  memset(image, u2_to_u8(0), sizeof(image));
 
  //        image, w, h, x, y
  draw_tile(image, 8, 8, 0, 0, address, palette_register, false, false);

  // Export image to file
  char path[32];
  sprintf(path, "tile_%04X%s.pgm", address, suffix);
  export_image(path, image, 8, 8);
  
}

static void dump_tiles_8000() {
  for(unsigned int i = 0; i <= 0xFF; i++) {
    uint16_t tile_address = 0x8000 + i * 0x10;
    dump_tile(tile_address, BGP, "_bgp");
    dump_tile(tile_address, OBP0, "_obp0");
    dump_tile(tile_address, OBP1, "_obp1");
  }
}

static void dump_tiles_9000() {
  for(int i = -0x80; i <= 0x7F; i++) {
    uint16_t tile_address = 0x9000 + i * 0x10;
    dump_tile(tile_address, BGP, "_bgp");
    dump_tile(tile_address, OBP0, "_obp0");
    dump_tile(tile_address, OBP1, "_obp1");
  }
}

static void dump_background_map(uint16_t map_address, bool bg_tiles, const char* suffix) {
  uint8_t image[32*8 * 32*8];
  memset(image, u2_to_u8(0), sizeof(image));
    
  // Loop over all 32x32 tiles to assemble a graphic
  for(unsigned int y = 0; y < 32 * 8; y++) {
    draw_background_line(image, 32*8, 32*8, 0, y, map_address, bg_tiles, 0, y);
  }
    
  // Export image to file
  char path[32];
  sprintf(path, "background_%04X%s.pgm", map_address, suffix);
  export_image(path, image, 32*8, 32*8);
}


static void dump_background_map_9800() {  
  dump_background_map(0x9800, false, "");
  dump_background_map(0x9800, true, "_bg");
}

static void dump_background_map_9C00() {  
  dump_background_map(0x9C00, false, "");
  dump_background_map(0x9C00, true, "_bg");
}

static void dump_sprites(bool background) {
  uint8_t image[(256 + 8) * (256 + 16)];
  memset(image, u2_to_u8(0), sizeof(image));
    
  // Loop over all 32x32 tiles to assemble a graphic
  for(unsigned int y = 0; y < (256 + 16); y++) {
#if 0
    if (y != (256 / 2)) {
      continue;
    }
#endif
    draw_sprites_line(&image[8], 256 + 8, 256 + 16, 8, y, y + 16, background);
  }
    
  // Export image to file
  char path[32];
  const char* suffix = background ? "_bg" : "_fg";
  sprintf(path, "sprites%s.pgm", suffix);
  export_image(path, image, 256 + 8, 256 + 16);
}

static void take_screenshot() {
  export_image("screenshot.pgm", gameboy_framebuffer, GAMEBOY_SCREEN_WIDTH, GAMEBOY_SCREEN_HEIGHT);
}

void gameboy_debug_hotkey(unsigned int f) {
  switch(f) {
  case 1:
    printf("Dumping tiles 0x8000!\n");
    dump_tiles_8000();
    break;
  case 2:
    printf("Dumping tiles 0x9000!\n");
    dump_tiles_9000();
    break;
  case 3:
    printf("Dumping background map 0x9800!\n");
    dump_background_map_9800();
    break;
  case 4:
    printf("Dumping background map 0x9C00!\n");
    dump_background_map_9C00();
    break;
  case 5:
    printf("Dumping background sprites!\n");
    dump_sprites(true);
    break;
  case 6:
    printf("Dumping foreground sprites!\n");
    dump_sprites(false);
    break;
  case 9:
    fast_mode = !fast_mode;
    printf("%s mode!\n", fast_mode ? "Fast" : "Normal");
    break;
  case 12:
    printf("Taking screenshot!\n");
    take_screenshot();
    break;
  default:
    printf("Unmapped debug hotkey: %d\n", f);
    break;
  }
}
