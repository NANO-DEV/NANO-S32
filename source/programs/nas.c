// User program: Intel 80386 Assembler

#include "types.h"
#include "ulib/ulib.h"

#define EOF 0xFFFFFFE0 // End Of File

// Code generation vars
uint cg_origin = 0x20000; // Absolute pos

// Symbol types
enum S_TYPE {
  S_LABEL,  // Label
  S_DATAD,  // Data double
  S_DATAW,  // Data word
  S_DATAB   // Data byte
};

// Symbols table
#define S_MAX 32
struct symbol {
  char  name[8];
  uint  type;  // See enum S_TYPE
  uint  value;
} s_table[S_MAX];

// Operand types
enum O_TYPE {
  O_ANY,  // unknown
  O_RD,   // Register double
  O_RMD,  // Memory double in a register
  O_MD,   // Memory double immediate
  O_IX,   // Immediate
};

// Registers
enum R_DEF {
  R_ANY, // Unknown
  R_AX,
  R_CX,
  R_DX,
  R_BX,
  R_SP,
  R_BP,
  R_SI,
  R_DI,
  R_COUNT
};

// CONST Registers data
struct reg_data_struct {
  char  name[5];
  uint  type;
  uint  encoding;
} const r_data[R_COUNT] =
{
  {"NO",  O_ANY, 0x00},
  {"eax", O_RD,  0x00},
  {"ecx", O_RD,  0x01},
  {"edx", O_RD,  0x02},
  {"ebx", O_RD,  0x03},
  {"esp", O_RD,  0x04},
  {"ebp", O_RD,  0x05},
  {"esi", O_RD,  0x06},
  {"edi", O_RD,  0x07},
};

// Instruction id
enum I_DEF {
  I_PUSH,
  I_POP,
  I_MOV_RID,
  I_MOV_RRD,
  I_MOV_RMD,
  I_MOV_MRD,
  I_MOV_RRMD,
  I_MOV_RMRD,
  I_CMP_RID,
  I_CMP_RRD,
  I_CMP_RMD,
  I_CMP_MRD,
  I_CMP_RRMD,
  I_CMP_RMRD,
  I_RET,
  I_INT,
  I_CALL,
  I_JMP,
  I_JE,
  I_JNE,
  I_JG,
  I_JGE,
  I_JL,
  I_JLE,
  I_JC,
  I_JNC,
  I_ADD_RID,
  I_ADD_RRD,
  I_ADD_RMD,
  I_ADD_MRD,
  I_ADD_RRMD,
  I_ADD_RMRD,
  I_SUB_RID,
  I_SUB_RRD,
  I_SUB_RMD,
  I_SUB_MRD,
  I_SUB_RRMD,
  I_SUB_RMRD,
  I_MUL_RD,
  I_DIV_RD,
  I_NOT_RD,
  I_AND_RID,
  I_AND_RRD,
  I_AND_RMD,
  I_AND_MRD,
  I_AND_RRMD,
  I_AND_RMRD,
  I_OR_RID,
  I_OR_RRD,
  I_OR_RMD,
  I_OR_MRD,
  I_OR_RRMD,
  I_OR_RMRD,
  I_COUNT
};

// CONST Instructions data
#define I_MAX_OPS 2 // Max instruction operands
typedef struct idata_t {
  char  mnemonic[6];
  uint  opcode;
  uint  nops;     // Number of operands
  uint  op_type[I_MAX_OPS]; // Type of operands
  uint  op_value[I_MAX_OPS]; // Restricted value
} idata_t;

