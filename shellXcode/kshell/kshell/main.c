/*
 * ====== POS =======
 * Projekt c. 3 - shell
 * Jakub Vones | xvones02
 *
 * */

/* makra */
#define _XOPEN_SOURCE 500
#define _XOPEN_SOURCE_EXTENDED 1
#define _POSIX_C_SOURCE 199506L
#ifndef _REENTRANT
#define _REENTRANT
#endif

#define DEBUG 0
#define PROMPT "$ "
#define BUFFER_MAX_LENGTH 512
#define PARSE_CHARS " \n\t"
#define MAZAT_DOKONCENE 1

/* hlavicky */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>

/* stavove hlasky */
typedef enum Status{
    OK = 0,
    ERR_SET_SIG_INT,
    ERR_SET_SIG_CHLD,
    ERR_MUTEX,
    ERR_COND,
    ERR_THREAD,
    ERR_MALLOC_ADD_LIST,
    ERR_BUFFER_MAX_LENGTH,
    ERR_PARSE_AMPERSAND,
    ERR_PARSE_SPECIAL_CHARS,
    ERR_PARSE_FILE,
    ERR_FORK,
    ERR_CMD_BG,
    ERR_INPUT_FILE,
    ERR_OUTPUT_FILE,
    ERR_MALLOC,
    ERR_CMD
} Num;

const char *msg_err[] = {
    "",
    "Chyba sigaction: sig int\n",
    "Chyba sigaction: sig chld\n",
    "Chyba inicializace mutexu\n",
    "Chyba inicializace cond\n",
    "Chyba pri vytvareni vlakna\n",
    "Chyba alokovani dat pri pridavani prvku do seznamu\n",
    "Chyba: maximalni delka vstupu byla prekrocena\n",
    "Chyba syntaxe: za ampersandem nesmi nasledovat dalsi text bez mezery\n",
    "Chyba syntaxe: specialni znaky &,<,> musi byt v prikazu at na poslednich mistech\n",
    "Chyba syntaxe: vstupni/vystupni soubor byl spatne zadan\n",
    "Chyba forku\n",
    "Chyba prikazu na pozadi\n",
    "Chyba otevreni souboru input\n",
    "Chyba otevreni souboru output\n",
    "Chyba alokovani prostoru\n",
    "Chyba prikazu"
};

/* napoveda programu */
const char *HELP_MSG    = "Program: Shell\n"
"Autor: Jakub Vones | xvones02\n\n"
"Pouziti: ./shell \n"
"    - spusteni libovolneho program s libovolnymi parametry\n"
"    - pri spusteni se bere v uvahu promenna PATH (nastavena pred spustenim shellu)\n"
"    - spusteni na pozadi pomoci &\n"
"    - presmerovani vstupu na vystup pomoci <a>\n"
"    - po spusteni shell vypise prompt a bude cekat na vstup pomoci read()\n"
"    - shell pouziva dve vlakna, prvni pro ziskani vstupu a druhe pro spousteni prikazu"
"Priklad pouziti: \n"
"    $ ls -l -a\n"
"    $ sleep 10m &\n"
"    $ echo karel >/tmp/test\n"
"\n"
;

/* struktury */
typedef struct pthreads{
    pthread_mutex_t mutex;
    pthread_cond_t cond;

} PTHREADS, *TPTHREADS;

typedef struct proces{
    pid_t pid;
    bool  dokoncen;
    bool  zobrazen;
    int cislo_procesu;

    struct proces *next;
    struct proces *prev;
} PROCCESS, *TPROCCESS;

typedef struct procesy{
    pid_t aktualni;
    int pocet_spustenych;

    TPROCCESS First;
    TPROCCESS Act;
    TPROCCESS Last;
} tList;

typedef struct buffer{
    char read_buffer[BUFFER_MAX_LENGTH + 1];    /* +1 pro konec retezce */
    int  length;
    bool reading;

} BUFFER, *TBUFFER;

typedef struct command{
    int argc;
    char **argv;
    char *input;
    char *output;
    bool na_pozadi;

} COMMAND, *TCOMMAND;

