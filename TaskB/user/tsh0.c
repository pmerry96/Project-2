#include "kernel/types.h"
#include "user/user.h"
#include "user/tsh.h"
#include "kernel/fcntl.h"

#define YES     1
#define NO      0
#define EOF     -1

static char *default_prompt = "tsh> ";

typedef struct BufferedLine {
    int len;                                // number of items (chars) in the buffer
    char buffer[TSH_MAX_CMD_LINE_LENGTH];   // buffer to hold the data
} BufferedLine;

typedef enum TokenType {
    TOKEN_INVALID,
    TOKEN_WORD,
    TOKEN_PIPE,
    TOKEN_LIST,
    TOKEN_REDIRECT_OUTPUT,
    TOKEN_REDIRECT_OUTPUT_APPEND,
    TOKEN_REDIRECT_INPUT,
    TOKEN_BACKGROUND,
    TOKEN_GROUP_START,
    TOKEN_GROUP_END,
} TokenType;

typedef struct Token {
    TokenType type;
    char *value;
} Token;

typedef struct TokenList {
    int len;
    Token tokens[TSH_MAX_NUM_TOKENS];
} TokenList;

enum RedirectionType {
    REDIRECT_INPUT,
    REDIRECT_OUTPUT,
    REDIRECT_APPEND,
    REDIRECT_NONE
};

// A Redirection data structure
typedef struct Redirection {
    int source_fd;              // redirect which the file represented by file descriptor
    int dest_fd;                // the file descriptor of the destination file, use only when >= 0
    char *path;                 // the path of the destination file, use only when NOT NULL
    enum RedirectionType type;  // The type of the redirected file
} Redirection;

enum CommandType {
    CMD_INVALID,
    CMD_EMPTY,
    CMD_SIMPLE,
    CMD_PIPELINE,
    CMD_LIST
};

struct SimpleCommand;
struct ListCommand;
struct Pipeline;

#define CMD_OK                                  0x00
#define CMD_SYNTAX_ERROR                        0x01
#define CMD_TOO_MANY_ARGUMENTS                  0x02
#define CMD_MISSING_REDIRECTION_DESTINATION     0x04
#define CMD_ARGUMENT_AFTER_REDIRECT             0x08
#define CMD_PIPELINE_TOO_LONG                   0x0f
#define CMD_TOO_MANY_COMMANDS                   0x10
#define CMD_UNKNOWN_TYPE                        0x20

#define CMD_BACKGROUND_MODE                     0x010000

//so Ive noticed a problem(feature) around this being a type-self referential struct
//IE
//Command thecmd
//thecmd.cmd.pipeline.commands[i].cmd.pipeline.commands[i]...
typedef struct Command { //im still stumped in how to extract the name of the command from this struct
    int type;
    int flag;
    union { //this means that the data in the union's mem locs is shared between the simple and pipeline structs
        struct SimpleCommand *simple;
        struct Pipeline *pipeline;
    } cmd; //as i learned about unions Im wondering why a union was used? seems to complicate things
} Command;

typedef struct SimpleCommand {
    enum CommandType type;
    int flag;
    char *name;
    int argc;
    char *argv[TSH_MAX_NUM_ARGUMENTS + 1];
    Redirection redirects[3];
} SimpleCommand;


typedef struct Pipeline {
    enum CommandType type;
    int flag;
    int len;
    Command commands[TSH_MAX_PIPELINE_LENGTH];
} Pipeline;

// The following three members stores private data
struct CommandData {
    int n_simples;
    SimpleCommand _simples[TSH_MAX_CMD_LIST_LENGTH];
    Pipeline _pipeline;
    Command cmd;
};

