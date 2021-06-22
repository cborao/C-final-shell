
// JULIO 2020. CÉSAR BORAO MORATINOS: shellproc.c

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <glob.h>
#include "shellproc.h"

// GENERIC SHELL PROCEDURES

// peek(): take a look of the first command in the line
char *
peek(char *line) {

	char *token;
        char *pointer;;
	char copy[strlen(line)];
	strncpy(copy,line,strlen(line)+1);
	pointer = copy;

	if ((token = strtok_r(pointer," \t",&pointer)) == NULL)
		return NULL;
	return token;
}

// hasglob(): return 1 if is a globbing caracter or 0 if not
int
hasglob(char *arg) {
	if (strchr(arg,'*') != NULL || strchr(arg,'?') != NULL || strchr(arg,'[') != NULL)
		return 1;
	return 0;
}

// globtrans(): make the globbing translation
int
globtrans(char *arg, char *tokenized[], int *offset) {

	glob_t glob_buffer;
	const char * pattern = arg;
	int match_count;

	if (hasglob(arg)) {
		glob(pattern,0,NULL,&glob_buffer);
		match_count = glob_buffer.gl_pathc;
		if (match_count == 0) {
			warnx("error in glob: match not found with %s pattern",pattern);
			return -1;
		}

		for (size_t j = 0; j < match_count; j++) {
			tokenized[*offset] = malloc(strlen(glob_buffer.gl_pathv[j])+1);
			if (tokenized[*offset] == NULL)
				errx(EXIT_FAILURE,"fatal error: malloc failed, not enough memory");

			strncpy(tokenized[*offset],glob_buffer.gl_pathv[j],strlen(glob_buffer.gl_pathv[j])+1);
			*offset += 1;
		}
		globfree(&glob_buffer);
	}
	return 1;
}

// tokenize(): return the line tokenized with glob and variable translation
int
tokenize(char *str, char* result[]) {
	int i = 0;
	char *token;
        char *pointer;
	char copy[strlen(str)];
	strncpy(copy,str,strlen(str)+1);
	pointer = copy;

	while((token = strtok_r(pointer," \t", &pointer)) != NULL && i < Max_Tok) {
		char *first = token;

		// variable translation
		if ((strncmp(first,"$",strlen("$")) == 0)) {
			first++;

			if ((token = getenv(first)) == NULL) {
				warnx("error: var %s does not exists",first);
				return -1;
			}
		}

		// globbing substitution
		if (hasglob(token)) {
			if (globtrans(token,result,&i) < 0)
				return -1;
		} else {
			result[i] = malloc(strlen(token)+1);
			strncpy(result[i],token,strlen(token)+1);
			i++;
		}
	}
	result[i] = NULL;
	return 1;
}

// free the malloc memory of other procedures
void
freedata(char *Data[]) {

	int i = 0;
	while (Data[i] != NULL) {
		free(Data[i]);
		i++;
	}
}

// cd(): built-in command to change the current directory
int
cd(char *line) {
	char *tokenized[Max_Tok];

	if (tokenize(line,tokenized) < 0)
		return -1;

	if (tokenized[1] != NULL) {
		if (chdir(tokenized[1]) < 0)
			return -1;

		return 1;
	}
	chdir(getenv("HOME"));
	freedata(tokenized);
	return 1;
}

// setvar(): built-in command to set an environment variable = value
int
setvar(char *buff) {
	char *pointer = buff;
	char *var = strtok_r(pointer, "=", &pointer);
	char *value = strtok_r(pointer, "=", &pointer);
	if (setenv(var,value,1) < 0)
		return -1;
	return 1;
}
