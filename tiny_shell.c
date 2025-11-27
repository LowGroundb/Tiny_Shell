#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdbool.h>
#include <fcntl.h>
extern char **environ;  // Για το execve


int redirect(char *infile, char *outfile, int append) {
    int fd;

    if (outfile != NULL) {
        if (append)
            fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0777);
        else
            fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0777);

        if (fd < 0) {
            perror("open");
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    if (infile != NULL) {
        fd = open(infile, O_RDONLY);
        if (fd < 0) {
            perror("open infile");
            return -1;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    return 0;
}

char* find_in_path(const char* command) {
    if (strchr(command, '/') != NULL) {
        if (access(command, X_OK) == 0) {
            return strdup(command);
        }
        return NULL;
    }
    char* path_env = getenv("PATH");
    if (!path_env) return NULL;
    
    char* path_copy = strdup(path_env);
    char* dir = strtok(path_copy, ":");
    char* result = NULL;
    
    while (dir != NULL) {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, command);
        
        if (access(full_path, X_OK) == 0) {
            result = strdup(full_path);
            break;
        }
        dir = strtok(NULL, ":");
    }
    
    free(path_copy);
    return result;
}

int execute_external_command(char *argv[], char *infile, char *outfile, int append) {
    char* full_path = find_in_path(argv[0]);
    if (!full_path) {
        printf("%s: command not found\n", argv[0]);
        return 1;
    }
    
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("fork");
        free(full_path);
        return 1;
    } 
    if (pid == 0) {
        
        //child
        if (redirect(infile, outfile, append) < 0)
            exit(1);
        execve(full_path, argv, environ);
        perror("execve");
        free(full_path);
        exit(127);
    }
    else {
        //parent
        int status;
        waitpid(pid, &status, 0);
        free(full_path);
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code != 0) {
                printf("Exit code: %d\n", exit_code);
            }
        } else if (WIFSIGNALED(status)) {
            printf("Process terminated by signal: %d\n", WTERMSIG(status));
        }
        return 1;
    }
}
int parse_redirection(char *argv[], int *argc,char **infile, char **outfile, int *append) {
    *infile = NULL;
    *outfile = NULL;
    *append = 0;
    for (int i = 0; i < *argc; i++) {
        if (argv[i] == NULL) continue;

        if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], ">>") == 0) {
            if (i + 1 >= *argc || argv[i+1] == NULL) {
                printf("Syntax error: missing output file\n");
                return -1;
            }

            *outfile = argv[i+1];
            *append = (strcmp(argv[i], ">>") == 0);

            argv[i]   = NULL;
            argv[i+1] = NULL;
            i++;
        }
        else if (strcmp(argv[i], "<") == 0) {
            if (i + 1 >= *argc || argv[i+1] == NULL) {
                printf("Syntax error: missing input file\n");
                return -1;
            }
            *infile = argv[i+1];
            argv[i]   = NULL;
            argv[i+1] = NULL;
            i++;
        }
    }
    int w = 0;
    for (int r = 0; r < *argc; r++)
        if (argv[r] != NULL)
            argv[w++] = argv[r];

    argv[w] = NULL;
    *argc = w;

    return 0;
}
int command_count(char *argv[], int argc){
    int count = 0;
    for (int i = 0; i < argc; i++) {
        if (i == 0 || (i > 0 && argv[i-1] == NULL)) {
            if (argv[i] != NULL) count++;
        }
    }
    return count;
}
int pipe_detection(char *argv[], int argc){
    for (int i = 0; i < argc; i++) {
        if (argv[i] == NULL) continue;

        if (strcmp(argv[i], "|") == 0 ){
            if (i==0 ||argv[i+1] == NULL || strcmp(argv[i+1], "|") == 0) {
                printf("Syntax error: missing command after pipe\n");
                return 2;
            }
            argv[i] = NULL;
        } 
    }
   return  command_count(argv, argc) ;

  
}
int piping(char *argv[], int pipe_count){
    int fd[pipe_count-1][2];
    pid_t pids[pipe_count];

    for(int i = 0; i < pipe_count - 1; i++){
        if(pipe(fd[i]) == -1){
            perror("pipe");
            return -1;
        }
    }
    int start = 0;
    for(int i = 0; i < pipe_count; i++){
        int end = start;
        while(argv[end] != NULL) {
            end++;
        }
        char **cmd = &argv[start];

        pids[i] = fork();
        if(pids[i] < 0){
            perror("fork");
            return -1;
        }
        if(pids[i] == 0){
            //child
            if(i > 0){
                dup2(fd[i-1][0], STDIN_FILENO);
            }
            if(i < pipe_count - 1){
                dup2(fd[i][1], STDOUT_FILENO);
            }
            for(int j = 0; j < pipe_count - 1; j++){
                close(fd[j][0]);
                close(fd[j][1]);
            }
            // κάνουμε redirect πριν το execvp
            char *infile = NULL;
            char *outfile = NULL;
            int append = 0;
            int dummy_argc = (end - start);  
            parse_redirection(cmd, &dummy_argc, &infile, &outfile, &append);

          
            if (redirect(infile, outfile, append) < 0) {
                exit(1);
                }
            execvp(cmd[0], cmd);
            perror("execvp");
            exit(127);
        }
        else{
            //parent
            start = end + 1;
        }
    }
    for(int i = 0; i < pipe_count - 1; i++){
        close(fd[i][0]);
        close(fd[i][1]);
    }
    for(int i = 0; i < pipe_count; i++){
        waitpid(pids[i], NULL, 0);
    }
    return 1;
}
int command_processing(char *argv[], int argc) {
    if (argc == 0 || argv[0] == NULL) return -1;
    // na rwthsw an to oti exit asdaskd kanei exit einai kako
    int commands = pipe_detection(argv, argc);
    if (commands == -1){
        return -1;
    }
    if (commands > 1) {
        return piping(argv, commands);
    }
    char* infile = NULL;
    char* outfile = NULL;
    int append = 0;
    parse_redirection(argv, &argc, &infile, &outfile, &append);

     if (strcmp(argv[0], "exit") == 0) {
        return 0;
    }
    if (strcmp(argv[0], "help") == 0) {
        printf("Available commands:\n");
        printf("help - Show this help message\n");
        printf("exit - Exit the shell\n");
        printf("ls - List directory contents\n");
        printf("cat - Concatenate and display files\n");
        printf("echo - Display a line of text\n");
        return 1;
    }
        return execute_external_command(argv, infile, outfile, append);
}


int input_detection(void){
    char buffer[1024];
    char *command[20];
    int argc=0;

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return -1; 
    }
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n')
        buffer[len - 1] = '\0';
    char *token = strtok(buffer, " ");
    while (token && argc < 20) {
        command[argc++] = token;
        token = strtok(NULL, " ");
    }
    command[argc] = NULL;
    if (argc == 0) {
        return 1;
    }
    return command_processing(command, argc);
    
}
void tiny_shell_loop(void){
    while (1){
    printf("tiny-shell>> ");
    if(input_detection()!=1){
        break;
}
}
}


int main(void){
    tiny_shell_loop();
    return 0;           
}