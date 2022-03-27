//GRR20190428 Eduardo Mathias de Souza 
// operating system check
//Para compilar gcc pingpong-contab.c ppos_core.c queue.c (Teste1)
//Para compilar gcc pingpong-contab-prio.c ppos_core.c queue.c (Teste2)
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
task_t *filaDormitorio;

int count = 0;
int contadorAux; 
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
    contadorAux++;
}

task_t* scheduler(){
    return filaTarefas;
}

void dispatcher_body (){
    task_t *next;
    while (1){
        if(queue_size((queue_t*)filaDormitorio) > 0 && contadorAux >= 1000){
            task_t *aux = filaDormitorio;
            task_t *auxNext;
            int tamanhoFila = queue_size((queue_t*)filaDormitorio);
            //Percorre a lista de adormecidas
            for(int i = 0; i < tamanhoFila; i++){
                auxNext = aux->next;
                aux->sleepTime--;
                //Caso o sleeptime da tarefa for zero, hora de acorda-la e jogar na fila de Prontas
                if(aux->sleepTime <= 0){
                    //Se o ponteiro aux aponta para o inicio da fila
                    if(aux == filaDormitorio){
                        //Se tem apenas um elemento no dormitorio, o inicio da fila passa a apontar para NULL
                        if(aux->next == aux)
                            filaDormitorio = NULL;
                        //Se existe mais de um elemento, o inicio da fila passa a apontar para o próximo elemento de aux*/
                        else
                            filaDormitorio = aux->next;
                        }
                    //Desperta a tarefa
                    task_resume(aux, &filaTarefas);
                    }
                aux = auxNext;
                }
            contadorAux = 0;}
        if(queue_size((queue_t*)filaTarefas) > 0){
            next = scheduler();                                 
            if(next)
                task_switch(next);}
        if(queue_size((queue_t*)filaTarefas) == 0 && queue_size((queue_t*)filaDormitorio) == 0)
            break;
        }
    //Após executar todas as tarefas, retorna para o contexto da função main ou para o dispatcher
    task_exit(0);
}


void ppos_init (){
    setvbuf (stdout, 0, _IONBF, 0);
    task_create(&Main, NULL, NULL);
    TarefaAtual = &Main;
    Main.id = 0;

    //Cria dispatcher
    task_create(&dispatcher, dispatcher_body, NULL);
    dispatcher.tipoTarefa = 0; //Dispatcher nao eh uma tarefa de usuario, sim de sistema

    filaTarefas = NULL;
    filaDormitorio = NULL;
    contadorTimer = QUANTUM;
    tempoAtual = -1;
    contadorAux = -1;
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
    task_yield();
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
   task->suspendedQueue = NULL;
   task->exitCode = 0;
   task->sleepTime = 0;

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
    task_t *aux = TarefaAtual->suspendedQueue;

    //Caso exista algum elemento na fila de tarefas suspensas da tarefa que será finalizada
    if(aux){
        task_t *aux2;
        int tamanhoFila = queue_size((queue_t*)aux);

        //Percorre a fila e resume todas as tarefas que pertencem a ela
        for(int i = 0; i < tamanhoFila; i++){
            aux2 = aux->next;
            task_resume(aux, &filaTarefas);
            aux = aux2;
        }
        TarefaAtual->suspendedQueue = NULL;
    }
    TarefaAtual->exitCode = exit_code;
    if(TarefaAtual != &dispatcher && TarefaAtual != &Main){
        TarefaAtual->status = 3;
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


void task_suspend (task_t **queue)
{
    //Troca o estado da tarefa para suspensa (0)
    TarefaAtual->status = 0;

    //Remove a tarefa da fila de prontas
    queue_remove((queue_t**) &filaTarefas, (queue_t*) TarefaAtual);

    //Insere a tarefa na fila recebida
    queue_append((queue_t**) queue, (queue_t*) TarefaAtual);
}

int task_resume (task_t *task, task_t **queue)
{
    //Se tarefa existe
    if(task){
        //Troca o estado da tarefa para pronta(2)
        task->status = 2;

        //Retira a tarefa de sua fila atual
        if(task->next && task->prev){
            task->prev->next = task->next;
            task->next->prev = task->prev;
        }
        task->next = NULL;
        task->prev = NULL;

        //Insere a tarefa na fila de tarefas prontas
        queue_append((queue_t**) &filaTarefas, (queue_t*) task);
    }
    return 1;
}

int task_join (task_t *task)
{
    //Se a tarefa recebida existe e nao esta finalizada, suspende a tarefa atual e a insere na fila de suspensas
    if(task && task->status != 3){
        task_suspend(&filaDormitorio);
        task_yield();
        return task->exitCode;
    }
    return -1;
}

void task_sleep (int t)
{
    //Se o tempo recebido for maior que zero, atualiza a variável sleepTime da tarefa atual
    if(t > 0){
        TarefaAtual->sleepTime = t;
        //Tira a tarefa da fila de prontas e a coloca na fila de dormitorio
        task_suspend(&filaDormitorio);
    }
    task_yield();
}

