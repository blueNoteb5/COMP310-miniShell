#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>

#define BOOLEAN int
#define TRUE 1
#define FALSE 0

//Struct for a linked list node
typedef struct node {
    struct node *next;
    pid_t pid;
} node;

//Function prototypes
void append(node **head, pid_t pid);

int getcmd(char *prompt, char *args[], BOOLEAN *background);

static void kill_signal_handler(int sig);

static void stop_signal_handler(int sig);

static void cd(char *path);

static void pwd();

static void jobs(node **head);

static void fg(int process_to_foreground);

pid_t remove_by_index(node **head, int n);

void output_redirection(char *command, char *file);

//Global variables
BOOLEAN background; //Boolean variable to check if the job should be run on the background
char *file; //Holds the string for file name
BOOLEAN redirection_flag = FALSE;
node *head; //Beginning of linked list
pid_t foreground_process_id = -1; //Indicates which pid is the foregroung
int list_size = 0; //size of the list
char *args[20]; //Array to hold arguments

int main(void) {

    head = malloc(sizeof(node)); //Malloc for the head of the list
    //initializes the list with a dummy node that will be index 0
    //Will always stay on the list
    head->pid = 0;
    head->next = NULL;

    int command_count = 0;//Counts how many commands user entered
    while (1) {
        redirection_flag = FALSE;
        //Clean up of jobs list every 10 commands entered
        if (command_count >= 10) {
            int n = list_size;
            for (int i = 1; i <= n; i++) {
                remove_by_index(&head, 1);
            }
            command_count = 0; //Resets command count
        }

        int count; //Holds count of how many arguments user typed
        count = getcmd("\n>>", args, &background);
        args[count] = NULL; //Last arg on the array is NULL, necessary
        //for the execvp() function

        //Binding signal handlers
        if (signal(SIGINT, kill_signal_handler) == SIG_ERR) {
            printf("Error binding signal. Abort\n");
            exit(EXIT_FAILURE);
        }
        if (signal(SIGTSTP, stop_signal_handler) == SIG_ERR) {
            printf("Error binding signal. Abort\n");
            exit(EXIT_FAILURE);
        }
        if (signal(SIGCHLD, SIG_IGN) == SIG_ERR){ //Makes sure there are no zombies!
            printf("Error binding signal. Abort\n");
            exit(EXIT_FAILURE);
        }

        //Testing for and running built-in functions
        if (strcmp(args[0], "exit") == 0) {
            exit(EXIT_SUCCESS);
        }
        if (strcmp(args[0], "cd") == 0) {
            cd(args[1]);
            continue;
        } else if (strcmp(args[0], "pwd") == 0) {
            pwd();
            continue;
        } else if (strcmp(args[0], "jobs") == 0) {
            jobs(&head);
            continue;
        } else if (strcmp(args[0], "fg") == 0) {
            int choice_of_process = args[1][0] - '0'; //Converts to int
            fg(choice_of_process);
            continue;
        }

        //Testing for and running redirection
        if (redirection_flag) {
            output_redirection(args[0], file);
            continue;
        }

        pid_t child_pid = fork(); //Forks

        if (child_pid == (pid_t) - 1) { //If error during fork
            fprintf(stderr, "Error during forking process. \n");
            exit(EXIT_FAILURE);
        } else if (child_pid == (pid_t) 0) { //Child code
            execvp(args[0], args);
        } else { //Parent code
            int status; //Holds status of child
            if (background == FALSE) {
                foreground_process_id = child_pid;
                pid_t pid = waitpid(child_pid, &status,
                                    WNOHANG | WUNTRACED); //Parent waits for child
                if (pid == (pid_t) - 1) { //If error during wait
                    fprintf(stdout, "Error during waitpid \n");
                    exit(EXIT_FAILURE);
                }
                printf("\n");
                wait(&status); //Makes sure there are no zombies

            } else {
                append(&head, child_pid); //If background, append to list
            }
        }
        command_count++;
    }
    return 1;
}

