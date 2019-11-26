#!/usr/bin/env python3

def parse_mgba_trace_line(line):
  result = {}
  registers, separator, opcode = line.partition('|')

  # Output registers
  for register in registers.strip().replace(': ', ':').split(' '):
    register_name, separator, register_value = register.partition(':')
    result[register_name.strip().lower()] = int(register_value, 16)

  # Get implied address
  if 'pc' in result:
    result['address'] = result['pc']

  # Output opcode
  _bytes, separator, mnemonic = opcode.partition(':') 
  result['bytes'] = bytes.fromhex(_bytes.strip())
  result['mnemonic'] = mnemonic.strip()
  return result


def parse_mgba_disassembly_line(line):
  result = {}
  address, separator, opcode = line.partition(':') 

  # Output address
  result['address'] = int(address, 16)

  # Output opcode
  _bytes, separator, mnemonic = opcode.partition('\t')
  result['bytes'] = bytes.fromhex(_bytes.strip())
  result['mnemonic'] = mnemonic.strip()
  return result

def parse_mgba_line(line):

  try:
    result = parse_mgba_disassembly_line(line)
    result['type'] = 'disassembly'
    return result
  except:
    pass

  try:
    result = parse_mgba_trace_line(line)
    result['type'] = 'trace'
    return result
  except:
    pass

  return None


if __name__ == '__main__':

  import sys

  if len(sys.argv) < 2:
    print('%s <command> [<path 1> [<path 2> [...]]]' % sys.argv[0])
    sys.exit(1)

  command = sys.argv[1]
  paths = sys.argv[2:]

  for path in paths:
    # Open each file
    with open(path, 'r') as f:
      lines = f.readlines()

      for line in lines:
        line = line.strip()
        if line == '':
          print('')
          continue

        # Parses output of "disassembly $0100 100"
        if command == 'disassembly':
          disassembly = parse_mgba_disassembly_line(line)
          print(disassembly)

        # Parses output of "trace 100"
        elif command == 'trace':
          trace = parse_mgba_trace_line(line)
          print(trace)

        # Parses output of "disassembly" and "trace" and generates some C code to unit test another disassembler
        elif command == 'example1':
          result = parse_mgba_line(line)
          print('disassemble(0x%04X, "%s");' % (result['address'], result['mnemonic']))

        else:

          print('Uknown command: %s' % command)
          break

  sys.exit(0)
