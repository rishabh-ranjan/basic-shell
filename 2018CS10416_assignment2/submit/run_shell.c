#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Name of file (in current directory) containing shell. */ 
#define SHELL "shell"
int main() {
	printf("run_shell: running shell\n");
	pid_t pid = fork();
	if (pid < 0) {
		// error
		perror(SHELL);
		exit(EXIT_FAILURE);
	}
	if (pid == 0) {
		// child process
		execlp("./"SHELL, "./"SHELL, (char *)NULL);
		// returns only if error
		perror(SHELL);
		exit(EXIT_FAILURE);
	} else {
		int status;
		// wait for child to terminate
		while (wait(&status) > 0) ;
	}
	printf("run_shell: shell exited\n");
}
