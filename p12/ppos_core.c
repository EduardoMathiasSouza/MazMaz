//GRR20190428 Eduardo Mathias de Souza 
// operating system check
//Para compilar gcc pingpong-mqueue.c ppos_core.c queue.c -lm -lpthread
#if defined(_WIN32) || (!defined(__unix__) && !defined(__unix) && (!defined(__APPLE__) || !defined(__MACH__)))
#warning Este codigo foi planejado para ambientes UNIX (LInux, *BSD, MacOS). A compilacao e execucao em outros ambientes e responsabilidade do usuario.
#endif

#include "ppos.h"
#include "ppos_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include "queue.h"

#define AGING -1
#define STACKSIZE 64*1024
#define QUANTUM 20
#define TAMANHO_BUFFER_CIRCULAR 6 

task_t Main;                            
task_t *TarefaAtual;                        
int contador = -1;                               
task_t dispatcher;                          
task_t *filaTarefas;                        
task_t *filaDormitorio;                 
struct sigaction action;                    //Estrutura que define o tratador de sinal
struct itimerval timer;                     //Estrutura de inicialização do timer
int contadorTimer;                          //Contador referente ao Quantum de cada tarefa
unsigned int tempoAtual;                    //Conta o tempo de execução do programa em milisegundos
int tempoAux;                               //Conta o tempo de execução para tratar sono de tarefas do dormitorio
int lock = 0;

unsigned int systime ()
{
    return tempoAtual;                      //Retorna o tempo atual de execução do programa
}

void enter_cs (int *lock)
{
  //espera ocupada
  while (__sync_fetch_and_or (lock, 1)) ;
}
 
void leave_cs (int *lock)
{
  *lock = 0 ;
}


//Função que será ativada quando o temporizador chegar a um determinado tempo
void tratador()
{
    if(TarefaAtual){
        TarefaAtual->tempoProcessamento++;
        //Caso seja uma tarefa de usuário
        if(TarefaAtual->tipoTarefa == 1){
            //Caso o quantum chegue a zero, o quantum da tarefa se torna o valor de quantum predefinido e a tarefa é adicionada ao final da fila de prontas
            //o contexto é mudado para o dispatcher
            if(TarefaAtual->contadorQuantumTarefa <= 0){
                TarefaAtual->contadorQuantumTarefa = QUANTUM;
                task_yield();
            }
            TarefaAtual->contadorQuantumTarefa--;
        }
    }
    tempoAtual++;
    tempoAux++;
}

task_t* scheduler()
{
    return filaTarefas;
}

void dispatcher_body()
{
    task_t * next;

    while(1){
        //Caso haja elementos no dormitorio e o contador auxiliar bata um segundo
        if(queue_size((queue_t*)filaDormitorio) > 0 && tempoAux >= 1){
            task_t *aux = filaDormitorio;
            task_t *auxNext;
            int tamanhoFila = queue_size((queue_t*)filaDormitorio);

            for(int i = 0; i < tamanhoFila; i++){
                auxNext = aux->next;

                //Atualiza o tempo de sono da tarefa
                aux->sleepTime--;

                //Se o tempo da tarefa analisada for 0
                if(aux->sleepTime <= 0){

                    //E for o comeco da fila
                    if(aux == filaDormitorio){
                        //Se existir apenas um elemento na fila, o inicio da fila aponta para nulo
                        if(aux->next == aux)
                            filaDormitorio = NULL;
                        //Senão, o inicio da fila aponta para o próximo elemento de aux
                        else
                            filaDormitorio = aux->next;
                    }
                    //Acorda a tarefa
                    task_resume(aux, &filaTarefas);
                }
                aux = auxNext;
            }
            //Reseta a variável tempoAux
            tempoAux = 0;
        }

        if(queue_size((queue_t*)filaTarefas) > 0){
            next = scheduler();                 
            if(next)
                task_switch(next);
        }

        //Se nao houver nada na fila de tarefas prontas e nem no dormitorio, encerra o while
        if(queue_size((queue_t*)filaTarefas) == 0 && queue_size((queue_t*)filaDormitorio) == 0)
            break;

    }
    task_exit(0);
}

void ppos_init()
{
    setvbuf (stdout, 0, _IONBF, 0);
    filaTarefas = NULL;
    filaDormitorio = NULL;
    tempoAux = -1;

    //Cria Main
    task_create(&Main, NULL, NULL);
    TarefaAtual = &Main;

    //Cria dispatcher
    task_create(&dispatcher, dispatcher_body, NULL);
    dispatcher.tipoTarefa = 0; //Dispatcher nao eh uma tarefa de usuario, sim de sistema

    contadorTimer = QUANTUM;
    tempoAtual = -1;
    
    //Registra a ação para o sinal de timer SIGALRM
    action.sa_handler = tratador;
    sigemptyset (&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction (SIGALRM, &action, 0) < 0){
        perror ("Erro em sigaction: ");
        exit (1);
    }

    //Ajusta valores  do temporizador, que irá gerar um pulso a cada 1 milisegundo
    timer.it_value.tv_usec = 1;             // primeiro disparo, em micro-segundos
    timer.it_value.tv_sec  = 0;             // primeiro disparo, em segundos
    timer.it_interval.tv_usec = 1000;       // disparos subsequentes, em micro-segundos
    timer.it_interval.tv_sec  = 0;          // disparos subsequentes, em segundos

    //Arma o temporizador ITIMER_REAL
    if (setitimer (ITIMER_REAL, &timer, 0) < 0){
        perror ("Erro em setitimer: ");
        exit (1);
    }
    task_yield();
}

