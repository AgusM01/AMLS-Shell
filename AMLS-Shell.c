#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define DELIMITER " \n"
#define READ_END 0 //Entrada estándar
#define WRITE_END 1  //Salida estándar


typedef struct _commands_list{ //Los programas y sus argumentos estarán en una lista enlazada.
    struct _commands_list* next;
    struct _commands_list* prev;
    char** args;
    char** files;
    int max_files;
    int red_flag;
    int n_node;
    int last_prog;
    int pos_files;
    int pos_args;
    int max_comands;
} commands_list;

commands_list* create_command_list (){
    commands_list* node = NULL;
    return node;
}

commands_list* command_list_agg_final(commands_list* com_list){
    commands_list* node = malloc(sizeof(commands_list));;
    node->files = NULL;
    node->max_files = 10;
    node->pos_args = 0;
    node->red_flag = 0;
    node->next = NULL;
    node->max_comands = 10;
    node->args = malloc(sizeof(char*) * node->max_comands);
    if (com_list == NULL){
        node->prev = NULL;
        node->n_node = 0;
        node->last_prog = 1;
        return node;
    }
    com_list->next = node; //Conecto los nodos
    com_list->last_prog = 0; //EL anterior no es mas mi ultimo
    node->prev = com_list; //Conecto los nodos
    node->n_node = com_list->n_node + 1;
    node->last_prog = 1;
    return node;
}
void agg_files_node(commands_list* com_list, char* file){
    if(com_list->max_files == com_list->pos_files){
        com_list->max_files *= 2;
        com_list->files = realloc(com_list->files, com_list->max_files);
    }
    if (file == NULL)
        com_list->args[com_list->pos_args] = NULL;
    if(file != NULL){
        com_list->files[com_list->pos_files] = malloc((strlen(file) + 1));
        strcpy(com_list->files[com_list->pos_files], file);
        com_list->pos_files += 1;
    }
    return;
}

void agg_args_node(commands_list* com_list, char* command){ //Cuando quiero guardar solo los argumentos a ejecutar.
    
    if(com_list->max_comands == com_list->pos_args){
        com_list->max_comands *= 2;
        com_list->args = realloc(com_list->args, com_list->max_comands);
    }
    if (command == NULL)
        com_list->args[com_list->pos_args] = NULL;
    else{
        com_list->args[com_list->pos_args] = malloc((strlen(command) + 1));
        strcpy(com_list->args[com_list->pos_args], command);
        com_list->pos_args += 1;
    }    
    return;
}

ssize_t read_stdin(char** buf){
    ssize_t len;
    size_t n;
    len = getline(buf, &n, stdin);
    return len;
}

commands_list* check_command(char* copy, char** args, commands_list* com_list){
    int i = 0;

    args[i] = strtok(copy, DELIMITER);
    com_list = command_list_agg_final(com_list);
    if((strcmp(args[i], ">") != 0) && (strcmp(args[i], "|") != 0))
        agg_args_node(com_list, args[i]); //Agrego el comando al nodo.

    i++;

    while(args[i - 1] != NULL){
        
        args[i] = strtok(NULL, DELIMITER);
        
        if (args[i] != NULL){
            if(strcmp(args[i], "|") == 0){
                agg_args_node(com_list, NULL);
                agg_files_node(com_list, NULL);
                com_list = command_list_agg_final(com_list); //Como hay un pipe, creo un nuevo ya que los comandos anteriores ya fueron guardados.
            }
            else{
                if (strcmp(args[i - 1], ">") == 0){
                    if (com_list->red_flag == 0)
                        com_list->files = malloc(sizeof(char*)* com_list->max_files);
                    agg_files_node(com_list, args[i]);
                    com_list->red_flag = 1;
                }
                else //Estoy en un comando válido.
                    if (strcmp(args[i], ">") != 0)
                        agg_args_node(com_list, args[i]);   
            } 
        }
        i++;
    }
    agg_files_node(com_list, NULL);
    agg_args_node(com_list, NULL);
    while(com_list->n_node != 0) //Mando la lista al inicio ya que debo ejecutar los programas secuencialmente.
        com_list = com_list->prev;
    return com_list;
}


