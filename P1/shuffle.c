#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>


int main(int argc, char* argv[]) {
	FILE *input;
	FILE *output;
	int size, i, j, checked, front, back;
	size = 0;
	char *array;


//first check whether the argumnet is correct or not

	//check whether it has two '-i','-o' arguments
	if (argv[1][0] != '-' || argv[3][0] != '-' || argc != 5 ) {
		fprintf(stderr, "Usage: shuffle -i inputfile -o outputfile\n");
		exit(1);
	}
	//condition with -i first then -o
	else if (argv[1][1] == 'i' && argv[3][1] == 'o') {
		input = fopen(argv[2], "r");
		output = fopen(argv[4], "w");
		if (input == NULL) {
			fprintf(stderr, "Error: Cannot open file %s\n", argv[2]);
			exit(1);
		}
		else if (output == NULL) {
			fprintf(stderr, "Error: Cannot open file %s\n", argv[4]);
			exit(1);
		}

		else {
			fseek(input, 0, SEEK_END);
			size = ftell(input);
			rewind(input);
		}
	}
	//condition with -o first then -i
	else if (argv[1][1] == 'o' && argv[3][1] == 'i') {
		input = fopen(argv[4], "r");
		output = fopen(argv[2], "w");
		if (input == NULL) {
			fprintf(stderr, "Error: Cannot open file %s\n", argv[4]);
			exit(1);
		}
		else if (output == NULL) {
			fprintf(stderr, "Error: Cannot open file %s\n", argv[2]);
			exit(1);
		}

		else {
			fseek(input, 0, SEEK_END);
			size = ftell(input);
			rewind(input);
		}
	}
	else {
		fprintf(stderr, "Usage: shuffle -i inputfile -o outputfile\n");
		exit(1);
	}

	array = (char*) malloc (size);
	if (array == NULL) {
		fprintf(stderr, "Could not allocate array!");
		exit(1);
	}
	if (size != 0) {
		while ( !feof(input)) {
			fread((char*) array, size, 1, input);
		}
	}


	//printf("%s", array);
	//printf("%s", array + size);

	checked = 0;
	i = 0;
	j = size;
	front = 0;
	back = size;
	while ( front < back ) {
		if ( checked == 0) {
			if (*(array + i) == '\n') {
				fwrite((char*) (array + front), 1, i - front, output);
				checked = 1;
				front = i;
				//num = 0;
			}
			i++;
			//num = num + 1;
		}
		if ( checked == 1) {
			if (*(array + j) == '\n' && j == size - 1) {
				j = j - 2;
				back = back - 1;
			}
			else if (*(array + j) == '\n') {
				fwrite((char*) (array + j), 1, back - j, output);
				checked = 0;
				back = j;
			}
			j--;
		}

	}
	if (size != 0) {
		fwrite("\n", 1, 1, output);
	}



	fclose(input);
	fclose(output);
	return (0);

}


