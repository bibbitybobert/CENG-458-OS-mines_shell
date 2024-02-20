#include <iostream>
#include <vector>
#include <regex>
#include <unistd.h>
#include <sys/wait.h>
#include "./command_ll.cpp"
#include <map>

using namespace std;

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
        cout << "ARGS: " << cmd.args[1] << endl;
        cout << env_var << ": " << getenv(env_var) << endl;
        free(env_var);
        status = 0;
    }
    else{
        child = fork();
        if(child == 0){ 
            status = execvp(cmd.cmd, cmd.args);
        }
        else{
            wait(NULL);
            status = 0;
        }
    }
    if(status == -1){
        cout << "ERROR" << endl;
        exit(1);
    }

}


void runUI(){
    string CL_in = "";
    while(CL_in != "exit"){
        cout << "mish> ";
        getline(cin, CL_in);
        command in_cmd(CL_in);
        run_commands(in_cmd);
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