/* globalni promenne */
PTHREADS mtx;        /* mutex */
tList      list;        /* seznam */
BUFFER     bfr;        /* buffer */
bool      end = false;    /* koncovy stav */

/* predpisy funkce */
void *spousteniPrikazu();
int  start_command_on_background(COMMAND *c);
int  start_command(COMMAND *c);
void iofiles(int *input, int *output, COMMAND *c);
int  get_command(COMMAND *c);
void initCommand(COMMAND *c);
void freeCommand(COMMAND *c);
int  cteniVstupu();
int  readStdin();
void showPrompt();
void await(bool reading);
void advance(bool with_signal);
int  initPthreads(PTHREADS *mtx, pthread_t *thread);
void freePthreads(PTHREADS *mtx, pthread_t *thread);
int  initSig(struct sigaction *sig_int, struct sigaction *sig_chld);
void handlerSigInt(int sig);
void handlerSigChld(int sig, siginfo_t *info, void *contxt);
void Error(Num n);
void writeError(Num n);
void writeText(char *text);
void initSeznam(tList *L);
void printSeznam(tList *L);
void printDokonceno(tList *L);
void printfStartProcess(pid_t pid);
void disposeSeznam(tList *L);
void setDokonceno(tList *L, pid_t pid, bool dokoncen);
int  sInsertLast(tList *L, pid_t pid, bool dokoncen);
int  sDelete(tList *L, pid_t pid);

/*
 * ==== Hlavni FCE MAIN
 * */
int main(int argc, char *argv[]){
    int error;
    struct sigaction sig_int;
    struct sigaction sig_chld;
    pthread_t thread;

    /* init */
    bfr.reading = true;
    list.aktualni = -1;
    list.pocet_spustenych = 0;
    initSeznam(&list);

    /* kontrola parametru */
    if(argc != 1){
        printf("%s", HELP_MSG);
        return EXIT_FAILURE;
    }

    /* inicializace sig */
    if((error = initSig(&sig_int, &sig_chld)) != OK){
        Error(error);
        return EXIT_FAILURE;
    }

    /* inicializace pthreads */
    if((error = initPthreads(&mtx, &thread)) != OK){
        Error(error);
        return EXIT_FAILURE;
    }

    /* cteni vstupu */
    if((error = cteniVstupu()) != OK){
        Error(error);
        freePthreads(&mtx, &thread);
        return EXIT_FAILURE;
    }

    /* uvolneni pthreads*/
    freePthreads(&mtx, &thread);
    disposeSeznam(&list);

    /* end */
    if(DEBUG){
        printf("------------------\nvse v poradku\n");
    }
    return EXIT_SUCCESS;
}

/*
 * ==== vlakno pro spousteni prikazu = shell
 * */
void *spousteniPrikazu(void *t){
    int status;
    COMMAND command;

    while(!end){
        /* vstup do KS */
        await(false);

        /* inicializace prikazu */
        initCommand(&command);

        /* obsluha prikazove radky */
        if(DEBUG) printf("shell: delka vstupu: %d-%d\n'%s'\n", (int)strlen(bfr.read_buffer), bfr.length, bfr.read_buffer);

        /* ukonceni shellu */
        if(strcmp("exit", bfr.read_buffer) == 0){
            end = true;
        }

        /* zpracovani prikazu */
        else if(bfr.length > 0){
            status = get_command(&command);

            /* prikaz byl spatne zadan*/
            if(status != OK){
                writeError((Num)status);
            }

            /* spusteni prikazu */
            else if(command.argc > 0){
                if(strcmp("exit", command.argv[0]) == 0){
                    end = true;
                }
                else{
                    /* spusteni */
                    status = command.na_pozadi ? start_command_on_background(&command) : start_command(&command);
                    if(status != OK){
                        writeError((Num)status);
                    }
                    else{}
                }
            }
        }

        /* uvolneni prikazu */
        freeCommand(&command);

        /* vystup z KS */
        bfr.reading = true;
        advance(true);
    }
    pthread_exit(t);
}

/*
 * ==== vykonani prikazu na pozadi
 * */
