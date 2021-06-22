
// JULIO 2020. CÉSAR BORAO MORATINOS: commproc.h

enum {
	Max_Buff = 6000,
	Max_Toks = 100,
	Max_Path = 500,
	Max_Args = 30,
        Max_Pipes = 15,
        Pipe_Len = 2,
        First = 1,
	Last = -1,
};

struct Lineinfo {
	int pipes[Max_Pipes][Pipe_Len];
	int numofcom;
	int numofpipes;
	char infile[Max_Toks];
        char outfile[Max_Toks];
	char herebuff[Max_Toks];
	int inbg;
};

struct Cmd {
	char path[Max_Path];
	char *arguments[Max_Args];
	int position;
};

typedef struct Lineinfo	Lineinfo;
typedef struct Cmd Cmd;

// check procedures

void checkconfig(char *line, Lineinfo *config);
void checkredirect(char *line, Lineinfo *config);
int checkbg(char *line, int *inbg);
void checkhere(char *line, Lineinfo *config, char *herebuff);

// load procedure

int loadcmd(Lineinfo config, Cmd *input, char *tokenized[], int position);

// pipeline generic procedures

int createpipes(int numofpipes, int pipes[Max_Pipes][Pipe_Len]);
int dupandclose(Cmd input, Lineinfo config);
int execmd(Cmd input, Lineinfo config, int *pid);
