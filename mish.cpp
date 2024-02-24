#include <iostream>
#include <vector>
#include <regex>
#include <unistd.h>
#include <sys/wait.h>
#include "./command_ll.cpp"
#include <map>
#include <fstream>
#include <fcntl.h>

using namespace std;

//int pipefd[2];

//function to run command after fork() has been called
int run_child_cmd(command cmd, int last_read_end){
    int status = -1; //status of the program
    int tmpFd; // used for file redirection

    //output redirect
    if(strcmp(cmd.out_file, "")){ //file output redirect
        tmpFd = open(cmd.out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666); //make sure that you create a new file if one doesnt exist
        if(tmpFd == -1){ //failed to open/create output file
            perror("Failed to open output file\n");
            //exit(1);
        }
        if(dup2(tmpFd, STDOUT_FILENO) != 1){ //redirect std out to the outfile
            perror("Failed to redirect stdout\n");
            //exit(1);
        }
        close(tmpFd);
    }
    else if(cmd.out_pipe){ //output redirect to pipe
        if(cmd.pipefd[1] == -1){ //failed to open write end of pipe
            perror("failed to open write pipe\n");
            exit(0);
        }
        else if(dup2(cmd.pipefd[1], STDOUT_FILENO) != 1){ //set std out to pipe and check to make sure that worked
            perror("Failed to redirect write to pipe\n");
            //exit(1);
        }
    }
    else{ //no output redirect
        tmpFd = dup(STDOUT_FILENO); //make sure to return default out to std out
        if(tmpFd == -1){ //unable to return to std out
            perror("Failed to duplicate terminal file descriptor\n");
            //exit(1);
        }

        cmd.pipefd[1] = dup(STDOUT_FILENO); //use std out rather than pipe for output
        if(cmd.pipefd[1] == -1){
            perror("Failed to duplcate terminal file descriptor to write pipe\n");
            //exit(1);
        }
    }

    //input redirect
    if(strcmp(cmd.in_file, "")){ //file input redirect
        tmpFd = open(cmd.in_file, O_RDONLY); //make sure that the file is read only
        if(tmpFd == -1){ //failed to open file
            perror("Failed to open input file\n");
            //exit(1);
        }
        if(dup2(tmpFd, STDIN_FILENO) == -1){ //failed to redirect input from file
            perror("Failed to redirect stdin\n");
            //exit(1);
        }
        close(tmpFd); 
    }
    else if(cmd.in_pipe){ //pipe input redirect
        if(last_read_end == -1){ //failed to open read end of pipe
            perror("Failed to open read pipe\n");
            //exit(1);
        }
        if(dup2(last_read_end, STDIN_FILENO) == -1){ //failed to redirect input from pipe
            perror("Failed to redirect stdin to read pipe\n");
            //exit(1);
        }
        close(last_read_end);
    }
    else{ //no input redirect
        tmpFd = dup(STDIN_FILENO); //make sure to redirect input to stdin
        if(tmpFd == -1){ //failed to make default read in, std in
            perror("Failed to duplicate std in\n");
            //exit(1);
        }

        last_read_end = dup(STDIN_FILENO); 
        if(last_read_end == -1){ 
            perror("Failed to duplicate std in to read pipe\n");
            //exit(1);
        }
    }

    status = execvp(cmd.cmd, cmd.args); //execute commands
    perror("execvp failed\n"); //process should close after executing commands, if it makes it to this line, there is an error
    exit(1);
    return status;
}

vector<pid_t> run_commands(command cmd, vector<pid_t> parallel_pids, int last_read_end){ //function to run parent process and fork 
    int status = -1; //status of the commands being executed
    pid_t child; //child process variable
    if(!strcmp(cmd.cmd, "cd")){ //cd commands need to be executed by the parent
        status = chdir(cmd.args[1]);
    }
    else if(!strcmp(cmd.cmd, "exit")){ //exit command also needs to be executed by the parent
        exit(0);
    }
    else if(cmd.change_env_var){ //changing environmental variables needs to be done by the parent
        char* env_var = (char *) calloc(strlen(cmd.cmd),sizeof(char)); // allocate space for the environmental variable temp variable (used by the setenv() function)
        strncpy(env_var, cmd.cmd, strlen(cmd.cmd) - 1); //copy name of env var you want to change
        setenv(env_var, cmd.args[1], 1); //set environmental variable
        //cout << env_var << ": " << getenv(env_var) << endl; //used for testing
        free(env_var); //free space allocated above
        status = 0; //status is OK
    }
    else{ //if not cd, exit, or setting env vars, must be regular bash command
        child = fork(); //make child process to run the bash command
        if(child == -1){ //error in forking
            perror("forking error\n");
            exit(1);
        }
        else if (child == 0){ //only child processes should execute bash commands
            //cout << "[RUN]: running child cmd: " << cmd.cmd << endl;
            status = run_child_cmd(cmd, last_read_end); //execute bash commands
            //cout << "[RUN]: done running child cmd: " << cmd.cmd << endl;
        }
        else{ 
            if(cmd.parallel){ //if parallel, let all parallel commands run before waiting
                parallel_pids.push_back(child); 
                status = 1;
            }
            else{ // if not parallel have parent wait on child process before continuting execution
                waitpid(child, &status, 0);
            }
        }     
    }
    if(status == -1){ //if process has an error, exit program
        perror("failed some command\n");
        //exit(1);
    }
    return parallel_pids; //vector for all the parallel child processes
}


