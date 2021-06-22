
// JULIO 2020. CÉSAR BORAO MORATINOS: commproc.c

#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <glob.h>
#include <fcntl.h>
#include "commproc.h"

// SPECIFIC SHELL PROCEDURES TO RUN PIPELINE

// search the command in "varpath" variable. return 1 if success, 0 if fail.
int
isinvar(char *command, char *commpath, char *varpath) {

	char *newpath;
	char path[strlen(varpath)+1];
	strncpy(path,varpath,strlen(varpath)+1);
	char *pointer = path;

	while((newpath = strtok_r(pointer,":",&pointer)) != NULL) {
		size_t size = strlen(newpath)+strlen("/")+strlen(command)+1;
		char name[size];
		snprintf(name,size,"%s/%s",newpath,command);

		if (access(name,X_OK) == 0) {
			strncpy(commpath,name,strlen(name)+1);
			return 1;
		}
	}
	return 0;
}

// search the command in working directory. Return 1 if success, 0 if fail.
int
isinwd(char *command, char *commpath) {

	char buffer[Max_Buff];

	getcwd(buffer,Max_Buff);
	size_t size = strlen(buffer)+strlen("/")+strlen(command)+1;
	char name[size];
	snprintf(name,size,"%s/%s",buffer,command);

	if (access(name,X_OK) == 0) {
		strncpy(commpath,name,strlen(name)+1);
		return 1;
	}
	return 0;
}

// CHECK PROCEDURES

// checkconfig(): get the command line configuration about number of pipes and commands.
void
checkconfig(char *line, Lineinfo *config) {

        char *com;
        int count = 0;
	char *pointer;
	char copy[strlen(line)+1];
	strncpy(copy,line,strlen(line)+1);
	pointer = copy;

        while ((com = strtok_r(pointer,"|",&pointer)) != NULL)
                count++;

        config->numofcom = count;
        config->numofpipes = count-1;
}

// checkredirect(): get all the information about command line redirections.
void
checkredirect(char *line, Lineinfo *config) {

        char *pointer;
        char *infile;
	char *outfile;
	config->infile[0] = '\0';
	config->outfile[0] = '\0';

        char copy[strlen(line)+1];
        strncpy(copy,line,strlen(line)+1);

        if ((pointer = strchr(copy,'<')) != NULL) {
                ++pointer;
                infile = strtok_r(pointer," \t",&pointer);
		if ((strncmp(infile,"$",strlen("$")) == 0) && getenv(++infile) != NULL)
			infile = getenv(infile);

                strncpy(config->infile,infile,strlen(infile)+1);
        }

        strncpy(copy,line,strlen(line)+1);
        if ((pointer = strchr(line,'>')) != NULL) {
		++pointer;
                outfile = strtok_r(pointer," \t",&pointer);
		if ((strncmp(outfile,"$",strlen("$")) == 0) && getenv(++outfile) != NULL)
			outfile = getenv(outfile);

                strncpy(config->outfile,outfile,strlen(outfile)+1);
        }
}

// checkbg(): return 1 if command line is in background, 0 if not.
int
checkbg(char *line, int *inbg) {

	char copy[strlen(line)+1];
	strncpy(copy,line,strlen(line)+1);

	if (strchr(copy,'&') != NULL) {
                *inbg = 1;
		return 1;
	}
        *inbg = 0;
	return 0;
}

// checkhere(): if the command line has "HERE{}", load it into the config herebuffer
void
checkhere(char *line, Lineinfo *config, char *herebuff) {

	config->herebuff[0] = '\0';
	if (strstr(line,"<") == NULL && strstr(line,">") == NULL &&
	    strstr(line,"&") == NULL && strcmp(herebuff,"\0") != 0) {

		strncpy(config->herebuff,herebuff,strlen(herebuff)+1);
	}
}

