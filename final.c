/*
 * bshell - A Basic Shell.
 * author - Rishabh Ranjan.
 */

#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Error prefix */
#define PREF "bshell"

/*
 * Handle error in system call.
 * Print error and exit.
 */
void sys_err(int x) {
	if (x < 0) {
		perror(PREF);
		exit(EXIT_FAILURE);
	}
}

/*
 * Handle error in program execution.
 * Print error prefixed by name of program and exit.
 */
void prog_err(int x, char *name) {
	if (x < 0) {
		perror(strcat(PREF, name));
		exit(EXIT_FAILURE);
	}
}

/*
 * Handle self-detected error.
 * Print error and exit.
 */
void self_err(char *msg) {
	fprintf(stderr, PREF": %s\n", s);
	exit(EXIT_FAILURE);
}

/*
 * Builtin function implementations.
 */

void builtin_cd(char **argv) {
	char *dir = argv[1] ? argv[1] : getenv("HOME"); // cd to ~ by default
	if (chdir(dir)) perror(PREF);
}

void builtin_pwd(char **argv) {
	printf("%s\n", getcwd(NULL, 0));
}

void builtin_mkdir(char **argv) {
	if (argv[1]) {
		if (mkdir(argv[1], 0777)) perror(PREF);
	} else {
		printf("usage: mkdir directory\n");
	}
}

void builtin_rmdir(char **argv) {
	if (argv[1]) {
		if (rmdir(argv[1])) perror(PREF);
	} else {
		printf("usage: rmdir directory");
	}
}

void builtin_exit(char **argv) {
	exit(EXIT_SUCCESS);
}

char *builtin_strs[] = {
	"cd",
	"pwd",
	"mkdir",
	"rmdir",
	"exit",
	NULL
};

void (*builtin_fns[])(char **argv) = {
	builtin_cd,
	builtin_pwd,
	builtin_mkdir,
	builtin_rmdir,
	builtin_exit
};

/*
 * If cmd is a builtin, return index in the builtin_strs array.
 * Else return -1.
 */
int find_builtin(const char *cmd) {
	int i;
	char *bstr;
	int flag = 0;
	for (i = 0; (bstr = builtin_strs[i]); ++i) {
		if (strcmp(bstr, cmd) == 0) {
			flag = 1;
			break;
		}
	}
	if (flag) return i;
	else return -1;
}

/*
 * Print the promt.
 */
#define PROMPT "\033[32mshell>\033[0m "
void print_prompt() {
	sys_err(printf("%s", PROMPT));
}

/*
 * Read a single line command.
 */
char *read_cmd() {
	char *line = NULL;
	size_t linecap = 0;
	int len = getline(&line, &linecap, stdin);
	sys_err(len);
	line[len-1] = '\0'; // remove '\n' from the end
	return line;
}

/*
 * Check if delimiter.
 */
#define DELIM " \t<>"
int is_delim(char c) {
	char *d = DELIM;
	for ( ; *d; ++d) {
		if (c == *d) return 1;
	}
	return 0;
}
#undef DELIM

/*
 * Put name of input and output file in the location pointed to
 * in 'infile' and 'outfile' respectively.
 * Remove redirection part from string.
 * If input or output file is not specified, put empty string.
 * All redirections must occur at the end of the string.
 */
#define IN_CHAR '<'
#define OUT_CHAR '>'
// Buffer size for storing infile and outfile
// Should be sufficient on most file systems
#define BUFSIZE 256
#define ERR_MSG "bad redirection"
void get_io(char *cmd, char **infilep, char **outfilep) {
	char *instr = *infilep = malloc(BUFSIZE * sizeof(char));
	char *outstr = *outfilep = malloc(BUFSIZE * sizeof(char));

	for ( ; *cmd; ++cmd) {
		if (*cmd == IN_CHAR || *cmd == OUT_CHAR) {
			char c = *cmd;
			*cmd = '\0';

			for (++cmd; *cmd && is_delim(*cmd); ++cmd) ;
			if (!(*cmd)) self_err(ERR_MSG);

			for ( ; *cmd && !is_delim(*cmd); ++cmd) {
				if (c == IN_CHAR) *instr++ = *cmd;
				else *outstr++ = *cmd;
			}
			--cmd; // revert the extra character read
		}
	}

	*instr = *outstr = '\0'; // termination
}
#undef BUFSIZE