int task_create( task_t *task,
                 void (*start_routine)(void *),
                 void *arg)
{
    char *stack;
    contador++;
    if(getcontext(&task->context) < 0)
        return -1;
    stack = malloc(STACKSIZE);
    if(stack){
        task->context.uc_stack.ss_sp = stack;
        task->context.uc_stack.ss_size = STACKSIZE;
        task->context.uc_stack.ss_flags = 0;
        task->context.uc_link = 0;
    }
    else{
        perror ("Erro na criação da pilha: ");
        return -1;
    }

    makecontext(&task->context, (void*)start_routine, 1, (char*) arg);
    if(task != &dispatcher)
        queue_append((queue_t**) &filaTarefas, (queue_t*) task);

    task->id = contador;
    task->prioridadeEstatica = 0;
    task->prioridadeDinamica = 0;

    task->tipoTarefa = 1; //A tarefa eh de usuario
    task->contadorQuantumTarefa = QUANTUM; //Inicializado o contador de quantum com o quantum predefinido

    task->tempoExecucao = systime();
    task->tempoProcessamento = 0;
    task->ativacoes = 0;
    task->suspendedQueue = NULL;
    task->exitCode = 0;
    task->sleepTime = 0;

    return task->id;
}

int task_switch (task_t *task)
{
    task_t *Aux = TarefaAtual;
    TarefaAtual = task;
    TarefaAtual->ativacoes++;
    if(swapcontext(&Aux->context, &task->context) < 0)
        return -1;

    return 0;
}

void task_exit (int exitCode)
{
    task_t *aux = TarefaAtual->suspendedQueue;

    //Caso haja algum elemento na fila de tarefas suspensas da tarefa que sera suspensa
    if(aux){
        task_t *aux2;
        int tamanhoFila = queue_size((queue_t*)aux);

        //Percorre a fila e acorda todas as tarefas que pertencem a ela
        for(int i = 0; i < tamanhoFila; i++){
            aux2 = aux->next;
            task_resume(aux, &filaTarefas);
            aux = aux2;
        }
        TarefaAtual->suspendedQueue = NULL;
    }
    TarefaAtual->exitCode = exitCode;

    //Caso a tarefa atual não seja o dispatcher, remove a tarefa atual da fila de prontas e troca pro dispatcher
    if(TarefaAtual != &dispatcher){
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
        task_switch(&Main);
    }
}

int task_id ()
{
    return TarefaAtual->id;
}

