#include <string.h>
#include <errno.h>
#include <stdio.h>   //fprintf, printf
#include "sys/ipc.h" //uid, gid
#include "sys/shm.h" //shared memory facility(shmat)
#include <unistd.h>  //getppid, setpgid
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>//S_IRUSR|S_IWUSR
#include <sys/types.h>//shm
#include <sys/time.h>
#include <libgen.h>
#include <sys/wait.h>

#define ACCESSRIGHTS S_IRUSR|S_IWUSR
#define NUMBEROFCHILDREN 8 
/*  1->(2,3,4,5) 5->(6,7,8) 
    1->2 SIGUSR1 2->(3,4) SIGUSR2 4->5 SIGUSR1 3->6 SIGUSR1 6->7 SIGUSR1 7->8 SIGUSR2 8->1 SIGUSR2*/

struct sigaction old, sig;
struct timeval MainTime, CurrTime, StartTime;
pid_t ppID, gID, ggid;
int signal1 = 0, signal2 = 0;
size_t memsize;
char *progname;
key_t SharedMemoryIdChildren;
pid_t *childID;

void Son(int index);
void CreateProcessTree(int pID);

void print_error(const char *module_name, const char *error_msg, const char *function_name) {
    fprintf(stderr, "%s: %s: %s\n", module_name, function_name ? function_name : "", error_msg);
}

int ChildNumber(int pID) {
	for (int i = NUMBEROFCHILDREN;; i--) if (childID[i] == pID) return i;
	return -1;
}

void Son(int index) {
	if (fork() == 0) {
		childID = shmat(SharedMemoryIdChildren, 0, 0);
		childID[index] = getpid();	
		CreateProcessTree(childID[index]);
	}
}

void CreateProcessTree(int pID) {
	gID = getpgid(pID);
	ppID = getppid();
	int Waiting = 0;
	int childNumber = ChildNumber(pID);

	switch(childNumber) {
		case 1:
			Son(2);
			Son(3);
			Son(4);
			Son(5);
			break;
		case 2:
			break;
		case 3:
			setpgid(getpid(), childID[3]);
			break;
		case 4:
		while (!Waiting) {
				int ShouldBeOne = 1;
				for (int i = 1; i <= 5; i++) {
					ShouldBeOne = ShouldBeOne && childID[i];
				}
				Waiting = ShouldBeOne;
			}
			setpgid(childID[4], childID[3]);
			break;
		case 5:
			Son(6);
			Son(7);
			Son(8);
			break;
		case 6:
			break;
		case 7:
			break;
		case 8:
			break;
		default:
			break;
	}
	while(1) pause();
}

int ttime(){
	gettimeofday(&CurrTime, NULL);
	timersub(&CurrTime, &StartTime, &MainTime);
	return MainTime.tv_usec/1000;
} 

void sendFromFirst() {
	signal1++;
	if (signal1 > 100) {
		kill(childID[8], SIGTERM);
	}
	else{
		printf("%d %d %d отправил SIGUSR1. %3d мксек\n", 1, getpid(), getppid(), ttime());
		kill(childID[2], SIGUSR1);
	}
}

void sendFromSecond() {
	signal2++; 
	printf("%d %d %d отправил SIGUSR2. %3d мксек\n", 2, getpid(), getppid(), ttime());    		
	kill(-getpgid(childID[3]), SIGUSR2);
}

void sendFromThird() {
	signal1++;
	printf("%d %d %d отправил SIGUSR1. %3d мксек\n", 3, getpid(), getppid(), ttime());
	kill(childID[6], SIGUSR1);
}

void sendFromFourth() {
	signal1++;
	printf("%d %d %d отправил SIGUSR1. %3d мксек\n", 4, getpid(), getppid(), ttime());
	kill(childID[5], SIGUSR1);
}

void sendFromFifth() {
	signal1++;
}

void sendFromSixth() {
	signal1++;
	printf("%d %d %d отправил SIGUSR1. %3d мксек\n", 6, getpid(), getppid(), ttime());
	kill(childID[7], SIGUSR1);
}

void sendFromSeventh() {
	signal2++; 
	printf("%d %d %d отправил SIGUSR2. %3d мксек\n", 7, getpid(), getppid(), ttime());
	kill(childID[8], SIGUSR2);
}

