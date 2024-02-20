#include <vector>
#include <cstdlib>
#include <regex>
#include <iostream>

using namespace std;

const regex STRING("[a-zA-Z]+", regex_constants::ECMAScript);
const regex CD_DIR("[./]*[a-zA-Z/0-9_]*", regex_constants::ECMAScript);
const regex ENV_VAR("[a-zA-Z]+=", regex_constants::ECMAScript);

struct command{
    public:
        char* cmd;
        char** args;
        int argc;
        vector<char*> arg_vec;
        string in_file;
        string out_file;
        bool parallel;
        bool change_env_var;
        command(std::string in_str);
        ~command();
        void init_arg_arry();
};

command::command(string in_str){
    parallel = false;
    argc = 0;
    args = {NULL};
    smatch match;
    cmd = new char[0];
    change_env_var = false;
    char * temp;
    //first look if cmd is setting new env var
    if(regex_search(in_str, match, ENV_VAR)){
        change_env_var = true;
        strcpy(cmd, match.str().c_str());
        in_str = in_str.substr(match.str().size());
        temp = new char[0];
        strcpy(temp, match.str().c_str());
        arg_vec.push_back(temp);

    }
    else{
        regex_search(in_str, match, STRING);
        strcpy(cmd, match.str().c_str());
        while(in_str.size() != match.str().size()){
            in_str = in_str.substr(match.str().size() + 1);
            if(strcmp(cmd, "cd") == 0){
                regex_search(in_str, match, CD_DIR);
                temp = new char[0];
                strcpy(temp, match.str().c_str());
                arg_vec.push_back(temp);
            }
        }
        in_str = "";
    }

    init_arg_arry();
}

command::~command(){
    //free(cmd);
    //free(args);
}

void command::init_arg_arry(){
    args = new char*[arg_vec.size() + 2];
    args[0] = cmd;
    int i = 1;
    for(i = 0; i < int(arg_vec.size()); i++){
        args[i + 1] = arg_vec.at(i);
    }
    args[i + 1] = NULL;
    argc = i;

}