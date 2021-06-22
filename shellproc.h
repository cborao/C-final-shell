
// JULIO 2020. CÉSAR BORAO MORATINOS: shellproc.h

enum {
	Max_Tok = 100
};

// generic shell procedures

char *peek(char *line);
int tokenize(char *str, char* result[]);
void freedata(char *Data[]);

// built-in commands

int cd(char *line);
int setvar(char *line);
