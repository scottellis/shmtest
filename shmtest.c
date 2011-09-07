/*
 * shmtest program - see README
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/user.h>	// PAGE_SIZE
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <time.h>
 
int child_pid;
int stop_time;
int shmid;
char *shared_mem;
sem_t *reader_sem;
sem_t *writer_sem;
const char reader_semname[] = "/shmchild-reader";
const char writer_semname[] = "/shmchild-writer";
 
// sem_open, sem_init, sem_post, sem_wait, sem_close, sem_unlink
int parent_process();
int child_process();
 
 
// no zombies
static void sigchld(int sig)
{
	while (waitpid(-1, NULL, WNOHANG) > 0)
		continue;		
		
	child_pid = 0;
}
 
static void sighup(int sig)
{
	stop_time = 1;
}
 
int setup_sig_handlers()
{
	struct sigaction sia;

	bzero(&sia, sizeof sia);
	sia.sa_handler = sighup;

	if (sigaction(SIGINT, &sia, NULL) < 0) {
		perror("sigaction(SIGINT)");
		return -1;
	}
	
	bzero(&sia, sizeof sia);
	sia.sa_handler = sigchld;
	
	if (sigaction(SIGCHLD, &sia, NULL) < 0) {
		perror("sigaction(SIGCHLD)");
		return -1;
	}
	
	return 1;
}

void create_shared_mem()
{
	shmid = shmget(IPC_PRIVATE, PAGE_SIZE, IPC_CREAT | IPC_EXCL | 0600);
	
	if (shmid == -1) {
		if (errno == EEXIST)
			printf("Shared memory segment already exists!!!\n");
		else
			perror("shmget");
		
		exit(EXIT_FAILURE);
	}
	
	shared_mem = (char *) shmat(shmid, NULL, 0);
	
	if (!shared_mem) {
		perror("shmat");
		exit(EXIT_FAILURE);
	}
}

// Start with the writer free(1) and the reader blocked(0)
void create_semaphores()
{
	writer_sem = sem_open(writer_semname, O_CREAT | O_EXCL, 0600, 1);
	
	if (writer_sem == SEM_FAILED) {
		perror("sem_open(writer)");
		exit(EXIT_FAILURE);
	}	
	
	reader_sem = sem_open(reader_semname, O_CREAT | O_EXCL, 0600, 1);
	
	if (reader_sem == SEM_FAILED) {
		perror("sem_open(reader)");
		sem_close(writer_sem);
		sem_unlink(writer_semname);
		exit(EXIT_FAILURE);
	}		
}

int main(int argc, char **argv)
{
	int status;
	
	// before the fork so the child sees the shared mem
	create_shared_mem();

	// ditto on the semaphores
	create_semaphores();
	
	memset(shared_mem, 0, PAGE_SIZE);
	
	child_pid = fork();
	
	if (child_pid == -1) {
		perror("fork");
		exit(1);
	}
	
	if (child_pid == 0)
		status = child_process();
	else 
		status = parent_process();
		
	return status;
}

int parent_process()
{
	int status;
	time_t t;
	struct tm *tm;

	// should never fail	
	if (setup_sig_handlers() < 0) {
		// but if it does, still don't want orphans or zombies
		kill(child_pid, SIGINT);
		
		if (wait(&status) == -1)
			perror("wait");
			
		return EXIT_FAILURE;
	}
	
	printf("\nuse ctl-c to exit\n\n");
	
	while (!stop_time) {
		// pretending that the writer has better things to do
		// the writer never waits
		if (sem_trywait(writer_sem)) {
			t = time(NULL);
			tm = localtime(&t);			
			strftime(shared_mem, 16, "%T", tm);
			
			// free the reader
			sem_post(reader_sem);
		}
		
		usleep(500000);
	}
	
	printf("\nWaiting for child to exit ");
	kill(child_pid, SIGINT);
	usleep(50000);
	
	while (child_pid) {
		printf(".");
		fflush(stdout);			
		sleep(1);
	}
	
	printf("\n");			
	
	sem_close(writer_sem);
	sem_unlink(writer_semname);
	
	sem_close(reader_sem);
	sem_unlink(reader_semname);
	
	return EXIT_SUCCESS;
}

int child_process()
{
	while (1) {
		// the reader on the other hand has nothing better 
		// to do so it always waits
		if (sem_wait(reader_sem) < 0)
			break;
			
		if (strlen(shared_mem) > 0)
			printf("%s\n", shared_mem);
	
		// free the writer
		sem_post(writer_sem);
	}	
		
	return EXIT_SUCCESS;	
}
