#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <fcntl.h>

#define TRUE 1
#define FALSE 0

#if HAVE_SETENV
	#undef setenv
#endif
#if !HAVE_DECL_SETENV
	extern int setenv (const char *, const char *, int);
#endif

#if HAVE_UNSETENV
	#undef unsetenv
#endif

#if !HAVE_DECL_UNSETENV
	#if VOID_UNSETENV
		extern void unsetenv (const char *);
	#else
		extern int unsetenv (const char *);
	#endif
#endif

struct user_details
{
	char *user_name;
	char *working_dir;
};

int getTimes;
struct tms cpu_time;
clock_t start,end,elapsed;

int sentToBackground;
pid_t bg_pid;

void get_user_details(struct user_details *info);
char *strdup(const char *str);
char * get_command(void);
void execute_command(char *cmd);
int setenv_cmd(char *token);
int unsetenv_cmd(char *token);
int fork_cmd(char *token);

int main(int argc, char **argv)
{

	struct user_details *info = NULL;
	char *cmd = NULL;

	info = (struct user_details*)malloc(sizeof(struct user_details));

	get_user_details(info);

	while(TRUE)
	{
		getTimes = FALSE;
		sentToBackground = FALSE;
		fprintf(stdout,"%s@csd_sh%s/# ",info->user_name,info->working_dir);
		cmd = strdup(get_command());
		execute_command(cmd);
		free(cmd);
	}


	exit(EXIT_SUCCESS);
}

void setToBGchild()
{
	if(sentToBackground == 1)
	{
		bg_pid = setsid();
		/*signal (SIGHUP,SIG_IGN);*/
		sleep(40);
	}
	return;	
}

int setToBGparent()
{
	if(sentToBackground == 1)
	{
		/*signal(SIGCHLD, SIG_IGN);*/
		fprintf(stdout,"The process %d is running in the background\n",bg_pid);
		return 1;
	}
	else
	{
		return 0;
	}
}

void start_T()
{
	if(getTimes == 1)
	{
		if((start = times(&cpu_time)) < 0)
		{
			fprintf(stderr,"times() failed!\n");
		}
	}
	return;
}

void end_T()
{
	if(getTimes == 1)
	{
		if((end = times(&cpu_time)) < 0)
		{
			fprintf(stderr,"times() failed!\n");
			return;
		}
		elapsed = end - start;
		fprintf(stdout,"user: %lf\tsys: %lf\treal: %lf\n",(double)cpu_time.tms_utime/(double)sysconf(_SC_CLK_TCK),(double)cpu_time.tms_stime/(double)sysconf(_SC_CLK_TCK),(double)elapsed/(double)sysconf(_SC_CLK_TCK));
	}
	return;
}

char *strdup(const char *str)
{
	int n = strlen(str) + 1;
	char *dup = malloc(n);

	if(dup)
	{
		strcpy(dup,str);
	}

	return dup;
}

