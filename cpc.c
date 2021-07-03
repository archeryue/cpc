#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

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

// instruction set: copy from c4, change JSR/ENT/ADJ/LEV/BZ/BNZ to CALL/NSVA/DSAR/RET/JZ/JNZ.
enum {IMM, LEA, JMP, JZ, JNZ, CALL, NSVA, DSAR, RET, LI, LC, SI, SC, PUSH,
    OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV,
    MOD, OPEN, READ, CLOS, PRTF, MALC, FREE, MSET, MCMP, EXIT};

// classes/keywords, support int64. Do not support for.
enum {Num = 128, Fun, Sys, Glo, Loc, Id,
    Char, Int, Int64, Enum, If, Else, Return, Sizeof, While,
    // operators in precedence order.
    Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge,
    Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak};

// fields of symbol_table: copy from c4, rename HXX to GXX
enum {Token, Hash, Name, Class, Type, Value, GClass, GType, GValue, SymSize};

// types of variables & functions in symbol_table
enum {CHAR, INT, INT64, PTR};

// src code & dump
char * src,
     * src_dump;

// symbol table & pointer
int64 * symbol_table,
      * symbol_ptr,
      * main_ptr;

int64 token, token_val;
int64 line;
int64 i; // reuse index var

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
                symbol_ptr += SymSize;
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

void check_id() {
    if (token != Id) {
        printf("line %lld: invalid or duplicate identifer\n", line);
        exit(-1);
    }
}

void check_local_id() {
    check_id();
    if (symbol_ptr[Class] == Loc) {
        printf("line %lld: duplicate declaration\n", line);
        exit(-1);
    }
}

void check_new_id() {
    check_id();
    if (symbol_ptr[Class]) {
        printf("line %lld: duplicate declaration\n", line);
        exit(-1);
    }
}

void parse_enum() {
    i = 0; // enum index
    while (token != '}') {
        check_new_id();
        assert(Id);
        // handle custom enum index
        if (token == Assign) {assert(Assign); assert(Num); i = token_val;}
        symbol_ptr[Class] = Num;
        symbol_ptr[Type] = INT64;
        symbol_ptr[Value] = i++;
        if (token == ',') tokenize();
    }
}

int64 parse_base_type() {
    // parse base type
    if (token == Char) {assert(Char); return CHAR;}
    else if (token == Int) {assert(Int); return INT;}
    else {assert(Int64); return INT64;}
}

void hide_global() {
    symbol_ptr[GClass] = symbol_ptr[Class];
    symbol_ptr[GType] = symbol_ptr[Type];
    symbol_ptr[GValue] = symbol_ptr[Value];
}

void recover_global() {
    symbol_ptr[Class] = symbol_ptr[GClass];
    symbol_ptr[Type] = symbol_ptr[GType];
    symbol_ptr[Value] = symbol_ptr[GValue];
}

int64 ibp;

void parse_param() {
    int64 type;
    i = 0;
    while (token != ')') {
        type = parse_base_type(); 
        // parse pointer's star
        while (token == Mul) {assert(Mul); type = type + PTR;}
        check_local_id(); assert(Id);
        hide_global();
        symbol_ptr[Class] = Loc;
        symbol_ptr[Type] = type;
        symbol_ptr[Value] = i++;
        if (token == ',') assert(',');
    }
    ibp = i + 1;
}

void parse_expr(int64 precd) {
    int64 type;
    int64* tmp_ptr;
    // const number
    if (token == Num) {
        assert(Num);
        *++code = IMM;
        *++code = token_val;
        type = INT;
    } 
    // const string
    else if (token == '"') {
        *++code = IMM;
        *++code = token_val; // string addr
        assert('"'); while (token == '"') assert('"'); // handle multi-row
        data = (char*)((int64)data + 8 & -8); // add \0 for string & align 8
        type = PTR;
    }
    else if (token == Sizeof) {
        assert(Sizeof); assert('(');
        type = parse_base_type();
        while (token == Mul) {assert(Mul); type = type + PTR;}
        assert(')');
        *++code = IMM;
        *++code = (type == CHAR) ? 1 : (type == INT ? 4 : 8); 
        type = INT64;
    }
    // handle identifer: variable or function all
    else if (token == Id) {
        assert(Id);   
        tmp_ptr = symbol_ptr; // for recursive parse
        // function call
        if (token == '(') {
            assert('(');
            i = 0; // number of args
            while (token != ')') {
                parse_expr(Assign);
                *++code = PUSH; i++;
                if (token == ',') assert(',');
            } assert(')');
            // native call
            if (tmp_ptr[Class] == Sys) *++code = tmp_ptr[Value];
            // fun call
            else if (tmp_ptr[Class] == Fun) {*++code = CALL; *++code = tmp_ptr[Value];}
            else {printf("%lld: invalid function call\n", line); exit(-1);}
            // delete stack frame for args
            if (i > 0) {*++code = DSAR; *++code = i;}
            type = tmp_ptr[Type];
        }
        // handle enum value
        else if (tmp_ptr[Class] == Num) {
            *++code = IMM; *++code = tmp_ptr[Value]; type = INT64;
        }
        // handle variables
        else {
            // local var, calculate addr base ibp
            if (tmp_ptr[Class] == Loc) {*++code = LEA; *++code = ibp - tmp_ptr[Value];}
            // global var
            else if (tmp_ptr[Class] == Glo) {*++code = IMM; *++code = tmp_ptr[Value];}
            else {printf("%lld: invalid variable\n", line); exit(-1);}
            type = tmp_ptr[Type];
            *++code = (type == CHAR) ? LC : LI;
        }
    }
}

