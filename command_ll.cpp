#include <vector>
#include <cstdlib>
#include <regex>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <string>

using namespace std;

const regex STRING("[a-zA-Z]+", regex_constants::ECMAScript);
const regex CD_DIR("[!./a-zA-Z/0-9_-]+", regex_constants::ECMAScript);
const regex ENV_VAR("[a-zA-Z_]+=", regex_constants::ECMAScript);
const regex NEW_PATH("[/][a-zA-Z0-9/:_. ]+", regex_constants::ECMAScript);
const regex OUT_REDIRECT(">[ ]*[a-zA-Z0-9./_]+", regex_constants::ECMAScript);
const regex OUT_REDIRECT_ERR(">", regex_constants::ECMAScript);
const regex IN_REDIRECT("<[ ]*[a-zA-Z0-9._]+", regex_constants::ECMAScript);
const regex IN_REDIRECT_ERR("<", regex_constants::ECMAScript);
const regex FILE_ADDR("[./]*[a-zA-Z0-9./_]+", regex_constants::ECMAScript);
const regex PARALLEL("[a-zA-Z0-9./_ <>:|-]+&", regex_constants::ECMAScript);
const regex PIPED("[a-zA-Z0-9./_ <>:-]+[|]", regex_constants::ECMAScript);

struct command{
    public:
        char* cmd;
        char** args;
        int argc;
        vector<string> arg_vec;
        char* in_file;
        char* out_file;
        bool parallel;
        bool change_env_var;
        int pipefd[2];
        bool out_pipe;
        bool in_pipe;
        command* next_cmd;
        command(std::string in_str);
        ~command();
        void init_arg_arry();
};

string trim(string in_str){
    size_t start = 0;
    size_t end = in_str.length();

    while(start < end && isspace(in_str[start])){
        ++start;
    }

    while(end > start && std::isspace(in_str[end-1])){
        --end;
    }

    return in_str.substr(start, end-start);
}

command::command(string in_str){
    parallel = false;
    argc = 0;
    args = {NULL};
    smatch match;
    cmd = new char[0];
    change_env_var = false;
    out_file = new char[0];
    in_file = new char[0];
    char * temp = new char[0];
    parallel = false;
    in_pipe = false;
    out_pipe = false;
    next_cmd = nullptr;
    arg_vec = {};
    cout << "\n[NEW COMMAND]: in_str: " << in_str << endl;

    if(regex_search(in_str, match, PARALLEL)){
        cout << "[PARSING]: found parallel at idx: " << match.position() << endl;
        cout << "[PARSING]: match size: " << match.str().size() << endl;
        cout << "[PARSING]: in_str size: " << trim(in_str).size() << endl;
        if(trim(match.str()).size() == trim(in_str).length()){
            cout << "[PARSING]: RUN IN BACKGROUND" << endl;
            in_str.resize(trim(in_str).size() - 1);
            cout << "new in_str: " << in_str << endl;
        }
        else{
            parallel = true;
            strncpy(temp, match.str().c_str(), match.str().size()-1);
            in_str = in_str.substr(match.str().size(), in_str.size());
            next_cmd = new command(in_str);
            (*next_cmd).parallel = true;
            cout << "out of making child command" << endl;
            string temp_str(temp);
            in_str = temp_str.substr(0, temp_str.size()-1);
            cout << "[PARALLEL]: cmd: {" << in_str << "}" << endl;
            cout << "[PARALLEL]: par_cmd: {" << next_cmd->cmd << "}" << endl; 
        }
    }

    if(regex_search(in_str, match, PIPED)){
        cout << "[PARSING]: found Piped" << endl;
        strncpy(temp, match.str().c_str(), match.str().size()-1);
        in_str = trim(in_str.substr(match.str().size(), in_str.size()));
        next_cmd = new command(in_str);
        out_pipe = true;
        next_cmd->in_pipe = true;
        cout << "here" << endl;
        string temp_str(temp);
        in_str = temp_str.substr(0, temp_str.size()-1);
        cout << "[PIPED]: cmd: {" << in_str << "}" << endl;
        cout << "[PIPED]: pipe_cmd: {" << next_cmd->cmd << "}" << endl; 
    }

    //first look if cmd is setting new env var
    if(regex_search(in_str, match, ENV_VAR)){
        //cout << "[PARSING]: setting env var" << endl;
        change_env_var = true;
        strcpy(cmd, match.str().c_str());
        in_str = in_str.substr(match.str().size());
        if(regex_search(in_str, match, NEW_PATH)){
            strcpy(temp, match.str().c_str());
            arg_vec.push_back(temp);
        }
        else{
            perror("bad path");
            exit(0);
        }

    }

    else{
        regex_search(in_str, match, STRING);
        strcpy(cmd, match.str().c_str());
        cout << "[COMMAND]: command found: " << cmd << endl;
        if(match.str().size() != in_str.size()){
            in_str = in_str.substr(match.str().size() + 1, in_str.size());
        }
        else{
            in_str = "";
        }
        if(regex_search(in_str, match, OUT_REDIRECT)){
            cout << "[OUT REDIRECT]: out redirect found: " << match.str() << endl;
            in_str.replace(match.position(), match.str().size(), "");
            string old_match = match.str();
            regex_search(old_match, match, FILE_ADDR);
            strcpy(out_file, match.str().c_str());
            cout << "[OUT REDIRECT]: redirect out to: " << out_file << endl;
        }
        else if(regex_search(in_str, match, OUT_REDIRECT_ERR)){
            perror("please provide output file");
            exit(1);
        }
        else{
            strcpy(out_file, "");
        }
        if(regex_search(in_str, match, IN_REDIRECT)){
            cout << "[IN REDIRECT]: in redirect found" << endl; 
            in_str.replace(match.position(), match.str().size(), "");
            string old_match = match.str();
            regex_search(old_match, match, FILE_ADDR);
            strcpy(in_file, match.str().c_str());
            cout << "[IN REDIRECT]: redirect in to: " << in_file << endl;
        }
        else if(regex_search(in_str, match, IN_REDIRECT_ERR)){
            perror("please provide input file");
            exit(1);
        }
        else{
            strcpy(in_file, "");
        }


        while(regex_search(in_str, match, CD_DIR)){
            strcpy(temp, match.str().c_str());
            cout << "[PARSING]: arguments found: " << temp << endl;
            arg_vec.push_back(temp);
            cout << "arg_vec size: " << arg_vec.size() << endl;
            cout << "arg_vec.back(): " << arg_vec.back() << endl;
            in_str.erase(match.position(), match.str().size());
            cout << "[ARGUMENTS]: " << temp << endl;
            cout << "[ARGUMENTS]: in_str after removing temp: " << in_str << endl;
            cout << "arg_vec.back(): " << arg_vec.back() << endl;
        }
        cout << "[ARGUMENTS]: arg_vec: " << endl;
        for(int i = 0; i < int(arg_vec.size()); i++){
            cout << "   " <<  arg_vec[i] << endl;
        }
        in_str = "";
    }

    init_arg_arry();
}

command::~command(){
}

void command::init_arg_arry(){
    args = new char*[arg_vec.size() + 2];
    args[0] = cmd;
    cout << "args[0]: " << cmd << endl;
    int i = 0;
    for(i = 0; i < int(arg_vec.size()); ++i){
        cout << "arg_vec.at(" << i << "): " << arg_vec.at(i) << endl;
        args[i+1] = new char[arg_vec[i].size()];
        strcpy(args[i+1], arg_vec[i].c_str());
        cout << "args[" << i+1 << "]: " << args[i+1] << endl;
    }
    args[i + 1] = NULL;
    argc = i;

}