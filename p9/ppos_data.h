//GRR20190428 Eduardo Mathias de Souza 
// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.4 -- Janeiro de 2022

// Estruturas de dados internas do sistema operacional

#ifndef __PPOS_DATA__
#define __PPOS_DATA__

#include <ucontext.h>		// biblioteca POSIX de trocas de contexto

// Estrutura que define um Task Control Block (TCB)
typedef struct task_t
{
  struct task_t *prev, *next ;		// ponteiros para usar em filas
  int id ;				// identificador da tarefa
  ucontext_t context ;
  int prioridadeEstatica;
  int prioridadeDinamica;
  int contadorQuantumTarefa;   
  int tipoTarefa; //(0) tarefa do Sistema; (1) tarefa do Usuario
  unsigned int tempoExecucao;
  unsigned int tempoProcessamento;  
  unsigned int ativacoes; //Quantas vezes tarefa foi ativada
  short status ;			// suspensa(0), pronta(1), rodando(2), finalizada (3)
  short preemptable ;			// pode ser preemptada?
  struct task_t *suspendedQueue;   //Fila de tarefas suspendidas por uma tarefa
  int exitCode;                    //Código de finalização da tarefa recebido na função task_exit
  int sleepTime;                   //Tarefa vai dormir por quanto tempo
} task_t ;

// estrutura que define um semáforo
typedef struct
{
  // preencher quando necessário
} semaphore_t ;

// estrutura que define um mutex
typedef struct
{
  // preencher quando necessário
} mutex_t ;

// estrutura que define uma barreira
typedef struct
{
  // preencher quando necessário
} barrier_t ;

// estrutura que define uma fila de mensagens
typedef struct
{
  // preencher quando necessário
} mqueue_t ;

#endif

