/* 
 * File:   functions.h
 * Author: Spiros Delviniotis
 *
 * Created on 28 June 2014, 6:47 μμ
 */


typedef struct list_node node;
typedef struct list_node *nodePtr;
typedef struct list_type list;
typedef struct list_type *listPtr;


struct list_node{
    int socket_id;  /* node ID */
    char *path;  /* name of job */
    pthread_mutex_t *mtx_client_ptr;  /* mutex of client */
    nodePtr next;  /* Ptr to next node */
};

struct list_type{
    nodePtr head;  /* Head of list */
    nodePtr last;  /* Last of list */
    int counter;  /* Counter of nodes in list */
};

int create_list(listPtr*);  /* create list */
int insert_node(listPtr* ,int , char*, pthread_mutex_t*);  /* insert node to FIFO */
void print_list(listPtr* );  /* print list */
void delete_node(listPtr* );  /* delete list node */
void destroy_list(listPtr* );  /* destroy list */