int cant_nodos(commands_list* com_list){
    commands_list* count_node = com_list;
    int cant_nodes = 0;
    for(; count_node != NULL; count_node = count_node->next)
        cant_nodes++;
    return cant_nodes;
}
void execute(commands_list* com_list){
    if (com_list->red_flag > 0){
        for(int i = 0; i < com_list->pos_files; i++){
            int fd = open(com_list->files[i], O_CREAT | O_WRONLY | O_TRUNC, 0644);
            dup2(fd, WRITE_END);
            close(fd);
        }
    }
    execvp(com_list->args[0], com_list->args);
    exit(EXIT_FAILURE);
}
void exec_com_list(commands_list* com_list){
    if (com_list->next != NULL){  //Hay pipes
        int cant_nodes = cant_nodos(com_list); //Cantidad de programas
        int fd[cant_nodes][2];
        commands_list* exec_node = com_list;
        for(; exec_node != NULL; exec_node = exec_node->next){
            if (exec_node->n_node == 0){ //Estoy en el primer programa.
                pipe(fd[exec_node->n_node]);
                pid_t primer_prog = fork();
                if (primer_prog == 0){
                    close(fd[exec_node->n_node][READ_END]); //Cierra la boca de lectura ya que es el primer programa.
                    dup2(fd[exec_node->n_node][WRITE_END], WRITE_END);
                    close(fd[exec_node->n_node][WRITE_END]); //Cierra este fd ya que dup hace una copia en la filetable donde está el 1.
                    execute(exec_node);
                }
                close(fd[exec_node->n_node][WRITE_END]); //Cierra la boca de escritura en el padre.
            }
            else{
                if (exec_node->last_prog == 1){
                    pipe(fd[exec_node->n_node]);
                    pid_t last_prog = fork();
                    if (last_prog == 0){
                        close(fd[exec_node->n_node][READ_END]); //Cierra la boca de lectura ya que es el primer programa.
                        dup2(fd[exec_node->n_node - 1][READ_END], 0);
                        close(fd[exec_node->n_node - 1][READ_END]);
                        execute(exec_node);
                    }
                    close(fd[exec_node->n_node][READ_END]); //Cierra la boca de escritura en el padre.
                }
                else{ //Estoy en los programas del medio
                    pipe(fd[exec_node->n_node]);
                    pid_t prog_middle = fork();
                    if (prog_middle == 0){
                        close(fd[exec_node->n_node][READ_END]); //Cierra su boca de lectura ya que va a leer del pipe anterior.
                        dup2(fd[exec_node->n_node][WRITE_END], 1); //Va a escribir en su boca de escritura.
                        dup2(fd[exec_node->n_node - 1][READ_END], 0); //Va a leer del pipe anterior.
                        close(fd[exec_node->n_node][WRITE_END]);
                        close(fd[exec_node->n_node - 1][READ_END]);
                        execute(exec_node);
                    }
                    close(fd[exec_node->n_node][WRITE_END]); //Cierra la boca de escritura ya que estoy en el padre.
                    close(fd[exec_node->n_node - 1][READ_END]);
                }
            }
        }
    }
    else{
        pid_t program = fork();
        if (program == 0)
            execute(com_list);
    }
    int status;
    wait(&status);
    return;
}
int main(){
    while(1){
        printf("$ ");
        char* buf = NULL;
        ssize_t len = read_stdin(&buf);
        char copy[len + 1];

        strcpy(copy, buf);
        free(buf);
        if (strcmp(copy, "exit\n") == 0)
            return 0;
        if (!strcmp(copy, "\n") == 0){

            char* args[len];
            commands_list* com_list = create_command_list();

            com_list = check_command(copy, (char**)args, com_list);
            exec_com_list(com_list);
            int status;
            wait(&status);
        }
        
    }
    return 0;
}
