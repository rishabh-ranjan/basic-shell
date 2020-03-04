all: run

run: run_shell
	./run_shell

run_shell: run_shell.c shell
	gcc -o $@ $<

shell: 2018CS10416_sh.c
	gcc -o $@ $<

clean:
	rm shell run_shell