int start_command_on_background(COMMAND *c){
    int input  = -1;
    int output = -1;
    int error  = 0;
    pid_t pid  = fork();

    /* synovsky proces */
    if(pid == 0){

        /* na pozadi */
        if(c->na_pozadi == true){
            if(c->output == NULL){
                output = open("/dev/null", O_WRONLY);
            }
            if(c->input == NULL){
                input  = open("/dev/null", O_RDONLY);
            }
        }

        /* presmerovani */
        iofiles(&input, &output, c);

        /* vykonani prikazu */
        execvp(c->argv[0], c->argv);

        /* chyba prikazu */
        perror(msg_err[ERR_CMD]);
        exit(EXIT_FAILURE);
    }
    /* otec */
    else if(pid > 0){
        /* vlozeni do seznamu */
        if((error = sInsertLast(&list, pid, false)) != OK){
            return error;
        }
        else{
            printfStartProcess(pid);
            return OK;
        }
    }
    /* chyba */
    else{
        return ERR_FORK;
    }
    return OK;
}

/*
 * ==== spusteni zadaneho prikazu
 * */
int start_command(COMMAND *c){
    int input  = -1;
    int output = -1;
    pid_t pid  = fork();

    /* synovsky proces */
    if(pid == 0){

        /* presmerovani */
        iofiles(&input, &output, c);

        /* vykonani prikazu */
        execvp(c->argv[0], c->argv);

        /* chyba prikazu */
        perror(msg_err[ERR_CMD]);
        exit(EXIT_FAILURE);
    }
    /* otec */
    else if(pid > 0){
        /* cekani na proces ... uchovani id, pro pripadne kill-nuti*/
        list.aktualni = pid;
        waitpid(pid, NULL, 0);
        list.aktualni = -1;
        return OK;
    }
    /* chyba */
    else{
        return ERR_FORK;
    }
    return OK;
}

/*
 * ==== presmerovani vstupnich a vystupnich souboru
 * */
void iofiles(int *input, int *output, COMMAND *c){

    /* otevreni inputu */
    if(c->input != NULL && strlen(c->input) > 0){
        if((*input = open(c->input, O_RDONLY)) == -1){
            perror(msg_err[ERR_INPUT_FILE]);
            exit(EXIT_FAILURE);
        }
    }

    /* otevreni outputu*/
    if(c->output != NULL && strlen(c->output) > 0){
        if((*output = open(c->output, O_CREAT | O_WRONLY | O_TRUNC, S_IWUSR | S_IRUSR)) == -1){
            perror(msg_err[ERR_OUTPUT_FILE]);
            exit(EXIT_FAILURE);
        }
    }

    /* presmerovani inputu */
    if(*output != -1){
        close(STDOUT_FILENO);
        dup2(*output, STDOUT_FILENO);
    }

    /* presmerovani outputu */
    if(*input != -1){
        close(STDIN_FILENO);
        dup2(*input, STDIN_FILENO);
    }

    return;
}

/*
 * ==== parsovani zadaneho prikazu
 * */
