#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//get cpu time for each cpu, total cpu did not add up, thus removed it
int* cpu(int num) {
	int* ptr = (int*)malloc((num) * sizeof(int));							//allocate size of int * num cpu in array
	FILE* fp = fopen("/proc/stat", "r");
	char buffer[1024];
	int a, b, c, d, e;
	fgets(buffer,1024,fp);
	int x;
	for (x = 0; x < num; x++) {
		fgets(buffer, 1024, fp);
		//user, nice, system, irq, softirq is included in cpu usage
		sscanf(buffer, "%*s %d %d %d %*d %*d %d %d",&a,&b,&c,&d,&e);		//by using scanf store value of user, nice, system, irq, softirq  1 2 3 6 7. irq and softirq, these are related with interrupts piaza
		ptr[x] = a + b + c + d + e;
	}
	fclose(fp);
	return ptr;
}

//get number of cores
int core(){
	FILE *fp = fopen("/proc/cpuinfo", "r");									
	char buffer[1024];
	int num;
	int x;
	char check[1024];
	while(1){
		fgets(buffer,1024,fp);												//read each line and check whether it starts with cpu cores
		memcpy(check,&buffer[0],9);
		check[9]='\0';
		if(strcmp(check, "cpu cores") ==0){
			break;
		}
	}
	sscanf(buffer,"%*s %*s %*s %d", &num);									//extract number of cores
	fclose(fp);
	return num;
}

//get system uptime, convert it to D H:MM:SS format
void uptime(){
	FILE *fp = fopen("/proc/uptime", "r");
	char buffer[1024];
	int num;
	fgets(buffer,1024,fp);
	sscanf(buffer,"%d",&num);
	fclose(fp);
	int D = num/86400;
	int H = (num/3600)%10;
	int M = (num/60)%60;
	int S = (num%60);
	printf("Uptime: %d day(s), %d:%02d:%02d\n",D,H,M,S);
}

//get memory,  (total mem - free mem) / total mem
void memory(){
	FILE *fp = fopen("/proc/meminfo", "r");
	char buffer[1024];
	int total;
	int free;
	fgets(buffer,1024,fp);
	sscanf(buffer,"%*s %d",&total);
	fgets(buffer,1024,fp);
	sscanf(buffer,"%*s %d",&free);
	fclose(fp);
	printf("Memory: %d kB / %d kB (%.1f%%)\n", total-free, total, (float)(total-free)/(float)total*100); //make percentage
}

int main(int argc, char *argv[]){
	//get number of cores
	int num = core();													//get number of core
	int period = 3;														//default period = 3 but if user identified the period you have to change it
	if(argc == 2){
		period = atoi(argv[1]);
	}
	if (period <= 0){
		printf("Invalid number\n");
		return 0;
	}
	while(1){
		//get the cpu usage time before X seconds
		int *old = cpu(num);											//cpu time after x sec - cpu time before x sec to get how much cpu used on 3 sec
		//loop every 3 seconds
		sleep(period);
		printf("(%d seconds later)\n",period);
		uptime();
		memory();
		//get the cpu usage time after the X seconds
		int *new = cpu(num);
		//add up to get total
		int total=0;
		int x;
		for(x = 0; x < num; x++){
			total += new[x] - old[x];
		}
		//calculating cpu usage of each core, repeat by number of cores
		for(x = 0; x < num; x++){
			if(total > 0){
				printf("CPU%d: %5.1f%%  ",x,(float)(new[x] - old[x])/(float)total*100);
			}
			//when total is 0, print 0.0 since division of 0 does not work
			else if(total == 0){
				printf("CPU%d: %5.1f%%  ",x,0.0);
			}
		}
		free(old);
		free(new);
		printf("\n\n");
	}
	return 0;
}
