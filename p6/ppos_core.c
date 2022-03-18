//GRR20190428 Eduardo Mathias de Souza 
// operating system check
//Para compilar gcc pingpong-contab.c ppos_core.c queue.c
//Para compilar gcc pingpong-contab-prio.c ppos_core.c queue.c 
#if defined(_WIN32) || (!defined(__unix__) && !defined(__unix) && (!defined(__APPLE__) || !defined(__MACH__)))
#warning Este codigo foi planejado para ambientes UNIX (LInux, *BSD, MacOS). A compilacao e execucao em outros ambientes e responsabilidade do usuario.
#endif

#include "ppos_data.h"
#include "ppos.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <ucontext.h>

#define AGING -1
#define STACKSIZE 64*1024
#define QUANTUM 20

task_t Main;
task_t *TarefaAtual, *TarefaAnterior;
task_t dispatcher;
task_t *filaTarefas; 
int count = 0; 
int aging;                                  
struct sigaction action;                    //Estrutura que define o tratador de sinal
struct itimerval timer;                     //Estrutura de inicialização do timer
int contadorTimer;                          //Contador referente ao Quantum de cada tarefa
unsigned int tempoAtual;                    //Conta o tempo de execução do programa em milisegundos

unsigned int systime ()
{
    return tempoAtual;                      //Retorna o tempo atual de execução do programa
}



//Função que será ativada quando o temporizador chegar a um determinado tempo
void trata_timer()
{
    TarefaAtual->tempoProcessamento++;
    if(TarefaAtual-> tipoTarefa == 1){
        //Caso o quantum chegue a zero, o quantum da tarefa se torna o valor de quantum predefinido e a tarefa é adicionada ao final da fila de prontas
        //o contexto é mudado para o dispatcher
        if(TarefaAtual->contadorQuantumTarefa <= 0){
            TarefaAtual->contadorQuantumTarefa = QUANTUM;
            task_yield();
        }
        TarefaAtual->contadorQuantumTarefa--;
    }
    tempoAtual++;
}

task_t* scheduler(){
    task_t* aux = filaTarefas;
    task_t* tarefa_prioritaria = NULL;
    int minPrio = 21;
    int tamanhoFila = queue_size((queue_t*) filaTarefas);

    //Percorre a lista de prontas
    for(int i = 0; i < tamanhoFila; i++){
        //Se o valor da prioridadeDinamica for menor que o valor de 'minPrio', 'minPrio' recebe a prioridadeDinamica
        if(aux->prioridadeDinamica <= minPrio){
            minPrio = aux->prioridadeDinamica;
            tarefa_prioritaria = aux;
        }
        aux = aux->next;
    }

    //Percorre a fila de prontas novamente
    for(int i = 0; i < tamanhoFila; i++){
        //Se a tarefa nao for a escolhida, nao seja a de maior prioridade,ela "envelhece" somando sua prioridade dinâmica com o valor AGING
        if(aux != tarefa_prioritaria){
            aux->prioridadeDinamica += AGING;
            if(aux->prioridadeDinamica < -20)
                aux->prioridadeDinamica = -20;
        }
        //Se a tarefa for a escolhida, seja a de maior prioridade, o valor de prioridade dinamica = estatica
        else
            aux->prioridadeDinamica = aux->prioridadeEstatica;
        aux = aux->next;}
    return tarefa_prioritaria;
}

void dispatcher_body (){
    task_t *next;
    while ( queue_size((queue_t*)filaTarefas) > 0 )
    {
        next = scheduler();
        if (next)
            task_switch (next) ; // transfere controle para a tarefa "next"
    }
    task_exit(0) ; // encerra a tarefa dispatcher
}


