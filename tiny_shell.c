#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

extern char **environ;  // Για το execve

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

int execute_external_command(char *argv[]) {
    char* full_path = find_in_path(argv[0]);
    if (!full_path) {
        printf("%s: command not found\n", argv[0]);
        return 1;
    }
    
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        free(full_path);
        return 1;
    } else if (pid == 0) {
        execve(full_path, argv, environ);
        perror("execve");
        free(full_path);
        exit(127);
    } else {
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

int command_processing(char *argv[], int argc) {
    if (argc == 0) return 1;
    
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
    if (strcmp(argv[0], "ls") == 0 || strcmp(argv[0], "cat") == 0 || strcmp(argv[0], "echo") == 0) {
        return execute_external_command(argv);
    }
    
    printf("%s: command not found.\n", argv[0]);
    return 1;
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