// loadpath(): if the path exists, load it in "commpath" and return 1, else return 0.
int
loadpath(char *command, char *commpath) {

	if (isinwd(command,commpath))
		return 1;

	if (isinvar(command,commpath,getenv("PATH")))
		return 1;

	warnx("error: command %s does not exist",command);
	return -1;
}

// loadarguments(): load tokenized[] into arguments[] avoiding redirections and bg indications
void
loadarguments(char *tokenized[], char *arguments[]) {

	int i = 0;
	while (tokenized[i] != NULL && strcmp(tokenized[i],"&") != 0 && strcmp(tokenized[i],"<") != 0
	 	&& strcmp(tokenized[i],">") != 0 && strcmp(tokenized[i],"HERE{") != 0) {

		if ((arguments[i] = malloc(strlen(tokenized[i])+1)) == NULL)
			errx(EXIT_FAILURE,"fatal error: malloc failed, not enough memory");

		strncpy(arguments[i],tokenized[i],strlen(tokenized[i])+1);
		i++;

	}
	arguments[i] = NULL;
}

// loadposition(): load the current position of the command in the pipeline
int
loadposition(Cmd *input, Lineinfo config, int position) {

	if (position == config.numofcom && position != 1) {
		input->position = Last;
		return input->position;
	}
        input->position = position;
        return input->position;
}

// loadcmd(): run all load procedures
int
loadcmd(Lineinfo config, Cmd *input, char *tokenized[], int position) {

	if (loadpath(tokenized[0],input->path) < 0)
		return -1;

	loadarguments(tokenized,input->arguments);
	loadposition(input,config,position);
	return 1;
}

// PIPELINE GENERIC PROCEDURES

// createpipes(): create n pipes and load pipes in array "pipes". (1 if success, else -1)
int
createpipes(int numofpipes, int pipes[Max_Pipes][Pipe_Len]) {

	for (int i = 0; i < numofpipes; i++) {
		if (pipe(pipes[i]) < 0) {
			warnx("error: cannot create pipes");
			return -1;
		}
	}
	return 1;
}

// hereredir(): do redirection in case of "HERE{}" instruction
int
hereredir(Lineinfo config) {

	int fd[Pipe_Len];
	int status;

	if (pipe(fd) < 0) {
		warnx("error: cannot create pipes");
		return -1;
	}

	switch (fork()) {
		case -1:
			warnx("error: cannot fork");
			return -1;

		case 0:
			if (close(fd[0]) < 0) {
				warnx("error: cannot close fd %d",fd[0]);
				return -1;
			}

			if ((write(fd[1],&config.herebuff,strlen(config.herebuff))) != strlen(config.herebuff)) {
				warnx("error: cannot write");
				return -1;
			}

			if (close(fd[1]) < 0) {
				warnx("error: cannot close fd %d",fd[1]);
				return -1;
			}
			exit(EXIT_SUCCESS);

		default:
			if (close(fd[1]) < 0) {
				warnx("error: cannot close fd %d",fd[1]);
				return -1;
			}

			if (dup2(fd[0],0) < 0) {
				warnx("error: dup2 failed in fd: %d",fd[0]);
				return -1;
			}

			if (close(fd[0]) < 0) {
				warnx("error: cannot close fd %d",fd[0]);
				return -1;
			}
			wait(&status);
			return 1;
	}
}