const idata_t i_data[I_COUNT] =
{
  {"push", 0x50, 1, {O_RD,  O_ANY},   {R_ANY, 0    }},
  {"pop",  0x58, 1, {O_RD,  O_ANY},   {R_ANY, 0    }},
  {"mov",  0xB8, 2, {O_RD,  O_IX },   {R_ANY, 0    }},
  {"mov",  0x89, 2, {O_RD,  O_RD },   {R_ANY, R_ANY}},
  {"mov",  0x8B, 2, {O_RD,  O_MD },   {R_ANY, 0    }},
  {"mov",  0x89, 2, {O_MD,  O_RD },   {0,     R_ANY}},
  {"mov",  0x8B, 2, {O_RD,  O_RMD},   {R_ANY, R_ANY}},
  {"mov",  0x89, 2, {O_RMD, O_RD },   {R_ANY, R_ANY}},
  {"cmp",  0x81, 2, {O_RD,  O_IX },   {R_ANY, 0    }},
  {"cmp",  0x39, 2, {O_RD,  O_RD },   {R_ANY, R_ANY}},
  {"cmp",  0x3B, 2, {O_RD,  O_MD },   {R_ANY, 0    }},
  {"cmp",  0x39, 2, {O_MD,  O_RD },   {0,     R_ANY}},
  {"cmp",  0x3B, 2, {O_RD,  O_RMD},   {R_ANY, R_ANY}},
  {"cmp",  0x39, 2, {O_RMD, O_RD },   {R_ANY, R_ANY}},
  {"ret",  0xC3, 0, {O_ANY, O_ANY},   {0,     0    }},
  {"int",  0xCD, 1, {O_IX,  O_ANY},   {0,     0    }},
  {"call", 0xE8, 1, {O_IX,  O_ANY},   {0,     0    }},
  {"jmp",  0xE9, 1, {O_IX,  O_ANY},   {0,     0    }},
  {"je",   0x74, 1, {O_IX,  O_ANY},   {0,     0    }},
  {"jne",  0x75, 1, {O_IX,  O_ANY},   {0,     0    }},
  {"jg",   0x7F, 1, {O_IX,  O_ANY},   {0,     0    }},
  {"jge",  0x7D, 1, {O_IX,  O_ANY},   {0,     0    }},
  {"jl",   0x7C, 1, {O_IX,  O_ANY},   {0,     0    }},
  {"jle",  0x7E, 1, {O_IX,  O_ANY},   {0,     0    }},
  {"jc",   0x72, 1, {O_IX,  O_ANY},   {0,     0    }},
  {"jnc",  0x73, 1, {O_IX,  O_ANY},   {0,     0    }},
  {"add",  0x81, 2, {O_RD,  O_IX },   {R_ANY, 0    }},
  {"add",  0x01, 2, {O_RD,  O_RD },   {R_ANY, R_ANY}},
  {"add",  0x03, 2, {O_RD,  O_MD },   {R_ANY, 0    }},
  {"add",  0x01, 2, {O_MD,  O_RD },   {0,     R_ANY}},
  {"add",  0x03, 2, {O_RD,  O_RMD},   {R_ANY, R_ANY}},
  {"add",  0x01, 2, {O_RMD, O_RD },   {R_ANY, R_ANY}},
  {"sub",  0x81, 2, {O_RD,  O_IX },   {R_ANY, 0    }},
  {"sub",  0x29, 2, {O_RD,  O_RD },   {R_ANY, R_ANY}},
  {"sub",  0x2B, 2, {O_RD,  O_MD },   {R_ANY, 0    }},
  {"sub",  0x29, 2, {O_MD,  O_RD },   {0,     R_ANY}},
  {"sub",  0x2B, 2, {O_RD,  O_RMD},   {R_ANY, R_ANY}},
  {"sub",  0x29, 2, {O_RMD, O_RD },   {R_ANY, R_ANY}},
  {"mul",  0xF7, 2, {O_RD,  O_ANY},   {R_ANY, 0    }},
  {"div",  0xF7, 2, {O_RD,  O_ANY},   {R_ANY, 0    }},
  {"not",  0xF7, 2, {O_RD,  O_ANY},   {R_ANY, 0    }},
  {"and",  0x81, 2, {O_RD,  O_IX },   {R_ANY, 0    }},
  {"and",  0x21, 2, {O_RD,  O_RD },   {R_ANY, R_ANY}},
  {"and",  0x23, 2, {O_RD,  O_MD },   {R_ANY, 0    }},
  {"and",  0x21, 2, {O_MD,  O_RD },   {0,     R_ANY}},
  {"and",  0x23, 2, {O_RD,  O_RMD},   {R_ANY, R_ANY}},
  {"and",  0x21, 2, {O_RMD, O_RD },   {R_ANY, R_ANY}},
  {"or",   0x81, 2, {O_RD,  O_IX },   {R_ANY, 0    }},
  {"or",   0x09, 2, {O_RD,  O_RD },   {R_ANY, R_ANY}},
  {"or",   0x0B, 2, {O_RD,  O_MD },   {R_ANY, 0    }},
  {"or",   0x09, 2, {O_MD,  O_RD },   {0,     R_ANY}},
  {"or",   0x0B, 2, {O_RD,  O_RMD},   {R_ANY, R_ANY}},
  {"or",   0x09, 2, {O_RMD, O_RD },   {R_ANY, R_ANY}},
};