void parse_stmt() {
    int64* a;
    int64* b;
    if (token == If) {
        assert(If); assert('('); parse_expr(Assign); assert(')');
        *++code = JZ; b = ++code; // JZ to false
        parse_stmt(); // parse true stmt
        if (token == Else) {
            assert(Else);
            *b = (int64)(code + 3); // write back false point
            *++code = JMP; b = ++code; // JMP to endif
            parse_stmt(); // parse false stmt
        }
        *b = (int64)(code + 1); // write back endif point
    }
    else if (token == While) {
        assert(While);
        a = code + 1; // write loop point
        assert('('); parse_expr(Assign); assert(')');
        *++code = JZ; b = ++code; // JZ to endloop
        parse_stmt();
        *++code = JMP; *++code = (int64)a; // JMP to loop point
        *b = (int64)(code + 1); // write back endloop point
    }
    else if (token == Return) {
        assert(Return);
        if (token != ';') parse_expr(Assign);
        assert(';');
        *++code = RET;
    }
    else if (token == '{') {
        assert('{');
        while (token != '}') parse_stmt(Assign);
        assert('}');
    }
    else if (token == ';') assert(';');
    else {parse_expr(Assign); assert(';');}
}

void parse_fun() {
    int64 type;
    i = ibp + 1; // keep space for bp
    // local variables must be declare in advance 
    while (token == Char || token == Int || token == Int64) {
        type = parse_base_type();
        while (token != ';') {
            // parse pointer's star
            while (token == Mul) {assert(Mul); type = type + PTR;}
            check_local_id(); assert(Id);
            hide_global();
            symbol_ptr[Class] = Loc;
            symbol_ptr[Type] = type;
            symbol_ptr[Value] = i++;
            if (token == ',') assert(',');
        }
        assert(';');
    }
    // new stack frame for vars
    *++code = NSVA;
    // stack frame size
    *++code = i - ibp;
    parse_stmt();
    *++code = RET;
    // recover global variables
    symbol_ptr = symbol_table;
    while (symbol_ptr[Token]) {
        if (symbol_ptr[Class] == Loc) recover_global();
        symbol_ptr = symbol_ptr + SymSize;
    }
}

void parse() {
    int64 type;
    token = 1; // just for loop condition
    while (token > 0) {
        tokenize(); // start or skip last ; | }
        // parse enum
        if (token == Enum) {
            assert(Enum);
            if (token != '{') assert(Id); // skip enum name
            assert('{'); parse_enum(); assert('}');
        } else {
            type = parse_base_type();
            // parse var or func definition
            while (token != ';' && token != '}') {
                // parse pointer's star
                while (token == Mul) {assert(Mul); type = type + PTR;}
                check_new_id();
                assert(Id);
                symbol_ptr[Type] = type;
                if (token == '(') {
                    // function
                    symbol_ptr[Class] = Fun;
                    symbol_ptr[Value] = (int64)(code + 1);
                    assert('('); parse_param(); assert(')');
                    parse_fun();
                } else {
                    // variable
                    symbol_ptr[Class] = Glo;
                    symbol_ptr[Value] = (int64)data;
                    data = data + 8; // keep 64 bits for each var
                }
                // handle int a,b,c;
                if (token == ',') assert(',');
            }
        }  
    }
}

void keyword() {
    char* keyword;
    keyword = "char int int64 enum if else return sizeof while "
        "open read close printf malloc free memset memcmp exit void main";
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
    if (!(symbol_table = malloc(MAX_SIZE / 16))) {
        printf("could not malloc(%lld) for symbol_table\n", MAX_SIZE / 16);
        return -1;
    }
    memset(code, 0, MAX_SIZE);
    memset(data, 0, MAX_SIZE);
    memset(stack, 0, MAX_SIZE);
    memset(symbol_table, 0, MAX_SIZE / 16);
    // init register
    bp = sp = stack + MAX_SIZE; // stack downwards
    ax = 0;
    return 0;
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
        // new stack frame for vars: save bp, bp -> caller stack, stack add frame
        else if (op == NSVA)    {*--sp = (int64)bp; bp = sp; sp = sp - *pc++;}
        // delete stack frame for args: same as x86 : add esp, <size>
        else if (op == DSAR)    sp = sp + *pc++;
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
    return 0;
}

int load_src(char* file) {
    int64 fd;
    int64 cnt;
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
    if ((cnt = read(fd, src, MAX_SIZE - 1)) <= 0) {
        printf("could not read source code(%lld)\n", cnt);
        return -1;
    }
    src[cnt] = 0; // EOF
    close(fd);
    return 0;
}

int main(int argc, char** argv) {
    // load source code
    if (load_src(*(argv+1)) != 0) return -1;
    // init memory & register
    if (init_vm() != 0) return -1;
    // prepare keywords for symbol table
    keyword();
    // parse and generate vm instructions, save to vm
    parse();
    // run vm and execute instructions
    return run_vm();
}
