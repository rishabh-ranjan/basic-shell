#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

/* 
 * Print the prompt.
 * add: use environment variables.
 */
void print_prompt() {
	char *prompt = "shell> ";
	printf("%s", prompt);
}

/*
 * Read a command.
 * todo: handle errors.
 * add: support escapes.
 * add: multi line command with '\', unbalanced brackets, unbalanced quotes.
 */
char *read_cmd() {
	char *line = NULL;
	size_t linecap = 0;
	getline(&line, &linecap, stdin);
	return line;
}

/*
 * Check if character is a delimiter.
 */
// should not be \', \" (?)
#define DELIM " \t\r\n\a"
int is_delim(char c) {
	static char *delim = DELIM;
	for (int i = 0; delim[i]; ++i) {
		if (c == delim[i]) return 1;
	}
	return 0;
}

/*
 * Dynamically resized buffer for tokenization.
 */
#define PARSE_BUFSIZE 64
size_t bufsize;
char **tokens;
int ind;

void buf_init() {
	bufsize = PARSE_BUFSIZE;
	free(tokens);
	tokens = malloc(bufsize * sizeof(char*));
	ind = 0;
}

void buf_add(char *token) {
	if (ind >= bufsize) {
		bufsize += PARSE_BUFSIZE;
		tokens = realloc(tokens, bufsize * sizeof(char*));
	}
	tokens[ind++] = token;
}

/*
 * Parse the read command.
 * Return argc.
 * Put tokens in argv.
 * todo: handle errors (esp malloc and realloc).
 * todo: handle contiguous delimiters.
 * todo: handle starting delimiters.
 * add: support double quoting and escaping.
 * add: support arbitrary nesting of (escaped?) single and double quotes
 */
// use PARSE_BUFSIZE to both initialize and expand buffer
#define PARSE_BUFSIZE 64
#define SINGLE_QUOTE '\''
char **parse_cmd(char *cmd) {
	buf_init();
	// avoid strsep/strtok to handle quotes and escapes in future
	char *token = cmd;
	int sq = 0; // unbalanced single quote
	int ctg = 1; // continguous delimiters
	for ( ; *cmd; ++cmd) {
		if (sq) {
			if (*cmd == SINGLE_QUOTE) {
				sq = 0;
				*cmd = '\0';
				buf_add(token);
				token = cmd + 1;
				ctg = 1;
			}
		} else {
			if (*cmd == SINGLE_QUOTE) {
				sq = 1;
				*cmd = '\0';
				token = cmd + 1;
			}
			if (is_delim(*cmd)) {
				*cmd = '\0';
				if (!ctg) buf_add(token);
				ctg = 1;
				token = cmd + 1;
			} else ctg = 0;
		}
	}
	// add terminating NULL pointer
	buf_add(NULL);
	return tokens;
}

/* Prepend error msgs with this. */
#define ERR_PREFIX "bsh"

/*
 * Launch a child process.
 * add: support for running in background using &.
 */
void launch_proc(char **argv) {
	pid_t pid = fork();	
	if (pid < 0) {
		// error forking
		perror(ERR_PREFIX);
	} else if (pid == 0) {
		// child process
		if (execvp(argv[0], argv) == -1) perror(ERR_PREFIX);
		exit(EXIT_FAILURE);
	} else {
		// parent process
		int status;
		do {
			waitpid(pid, &status, WUNTRACED);
		} while (!(WIFEXITED(status) || !WIFSIGNALED(status)));
	}
}

/*
 * Builtin function implementations.
 */

void builtin_cd(char **argv) {
	assert(strcmp(argv[0], "cd") == 0);
	char *dir = argv[1] ? argv[1] : getenv("HOME"); // cd to ~ by default
	if (chdir(dir)) perror(ERR_PREFIX);
}

void builtin_pwd(char **argv) {
	assert(strcmp(argv[0], "pwd") == 0);
	printf("%s\n", getcwd(NULL, 0));
}

void builtin_mkdir(char **argv) {
	assert(strcmp(argv[0], "mkdir") == 0);
	if (argv[1]) {
		if (mkdir(argv[1], 0777)) perror(ERR_PREFIX);
	} else {
		printf("usage: mkdir directory\n");
	}
}

void builtin_rmdir(char **argv) {
	assert(strcmp(argv[0], "rmdir") == 0);
	if (argv[1]) {
		if (rmdir(argv[1])) perror(ERR_PREFIX);
	} else {
		printf("usage: rmdir directory");
	}
}

void builtin_exit(char **argv) {
	assert(strcmp(argv[0], "exit") == 0);
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
 * Execute a command by checking if it is builtin or not.
 */
void exec_cmd(char **argv) {
	int i;
	char *bstr;
	int flag = 0;
	for (i = 0; (bstr = builtin_strs[i]) != NULL; ++i) {
		if (strcmp(bstr, argv[0]) == 0) {
			flag = 1;
			break;
		}
	}
	if (flag) {
		(*builtin_fns[i])(argv);
	} else {
		launch_proc(argv);
	}
}

int main() {
	while (1) {
		print_prompt();
		char *cmd = read_cmd();
		char **argv = parse_cmd(cmd);
		// known ISSUE: executing an empty string gets stuck
		exec_cmd(argv);
	}
}

