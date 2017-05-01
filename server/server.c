/* 
 * File:   server.c
 * Author: Spiros Delviniotis
 *
 * Created on 28 June 2014, 6:46 μμ
 */

#include <stdio.h>
#include <sys/param.h>
#include <arpa/inet.h>	
#include <sys/wait.h>	     
#include <sys/types.h>	     	
#include <sys/socket.h>	     
#include <sys/stat.h>
#include <netinet/in.h>	     	
#include <netdb.h>
#include <unistd.h>	
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <pthread.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>

#include "functions.h"


/* Global variables */
int thread_pool_size;
int working_state_flag;
int queue_size;
listPtr job_queue;  /* queued list jobs */
pthread_mutex_t mtx_pool;
pthread_cond_t cond_nonempty;
pthread_cond_t cond_nonfull;

int write_all( int fd , void *buff , size_t size) 
{
    int sent, n;
    for ( sent = 0; (unsigned)sent < size ; sent += n )
    {
        n = write( fd ,(char*)buff + sent , size - sent);
        if (n == -1)
        {
            printf("Server Error: write_all\n");
            return -1;
        }
        
        printf("     =================================== WRITE> %d\n",n);
    }
	
    return n;
}

int read_all( int fd, void *buff, size_t size)
{
    int read_=0;
    int n;
    
    for ( read_ = 0; (unsigned)read_ < size ; read_ += n )
    {
        n = read( fd ,(char*)buff + read_ , size - read_);
        if (n == -1)
        {
            printf("Server Error: read_all\n");
            return -1;
        }
        
        if (n == 0)  //time out
        {
            printf("Server Error: read_all: time out\n");
            return -1;
        }
        
        printf("     =================================== READ> %d\n",n);
    }
    
    //printf("=================================== READ> %d\n",n);
    
    return n;
}


int get_files(char *pth, int socket, int make_job_queue_flag, pthread_mutex_t *mtx)  
/*
	returns: -1(fail) , 0(queue state), file counter(>0))
*/
{
    char path[1000];
    int status;
    static int file_counter = 0;
    DIR *dp;
    struct dirent *result;
    struct dirent files;
    
    strcpy(path,pth);
    
    dp = opendir(path);
    if (dp == NULL)
    {
        perror("Get_files Error: opendir\n");
        return -1;
    }
        
    char newp[1000];
    struct stat buf;
    readdir_r(dp, &files, &result);
    while ( result != NULL)
    {
        if (!strcmp(files.d_name,".") || !strcmp(files.d_name,".."))
        {
            readdir_r(dp, &files, &result);
            continue;
        }

        strcpy(newp,path);
        strcat(newp,"/");
        strcat(newp,files.d_name); 
        
        //stat function returns a structure of information about the file    
        if (stat(newp,&buf) == -1)
        {
            perror("Get_files Error: stat");
            return -1;
        }
        
        if (S_ISDIR(buf.st_mode)) // if directory, then add a "/" to current path
        {
            strcat(path,"/");
            strcat(path,files.d_name);
            
            if (get_files(path, socket, make_job_queue_flag, mtx) == -1)
            {
                printf("Get_files Error: get_files( ANADROMI )\n");
                return -1;
            }
            
            strcpy(path,pth);
        }
        else
        {
            printf("%s\n",newp);
            
            if (make_job_queue_flag == 1) // Critical section
            {
                /******************** -- Critical -- **************************/
                pthread_mutex_lock( &mtx_pool );
                while (job_queue->counter >= queue_size ) 
                {
                    printf (" >> Found QUEUE Full.\n");
                    pthread_cond_wait(&cond_nonfull , &mtx_pool );
                }

                /* insert at queue */
                status = insert_node(&job_queue, socket , newp, mtx);
                if (status > 0)
                {
                    printf("Get_files Error: insert node in queue\n");
                    return -1;
                }

                pthread_mutex_unlock(&mtx_pool );
                
                pthread_cond_signal( &cond_nonempty );//producer
                /**************************************************************/
            }
            else
            {
                file_counter++;
            }
            
        }
        
        readdir_r(dp, &files, &result);
    }
	
    return file_counter;
}


