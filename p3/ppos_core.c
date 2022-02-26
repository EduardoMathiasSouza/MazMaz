// operating system check
#if defined(_WIN32) || (!defined(__unix__) && !defined(__unix) && (!defined(__APPLE__) || !defined(__MACH__)))
#warning Este codigo foi planejado para ambientes UNIX (LInux, *BSD, MacOS). A compilacao e execucao em outros ambientes e responsabilidade do usuario.
#endif

#define _XOPEN_SOURCE 600	/* para compilar no MacOS */
#include "ppos_data.h"
#include "ppos.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>


#define STACKSIZE 64*1024

task_t Main;
task_t *TarefaAtual, *TarefaAnterior;
task_t dispatcher;
task_t *filaTarefas; 
int count = 0; 

task_t* scheduler(){
    return filaTarefas;
}

void dispatcher_body (){
    task_t *next;
    while ( queue_size((queue_t*)filaTarefas) > 0 )
    {
        next = scheduler();
        if (next)
        {
            task_switch (next) ; // transfere controle para a tarefa "next"
        }
    }
    task_exit(0) ; // encerra a tarefa dispatcher
}


void ppos_init (){
    setvbuf (stdout, 0, _IONBF, 0);
    TarefaAtual = &Main;
    Main.id = 0;

    /*Cria dispatcher*/
    task_create(&dispatcher, dispatcher_body, NULL);

    filaTarefas = NULL;
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
      perror ("Erro na criação da pilha: ") ;
      return -1;
   }
   makecontext(&task->context,(void*)start_routine, 1, (char*) arg);
   if(task != &dispatcher)
        queue_append((queue_t**) &filaTarefas, (queue_t*) task);

   task->id = count;

    #ifdef DEBUG
    printf("Criou tarefa: %d\n", task->id);
    #endif

    return task->id;
}

int task_switch (task_t *task){
    TarefaAnterior = TarefaAtual;
    TarefaAtual = task;

    if(swapcontext(&TarefaAnterior->context, &task->context) < 0)
        return -1;
        
    #ifdef DEBUG
    printf("Trocou contexto De %d Para %d\n", TarefaAnterior->id, task->id);
    #endif

    return 0;
}

void task_exit (int exit_code){
    if(TarefaAtual != &dispatcher && TarefaAtual != &Main){
        queue_remove((queue_t**) &filaTarefas, (queue_t*) TarefaAtual);
        task_switch(&dispatcher);
    }

    /*Caso a tarefa a ser finalizada seja o dispatcher, troca para a tarefa main*/
    else
        task_switch(&Main);
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