int pipeline_cmd(char **argv_array,int num_pipes,int argc)
{
	char **current_cmd = NULL;
	int current_argc = 0, start_dir = 0;
	int i = 0, z = 0;
	pid_t pid;
	int **pipefds;
	start_T();
	/* allocate memory for the descriptors */
	pipefds = malloc((num_pipes)*sizeof(int *));
	pipefds[0] = malloc(2*sizeof(int));
	pipefds[1] = malloc(2*sizeof(int));

	/* create the pipes */
	for(i = 0; i < num_pipes; i++)
	{
		if((pipe(pipefds[i])) < 0)
		{
			fprintf(stderr,"pipe() failed!\n");
			return 0;
		}
	}

	/* find how many tokens are behind the first '|' */
	for(i = 0; i < argc; i++)
	{
		if( (strcmp("|",argv_array[i]) == 0) )
		{
			break;
		}
		current_argc++;
	}

	/* create an array of argvs to store the current command */
	current_cmd = malloc((current_argc) * sizeof(char *));
	for(i = 0; i < current_argc; i++)
	{
		current_cmd[i] = strdup(argv_array[i]);
	}
	current_cmd[current_argc] = NULL;
	start_dir = current_argc+1;

	/* execute the first command */
	if( (pid=fork()) == 0)
	{
		dup2(pipefds[0][1], 1);
		close(pipefds[0][0]);
		execvp(current_cmd[0], current_cmd);
	}
	else if(pid < 0)
	{
		fprintf(stderr,"fork() failed!\n");
    		return 0;
	}
	close(pipefds[0][1]);
	waitpid(pid, NULL, 0);

	/* execute all the commands left but not the last one */
	for(i = 1; i < num_pipes; i++)
	{
		/* dealocate the previous array of argvs to get rdy for the next one */
		for(z = 0; z < current_argc; z++)
		{
			free(current_cmd[z]);
			current_cmd[z] = NULL;
		}
		free(current_cmd);
		current_argc = 0;
		current_cmd = NULL;		
	
		for(z = start_dir; z < argc; z++)
		{
			if( (strcmp("|",argv_array[z]) == 0) )
			{
				break;
			}
			current_argc++;
		}
		/* create an array of argvs to store the current command */
		current_cmd = malloc(current_argc * sizeof(char *));
		for(z = 0; z < current_argc; z++)
		{
			current_cmd[z] = strdup(argv_array[start_dir+z]);
		}
		current_cmd[current_argc] = NULL;
		start_dir += current_argc+1;

		if( (pid=fork()) == 0)
		{
			dup2(pipefds[i-1][0], 0);
			close(pipefds[i-1][1]);
			dup2(pipefds[i][1], 1);
			close(pipefds[i][0]);
			execvp(current_cmd[0], current_cmd);
		}
		else if(pid < 0)
		{
			fprintf(stderr,"fork() failed!\n");
    			return 0;
		}
		close(pipefds[i-1][0]);
		close(pipefds[i-1][1]);
		close(pipefds[i][1]);
		waitpid(pid, NULL, 0);
	}
	/* dealocate the previous array of argvs to get rdy for the next one */
	for(i = 0; i < current_argc; i++)
	{
		free(current_cmd[i]);
		current_cmd[i] = NULL;
	}
	free(current_cmd);
	current_argc = 0;
	current_cmd = NULL;

	for(i = start_dir; i < argc; i++)
	{
		current_argc++;
	}
	
	/* create an array of argvs to store the current command */
	current_cmd = malloc(current_argc * sizeof(char *));
	for(i = 0; i < current_argc; i++)
	{
		current_cmd[i] = strdup(argv_array[start_dir+i]);
	}
	current_cmd[current_argc] = NULL;
	
	/* execute the last command */
	if((pid=fork())==0)
	{
		dup2(pipefds[num_pipes-1][0], 0);
		close(pipefds[num_pipes-1][1]);
		execvp(current_cmd[0], current_cmd);
	}
	close(pipefds[num_pipes-1][0]);
	close(pipefds[num_pipes-1][1]);
	waitpid(pid, NULL, 0);

	for(i = 0; i < num_pipes; i++)
	{
		close(pipefds[i][0]);
		close(pipefds[i][1]);
	}
	/* dealocate the array of argvs */
	for(i = 0; i < current_argc; i++)
	{
		free(current_cmd[i]);
		current_cmd[i] = NULL;
	}
	free(current_cmd);
	current_argc = 0;
	current_cmd = NULL;
	end_T();
	return 1;
}