void sendFromEighth() {
	signal2++;
	printf("%d %d %d отправил SIGUSR2. %3d мксек\n", 8, getpid(), getppid(), ttime());
	kill(childID[1], SIGUSR2);
}

static void MyOwnHandler(int signal) {
	int childNum = ChildNumber(getpid());
	int timefromstart = ttime();			
  	if (signal == SIGUSR1) {
        switch (childNum) {
        	case 2: 
			printf("%d %d %d получил SIGUSR1. %3d мксек\n", 2, getpid(), getppid(), timefromstart);
			sendFromSecond();
			break;
        	case 6: 
			printf("%d %d %d получил SIGUSR1. %3d мксек\n", 6, getpid(), getppid(), timefromstart);
			sendFromSixth();
			break;
        	case 5:
			printf("%d %d %d получил SIGUSR1. %3d мксек\n", 5, getpid(), getppid(), timefromstart);
			sendFromFifth();
			break;
        	case 7:
			printf("%d %d %d получил SIGUSR1. %3d мксек\n", 7, getpid(), getppid(), timefromstart);
			sendFromSeventh();
			break;
		default:   
			print_error(progname, strerror(errno), "");
			break;
		}
		return;
	}

    if (signal == SIGUSR2) { 
		switch (childNum) {         	
        	case 1: 
			printf("%d %d %d получил SIGUSR2. %3d мксек\n", 1, getpid(), getppid(), timefromstart);
			sendFromFirst();
			break;				
        	case 3: 
			printf("%d %d %d получил SIGUSR2. %3d мксек\n", 3, getpid(), getppid(), timefromstart);
			sendFromThird();
			break;
        	case 4:
				printf("%d %d %d получил SIGUSR2. %3d мксек\n", 4, getpid(), getppid(), timefromstart);
			sendFromFourth();
			break;
		case 8: 
			printf("%d %d %d получил SIGUSR2. %3d мксек\n", 8, getpid(), getppid(), timefromstart);
			sendFromEighth();
			break;	
		default:   
			print_error(progname, strerror(errno), "");
			break;
		}
		return;	     
	}
	if (signal == SIGTERM) {
        switch (childNum) {
		case 0: 
			wait(NULL);
            		exit(EXIT_SUCCESS); 
			break;
		case 1:	waitpid(childID[1], 0, 0);
                	printf("%d  %d  %d  завершился SIGUSR1: %3d,  SIGUSR2: %3d\n", 1, getpid(), ppID, signal1, signal2);
                	kill(childID[0],SIGTERM);
                	exit(EXIT_SUCCESS);  
			break;
		default:
			printf("%d  %d  %d  завершился SIGUSR1: %3d,  SIGUSR2: %3d\n", childNum, getpid(), ppID, signal1, signal2);
            		kill(childID[childNum-1], SIGTERM);
            		exit(EXIT_SUCCESS);   
			break;
	   	}	
		return;
	}	   		
	return;
}


int main(int argc, char *argv[]) {
	progname = basename(argv[0]);
	memsize = (NUMBEROFCHILDREN+1) * sizeof(pid_t);
    //
    	if((SharedMemoryIdChildren = shmget(IPC_PRIVATE, memsize , ACCESSRIGHTS)) == -1 )   { 
        	print_error(progname, strerror(errno), "");
        	exit(EXIT_FAILURE);
    	}
    	childID = shmat(SharedMemoryIdChildren, 0, 0); //shmat возвращает адрес разделяемой памяти 
    	memset(childID, 0, memsize);
    	childID[0] =  getpid();

    //
  	sig.sa_handler = MyOwnHandler;
  	sigemptyset (&sig.sa_mask);
  	sig.sa_flags = 0;
  	sigaction (SIGUSR1, &sig, &old);
  	sigaction (SIGUSR2, &sig, &old);
  	sigaction (SIGTERM, &sig, &old);
	if (fork() == 0) {
		childID[1] = getpid();
		CreateProcessTree(childID[1]);
	}
    //

	int Waiting = 0;
	while (!Waiting) {
		int ShouldBeOne = 1;
		for (int i = 1; i <= NUMBEROFCHILDREN; i++) {
			ShouldBeOne = ShouldBeOne && childID[i];
		}
		Waiting = ShouldBeOne;
	}
	system("ps --forest");
	gettimeofday(&StartTime, NULL);
	kill(childID[1], SIGUSR2);
	while(1) pause();
}
