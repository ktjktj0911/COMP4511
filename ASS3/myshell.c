#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_CMDLINE_LEN 256
#define MAX_PIPE	127
#define MAX_ARG		256

struct job_list {
	int cnt;
	char stat[10];
	pid_t pid;
	pid_t pgid;
	char command[MAX_CMDLINE_LEN];
	struct job_list* next;
};

int foreground;
struct job_list* jobs;

void process_cmd(char* cmdline);
void argv_cmd(char* command);
void show_prompt();
int get_cmd_line(char* cmdline);
int tokenize(char** argv, char* line, char* delimiter);


void append(struct job_list** head_ref, char* line) {
	char* argv[MAX_ARG];
	int num = tokenize(argv, line, " ");
	char command[MAX_ARG] = "\0";
	for (int x = 3; x < num; x++) {
		strcat(command, argv[x]);
		strcat(command, " ");
	}

	struct job_list* new_node = (struct job_list*)malloc(sizeof(struct job_list));
	struct job_list* last = *head_ref;

	new_node->next = NULL;
	new_node->pgid = atoi(argv[1]);
	new_node->pid = atoi(argv[2]);
	strcpy(new_node->stat, argv[0]);
	strcpy(new_node->command, command);

	if (*head_ref == NULL) {
		*head_ref = new_node;
		new_node->cnt = 1;
		return;
	}

	while (last->next != NULL) {
		last = last->next;
	}

	last->next = new_node;
	new_node->cnt = last->cnt + 1;
	return;
}

void clear(struct job_list* head) {
	if (head == NULL) {
		return;
	}
	struct job_list* ptr = head;
	struct job_list* ptrn = ptr->next;
	while (ptr != NULL) {
		free(ptr);
		ptr = ptrn;
		if (ptrn != NULL) {
			ptrn = ptrn->next;
		}
	}
}

int IOredirect_index(char** argv, int argc) {
	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "<") == 0 ||
			strcmp(argv[i], ">") == 0 ||
			strcmp(argv[i], ">>") == 0 ||
			strcmp(argv[i], "&>") == 0) {
			return i;
		}
	}
	return -1;
}


//signal handler 
void handler(int signal) {
	switch (signal) {
	case SIGCHLD:
		waitpid(-1, NULL, WNOHANG);
		tcsetpgrp(STDIN_FILENO, foreground);
		break;
	case SIGINT:
		fprintf(stderr, "\nCTRL+C\n");
		break;
	}
}

int main() {
	sigset_t block, prev;
	sigemptyset(&block);
	sigaddset(&block, SIGTTOU);
	sigprocmask(SIG_BLOCK, &block, &prev);			///block SIGTTOU

	if (signal(SIGCHLD, handler) == SIG_ERR || signal(SIGINT, handler) == SIG_ERR) {		//block CTRL+C and not wait for 
		perror("signal");
	}
	foreground = getpgrp();
	char cmdline[MAX_CMDLINE_LEN];
	while (1) {
		tcsetpgrp(STDIN_FILENO, foreground);			//set foreground
		show_prompt();
		if (get_cmd_line(cmdline) == -1)
			continue; /* empty line handling */
		int fd[2];
		char buffer[4096] = {};
		pipe(fd);
		int child_pid = fork();
		if (child_pid == 0) {
			close(1);
			dup2(fd[1], 1);
			close(fd[0]);
			char ppid[10];
			sprintf(ppid, "%d", getppid());
			execlp("ps", "ps", "--ppid", ppid, "-o", "stat", "-o", "pgid", "-o", "pid", "-o", "command", NULL);
		}
		else {
			close(fd[1]);
			waitpid(child_pid, 0, 0);
			read(fd[0], buffer, sizeof(buffer));
			char* argv[MAX_ARG];
			int argc = tokenize(argv, buffer, "\n\t");
			int end = 0;
			while (argv[end] != NULL) {
				end++;
			}
			for (int x = 1; x < end - 1; x++) {
				append(&jobs, argv[x]);
			}
		}

		if (strcmp(cmdline, "jobs") == 0) {
			struct job_list* ptr = jobs;
			printf("\tpgid\tpid\tstat\tcommand\n");
			while (ptr != NULL) {
				printf("[%d]\t%d\t%d\t%s\t%s\n", ptr->cnt, ptr->pgid, ptr->pid,ptr->stat, ptr->command);
				ptr = ptr->next;
			}
		}
		else {
			process_cmd(cmdline);
		}
		clear(jobs);
		jobs = NULL;
	}
	return 0;
}

