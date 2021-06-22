
// JULIO 2020. CÉSAR BORAO MORATINOS: shell.c

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
#include "shellproc.h"
#include "commproc.h"


// runcommands(): run each command in the pipeline
int
runcommands(char *line, Lineinfo config, int *pid) {

	char *comm;
	int commpos = 0;
	while ((comm = strtok_r(line, "|", &line)) != NULL) {
		Cmd input;
		char *tokenized[Max_Tok];

		if (tokenize(comm,tokenized) < 0)
			return -1;

		commpos++;
		if (loadcmd(config,&input,tokenized,commpos) < 0)
			return -1;

		if (execmd(input,config,pid) < 0)
			return -1;

		freedata(tokenized);
		freedata(input.arguments);
	}
	return 1;
}

// runfather(): collect the child's status
int
runfather(Lineinfo config, int pid) {

	int status;
	int result;

	for (size_t j = 0; j < config.numofpipes; j++) {
		if (close(config.pipes[j][0]) < 0) {
			warnx("error: cannot close fd %d", config.pipes[j][0]);
			return -1;
		}
		if (close(config.pipes[j][1]) < 0) {
			warnx("error: cannot close fd %d", config.pipes[j][1]);
			return -1;
		}
	}

	if (config.inbg)
		return -1;

	while ((result = wait(&status)) != -1 && WIFEXITED(status)) {
		if (result == pid)
			break;
	}
	char out[4];
	sprintf(out, "%d", WEXITSTATUS(status));
	if (setenv("result",out,1) < 0) {
		warnx("error: cannot update $result variable");
		return -1;
	}
	return 1;
}

// runpipeline(): load the pipeline config and run it
int
runpipeline(char *line, char herebuff[Max_Buff]) {

	Lineinfo config;
	int pid;

        checkconfig(line,&config);
        checkredirect(line,&config);
	checkbg(line,&config.inbg);
	checkhere(line,&config,herebuff);

	// create pipes (from commproc)
        if (createpipes(config.numofpipes,config.pipes) < 0)
		return -1;

	// run commands
	if (runcommands(line,config,&pid) < 0)
		return -1;

	// run father
	runfather(config,pid);
	return 1;
}

// loadhere(): load information inside "HERE{}" in a buffer
void
loadhere(char herebuff[]) {
	char tmpbuff[Max_Buff];
	char *line;

	line = fgets(tmpbuff,Max_Buff,stdin);
	while (strstr(line,"}") == NULL) {
		strncat(herebuff,tmpbuff,strlen(tmpbuff)+1);
		line = fgets(tmpbuff,Max_Buff,stdin);
	}
}

// resetbuff(): reset buffer
void
resetbuff(char buff[]) {
	for (size_t i = 0; i < Max_Buff; i++) {
		if (buff[i] == '\0')
			break;
		buff[i] = '\0';
	}
}

// commandselect: select the command between built-in commands (cd,ifnot,ifok,var=value) or pipeline
int
commselect(char *buff, char *herebuff) {

	char *comm;
	if ((comm = peek(buff)) != NULL) {

		if (strcmp(comm,"ifok") == 0 && strcmp(getenv("result"),"0") != 0) {
			if (setenv("result","1",1) < 0)
				warnx("error: cannot update $result variable");
			return -1;
		}

		if (strcmp(comm,"ifnot") == 0 && strcmp(getenv("result"),"0") == 0) {
			if (setenv("result","1",1) < 0)
				warnx("error: cannot update $result variable");
			return -1;
		}

		if (strcmp(buff,"ifok") == 0 || strcmp(buff,"ifnot") == 0)
			return -1;

		if (strcmp(comm,"ifok") == 0 || strcmp(comm,"ifnot") == 0) {
			buff = strstr(buff," ") + 1;
			comm = buff;
		}

		// built-in command: change working directory
		if (strcmp(comm,"cd") == 0) {
			if (cd(buff) < 0) {
				warnx("error: in cd, cannot change working directory");
				if (setenv("result","1",1) < 0)
					warnx("error: cannot update $result variable");
			}
			return 1;
		}

		// built-in command: set new variable var=value
		if (strchr(comm,'=') != NULL) {
			if (setvar(buff) < 0) {
				warnx("error: variable cannot set");
				if (setenv("result","1",1) < 0)
					warnx("error: cannot update $result variable");
			}
			return 1;
		}

		if (strstr(buff,"HERE{") != NULL)
			loadhere(herebuff);

		if (runpipeline(buff, herebuff) < 0) {
			warnx("error: cannot run command line");
			if (setenv("result","1",1) < 0)
				warnx("error: cannot update $result variable");
		}
		return 1;
	}
	return -1;
}

int
main(int argc, char const *argv[]) {

	char *line;
	char buff[Max_Buff];
	char herebuff[Max_Buff];

	--argc;
	if (argc > 0)
		errx(EXIT_FAILURE,"run ./shell without arguments");

	if (setenv("result","0",1) < 0)
		return -1;

	for (;;) {
		fprintf(stdout, ">> ");
		fflush(stdout);

		// get line
		if ((line = fgets(buff,Max_Buff,stdin)) == NULL) {
			if (feof(stdin))
				errx(EXIT_SUCCESS,"execution finished successfully");

			errx(EXIT_FAILURE,"fatal error: Input error occurred");
		}
		buff[strlen(buff)-1] = '\0';

		// command selection
		resetbuff(herebuff);
		commselect(buff,herebuff);
	}
	exit(EXIT_FAILURE);
}
