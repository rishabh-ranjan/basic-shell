#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

/* Error prefix */
#define PREF "bsh"

/*
 * Handle error in system call.
 * Print error and exit.
 */
#define ERR(x) { if (x < 0) { perror(PREF); \
	exit(EXIT_FAILURE); } }

/*
 * Handle self-detected error.
 * Print error and exit.
 */
#define ERRS(s) { fprintf(stderr, PREF": %s\n", s); \
	exit(EXIT_FAILURE); } }

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
	ERR(getline(&line, &linecap, stdin));
	return line;
}

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

/*
 * Check if delimiter.
 */
#define DELIM " \t<>"
int is_delim(char c) {
	char *d = DELIM;
	for ( ; *d; ++d) {
		if (c == d) return 1;
	}
	return 0;
}

/*
 * Put name of input and output file in the location pointed to
 * in 'infile' and 'outfile' respectively.
 * Remove redirection part from string.
 * If input or output file is not specified, put NULL.
 * All redirections must occur at the end of the string.
 */
#define IN_CHAR '<'
#define OUT_CHAR '>'
// Buffer size for storing infile and outfile
// Should be sufficient on most file systems
#define BUFSIZE 256
#define ERR_MSG "bad redirection"
void get_io(char *cmd, char **infile, char **outfile) {
	char *inp = *infile = malloc(BUFSIZE * sizeof(char));
	char *outp = *outfile = malloc(BUFSIZE * sizeof(char));

	for ( ; *cmd; ++cmd) {
		if (*cmd == IN_CHAR || *cmd == OUT_CHAR) {
			char ch = *cmd;
			*cmd = '\0';

			for ( ; *cmd && is_delim(*cmd); ++cmd) ;
			if (!*cmd) ERRS(ERR_MSG);

			for ( ; *cmd && !is_delim(*cmd); ++cmd) {
				if (ch == IN_CHAR) *inp++ = *cmd;
				else *outp++ = *cmd;
			}
			--cmd; // revert the extra character read
		}
	}

	*inp = *outp = '\0'; // termination
}

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
 * Tokenize command.
 */
#define DELIM " \t"
char **parse_cmd(char *cmd) {
	buf_init();
	while ((token = strsep(&cmd, DELIM)) != NULL) {
		buf_add(token);
	}
	buf_add(NULL);
	return tokens;
}