/* Thread for serving each client */
void *worker_thread()
{
    int temp_return_value = -1;
    int file_descriptor;
    int blocks=0;
    int i;
    int read_bytes;
    int size_path;
    int rest_file;  //rest of file in bytes
    int j;
    int sock;
    long size_of_block = sysconf(_SC_PAGESIZE);
    long size;
    char *read_buffer;
    char *job_path;
    pthread_mutex_t *mtx;
    struct stat st;
    
    printf("Pool Thread[%ld]: Created.\n",pthread_self());
    
    /* Allocate memory */
    read_buffer = (char*)malloc(sizeof(char)*size_of_block);
    if (read_buffer == NULL)
    {
        perror("Server Error (WORKER): malloc read_buffer");
        return(NULL);
    }
    
    while (job_queue->counter > 0 || working_state_flag>0) //I have jobs in queue
    {
        /********************** Get node from Queue(obtain) *******************/
        /**************************** -- Critical -- **************************/
        pthread_mutex_lock(&mtx_pool );

        while (job_queue->counter <= 0) 
        {
            printf(" >> Found QUEUE Empty \n");
            pthread_cond_wait(&cond_nonempty , &mtx_pool );
        }

        /* Get DATA */
        sock = job_queue->head->socket_id;
        size_path = strlen(job_queue->head->path);
        mtx = job_queue->head->mtx_client_ptr;

        job_path = (char*)malloc(sizeof(char)*size_path);
        if (job_path == NULL)
        {
            perror("Server Error (WORKER): malloc job_path");
            return(NULL);
        }
        strcpy(job_path, job_queue->head->path);
        
        /* Delete node from queue */
        delete_node(&job_queue);

        pthread_mutex_unlock(&mtx_pool );
        /**********************************************************************/
        
        /* lock SENDING MUTEX to write */
        pthread_mutex_lock( mtx );
        printf("Pool Thread[%ld]: mutex LOCKED\n",pthread_self());
        
        /* Open file to copy data to socket */
        file_descriptor = open(job_path , O_RDONLY);
        if (file_descriptor < 0)
        {
            perror("Server Error (WORKER): open file");
            return(NULL);
        }

        /* get size of file */
        stat(job_path, &st);
        size = st.st_size;

        blocks = size/size_of_block;

        if (blocks == 0)  //data < 4096 bytes
        {
            blocks = 1;
            rest_file = size;
        }
        else
        {
            rest_file = size - blocks*size_of_block;
            if (rest_file > 0)
            {
                blocks++;
            }
        }
        
        printf("Server (WORKER): file> \"%s\" ==== size> %ld ==== delivering in %d blocks.\n", job_path, size, blocks);

        /* write to socket metadata and data */
        temp_return_value = write_all(sock, &blocks, sizeof(int));
        if (temp_return_value < 0)  //number of blocks
        {
            perror("Server Error (WORKER): write size_of_block in socket");
            return(NULL);
        }
        printf("Server (WORKER): blocks-> %d\n",blocks);

        
        if (temp_return_value = write_all(sock, &size_path, sizeof(int)) < 0)  //path size
        {
            perror("Server Error (WORKER): write size_path in socket");
            return(NULL);
        }

        if (temp_return_value = write_all(sock, job_path, strlen(job_path)) < 0)  //path
        {
            perror("Server Error (WORKER): write path in socket");
            return(NULL);
        }

        if (temp_return_value = write_all(sock, &st.st_mode, sizeof(int)) < 0)  //permissions
        {
            perror("Server Error (WORKER): write permissions in socket");
            return(NULL);
        }

        for (i=0; i<blocks; i++)
        {
            read_bytes = 0;

            /* Read 1 block from file */
            read_bytes = read(file_descriptor, read_buffer, size_of_block);
            if (read_bytes < 0)
            {
                perror("Server Error (WORKER): read file to copy");
                return(NULL);
            }
            printf("Server (WORKER): Will be transfered %d bytes.\n",read_bytes);

            /* write size of read */
            if (temp_return_value = write_all(sock, &read_bytes, sizeof(int)) < 0)
            {
                perror("Server Error (WORKER): write read_bytes in socket");
                return(NULL);
            }

            /* write 1 block to socket */
            if (temp_return_value = write_all(sock, read_buffer, read_bytes) < 0)
            {
                perror("Server Error (WORKER): write file in socket");
                return(NULL);
            }
        }
        
        /* delete from queue */
        close(file_descriptor);
    
        /* Unlock SENDING MUTEX */
        pthread_mutex_unlock( mtx );
        printf("Pool Thread[%ld]: mutex UN-LOCKED\n",pthread_self());
        
        pthread_cond_signal( &cond_nonfull);
    }
    
    printf("Pool Thread[%ld]: Exiting...\n",pthread_self());
    
    pthread_exit(NULL);
}


