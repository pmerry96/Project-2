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

#define CMD_SYNTAX_ERROR                        0x01
#define CMD_TOO_MANY_ARGUMENTS                  0x02
#define CMD_MISSING_REDIRECTION_DESTINATION     0x04
#define CMD_ARGUMENT_AFTER_REDIRECT             0x08
#define CMD_PIPELINE_TOO_LONG                   0x0f
#define CMD_TOO_MANY_COMMANDS                   0x10
#define CMD_UNKNOWN_TYPE                        0x20

#define CMD_BACKGROUND_MODE                     0x010000

typedef struct Command {
    int type;
    int flag;
    union {
        struct SimpleCommand *simple;
        struct Pipeline *pipeline;
    } cmd;
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
    simple->type = CMD_EMPTY;
    Token *p = start;

    // get the command path
    if (p < tail && p->type == TOKEN_WORD) {
        simple->type = CMD_SIMPLE;
        simple->name = p->value;
        simple->flag = 0;   // clear flags
    }

    // get the arguments
    int i = 0;
    while (p < tail && p->type == TOKEN_WORD && i < TSH_MAX_NUM_ARGUMENTS) {
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

    curr = prev = head;
    Pipeline *pipeline = &(cmd_data->_pipeline);
    while (curr < tail && pipeline->len < TSH_MAX_PIPELINE_LENGTH && cmd_data->n_simples < TSH_MAX_CMD_LIST_LENGTH) {
        curr = find_next_toke_with_type(TOKEN_PIPE, prev, tail);
        if (curr < tail) {
            if (pipeline->len == 0) {
                init_pipeline(pipeline);
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
    write(1, shell->prompt, strlen(shell->prompt));
    ClearLine(&shell->cmdline);
    ReadLine(&shell->cmdline);
    Tokenize(&shell->cmdline, &shell->tokens);
    PrintTokenList(&shell->tokens);
    struct TokenList *tl = &(shell->tokens);

    shell->_cmd_data.n_simples = 0;
    shell->_cmd_data._pipeline.len = 0;
    ParseCommand(tl->tokens, tl->tokens + tl->len, &shell->_cmd_data);
    shell->cmd = &(shell->_cmd_data.cmd);

    PrintCommand(shell->cmd, "");
    return 0;
}


int runPipelineCommnad(Pipeline *pipeline) {
    // TODO
    return 0;
}

int runSimpleCommand(SimpleCommand *cmd) {
    //TODO
    return 0;
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
        } else if (strcmp(cmd->name, "exit") == 0) {
            //TODO
        } else if (strcmp(cmd->name, "cd") == 0) {
            //TODO
        } else {
            runSimpleCommand(cmd);
        }
    } else if (command->type == CMD_PIPELINE) {
        Pipeline *pipeline = command->cmd.pipeline;
        runPipelineCommnad(pipeline);
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

    printf("%s (%d) was terminated\n", this_shell.name, getpid());
    exit(this_shell.last_exit_status);
}
