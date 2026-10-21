// Не очень хорошая программа, которая
// в бесконечном цикле открывает файл, но никогда
// не закрывает его. Нужная для иллюстрации работы
// check_close.bt.

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

int main (int argc, char *argv[])
{
    if (argc != 2) {
   		printf(stderr, "Usage: %s <file>\n", argv[0]);
   		exit(EXIT_FAILURE);
    }

	int err;
    FILE *f;
	for(int i = 0; ; i++ ) {
		sleep(1);
		fprintf(stdout, "%d\n", i);
		f = fopen(argv[1], "r");
		if (f == NULL) {
			fprintf(stderr, "cannot open file: %s\n", strerror(errno));
			break;
		}
	}
	return 1;
}
