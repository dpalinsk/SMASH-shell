/* @dpalinsk
 * smash.c
 * Stop Mucking Around SHell
 * GET SMASHED!
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#define BUFFSIZE 100 // max number of tokens on a line.

 char * readline1();
 void looper();
 char** splitline(char *line);
 void exit1();
 int check_for_redirect(char ** command);
 int redirect(char ** command);
 int check_for_pipe(char ** command);

 FILE * finp;
 FILE * foutp;
 char * line;
 char ** args;
 int arglen;
 pid_t child;
 int sigflag; //not safe to print in signal handler
 //jmp_buf env;
 int input_redirect_index;
 int output_redirect_index;
 int redirect_flag;
 int input_redirect_fd;
 int output_redirect_fd;
 int pipe_flag;
 int pipe_index;

char * readline1(){
 	size_t n =0;
 	char *line=NULL;
 	getline(&line, &n, stdin);
 	return line;
 }

char **splitline(char *line){
 	arglen=0;
 	int bufsize= BUFFSIZE+1, index = 0;
 	char **tokens = calloc(bufsize, (sizeof(char *)));
 	char * token;
 	const char delim[5] = " \t\r\n";
 	//const char delim2[5]="<>|&"
 	/* Possibly implement handling no spaces between chars in delim 2. Maybe strpbrk?*/
 	if (NULL == tokens){
 		perror("SMASH: ALLOCATION ERROR! GET SMASHED!");
 		exit(EXIT_FAILURE);
 	}

 	token = strtok(line, delim);

 	while (token != NULL) {
 		arglen+=1;
 		tokens[index]=token;
 		index++;

 		if (index >= bufsize){
 			bufsize+=BUFFSIZE;
 			tokens=realloc(tokens, bufsize*sizeof(char*));
 			if (NULL == tokens){
 				perror("SMASH: ALLOCATION ERROR! GET SMASHED!");
 				exit(EXIT_FAILURE);
 			}
 		}
 		token = strtok(NULL, delim);
 	}
 	return tokens;
 }

void myinfo(){
 	pid_t pid = getpid();
 	pid_t ppid = getppid();
 	printf("PID: %d\nPPID: %d\n",pid,ppid );
 }

void mycd(char * command){
 	char buff[PATH_MAX + 1];
 	char * cpath;
	char * upath = ""; //user path
	upath = command;
	cpath = getcwd(buff, PATH_MAX+1);
	int len = strlen(cpath) + strlen(upath) + 2 ;
	char* newpath = malloc(len * sizeof(char)); 
	newpath = strcpy(newpath, cpath);
	newpath = strcat(newpath, "/");
	newpath = strcat(newpath,upath);
	//printf("%s\n", newpath);
	if (chdir(newpath) != 0  ){
		perror("SMASH");
	}
	free(newpath);
}

int SMASHexec(char ** command){
	int status;
	//printf("Hi\n");
	if (0==strcmp("exit", command[0])){
		free(line);
		free(args);
		exit(0);
	} else if (0==strcmp("myinfo", command[0])){
		myinfo();
	} else if (0==strcmp("cd", command[0]) && NULL == args[1]) {
		char * HOME="";
		HOME = getenv("HOME");
		if (NULL != HOME){
			chdir(HOME);
		} else{
			printf("$HOME path corrupt\n");
		}
		
	}else if (0 == strcmp("cd", command[0])){
		mycd(command[1]);
	}else {
		
		if (pipe_flag){
			args[pipe_index]=NULL;
			//pid_t child2;
			int fds[2];
			int buffersize = arglen - pipe_index -1;
			//printf("%d\n", buffersize );
			char ** buffer = calloc(sizeof(char*), buffersize);
			for (int i = 1; i <= buffersize; ++i)
			{
				buffer[i-1] = command[pipe_index + i];
				//printf("ABC%s\n", buffer[i-1] );
			}
			//printf("%s\n", buffer[0] );
			pipe(fds);
			//child = fork();
			if (fork() == 0){
				dup2(fds[0], STDIN_FILENO);
				//close(fds[1]);
				close(fds[0]);
				//child2 = fork();
				if (fork() == 0){
					dup2(fds[1], STDOUT_FILENO);
					close(fds[0]);
					//close(fds[1]);
					//printf("1st %s\n", command[0] );
					execvp(command[0], command);
					
				}
				wait(NULL);
				close(fds[1]);
				execvp(buffer[0], buffer);
			}
			close(fds[1]);
			close(fds[0]);
			free(buffer);
			wait(NULL);		
		} else {
			//printf("B\n");
			child = fork();
			if (child){
			//parent
				if (0==strcmp(command[arglen-1], "&")){
					char buff[PATH_MAX + 1];
					char * cpath;
					cpath = getcwd(buff, PATH_MAX+1);
					printf("\n< %s SMASH >$ ", cpath);
					waitpid(-1, &status, WNOHANG );
				} else {
					waitpid(child, &status, 0 );
				}

			} else{
			//child
			//sleep(5); used for testing of sigint handler
				redirect(args);
				if (redirect_flag){
					if ( redirect_flag == 1 ){
						args[input_redirect_index]=NULL;
					} else if ( redirect_flag == 2 ){
						args[output_redirect_index]=NULL;
					} else if ( redirect_flag == 3){
						args[output_redirect_index]=NULL;
						args[input_redirect_index]=NULL;
					}

				}
				int failure = execvp(command[0], command);
				if (-1 == failure){
					perror("SMASH");
					exit(EXIT_FAILURE);
				}
			}
		}
	}
	return child;
}