void
PrintCommand(Command *cmd, char *indent) {
    if (cmd->type == CMD_SIMPLE) {
        struct SimpleCommand *simple = cmd->cmd.simple;
        printf("%sSimple: cmd=%s\n", indent, simple->name);
        for (int i = 0; i < simple->argc; i++)
            printf("%s\targv[%d]=%s\n", indent, i, simple->argv[i]);
        for (int i = 0; i < 3; i++) {
            Redirection *r = simple->redirects + i;
            if (r->type != REDIRECT_NONE) {
                printf("%s\tREDIRECT: type=%d fd=%d path=%s\n", indent, r->type, r->source_fd, r->path);
            }
        }
        int has_error = (simple->flag & 0xffff);
        printf("%s\tFlags=%x (%s)\n",indent,  simple->flag, (has_error ? "Error" : "OK"));
        printf("\n");
    } else if (cmd->type == CMD_PIPELINE) {
        Pipeline *pipeline = cmd->cmd.pipeline;
        printf("Pipeline: len=%d flag=%x\n", pipeline->len, pipeline->flag);
        for (int i = 0; i < pipeline->len; i++) {
            if (i > 0) printf("\t|\n");
            PrintCommand(&(pipeline->commands[i]), "\t");
        }
        printf("\tEND;\n");
    } else {
        Debug("Unknown command type (%d)", cmd->type);
    }
}

Token *
find_next_toke_with_type(int token_type, Token *tokens, Token *tail) {
    Token *p = tokens;
    while (p < tail) {
        if (p->type == token_type) {    /* found */
            return p;
        }
        p++;
    }
    return tail;    /* not found */
}

int
ParseSimpleCommand(Token *start, Token *tail, SimpleCommand *simple) {
    simple->type = CMD_EMPTY; //there is no simple command yet
    Token *p = start;

    // get the command path
    if (p < tail && p->type == TOKEN_WORD) { //does this correspond with path in exec()?
        simple->type = CMD_SIMPLE;
        simple->name = p->value;
        simple->flag = 0;   // clear flags
    }

    // get the arguments
    int i = 0;
    while ( (p < tail) && (p->type == TOKEN_WORD) && i < (TSH_MAX_NUM_ARGUMENTS)) {
        simple->argv[i] = p->value;
        p++;
        i++;
    }
    simple->argv[i] = 0;
    simple->argc = i;
    if (i > TSH_MAX_NUM_ARGUMENTS) {
        simple->flag |= CMD_TOO_MANY_ARGUMENTS;
        ErrorU("too many arguments");
        return simple->flag;
    }

    // get the redirection(s), if there is any
    for (int i = 0; i < 3; i++) {
        simple->redirects[i].type = REDIRECT_NONE;
        simple->redirects[i].source_fd = i;
        simple->redirects[i].dest_fd = -1;
        simple->redirects[i].path = 0;
    }

    while (p < tail) {
        Debug("type=%d vale=%s", p->type, p->value);
        int fd = 1;
        if (p->type == TOKEN_REDIRECT_INPUT) {
            fd = 0;
            simple->redirects[fd].source_fd = 0;
            simple->redirects[fd].type = REDIRECT_INPUT;
        } else if (p->type == TOKEN_REDIRECT_OUTPUT) {
            fd = 1;
            simple->redirects[fd].source_fd = 1;
            simple->redirects[fd].type = REDIRECT_OUTPUT;
        } else if (p->type == TOKEN_REDIRECT_OUTPUT_APPEND) {
            fd = 1;
            simple->redirects[fd].source_fd = 1;
            simple->redirects[fd].type = REDIRECT_APPEND;
        } else {
            if (p->type == TOKEN_BACKGROUND) {
                simple->flag |= CMD_BACKGROUND_MODE;
                p += 1;
                continue;
            } else {
                simple->flag |= CMD_SYNTAX_ERROR;
                ErrorU("Syntax error - unexpected token type");
                p += 1;
                continue;
            }
        }

        Token *q = p + 1;
        if (q < tail && q->type == TOKEN_WORD)
            simple->redirects[fd].path = q->value;
        else {
            simple->flag |= CMD_MISSING_REDIRECTION_DESTINATION;
            ErrorU("Missing redirection destination");
            return simple->flag;
        }
        p += 2;
    }

    if (p != tail) {
        simple->flag |= CMD_ARGUMENT_AFTER_REDIRECT | CMD_SYNTAX_ERROR;
        ErrorU("Syntax error - argument after redirection");
        return simple->flag;
    }
    return simple->flag;
}

static void init_pipeline(Pipeline *pipeline) {
    pipeline->type = CMD_PIPELINE;
    pipeline->flag = 0;
    pipeline->len = 0;
}