void task_setprio (task_t *task, int prio)
{
    
    if(prio >= -20 && prio <= 20){
        if(!task){
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

void task_yield ()
{
    if(TarefaAtual != &dispatcher && TarefaAtual->status != 0){
        queue_remove((queue_t**) &filaTarefas, (queue_t*) TarefaAtual);
        queue_append((queue_t**) &filaTarefas, (queue_t*) TarefaAtual);
    }
    task_switch(&dispatcher);
}

void task_suspend (task_t **queue)
{
    task_t* aux = TarefaAtual;

    //Troca o status da tarefa para suspensa (0)
    aux->status = 0;

    //Remove a tarefa da fila de prontas
    queue_remove((queue_t**) &filaTarefas, (queue_t*) aux);

    //Insere a tarefa na fila recebida
    queue_append((queue_t**) queue, (queue_t*) aux);
}

void task_resume (task_t *task, task_t **queue)
{
    if(task){
        task->status = 1; //Troca o status da tarefa para pronta (1)

        //Retira a tarefa de sua fila atual
        if(task->next && task->prev){
            task->prev->next = task->next;
            task->next->prev = task->prev;
        }
        task->next = NULL;
        task->prev = NULL;

        //Insere a tarefa recebida na lista de prontas
        queue_append((queue_t**) &filaTarefas, (queue_t*) task);
    }
}

int task_join (task_t *task)
{
    //Caso a tarefa recebida exista e ainda estiver ativa, suspende a tarefa atual e a insere na fila de suspensas
    if(task && task->status != 3){
        task_suspend(&task->suspendedQueue);
        task_yield();
        return task->exitCode;
    }
    return -1;
}

void task_sleep (int t)
{
    //Caso o tempo recebido seja maior que zero, atualiza a variável sleepTime da tarefa atual e a suspende
    if(t > 0){
        TarefaAtual->sleepTime = t;
        task_suspend(&filaDormitorio);
    }
    task_yield();
}


int sem_create(semaphore_t *s, int value)
{
    //Caso o semaforo exista, recebe o valor passado e inicia sua fila vazia
    if(s){
        s->contador_sem = value;
        s->filaTarefas_sem = NULL;
        s->existe = 1;
        s->lock = 0;
        return 0;
    }
    return -1;
}

int sem_down(semaphore_t *s)
{
    enter_cs(&s->lock);
    if(s->existe == 1){ //Se o semaforo existe
        s->contador_sem--; //Decrementa o contador do semaforo
        if(s->contador_sem < 0){ //Caso o contador_sem seja menor que zero, suspende tarefa atual e a coloca no final da fila do semáforo recebido
            task_suspend(&s->filaTarefas_sem);
            leave_cs(&s->lock);
            task_yield();//Tarefa bloqueada, devolve o contexto para o dispatcher
        }
        else{
            leave_cs(&s->lock);}
        return 0;
    }
    leave_cs(&s->lock);
    return -1;
}

int sem_up(semaphore_t *s)
{
    enter_cs(&s->lock);
    if(s->existe == 1){
        s->contador_sem++;//Incrementa o contador do semaforo
        if(s->contador_sem <= 0 && s->filaTarefas_sem){//Se existir elementos na fila, e contador for menor ou igual a zero, acorda o primeiro elemento da fila do semáforo
            task_t* aux = s->filaTarefas_sem;
            if(aux->next != aux)
                s->filaTarefas_sem = aux->next;
            else
                s->filaTarefas_sem = NULL;
            task_resume(aux, &filaTarefas);
        }
        leave_cs(&s->lock);
        return 0;
    }
    leave_cs(&s->lock);
    return -1;
}

int sem_destroy(semaphore_t *s)
{
    enter_cs(&s->lock);
    if(s){//Caso o semaforo exista
        task_t *aux = s->filaTarefas_sem;
        task_t *auxNext;
        int tamanhoFila = queue_size((queue_t*)aux);

        //Percorre a fila e acorda todo mundo
        for(int i = 0; i < tamanhoFila; i++){
            auxNext = aux->next;
            task_resume(aux, &filaTarefas);
            aux = auxNext;
        }
        s->existe = 0;
        leave_cs(&s->lock);
        return 0;
    }
    leave_cs(&s->lock);
    return -1;
}


int mqueue_create(mqueue_t *queue, int max, int size)
{
    if(queue){ //Se fila passada existe
        if(max > 0 && size >= 0){ //Se tamanho max de mensagens na fila existe e tamanho maximo das mensagens existe
            queue->qntdMaxMsg = max;
            queue->tamMaxMsg = size;
            queue->quantidade = 0;
            queue->filaMensagens = malloc(max * size);
            sem_create(&queue->buffer, 1);//Inicializa buffer com 1 (Existe buffer)
            sem_create(&queue->item, 0);//Inicializa item com 0 (Nao existe item ainda na fila de mensagens)
            sem_create(&queue->vaga, TAMANHO_BUFFER_CIRCULAR);//Inicializa vaga com o tamanho do buffer circular (Todas as vagas estão vazias)
            queue->existe = 1; //Inicializa que a fila foi inicializada
            return 0;
        }
    }
    return -1;
}


int mqueue_send(mqueue_t *queue, void *msg)
{
    if(queue->existe == 1){//Caso fila existe
        sem_down(&queue->vaga);//Bloqueante, verifica se existe vaga para mandar msg
        sem_down(&queue->buffer);//Bloqueante, pega buffer

        if(queue->quantidade < TAMANHO_BUFFER_CIRCULAR){//Verifica se a quantidade de mensagens na fila é menor que o tamanho do buffer
            memcpy(&queue->filaMensagens[queue->quantidade], msg, queue->tamMaxMsg);//Envia para a fila de mensagens a msg
            queue->quantidade++;//aumenta a quantidade de mensagens da fila
        }

        sem_up(&queue->buffer);//Libera buffer
        sem_up(&queue->item);//Adiciona mais um item ao buffer
        return 0;
    }
    return -1;
}

int mqueue_recv(mqueue_t *queue, void *msg)
{
    if(queue->existe == 1){
        sem_down(&queue->item);//Bloqueante, verifica se existe item para receber msg
        sem_down(&queue->buffer);//Pega o buffer pra si

        memcpy(msg, &queue->filaMensagens[0], queue->tamMaxMsg);//Retira uma mensagem da fila de mensagens
        queue->quantidade--;//reduz a qntd de itens na fila de mensagens

        for(int i = 0; i < queue->quantidade; i++){//Atualiza as mensagens após retirada da mensagem
            queue->filaMensagens[i] = queue->filaMensagens[i + 1];
        }

        sem_up(&queue->buffer);//Libera o buffer
        sem_up(&queue->vaga);//Aumenta em uma vaga a fila de mensagens
        return 0;
    }
    return -1;
}

int mqueue_destroy(mqueue_t *queue)
{
    if(queue->existe == 1){
        sem_destroy(&queue->buffer);
        sem_destroy(&queue->item);
        sem_destroy(&queue->vaga);
        queue->existe = 0;
        return 0;
    }
    return -1;
}

int mqueue_msgs(mqueue_t *queue)
{
    return queue->quantidade;
}
