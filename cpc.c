#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#define ok 0
#define int int32_t
#define int64 int64_t

int64 MAX_SIZE = 128 * 1024 * 8; // 1MB = 128k * 64bit

int64 * code,         // code segment
      * code_dump,    // for dump
      * stack;        // stack segment
char  * data;         // data segment

int64 * pc,           // pc register
      * sp,           // rsp register
      * bp;           // rbp register

int64 ax,             // common register
      cycle;

// instruction set: copy from c4, change ENT/ADJ/LEV to NSF/CSF/RET, add CALL.
enum {IMM, LEA, JMP, JZ, JNZ, CALL, NSF, CSF, RET, LI, LC, SI, SC, PUSH,
    OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV,
    MOD, OPEN, READ, CLOS, PRTF, MALC, FREE, MSET, MCMP, EXIT};

// keywords & operators in precedence order, support int64. Do not support for.
enum {Num = 128, Fun, Sys, Glo, Loc, Id,
    Char, Int, Int64, Enum, If, Else, Return, Sizeof, While,
    Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge,
    Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak};

// fields of symbol_table: copy from c4, delete HXXX
enum {Token, Hash, Name, Class, Type, Value, PtrSize};

// types of variables & functions in symbol_table
enum {CHAR, INT, INT64, PTR};

// src code & dump
char * src,
     * src_dump;

// symbol table & reuse pointer
int64 * symbol_table,
      * symbol_ptr,
      * main_ptr;

int64 token, token_val;
int64 line;

void tokenize() {
    char* ch_ptr;

    while((token = *src++)) {
        if (token == '\n') line++;
        // skip marco
        else if (token == '#') while (*src != 0 && *src != '\n') src++;
        // handle symbol
        else if ((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || (token == '_')) {
            ch_ptr = src - 1;
            while ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z')
                    || (*src >= '0' && *src <= '9') || (*src == '_'))
                // use token store hash value
                token = token * 147 + *src++;
            // keep hash
            token = (token << 6) + (src - ch_ptr);
            symbol_ptr = symbol_table;
            // search same symbol in table
            while(symbol_ptr[Token]) {
                if (token == symbol_ptr[Hash] && !memcmp((char*)symbol_ptr[Name], ch_ptr, src - ch_ptr)) {
                    token = symbol_ptr[Token];
                    return;
                }
                symbol_ptr += PtrSize;
            }
            // add new symbol
            symbol_ptr[Name] = (int64)ch_ptr;
            symbol_ptr[Hash] = token;
            token = symbol_ptr[Token] = Id;
            return;
        }
        // handle number
        else if (token >= '0' && token <= '9') {
            // DEC, ch_ptr with 1 - 9
            if ((token_val = token - '0'))
                while (*src >= '0' && *src <= '9') token_val = token_val * 10 + *src++ - '0';
            //HEX, ch_ptr with 0x
            else if (*src == 'x' || *src == 'X')
                while ((token = *++src) && ((token >= '0' && token <= '9') || (token >= 'a' && token <= 'f')
                        || (token >= 'A' && token <= 'F')))
                    // COOL!
                    token_val = token_val * 16 + (token & 0xF) + (token >= 'A' ? 9 : 0);
            // OCT, start with 0
            else while (*src >= '0' && *src <= '7') token_val = token_val * 8 + *src++ - '0';
            token = Num;
            return;
        }
        // handle string & char
        else if (token == '"' || token == '\'') {
            ch_ptr = data;
            while (*src != 0 && *src != token) {
                if ((token_val = *src++) == '\\') {
                    // only support escape char '\n'
                    if ((token_val = *src++) == 'n') token_val = '\n';
                }
                // store string to data segment
                if (token == '"') *data++ = token_val;
            }
            src++;
            if (token == '"') token_val = (int64)ch_ptr; 
            // single char is Num
            else token = Num;
            return;
        }
        // handle comments or divide
        else if (token == '/') {
            if (*src == '/') {
                // skip comments
                while (*src != 0 && *src != '\n') src++;
            } else {
                // divide
                token = Div;
                return;
            }
        }
        // handle all kinds of operators, copy from c4.
        else if (token == '=') {if (*src == '=') {src++; token = Eq;} else token = Assign; return;}
        else if (token == '+') {if (*src == '+') {src++; token = Inc;} else token = Add; return;}
        else if (token == '-') {if (*src == '-') {src++; token = Dec;} else token = Sub; return;}
        else if (token == '!') {if (*src == '=') {src++; token = Ne;} return;}
        else if (token == '<') {if (*src == '=') {src++; token = Le;} else if (*src == '<') {src++; token = Shl;} else token = Lt; return;}
        else if (token == '>') {if (*src == '=') {src++; token = Ge;} else if (*src == '>') {src++; token = Shr;} else token = Gt; return;}
        else if (token == '|') {if (*src == '|') {src++; token = Lor;} else token = Or; return;}
        else if (token == '&') {if (*src == '&') {src++; token = Lan;} else token = And; return;}
        else if (token == '^') {token = Xor; return;}
        else if (token == '%') {token = Mod; return;}
        else if (token == '*') {token = Mul; return;}
        else if (token == '[') {token = Brak; return;}
        else if (token == '?') {token = Cond; return;}
        else if (token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ',' || token == ':') return;
    }
}

