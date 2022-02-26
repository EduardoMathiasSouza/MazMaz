//GRR20190428 Eduardo Mathias de Souza 
#include "queue.h"
#include <stdio.h>

int queue_size (queue_t *queue){
    int tamanho = 0;//Inicializa tamanho como 0
    queue_t *aux;
    if(queue != NULL){//Caso a fila nao seja vazia						
        aux = queue;
	do{
	    tamanho += 1;
	    aux = aux->next;}
	while(aux != queue);//Enquanto nao chegarmos ao final da fila, aumentamos tamanho em 1 e a variavel auxiliar que estamos usando recebe o prox elem. da fila
}
    return tamanho;//Retorna tamanho
}

void queue_print (char *name, queue_t *queue, void print_elem (void*) ){
    queue_t *aux;
    printf("%s: [", name);
    if (queue != NULL) {//Se a fila nao for vazia
        print_elem(queue);//Imprime o primeiro elemento da fila
        printf(" ");
        aux = queue->next;//Aux recebe o proximo elemento
        while (aux != queue){//Enquanto nao chegamos ao fim da fila
            print_elem(aux);
            aux = aux->next;
            printf(" ");
        }
    }
    printf("]\n");}//Caso a fila seja vazia, imprime-se apenas []

int queue_append(queue_t **queue, queue_t *elem){
    if(elem->prev != NULL || elem->next != NULL){//Caso o elemento pertenca a outra fila,
        fprintf(stderr,"O elemento nao deve estar em outra fila\n");
        return -1;}
    if(!elem){//Caso nao exista elemento
        fprintf(stderr,"o elemento deve existir\n");
        return -1;
    }
    if(!queue){//Caso nao exista fila
        fprintf(stderr,"a fila deve existir\n");
        return -1;
    }
    if (*queue == NULL)//Se a fila eh vazia
    {
        elem->next = elem->prev = elem;
        *queue = elem;
        return 0;
    }//Caso a fila nao seja vazia
    queue_t *ultimo = (*queue)->prev;
    elem->next = *queue;
    (*queue)->prev = elem;
    elem->prev = ultimo;
    ultimo->next = elem;
    return 0;
}

int queue_remove (queue_t **queue, queue_t *elem){
    if(!elem){//Caso nao exista elemento
        fprintf(stderr,"o elemento deve existir\n");
        return -1;
    }
    if(!queue){//Caso nao exista fila
        fprintf(stderr,"a fila deve existir\n");
        return -1;
    }
    if (*queue == NULL)//Caso a fila eh vazia
    {
       fprintf(stderr,"a fila não deve estar vazia\n");
       return -1;
    }
    queue_t *aux = *queue;
    queue_t *elemaux;
    int tamanho_fila = queue_size(*queue);//tamanho_fila recebe o tamanho da fila
    int flagNaFila = 0;//Flag para guardar se o elemento que queremos remover esta na fila
    for(int i= 0; i < tamanho_fila; i++){//Enquanto nao chegamos ao fim da fila, analisamos se o elemento se localiza na mesma
        if(elem == aux){
            flagNaFila = 1;//Caso sim, flag recebe 1
            elemaux = aux;}//Elemaux recebe esse elemento na fila 
        aux = aux->next;
    }

    if(!flagNaFila){//Caso o elemento nao pertenca a fila
        fprintf(stderr,"o elemento deve pertencer a fila indicada\n");
        return -1;
    }

    if(elemaux == *queue){//Se o elemento a ser removido for o primeiro da fila
        if(elemaux->next == elemaux){//Se o elemento for o primeiro e unico na fila
            elemaux->next = NULL;
            elemaux->prev = NULL;
            *queue = NULL;
            return 0;
        }
        *queue = elemaux->next;//Caso nao seja o unico na fila
    }
    elemaux->prev->next = elemaux->next;
    elemaux->next->prev = elemaux->prev;
    elemaux->next = NULL;
    elemaux->prev = NULL;
    return 0;
}
