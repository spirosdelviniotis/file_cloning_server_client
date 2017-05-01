/* 
 * File:   functions.c
 * Author: Spiros Delviniotis
 *
 * Created on 28 June 2014, 6:47 μμ
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "functions.h"

int insert_node(listPtr* lista, int socket, char *file_path, pthread_mutex_t *mtx)
{
    nodePtr temp_ptr = NULL;
    temp_ptr = (nodePtr)malloc(sizeof(node));
    if (temp_ptr == NULL)
    {
        perror("insert_node: malloc node");
        exit(EXIT_FAILURE);
    }
    
    temp_ptr->socket_id = socket;
    temp_ptr->next = NULL;
    temp_ptr->mtx_client_ptr = mtx;
    temp_ptr->path = (char*)malloc(sizeof(char)*strlen(file_path));
    if (temp_ptr->path == NULL)
    {
        perror("insert_node: malloc path");
        exit(EXIT_FAILURE);
    }
    
    strcpy(temp_ptr->path, file_path);
    
    if ((*lista)->head == NULL)  //empty
    {
        (*lista)->head = temp_ptr;
        (*lista)->last = temp_ptr;
    }
    else
    {
        (*lista)->last->next = temp_ptr;
        (*lista)->last = temp_ptr;
    }
    
    (*lista)->counter = (*lista)->counter + 1;
    
    printf("    insert_node: NODE INSERTED.\n");
    
    return 0;
}


void print_list(listPtr* lista)
{
    int i;
    nodePtr temp_ptr = NULL;
    
    if ((*lista)->head != NULL)
    {
        temp_ptr = (*lista)->head; 
    }
    else
    {
        printf("    print_list: EMPTY LIST\n");
        return;
    }
    
    while (temp_ptr->next != NULL)  //while next != NULL
    {
        printf("    print_list: PATH = %s\n",temp_ptr->path);
        
        temp_ptr = temp_ptr->next;
    }
    printf("    print_list: ->PATH = %s\n",temp_ptr->path);
    
    return;
}


void delete_node(listPtr* lista)
{
    nodePtr temp_ptr = NULL;
    
    temp_ptr = (*lista)->head;
    
    (*lista)->head = temp_ptr->next;
    free(temp_ptr->path);
    free(temp_ptr);
    
    (*lista)->counter = (*lista)->counter - 1;
    if ((*lista)->counter == 0)
    {
        printf("    delete_node: LIST DELETED.\n");
    }
    else
    {
        printf("    delete_node: NODE DELETED.\n");
    }
    
    return;
}


int create_list(listPtr* lista)
{
    (*lista) = (listPtr)malloc(sizeof(list));
    if (*lista == NULL)
    {
        perror("create_list: malloc");
        exit(EXIT_FAILURE);
    }
    
    (*lista)->counter = 0;
    (*lista)->head = NULL;
    
    printf("    create_list: CREATED.\n");
    
    return 0;
}


void destroy_list(listPtr* lista)
{
    int i = 0;
    
    while ((*lista)->head != NULL)
    {
        delete_node(lista);
    }
    
    free(*lista);
    
    printf("    destroy_list: DESTROIED.\n");
    
    return;
}