int get_command(COMMAND *c){
    char *words = NULL;
    int delka   = 0;
    int count   = 0;
    int predch  = -1;
    int p_slov  = 0;

    /* pocet slov */
    for(count = 0; count < strlen(bfr.read_buffer); count++){
        switch(bfr.read_buffer[count]){
            case ' ':
            case '\t':
            case '\n':
                p_slov++;
            default:
                break;
        }
    }

    /* alokovani pole */
    if((c->argv = (char**) malloc((p_slov * sizeof(char*)) + 2*sizeof(char*))) == NULL){
        return ERR_MALLOC;
    }

    /* ziskani slov */
    words = strtok(bfr.read_buffer, PARSE_CHARS);
    count = 0;
    while(words != NULL){
        delka = strlen(words);

        switch(*words){
                /* na pozadi */
            case '&':
                if(delka != 1){
                    return ERR_PARSE_AMPERSAND;
                }
                c->na_pozadi = true;
                break;
                /* outpur file */
            case '>':
                if(delka <= 2){
                    return ERR_PARSE_FILE;
                }
                words++;
                c->output = (char*) malloc(delka * sizeof(char));
                strcpy(c->output, words);
                c->output[delka-1] = '\0';
                break;
                /* input file */
            case '<':
                if(delka <= 2){
                    return ERR_PARSE_FILE;
                }
                words++;
                c->input = (char*) malloc(delka * sizeof(char));
                strcpy(c->input, words);
                c->input[delka-1] = '\0';
                break;
                /* nazev + parametry */
            default:
                /* specialni znaky musi byt na konci */
                if(predch+1 != count){
                    return ERR_PARSE_SPECIAL_CHARS;
                }
                predch = count;

                c->argv[c->argc] = NULL;
                if((c->argv[c->argc] = (char*) malloc((delka * sizeof(char)) + sizeof(char))) == NULL){
                    return ERR_MALLOC;
                }
                strcpy(c->argv[c->argc], words);
                c->argv[c->argc][delka] = '\0';
                c->argc++;
                break;
        }
        count  = count + 1;
        words  = strtok(NULL, PARSE_CHARS);
    }

    /* posledni argument musi byt nulovy */
    /*c->argv[c->argc] = (char*) malloc(1 * sizeof(char));*/
    c->argv[c->argc] = (char*) 0;

    if(DEBUG){
        printf("--------------\n");
        printf("Pocet slov = %d\n", c->argc);
        for(count = 0; count < c->argc; count++){
            printf("argv[%d] %d = '%s'\n", count, (int)strlen(c->argv[count]), c->argv[count]);
        }
        printf("input = %s\noutput = %s\nna pozadi = %d\n", c->input, c->output, (int)c->na_pozadi);
        printf("--------------\n");
    }

    return OK;
}

/*
 * ==== inicializace prikazu
 * */
void initCommand(COMMAND *c){
    c->argc      = 0;
    c->argv      = NULL;
    c->output    = NULL;
    c->input     = NULL;
    c->na_pozadi = false;
    return;
}

/*
 * ==== uvolneni prikazu
 * */
void freeCommand(COMMAND *c){
    int i;
    if(c->argv != NULL){
        for(i = 0; i < c->argc; i++){
            if(c->argv[i] != NULL)
                free(c->argv[i]);
        }
        free(c->argv);
    }
    c->argc      = 0;
    c->na_pozadi = false;
    if(c->output != NULL) free(c->output);
    if(c->input  != NULL) free(c->input);
    return;
}

/*
 * ==== cteni vstupu
 * */
int cteniVstupu(){
    int error;
    bool with_signal = true;

    while (!end){
        /* vstup do KS */
        await(true);

        /* muzume zacit cist dokoncene procesy a zapsat prompt */
        if(!end){

            if(DEBUG){
                printSeznam(&list);
            }

            /* vypsani dokoncenych procesu */
            printDokonceno(&list);

            /* vypis promptu */
            showPrompt();

            /* cteni ze stdin */
            if((error = readStdin()) != OK){
                bfr.reading = true;
                writeError((Num)error);
            }
            else{
                bfr.reading = false;
            }

            with_signal = true;
        }
        /* nastal end - neni co odblokovat */
        else{
            bfr.reading = false;
            with_signal = false;
        }

        /* odblokovani cekajici udalosti + vystup z KS */
        advance(with_signal);
        if(!with_signal) return OK;
    }

    return OK;
}

/*
 * ==== cteni ze stdin pomoci fce read
 * */
int readStdin(){
    bfr.length = 0;
    bfr.read_buffer[0] = '\0';

    bfr.length = read(STDIN_FILENO, (void*)bfr.read_buffer, BUFFER_MAX_LENGTH + 1);

    /* kontrola delky - BUFFER_MAX_LENGTH */
    if(bfr.length > BUFFER_MAX_LENGTH && bfr.read_buffer[BUFFER_MAX_LENGTH] != '\n'){
        while('\n' != getchar());
        return ERR_BUFFER_MAX_LENGTH;
    }
    else if(bfr.length > 0){
        /* nastaveni konce stringu */
        bfr.read_buffer[bfr.length-1] = '\0';
        bfr.length = (int)strlen(bfr.read_buffer);
    }
    return OK;
}

/*
 * ==== vypis promptu
 * */
