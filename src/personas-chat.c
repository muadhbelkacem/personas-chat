#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_TEXT 4096
#define READ_BUF 4096
#define WRAP_WIDTH 80 // maximum characters per line in log file

#define RESET "\033[0m"
#define BOLD "\033[1m"
#define DIM "\033[2m"
#define CYAN_B "\033[96m"
#define GREEN_B "\033[92m"
#define BLUE_B "\033[94m"
#define YELLOW_B "\033[93m"
#define WHITE_B "\033[97m"
#define GRAY "\033[90m"

// --- Helper: Wrap text nicely when writing to log file ---
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
        // find how many chars fit in the remaining space
        const char *start = p;
        size_t count = 0;
        while (*p && *p != '\n' && count < WRAP_WIDTH - line_len)
        {
            p++;
            count++;
        }

        // if we reached end of line or newline
        if (*p == '\n')
        {
            fwrite(start, 1, p - start, log);
            fputc('\n', log);
            line_len = 0;
            p++;
            for (size_t i = 0; i < prefix_len; i++)
                fputc(' ', log); // indent continuation
            line_len = prefix_len;
            continue;
        }

        // if line too long, backtrack to last space
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

        // skip spaces
        while (*end == ' ')
            end++;
        p = end;

        // indent continuation
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
    printf("%s%s🤖 AI Duo Chat (Endless Streaming)%s\n", CYAN_B, BOLD, RESET);
    printf("%sTwo AI personas will keep talking to each other until you exit (Ctrl+C).%s\n", GRAY, RESET);
    printf("Press %sCtrl+C%s to stop.\n\n", YELLOW_B, RESET);

    char persona1[64] = "Muadh";
    char persona2[64] = "Ahmed";

    printf("%sPersona 1:%s %s\n", GREEN_B, RESET, persona1);
    printf("%sPersona 2:%s %s\n\n", BLUE_B, RESET, persona2);

    FILE *log = fopen("conversation.txt", "a");
    if (!log)
    {
        perror("conversation.txt");
        return 1;
    }

    fprintf(log, "Persona 1: %s\n", persona1);
    fprintf(log, "Persona 2: %s\n\n", persona2);
    fflush(log);

    char *msg = strdup("Hello, how are you today?");
    printf("%s%s%s:%s\n", BOLD, GREEN_B, persona1, RESET);
    printf("%s%s%s\n\n", WHITE_B, msg, RESET);

    char prefix[128];
    snprintf(prefix, sizeof(prefix), "%s: ", persona1);
    write_wrapped(log, prefix, msg);

    int i = 0;
    while (1)
    {
        const char *speaker = (i % 2 == 0) ? persona2 : persona1;
        const char *listener = (i % 2 == 0) ? persona1 : persona2;

        if (i % 2 == 0)
            printf("%s%s%s:\n", BOLD, BLUE_B, speaker);
        else
            printf("%s%s%s:\n", BOLD, GREEN_B, speaker);
        fflush(stdout);

        char prompt[MAX_TEXT];
        snprintf(prompt, sizeof(prompt),
                 "You are %s. Respond naturally to the following message from %s:\n\n\"%s\"\n",
                 speaker, listener, msg);

        free(msg);
        msg = ask_ollama(prompt);

        snprintf(prefix, sizeof(prefix), "%s: ", speaker);
        write_wrapped(log, prefix, msg);

        fflush(stdout);
        i++;
    }

    free(msg);
    fclose(log);
    printf("%sEnd of conversation.%s\n", DIM, RESET);
    return 0;
}
