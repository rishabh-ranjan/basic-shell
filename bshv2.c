#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

//#define DEBUG
/*
 * ISSUES!
 * a.out instead of ./a.out has to be supported.
 */

/* Error prefix */
#define PREF "bsh"

/*
 * Handle error in system call.
 * Print error and exit.
 */
#define ERR(x) { if (x < 0) { perror(PREF); \
	exit(EXIT_FAILURE); } }
#define ERRM(s, x) { if (x < 0) { perror(strcat(PREF": ", s)); \
	exit(EXIT_FAILURE); } }

/*
 * Handle self-detected error.
 * Print error and exit.
 */
#define ERRS(s) { fprintf(stderr, PREF": %s\n", s); \
	exit(EXIT_FAILURE); }

/*
 * Builtin function implementations.
 */

void builtin_cd(char **argv) {
	// assert(strcmp(argv[0], "cd") == 0);
	char *dir = argv[1] ? argv[1] : getenv("HOME"); // cd to ~ by default
	if (chdir(dir)) perror(PREF);
}

void builtin_pwd(char **argv) {
	// assert(strcmp(argv[0], "pwd") == 0);
	printf("%s\n", getcwd(NULL, 0));
}

void builtin_mkdir(char **argv) {
	// assert(strcmp(argv[0], "mkdir") == 0);
	if (argv[1]) {
		if (mkdir(argv[1], 0777)) perror(PREF);
	} else {
		printf("usage: mkdir directory\n");
	}
}

void builtin_rmdir(char **argv) {
	// assert(strcmp(argv[0], "rmdir") == 0);
	if (argv[1]) {
		if (rmdir(argv[1])) perror(PREF);
	} else {
		printf("usage: rmdir directory");
	}
}

void builtin_exit(char **argv) {
	// assert(strcmp(argv[0], "exit") == 0);
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
#define PROMPT "shell> "
void print_prompt() {
	ERR(printf("%s", PROMPT));
}

/*
 * Read a single line command.
 */
char *read_cmd() {
	char *line = NULL;
	size_t linecap = 0;
	int len = getline(&line, &linecap, stdin);
	ERR(len);
	line[len-1] = '\0'; // remove '\n' from the end
	return line;
}

#if 0
/*
 * Extract upto next pipe.
 * Return pointer to after next pipe or off-the-end if none.
 * Current extraction can be accessed through the original
 * input pointer.
 */
#define PIPE_CHAR '|'
char *get_prog(char *cmd) {
	for ( ; *cmd; ++cmd) {
		if (*cmd == PIPE_CHAR) {
			*cmd = '\0';
			return cmd + 1;
		}
	}
	return cmd;
}
#endif

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
			if (!(*cmd)) ERRS(ERR_MSG);

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
	if (!tokens) ERR(-1);
	ind = 0;
}

void buf_add(char *token) {
	if (ind >= bufsize) {
		bufsize *= 2;
		tokens = realloc(tokens, bufsize * sizeof(char*));
		if (!tokens) ERR(-1);
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

int dup_in;
int dup_out;

void dup_io() {
	dup_in = dup(STDIN_FILENO);
	ERR(dup_in);
	dup_out = dup(STDIN_FILENO);
	ERR(dup_out);
}

void restore_io() {
	ERR(dup2(dup_in, STDIN_FILENO));
	ERR(dup2(dup_out, STDOUT_FILENO));
}

#if 0
void close_dup() {
	ERR(close(dup_in));
	ERR(close(dup_out));
}
#endif

#define PIPE_DELIM "|"
void exec_cmd(char *cmd) {
	char *prog;
	int pfd[2];
	pfd[0] = STDIN_FILENO;

	while ((prog = strsep(&cmd, PIPE_DELIM)) != NULL) {

#ifdef DEBUG
		fprintf(stderr, "prog -> %s\n", prog);
#endif

		int in = pfd[0];
		int out;
		if (cmd) {
			pipe(pfd);
#ifdef DEBUG
			fprintf(stderr, "opened fd's %d, %d\n", pfd[0], pfd[1]);
#endif
			out = pfd[1];
		} else {
			out = STDOUT_FILENO;
		}

		char *infile, *outfile;
		get_io(prog, &infile, &outfile);
		if (*infile) {
			in = open(infile, O_RDONLY);
			ERR(in);
#ifdef DEBUG
			fprintf(stderr, "opened fd %d\n", in);
#endif
		}
		if (*outfile) {
			out = open(outfile, O_WRONLY | O_CREAT | O_TRUNC);
			ERR(out);
#ifdef DEBUG
			fprintf(stderr, "opened fd %d\n", out);
#endif
		}

#ifdef DEBUG
		fprintf(stderr, "in -> %d\n", in);
		fprintf(stderr, "out -> %d\n", out);
#endif
		dup2(in, STDIN_FILENO);
		dup2(out, STDOUT_FILENO);

		char **argv = parse_cmd(prog);
#ifdef DEBUG
		char **argvc = argv;
		fprintf(stderr, "%s\n", "---");
		for ( ; *argvc; ++argvc) {
			fprintf(stderr, "%s\n", *argvc);
		}
		fprintf(stderr, "%s\n", "---");
#endif
		int bi = find_builtin(argv[0]);
		if (bi >= 0) {
#ifdef DEBUG
			fprintf(stderr, "%s\n", "builtin");
			fprintf(stderr, "%s\n", "---");
#endif
			(*builtin_fns[bi])(argv);
		} else {
#ifdef DEBUG
			fprintf(stderr, "%s\n", "forking");
			fprintf(stderr, "%s\n", "---");
#endif
			pid_t pid = fork();
			ERR(pid);
			if (pid == 0) ERRM(argv[0], execvp(argv[0], argv));
		}

		if (in != STDIN_FILENO) {
			ERR(close(in));
#ifdef DEBUG
			fprintf(stderr, "closed fd %d\n", in);
#endif
		}
		if (out != STDOUT_FILENO) {
			ERR(close(out));
#ifdef DEBUG
			fprintf(stderr, "closed fd %d\n", out);
#endif
		}
		restore_io();
	}
	int status;
	while (wait(&status) > 0) ;
}

int main() {
	dup_io();
	while (1) {
		print_prompt();
		char *cmd = read_cmd();
#ifdef DEBUG
		fprintf(stderr, "read_cmd() -> %s\n", cmd);
#endif
		exec_cmd(cmd);
#ifdef DEBUG
		fprintf(stderr, "%s\n", "---");
#endif
	}
}
