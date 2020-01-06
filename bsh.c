#include <stdio.h>

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
 * add: multi line command with '\'
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
#define DELIM " \t\r\n\a"
int is_delim(char c) {
	static char *delim = DELIM;
	for (int i = 0; delim[i]; ++i) {
		if (c == delim[i]) return 1;
	}
	return 0;
}

/*
 * Parse the read command.
 * Return argc.
 * Put tokens in argv.
 * todo: handle errors (esp malloc and realloc).
 * todo: handle contiguous delimiters.
 * todo: handle starting delimiters.
 * add: support quoting and escaping.
 */
// use PARSE_BUFSIZE to both initialize and expand buffer
#define PARSE_BUFSIZE 64
char **parse_cmd(char *cmd) {
	size_t bufsize = PARSE_BUFSIZE;
	char **tokens = malloc(bufsize * sizeof(char*));

	// avoid strsep/strtok to handle quotes and escapes in future
	size_t ind = 0;
	char *token = cmd;
	for ( ; *cmd; ++cmd) {
		if (!is_delim(*cmd)) continue;
		*cmd = '\0';

		if (ind >= bufsize) {
			bufsize += PARSE_BUFSIZE;
			tokens = realloc(tokens, bufsize * sizeof(char*));
		}
		tokens[ind++] = token;
		token = cmd+1;
	}

	// add terminating NULL pointer
	if (ind >= bufsize) {
		bufsize += 1;
		tokens = realloc(tokens, bufsize * sizeof(char*));
	}
	tokens[ind] = NULL;
	return tokens;
}
