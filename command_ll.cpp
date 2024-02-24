#include <vector>
#include <cstdlib>
#include <regex>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <string>

using namespace std;

const regex STRING("[a-zA-Z]+", regex_constants::ECMAScript); //regex to search for bash commands
const regex CD_DIR("[!./a-zA-Z/0-9_-]+", regex_constants::ECMAScript); // regex to search for directories in command 
const regex ENV_VAR("[a-zA-Z_]+=", regex_constants::ECMAScript); // regex to search for environmental variable change requests
const regex NEW_PATH("[/][a-zA-Z0-9/:_. ]+", regex_constants::ECMAScript); // regex to search for a file path to use 
const regex OUT_REDIRECT(">[ ]*[a-zA-Z0-9./_]+", regex_constants::ECMAScript); // regex to search for output redirection to files
const regex OUT_REDIRECT_ERR(">", regex_constants::ECMAScript); // regex to search for improper use of the output redirection command
const regex IN_REDIRECT("<[ ]*[a-zA-Z0-9._]+", regex_constants::ECMAScript); // regex to search for input redirection to files
const regex IN_REDIRECT_ERR("<", regex_constants::ECMAScript); // regex to search for improper use of the input redirection command
const regex FILE_ADDR("[./]*[a-zA-Z0-9./_]+", regex_constants::ECMAScript); // regex to search for a new file address
const regex PARALLEL("[a-zA-Z0-9./_ <>:|-]+&", regex_constants::ECMAScript); // regex to search if bash command should be run in parallel with another bash command
const regex PIPED("[a-zA-Z0-9./_ <>:-]+[|]", regex_constants::ECMAScript); // regex to search for if a bash command should be piped into another bash command

struct command{ // structure for holding instructions to execute
    public:
        char* cmd; //bash command to execute
        char** args; // arguments for command 
        int argc; // argument count 
        vector<string> arg_vec; // vector to easily store arguments before being refactored into a less dynamic data structure
        char* in_file; // potental input file to read from
        char* out_file; // potential output file to write to
        bool parallel; // true if command should be run in parallel, false if else
        bool change_env_var; // true if user is attempting to change an environmental variable, false if not
        bool out_pipe; // true if command is writing to a pipe, false if else
        bool in_pipe; // true if commmand is reading from a pipe, false if else
        int pipefd[2];
        command* next_cmd; // pointer to the next command 
        command(std::string in_str); // constructor
        ~command(); // destructor
        void init_arg_arry(); // function to convert the argument vector to a char array array to be used by the execvp() command
};

string trim(string in_str){ //function used to trim whitespace from a string
    size_t start = 0; //start of string 
    size_t end = in_str.length(); // end of string

    while(start < end && isspace(in_str[start])){ //get location of first non-whitespace character
        ++start;
    }

    while(end > start && std::isspace(in_str[end-1])){ //get location of last non-whitespace character
        --end;
    }

    return in_str.substr(start, end-start); // trim whitespace
}