/*
 * Dynamically resized buffer for tokenization.
 */
#define BUFSIZE 64
size_t bufsize;
char **tokens;
int ind;

void buf_init() {
	bufsize = BUFSIZE;
	free(tokens);
	tokens = malloc(bufsize * sizeof(char*));
	if (!tokens) sys_err(-1);
	ind = 0;
}

void buf_add(char *token) {
	if (ind >= bufsize) {
		bufsize *= 2;
		tokens = realloc(tokens, bufsize * sizeof(char*));
		if (!tokens) sys_err(-1);
	}
	tokens[ind++] = token;
}

/*
 * Parse command into argv.
 */
#define DELIM " \t"
char **parse_cmd(char *cmd) {
	buf_init();
	char *token;
	while ((token = strsep(&cmd, DELIM)) != NULL) {
		if (*token) { // check for empty tokens
			buf_add(token);
		}
	}
	buf_add(NULL);
	return tokens;
}

/* File descriptors to duplicated stdin and stdout. */
int dup_in;
int dup_out;

void dup_io() {
	dup_in = dup(STDIN_FILENO);
	sys_err(dup_in);
	dup_out = dup(STDIN_FILENO);
	sys_err(dup_out);
}

void restore_io() {
	sys_err(dup2(dup_in, STDIN_FILENO));
	sys_err(dup2(dup_out, STDOUT_FILENO));
}

/*
 * Execute an entire command.
 * Includes piping, redirection, handling builtin commands, and forking.
 *
 * Notes / Salient features:
 * - presence or absence of spaces, tabs, etc. around |, <, > are supported.
 * - any number of pipes are supported.
 * - any combination of piping and redirection is supported.
 * - piped parts are executed concurrently as in bash.
 * - proper release of resources has been ensured.
 */
#define PIPE_DELIM "|"
void exec_cmd(char *cmd) {

	char *prog; // individual program to be executed at a time
	int pfd[2]; // pipe file descriptors
	pfd[0] = STDIN_FILENO;

	// loop through every piped part
	while ((prog = strsep(&cmd, PIPE_DELIM)) != NULL) {
		// piping
		int in = pfd[0];
		int out;
		if (cmd) {
			pipe(pfd);
			out = pfd[1];
		} else {
			out = STDOUT_FILENO;
		}

		// redirection
		char *infile, *outfile;
		get_io(prog, &infile, &outfile);
		if (*infile) { // if infile is non-empty
			in = open(infile, O_RDONLY);
			sys_err(in);
		}
		if (*outfile) { // if outfile is non-empty
			out = open(outfile, O_WRONLY | O_CREAT | O_TRUNC);
			sys_err(out);
		}

		dup2(in, STDIN_FILENO);
		dup2(out, STDOUT_FILENO);

		// execution
		char **argv = parse_cmd(prog);
		int bi = find_builtin(argv[0]);
		if (bi >= 0) {
			(*builtin_fns[bi])(argv);
		} else {
			pid_t pid = fork();
			sys_err(pid);
			if (pid == 0) prog_err(argv[0], execvp(argv[0], argv));
		}

		if (in != STDIN_FILENO) sys_err(close(in));
		if (out != STDOUT_FILENO) sys_err(close(out));
		restore_io(); // revert stdin and stdout
	}
	int status;
	while (wait(&status) > 0) ; // wait for all child processes to terminate
}

int main() {
	dup_io();
	// shell loop
	while (1) {
		print_prompt();
		exec_cmd(read_cmd());
	}
}
