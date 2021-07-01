#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#define int64 long long

int64 MAX_SIZE = 256 * 1024;
int64 token;
char* src;

void tokenizer() {
	// todo
}

void parser() {
	// todo
}

void expEval() {
	// todo
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
        printf("could not malloc(%lld) for source area\n", MAX_SIZE);
        return -1;
    }

	char ch;
	int cnt = 0;
	while((ch = getc(fp)) != EOF) src[cnt++] = ch;
	src[cnt] = '\0';
	fclose(fp);

	parser();
    return runVM();
}