int fork_cmd(char *token)
{
	pid_t c_pid, pid;
  	int status,pipes = 0, file_locationIn = 0, file_locationOut = 0, file_locationApp = 0;
	int pipeline_status = 0, redirect_out = 0, redirect_in = 0, redirect_append = 0;

	/* Create an argv array to put it in execvp */
	char **argv_array = NULL;
	int argc = 0;
	
	while(token != NULL)
	{
		if((argv_array = realloc(argv_array,(argc + 1) * sizeof(char *))) == NULL)
		{
			fprintf(stdout,"realloc() failed!\n");
			return 0;
		}
	
		if((argv_array[argc] = strdup(token)) == NULL)
		{
			fprintf(stdout,"strdup() failed!\n");
			return 0;
		}

		/* Check if will be needed pipes */
		if(strcmp("|",token) == 0)
		{
			pipeline_status = 1;
			pipes++;
		}

		/* Check if will be needed redirection */
		if(strcmp(">",token) == 0)
		{
			redirect_out = 1;
			argv_array[argc] = NULL;
			file_locationOut = argc+1;
		}
		
		/* Check if will be needed redirection */
		if(strcmp("<",token) == 0)
		{
			redirect_in = 1;
			argv_array[argc] = NULL;
			file_locationIn = argc+1;
		}

		/* Check if will be needed redirection */
		if(strcmp(">>",token) == 0)
		{
			redirect_append = 1;
			argv_array[argc] = NULL;
			file_locationApp = argc+1;
		}

		argc++;
		token = strtok(NULL,"\n, ,\t");
	}
	/********************************************/

	if(strcmp("&",argv_array[argc-1]) == 0)
	{
		sentToBackground = TRUE;
		argv_array[argc-1] = NULL;
		argc--;
	}
	
	if(pipeline_status == 1)
	{
		status = pipeline_cmd(argv_array,pipes,argc);

		if(status == 1)
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		start_T();
		c_pid = fork();

		if (c_pid == 0)
		{/* CHILD */
			int fd;
			setToBGchild();
			
			if(redirect_out == 1)
			{
				/* grant all permissions to everyone */
				if( (fd = open(argv_array[file_locationOut],O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0)
				{
					fprintf(stderr,"open() failed!\n");
    					exit(EXIT_FAILURE);
				}
				
				dup2(fd,1);
				dup2(fd,2);

				close(fd);
			}
			
			if(redirect_in == 1)
			{
				/* grant read and write permissions for the user, but only read permissions for group and system. */
				if( (fd = open(argv_array[file_locationIn],O_RDONLY,0777)) < 0)
				{
					fprintf(stderr,"open() failed!\n");
    					exit(EXIT_FAILURE);
				}
				
				dup2(fd,0);
				dup2(fd,2);

				close(fd);
			}

			if(redirect_append == 1)
			{
				/* grant read and write permissions for the user, but only read permissions for group and system. */
				if( (fd = open(argv_array[file_locationApp],O_WRONLY|O_CREAT|O_APPEND,0644)) < 0)
				{
					fprintf(stderr,"open() failed!\n");
    					exit(EXIT_FAILURE);
				}
				
				dup2(fd,1);
				dup2(fd,2);

				close(fd);
			}
                                                                                                                                                               
    			execvp( argv_array[0], argv_array);
    	  		/*only get here if execvp failed*/                                                                                                                                             
    			fprintf(stdout,"execvp() failed!\n");
			exit(EXIT_FAILURE);
  		}
		else if (c_pid > 0)
		{/* PARENT */
			if(setToBGparent() == 0)
			{
    				if( (pid = wait(&status)) < 0)
				{
      					fprintf(stderr,"wait() failed!\n");
    					return 0;
    				}
			}
			end_T();
			return 1;
  		}
		else
		{
			fprintf(stderr,"fork() failed!\n");
    			return 0;
  		}	
	}
}

int setenv_cmd(char *token)
{
	char *var = NULL;
	char *value = NULL;
	char *anwser = NULL;
	
	/* copy the VAR string */
	if((token = strtok(NULL,"\n, ,\t")) != NULL)
	{
		var = strdup(token);
	}
	else
	{
		return 0;
	}
        /* copy the VALUE string */
	if((token = strtok(NULL,"\n, ,\t")) != NULL)
	{
		value = strdup(token);
		/* check if the given enviroment variable already exists */
		start_T();
		if(getenv(var) != NULL)
		{
			fprintf(stdout,"The enviroment variable: %s already exist!\n",var);
			fprintf(stdout,"Do you want to overwrite its value?\n[Y/N] : ");
			anwser = strdup(get_command());
			if((strcmp("Y\n",anwser) == 0) || (strcmp("N\n",anwser) == 0))
			{
				/* set overwrite = 1 */
				if((strcmp("Y",token) == 0))
				{
					if((setenv(var,value,1)) != 0)
					{
						free(anwser);
						free(var);
						free(value);
						return 0;
					}
					end_T();
					free(anwser);
					free(var);
					free(value);
					return 1;
				}
				/* set overwrite = 0 */
				else
				{
					if((setenv(var,value,0)) != 0)
					{
						free(anwser);
						free(var);
						free(value);
						return 0;
					}
					end_T();
					free(anwser);
					free(var);
					free(value);
					return 1;
				}
			}
			else
			{
				fprintf(stdout,"By default the value of %s will not be overwriten\n",var);
				if((setenv(var,value,0)) != 0)
				{
					free(anwser);
					free(var);
					free(value);
					return 0;
				}
				end_T();
				free(anwser);
				free(var);
				free(value);
				return 1;
			}
			
		}
		/* the given enviroment variable doest excist 
		   so we dont care about overwrite's value */
		if((setenv(var,value,1)) != 0)
		{
			free(var);
			free(value);
			return 0;
		}
		end_T();
		free(var);
		free(value);
		return 1;
	}
	else
	{
		free(var);
		return 0;
	}
}

int unsetenv_cmd(char *token)
{
	char *var = NULL;
	/* copy the VAR string */
	if((token = strtok(NULL,"\n, ,\t")) != NULL)
	{
		var = strdup(token);
		start_T();
		if((unsetenv(var) != 0))
		{
			free(var);
			return 0;
		}
		end_T();
		free(var);
		return 1;
	}
	else
	{
		return 0;
	}
}

int cd_cmd(char *token)
{
	char *path = NULL;
	/* copy the VAR string */
	if((token = strtok(NULL,"\n, ,\t")) != NULL)
	{
		path = strdup(token);
		start_T();
		if((chdir(path) != 0))
		{
			free(path);
			return 0;
		}
		end_T();
		free(path);
		return 1;
	}
	else
	{
		return 0;
	}
}

void execute_command(char *cmd)
{
	char *token = NULL;
	int   state = 0;

	token = strtok(cmd,"\n, ,\t");
	if(token != NULL)
	{
			/********** Measure Time ***********/
		if(strcmp("csdTime",token) == 0)
		{
			getTimes = TRUE;
			token = strtok(NULL,"\n, ,\t");
		}
			  /********* exit command *********/
		if(strcmp("exit",token) == 0)
		{
			exit(EXIT_SUCCESS);
		}
			 /********* setenv command *********/
		else if(strcmp("setenv",token) == 0)
		{
			state = setenv_cmd(token);
			if(state == 1)
			{
				return;
			}
			else
			{
				fprintf(stderr,"Command not found! - setenv() failed!\n");
				return;
			}
		}
			/********* unsetenv command *********/
		else if(strcmp("unsetenv",token) == 0)
		{
			state = unsetenv_cmd(token);
			if(state == 1)
			{
				return;
			}
			else
			{
				fprintf(stderr,"Command not found! - unsetenv() failed!\n");
				return;
			}
		}
			/********* cd command *********/
		else if(strcmp("cd",token) == 0)
		{
			state = cd_cmd(token);
			if(state == 1)
			{
				return;
			}
			else
			{
				fprintf(stderr,"Command not found! - chdir() failed!\n");
				return;
			}
		}
		else
		{	/********* commands with fork being used *********/
			state = fork_cmd(token);
			if(state == 1)
			{
				return;
			}
			else
			{
				fprintf(stderr,"Command not found!\n");
				return;
			}
		}
	}
	else
	{
		fprintf(stderr,"Command not found!\n");
		return;
	}
}

void get_user_details(struct user_details *info)
{
	/* man getpwuid */
	struct passwd *passwd; 
	char cwd[1024];

	/* Get the uid of the running processand use it to get a record from /etc/passwd */
	if((passwd = getpwuid ( getuid())) == NULL) 
	{
		fprintf(stderr, "\nERROR: getpwuid() failed!\n\nAborting...\n");
		exit(EXIT_FAILURE);
	}

	/* Get the current directory and store it to the struct */
	if (getcwd(cwd, sizeof(cwd)) != NULL)
	{
		info->working_dir = (char *)strdup(cwd);
	}
        else
	{
        	fprintf(stderr, "\nERROR: getcwd() failed!\n\nAborting...\n");
		exit(EXIT_FAILURE);
	}

	/* Store the user name to the struct */
	info->user_name = (char *)strdup(passwd->pw_name);

	return;
}

char * get_command(void)
{
	char * line = malloc(100), * linep = line, * linen = NULL;
    	size_t lenmax = 100, len = lenmax;
    	int c;

    	if(line == NULL)
	{
        	return NULL;
	}

    	for(;;)
	{
		c = fgetc(stdin);

        	if(c == EOF)
		{
        		break;
		}

        	if(--len == 0)
		{
        	    	len = lenmax;
        	    	linen = realloc(linep, lenmax *= 2);

        		if(linen == NULL)
		    	{
        	        	free(linep);
        	        	return NULL;
       	     		}
           	 	line = linen + (line - linep);
           	 	linep = linen;
        	}

   	     	if((*line++ = c) == '\n')
		{
   	        	break;
		}
   	 }

   	 *line = '\0';
   	 return linep;
}