void prompter(){
	char * cpath;
	char buff[PATH_MAX + 1];
	cpath = getcwd(buff, PATH_MAX+1);
	printf("\n< %s SMASH >$ ", cpath);
}

void sig_handler(int sig){
	signal(sig, sig_handler);

	switch (sig) {
		case (SIGINT) :
			if (0==child){
				sigflag=1;
				//prompter();
				return;
			} else{
				kill(child, SIGINT);
			}
	}
	return;
}

/*  checks for redirect.
	return  0 for no redirect
			1 for input redirect
			2 for output redirect
			3 for both
	if 1 or 2, set input_red_index or output_red_index respectively
	if 3, set both. fu
	//tracker keeps track of return;
*/
int check_for_redirect(char ** command){
	int tracker=0; 
	for (int i = 0; i < arglen; ++i){
		if ( 0 == strcmp(command[i], "<") ){
			input_redirect_index=i;
			tracker+=1;
		} else if ( 0 == strcmp(command[i], ">")){
			output_redirect_index=i;
			tracker+=2;
		}
	} 

	return(tracker);
}

int redirect(char ** command){
	if (0 == redirect_flag){
		return 0;
	} else if (1 == redirect_flag){
		//printf("%s\n", command[input_redirect_index+1]);
		if (NULL == (finp = fopen(command[input_redirect_index+1],"r+"))){
			perror("Invalid File. Errno ");
			exit(errno);
		}
		int finpfd=fileno(finp);
		
		dup2(finpfd, STDIN_FILENO);
		foutp = stdout;
	} else if ( 2==redirect_flag ){
		if (NULL == (foutp = fopen(command[output_redirect_index+1],"w+"))){
			perror("Invalid File. Errno ");
			exit(errno);
		}
		printf("HI");
		int foutpfd=fileno(foutp);
		dup2(foutpfd, STDOUT_FILENO);
		finp = stdin; 
	} else if (3 == redirect_flag ) {
		if (NULL == (foutp = fopen(command[output_redirect_index+1],"w+"))){
			perror("Invalid File. Errno ");
			exit(errno);
		}
		if (NULL == (finp = fopen(command[input_redirect_index+1],"r+"))){
			perror("Invalid File. Errno ");
			exit(errno);
		}
		int finpfd=fileno(finp);
		dup2(finpfd, STDIN_FILENO);
		int foutpfd=fileno(foutp);
		dup2(foutpfd, STDOUT_FILENO);
	}
	return 0;
}

int check_for_pipe(char ** command){
	int status=0;
	for ( int i = 0; i < arglen; ++i){
		if (0==strcmp("|", command[i])){
			status=1;
			pipe_index=i;
		}
	}
	return status;
}

void looper(){
	int status=1;
	char * cpath;
	char buff[PATH_MAX + 1];
	sigflag=0;
	do {
		//setjmp (env);
		input_redirect_index=0, output_redirect_index=0;
		cpath = getcwd(buff, PATH_MAX+1);
		printf("< %s ", cpath);
		line = readline("SMASH >$ ");
		if (NULL == line){
			exit(EOF);
		}
		add_history(line);
		if (strlen(line)>=1){
			args = splitline(line);
			redirect_flag=check_for_redirect(args);
			pipe_flag=check_for_pipe(args);
			SMASHexec(args);
			//signal(SIGINT, sig_handler);
			free(args);
		}
		//printf("hi!");
		//sleep(1);
		free(line);
	}while (status);

}



//silly little procrastination ascii function.
void intro(){
	//ASCII GENERATED BY http://www.network-science.de/ascii/
	char * space = " ";
	printf("%27sWelcome to\n", space);
	printf("%13s  _____ __  __           _____ _    _\n", space);
	printf("%13s / ____|  \\/  |   /\\    / ____| |  | |\n", space);
	printf("%13s| (___ | \\  / |  /  \\  | (___ | |__| |\n", space);
		printf("%13s \\___ \\| |\\/| | / /\\ \\  \\___ \\|  __  |\n", space);
		printf("%13s ____) | |  | |/ ____ \\ ____) | |  | |\n", space);
		printf("%13s|_____/|_|  |_/_/    \\_\\_____/|_|  |_|\n\n", space);
		printf("%10sThe Stop Mucking Around SHell - GET SMASHED!\n", space);
		printf("%13sBy: Derek Palinski\n\n", space); 
	}

int main (){
 	signal(SIGINT, sig_handler);
	intro();
 	//myinfo();
	looper();
	
}