// Symbols reference table
#define S_MAX_REF 32
struct s_ref_struct {
  uint    offset;
  uint    symbol;
  uint    operand;
  uint    instr_id;
  idata_t instruction;
} s_ref[S_MAX_REF];

// Is string uint?
bool sisu(char *src)
{
  uint base = 10;

  if(!src[0]) {
    return FALSE;
  }

  // Is hex?
  if(src[0]=='0' && src[1]=='x' && src[2]) {
    src += 2;
    base = 16;
  }

  while(*src) {
    uint digit = *src<='9'? *src-'0' :
      *src>='a'? 0xA+*src-'a' : 0xA+*src-'A';

    if(digit>=base) {
      return FALSE;
    }
    src++;
  }
  return TRUE;
}

// Encode and write instruction to buffer
uint encode_instruction(uint8_t *buffer, uint offset, uint id, uint *op)
{
  switch(id) {

  case I_RET: {
    buffer[offset++] = i_data[id].opcode;
    debug_putstr(": %2x", buffer[offset-1]);
    break;
  }

  case I_PUSH:
  case I_POP: {
    buffer[offset++] =
      i_data[id].opcode + r_data[op[0]].encoding;

    debug_putstr(": %2x", buffer[offset-1]);
    break;
  }

  case I_INT: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = op[0];
    debug_putstr(": %2x %2x", buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_JG:
  case I_JGE:
  case I_JL:
  case I_JLE:
  case I_JC:
  case I_JNC:
  case I_JE:
  case I_JNE: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset] = (op[0]-cg_origin-(offset+1));
    offset++;
    debug_putstr(": %2x %2x", buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_CALL:
  case I_JMP: {
    uint address = op[0]-cg_origin-(offset+5);
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = address & 0xFF;
    buffer[offset++] = (address >> 8 ) & 0xFF;
    buffer[offset++] = (address >> 16) & 0xFF;
    buffer[offset++] = (address >> 24) & 0xFF;
    debug_putstr(": %2x %2x %2x %2x %2x",
      buffer[offset-5], buffer[offset-4], buffer[offset-3],
      buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_OR_RID: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = 0xC0|(0x01<<3)|r_data[op[0]].encoding;
    buffer[offset++] = op[1] & 0xFF;
    buffer[offset++] = (op[1] >> 8 ) & 0xFF;
    buffer[offset++] = (op[1] >> 16) & 0xFF;
    buffer[offset++] = (op[1] >> 24) & 0xFF;
    debug_putstr(": %2x %2x %2x %2x %2x %2x",
      buffer[offset-6], buffer[offset-5], buffer[offset-4],
      buffer[offset-3], buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_AND_RID: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = 0xC0|(0x04<<3)|r_data[op[0]].encoding;
    buffer[offset++] = op[1] & 0xFF;
    buffer[offset++] = (op[1] >> 8 ) & 0xFF;
    buffer[offset++] = (op[1] >> 16) & 0xFF;
    buffer[offset++] = (op[1] >> 24) & 0xFF;
    debug_putstr(": %2x %2x %2x %2x %2x %2x",
      buffer[offset-6], buffer[offset-5], buffer[offset-4],
      buffer[offset-3], buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_NOT_RD: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = 0xC0|(0x02<<3)|r_data[op[0]].encoding;
    debug_putstr(": %2x %2x", buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_MUL_RD: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = 0xC0|(0x04<<3)|r_data[op[0]].encoding;
    debug_putstr(": %2x %2x", buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_DIV_RD: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = 0xC0|(0x06<<3)|r_data[op[0]].encoding;
    debug_putstr(": %2x %2x", buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_SUB_RID: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = 0xC0|(0x05<<3)|r_data[op[0]].encoding;
    buffer[offset++] = op[1] & 0xFF;
    buffer[offset++] = (op[1] >> 8 ) & 0xFF;
    buffer[offset++] = (op[1] >> 16) & 0xFF;
    buffer[offset++] = (op[1] >> 24) & 0xFF;
    debug_putstr(": %2x %2x %2x %2x %2x %2x",
      buffer[offset-6], buffer[offset-5], buffer[offset-4],
      buffer[offset-3], buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_ADD_RID: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = 0xC0|r_data[op[0]].encoding;
    buffer[offset++] = op[1] & 0xFF;
    buffer[offset++] = (op[1] >> 8 ) & 0xFF;
    buffer[offset++] = (op[1] >> 16) & 0xFF;
    buffer[offset++] = (op[1] >> 24) & 0xFF;
    debug_putstr(": %2x %2x %2x %2x %2x %2x",
      buffer[offset-6], buffer[offset-5], buffer[offset-4],
      buffer[offset-3], buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_CMP_RID: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = 0xC0|(0x07<<3)|r_data[op[0]].encoding;
    buffer[offset++] = op[1] & 0xFF;
    buffer[offset++] = (op[1] >> 8 ) & 0xFF;
    buffer[offset++] = (op[1] >> 16) & 0xFF;
    buffer[offset++] = (op[1] >> 24) & 0xFF;
    debug_putstr(": %2x %2x %2x %2x %2x %2x",
      buffer[offset-6], buffer[offset-5], buffer[offset-4],
      buffer[offset-3], buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_MOV_RID: {
    buffer[offset++] = i_data[id].opcode + r_data[op[0]].encoding;
    buffer[offset++] = op[1] & 0xFF;
    buffer[offset++] = (op[1] >> 8 ) & 0xFF;
    buffer[offset++] = (op[1] >> 16) & 0xFF;
    buffer[offset++] = (op[1] >> 24) & 0xFF;
    debug_putstr(": %2x %2x %2x %2x %2x",
      buffer[offset-5], buffer[offset-4], buffer[offset-3],
      buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_MOV_RMD: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = 0x05|(r_data[op[0]].encoding<<3);
    buffer[offset++] = op[1] & 0xFF;
    buffer[offset++] = (op[1] >> 8 ) & 0xFF;
    buffer[offset++] = (op[1] >> 16) & 0xFF;
    buffer[offset++] = (op[1] >> 24) & 0xFF;
    debug_putstr(": %2x %2x %2x %2x %2x %2x",
      buffer[offset-6], buffer[offset-5], buffer[offset-4],
      buffer[offset-3], buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_CMP_MRD: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = 0x05|(r_data[op[1]].encoding<<3);
    buffer[offset++] = op[1] & 0xFF;
    buffer[offset++] = (op[1] >> 8 ) & 0xFF;
    buffer[offset++] = (op[1] >> 16) & 0xFF;
    buffer[offset++] = (op[1] >> 24) & 0xFF;
    debug_putstr(": %2x %2x %2x %2x %2x %2x",
      buffer[offset-6], buffer[offset-5], buffer[offset-4],
      buffer[offset-3], buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_CMP_RMRD: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = (r_data[op[1]].encoding<<3)|r_data[op[0]].encoding;
    debug_putstr(": %2x %2x", buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_OR_RRD:
  case I_AND_RRD:
  case I_SUB_RRD:
  case I_ADD_RRD:
  case I_CMP_RRD:
  case I_MOV_RRD: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = 0xC0|(r_data[op[1]].encoding<<3)|r_data[op[0]].encoding;
    debug_putstr(": %2x %2x", buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_OR_RMD:
  case I_AND_RMD:
  case I_SUB_RMD:
  case I_ADD_RMD:
  case I_CMP_RMD: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = 0x05|(r_data[op[0]].encoding<<3);
    buffer[offset++] = op[1] & 0xFF;
    buffer[offset++] = (op[1] >> 8 ) & 0xFF;
    buffer[offset++] = (op[1] >> 16) & 0xFF;
    buffer[offset++] = (op[1] >> 24) & 0xFF;
    debug_putstr(": %2x %2x %2x %2x %2x %2x",
      buffer[offset-6], buffer[offset-5], buffer[offset-4],
      buffer[offset-3], buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_OR_MRD:
  case I_AND_MRD:
  case I_SUB_MRD:
  case I_ADD_MRD:
  case I_MOV_MRD: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = 0x05|(r_data[op[1]].encoding<<3);
    buffer[offset++] = op[1] & 0xFF;
    buffer[offset++] = (op[1] >> 8 ) & 0xFF;
    buffer[offset++] = (op[1] >> 16) & 0xFF;
    buffer[offset++] = (op[1] >> 24) & 0xFF;
    debug_putstr(": %2x %2x %2x %2x %2x %2x",
      buffer[offset-6], buffer[offset-5], buffer[offset-4],
      buffer[offset-3], buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_OR_RRMD:
  case I_AND_RRMD:
  case I_SUB_RRMD:
  case I_ADD_RRMD:
  case I_CMP_RRMD:
  case I_MOV_RRMD: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = (r_data[op[0]].encoding<<3)|r_data[op[1]].encoding;
    debug_putstr(": %2x %2x", buffer[offset-2], buffer[offset-1]);
    break;
  }

  case I_OR_RMRD:
  case I_AND_RMRD:
  case I_SUB_RMRD:
  case I_ADD_RMRD:
  case I_MOV_RMRD: {
    buffer[offset++] = i_data[id].opcode;
    buffer[offset++] = (r_data[op[1]].encoding<<3)|r_data[op[0]].encoding;
    debug_putstr(": %2x %2x", buffer[offset-2], buffer[offset-1]);
    break;
  }

  default: {
    debug_putstr(": Instruction not found\n");
    return ERROR_NOT_FOUND;
  }

  };

  return offset;
}

// Encode and write data to buffer
uint encode_data(uint8_t *buffer, uint offset, uint type, uint value)
{
  switch(type) {

  case S_DATAD: {
    buffer[offset++] = value & 0xFF;
    buffer[offset++] = (value >> 8 ) & 0xFF;
    buffer[offset++] = (value >> 16) & 0xFF;
    buffer[offset++] = (value >> 24) & 0xFF;
    break;
  }

  case S_DATAW: {
    buffer[offset++] = value & 0xFF;
    buffer[offset++] = value >> 8;
    break;
  }

  case S_DATAB: {
    buffer[offset++] = value & 0xFF;
    break;
  }

  default: {
   debug_putstr(": Unknown data type\n");
   break;
  }

  };
  return offset;
}

// Given a file and offset, returns a file line and offset to next line start
static uint read_line(char *buff, uint buff_size, char *file, uint offset)
{
  // Clear buffer and read
  memset(buff, 0, buff_size);
  uint readed = read_file(buff, file, offset, buff_size);

  // Return on error
  if(readed >= ERROR_ANY) {
    return readed;
  }

  // Return EOF in case
  if(readed == 0) {
    debug_putstr("EOF\n");
    return EOF;
  }

  // Clamp buff to a single line
  for(uint i=0; i<readed; i++) {

    // Ignore '\r'
    if(buff[i] == '\r') {
      buff[i] = ' ';
    }

    // If new line, clamp buff to a single line
    // and return offset of next char
    if(buff[i] == '\n') {
      buff[i] = 0;
      return offset+i+1;
    }

    // If last line, clamp buff
    if(i==readed-1 && readed!=buff_size) {
      buff[i+1] = 0;
      return offset+i+1;
    }
  }

  // Line is too long
  return ERROR_ANY;
}

// Given a text line, returns token count and token start vectors
static uint tokenize_line(char *buff, uint size, char *tokv[], uint max_tok)
{
  uint tokc = 0;
  tokv[0] = buff;
  buff[size-1] = 0;

  while(*buff && tokc<max_tok) {
    // Remove initial separator symbols
    while(*buff==' ' || *buff=='\t' || *buff==',') {
      *buff = 0;
      buff++;
    }

    // Current token starts here
    tokv[tokc] = buff;
    while(*buff && *buff != ' '  &&
                   *buff != '\t' &&
                   *buff != ','  &&
                   *buff != ';') {
      buff++;
    }

    // Skip comments
    if(tokv[tokc][0] == ';') {
      tokv[tokc][0] = 0;
      return tokc;
    }

    // Next token
    if(tokv[tokc][0] != 0) {
      tokc++;
    }
  }

  return tokc;
}

// Find or add symbol to table
uint find_or_add_symbol(char *name)
{
  for(uint s=0; s<S_MAX; s++) {
    if(s_table[s].name[0] == 0) {
      // Not found, so add
      strncpy(s_table[s].name, name, sizeof(s_table[s].name));
      return s;
    } else if(!strcmp(s_table[s].name, name)) {
      // Found, return index
      return s;
    }
  }
  return ERROR_NOT_FOUND;
}

// Find symbol in table and return index
uint find_symbol(char *name)
{
  for(uint s=0; s<S_MAX; s++) {
    if(s_table[s].name[0] == 0) {
      // Not found and no more symbols
      break;
    } else if(!strcmp(s_table[s].name, name)) {
      // Found
      return s;
    }
  }
  return ERROR_NOT_FOUND;
}

// Append a symbol reference to symbol references table
void append_symbol_ref(uint symbol, uint operand, uint offset,
  uint instr_id, idata_t *instruction)
{
  for(uint r=0; r<S_MAX_REF; r++) {
    if(s_ref[r].offset == 0) {

      // First empty, add here
      s_ref[r].symbol = symbol;
      s_ref[r].operand = operand;
      s_ref[r].offset = offset;
      s_ref[r].instr_id = instr_id;

      memcpy(&s_ref[r].instruction, instruction,
        sizeof(s_ref[r].instruction));

      break;
    }
  }
}

// Find matching instruction and return id
uint find_instruction(idata_t *ci)
{
  for(uint id=0; id<I_COUNT; id++) {
    // Check same name and number of args
    if(!strcmp(ci->mnemonic, i_data[id].mnemonic) &&
      ci->nops==i_data[id].nops) {

      // Assume match
      uint match = 1;
      for(uint i=0; i<ci->nops; i++) {

        // Unless some arg is not the same type
        if(ci->op_type[i] != i_data[id].op_type[i]) {
          match = 0;
          break;
        }

        // Or a non matching specific value is required
        if(i_data[id].op_value[i]!=0 &&
          i_data[id].op_value[i]!=ci->op_value[i]) {
          match = 0;
          break;
        }
      }

      // Not match, continue search
      if(!match) {
        continue;
      }

      // Match: return id
      return id;
    }
  }
  return ERROR_NOT_FOUND;
}

// Program entry point
int main(int argc, char *argv[])
{
  // Check args
  if(argc != 2) {
    putstr("usage: %s <inputfile>\n", argv[0]);
    return 0;
  }

  // Compute output file name
  char ofile[14] = {0};
  strncpy(ofile, argv[1], sizeof(ofile));
  uint dot = strchr(ofile, '.');
  if(dot) {
    ofile[dot-1] = 0;
  }
  ofile[sizeof(ofile)-strlen(".bin")-2] = 0;
  strncat(ofile, ".bin", sizeof(ofile));

  // Output buffer and offset
  uint8_t obuff[1024] = {0};
  uint ooffset = 0;
  // Clear output buffer
  memset(obuff, 0, sizeof(obuff));

  // Clear symbols table
  memset(s_table, 0, sizeof(s_table));

  // Clear references table
  memset(s_ref, 0, sizeof(s_ref));

  // Input file buffer and offset
  char fbuff[2048] = {0};
  uint foffset = 0;
  // Process input file line by line
  uint fline = 1;
  while((foffset=read_line(fbuff, sizeof(fbuff), argv[1], foffset))!=EOF) {
    // Exit on read error
    if(foffset >= ERROR_ANY) {
      putstr("Error reading input file\n");
      debug_putstr("Error (%x) reading input file (%s)\n", foffset, argv[0]);
      ooffset = 0;
      break;
    }

    // Tokenize line
    char *tokv[32] = {NULL};
    const uint tokc =
      tokenize_line(fbuff, sizeof(fbuff), tokv, sizeof(tokv)/sizeof(char*));

    // Check special directives
    if(tokc==2 && !strcmp(tokv[0], "ORG")) {
      cg_origin = stou(tokv[1]);
      debug_putstr("origin = %x\n", cg_origin);
    }

    // Check labels
    else if(tokc==1 && tokv[0][strlen(tokv[0])-1] == ':') {
      tokv[0][strlen(tokv[0])-1] = 0;
      const uint symbol = find_or_add_symbol(tokv[0]);
      s_table[symbol].value = ooffset;
      s_table[symbol].type = S_LABEL;

      debug_putstr("label %s = ORG+%x\n",
        s_table[symbol].name, s_table[symbol].value);
    }

    // Check memory addresses
    else if(tokc>=3 && !strcmp(tokv[1], "dd")) {
      const uint symbol = find_or_add_symbol(tokv[0]);
      s_table[symbol].value = ooffset;
      s_table[symbol].type = S_DATAD;

      debug_putstr("dword %s = ORG+%x : ",
        s_table[symbol].name, s_table[symbol].value);

      for(uint n=2; n<tokc; n++) {
        debug_putstr("%2x ", stou(tokv[n]));
        ooffset = encode_data(obuff, ooffset,
          s_table[symbol].type, stou(tokv[n]));
      }
      debug_putstr("\n");
    }
    else if(tokc>=3 && !strcmp(tokv[1], "dw")) {
      const uint symbol = find_or_add_symbol(tokv[0]);
      s_table[symbol].value = ooffset;
      s_table[symbol].type = S_DATAW;

      debug_putstr("word %s = ORG+%x : ",
        s_table[symbol].name, s_table[symbol].value);

      for(uint n=2; n<tokc; n++) {
        debug_putstr("%2x ", stou(tokv[n]));
        ooffset = encode_data(obuff, ooffset,
          s_table[symbol].type, stou(tokv[n]));
      }
      debug_putstr("\n");
    }
    else if(tokc>=3 && !strcmp(tokv[1], "db")) {
      const uint symbol = find_or_add_symbol(tokv[0]);
      s_table[symbol].value = ooffset;
      s_table[symbol].type = S_DATAB;

      debug_putstr("byte %s = ORG+%x : ",
        s_table[symbol].name, s_table[symbol].value);

      for(uint n=2; n<tokc; n++) {
        debug_putstr("%2x ", stou(tokv[n]));
        ooffset = encode_data(obuff, ooffset,
          s_table[symbol].type, stou(tokv[n]));
      }
      debug_putstr("\n");
    }

    // Check instructions
    else if(tokc) {
      uint symbol_id=0xFFFF, nas=0xFFFF;
      idata_t ci;

      // Get instruction info
      strncpy(ci.mnemonic, tokv[0], sizeof(ci.mnemonic));
      ci.opcode = 0;
      ci.nops = tokc-1;
      debug_putstr("%s%6c", ci.mnemonic, ' ');

      uint op = 0;
      for(op=0; op<ci.nops; op++) {
        uint is_pointer = 0;
        ci.op_type[op] = O_ANY;
        ci.op_value[op] = 0;

        if(tokv[op+1][0]=='[' && tokv[op+1][strlen(tokv[op+1])-1]==']') {
          is_pointer = 1;
          tokv[op+1][strlen(tokv[op+1])-1] = 0;
          tokv[op+1]++;
        }

        // Check registers
        for(uint r=0; r<R_COUNT; r++) {
          if(!strcmp(tokv[op+1], r_data[r].name)) {
            ci.op_type[op] = is_pointer ? O_RMD : r_data[r].type;
            ci.op_value[op] = r;
            debug_putstr("%s:%s%9c", is_pointer?"pr":"r ",r_data[r].name, ' ');
            break;
          }
        }

        // If not a register, check immediate
        if(ci.op_type[op] == O_ANY && sisu(tokv[op+1])) {
          ci.op_type[op] = is_pointer ? O_MD : O_IX;
          ci.op_value[op] = stou(tokv[op+1]);
          debug_putstr("%s:%x%9c", is_pointer?"p":"i",ci.op_value[op], ' ');
        }

        // Otherwise, it must be a symbol
        if(ci.op_type[op] == O_ANY) {
          symbol_id = find_or_add_symbol(tokv[op+1]);
          nas = op;

          ci.op_type[op] = is_pointer ? O_MD : O_IX;
          ci.op_value[op] = stou(tokv[op+1]);
          debug_putstr("%s:%s%9c", is_pointer?"p":"i", tokv[op+1], ' ');
        }
      }

      // Make debug info look better
      for(uint opc=op; opc<I_MAX_OPS; opc++) {
        debug_putstr("%9c", ' ');
      }

      // Process line
      const uint instr_id = find_instruction(&ci);

      // Show error if no opcode found
      if(instr_id == ERROR_NOT_FOUND) {
        putstr("error: line %u. Instruction not found\n", fline);
        debug_putstr("error: line %u. Instruction not found\n", fline);
        ooffset = 0;
        break;
      }

      // Set opcode
      ci.opcode = i_data[instr_id].opcode;

      // Append symbol reference to table
      if(symbol_id != 0xFFFF) {
        append_symbol_ref(symbol_id, nas, ooffset, instr_id, &ci);
      }

      // Write instruction to buffer
      ooffset = encode_instruction(obuff, ooffset, instr_id, ci.op_value);

      // Show error if not able to encode
      if(ooffset == ERROR_NOT_FOUND) {
        putstr("error: line %u. Can't encode instruction\n", fline);
        debug_putstr("error: line %u. Can't encode instruction\n", fline);
        ooffset = 0;
        break;
      }

      debug_putstr("\n");
    }
    fline++;
  }

  // Symbol table is filled, resolve references
  if(ooffset != 0) {
    for(uint r=0; r<S_MAX_REF; r++) {

      // No more symbols: break
      if(s_ref[r].offset == 0) {
        break;
      }

      // Correct arg value
      s_ref[r].instruction.op_value[s_ref[r].operand] =
        s_table[s_ref[r].symbol].value + cg_origin;

      debug_putstr("Solved symbol. Instruction %s%32c at %x : arg %d : %s%60c = %x ",
        i_data[s_ref[r].instr_id].mnemonic, ' ',
        s_ref[r].offset,
        s_ref[r].operand,
        s_table[s_ref[r].symbol].name, ' ',
        s_ref[r].instruction.op_value[s_ref[r].operand],
        ' ');

      // Overwrite the old instruction
      encode_instruction(obuff, s_ref[r].offset,
        s_ref[r].instr_id, s_ref[r].instruction.op_value);

      debug_putstr("\n");
    }
  }

  // Write output file
  if(ooffset != 0) {
    debug_putstr("Write file: %s, %ubytes\n", ofile, ooffset);
    uint result = write_file(obuff, ofile, 0, ooffset, FWF_CREATE|FWF_TRUNCATE);
    if(result != ooffset) {
      putstr("Error writting file\n");
      debug_putstr("Write file failed\n");
      return 1;

    } else {
      debug_putstr("Done\n\n", ofile, ooffset);

      // Dump saved file
      debug_putstr("Dump: \n", ofile, ooffset);

      result = read_file(obuff, ofile, 0, ooffset);
      if(result == ooffset) {
        for(uint i=0; i<result; i++) {
          debug_putstr("%2x ", obuff[i]);
        }
        
        debug_putstr("\n\n");
      } else {
        debug_putstr("Dump file contents failed\n");
      }
    }
  }

  return 0;
}