/* Thread for serving each client */
void *client_handler(void *socket_fd)
{
    int sock = *(int*)socket_fd;
    int lenth = 0;
    int temp_return_value = -1;
    int file_counter = 0;
    char *buffer;
    char end_message[3];
    pthread_mutex_t *mtx_client;  //mutex of client
    
    printf("Thread Client[%ld]: Created.\n",pthread_self());
    
    if (pthread_detach(pthread_self()) < 0)
    {
	perror("pthread_detach");
        return(NULL);
    }
    
    /* Allocate memory for mutex */
    mtx_client = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if (mtx_client == NULL)
    {
        printf("mutex init failed\n");
        return(NULL);
    }
    
    /* Initialize mutex client */
    if (pthread_mutex_init(mtx_client, NULL) != 0)
    {
        printf("mutex init failed\n");
        return(NULL);
    }
    
    /* Read lenth of string */
    if (read_all(sock, &lenth, sizeof(int)) < 0)
    {
        perror("Server: read lenth");
        return(NULL);
    }
    printf("Server: lenth= %d\n",lenth);
    
    /* Reply */
    if (temp_return_value = write_all(sock,"OK_1", strlen("OK_1")) < 0)
    {
        perror("Server: write OK_1");
        return(NULL);
    }
    
    /* Reserve memory for buffer */
    buffer = (char*)malloc(lenth*sizeof(char));
    if (buffer == NULL)
    {
        perror("Server: malloc buffer");
        return(NULL);
    }
    
    if (read_all(sock, buffer, lenth) < 0) 
    {  
        perror("Server: read directory");
        return(NULL);
    }
    printf("Server: directory = %s\n",buffer);
   
    /* Reply */
    if (temp_return_value = write_all(sock, "OK_2", strlen("OK_2")) < 0)
    {
        perror("Server: write");
        return(NULL);
    }
    
    /****************************** Place jobs(files) ***************************/
    if ((file_counter = get_files(buffer, sock, 0, mtx_client)) == -1)  //get file counter ONLY!
    {
        printf("Server Error: get_files\n");
        return(NULL);
    }
    printf("Server: JOB COUNTER = %d\n",file_counter);
    
    /* Send file counter to client */
    if (temp_return_value = write_all(sock, &file_counter, sizeof(int)) < 0)  //number of files
    {
        perror("Server: write size_of_block in socket");
        return(NULL);
    }
    
    if ((file_counter = get_files(buffer, sock, 1, mtx_client)) == -1)  //get Queue!
    {
        printf("Server Error: get_files\n");
        return(NULL);
    }
    printf("Server: JOB COUNTER = %d\n",file_counter);  //-2 == Exit Success 
    
    
    
    /* Close Connection */
    if (read_all(sock, end_message, 3*sizeof(char)) < 0) 
    {  
        perror("Server: read directory");
        return(NULL);
    }
    printf("Server: Clossing message = %s\n",end_message);
    close(sock);
    
    /* Destroy Mutex Client */
    pthread_mutex_destroy( mtx_client );
    
    printf("Thread Client[%ld]: Exiting...\n",pthread_self());
    
    pthread_exit(NULL);
}