void ppos_init (){
    setvbuf (stdout, 0, _IONBF, 0);
    TarefaAtual = &Main;
    Main.id = 0;

    //Cria dispatcher
    task_create(&dispatcher, dispatcher_body, NULL);
    dispatcher.tipoTarefa = 0; //Dispatcher nao eh uma tarefa de usuario, sim de sistema

    filaTarefas = NULL;
    contadorTimer = QUANTUM;
    tempoAtual = -1;

    //Registra a ação para o sinal de timer SIGALRM
    action.sa_handler = trata_timer;
    sigemptyset (&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction (SIGALRM, &action, 0) < 0){
        fprintf(stderr,"Erro em sigaction: ");
        exit (1);
    }

    //Ajusta valores  do temporizador, que irá gerar um pulso a cada 1 milisegundo
    timer.it_value.tv_usec = 1;             // primeiro disparo, em micro-segundos
    timer.it_value.tv_sec  = 0;             // primeiro disparo, em segundos
    timer.it_interval.tv_usec = 1000;       // disparos subsequentes, em micro-segundos
    timer.it_interval.tv_sec  = 0;          // disparos subsequentes, em segundos

    //Arma o temporizador ITIMER_REAL
    if (setitimer (ITIMER_REAL, &timer, 0) < 0){
        fprintf(stderr,"Erro em setitimer: ");
        exit (1);
    }
}

int task_create (task_t *task, void (*start_routine)(void *),  void *arg){
    count++;
    getcontext(&task->context);
    char *stack = malloc(STACKSIZE);
    if (stack){
      task->context.uc_stack.ss_sp = stack ;
      task->context.uc_stack.ss_size = STACKSIZE ;
      task->context.uc_stack.ss_flags = 0 ;
      task->context.uc_link = 0 ;
   }
   else
   {
      perror("Erro na criação da pilha: ") ;
      return -1;
   }
   makecontext(&task->context,(void*)start_routine, 1, (char*) arg);
   if(task != &dispatcher)
        queue_append((queue_t**) &filaTarefas, (queue_t*) task);

   task->id = count;
   task->prioridadeEstatica = 0;
   task->prioridadeDinamica = 0;
   
    task->tipoTarefa = 1;                //A tarefa é de usuário
    task->contadorQuantumTarefa = QUANTUM;        //Inicializado o contador de quantum com o quantum predefinido
    task->tempoExecucao = systime();
    task->tempoProcessamento = 0;
    task->ativacoes = 0;

    return task->id;
}

int task_switch (task_t *task){
    TarefaAnterior = TarefaAtual;
    TarefaAtual = task;
    TarefaAtual->ativacoes++;
    if(swapcontext(&TarefaAnterior->context, &task->context) < 0)
        return -1;
    return 0;
}

void task_exit (int exit_code){
    if(TarefaAtual != &dispatcher && TarefaAtual != &Main){
        queue_remove((queue_t**) &filaTarefas, (queue_t*) TarefaAtual);
        TarefaAtual->tempoExecucao = systime() - TarefaAtual->tempoExecucao;
        printf("Task: %d exit: excecution time %d ms, processor time %d ms, %d activations\n",
                TarefaAtual->id, TarefaAtual->tempoExecucao, TarefaAtual->tempoProcessamento, TarefaAtual->ativacoes);
        task_switch(&dispatcher);
    }
    //Caso a tarefa a ser finalizada seja o dispatcher, troca para a tarefa main
    else{
        dispatcher.tempoExecucao = systime() - dispatcher.tempoExecucao;
        printf("Task: %d exit: excecution time %d ms, processor time %d ms, %d activations\n",
                 dispatcher.id, dispatcher.tempoExecucao,  dispatcher.tempoProcessamento, dispatcher.ativacoes);
        task_switch(&Main);}
}

int task_id (){
    return TarefaAtual->id;
}

void task_yield (){
    if(TarefaAtual != &Main && TarefaAtual != &dispatcher){
        queue_remove((queue_t**) &filaTarefas, (queue_t*) TarefaAtual);
        queue_append((queue_t**) &filaTarefas, (queue_t*) TarefaAtual);
    }
    task_switch(&dispatcher);
}

void task_setprio (task_t *task, int prio)
{
    if(prio >= -20 && prio <= 20){
        if(task == NULL){
            TarefaAtual->prioridadeEstatica = prio;
            TarefaAtual->prioridadeDinamica = prio;
        }
        else{
            task->prioridadeEstatica = prio;
            task->prioridadeDinamica = prio;
        }
    }
    else
       fprintf(stderr,"A prioridade esta fora dos valores permitidos\n");
}

int task_getprio (task_t *task)
{
    if(task == NULL)
        return TarefaAtual->prioridadeEstatica;

    return task->prioridadeEstatica;
}
