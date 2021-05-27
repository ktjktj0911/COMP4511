#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <dirent.h>

int uid;
struct required {
	int PID;
	char USER[1024];
	int TIME;
	int VIRT;
	char CMD[1024];
};

//filter when there is -u flage
int filter_u(const struct dirent *dir){
	char path[1024];
	int id;
	//find directory only contains number
	if(!fnmatch("[1-9]*", dir->d_name, 0)){				//return 0 when string match the pattern
		//find the uid of the process
		sprintf(path,"/proc/%s/status", dir->d_name);
		char buffer[1024];
		FILE *fp = fopen(path, "r");
		char check[1024];
		while(1){
			fgets(buffer,1024,fp);
			memcpy(check,&buffer[0],3);
			check[3] = '\0';
			if(strcmp(check, "Uid") == 0){
				break;
			}
		}
		fclose(fp);
		sscanf(buffer,"%*s %d",&id);
		if(id == uid){
			return 1;
		}
	}
	return 0;
}

//filter when there is no -u flag
int filter_a(const struct dirent* dir) {
	return !fnmatch("[1-9]*", dir->d_name, 0);						//if -u flag is not set, you do have to find all the process
}

struct required process(char *dir){
	struct required a;
	int utime;				//user mode time
	int ktime;				//kernel mode time
	char name[1024];			//name of the process
	
	char path[1024];		// path of a process
	sprintf(path,"/proc/%s/stat", dir);

	// file open path and save values
	char buffer[1024];
	FILE *fp = fopen(path, "r");
	fgets(buffer,1024,fp);
	sscanf(buffer,"%d %s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %d %d %*s %*s %*s %*s %*s %*s %*s %d", &a.PID, name, &utime, &ktime, &a.VIRT);
	fclose(fp);

	//remove brackets
	char *cmd = name + 1;
	name[strlen(cmd)] = 0; 
	strcpy(a.CMD, cmd);

	//to find the uid
	int id;
	sprintf(path, "/proc/%s/status", dir);
	fp = fopen(path, "r");
	char check[1024];
	while(1){
		fgets(buffer, 1024, fp);
		memcpy(check, &buffer[0], 3);
		check[3] = '\0';
		if(strcmp(check, "Uid") == 0){
			break;
		}
	}
	fclose(fp);
	sscanf(buffer, "%*s %d", &id);

	//get uname and save values (getpwuid search for ID to find username)
	char* user = getpwuid(id)->pw_name;
	strcpy(a.USER, user);

	//CPU time spent in user and kernel mode (change it to seconds)
	a.TIME = (utime + ktime) / 100;
	
	return a;
}

//sort by mem
int compare_mem(const void *a, const void *b){
	struct required *oa = a;
	struct required *ob = b;

	return (oa->VIRT < ob->VIRT) - (oa->VIRT > ob->VIRT);
}

//sort by time
int compare_time(const void *a, const void*b){
	struct required *oa = a;
	struct required *ob = b;

	return (oa->TIME < ob->TIME) - (oa->TIME > ob->TIME);
}

int main (int argc, char *argv[]){
	int u = 0, m = 0, p = 0;
	//finding flag
	int x;
	for(x = 0; x < argc; x++){
		if(strcmp(argv[x], "-u") == 0){
			u = x;
		}
		else if(strcmp(argv[x], "-m") == 0){
			m = x;
		}
		else if(strcmp(argv[x], "-p") == 0){
			p = x;
		}	
	}

	struct dirent **namelist;
	int n;
	if (u) {
		// if havent written anything after u flag ex) ./myps -u
		if (u+1 == argc){
			printf("Invalid user: \n");
			return 0;
		}
		//finding uid using uname (if not found error)   getpwnam (search for name to get uid)
		else if (getpwnam(argv[u + 1]) == NULL) {
			printf("Invalid user: %s\n", argv[u+1]);
			return 0;
		}
		uid = getpwnam(argv[u + 1])->pw_uid;
		n = scandir("/proc", &namelist, filter_u, 0);		//scan using matching uid
	}
	else {
		n = scandir("/proc", &namelist, filter_a, 0);		//scan all the directory consist of numbers
	}
	struct required* all = (struct required*)malloc(n * sizeof(struct required));		//returns number of matching directory (n)

	printf("%7s %-20s %8s %10s %s\n","PID", "USER", "TIME", "VIRT", "CMD");
	int i = 0;
	while(i < n) {
		all[i] = process(namelist[i]->d_name);				//processing namelist[i]
		free(namelist[i]);
		i++;
	}

	if(m) {
			qsort(all, n, sizeof(struct required), compare_mem);	// sort all using mem
	}
	else if(p) {
		qsort(all, n, sizeof(struct required), compare_time);		//sort all using time
	}
	for (x = 0; x < n; x++) {
		char time[16];
		sprintf(time, "%02d:%02d:%02d", ((all[x].TIME) / 3600) % 100, ((all[x].TIME) / 60) % 60, (all[x].TIME) % 60);
		printf("%7d %-20s %8s %10d %s\n", all[x].PID, all[x].USER, time, all[x].VIRT/1024, all[x].CMD);
	}
	free(all);
	free(namelist);
	return 0;
}
