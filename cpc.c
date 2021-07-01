#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#define ok 0

int64_t MAX_SIZE = 128 * 1024 * 4; // 1MB = 128k * 64bit
int64_t token;
char* src;

int64_t * code, 		// code segment
		* code_dump, 	// for dump
		* stack;    	// stack
char* data;       		// data segment

int64_t	* pc,
		* sp,
		* bp;

int64_t ax, 	// common register
		cycle;

// instruction set: copy from c4, change ENT/ADJ/LEV to NSF/CSF/RET.
enum {LEA, IMM, JMP, CALL, JZ, JNZ, NSF, CSF, RET, LI, LC, SI, SC, PUSH,
	OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV,
	MOD, OPEN, READ, CLOS, PRTF, MALC, FREE, MSET, MCMP, EXIT};

void tokenize() {
	// todo
}

void parse() {
	// todo
}

void generate() {
	// todo
}

void expEval() {
	// todo
}

int initVM() {
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

int runVM() {
	int64_t op;
	int64_t* tmp;
	while (1) {
		op = *pc++; // read instruction
		// load & save
		if (op == IMM)			ax = *pc++;				// load immediate
		else if (op == LC)		ax = *(char*)ax;		// load char
		else if (op == LI)		ax = *(int64_t*)ax;         // load int
		else if (op == SC)		*(char*)*sp++ = ax;    	// save char to stack
		else if (op == SI)		*(int64_t*)*sp++ = ax;      // save int to stack
		else if (op == PUSH)	*--sp = ax;				// push ax to stack
		// jump
		else if (op == JMP)		pc = (int64_t*)*pc;	// jump
		else if (op == JZ)		pc = ax ? pc + 1 : (int64_t*)*pc; // jump if ax == 0
		else if (op == JNZ)		pc = ax ? (int64_t*)*pc : pc + 1; // jump if ax != 0
		// arithmetic
		else if (op == OR)		ax = *sp++ | ax;
		else if (op == XOR) 	ax = *sp++ ^ ax;
		else if (op == AND) 	ax = *sp++ & ax;
		else if (op == EQ)  	ax = *sp++ == ax;
		else if (op == NE)  	ax = *sp++ != ax;
		else if (op == LT)  	ax = *sp++ < ax;
		else if (op == LE)  	ax = *sp++ <= ax;
		else if (op == GT)  	ax = *sp++ >  ax;
		else if (op == GE)  	ax = *sp++ >= ax;
		else if (op == SHL) 	ax = *sp++ << ax;
		else if (op == SHR) 	ax = *sp++ >> ax;
		else if (op == ADD) 	ax = *sp++ + ax;
		else if (op == SUB) 	ax = *sp++ - ax;
		else if (op == MUL) 	ax = *sp++ * ax;
		else if (op == DIV) 	ax = *sp++ / ax;
		else if (op == MOD) 	ax = *sp++ % ax;
		// some complicate instructions for function call
		// call function: push pc + 1 to stack & pc jump to func addr(pc point to)
		else if (op == CALL) 	{*--sp = (int64_t)(pc+1); pc = (int64_t*)*pc;}
		// new stack frame: save bp, bp -> caller stack, stack add frame
		else if (op == NSF)  	{*--sp = (int64_t)bp; bp = sp; sp = sp - *pc++;}
		// clean stack frame: stack clean frame, same as x86 : add esp, <size>
		else if (op == CSF)		sp = sp + *pc++;
		// return caller: retore stack, retore old bp, pc point to caller code addr(store by CALL) 
		else if (op == RET)		{sp = bp; bp = (int64_t*)*sp++; pc = (int64_t*)*sp++;}		
		// load arguments address: load effective address
		else if (op == LEA)		ax = (int64_t)bp + *pc++;
		// end for call function.
		// native call
		else if (op == EXIT)	{printf("exit(%lld)\n", *sp); return *sp;}
		else if (op == OPEN)	{ax = open((char*)sp[1], sp[0]);}
		else if (op == CLOS)	{ax = close(*sp);}
		else if (op == READ)	{ax = read(sp[2], (char*)sp[1], *sp);}
		else if (op == PRTF)	{tmp = sp + pc[1]; ax = printf((char*)tmp[-1], tmp[-2], tmp[-3],
								tmp[-4], tmp[-5], tmp[-6]);}
		else if (op == MALC)	{ax = (int64_t)malloc(*sp);}
		else if (op == FREE)	{free((int64_t*)*sp);}
		else if (op == MSET)	{ax = (int64_t)memset((char*)sp[2], sp[1], *sp);}
		else if (op == MCMP)	{ax = memcmp((char*)sp[2], (char*)sp[1], *sp);}
		else {
			printf("unkown instruction: %lld\n", op);
			return -1;
		}
	}
	return ok;
}

int loadSourceCode(char* file) {
	int64_t fd;
	// use open/read/close for bootstrap.
    if ((fd = open(file, 0)) < 0) {
        printf("could not open source code(%s)\n", file);
        return -1;
    }
    if (!(src = malloc(MAX_SIZE))) {
        printf("could not malloc(%lld) for source area\n", MAX_SIZE);
        return -1;
    }
	int64_t cnt;
	if ((cnt = read(fd, src, MAX_SIZE - 1)) <= 0) {
		printf("could not read source code(%lld)\n", cnt);
		return -1;
	}
	src[cnt] = '\0';
	close(fd);
	return ok;
}

int main(int argc, char** argv) {
	// load source code
	if (loadSourceCode(*(argv+1)) != ok) return -1;
	// init memory & register
	if (initVM() != ok) return -1;
	// parse: tokenize & parse get AST
	parse();
	// generate instructions from AST for VM
	generate();
	/*int64_t i = 0;*/
    /*code[i++] = IMM;*/
    /*code[i++] = 10;*/
    /*code[i++] = PUSH;*/
    /*code[i++] = IMM;*/
    /*code[i++] = 20;*/
    /*code[i++] = ADD;*/
    /*code[i++] = PUSH;*/
    /*code[i++] = EXIT;*/
    /*pc = code;*/
	// run
    return runVM();
}