static void set_command(Command *command, enum CommandType type, void *concrete_command) {
    command->type = type;
    if (type == CMD_PIPELINE) {
        Pipeline *pipeline = (Pipeline *)concrete_command;
        command->cmd.pipeline = pipeline;
        command->flag = pipeline->flag;
    } else if (type == CMD_SIMPLE) {
        SimpleCommand *simple = (SimpleCommand *)concrete_command;
        command->cmd.simple = simple;
        command->flag = simple->flag;
    } else {
        command->type = CMD_INVALID;
        command->flag = CMD_UNKNOWN_TYPE;
    }
}

void
ParsePipeline(Token *head, Token *tail, struct CommandData *cmd_data) {
    Token *prev, *curr;
    Command *command = &(cmd_data->cmd);

    curr = prev = head;  //and tail and curr all start the same, prob zero
    Pipeline *pipeline = &(cmd_data->_pipeline); //take the command data struct passed in and pull out the pipeline element

    //while the current pointer is less than tail
    //  and
    //the pipeline length is less than max
    //  and
    // the cmd_data->n_simples is less than the max length of a list of commands
    while (curr < tail && pipeline->len < TSH_MAX_PIPELINE_LENGTH && cmd_data->n_simples < TSH_MAX_CMD_LIST_LENGTH) {
        //curr becomes the index of the next token;
        curr = find_next_toke_with_type(TOKEN_PIPE, prev, tail);
        if (curr < tail) { //if curr is not at the end of the string
            if (pipeline->len == 0) { //if the pipeline length is zero
                init_pipeline(pipeline); //initialize the pipeline
                if (command->type != CMD_LIST) {
                    set_command(command, CMD_PIPELINE, pipeline);
                }
            }

            SimpleCommand *simple = &cmd_data->_simples[cmd_data->n_simples++];
            ParseSimpleCommand(prev, curr, simple);

            set_command(&(pipeline->commands[pipeline->len]), CMD_SIMPLE, simple);
            pipeline->len++;

            curr++;
            prev = curr;
        }
    }

    if (cmd_data->n_simples >= TSH_MAX_CMD_LIST_LENGTH) { /* check too many command error */
        pipeline->flag |= CMD_TOO_MANY_COMMANDS;
        command->flag |= pipeline->flag;
        ErrorU("Too many commands");
        return;
    }
    SimpleCommand *simple = &cmd_data->_simples[cmd_data->n_simples++];
    ParseSimpleCommand(prev, tail, simple);
    if (pipeline->len == 0) {   /* simple command */
        set_command(command, CMD_SIMPLE, simple);
    }

    if (pipeline->len >= TSH_MAX_PIPELINE_LENGTH) { /* check pipeline too long error*/
        pipeline->flag |= CMD_PIPELINE_TOO_LONG;
        command->flag |= pipeline->flag;
        ErrorU("Pipeline too long");
        return;
    }

    if (pipeline->len > 0) { /* last pipeline element */
        set_command(&(pipeline->commands[pipeline->len]), CMD_SIMPLE, simple);
        pipeline->len++;
    }
    return;
}

void
ParseCommand(Token *head, Token *tail, struct CommandData *cmd_data) {
    ParsePipeline(head, tail, cmd_data);
}

typedef struct ShellState {
    int should_run;
    int last_exit_status;
    char *prompt;
    BufferedLine cmdline;
    TokenList tokens;
    Command *cmd;
    char *name;

    struct CommandData _cmd_data;
} ShellState;

ShellState this_shell;

int
ReadLine(BufferedLine *line) {
    char *p, c;
    int nleft;

    p = line->buffer;
    nleft = sizeof(line->buffer);

    while (nleft > 0) {
        if (read(1, &c, sizeof(c)) != 1)    // no data to read, end-of-file or error
            break;
        if (c == '\n' || c == EOF)          // stop after seeing a new line
            break;
        *p++ = c;
        nleft--;
    }
    line->len = sizeof(line->buffer) - nleft; // update the number of chars in the buffer
    line->buffer[line->len] = '\0';            // end the buffer with '\0'

    return line->len;
}

