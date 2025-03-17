#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define BLUE    "\x1b[34m"
#define RESET   "\x1b[0m"  

const int RSH_LINE_BUFSIZE = 256;
const int RSH_ARG_BUFSIZE = 64;
const int DIR_LEN = 64;
const int RSH_MAX_DIR_LEN = 256;
const char RSH_DELIMITER = ' ';
const char *ROOT_DIR = "/";
const char *ROOT_DIR_INST = "~";
const char *PREV_DIR_INST = "..";
const char *SAME_DIR_INST = ".";
char *MACHINE_ROOT_DIR;

struct DirectoryNode {
    struct DirectoryNode *parent;
    char current_path[RSH_MAX_DIR_LEN];
    char dir_name[DIR_LEN];
};

int arg_size;
struct DirectoryNode *current_dir_node;


void print_args(char **args) {
    printf("ARGS: ");
    for (int i = 0; i < arg_size; i++) {
        printf("%s ", args[i]);
    }
    printf("\n");
}

void clear_dir_tree() {
    while (current_dir_node) {
        struct DirectoryNode *parent = current_dir_node->parent;
        free(current_dir_node);
        current_dir_node = parent;
    }
}

struct DirectoryNode *create_directory_node(struct DirectoryNode *parent, const char * dir_name, const char *new_dir_path) {
    struct DirectoryNode *new_node = malloc(sizeof(struct DirectoryNode));
    new_node->parent = parent;
    strcpy(new_node->dir_name, dir_name);
    strcpy(new_node->current_path, new_dir_path);
    return new_node;
}

int rsh_init() {
    current_dir_node = create_directory_node(NULL, ROOT_DIR, ROOT_DIR);
    MACHINE_ROOT_DIR = malloc(RSH_MAX_DIR_LEN);
    if (getcwd(MACHINE_ROOT_DIR, RSH_MAX_DIR_LEN) == NULL) {
        perror("getcwd failed");
        free(MACHINE_ROOT_DIR);
        exit(EXIT_FAILURE);
    }
    
    if (current_dir_node == 0) {
        return 0;
    } 
    return 1;
}

int rsh_cleanup() {
    clear_dir_tree();
    free(MACHINE_ROOT_DIR);
    MACHINE_ROOT_DIR = NULL;

    return 1;
}

// builtin functions

int rsh_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "rsh: expected argument to \"cd\"\n");
    } else {
        if (strcmp(args[1], PREV_DIR_INST) == 0 && !current_dir_node->parent) {
            // already at root 
            return 1;
        } else if (strcmp(args[1], SAME_DIR_INST) == 0) {
            // stay in same directory
            return 1;
        } else if (strcmp(args[1], ROOT_DIR_INST) == 0) {
            if (chdir(MACHINE_ROOT_DIR) != 0) {
                perror("rsh");
            } else {
                clear_dir_tree();
                current_dir_node = create_directory_node(NULL, ROOT_DIR, ROOT_DIR);
            }
        } else {
            if (chdir(args[1]) != 0) {
                perror("rsh");
            } else {
                if (strcmp(args[1], PREV_DIR_INST) == 0) {
                    // go to parent
                    struct DirectoryNode *parent = current_dir_node->parent;
                    free(current_dir_node);
                    current_dir_node = parent;
                } else {
                    char new_path[1024];
                    strcat(new_path, current_dir_node->current_path);
                    if (current_dir_node->parent) {
                        strcat(new_path, "/");
                    }
                    strcat(new_path, args[1]);
                    new_path[strlen(new_path)] = '\0';
                    current_dir_node = create_directory_node(current_dir_node, args[1], new_path);
                }
                
            }
        }
    }
    return 1;
}

int rsh_exit(char **args) {
    rsh_cleanup();
    exit(EXIT_SUCCESS);
}

int rsh_pwd(char **args) {
    printf("%s\n", current_dir_node->current_path);
    return 1;
}

const char *builtin_str[] = {
    "cd",
    "exit",
    "pwd",
};

int (*builtin_func[])(char **) = {
    &rsh_cd,
    &rsh_exit,
    &rsh_pwd,
};

int rsh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

char *rsh_getline() {
    int i = 0;
    char read_char;
    int buf_size = RSH_LINE_BUFSIZE;
    char * buffer = malloc(sizeof(char) * RSH_LINE_BUFSIZE);

    if (!buffer) {
        printf("rsh error: issue allocating buffer");
    }
    
    while(scanf("%c", &read_char)) {
        if (read_char == '\n' || read_char == EOF) {
            buffer[i] = '\0';
            break;
        }

        buffer[i] = read_char;
        i += 1;
        if (i >= buf_size) {
            buf_size += RSH_LINE_BUFSIZE;
            buffer = realloc(buffer, sizeof(char) * buf_size);
            if (!buffer) {
                printf("rsh error: issue allocating buffer");
            }
        }
    }

    return buffer;
}

char **rsh_gettokens(char *line) {
    int buf_size = RSH_ARG_BUFSIZE;
    char **tokens = malloc(sizeof(char *) * RSH_ARG_BUFSIZE);
    int i = 0;

    char *tok = strtok(line, &RSH_DELIMITER);
    while (tok) {
        tokens[i] = tok;
        i += 1;

        if (i >= buf_size) {
            buf_size += RSH_ARG_BUFSIZE;
            tokens = realloc(tokens, sizeof(char) * buf_size);
            if (!tokens) {
                printf("rsh error: issue allocating buffer");
            }
        }
        tok = strtok(NULL, &RSH_DELIMITER);
    }

    arg_size = i;
    return tokens;
}

int rsh_launch(char **args) {
    pid_t pid, wpid;
    pid = fork();
    int status;
    if (pid < 0) {
        perror("Error creating new process");
    }
    
    if (pid == 0) {
        if (execvp(args[0], args) == -1) {
            perror("rsh");
        }
        exit(EXIT_FAILURE);
    } else {
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
          } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

int rsh_exec(char **args) {

    if (args[0] == NULL) {
        // An empty command was entered.
        return 1;
    }

    for (int i = 0; i < rsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return rsh_launch(args);
}

int rsh_loop() {
    int status = 0;
    char *line;
    char **args;

    printf(BLUE "Welcome to rshell!\n" RESET);
    do {
        printf(BLUE "%s > " RESET, current_dir_node->current_path);
        line = rsh_getline();
        args = rsh_gettokens(line);
        status = rsh_exec(args);
        
        free(line);
        free(args);
    } while (status);

    return 0;
}

int main(int argc, char ** argv) {
    if (argc != 1) {
        printf("More than 1 argument, given %d\n", argc);
        return 1;
    }
    
    if (!rsh_init()) {
        fprintf(stderr, "Error setting up shell");
    } else {
        rsh_loop();
    }

    rsh_cleanup();
    return EXIT_SUCCESS;
}