void assert(int64 tk) {
    if (token != tk) {
        printf("expect token: %lld(%c), get: %lld(%c)\n", tk, (int)tk, token, (int)token);
        exit(-1);
    }
    tokenize();
}

void parse() {
    tokenize();
    while (token > 0) {
        // parse enum
        // parse var
        // parse func
    }
}

void keyword() {
    char* keyword = "char int int64 enum if else return sizeof while "
        "open read close printf malloc free memset memcmp exit void main";
    int64 i;
    // add keywords to symbol table
    i = Char; while (i <= While) {tokenize(); symbol_ptr[Token] = i++;}
    // add Native CALL to symbol table
    i = OPEN; while (i <= EXIT) {
        tokenize();
        symbol_ptr[Class] = Sys;
        symbol_ptr[Type] = INT;
        symbol_ptr[Value] = i++;
    }
    tokenize(); symbol_ptr[Token] = Char; // handle void type
    tokenize(); main_ptr = symbol_ptr; // keep track of main
}

int init_vm() {
    // allocate memory for virtual machine
    if (!(code = code_dump = malloc(MAX_SIZE))) {
        printf("could not malloc(%lld) for code segment\n", MAX_SIZE);
        return -1;
    }
    if (!(data = malloc(MAX_SIZE))) {
        printf("could not malloc(%lld) for data segment\n", MAX_SIZE);
        return -1;
    }
    if (!(stack = malloc(MAX_SIZE))) {
        printf("could not malloc(%lld) for stack segment\n", MAX_SIZE);
        return -1;
    }
    memset(code, 0, MAX_SIZE);
    memset(data, 0, MAX_SIZE);
    memset(stack, 0, MAX_SIZE);
    // init register
    bp = sp = stack + MAX_SIZE; // stack downwards
    ax = 0;
    return ok;
}

