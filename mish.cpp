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

int pipefd[2];

int run_child_cmd(command cmd){
    int status = -1;
    int tmpFd;

    //input redirect
    if(strcmp(cmd.in_file, "")){
        tmpFd = open(cmd.in_file, O_RDONLY);
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
    else if(cmd.in_pipe){
        close(pipefd[1]);
        if(pipefd[0] == -1){
            perror("Failed to open read pipe\n");
            exit(1);
        }
        if(dup2(pipefd[0], STDIN_FILENO) == -1){
            perror("Failed to redirect stdin to read pipe\n");
            exit(1);
        }
        close(pipefd[0]);
    }
    else{
        tmpFd = dup(STDIN_FILENO);
        if(tmpFd == -1){
            perror("Failed to duplicate std in\n");
            exit(1);
        }

        pipefd[0] = dup(STDIN_FILENO);
        if(pipefd[0] == -1){
            perror("Failed to duplicate std in to read pipe\n");
            exit(1);
        }
    }

    //output redirect
    if(strcmp(cmd.out_file, "")){
        tmpFd = open(cmd.out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if(tmpFd == -1){
            perror("Failed to open output file\n");
            exit(1);
        }
        if(dup2(tmpFd, STDOUT_FILENO) != 1){
            perror("Failed to redirect stdout\n");
            exit(1);
        }
        close(tmpFd);
    }
    else if(cmd.out_pipe){
        close(pipefd[0]);
        if(pipefd[1] == -1){
            perror("failed to open write pipe\n");
            exit(0);
        }
        if(dup2(pipefd[1], STDOUT_FILENO) != 1){
            perror("Failed to redirect write to pipe\n");
            exit(1);
        }
        close(pipefd[1]);    
    }
    else{
        tmpFd = dup(STDOUT_FILENO);
        if(tmpFd == -1){
            perror("Failed to duplicate terminal file descriptor\n");
            exit(1);
        }

        pipefd[1] = dup(STDOUT_FILENO);;
        if(pipefd[1] == -1){
            perror("Failed to duplcate terminal file descriptor to write pipe\n");
            exit(1);
        }
    }

    status = execvp(cmd.cmd, cmd.args);
    perror("execvp failed\n");
    exit(1);
    return status;
}

vector<pid_t> run_commands(command cmd, vector<pid_t> parallel_pids){
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
        // if(cmd.parallel){
        //     while(cmd.parallel && cmd.next_cmd != nullptr){
        //         pid_t P_child = fork();
        //         if(P_child == -1){
        //             perror("Parallel fork failed\n");
        //             exit(1);
        //         }
        //         else if(P_child == 0){
        //             run_commands(cmd);
        //             exit(0);
        //         }
        //         cmd = *cmd.next_cmd;
        //     }

        //     while(wait(&status) > 0);
        // }
        // else{
            child = fork();
            if(child == -1){
                perror("forking error\n");
                exit(1);
            }
            else if (child == 0){
                cout << "[RUN]: running child cmd: " << cmd.cmd << endl;
                status = run_child_cmd(cmd);
                cout << "[RUN]: done running child cmd: " << cmd.cmd << endl;
            }
            else{
                if(cmd.parallel){
                    parallel_pids.push_back(child);
                    status = 1;
                }
                else{  
                    waitpid(child, &status, 0);
                }
            }
        //}        
    }
    if(status == -1){
        perror("failed some command\n");
        exit(1);
    }
    return parallel_pids;
}


void runUI(){
    string CL_in = "";
    while(CL_in != "exit"){
        vector<pid_t> run_parallel;
        //int pipefd[2];
        cout << "mish> ";
        getline(cin, CL_in);
        smatch match;
        command in_cmd(CL_in);
        bool run = true;
        while(run){
            if(in_cmd.out_pipe){
                if(pipe(pipefd) == -1){
                    perror("piping error\n");
                    exit(1);
                }
            }
            cout << "[RUN]: running command: " << in_cmd.cmd << endl;
            cout << "[RUN]: with arguments: " << endl;
            for(int i =0 ; i < in_cmd.argc + 1; i++){
                cout << in_cmd.args[i] << endl;
            }
            run_parallel = run_commands(in_cmd, run_parallel);
            cout << "[RUN]: finished running command: " << in_cmd.cmd << endl;
            if(in_cmd.out_pipe){
                close(pipefd[1]);
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
        
}

void runScript(char* script_file){
    cout << "running script" << endl;
    fstream inFile;
    inFile.open(script_file, ios::in);
    if(!inFile.is_open()){
        perror("Error loading script file");
        exit(1);
    }
    string CL_in = "";
    while(getline(inFile, CL_in)){
        vector<pid_t> run_parallel;
        //int pipefd[2];
        cout << "mish> ";
        smatch match;
        command in_cmd(CL_in);
        bool run = true;
        while(run){
            if(in_cmd.out_pipe){
                if(pipe(pipefd) == -1){
                    perror("piping error\n");
                    exit(1);
                }
            }
            cout << "[RUN]: running command: " << in_cmd.cmd << endl;
            cout << "[RUN]: with arguments: " << endl;
            for(int i =0 ; i < in_cmd.argc + 1; i++){
                cout << in_cmd.args[i] << endl;
            }
            run_parallel = run_commands(in_cmd, run_parallel);
            cout << "[RUN]: finished running command: " << in_cmd.cmd << endl;
            if(in_cmd.out_pipe){
                close(pipefd[1]);
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
    inFile.close();

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