int getcmd(char *prompt, char *args[], BOOLEAN *background) {
    int length; //Length of the line to be read
    int i = 0; //index for the args[] array
    char *token;  //Pointer to a string th
    char *last_char; //Pointer to the last char of a line
    char *line = NULL; //Pointer to the line to be read
    size_t linecap = 0; //Variable to be used to store the size of the line read

    printf("%s", prompt);
    //Allocates 100 chars for the line
    line = (char *) malloc((100) * sizeof(char));
    //Length is the length of line
    //getline() automatically reallocs if more space is needed
    length = getline(&line, &linecap, stdin);

    if (length <= 0) {  //If error with length, quit
        exit(-1);
    }

    last_char = strchr(line, '&'); //Finds & in line
    if (last_char != NULL) {   //If loc is not NULL
        *background = TRUE;  //background is true
        *last_char = ' ';
    } else { //If not
        *background = FALSE; //background is false
    }

    while ((token = strsep(&line, " \t\n")) != NULL) { //While there is a token
        if (strcmp(token, ">") == 0) {
            redirection_flag = TRUE;
            file = strsep(&line, " \t\n");
            continue;
        }

        for (int j = 0; j < strlen(token); j++) {
            if (token[j] <= 32) { //If char at index j is not a print character
                if (token[j] == '\04') { //If <Command><d> char, exit
                    exit(EXIT_SUCCESS);
                }
                token[j] = '\0'; //The char at index j is now the null char
            }
        }

        if (strlen(token) > 0) {
            args[i++] = token; //The args array at i now contains token
        }
    }

    free(line); //Frees the memory allocated for the line
    return i;
}

//Handler for kill signal
static void kill_signal_handler(int sig) {
    kill(foreground_process_id, SIGKILL);
    return;
}

//Handler for stop signal
static void stop_signal_handler(int sig) {
    return;
}

//Appends to list
void append(node **head, pid_t pid) {
    node *current = *head;

    while (current->next != NULL) {
        current = current->next;
    }

    current->next = malloc(sizeof(node));
    current->next->pid = pid;
    current->next->next = NULL;
    list_size++;
    return;
}

//built-in cd function
static void cd(char *path) {
    int return_value = chdir(path);
    if (return_value == -1) {
        printf("Error during chdir\n");
    }
    return;
}

//built-in pwd function
static void pwd() {
    char *pwd = getcwd(NULL, 0);
    printf("%s\n", pwd);
    free(pwd);
    return;
}

//built-in jobs function
static void jobs(node **head) {
    node *current = *head;
    int i = 1;
    while (current != NULL) {
        if (current->pid != 0) {
            printf("Job: %d Process ID: %d\n", i, current->pid);
            i++;
        }
        current = current->next;
    }
}

//built-in fg function
static void fg(int process_to_foreground) {
    int status;
    //Checks if user entered a valid number
    if (process_to_foreground > list_size || process_to_foreground < 1) {
        printf("There is no such process\n");
        return;
    } else {
        //Gets the id of desired process
        foreground_process_id = remove_by_index(&head, process_to_foreground);
    }
    //Waits for it
    pid_t pid = waitpid(foreground_process_id, &status,
                        WNOHANG | WUNTRACED); //Parent waits for child
    if (pid == (pid_t) - 1) { //If error during wait
        fprintf(stdout, "Error during waitpid \n");
        exit(EXIT_FAILURE);
    }
    printf("\n");
    wait(&status);
}

//Removes by index and returns pid of removed node
pid_t remove_by_index(node **head, int n) {
    int i = 0;
    int return_pid = -1;
    node *current = *head;
    node *temp = NULL;

    if (n == 0) {
        return return_pid;
    }

    for (int i = 0; i < n - 1; i++) {
        if (current->next == NULL) {
            return -1;
        }
        current = current->next;
    }

    temp = current->next;
    return_pid = temp->pid;
    current->next = temp->next;
    free(temp);
    list_size--;
    return return_pid;
}

//Handles output_redirection
void output_redirection(char *command, char *file) {
    pid_t child_pid = fork(); //Forks

    if (child_pid == (pid_t) - 1) { //If error during fork
        fprintf(stderr, "Error during forking process. \n");
        exit(EXIT_FAILURE);
    } else if (child_pid == (pid_t) 0) { //Child code
        close(1); //Closes stdout
        //Opens a file
        int file_value = open(file, O_WRONLY | O_CREAT | O_TRUNC,
                              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (file_value == -1) {
            printf("Error opening file\n");
        }
        execvp(args[0], args);
    } else { //Parent code
        int status; //Holds status of child
        foreground_process_id = child_pid;
        pid_t pid = waitpid(child_pid, &status,
                            WNOHANG | WUNTRACED); //Parent waits for child
        if (pid == (pid_t) - 1) { //If error during wait
            fprintf(stdout, "Error during waitpid \n");
            exit(EXIT_FAILURE);
        }
        printf("\n");
        wait(&status);
    }
}