int main(int argc, char** argv) {
    int port;
    int sock;
    int newsock;
    int status;
    int i;
    int err;
    struct sockaddr_in server, client;
    socklen_t clientlen;
    struct sockaddr *serverptr = (struct sockaddr *)&server;
    struct sockaddr *clientptr = (struct sockaddr *)&client;
    
    pthread_t *pool_threads_ptr;
    
    /* parse arguments */
    if (argc < 6)
    {
        printf("Server Error: few arguments.\n");
        return (EXIT_FAILURE);
    }
    
    if (strcmp(argv[1], "-p") == 0)  //port
    {
        port = atoi(argv[2]);
    }
    else 
    {
        printf("Server Error: wrong port.\n");
        return (EXIT_FAILURE);
    }
    
    if (strcmp(argv[3], "-s") == 0)  //thread_pool_size
    {
        thread_pool_size = atoi(argv[4]);
    }
    else 
    {
        printf("Server Error: wrong thread_pool_size.\n");
        return (EXIT_FAILURE);
    }
    
    if (strcmp(argv[5], "-q") == 0)  //queue_size
    {
        queue_size = atoi(argv[6]);
    }
    else 
    {
        printf("Server Error: wrong queue_size.\n");
        return (EXIT_FAILURE);
    }
    
    /* Print server's parameters */
    printf("Server: Parameters are:\n");
    printf("Server:                 port:   %d\n",port);
    printf("Server:                 thread_pool_size:   %d\n",thread_pool_size);
    printf("Server:                 queue_size:   %d\n",queue_size);
    
    /* Create queue */
    status = create_list(&job_queue);
    if (status == 1)
    {
        printf("Server Error: queue Creation\n");
        return (EXIT_FAILURE);
    }
    
    /* Initialize mutex-cond_var */
    if (pthread_mutex_init(&mtx_pool, NULL) != 0)
    {
        printf("Server Error: mutex init failed\n");
        return 1;
    }
    pthread_cond_init(&cond_nonempty , 0);
    pthread_cond_init(&cond_nonfull , 0);
    
    /* Create thread pool */
    working_state_flag = 1;  //set->WORKING STATE in order not to terminate pool threads
    pool_threads_ptr = (pthread_t*)malloc(sizeof(pthread_t)*thread_pool_size);
    if (pool_threads_ptr == NULL)
    {
        perror("Server Error: malloc pool threads");
        return (EXIT_FAILURE);
    }
    
    for (i=0; i<thread_pool_size; i++)
    {
        err = pthread_create(&pool_threads_ptr[i] , NULL , worker_thread , 0 );
        if (err != 0)
        {
            printf("Server Error: Can't create thread :[%s]", strerror(err));
        }
    }
    
    /* Create socket */
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Server Error: socket create");
        return (EXIT_FAILURE);
    }
    
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
    
    /* Bind socket to address */
    if (bind(sock, serverptr, sizeof(server)) < 0)
    {
        perror("Server Error: bind socket");
        return (EXIT_FAILURE);
    }
    
    /* Listen for connections */
    if (listen(sock, 5) < 0)  //  5 must be changed to have many connections
    {
        perror("Server Error: listen for connections");
        return (EXIT_FAILURE);
    }
    printf("Server: Listening for connections to port: %d\n",port);
    
    int *newsock_ptr;
    while (1)
    {
        /* initialize */
        clientlen = sizeof(*clientptr);
        pthread_t thread_client;
        newsock_ptr = malloc(1);
        
        /* Accept connection */
        newsock = accept(sock, clientptr, &clientlen);
        if (newsock < 0)
        {
            perror("Server Error: Accept connection");
            return (EXIT_FAILURE);
        }
        *newsock_ptr = newsock;
        
        /* Create thread client */
        err = pthread_create(&thread_client , NULL , client_handler , (void*)newsock_ptr );
        if (err != 0)
        {
            printf("Server Error: Can't create thread :[%s]", strerror(err));
        }        
    }
    
    /* Free resources */
    destroy_list(&job_queue);
    
    /* Join pool threads */
    for (i=0; i<thread_pool_size; i++)
    {
        pthread_join(pool_threads_ptr[i] , 0);
    }
    
    /* Destroy Mutexes-Cond_Var */
    pthread_cond_destroy( &cond_nonempty );
    pthread_cond_destroy( &cond_nonfull );
    pthread_mutex_destroy( &mtx_pool );

    return (EXIT_SUCCESS);
}

