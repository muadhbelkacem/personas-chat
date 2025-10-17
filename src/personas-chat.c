#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_TEXT 4096
#define READ_BUF 4096
#define WRAP_WIDTH 80

#define RESET "\033[0m"
#define BOLD "\033[1m"
#define DIM "\033[2m"
#define CYAN_B "\033[96m"
#define GREEN_B "\033[92m"
#define BLUE_B "\033[94m"
#define YELLOW_B "\033[93m"
#define WHITE_B "\033[97m"
#define GRAY "\033[90m"

void write_wrapped(FILE *log, const char *prefix, const char *text)
{
    if (!log || !text)
        return;
    size_t prefix_len = strlen(prefix);
    fprintf(log, "%s", prefix);
    size_t line_len = prefix_len;
    const char *p = text;
    while (*p)
    {
        const char *start = p;
        size_t count = 0;
        while (*p && *p != '\n' && count < WRAP_WIDTH - line_len)
        {
            p++;
            count++;
        }
        if (*p == '\n')
        {
            fwrite(start, 1, p - start, log);
            fputc('\n', log);
            line_len = 0;
            p++;
            for (size_t i = 0; i < prefix_len; i++)
                fputc(' ', log);
            line_len = prefix_len;
            continue;
        }
        const char *end = p;
        if (*p && *p != '\n' && *p != '\0')
        {
            const char *back = p;
            while (back > start && *back != ' ')
                back--;
            if (back > start)
                end = back;
        }
        fwrite(start, 1, end - start, log);
        fputc('\n', log);
        while (*end == ' ')
            end++;
        p = end;
        for (size_t i = 0; i < prefix_len; i++)
            fputc(' ', log);
        line_len = prefix_len;
    }
    fputc('\n', log);
    fputc('\n', log);
    fflush(log);
}

char *ask_ollama(const char *prompt)
{
    int inpipe[2], outpipe[2];
    if (pipe(inpipe) == -1 || pipe(outpipe) == -1)
    {
        perror("pipe");
        return NULL;
    }
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return NULL;
    }
    if (pid == 0)
    {
        dup2(inpipe[0], STDIN_FILENO);
        dup2(outpipe[1], STDOUT_FILENO);
        close(inpipe[0]);
        close(inpipe[1]);
        close(outpipe[0]);
        close(outpipe[1]);
        execlp("ollama", "ollama", "run", "gemma3", (char *)NULL);
        perror("execlp");
        _exit(127);
    }
    close(inpipe[0]);
    close(outpipe[1]);
    write(inpipe[1], prompt, strlen(prompt));
    close(inpipe[1]);
    char buf[READ_BUF];
    ssize_t r;
    size_t total = 0;
    char *output = NULL;
    printf("%s", WHITE_B);
    fflush(stdout);
    while ((r = read(outpipe[0], buf, sizeof(buf))) > 0)
    {
        fwrite(buf, 1, r, stdout);
        fflush(stdout);
        output = realloc(output, total + r + 1);
        memcpy(output + total, buf, r);
        total += r;
    }
    printf("%s\n", RESET);
    fflush(stdout);
    if (output)
        output[total] = '\0';
    close(outpipe[0]);
    waitpid(pid, NULL, 0);
    if (!output)
        return strdup("(no response)");
    char *start = output;
    while (*start && strchr("\n\r \t", *start))
        start++;
    char *end = output + total - 1;
    while (end > start && strchr("\n\r \t", *end))
        *end-- = '\0';
    return start == output ? output : strdup(start);
}

int main(void)
{
    printf("%s%s🤖 AI Duo Chat%s\n", CYAN_B, BOLD, RESET);
    printf("%sYou can always exit anytime using Ctrl+C.%s\n\n", GRAY, RESET);

    char persona1[64] = "Muadh";
    char persona2[64] = "Ahmed";

    printf("%sPersona 1:%s %s\n", GREEN_B, RESET, persona1);
    printf("%sPersona 2:%s %s\n\n", BLUE_B, RESET, persona2);

    int ask_every_time = 1;
    char choice[8];
    printf("%sDo you want me to ask after every reply if you want to exit? (Y/n): %s", YELLOW_B, RESET);
    fflush(stdout);

    if (fgets(choice, sizeof(choice), stdin))
    {
        if (choice[0] == 'n' || choice[0] == 'N')
            ask_every_time = 0;
    }

    char input[MAX_TEXT];
    printf("\n%sEnter the initial message to start the conversation:%s\n> ", YELLOW_B, RESET);
    fflush(stdout);
    if (!fgets(input, sizeof(input), stdin))
    {
        fprintf(stderr, "Error reading input.\n");
        return 1;
    }
    input[strcspn(input, "\n")] = 0;
    if (strlen(input) == 0)
        strcpy(input, "Let's start a deep and continuous discussion — no greetings needed.");

    FILE *log = fopen("conversation.txt", "a");
    if (!log)
    {
        perror("conversation.txt");
        return 1;
    }

    fprintf(log, "Persona 1: %s\nPersona 2: %s\n\n", persona1, persona2);
    fflush(log);

    char *msg = strdup(input);
    printf("%s%s%s:%s\n", BOLD, GREEN_B, persona1, RESET);
    printf("%s%s%s\n\n", WHITE_B, msg, RESET);

    char prefix[128];
    snprintf(prefix, sizeof(prefix), "%s: ", persona1);
    write_wrapped(log, prefix, msg);

    char *conversation = NULL;
    size_t convo_len = asprintf(&conversation, "%s: %s\n", persona1, msg);

    int i = 0;
    char exit_choice[8];

    while (1)
    {
        if (i > 0 && ask_every_time)
        {
            printf("%sDo you want to exit? (Y/n): %s", YELLOW_B, RESET);
            fflush(stdout);
            if (fgets(exit_choice, sizeof(exit_choice), stdin))
            {
                exit_choice[strcspn(exit_choice, "\n")] = 0;
                if (exit_choice[0] == 'y' || exit_choice[0] == 'Y' || exit_choice[0] == '\0')
                {
                    printf("%sExiting conversation...%s\n", DIM, RESET);
                    break;
                }
            }
        }

        const char *speaker = (i % 2 == 0) ? persona2 : persona1;
        const char *listener = (i % 2 == 0) ? persona1 : persona2;

        printf("%s%s%s:%s\n", BOLD, (i % 2 == 0) ? BLUE_B : GREEN_B, speaker, RESET);
        fflush(stdout);

        char prompt[MAX_TEXT];
        snprintf(prompt, sizeof(prompt),
                 "You are %s. Continue this ongoing discussion with %s. "
                 "Do not greet, introduce yourself, or repeat ideas. "
                 "Stay natural and continue mid-discussion.\n\n"
                 "Conversation so far:\n%s\n",
                 speaker, listener, conversation);

        free(msg);
        msg = ask_ollama(prompt);

        snprintf(prefix, sizeof(prefix), "%s: ", speaker);
        write_wrapped(log, prefix, msg);

        size_t new_len = convo_len + strlen(speaker) + strlen(msg) + 4;
        conversation = realloc(conversation, new_len);
        convo_len += sprintf(conversation + convo_len, "%s: %s\n", speaker, msg);

        fflush(stdout);
        i++;
    }

    free(msg);
    free(conversation);
    fclose(log);
    printf("%sEnd of conversation.%s\n", DIM, RESET);
    return 0;
}