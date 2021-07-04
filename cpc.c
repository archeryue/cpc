#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#define int int64_t

int MAX_SIZE = 128 * 1024 * 8; // 1MB = 128k * 64bit

int * code,         // code segment
    * code_dump,    // for dump
    * stack;        // stack segment
char* data;         // data segment

int * pc,           // pc register
    * sp,           // rsp register
    * bp;           // rbp register

int ax,             // common register
    cycle;

// instruction set: copy from c4, change JSR/ENT/ADJ/LEV/BZ/BNZ to CALL/NSVA/DSAR/RET/JZ/JNZ.
enum {IMM, LEA, JMP, JZ, JNZ, CALL, NSVA, DSAR, RET, LI, LC, SI, SC, PUSH,
    OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV,
    MOD, OPEN, READ, CLOS, PRTF, MALC, FREE, MSET, MCMP, EXIT};

// classes/keywords, Do not support for.
enum {Num = 128, Fun, Sys, Glo, Loc, Id,
    Char, Int, Enum, If, Else, Return, Sizeof, While,
    // operators in precedence order.
    Assign, Cond, Lor, Land, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge,
    Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak};

// fields of symbol_table: copy from c4, rename HXX to GXX
enum {Token, Hash, Name, Class, Type, Value, GClass, GType, GValue, SymSize};

// types of variables & functions in symbol_table
enum {CHAR, INT, PTR};

// src code & dump
char* src,
    * src_dump;

// symbol table & pointer
int * symbol_table,
    * symbol_ptr,
    * main_ptr;

int token, token_val;
int line;
int i; // reuse index var

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
            symbol_ptr[Name] = (int)ch_ptr;
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
            if (token == '"') token_val = (int)ch_ptr; 
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
        else if (token == '&') {if (*src == '&') {src++; token = Land;} else token = And; return;}
        else if (token == '^') {token = Xor; return;}
        else if (token == '%') {token = Mod; return;}
        else if (token == '*') {token = Mul; return;}
        else if (token == '[') {token = Brak; return;}
        else if (token == '?') {token = Cond; return;}
        else if (token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ',' || token == ':') return;
    }
}

void assert(int tk) {
    if (token != tk) {
        printf("expect token: %lld(%c), get: %lld(%c)\n", tk, (char)tk, token, (char)token);
        exit(-1);
    }
    tokenize();
}

void check_local_id() {
    if (token != Id) {printf("line %lld: invalid identifer\n", line); exit(-1);}
    if (symbol_ptr[Class] == Loc) {
        printf("line %lld: duplicate declaration\n", line);
        exit(-1);
    }
}

void check_new_id() {
    if (token != Id) {printf("line %lld: invalid identifer\n", line); exit(-1);}
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
        symbol_ptr[Type] = INT;
        symbol_ptr[Value] = i++;
        if (token == ',') tokenize();
    }
}