void process_cmd(char* cmdline) {
	int background = 0;
	if (cmdline[strlen(cmdline) - 1] == '&') {
		background = 1;
		cmdline[strlen(cmdline) - 1] = '\0';
	}
	int  pgid, status, fd_in = 0;
	char* pipe_command[MAX_PIPE];
	char ch = '|';
	int num_pipe = tokenize(pipe_command, cmdline, &ch);
	int fd[(num_pipe - 1) * 2];
	int pid[num_pipe];

	int x = 0;
	if (num_pipe > 1) {
		for (; x < num_pipe; x++) {
			pipe(fd + x * 2);
			if ((pid[x] = fork()) == 0) {
				if (x == 0)
					pgid = 0;
				if (setpgid(0, pgid)) {
					perror("child setpgid");
				}
				else if (x == 0) {
					tcsetpgrp(STDIN_FILENO, getpgrp());			//set foreground group using pgrp
				}
				dup2(fd_in, 0);
				if ((x + 1) < num_pipe) {
					dup2(fd[(x * 2) + 1], 1);
				}
				close(fd[x * 2]);
				argv_cmd(pipe_command[x]);
			}
			else {
				if (x == 0) {
					pgid = pid[x];
				}
				if (setpgid(pid[x], pgid))
					perror("parent setpgid");
				if ((x + 1) < num_pipe) {
					close(fd[(x * 2) + 1]);
					fd_in = fd[x * 2];
				}
			}
		}
	}
	else if (num_pipe == 1) {
		if ((pid[0] = fork()) == 0) {
			pgid = 0;
			if (setpgid(0, pgid))
				perror("child setpgid");
			tcsetpgrp(STDIN_FILENO, getpgrp());
			argv_cmd(pipe_command[x]);
		}
		else {
			pgid = pid[0];
			if (setpgid(pid[0], pgid))
				perror("parent setpgid");
		}
	}

	if (background == 0) {
		for (int x = 0; x < num_pipe; x++) {
			waitpid(pid[x], 0, WUNTRACED);
		}
	}
	for (int x = 0; x < (num_pipe - 1) * 2; x++) {
		close(fd[x]);
	}
	return;
}