void showPrompt(){
    write(STDOUT_FILENO, (void*)PROMPT, strlen(PROMPT));
    return;
}

/*
 * ==== vstup do kriticke sekce
 * */
void await(bool reading){
    pthread_mutex_lock(&(mtx.mutex));
    while(reading ? !bfr.reading : bfr.reading){
        pthread_cond_wait(&(mtx.cond), &(mtx.mutex));
    }
    return;
}

/*
 * ==== uvolneni kriticke sekce
 * */
void advance(bool with_signal){
    /* odblokovani cekajici udalosti + vystup z KS */
    if(with_signal) pthread_cond_signal(&(mtx.cond));
    pthread_mutex_unlock(&(mtx.mutex));
    return;
}

/*
 * ==== inicializace pthreads
 * */
int initPthreads(PTHREADS *mtx, pthread_t *thread){

    /* inicializace mutexu */
    if(pthread_mutex_init(&(mtx->mutex), NULL) != 0){
        return ERR_MUTEX;
    }

    /* inicializace condu */
    if(pthread_cond_init(&(mtx->cond), NULL) != 0){
        pthread_mutex_destroy(&(mtx->mutex));
        return ERR_COND;
    }

    /* vytvoreni vlakna pro spousteni prikazu */
    if(pthread_create(thread, NULL, spousteniPrikazu, NULL) != 0){
        pthread_cond_destroy(&(mtx->cond));
        pthread_mutex_destroy(&(mtx->mutex));
        return ERR_THREAD;
    }

    return OK;
}

/*
 * ==== uvolneni pthreads
 * */
void freePthreads(PTHREADS *mtx, pthread_t *thread){
    pthread_join(*thread, NULL);
    pthread_cond_destroy(&(mtx->cond));
    pthread_mutex_destroy(&(mtx->mutex));
    return;
}

/*
 * ==== inicializace potrebnych zdroju
 * */
int initSig(struct sigaction *sig_int, struct sigaction *sig_chld){

    /* handler pro int */
    sig_int->sa_handler     = handlerSigInt;
    sig_int->sa_flags     = SA_RESTART;
    sigemptyset(&sig_int->sa_mask);

    /* set sig action - int*/
    if(sigaction(SIGINT, sig_int, NULL) == -1){
        return ERR_SET_SIG_INT;
    }

    /* handler pro chld */
    sig_chld->sa_sigaction  = handlerSigChld;
    sig_chld->sa_flags    = SA_RESTART | SA_SIGINFO | SA_NOCLDSTOP;
    sigemptyset(&sig_chld->sa_mask);

    /* set sig action - chld */
    if(sigaction(SIGCHLD, sig_chld, NULL) == -1){
        return ERR_SET_SIG_CHLD;
    }

    return OK;
}

/*
 * ==== handler pro sig int
 * */
void handlerSigInt(int sig){
    /* existuje spusteny proces na popredi */
    if(list.aktualni > 0){
        writeText("\n");
        kill(list.aktualni, SIGINT);
    }
    /* v opacnem pripade pouze novy PROMPT na novy radek*/
    else{
        writeText("\n");
        showPrompt();
    }
    return;
}

/*
 * ==== handler pro sig chld
 * */
void handlerSigChld(int sig, siginfo_t *pid, void *contxt){

    waitpid(pid->si_pid, NULL, 0);
    if(DEBUG){
        printf("dokonceno\n");
    }
    setDokonceno(&list, pid->si_pid, true);
}

/*
 * ==== vypis chybove hlasky
 * */
void Error(Num n){
    fprintf (stderr, msg_err[n]);
}

/*
 * ==== zapsani chybove hlasky na stdout
 * */
void writeError(Num n){
    write(STDERR_FILENO, (void*)msg_err[n], strlen(msg_err[n]));
}

/*
 * ==== zapsani textu na stdout
 * */
void writeText(char *text){
    write(STDOUT_FILENO, (void*)text, strlen(text));
}

/*
 * ==== inicializace seznamu
 * */
void initSeznam(tList *L){
    L->First = NULL;
    L->Last  = NULL;
    L->Act   = NULL;
    return;
}