// firstredir(): do redirections in case First
int
firstredir(Cmd input, Lineinfo config) {

	// if pipeline has more than 1 command
	if (config.numofpipes > 0)
		if (dup2(config.pipes[0][1],1) < 0) {
			warnx("error: dup2 failed in fd: %d",config.pipes[0][1]);
			return -1;
		}
	// if pipeline has input redirection
	if (strcmp(config.infile,"\0") != 0) {
		int fin;
		if ((fin = open(config.infile,O_RDONLY)) == -1) {
			warnx("error: open failed: %s",config.infile);
			return -1;
		}
		if (dup2(fin,0) < 0) {
			warnx("error: dup2 failed in fd: %d",fin);
			return -1;
		}
		if (close(fin) < 0) {
			warnx("error: close failed in fd: %d",fin);
			return -1;
		}
	}

	// if pipeline is one command and has output redirection
	if (config.numofpipes == 0 && strcmp(config.outfile,"\0") != 0) {
		int fout;
		if ((fout = open(config.outfile, O_CREAT | O_WRONLY | O_TRUNC, 0640)) == -1) {
			warnx("error: open failed: %s",config.outfile);
			return -1;
		}
		if (dup2(fout,1) < 0) {
			warnx("error: dup2 failed in fd: %d",fout);
			return -1;
		}
		 if (close(fout) < 0) {
			warnx("error: close failed in fd: %d",fout);
 			return -1;
		 }
	}

	// if pipeline has "HERE{}" redirection
	if (strcmp(config.herebuff,"\0") != 0) {
		if (hereredir(config) < 0)
			return -1;
	}

	// if pipeline has & (background) and not an input redirection
	if (config.inbg && !strcmp(config.infile,"\0") == 0) {
		int fnull;
		if ((fnull = open("/dev/null", O_RDONLY)) == -1) {
			warnx("error: open failed: /dev/null");
			return -1;
		}
		if (dup2(fnull,0) < 0) {
			warnx("error: dup2 failed in fd: /dev/null");
			return -1;
		}
		if (close(fnull) < 0) {
			warnx("error: close failed in fd: %d",fnull);
			return -1;
		}
	}
	return 1;
}

// lastredir(): do redirections in case Last
int
lastredir(Cmd input, Lineinfo config) {

	// last command input redirected to previous output
	if (dup2(config.pipes[config.numofpipes-1][0],0) < 0) {
		warnx("error: dup2 failed in fd: %d",config.pipes[config.numofpipes-1][0]);
		return -1;
	}

	// if pipeline has output redirection
	if (strcmp(config.outfile,"\0") != 0) {
		int fout;
		if ((fout = open(config.outfile, O_CREAT | O_WRONLY | O_TRUNC, 0640)) == -1) {
			warnx("error: open failed: %s",config.outfile);
			return -1;
		}
		if (dup2(fout,1) < 0) {
			warnx("error: dup2 failed in fd: %d",fout);
			return -1;
		}
		if (close(fout) < 0) {
			warnx("error: close failed in fd: %d",fout);
			return -1;
		}
	}
	return 1;
}

// dupandclose(): do dups and close fd (of pipes)
int
dupandclose(Cmd input,  Lineinfo config) {

	switch (input.position) {
		case First:
			if (firstredir(input,config) < 0)
				return -1;
			break;

		case Last:
			if (lastredir(input,config) < 0)
				return -1;
			break;

		default:
			if (dup2(config.pipes[input.position-2][0],0) < 0) {
				warnx("error: dup2 failed in fd: %d",config.pipes[input.position-2][0]);
				return -1;
			}
			if (dup2(config.pipes[input.position-1][1],1) < 0) {
				warnx("error: dup2 failed in fd: %d",config.pipes[input.position-1][1]);
				return -1;
			}
	}

	// closing pipes
	for (size_t i = 0; i < config.numofpipes; i++) {
		if (close(config.pipes[i][0]) < 0) {
			warnx("error: close failed in fd: %d",config.pipes[i][0]);
			return -1;
		}
		if (close(config.pipes[i][1]) < 0) {
			warnx("error: close failed in fd: %d",config.pipes[i][1]);
			return -1;
		}
	}
	return 1;
}

// execute the command
int
execmd(Cmd input, Lineinfo config, int *pid) {

	switch (*pid=fork()) {
		case -1:
			errx(EXIT_FAILURE, "error: cannot fork");
		case 0:
			if (dupandclose(input,config) < 0)
				return -1;

			execv(input.path,input.arguments);
			errx(EXIT_FAILURE, "error: cannot execv");
	}
	return 1;
}