void argv_cmd(char* command) {											//pipe
	int infd = -1, outfd = -1, appfd = -1, errfd = -1;
	char* argv[MAX_ARG];
	char* space = " \t";
	int argc = tokenize(argv, command, space);
	while (1) {
		int index = IOredirect_index(argv, argc);
		if (index == -1) {
			break;
		}
		if (strcmp(argv[index], "<") == 0) {
			infd = open(argv[index + 1], O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP);
			if (infd == -1) {
				printf("file not found\n");
				break;
			}
		}
		else if (strcmp(argv[index], ">") == 0) {
			outfd = open(argv[index + 1], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
		}
		else if (strcmp(argv[index], ">>") == 0) {
			appfd = open(argv[index + 1], O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
		}
		else if (strcmp(argv[index], "&>") == 0) {
			errfd = open(argv[index + 1], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
		}

		if (infd != -1 || outfd != -1 || appfd != -1 || errfd != -1) {
			for (int i = index; i < argc; i++) {
				if (i + 2 >= argc)
					argv[i] = NULL;
				else
					argv[i] = argv[i + 2];
			}
			argc -= 2;
		}
	}
	if (infd != -1 || outfd != -1 || appfd != -1 || errfd != -1) {
		if (infd != -1) {
			close(0);
			dup2(infd, 0);
		}

		if (outfd != -1) {
			close(1);
			dup2(outfd, 1);
		}
		else if (appfd != -1) {
			close(1);
			dup2(appfd, 1);
		}
		else if (errfd != -1) {
			close(1);
			close(2);
			dup2(errfd, 1);
			dup2(errfd, 2);
		}
	}
	if (strcmp(argv[0], "bg") == 0) {
		struct job_list* ptr = jobs;
		struct job_list* target = NULL;
		if (argv[1] == NULL) {
			while (ptr != NULL) {
				if (strcmp(ptr->stat, "T") == 0) {
					target = ptr;
				}
				ptr = ptr->next;
			}
		} else {
			char* command = argv[1] + 1;
			int num = atoi(command);
			target = jobs;
			while (target != NULL) {
				if (strcmp(target->stat, "T") == 0 && target->cnt == num) {
					break;
				}
				target = target->next;
			}
		}
		if (target == NULL) {
			fprintf(stderr, "mysh: bg: job not found\n");
			exit(127);
		}
		else {
			kill(-(target->pgid), SIGCONT);
			tcsetpgrp(STDIN_FILENO, target->pgid);
			exit(127);
		}
		return;
	}

	/*if (strcmp(argv[0], "fg") == 0) {
		struct job_list* ptr = jobs;
		struct job_list* target = NULL;
		if (argv[1] == NULL) {
			while (ptr != NULL) {
				if (strcmp(ptr->stat, "T") == 0) {
					target = ptr;
				}
				if (target == NULL && ptr->next == NULL) {
					target = ptr;
					waitid(P_PGID, target->pgid, 0, WSTOPPED);
					exit(127);
				}
				ptr = ptr->next;
			}
		}
		else {
			char* command = argv[1] + 1;
			int num = atoi(command);
			target = jobs;
			while (target != NULL) {
				if (strcmp(target->stat, "T") == 0 && target->cnt == num) {
					break;
				}
				target = target->next;
			}
		}
		if (target == NULL) {
			fprintf(stderr, "mysh: fg: job not found\n");
			exit(127);
		}
		else {
			kill((target->pgid), SIGCONT);
			waitid(P_PGID, target->pgid, 0, WSTOPPED);
			tcsetpgrp(STDIN_FILENO, target->pgid);
			exit(127);
		}
		return;
	}*/						//failed to implement fg

	int result = execvp(argv[0], argv);
	if (result == -1) {
		printf("command not found\n");
		exit(127);
	}
}


//showing prompt 
void show_prompt() {
	char cwd[200];
	getcwd(cwd, sizeof(cwd));
	if (strcmp(cwd, "/") == 0) {
		printf("[/] myshell> ");
	}
	else {
		char* cur;
		char* dir = strtok(cwd, "/");
		while (dir != NULL) {
			cur = dir;
			dir = strtok(NULL, "/");
		}
		printf("[%s] myshell> ", cur);
	}
}

//get cmd without and space at front and end
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
	if (i == end) {
		return -1;
	}
	//trim command
	while (cmd[str] == ' ' || cmd[str] == '\t') {
		str++;
	}
	while (cmd[end] == ' ' || cmd[end] == '\t' || cmd[end] == '\0') {
		end--;
	}
	for (int x = str, y = 0; x <= end; x++, y++) {
		cmdline[y] = cmd[x];
	}
	cmdline[end - str + 1] = '\0';

	if (strcmp(cmdline, "exit") == 0) {
		exit(1);
	}
	return 0;
}

//tokenize
int tokenize(char** argv, char* line, char* delimiter) {
	int argc = 0;
	argv[argc++] = strtok(line, delimiter);
	for (; argc < MAX_ARG; argc++) {
		argv[argc] = strtok(NULL, delimiter);
		if (argv[argc] == NULL) {
			break;
		}
		if (strlen(argv[argc]) == 0) {
			argc--;
		}
	}
	return argc;
}