void runUI(){ //if no input file provided, get input from user
    string CL_in = ""; //user input 
    while(CL_in != "exit"){ //run until crash or user types "exit" in terminal
        vector<pid_t> run_parallel; //vector for all parallel child processes
        cout << "mish> "; //prompt
        getline(cin, CL_in); // get data from terminal
        command in_cmd(CL_in); // make data structure for command that user wants executed
        bool run = true; 
        int last_read_end = -1;
        if(CL_in == ""){
            run = false;
        }
        while(run){ // keep running until all of the user commands have been executed
            if(in_cmd.out_pipe){ //if the command we are executing is writing to the pipe, open the pipe
                if(pipe(in_cmd.pipefd) == -1){ //error making the pipe
                    perror("piping error\n");
                    //exit(1);
                }
            }
            //cout << "[RUN]: running command: " << in_cmd.cmd << endl; //debugging
            //cout << "[RUN]: with arguments: " << endl; //debugging
            // for(int i =0 ; i < in_cmd.argc + 1; i++){ // more debugging
            //     cout << in_cmd.args[i] << endl;
            // }
            run_parallel = run_commands(in_cmd, run_parallel, last_read_end); //run all commands and get all parallel processes we need to wait on later
            //cout << "[RUN]: finished running command: " << in_cmd.cmd << endl; //debugging
            if(in_cmd.out_pipe){ //if bash command was writing to pipe, ensure that the write end is closed
                last_read_end = in_cmd.pipefd[0];
                close(in_cmd.pipefd[1]);
            }
            if(in_cmd.next_cmd == nullptr){ //if we get to end of command list, stop running 
                run = false;
            }
            else{ //if there are still bash commands to be executed, keep running 
                in_cmd = * in_cmd.next_cmd;
            }
        }
        if(in_cmd.parallel){ //if we needed to execute in parallel, we wait for all child processes to finish here
            for(pid_t i : run_parallel){
                int status;
                waitpid(i, &status, 0);
            }
        }
    }
        
}

void runScript(char* script_file){ //if script file provided, read from that file instead of terminal
    //cout << "running script" << endl; //debugging
    fstream inFile;
    inFile.open(script_file, ios::in); //open input file to read
    if(!inFile.is_open()){ //if unable to open the input file, send error message and close the program
        perror("Error loading script file"); 
        exit(1);
    }
    string CL_in = ""; //variable to keep track of the commands we want to execute
    while(getline(inFile, CL_in)){ // get data from file, line by line, same as reading from user, just from a file this time
        vector<pid_t> run_parallel;
        smatch match;
        command in_cmd(CL_in);
        bool run = true;
        int last_read_end = -1;
        if(CL_in == ""){
            run = false;
        }
        while(run){
            if(in_cmd.out_pipe){
                if(pipe(in_cmd.pipefd) == -1){
                    perror("piping error\n");
                    //exit(1);
                }
            }
            // cout << "[RUN]: running command: " << in_cmd.cmd << endl; //debugging
            // cout << "[RUN]: with arguments: " << endl;
            // for(int i =0 ; i < in_cmd.argc + 1; i++){
            //     cout << in_cmd.args[i] << endl;
            // }
            run_parallel = run_commands(in_cmd, run_parallel, last_read_end);
            //cout << "[RUN]: finished running command: " << in_cmd.cmd << endl; //debugging
            if(in_cmd.out_pipe){
                last_read_end = in_cmd.pipefd[0];
                close(in_cmd.pipefd[1]);
            }
            if(in_cmd.next_cmd == nullptr){
                run = false;
            }
            else{
                in_cmd = * in_cmd.next_cmd;
            }
        }
        if(in_cmd.parallel){
            for(pid_t i : run_parallel){
                int status;
                waitpid(i, &status, 0);
            }
        }
    }
    inFile.close(); //close the input file

}

int main(int argc, char* argv[]){ //main funtion where program starts
    if(argc == 1){ //if no file provided, get instructions from terminal and user
        runUI();
    }
    else if(argc == 2){ // if file provided, get instructions from input file provided
        runScript(argv[1]);
    }
    else{
        perror("please provide 0-1 input files");
        exit(1);
    }
    return 0;
}