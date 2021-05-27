#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_CMDLINE_LEN 1024
int child_status = 0;


int get_cmd_line(char* cmdline);
void show_prompt();
void free_all(char** argv);
int parse(char* cmdline, char** argv);
void special(char** argv);

void handler(int signal) {						
	switch(signal){
		case SIGCHLD:							//waits for process      WNOHANG return immediately if no child has exited.
			waitpid(-1, NULL, WNOHANG);
			break;
		case SIGINT:							//blocks the CTRL+C
			printf("\nUse exit\n");
			break;
	}
}

int main(){
	if (signal(SIGCHLD, handler) == SIG_ERR || signal(SIGINT, handler) == SIG_ERR){
		perror("signal");
	}

	char cmdline[MAX_CMDLINE_LEN];
	while(1){
		pid_t child_pid;
		show_prompt();
		if (get_cmd_line(cmdline) == -1)
			continue;

		char* argv[128] = {};
		int background = parse(cmdline, argv);
		special(argv);

		int status = 0;
		
		int x = 0;
		if (argv[0] == NULL){
			continue;
		} else if (strcmp(argv[0], "exit") == 0){		// if user typed exit force quite 
			free_all(argv);
			exit(0);
		} else if (strcmp(argv[0], "cd") == 0){			// if user typed cd change directory
			if (background == 0){
				if (argv[1] == NULL){
					chdir(getenv("HOME"));
				} else{
					if (chdir(argv[1]) == -1){
						printf("%s: %s\n", argv[1], strerror(errno));
					}
				}
			} else{										// background = 1 , 
				if(fork() == 0){
					if (argv[1] == NULL){
						chdir(getenv("HOME"));
					} else {
						if(chdir(argv[1]) == -1){
							printf("%s: %s\n", argv[1], strerror(errno));
						}
					}
					free_all(argv);
					return errno;
				}
			}
		} else if (strcmp(argv[0], "jobs") == 0){		//if jobs 
			int fd[2];
			char buffer[4096] = {};
			pipe(fd);
			child_pid = fork();
			if(child_pid == 0){							//use pipe to get jobs
				close(1);								//first use pid to generate ps command and pip it to parents
				dup2(fd[1], 1);
				close(fd[0]);
				char ppid[10];
				sprintf(ppid,"%d", getppid());
				execlp("ps", "ps", "--ppid", ppid, "-o", "pid", "-o", "command",NULL);
			} else{										//parents process the execlp result, to generate similar as the assignment description
				close(fd[1]);
				wait(NULL);
				read(fd[0], buffer, sizeof(buffer));
				int str = 16;
				int end = strlen(buffer) - 41;			//I have tested quite alot to get the correct offset to start and end
				buffer[end] = '\0';
				int num = 1;
				for(int x = str; x < end; x ++){
					if (x == str){
						printf("[%d]",num);
					}
					if (buffer[x] =='\n'){
						printf(" &%c", buffer[x]);		//at the end print & and change line)
						num++;
						if(x+1 < end){
							printf("[%d]",num);
						}
						continue;
					}
					printf("%c", buffer[x]);
				}
			}
		} else{
			child_pid = fork();							//if none special cases to execvp
			if(child_pid == 0){
				
				int result = execvp(argv[0], argv);
				if(result == -1){
					printf("command not found\n");
				}
				free_all(argv);
				exit(127);
			} else{
				if (background == 0){					//if background = 0  wait, if not don't wait
					waitpid(child_pid, &status, NULL);
				}
			}
		}
		free_all(argv);
		child_status = status;
	}
	return 0;
}

//replace old arg to new one (used in special)
char* replace(char* s, char* old, char* new){
	char* result;
	int i, cnt = 0;
	int newlen = strlen(new);
	int oldlen = strlen(old);
	
	if(!strstr(s,"\'") && strstr(s,old)){
		for (i = 0; s[i] != '\0'; i++){
			if (strstr(&s[i], old) == &s[i]) {
				cnt++;
	
				i += oldlen -1;
			}
		}
	} else{
		return s;
	}
	result = (char*)malloc(i + cnt * (newlen - oldlen) + 1);
	char *remove = s;	
	i = 0;
	while (*s){
		if(strstr(s, old) == s) {
			strcpy(&result[i], new);
			i += newlen;
			s += oldlen;
		} else {
			result[i++] = *s++;
		}
	}
	result[i] = '\0';
	free(remove);
	remove = NULL;
	return result;
}

