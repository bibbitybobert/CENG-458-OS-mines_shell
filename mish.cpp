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

int run_child_cmd(command cmd){
    int status = -1;
    //output redirect
    if(strcmp(cmd.out_file, "")){
        int tmpFd = open(cmd.out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if(tmpFd == -1){
            perror("Failed to open output file\n");
            exit(1);
        }
        if(dup2(tmpFd, 1) != 1){
            perror("Failed to redirect stdout\n");
            exit(1);
        }
        close(tmpFd);
    }
    else{
        int terminalFD = dup(STDOUT_FILENO);
        if(terminalFD == -1){
            perror("Failed to duplicate terminal file descriptor\n");
            exit(1);
        }
    }

    //input redirect
    if(strcmp(cmd.in_file, "")){
        int tmpFd = open(cmd.in_file, O_RDONLY);
        if(tmpFd == -1){
            perror("Failed to open input file\n");
            exit(1);
        }
        if(dup2(tmpFd, STDIN_FILENO) == -1){
            perror("Failed to redirect stdin\n");
            exit(1);
        }
        close(tmpFd);
    }
    else{

    }
    status = execvp(cmd.cmd, cmd.args);
    perror("execvp failed\n");
    exit(1);
    return status;
}

void run_commands(command cmd){
    int status = -1;
    pid_t child;
    if(!strcmp(cmd.cmd, "cd")){
        status = chdir(cmd.args[1]);
    }
    else if(!strcmp(cmd.cmd, "exit")){
        exit(0);
    }
    else if(cmd.change_env_var){
        char* env_var = (char *) calloc(strlen(cmd.cmd),sizeof(char));
        strncpy(env_var, cmd.cmd, strlen(cmd.cmd) - 1);
        setenv(env_var, cmd.args[1], 1);
        cout << env_var << ": " << getenv(env_var) << endl;
        free(env_var);
        status = 0;
    }
    else{
        if(cmd.parallel){
            child = fork();
            if(child == -1){
                perror("fork failed\n");
                exit(1);
            }
            else if(child == 0){
                cout << "run_commands" << endl;
                run_commands(*cmd.parallel_cmd);
                exit(0);
            }
            run_child_cmd(cmd);
            waitpid(child, &status, 0);
        }
        else{
            child = fork();
            if(child == 0){ 
                status = run_child_cmd(cmd);
            }
            else{
                waitpid(child, &status, 0);
            }
        }
        
    }
    if(status == -1){
        perror("failed some command\n");
        exit(1);
    }

}


void runUI(){
    string CL_in = "";
    while(CL_in != "exit"){
        cout << "mish> ";
        getline(cin, CL_in);
        smatch match;
        command in_cmd(CL_in);
        do{ //need to figure out how to make parallel instructions run in parallel
            run_commands(in_cmd);
        }while(in_cmd.parallel_cmd != nullptr);
    }
        
}

void runScript(char* script_file){
    cout << "RUNNING SCRIPT: " << script_file << endl;

}

int main(int argc, char* argv[]){
    if(argc == 1){
        runUI();
    }
    else if(argc == 2){
        runScript(argv[1]);
    }
    return 0;
}