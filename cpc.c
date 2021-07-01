#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

uint32_t MAX_SIZE = 256 * 1024 * 4; // 1MB = 256k*32bit
uint32_t token;
char* src;

uint32_t* code, 		// code segment
		* code_dump, 	// for dump
		* stack;    	// stack
char* data;       		// data segment

uint32_t* pc,
		* sp,
		* bp;

uint32_t ax, 	// common register
		 cycle;

void tokenizer() {
	// todo
}

void parser() {
	// todo
}

void expEval() {
	// todo
}

int initVM() {
	// allocate memory for virtual machine
    if (!(code = code_dump = malloc(MAX_SIZE))) {
        printf("could not malloc(%d) for code segment\n", MAX_SIZE);
		return -1;
    }
    if (!(data = malloc(MAX_SIZE))) {
        printf("could not malloc(%d) for data segment\n", MAX_SIZE);
		return -1;
    }
    if (!(stack = malloc(MAX_SIZE))) {
        printf("could not malloc(%d) for stack segment\n", MAX_SIZE);
		return -1;
    }

    memset(code, 0, MAX_SIZE);
    memset(data, 0, MAX_SIZE);
    memset(stack, 0, MAX_SIZE);

	// init register
	bp = sp = stack + MAX_SIZE; // stack downwards
	ax = 0;

	return 0;
}

int runVM() {
	//todo
	return 0;
}

int main(int argc, char** argv) {
	FILE* fp;
    if ((fp = fopen(*(argv+1), "r")) == NULL) {
        printf("could not open source code(%s)\n", *(argv+1));
        return -1;
    }

    if (!(src = malloc(MAX_SIZE))) {
        printf("could not malloc(%d) for source area\n", MAX_SIZE);
        return -1;
    }

	char ch;
	int cnt = 0;
	while((ch = getc(fp)) != EOF) src[cnt++] = ch;
	src[cnt] = '\0';
	fclose(fp);

	// init memory & register
	if (initVM() != 0) return -1;
	// parser: tokenize & parse
	parser();

    return runVM();
}