//remove specific char (used in special)
void remove_char(char* s, char w){
	int y = 0;
	for (int x = 0; x <= strlen(s); x++){
		if(s[x] == '\0'){
			s[y] = s[x];
			break;
		}
		if(s[x] == w){
			continue;
		}
		s[y] = s[x];
		y++;
	}
}

void special(char** argv){											// if special case (echo) 
	char ID[10];
 	sprintf(ID, "%d", getpid());
	char STAT[10];
	sprintf	(STAT, "%d", WEXITSTATUS(child_status));				//last process exit status
	int x = 0;
	while(argv[x] != NULL){
		argv[x] = replace(argv[x], "$$", ID);
		argv[x] = replace(argv[x], "$?", STAT);
		remove_char(argv[x], '\'');
		remove_char(argv[x], '\"');
		x++;
	}
}

//finds the quote part of the command.
int parse(char* cmdline, char** argv){					
	char token[MAX_CMDLINE_LEN];
	int quote = 1;
	int argc = 0;
	for (int i = 0, j = 0; i < MAX_CMDLINE_LEN; i++, j++){
		if (cmdline[i] == '\'' || cmdline[i] == '\"') {		
			quote = quote * -1;
		}
		if ((cmdline[i] == ' ' || cmdline[i] == '\t') && quote == 1) {		// if not inside the quote and there is a space, tokenize
			token[j] = '\0';												// to cut the string (when doing strlen(token) it only counts till '\0'
			char* r = malloc((strlen(token) + 1) * sizeof(char));
			strcpy(r, token);												
			argv[argc++] = r;
			while(cmdline[i] == ' ' || cmdline[i] == '\t'){					//remove space between 
			       i++;
			}
			i--;															//minus to point correct char
			j = -1;															//count length of the token
			continue;
		}
		if (cmdline[i] == '\0') {											// reach the end of the command
			token[j] = '\0';
			char* r = malloc((strlen(token) + 1) * sizeof(char));
			strcpy(r, token);
			argv[argc++] = r;
			break;
		}
		token[j] = cmdline[i];												// editing the original command line
	}
	int background = 0;														//if at the end has  & backgound = 1
	if (strcmp(argv[argc-1], "&") == 0) {
		background = 1;
		free(argv[--argc]);
		argv[argc] = NULL;
	} else {
		argv[argc] = NULL;
	}
	return background;
}

void free_all(char** argv){
	int x = 0;
	while(argv[x] != NULL){
		free(argv[x]);
		argv[x] = NULL;
		x++;
	}
}

//show prompt 
void show_prompt(){
	char cwd[200];
	getcwd(cwd, sizeof(cwd));
	if (strcmp(cwd,"/") == 0) {
		printf("[/] myshell> ");
	} else if (strcmp(cwd, getenv("HOME")) == 0) {
		printf("[~] myshell> ");
	} else {
		char* cur;											//get the last token for the cur directory
		char* dir = strtok(cwd, "/");
		while (dir != NULL) {
			cur = dir;
			dir = strtok(NULL, "/");
		}
		printf("[%s] myshell> ", cur);
	}
}

//get command line and trimout spaces in one string
int get_cmd_line(char* cmdline) {
	int i;
	int end;
	int str = 0;
	char cmd[MAX_CMDLINE_LEN];
	if (!fgets(cmd, MAX_CMDLINE_LEN, stdin))
		return -1;
	end = strlen(cmd);
	cmd[--end] = '\0';
	i = 0;
	while (i < end && cmd[i] == ' ') {
		++i;
	}
	if(i == end) {
		return -1;
	}
	//trim command
	while(cmd[str] == ' ' || cmd[str] == '\t'){
		str++;
	}
	while(cmd[end] == ' ' || cmd[end] == '\t' || cmd[end] == '\0'){
		end--;
	}
	for(int x = str, y = 0; x <= end; x++, y++){
		cmdline[y] = cmd[x];
	}
	cmdline[end - str + 1] = '\0';
	return 0;
}

