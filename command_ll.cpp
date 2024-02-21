#include <vector>
#include <cstdlib>
#include <regex>
#include <iostream>

using namespace std;

const regex STRING("[a-zA-Z]+", regex_constants::ECMAScript);
const regex CD_DIR("[./a-zA-Z/0-9_]+", regex_constants::ECMAScript);
const regex ENV_VAR("[a-zA-Z]+=", regex_constants::ECMAScript);
const regex NEW_PATH("[/][a-zA-Z0-9/:. ]+", regex_constants::ECMAScript);
const regex OUT_REDIRECT(">[ ]*[a-zA-Z0-9./]+", regex_constants::ECMAScript);
const regex IN_REDIRECT("<[ ]*[a-zA-Z0-9.]+", regex_constants::ECMAScript);
const regex FILE_ADDR("[./]*[a-zA-Z0-9./]+", regex_constants::ECMAScript);

struct command{
    public:
        char* cmd;
        char** args;
        int argc;
        vector<char*> arg_vec;
        char* in_file;
        char* out_file;
        bool parallel;
        bool change_env_var;
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

    //first look if cmd is setting new env var
    if(regex_search(in_str, match, ENV_VAR)){
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
        in_str = in_str.substr(match.str().size());
        if(regex_search(in_str, match, OUT_REDIRECT)){
            in_str.replace(match.position(), match.str().size(), "");
            string old_match = match.str();
            regex_search(old_match, match, FILE_ADDR);
            strcpy(out_file, match.str().c_str());
        }
        else{
            strcpy(out_file, "");
        }
        if(regex_search(in_str, match, IN_REDIRECT)){
            in_str.replace(match.position(), match.str().size(), "");
            string old_match = match.str();
            regex_search(old_match, match, FILE_ADDR);
            strcpy(in_file, match.str().c_str());
        }
        else{
            strcpy(in_file, "");
        }
        while(regex_search(in_str, match, CD_DIR)){
            strcpy(temp, match.str().c_str());
            arg_vec.push_back(temp);
            trim(in_str.replace(match.position(), match.str().size(), ""));
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
    int i = 1;
    for(i = 0; i < int(arg_vec.size()); i++){
        args[i + 1] = arg_vec.at(i);
    }
    args[i + 1] = NULL;
    argc = i;

}