void ClearLine(BufferedLine *line) {      // file the buffer with 0
    char *p = line->buffer;
    char *tail = line->buffer + sizeof(line->buffer);
    while (p < tail) *p++ = '\0';
    line->len = 0;
}

static char whitespace[] = " \t\r\n\v";
static char symbols[] = "<|>&;";

void Tokenize(BufferedLine *line, TokenList *tl) {
    char *p = line->buffer;
    char *tail = line->buffer + strlen(line->buffer);
    int idx = 0;
    char next_char = '\0';

    while (p < tail && idx < sizeof(tl->tokens)) {
        while (p < tail && strchr(whitespace, *p)) // skip leading whitespace
            p++;
        if (p == tail) break;

        next_char = *p;
        if (!strchr(symbols, next_char)) { // find a word token
            tl->tokens[idx].type = TOKEN_WORD;
            tl->tokens[idx].value = p;
            idx++;

            while (p < tail && !strchr(whitespace, *p) && !strchr(symbols, *p))
                p++;
            next_char = *p;
            *p = '\0';
        }

        if (strchr(whitespace, next_char)) {
            p++;
            continue;
        }

        if (strchr(symbols, next_char)) {  // a symbol token
            if (next_char == '>' && *(p + 1) == '>') { // Handle the case of ">>"
                tl->tokens[idx].type = TOKEN_REDIRECT_OUTPUT_APPEND;
                tl->tokens[idx].value = p;
                p += 2;
                idx++;
                continue;
            }

            switch (next_char) { // set correct token type
                case '<':
                    tl->tokens[idx].type = TOKEN_REDIRECT_INPUT;
                    break;
                case '>':
                    tl->tokens[idx].type = TOKEN_REDIRECT_OUTPUT;
                    break;
                case '|':
                    tl->tokens[idx].type = TOKEN_PIPE;
                    break;
                case '&':
                    tl->tokens[idx].type = TOKEN_BACKGROUND;
                    break;
                case ';':
                    tl->tokens[idx].type = TOKEN_LIST;
                    break;
                default:
                    tl->tokens[idx].type = TOKEN_INVALID;
            }
            tl->tokens[idx].value = p;
            p++;
            idx++;
            continue;
        }
    }
    tl->len = idx;
}

void
PrintTokenList(TokenList *tl) {
    printf("Tokens\n");
    for (int i = 0; i < tl->len; i++) {
        Token *t = tl->tokens + i;
        if (t->type == TOKEN_WORD)
            printf("\tToken[%d]: Type=%d Value=%s Len=%d\n", i, t->type, t->value, strlen(t->value));
        else
            printf("\tToken[%d]: Type=%d\n", i, t->type);
    }
    printf("\n");
}

int
GetCommand(ShellState *shell) {
    //write to fd 1 the contents of shell->prompt for shell->prompt chars
    write(1, shell->prompt, strlen(shell->prompt));
    //eat the command line into stdin
    ClearLine(&shell->cmdline);
    //parse the line into a command
    ReadLine(&shell->cmdline);
    //grab the tokens that were in the command
    Tokenize(&shell->cmdline, &shell->tokens);
    //PrintTokenList(&shell->tokens);
    struct TokenList *tl = &(shell->tokens);

    shell->_cmd_data.n_simples = 0;
    shell->_cmd_data._pipeline.len = 0;
    ParseCommand(tl->tokens, tl->tokens + tl->len, &shell->_cmd_data);
    shell->cmd = &(shell->_cmd_data.cmd);

    //PrintCommand(shell->cmd, "");
    return 0;
}

int runSimpleCommand(SimpleCommand *cmd) {
    //RESOLVED_TODO - simple commands seem to be working after testing
    //tested commands include ls, wc, cat, echo, etc
    if(cmd->flag != 0x00) //is flag 0x00 indicative of no error?
    {
        //return some sort of error, possibly errno
    }else{
        int pid = fork();
        if(pid == 0)
        {
            exec(cmd->argv[0], cmd->argv); //this might be too high level, but im not sure what itd want.
            //so this will make it to exec, but im worried that I will not be able to redirect output.
            /*
             * soln: pass a flag "ispiped", that denotes whether it should pass output to stdin or stdout
             */
            close(pid);
        }else{
            //in parent context
            wait(&pid); //pass it the pid to wait on
            close(pid);
        }
    }
    return 0;
}