int parse_base_type() {
    // parse base type
    if (token == Char) {assert(Char); return CHAR;}
    else {assert(Int); return INT;}
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

int ibp;

void parse_param() {
    int type;
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

int type; // pass type in recursive parse expr

void parse_expr(int precd) {
    int tmp_type;
    int* tmp_ptr;
    // const number
    if (token == Num) {
        tokenize();
        *++code = IMM;
        *++code = token_val;
        type = INT;
    } 
    // const string
    else if (token == '"') {
        *++code = IMM;
        *++code = token_val; // string addr
        assert('"'); while (token == '"') assert('"'); // handle multi-row
        data = (char*)((int)data + 8 & -8); // add \0 for string & align 8
        type = PTR;
    }
    else if (token == Sizeof) {
        tokenize(); assert('(');
        type = parse_base_type();
        while (token == Mul) {assert(Mul); type = type + PTR;}
        assert(')');
        *++code = IMM;
        *++code = (type == CHAR) ? 1 : 8; 
        type = INT;
    }
    // handle identifer: variable or function all
    else if (token == Id) {
        tokenize();
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
            else {printf("line %lld: invalid function call\n", line); exit(-1);}
            // delete stack frame for args
            if (i > 0) {*++code = DSAR; *++code = i;}
            type = tmp_ptr[Type];
        }
        // handle enum value
        else if (tmp_ptr[Class] == Num) {
            *++code = IMM; *++code = tmp_ptr[Value]; type = INT;
        }
        // handle variables
        else {
            // local var, calculate addr base ibp
            if (tmp_ptr[Class] == Loc) {*++code = LEA; *++code = ibp - tmp_ptr[Value];}
            // global var
            else if (tmp_ptr[Class] == Glo) {*++code = IMM; *++code = tmp_ptr[Value];}
            else {printf("line %lld: invalid variable\n", line); exit(-1);}
            type = tmp_ptr[Type];
            *++code = (type == CHAR) ? LC : LI;
        }
    }
    // cast or parenthesis
    else if (token == '(') {
        assert('(');
        if (token == Char || token == Int) {
            tmp_type = token - Char + CHAR;
            while (token == Mul) {assert(Mul); tmp_type = tmp_type + PTR;}
            // use precedence Inc represent all unary operators
            assert(')'); parse_expr(Inc); type = tmp_type;
        } else {
            parse_expr(Assign); assert(')');
        }
    }
    // derefer
    else if (token == Mul) {
        tokenize(); parse_expr(Inc);
        if (type >= PTR) type = type - PTR;
        else {printf("line %lld: invalid dereference\n", line); exit(-1);}
        *++code = (type == CHAR) ? LC : LI;
    }
    // reference
    else if (token == And) {
        tokenize(); parse_expr(Inc);
        if (*code == LC || *code == LI) code--; // rollback load by addr
        else {printf("line %lld: invalid reference\n", line); exit(-1);}
        type = type + PTR;
    }
    // Not
    else if (token == '!') {
        tokenize(); parse_expr(Inc);
        *++code = PUSH; *++code = IMM; *++code = 0; *++code = EQ;
        type = INT;
    }
    // bitwise
    else if (token == '~') {
        tokenize(); parse_expr(Inc);
        *++code = PUSH; *++code = IMM; *++code = -1; *++code = XOR;
        type = INT;
    }
    // positive
    else if (token == And) {tokenize(); parse_expr(Inc); type = INT;}
    // negative
    else if (token == Sub) {
        tokenize(); parse_expr(Inc);
        *++code = PUSH; *++code = IMM; *++code = -1; *++code = MUL;
        type = INT;
    }
    // ++var --var
    else if (token == Inc || token == Dec) {
        i = token; tokenize(); parse_expr(Inc);
        // save var addr, then load var val
        if (*code == LC) {*code = PUSH; *++code = LC;}
        else if (*code == LI) {*code = PUSH; *++code = LI;}
        else {printf("line %lld: invalid Inc or Dec\n", line); exit(-1);}
        *++code = PUSH; // save var val
        *++code = IMM; *++code = (type > PTR) ? 8 : 1;
        *++code = (i == Inc) ? ADD : SUB; // calculate
        *++code = (type == CHAR) ? SC : SI; // write back to var addr
    }
    else {printf("line %lld: invalid expression\n", line); exit(-1);}
    // use [precedence climbing] method to handle binary(or postfix) operators
    while (token >= precd) {
        tmp_type = type;    
        // assignment
        if (token == Assign) {
            tokenize();
            if (*code == LC || *code == LI) *code = PUSH;
            else {printf("line %lld: invalid assignment\n", line); exit(-1);}
            parse_expr(Assign); type = tmp_type; // type can be cast
            *++code = (type == CHAR) ? SC : SI;
        }
        // ? :, same as if stmt
        else if (token == Cond) {
            tokenize(); *++code = JZ; tmp_ptr = ++code;
            parse_expr(Assign); assert(':');
            *tmp_ptr = (int)(code + 3);
            *++code = JMP; tmp_ptr = ++code; // save endif addr
            parse_expr(Cond);
            *tmp_ptr = (int)(code + 1); // write back endif point
        }
        // logic operators, simple and boring, copy from c4
        else if (token == Lor) {
            tokenize(); *++code = JNZ; tmp_ptr = ++code;
            parse_expr(Land); *tmp_ptr = (int)(code + 1); type = INT;}
        else if (token == Land) {
            tokenize(); *++code = JZ; tmp_ptr = ++code;
            parse_expr(Or); *tmp_ptr = (int)(code + 1); type = INT;}
        else if (token == Or)  {tokenize(); *++code = PUSH; parse_expr(Xor); *++code = OR;  type = INT;}
        else if (token == Xor) {tokenize(); *++code = PUSH; parse_expr(And); *++code = XOR; type = INT;}
        else if (token == And) {tokenize(); *++code = PUSH; parse_expr(Eq);  *++code = AND; type = INT;}
        else if (token == Eq)  {tokenize(); *++code = PUSH; parse_expr(Lt);  *++code = EQ;  type = INT;}
        else if (token == Ne)  {tokenize(); *++code = PUSH; parse_expr(Lt);  *++code = NE;  type = INT;}
        else if (token == Lt)  {tokenize(); *++code = PUSH; parse_expr(Shl); *++code = LT;  type = INT;}
        else if (token == Gt)  {tokenize(); *++code = PUSH; parse_expr(Shl); *++code = GT;  type = INT;}
        else if (token == Le)  {tokenize(); *++code = PUSH; parse_expr(Shl); *++code = LE;  type = INT;}
        else if (token == Ge)  {tokenize(); *++code = PUSH; parse_expr(Shl); *++code = GE;  type = INT;}
        else if (token == Shl) {tokenize(); *++code = PUSH; parse_expr(Add); *++code = SHL; type = INT;}
        else if (token == Shr) {tokenize(); *++code = PUSH; parse_expr(Add); *++code = SHR; type = INT;}
        // arithmetic operators
        else if (token == Add) {
            tokenize(); *++code = PUSH; parse_expr(Mul);
            // int pointer * 8
            if (tmp_type > PTR) {*++code = PUSH; *++code = IMM; *++code = 8; *++code = MUL;}
            *++code = ADD; type = tmp_type;
        }
        else if (token == Sub) {
            tokenize(); *++code = PUSH; parse_expr(Mul);
            if (tmp_type > PTR && tmp_type == type) {
                // pointer - pointer, ret / 8
                *++code = SUB; *++code = PUSH;
                *++code = IMM; *++code = 8;
                *++code = DIV; type = INT;}
            else if (tmp_type > PTR) {
                *++code = PUSH;
                *++code = IMM; *++code = 8;
                *++code = MUL;
                *++code = SUB; type = tmp_type;}
            else *++code = SUB;
        }
        else if (token == Mul) {tokenize(); *++code = PUSH; parse_expr(Inc); *++code = MUL; type = INT;}
        else if (token == Div) {tokenize(); *++code = PUSH; parse_expr(Inc); *++code = DIV; type = INT;}
        else if (token == Mod) {tokenize(); *++code = PUSH; parse_expr(Inc); *++code = MOD; type = INT;}
        // var++, var--
        else if (token == Inc || token == Dec) {
            *++code = PUSH; // just modify value in mem, not register
            *++code = IMM; *++code = (type > PTR) ? 8 : 1;
            *++code = (token == Inc) ? ADD : SUB;
            *++code = (type == CHAR) ? SC : SI;
            *++code = PUSH; // restore ax for current expr calculate
            *++code = IMM; *++code = (type > PTR) ? sizeof(int) : sizeof(char);
            *++code = (token == Inc) ? SUB : ADD;
            tokenize();
        }
        // a[x] = *(a + x)
        else if (token == Brak) {
            assert(Brak); *++code = PUSH; parse_expr(Assign); assert(']');
            if (tmp_type > PTR) {*++code = PUSH; *++code = IMM; *++code = 8; *++code = MUL;}
            else if (tmp_type < PTR) {printf("line %lld: invalid index op\n", line); exit(-1);}
            *++code = ADD; type = tmp_type - PTR;
            *++code = (type == CHAR) ? LC : LI;
        }
        else {printf("%lld: invlid token=%lld\n", line, token); exit(-1);}
    }
}

void parse_stmt() {
    int* a;
    int* b;
    if (token == If) {
        assert(If); assert('('); parse_expr(Assign); assert(')');
        *++code = JZ; b = ++code; // JZ to false
        parse_stmt(); // parse true stmt
        if (token == Else) {
            assert(Else);
            *b = (int)(code + 3); // write back false point
            *++code = JMP; b = ++code; // JMP to endif
            parse_stmt(); // parse false stmt
        }
        *b = (int)(code + 1); // write back endif point
    }
    else if (token == While) {
        assert(While);
        a = code + 1; // write loop point
        assert('('); parse_expr(Assign); assert(')');
        *++code = JZ; b = ++code; // JZ to endloop
        parse_stmt();
        *++code = JMP; *++code = (int)a; // JMP to loop point
        *b = (int)(code + 1); // write back endloop point
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
    int type;
    i = ibp + 1; // keep space for bp
    // local variables must be declare in advance 
    while (token == Char || token == Int) {
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
    int type;
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
                    symbol_ptr[Value] = (int)(code + 1);
                    assert('('); parse_param(); assert(')');
                    parse_fun();
                } else {
                    // variable
                    symbol_ptr[Class] = Glo;
                    symbol_ptr[Value] = (int)data;
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
    keyword = "char int int enum if else return sizeof while "
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
    int op;
    int* tmp;
    while (1) {
        op = *pc++; // read instruction
        // load & save
        if (op == IMM)          ax = *pc++;                     // load immediate(or global addr)
        else if (op == LEA)     ax = (int)bp + *pc++;           // load local addr
        else if (op == LC)      ax = *(char*)ax;                // load char
        else if (op == LI)      ax = *(int*)ax;                 // load int
        else if (op == SC)      *(char*)*sp++ = ax;             // save char to stack
        else if (op == SI)      *(int*)*sp++ = ax;              // save int to stack
        else if (op == PUSH)    *--sp = ax;                     // push ax to stack
        // jump
        else if (op == JMP)     pc = (int*)*pc;                 // jump
        else if (op == JZ)      pc = ax ? pc + 1 : (int*)*pc;   // jump if ax == 0
        else if (op == JNZ)     pc = ax ? (int*)*pc : pc + 1;   // jump if ax != 0
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
        else if (op == CALL)    {*--sp = (int)(pc+1); pc = (int*)*pc;}
        // new stack frame for vars: save bp, bp -> caller stack, stack add frame
        else if (op == NSVA)    {*--sp = (int)bp; bp = sp; sp = sp - *pc++;}
        // delete stack frame for args: same as x86 : add esp, <size>
        else if (op == DSAR)    sp = sp + *pc++;
        // return caller: retore stack, retore old bp, pc point to caller code addr(store by CALL) 
        else if (op == RET)     {sp = bp; bp = (int*)*sp++; pc = (int*)*sp++;}        
        // end for call function.
        // native call
        else if (op == OPEN)    {ax = open((char*)sp[1], sp[0]);}
        else if (op == CLOS)    {ax = close(*sp);}
        else if (op == READ)    {ax = read(sp[2], (char*)sp[1], *sp);}
        else if (op == PRTF)    {tmp = sp + pc[1]; ax = printf((char*)tmp[-1], tmp[-2], tmp[-3],
                                                                    tmp[-4], tmp[-5], tmp[-6]);}
        else if (op == MALC)    {ax = (int)malloc(*sp);}
        else if (op == FREE)    {free((void*)*sp);}
        else if (op == MSET)    {ax = (int)memset((char*)sp[2], sp[1], *sp);}
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
    int fd;
    int cnt;
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

// after bootstrap use [int] istead of [int32_t]
int32_t main(int32_t argc, char** argv) {
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