/*
 * ==== vypis seznamu --- POUZE PRO KONTROLNI VYPIS ... nevyuzito
 * */
void printSeznam(tList *L){
    TPROCCESS aktual;

    for(aktual=L->First; aktual!=NULL; aktual=aktual->next){
        if(DEBUG){
            printf("Id: %d, dokonceno: %d\n", aktual->pid, (int)aktual->dokoncen);
        }
    }
    return;
}

/*
 * ==== vypis dokoncenych procesu
 * */
void printDokonceno(tList *L){
    TPROCCESS aktual;
    char text[35];

    for(aktual=L->First; aktual!=NULL; aktual=aktual->next){
        if(aktual->dokoncen && aktual->zobrazen == false){
            aktual->zobrazen = true;

            /* vypis stavu - dokoncen */
            sprintf(text, "[%d]\t%d\tDokoncen\n", aktual->cislo_procesu, (int)aktual->pid);
            writeText(text);

            /* smazani */
            if(MAZAT_DOKONCENE){
                sDelete(L, aktual->pid);
            }
        }
    }
    return;
}

/*
 * ==== vypsani odstartovani procesu na pozadi
 * */
void printfStartProcess(pid_t pid){
    char text[30];
    sprintf(text, "[%d]\t%d\n", list.pocet_spustenych, (int)pid);
    writeText(text);
    return;
}

/*
 * ==== vyprazdneni seznamu
 * */
void disposeSeznam(tList *L){
    TPROCCESS pomocny;

    while(NULL != (pomocny = L->First)){
        L->First = pomocny->next;
        free(pomocny);
    }
    L->Act  = NULL;
    L->Last = NULL;
    return;
}

/*
 *´==== nastaveni parametru dokoncen dle pid
 * */
void setDokonceno(tList *L, pid_t pid, bool dokoncen){
    TPROCCESS aktual;

    for(aktual=L->First; aktual!=NULL; aktual=aktual->next){
        if(aktual->pid == pid){
            aktual->dokoncen = dokoncen;
            aktual->zobrazen = false;
        }
    }
    return;
}

/*
 * ==== vlozeni prvku na posledni misto
 * */
int sInsertLast(tList *L, pid_t pid, bool dokoncen){
    struct proces *novy;

    /* alokace noveho prvku */
    if((novy = malloc(sizeof(struct proces))) == NULL){
        return ERR_MALLOC_ADD_LIST;
    }

    novy->next = NULL;
    novy->prev = NULL;

    /* pokud nemame prvni, novy je prvni */
    if(L->First == NULL){
        L->First = novy;
    }

    /* posledni ulozim do leve strany pointeru a pokud neni NULL ... levemu ulozim pointer na novy prvek */
    if(NULL != (novy->prev = L->Last)){
        novy->prev->next = novy;
    }

    /* poslednim je nove pridany */
    L->Last = novy;

    /* data */
    list.pocet_spustenych ++;

    novy->dokoncen = dokoncen;
    novy->pid      = pid;
    novy->zobrazen = false;
    novy->cislo_procesu = list.pocet_spustenych;

    return OK;
}

/*
 * ==== smazat prvek ze seznamu
 * */
int sDelete(tList *L, pid_t pid){
    TPROCCESS aktual;

    for(aktual=L->First; aktual!=NULL; aktual=aktual->next){
        if(aktual->pid == pid){

            /* smazat prvek */
            if(aktual->prev != NULL && aktual->next != NULL){
                aktual->prev->next = aktual->next;
                aktual->next->prev = aktual->prev;
            }
            else if(aktual->prev == NULL && aktual->next != NULL){
                /* jedna se o pvni prvek */
                L->First = aktual->next;
                aktual->next->prev = NULL;
            }
            else if(aktual->prev != NULL && aktual->next == NULL){
                /* jedna se o posledni prvek */
                L->Last  = aktual->prev;
                aktual->prev->next = NULL;
            }
            else{
                /* pouze jeden prvek - ten aktualni */
                L->First = NULL;
                L->Last  = NULL;
                L->Act   = NULL;
            }

            free(aktual);
            return OK;
        }
    }
    return OK;
}