/*
 * supposing that a pipeline command consists of one or more pipes in a command line, let us take for instance the command
 * > history | grep ls | wc //a command to count how many calls to ls we make
 *
 * this will then be broken down into two parts
 *
 *          history | grep ls | wc
 *          ^^^^^^^   ^^^^^^^^^^^^
 *       Simple CMD | subsequent (possibly) pipeline command
 *
 *                    grep ls | wc
 *                    ^^^^^^^ | ^^
 *                 simple cmd | subsequent (possibly) pipeline command
 *
 *                              wc
 *                              ^^
 *                              simple command to stdout
 *
 * So looking at the above example, the process can be abstracted into the following algorithm
 * 1 - recieve command (that may or may not be pipelined
 * 2 - make a pass through the command, detecting the presence of a "|" or not
 *      2a - if a pipe exists:
 *          2ai   - "break" string immediately before the first pipeline, this promises the "first" of the substrings are then going to be a simple command
 *                  *2ai - note we will not be writing this output to STDOUT, but rather to STDIN such that the piped cmd can "grab" it
 *          2aii  - with the now "peeled away" pipeline command, pass the latter part from the break back into the runPipelineCommand() call
 *                  *2aii - if there is any output expected from this, it will be directed to STDOUT in the runSimpleCommand() call
 *          2aiii - return to step one now considering hte "peeled away" pipeline command
 *
 *      2b - if no pipe exists, pass the cmd to runSimpleCommand()
 */

void
panic(char *s) //included simply to give a gracefull terminating function in case of error
{
    fprintf(2, "%s\n", s);
    exit(1);
}
// pipeline.commands[].simple.argv is the simple command
// pipeline.commands[:] is the array of simple commands, accessed similarly to above
//your problem is in redirecting the input
int runPipelineCommnad(Pipeline *pipeline) {//nice typo there @ author.
    // DONE: the below (active) code is not functioning. A pipeline command still executes the first command but does not redirect the output nor does it execute the subsequent commands
    // TODO: fix the below code to no longer execute the first single command, but all commands in sequence

    //The below code came from the following:
    // ~/user/sc.h
    //      in function 'runcmd(struct cmd)
    //             ...
    //              switch(cmd->type)
    //                  ...
    //                  case PIPE:
    //                      BEGIN COPIED PORTION ***
    //                      ...
    //                      END COPIED PORTION   ***
    //                  break;
    //              ...
    //          ...
    //      ...
    //  ...
    int p[2];
    pipe(p);
    if(fork() == 0){
        close(p[1]);
        dup(p[1]); //sending stdout to the pipe rather than the cmd line
        //close(p[1]);
        printf("len = %d\n", pipeline->len);
        char* buf[256];
        while(read(pfd[0], buff, 256))
        {
        	write(0, buf, 256); //write to stdin
        }
        SimpleCommand* simplecmd;
	    for(int i = 0; i <= pipeline->len; i++)
	    {
	    	printf("i = %d\nls", i);
	    	if(i == pipeline->len)
		    {
	    		close(p[1]);
		    }
	    	simplecmd = pipeline->commands[i].cmd.simple;
	    	printf("pipeline->commands[i].cmd.simplecmd->argv = %s", simplecmd->argv);
	    	for(int j = 0; j < simplecmd->argc; j++)
		        printf("exec simplecmd->argv[ %d ] = %s\n", i, simplecmd->argv[j]);
	    	exec(simplecmd->argv[0], simplecmd->argv);
		    pipeline = pipeline->commands[i].cmd.pipeline;
	    }
    }else{
    	//what does the parent process do
    }
    /*
    if(fork() == 0){
        close(0); //this is closing this process? -> close takes an int file descriptor, child context is 0, thus this is closing itself
        dup(p[0]);
	    for(int i = 0; i <= pipeline->len; i++)
	    {
		    if(i == pipeline->len - 1)
		    {
			    close(p[1]);
		    }
		    SimpleCommand* simplecmd = pipeline->commands[i].cmd.simple;
		    printf("exec simplecmd->argv[ %d ] = %s\n", i, simplecmd->argv[i]);
		    exec(simplecmd->argv[0], simplecmd->argv);
		    pipeline = pipeline->commands[i].cmd.pipeline;
	    }
	    close(p[0]);
	    close(p[1]);
    }
     */
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    return(0);
}