int run_vm() {
    int64 op;
    int64* tmp;
    while (1) {
        op = *pc++; // read instruction
        // load & save
        if (op == IMM)          ax = *pc++;                         // load immediate(or global addr)
        else if (op == LEA)     ax = (int64)bp + *pc++;           // load local addr
        else if (op == LC)      ax = *(char*)ax;                    // load char
        else if (op == LI)      ax = *(int64*)ax;                 // load int
        else if (op == SC)      *(char*)*sp++ = ax;                 // save char to stack
        else if (op == SI)      *(int64*)*sp++ = ax;              // save int to stack
        else if (op == PUSH)    *--sp = ax;                         // push ax to stack
        // jump
        else if (op == JMP)     pc = (int64*)*pc;                 // jump
        else if (op == JZ)      pc = ax ? pc + 1 : (int64*)*pc;   // jump if ax == 0
        else if (op == JNZ)     pc = ax ? (int64*)*pc : pc + 1;   // jump if ax != 0
        // arithmetic
        else if (op == OR)      ax = *sp++ |  ax;
        else if (op == XOR)     ax = *sp++ ^  ax;
        else if (op == AND)     ax = *sp++ &  ax;
        else if (op == EQ)      ax = *sp++ == ax;
        else if (op == NE)      ax = *sp++ != ax;
        else if (op == LT)      ax = *sp++ <  ax;
        else if (op == LE)      ax = *sp++ <= ax;
        else if (op == GT)      ax = *sp++ >  ax;
        else if (op == GE)      ax = *sp++ >= ax;
        else if (op == SHL)     ax = *sp++ << ax;
        else if (op == SHR)     ax = *sp++ >> ax;
        else if (op == ADD)     ax = *sp++ +  ax;
        else if (op == SUB)     ax = *sp++ -  ax;
        else if (op == MUL)     ax = *sp++ *  ax;
        else if (op == DIV)     ax = *sp++ /  ax;
        else if (op == MOD)     ax = *sp++ %  ax;
        // some complicate instructions for function call
        // call function: push pc + 1 to stack & pc jump to func addr(pc point to)
        else if (op == CALL)    {*--sp = (int64)(pc+1); pc = (int64*)*pc;}
        // new stack frame: save bp, bp -> caller stack, stack add frame
        else if (op == NSF)     {*--sp = (int64)bp; bp = sp; sp = sp - *pc++;}
        // clean stack frame: stack clean frame, same as x86 : add esp, <size>
        else if (op == CSF)     sp = sp + *pc++;
        // return caller: retore stack, retore old bp, pc point to caller code addr(store by CALL) 
        else if (op == RET)     {sp = bp; bp = (int64*)*sp++; pc = (int64*)*sp++;}        
        // end for call function.
        // native call
        else if (op == OPEN)    {ax = open((char*)sp[1], sp[0]);}
        else if (op == CLOS)    {ax = close(*sp);}
        else if (op == READ)    {ax = read(sp[2], (char*)sp[1], *sp);}
        else if (op == PRTF)    {tmp = sp + pc[1]; ax = printf((char*)tmp[-1], tmp[-2], tmp[-3],
                                                                    tmp[-4], tmp[-5], tmp[-6]);}
        else if (op == MALC)    {ax = (int64)malloc(*sp);}
        else if (op == FREE)    {free((void*)*sp);}
        else if (op == MSET)    {ax = (int64)memset((char*)sp[2], sp[1], *sp);}
        else if (op == MCMP)    {ax = memcmp((char*)sp[2], (char*)sp[1], *sp);}
        else if (op == EXIT)    {printf("exit(%lld)\n", *sp); return *sp;}
        else {
            printf("unkown instruction: %lld\n", op);
            return -1;
        }
    }
    return ok;
}

int load_src(char* file) {
    int64 fd;
    // use open/read/close for bootstrap.
    if ((fd = open(file, 0)) < 0) {
        printf("could not open source code(%s)\n", file);
        return -1;
    }
    if (!(src = src_dump = malloc(MAX_SIZE))) {
        printf("could not malloc(%lld) for source code\n", MAX_SIZE);
        return -1;
    }
    line = 0;
    int64 cnt;
    if ((cnt = read(fd, src, MAX_SIZE - 1)) <= 0) {
        printf("could not read source code(%lld)\n", cnt);
        return -1;
    }
    src[cnt] = 0; // EOF
    close(fd);
    return ok;
}

int main(int argc, char** argv) {
    // load source code
    if (load_src(*(argv+1)) != ok) return -1;
    // init memory & register
    if (init_vm() != ok) return -1;
    // prepare keywords for symbol table
    keyword();
    // parse and generate vm instructions, save to vm
    parse();
    // run vm and execute instructions
    return run_vm();
}
