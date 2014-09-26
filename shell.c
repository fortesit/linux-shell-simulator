/*

 Linux shell simulator

 Version: 1.0
 GitHub repository: https://github.com/fortesit/linux-shell-simulator
 Author: Sit King Lok
 Last modified: 2014-09-26 19:48
 
 Description:
 A simplified Linux shell with the following features
 1. Accept most of the commands with arguments (e.g. cd, ls -a, exit)
 2. I/O redirection (i.e. >, >>, <, <<)
 3. Pipes (i.e. |)
 4. Signal handling (e.g. Ctrl+Z, Ctrl+C)
 5. Job control (i.e. jobs, fg)

 Usage:
 gcc shell.c -o shell
 ./shell

 Platform:
 Unix(Mac/Linux)

 Note:
 All arguments should have a white-space in between.
 
 Example:
 See README.md

*/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define STRSIZ 255

enum cmdType {START, REDIR_IN, REDIR_OUT, BUILTIN, ARG, PIPE, CMDNAME, IN_FILE, OUT_FILE, BUILTIN_WITH_ARG};
int tokenType[128] = {START}, numOfTokens, currentCmdPos = 0, pipes[4], curPipe = 0, errsv, processCnt;
char inputString[STRSIZ + sizeof(char) * 2], inputStringBackup[STRSIZ + sizeof(char)], tokenizedInput[128][STRSIZ]; // inputString[] = STRSIZ + \n + \0
pid_t pid[3];

struct suspendedJobs
{
    int jobNum;
    pid_t pid[3];
    char jobName[STRSIZ];
    struct suspendedJobs *next;
};

struct suspendedJobs *head = NULL, *curr = NULL;

struct suspendedJobs* addJob(pid_t pid[3])
{
    int i;
    struct suspendedJobs *ptr = (struct suspendedJobs*)malloc(sizeof(struct suspendedJobs));
    if (ptr == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return NULL;
    }
    ptr->jobNum = (head ? curr->jobNum + 1 : 1); // If no head, set jobNum to 1
    for (i = 0; i < 3; i++) {
        ptr->pid[i] = pid[i];
    }
    strcpy(ptr->jobName, inputStringBackup);
    ptr->next = NULL;
    if (head == NULL) {
        head = curr = ptr;
    } else {
        curr = curr->next = ptr; // Change curr->next in the current node to point to this node (ptr), and then set this node to be the current node.
    }
    return ptr;
}

struct suspendedJobs* searchJob(int jobNum, struct suspendedJobs **preceding)
{
    struct suspendedJobs *ptr = head;
    struct suspendedJobs *tmp = NULL;
    while (ptr != NULL) { // Parse until no more nodes
        if (ptr->jobNum == jobNum) {
            if (preceding) { // If there exist a preceding node
                *preceding = tmp; // Set the preceding node
            }
            return ptr;
        } else {
            tmp = ptr;
            ptr = ptr->next; // Move to next node
        }
    }
    return NULL; // No results or the linked list is empty
}

bool deleteJob(int jobNum)
{
    struct suspendedJobs *preceding = NULL, *del = NULL, *ptr;
    ptr = del = searchJob(jobNum, &preceding); // Passed by reference. Content of preceding will be changed.
    if (del == NULL) {
        return EXIT_FAILURE;
    } else {
        while (ptr != curr) { // Decrease the jobNum after.
            ptr = ptr->next;
            ptr->jobNum--;
        }
        if (preceding != NULL) {
            preceding->next = del->next; // Disassoicate the node (del)
        }
        if (del == curr) { // Consider the case of deleting the last node
            curr = preceding; // curr should be reset to the preceding node
        }
        if (del == head) { // Consider the case of deleting the first node
            head = del->next; // head should be changed as well
        }
    }
    free(del);
    del = NULL;
    return EXIT_SUCCESS;
}