int
RunCommand(ShellState *shell) {
    Command *command = shell->cmd;
    if (command->type == CMD_EMPTY) {
        return 1;

    } else if (command->type == CMD_INVALID) {
        ErrorU("invalid command");
        return -1;

    } else if (command->type == CMD_SIMPLE) {
        SimpleCommand *cmd = command->cmd.simple;
        if (strcmp(cmd->name, "quit") == 0) {
            shell->should_run = NO;
            if (cmd->argc > 1) {
                shell->last_exit_status = atoi(cmd->argv[1]);
            }
            return 0;
        } else if (strcmp(cmd->name, "exit") == 0) { //these are  "built in commands"
            shell->should_run = NO;
        } else if (strcmp(cmd->name, "cd") == 0) {
            //REOLVED_TODO: implement case where we call a directory
            if(!(strcmp(cmd->argv[1], "..") == 0 || strcmp(cmd->argv[1], ".") == 0)){ //thus we are accessing either this dir or a previous dir
                if (chdir(cmd->argv[1]) != 0) {
                    printf("Error calling directory named %s\n", cmd->argv[1]);
                }
            }else{
                if(strcmp(cmd->argv[1], "..") == 0)
                {
                    chdir("..");
                    printf("current directory = %s\n", shell->name);
                }else{
                    //self referential call to cd, no change in dirs necessary.
                    printf("Already in directory referenced by '.'\n");

                    //chdir(".");
                }
            }
        } else {
            runSimpleCommand(cmd);
        }
    } else if (command->type == CMD_PIPELINE) {
        Pipeline *pipeline = command->cmd.pipeline;
        runPipelineCommnad(pipeline); //here we can expect the command to be a full pipeline, I believe though that this enters into a sort of recursive state (more below)
        /*
         * supposing that a pipeline command consists of one or more pipes in a command line, let us take for instance the command
         * > history | grep ls | wc //a command to count how many calls to ls we make
         *
         * this will then be broken down into two parts
         *
         *          history | grep ls | wc
         *          ^^^^^^^   ^^^^^^^^^^^^
         *       Simple CMD | subsequent (possibly) pipeline command
         *
         *                   grep ls | wc
         *                   ^^^^^^^ | ^^
         *                simple cmd | subsequent (possibly) pipeline command
         *
         *                              wc
         *                              ^^
         *                              simple command
         *
         * So looking at the above example, the process can be abstracted into the following algorithm
         * 1 - recieve command (that may or may not be pipelined
         * 2 - make a pass through the command, detecting the presence of a "|" or not
         *      2a - if a pipe exists:
         *          2ai   - "break" string immediately before the first pipeline, this promises the "first" of the substrings are then going to be a simple command
         *                  *2ai - note we will not be writing this output to STDOUT, but rather to STDIN such that the piped cmd can "grab" it
         *          2aii  - with the now "peeled away" pipeline command, pass the latter part from the break back into the runPipelineCommand() call
         *                  *2aii - if there is any output expected from this, it will be directed to STDOUT in the runSimpleCommand() call
         *          2aiii - return to step one now considering hte "peeled away" pipeline command
         *
         *      2b - if no pipe exists, pass the cmd to runSimpleCommand()
         */
    }
    return 0;
}


int
main(int argc, char *argv[]) {

    this_shell.prompt = default_prompt;
    this_shell.name = "tsh";
    this_shell.should_run = 1;
    this_shell.cmd = &(this_shell._cmd_data.cmd);

    while (this_shell.should_run) {
        GetCommand(&this_shell);
        RunCommand(&this_shell);
    }

    //printf("%s (%d) was terminated\n", this_shell.name, getpid());
    exit(this_shell.last_exit_status);
}