command::command(string in_str){ //constructor
    //set default values for all variables in struct
    parallel = false; 
    pipefd[2] = {};
    argc = 0; 
    args = {NULL}; // default, no arguments, but execvp() requires any argument list to be ended with a NULL character
    smatch match; // varible for regex matching
    cmd = new char[0];
    change_env_var = false;
    out_file = new char[0];
    in_file = new char[0];
    char * temp = new char[0]; //temp char array for temporary data storage
    parallel = false;
    in_pipe = false;
    out_pipe = false;
    next_cmd = nullptr;
    arg_vec = {};
    //cout << "\n[NEW COMMAND]: in_str: " << in_str << endl; //debugging

    if(regex_search(in_str, match, PARALLEL)){ //first search if the command is going to be run in parallel 
        // cout << "[PARSING]: found parallel at idx: " << match.position() << endl; //debugging
        // cout << "[PARSING]: match size: " << match.str().size() << endl;
        // cout << "[PARSING]: in_str size: " << trim(in_str).size() << endl;
        if(trim(match.str()).size() == trim(in_str).length()){ //if parallel character not sandwiched btween two commands, ignore (real bash would have this run in the background but im not that smart)
            //cout << "[PARSING]: RUN IN BACKGROUND" << endl; //debugging
            in_str.resize(trim(in_str).size() - 1); // resize the instruction to exclude the tailing '&'
           // cout << "new in_str: " << in_str << endl; //debugging
        }
        else{ // if the '&' is between two commands, they need to be marked to be run in parallel
            parallel = true; // first command marked to be run in parallel
            strncpy(temp, match.str().c_str(), match.str().size()-1); // set first command aside for now
            in_str = in_str.substr(match.str().size(), in_str.size()); 
            next_cmd = new command(in_str); // continue parsing the rest of the instruction, but add it as a new instruction to be executed after the current
            (*next_cmd).parallel = true; // mark the next command to be run in parallel
            //cout << "out of making child command" << endl; //debugging
            string temp_str(temp); //temp string to reset instruction so we can continue parsing this first command
            in_str = temp_str.substr(0, temp_str.size()-1); // ^
            //cout << "[PARALLEL]: cmd: {" << in_str << "}" << endl; //debugging
            //cout << "[PARALLEL]: par_cmd: {" << next_cmd->cmd << "}" << endl; 
        }
    }

    if(regex_search(in_str, match, PIPED)){ // search if the command needs to pipe to another command
        //cout << "[PARSING]: found Piped" << endl; // debugging
        strncpy(temp, match.str().c_str(), match.str().size()-1); // set write command aside for now
        in_str = trim(in_str.substr(match.str().size(), in_str.size())); // get read command
        if(next_cmd == nullptr){ // if there is no other command as the next command 
            next_cmd = new command(in_str); //set next command to be the piped command
        }
        else{ //if there is another command as the next command (from the parallel parsing)
            command* temp_next = next_cmd; // temp store ptr to parallel command(s)
            next_cmd = new command(in_str); // make piped command next on list 
            
            //search for end of next command, where the next command can go
            command* search = next_cmd; 
            while(search != nullptr){
                search = next_cmd->next_cmd;
            }
            search = temp_next; //set parallel command at end of piped commands
        }
        out_pipe = true; // make sure this is the command that is writing
        next_cmd->in_pipe = true; // make next command read in from pipe
        string temp_str(temp); // set to keep parsing
        in_str = temp_str.substr(0, temp_str.size()-1);
        //cout << "[PIPED]: cmd: {" << in_str << "}" << endl; //debugging
        //cout << "[PIPED]: pipe_cmd: {" << next_cmd->cmd << "}" << endl; 
    }

    //first look if cmd is setting new env var
    if(regex_search(in_str, match, ENV_VAR)){
        //cout << "[PARSING]: setting env var" << endl; //debugging 
        change_env_var = true; // this command is trying to set a new value for an environemental variable
        strcpy(cmd, match.str().c_str()); // get env var name that user is trying to set
        in_str = in_str.substr(match.str().size()); // get arguments/ rest of instruction back into string format for further parsing
        if(regex_search(in_str, match, NEW_PATH)){ // if a new path is found (for PATH) 
            strcpy(temp, match.str().c_str()); //set it as an arg
            arg_vec.push_back(temp);
        }
        else{ // if not a path throw error
            perror("bad path");
            exit(0);
        }

    }

    else{ // Once done parsing out parallelization, pipes and env var declarations, all rest should be bash commands
        regex_search(in_str, match, STRING); // search for command
        strcpy(cmd, match.str().c_str()); // set cmd as command char array
        //cout << "[COMMAND]: command found: " << cmd << endl; // debugging
        if(match.str().size() != in_str.size()){ // remove command from string to parse so rest should be args
            in_str = in_str.substr(match.str().size() + 1, in_str.size()); 
        }
        else{ // if no args, set string to parse as empty so it skips the other parsing 
            in_str = "";
        }
        if(regex_search(in_str, match, OUT_REDIRECT)){ // if an output file redirect is found
            //cout << "[OUT REDIRECT]: out redirect found: " << match.str() << endl; // debugging
            in_str.replace(match.position(), match.str().size(), ""); // clear file and deliminator so we can continue to parse
            string old_match = match.str(); 
            regex_search(old_match, match, FILE_ADDR); // parse the file from the matching substring
            strcpy(out_file, match.str().c_str()); //set the out_file var to the output file name
            //cout << "[OUT REDIRECT]: redirect out to: " << out_file << endl; //debugging
        }
        else if(regex_search(in_str, match, OUT_REDIRECT_ERR)){ // if incorrect usage of the output redirect deliminator
            perror("please provide output file"); // throw error
            exit(1);
        }
        else{ // if no output file, clear out_file var
            strcpy(out_file, "");
        }
        if(regex_search(in_str, match, IN_REDIRECT)){ //if an input file redirect is found
            //cout << "[IN REDIRECT]: in redirect found" << endl; // debugging
            in_str.replace(match.position(), match.str().size(), ""); // same procedure as output file parsing
            string old_match = match.str();
            regex_search(old_match, match, FILE_ADDR);
            strcpy(in_file, match.str().c_str());
            //cout << "[IN REDIRECT]: redirect in to: " << in_file << endl; // debugging
        }
        else if(regex_search(in_str, match, IN_REDIRECT_ERR)){ // if incorrect usage of the input redirect deliminator
            perror("please provide input file"); // throw error
            exit(1);
        }
        else{ //if no input file redirection, clear in_file var
            strcpy(in_file, "");
        }


        while(regex_search(in_str, match, CD_DIR)){ // get all the arguments that havent gotten parsed out yet; they should all be legal and if not the execvp will throw an error
            strcpy(temp, match.str().c_str()); // get the argument in a char array
            //cout << "[PARSING]: arguments found: " << temp << endl; // debugging
            arg_vec.push_back(temp); // add it to the dynamic argument list
            //cout << "arg_vec size: " << arg_vec.size() << endl; // debugging
            //cout << "arg_vec.back(): " << arg_vec.back() << endl;
            in_str.erase(match.position(), match.str().size()); // clear the argument from the remaining string to be parsed
            //cout << "[ARGUMENTS]: " << temp << endl; // debugging
            //cout << "[ARGUMENTS]: in_str after removing temp: " << in_str << endl;
            //cout << "arg_vec.back(): " << arg_vec.back() << endl;
        }
        //cout << "[ARGUMENTS]: arg_vec: " << endl; // debugging
        // for(int i = 0; i < int(arg_vec.size()); i++){
        //     cout << "   " <<  arg_vec[i] << endl;
        // }
        in_str = ""; // make sure to clear any whitespace just in case
    }
 
    init_arg_arry(); // convert dynamic argument list to a static one
}

command::~command(){ // destructor
}

void command::init_arg_arry(){ // function used to convert a dynamic argument list in the form of a vector to a char* array
    args = new char*[arg_vec.size() + 2]; // allocate the space needed to store the arguments
    args[0] = cmd; // set first argument as the command
    //cout << "args[0]: " << cmd << endl; // debugging
    int i = 0; 
    for(i = 0; i < int(arg_vec.size()); ++i){ // for all the arguments in the dynamic array
        //cout << "arg_vec.at(" << i << "): " << arg_vec.at(i) << endl; // debugging
        args[i+1] = new char[arg_vec[i].size()]; // add argument to the static array 
        strcpy(args[i+1], arg_vec[i].c_str()); // make sure that its a copy of the data and not a pointer to the data to keep arguments correct
        //cout << "args[" << i+1 << "]: " << args[i+1] << endl; // debugging
    }
    args[i + 1] = NULL; // last argument of the static argument array needed by execvp needs to be NULL
    argc = i; // make sure the argument count is accurate

}