void setSignalBehavior(int restoreFlag)
{
    if (restoreFlag) {
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
    } else {
        signal(SIGINT, SIG_IGN);
        signal(SIGTERM, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
    }
}

void printPrompt()
{
    char buf[BUFSIZ + sizeof(char)];
    if (feof(stdin)) {
        clearerr(stdin);
        printf("\n");
    }
    printf("[3150 shell:%s]$ ", getcwd(buf, BUFSIZ));
}

bool readInput()
{
    strcpy(inputString, "");
    if (fgets(inputString, sizeof(inputString), stdin) != NULL) {
        if (strchr(inputString, '\n')) {
            return EXIT_SUCCESS;
        } else {
            fprintf(stderr, "Command is too long!\n");
            do {
                fgets(inputString, sizeof(inputString), stdin); // Clear string buffer
            } while (!strchr(inputString, '\n')); // until '\n' appear
        }
    }
    return EXIT_FAILURE;
}

bool tokenize()
{
    int i, * previousToken = tokenType, piped = 0, redirected = 0;
    bool invalidInput = false;
    char * token;
    for (i = 0; i < 128; i++) {
        strcpy(tokenizedInput[i], "");
    }
    inputString[strlen(inputString)-1] = '\0'; // Trim \n off
    strcpy(inputStringBackup, inputString);
    for (i = 0, token = strtok(inputString, " "); token; i++, previousToken++, token = strtok(NULL, " ")) {
        switch (*previousToken) {
            case START:
                if (!(strcmp(token, "exit") && strcmp(token, "jobs"))) {
                    *(previousToken + 1) = BUILTIN;
                } else if (!(strcmp(token, "cd") && strcmp(token, "fg"))) {
                    *(previousToken + 1) = BUILTIN_WITH_ARG;
                } else if (!(strchr(token, 9) || strchr(token, 62) || strchr(token, 60) || strchr(token, 124) || strchr(token, 42) || strchr(token, 33) || strchr(token, 96) || strchr(token, 39) || strchr(token, 34))) {
                    *(previousToken + 1) = CMDNAME;
                } else {
                    invalidInput = true;
                }
                break;
            case REDIR_IN:
                if (!(strchr(token, 9) || strchr(token, 62) || strchr(token, 60) || strchr(token, 124) || strchr(token, 42) || strchr(token, 33) || strchr(token, 96) || strchr(token, 39) || strchr(token, 34))) {
                    *(previousToken + 1) = IN_FILE;
                } else {
                    invalidInput = true;
                }
                break;
            case REDIR_OUT:
                if (!(strchr(token, 9) || strchr(token, 62) || strchr(token, 60) || strchr(token, 124) || strchr(token, 42) || strchr(token, 33) || strchr(token, 96) || strchr(token, 39) || strchr(token, 34))) {
                    *(previousToken + 1) = OUT_FILE;
                } else {
                    invalidInput = true;
                }
                break;
            case BUILTIN:
                invalidInput = true;
                break;
            case ARG:
                if (!(strcmp(token, "<") || piped)) {
                    *(previousToken + 1) = REDIR_IN;
                    redirected++;
                } else if (!(strcmp(token, ">") && strcmp(token, ">>"))) {
                    *(previousToken + 1) = REDIR_OUT;
                    redirected++;
                } else if (!strcmp(token, "|") && piped < 2) {
                    *(previousToken + 1) = PIPE;
                    piped++;
                } else if (!(strchr(token, 9) || strchr(token, 62) || strchr(token, 60) || strchr(token, 124) || strchr(token, 42) || strchr(token, 33) || strchr(token, 96) || strchr(token, 39) || strchr(token, 34))) {
                    *(previousToken + 1) = ARG;
                } else {
                    invalidInput = true;
                }
                break;
            case PIPE:
                if (!(strchr(token, 9) || strchr(token, 62) || strchr(token, 60) || strchr(token, 124) || strchr(token, 42) || strchr(token, 33) || strchr(token, 96) || strchr(token, 39) || strchr(token, 34)) && (strcmp(token, "cd") && strcmp(token, "exit") && strcmp(token, "fg") && strcmp(token, "jobs"))) {
                    *(previousToken + 1) = CMDNAME;
                } else {
                    invalidInput = true;
                }
                break;
            case CMDNAME:
                if (!(strcmp(token, "<") || piped)) {
                    *(previousToken + 1) = REDIR_IN;
                    redirected++;
                } else if (!(strcmp(token, ">") && strcmp(token, ">>"))) {
                    *(previousToken + 1) = REDIR_OUT;
                    redirected++;
                } else if (!strcmp(token, "|") && piped < 2) {
                    *(previousToken + 1) = PIPE;
                    piped++;
                } else if (!(strchr(token, 9) || strchr(token, 62) || strchr(token, 60) || strchr(token, 124) || strchr(token, 42) || strchr(token, 33) || strchr(token, 96) || strchr(token, 39) || strchr(token, 34))) {
                    *(previousToken + 1) = ARG;
                } else {
                    invalidInput = true;
                }
                break;
            case IN_FILE:
                if (!(strcmp(token, ">") && strcmp(token, ">>")) && redirected < 2) {
                    *(previousToken + 1) = REDIR_OUT;
                    redirected++;
                } else if (!strcmp(token, "|") && piped < 2) {
                    *(previousToken + 1) = PIPE;
                    piped++;
                } else {
                    invalidInput = true;
                }
                break;
            case OUT_FILE:
                if (!(strcmp(token, "<") || piped)  && redirected < 2) {
                    *(previousToken + 1) = REDIR_IN;
                    redirected++;
                } else {
                    invalidInput = true;
                }
                break;
            case BUILTIN_WITH_ARG:
                if (!(strchr(token, 9) || strchr(token, 62) || strchr(token, 60) || strchr(token, 124) || strchr(token, 42) || strchr(token, 33) || strchr(token, 96) || strchr(token, 39) || strchr(token, 34))) {
                    *(previousToken + 1) = ARG;
                } else {
                    invalidInput = true;
                }
                break;
        }
        strcpy(tokenizedInput[i+1], token);
    }
    numOfTokens = i;
    if (!(strcmp(tokenizedInput[1], "exit") && strcmp(tokenizedInput[1], "jobs"))) {
        if (i != 1) {
            printf("%s: wrong number of arguments\n", tokenizedInput[1]);
            return false;
        }
    } else if (!(strcmp(tokenizedInput[1], "cd") && strcmp(tokenizedInput[1], "fg"))) {
        if (i != 2) {
            printf("%s: wrong number of arguments\n", tokenizedInput[1]);
            return false;
        }
    }
    if (invalidInput || tokenType[i] == REDIR_IN || tokenType[i] == REDIR_OUT || tokenType[i] == PIPE || tokenType[i] == BUILTIN_WITH_ARG) {
        printf("Error: invalid input command line\n");
        return false;
    }
    return true;
}

bool nextCommand()
{
    int i;
    for (i = currentCmdPos + 1; i <= numOfTokens; i++) {
        if (tokenType[i] == CMDNAME || tokenType[i] == BUILTIN || tokenType[i] == BUILTIN_WITH_ARG) {
            currentCmdPos = i;
            return true; // Some more tokens left
        }
    }
    currentCmdPos = 0; // Reset current position when no more tokens left
    return false; // No more tokens left
}

void runBuiltIn()
{
    struct suspendedJobs *ptr;
    int i, status, jobNum = atoi(tokenizedInput[2]);
    bool resumed = false;
    if (!strcmp(tokenizedInput[1], "cd")) {
        if (chdir(tokenizedInput[2])) {
            printf("[%s]: cannot change directory.\n", tokenizedInput[2]);
        }
    }
    if (!strcmp(tokenizedInput[1], "exit")) {
        if (head != NULL) {
            fprintf(stderr, "There is at least one suspended job\n");
            return;
        }
        exit(EXIT_SUCCESS);
    }
    if (!strcmp(tokenizedInput[1], "jobs")) {
        if ((ptr = head) == NULL) {
            printf("No suspended jobs\n");
        }
        while (ptr != NULL) {
            printf("[%d]: %s\n", ptr->jobNum, ptr->jobName);
            ptr = ptr->next;
        }
    }
    if (!strcmp(tokenizedInput[1], "fg")) {
        if (head == NULL || (jobNum < 1 || jobNum > curr->jobNum)) {
            fprintf(stderr, "No such job!\n");
        } else {
            ptr = searchJob(jobNum, NULL);
            printf("Job wake up: %s\n", ptr->jobName);
            for (i = 0; i < 3 && ptr->pid[i] != 0; i++) {
                if (kill(ptr->pid[i], SIGCONT) == -1) {
                    fprintf(stderr, "Errors occur when sending signal to child process of PID: %d\n", ptr->pid[i]);
                }
            }
            for (i = 0; i < 3 && ptr->pid[i] != 0; i++) {
                if (waitpid(ptr->pid[i], &status, WUNTRACED) != ptr->pid [i]) {
                    fprintf(stderr, "Child process terminated unexpectedly. Program exit.\n");
                    exit(EXIT_FAILURE);
                } else if (i == 0 && WIFSTOPPED(status)) { // Stopped and can be resumed by ^Z or kill
                    printf("\n");
                    resumed = true;
                }
            }
            if (!resumed) { // If it resumed again we do not need to delete the job
                deleteJob(jobNum);
            }
        }
    }
}

void closePipesFD()
{
	close(pipes[0]);
	close(pipes[1]);
	close(pipes[2]);
	close(pipes[3]);
}

bool initIORedirection()
{
	int i, isEndingPipe = 0;
	int in_file, out_file;
	for (i = currentCmdPos - 1; i < numOfTokens && !isEndingPipe; i++) {  // Start from currentCmdPos-1 to read the pipe "|"
		switch(tokenType[i]) {
			case PIPE:
				if (i == currentCmdPos - 1) { // Pipe into this program
					dup2(pipes[curPipe], STDIN_FILENO);
					curPipe += 2;  // Move to next pipe. Note this is in CHILD, so we must do it later in PARENT also
				} else {  // Pipe out
					dup2(pipes[curPipe+1], STDOUT_FILENO);
					isEndingPipe = 1;   // Finished reading command, exit loop
				}
				break;
			case REDIR_IN:
				in_file = open(tokenizedInput[i+1], O_RDONLY);
                errsv = errno;
				if (in_file < 0){
					if (errsv == ENOENT) {
						fprintf(stderr, "%s: no such file or directory\n", tokenizedInput[i+1]);
					} else {
						fprintf(stderr, "%s: unknown error\n", tokenizedInput[i+1]);
					}
					return false;
				}
                dup2(in_file, STDIN_FILENO);
				close(in_file);
				break;
			case REDIR_OUT:
				if (!strcmp(tokenizedInput[i], ">")) {  // Create a new file
					out_file = open(tokenizedInput[i+1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
				} else if (!strcmp(tokenizedInput[i], ">>"))  { // Append mode
					out_file = open(tokenizedInput[i+1], O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
				}
                errsv = errno;
				if (out_file < 0) {
					if (errsv == EACCES) {
						fprintf(stderr, "%s: Permission denied\n", tokenizedInput[i+1]);
					} else {
						fprintf(stderr, "%s: unknown error\n", tokenizedInput[i+1]);
					}
					return false;
				}
				dup2(out_file, STDOUT_FILENO);
				close(out_file);
				break;
			default:
				break;
		}
	}
	closePipesFD();
	return true;
}

void execCmd()
{
    char * args[128];
    int i;
    for (i = 0; i < numOfTokens - currentCmdPos + 1 && !(tokenType[currentCmdPos+i] == REDIR_IN || tokenType[currentCmdPos+i] == REDIR_OUT || tokenType[currentCmdPos+i] == PIPE); i++) {
        *(args + i) = tokenizedInput[currentCmdPos+i];
    }
    args[i] = NULL;
    execvp(tokenizedInput[currentCmdPos], args);
    errsv = errno;
    if (errsv == ENOENT) {
        fprintf(stderr, "%s: command not found\n", tokenizedInput[currentCmdPos]);
    } else {
        fprintf(stderr, "%s: unknown error\n", tokenizedInput[currentCmdPos]);
    }
}

void forkCmd()
{
    switch (pid[processCnt-1] = fork()) { // Fork error = -1, Child = 0, Parent > 0
        case -1:
            fprintf(stderr, "Fork is unsuccessful. Program exit.\n");
            exit(EXIT_FAILURE);
        case 0:
            setSignalBehavior(true);
            if (initIORedirection()) { // Handle IO redirection & pipe
				execCmd();  // Create Process of this cmd and supply it with arguments.
            }
			exit(EXIT_FAILURE); // Unsuccessful command execution
        default:
            break;
    }
}

void processInput()
{
	int i, status;
    processCnt = 0;
    for (i = 0; i < 3; i++) {
        pid[i] = 0;
    }
	pipe(pipes); // First pipe
	pipe(pipes + 2); // Second pipe
	curPipe = 0;
    while (nextCommand()) {
        if (tokenType[currentCmdPos] == BUILTIN || tokenType[currentCmdPos] == BUILTIN_WITH_ARG) {
            runBuiltIn();
        } else {
			processCnt++;
            forkCmd();  // Fork and execute the cmd
			if (processCnt > 1) { // More than one process means there are pipes
				curPipe += 2;   // Move to next pipe
			}
        }
    }
	closePipesFD();
	for (i = 0; i < processCnt; i++) {
        if (waitpid(pid[i], &status, WUNTRACED) != pid [i]) {
            fprintf(stderr, "Child process terminated unexpectedly. Program exit.\n");
            exit(EXIT_FAILURE);
        } else if (i == 0) {
            if (WIFSIGNALED(status)) { // Terminated due to signal ^C
                printf("\n");
            }
            if (WIFSTOPPED(status)) { // Stopped and can be resumed by ^Z or kill
                printf("\n");
                addJob(pid);
            }
        }
    }
}

int main(int argc, const char * argv[], const char * envp[])
{
    if (setenv("PATH", getenv("PATH"), 1) == -1) { // Successful = 0, Unsuccessful = -1
        fprintf(stderr, "setenv error. Program exit.\n");
        return EXIT_FAILURE;
    }
    setSignalBehavior(false);
    while (true) {
        printPrompt();
        if (readInput()) {
            continue;
        }
        if (tokenize()) {
            processInput();
        }
    }
    return EXIT_FAILURE;
}
