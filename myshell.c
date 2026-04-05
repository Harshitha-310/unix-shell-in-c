#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_TOKENS 100
#define HISTORY_SIZE 100

char *history[HISTORY_SIZE];
int historyCount = 0;

/* ---------------- HISTORY FUNCTIONS ---------------- */

void addToHistory(char *command)
{
    if(historyCount < HISTORY_SIZE)
        history[historyCount++] = strdup(command);
}

void showHistory()
{
    for(int i=0;i<historyCount;i++)
        printf("%d %s\n", i+1, history[i]);
}

/* ---------------- SINGLE COMMAND ---------------- */

void executeCommand(char *line)
{
    char *args[MAX_TOKENS];
    int i=0;

    char *token=strtok(line," \t\n");

    while(token!=NULL && i<MAX_TOKENS-1)
    {
        args[i++]=token;
        token=strtok(NULL," \t\n");
    }

    args[i]=NULL;

    if(args[0]==NULL)
        return;

    if(strcmp(args[0],"cd")==0)
    {
        if(args[1]==NULL)
            fprintf(stderr,"cd: missing argument\n");
        else if(chdir(args[1])!=0)
            perror("cd");

        return;
    }

    pid_t pid=fork();

    if(pid==0)
    {
        if(execvp(args[0],args)==-1)
            printf("Shell: Incorrect command\n");

        exit(1);
    }
    else if(pid>0)
        waitpid(pid,NULL,0);
    else
        perror("fork");
}

/* ---------------- PARALLEL COMMANDS ---------------- */

void executeParallelCommands(char *line)
{
    char *commands[MAX_TOKENS];
    int count=0;

    char *token=strtok(line,"&&");

    while(token!=NULL)
    {
        commands[count++]=token;
        token=strtok(NULL,"&&");
    }

    pid_t pids[MAX_TOKENS];

    for(int i=0;i<count;i++)
    {
        while(*commands[i]==' ')
            commands[i]++;

        char *args[MAX_TOKENS];
        int argCount=0;

        char *arg=strtok(commands[i]," \t\n");

        while(arg!=NULL)
        {
            args[argCount++]=arg;
            arg=strtok(NULL," \t\n");
        }

        args[argCount]=NULL;

        if(strcmp(args[0],"cd")==0)
        {
            if(args[1]==NULL)
                fprintf(stderr,"cd: missing argument\n");
            else if(chdir(args[1])!=0)
                perror("cd");

            continue;
        }

        pid_t pid=fork();

        if(pid==0)
        {
            execvp(args[0],args);
            printf("Shell: Incorrect command\n");
            exit(1);
        }
        else
            pids[i]=pid;
    }

    for(int i=0;i<count;i++)
        waitpid(pids[i],NULL,0);
}

/* ---------------- SEQUENTIAL COMMANDS ---------------- */

void executeSequentialCommands(char *line)
{
    char *commands[MAX_TOKENS];
    int i=0;

    char *token=strtok(line,"##");

    while(token!=NULL)
    {
        commands[i++]=strdup(token);
        token=strtok(NULL,"##");
    }

    for(int j=0;j<i;j++)
    {
        char *cmd=commands[j];

        while(*cmd==' ')
            cmd++;

        executeCommand(cmd);

        free(commands[j]);
    }
}

/* ---------------- REDIRECTION ---------------- */

void executeCommandRedirection(char *line)
{
    char *args[MAX_TOKENS];
    char *outfile=NULL;

    int append=0;

    char *redir=strstr(line,">>");

    if(redir)
        append=1;
    else
        redir=strstr(line,">");

    if(redir)
    {
        *redir='\0';
        redir++;

        if(append)
            redir++;

        while(*redir==' ')
            redir++;

        outfile=redir;
    }

    int i=0;

    char *token=strtok(line," \t\n");

    while(token!=NULL)
    {
        args[i++]=token;
        token=strtok(NULL," \t\n");
    }

    args[i]=NULL;

    pid_t pid=fork();

    if(pid==0)
    {
        int fd;

        if(append)
            fd=open(outfile,O_WRONLY|O_CREAT|O_APPEND,0644);
        else
            fd=open(outfile,O_WRONLY|O_CREAT|O_TRUNC,0644);

        dup2(fd,STDOUT_FILENO);
        close(fd);

        execvp(args[0],args);

        printf("Shell: Incorrect command\n");
        exit(1);
    }

    waitpid(pid,NULL,0);
}

/* ---------------- PIPELINE ---------------- */

void executeCommandPipelines(char *line)
{
    char *commands[MAX_TOKENS];

    int numCommands=0;

    char *token=strtok(line,"|");

    while(token!=NULL)
    {
        commands[numCommands++]=token;
        token=strtok(NULL,"|");
    }

    int pipefd[2*(numCommands-1)];

    for(int i=0;i<numCommands-1;i++)
        pipe(pipefd+i*2);

    for(int i=0;i<numCommands;i++)
    {
        if(fork()==0)
        {
            if(i>0)
                dup2(pipefd[(i-1)*2],STDIN_FILENO);

            if(i<numCommands-1)
                dup2(pipefd[i*2+1],STDOUT_FILENO);

            for(int j=0;j<2*(numCommands-1);j++)
                close(pipefd[j]);

            char *args[MAX_TOKENS];

            int k=0;

            char *arg=strtok(commands[i]," \t\n");

            while(arg!=NULL)
            {
                args[k++]=arg;
                arg=strtok(NULL," \t\n");
            }

            args[k]=NULL;

            execvp(args[0],args);

            printf("Shell: Incorrect command\n");

            exit(1);
        }
    }

    for(int i=0;i<2*(numCommands-1);i++)
        close(pipefd[i]);

    for(int i=0;i<numCommands;i++)
        wait(NULL);
}

/* ---------------- MAIN LOOP ---------------- */

int main()
{
    char *line=NULL;

    size_t len=0;

    char cwd[1024];

    signal(SIGINT,SIG_IGN);
    signal(SIGTSTP,SIG_IGN);

    while(1)
    {
        getcwd(cwd,sizeof(cwd));

        printf("%s$ ",cwd);

        getline(&line,&len,stdin);

        line[strcspn(line,"\n")]=0;

        if(strcmp(line,"exit")==0)
            break;

        if(strcmp(line,"history")==0)
        {
            showHistory();
            continue;
        }

        if(strcmp(line,"!!")==0)
        {
            if(historyCount==0)
            {
                printf("No commands in history\n");
                continue;
            }

            strcpy(line,history[historyCount-1]);
            printf("%s\n",line);
        }

        if(line[0]=='!' && strlen(line)>1)
        {
            int index=atoi(&line[1])-1;

            if(index<0 || index>=historyCount)
            {
                printf("Invalid history command\n");
                continue;
            }

            strcpy(line,history[index]);

            printf("%s\n",line);
        }

        addToHistory(line);

        if(strstr(line,"&&"))
            executeParallelCommands(line);

        else if(strstr(line,"##"))
            executeSequentialCommands(line);

        else if(strstr(line,">"))
            executeCommandRedirection(line);

        else if(strstr(line,"|"))
            executeCommandPipelines(line);

        else
            executeCommand(line);
    }

    return 0;
}