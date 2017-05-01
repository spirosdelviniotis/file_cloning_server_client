/* 
 * File:   client.c
 * Author: Spiros Delviniotis
 *
 * Created on 28 June 2014, 6:47 μμ
 */


#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>


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
    
    return n;
}


int main(int argc, char** argv) {
    int server_port;
    int sock;
    int lenth = 0;
    int i;
    char directory[100];
    char server_ip[100];
    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr *)&server;
    struct hostent *rem;
    
    /* parse arguments */
    if (argc < 6)
    {
        printf("Client Error: few arguments.\n");
        return (EXIT_FAILURE);
    }
    
    if (strcmp(argv[1], "-i") == 0)  //server_ip
    {
        strcpy(server_ip,argv[2]);
    }
    else 
    {
        printf("Client Error: wrong ip.\n");
        return (EXIT_FAILURE);
    }
    
    if (strcmp(argv[3], "-p") == 0)  //server_port
    {
        server_port = atoi(argv[4]);
    }
    else 
    {
        printf("Client Error: wrong server_port.\n");
        return (EXIT_FAILURE);
    }
    
    if (strcmp(argv[5], "-d") == 0)  //directory
    {
        strcpy(directory,argv[6]);
    }
    else 
    {
        printf("Client Error: wrong directory.\n");
        return (EXIT_FAILURE);
    }
    
    /* Print server's parameters */
    printf("Client: Parameters are:\n");
    printf("Client:                 server IP:   %s\n",server_ip);
    printf("Client:                 server port:   %d\n",server_port);
    printf("Client:                 directory:   %s\n",directory);
    
    /* Create socket */
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock <0)
    {
        perror("Client Error: socket create");
        return (EXIT_FAILURE);
    }
    
    rem = gethostbyname(server_ip);
    if (rem == NULL)
    {
        herror("Client Error: gethostbyname");
        return (EXIT_FAILURE);
    }
    
    server.sin_family = AF_INET;
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(server_port);
    
    /* Initiate connection */
    if (connect(sock, serverptr, sizeof(server)) < 0)
    {
        perror("Client Error: connect");
        return (EXIT_FAILURE);
    }
    printf("Client: Connecting to %s port %d\n", server_ip, server_port);
    
    int temp_return_value = -1;
    char buf[4];
    lenth = strlen(directory);
        
    /* Write lenth of directory */
    if (temp_return_value = write_all(sock, &lenth, sizeof(int)) < 0)
    {
        perror("Client Error: write lenth");
        return (EXIT_FAILURE);
    }
        
    /* Read reply */
    if (read_all(sock, buf, 4*sizeof(char)) < 0)
    {
        perror("Client Error: read");
        return (EXIT_FAILURE);
    }
    printf("Client: RESPONCE_1: %s\n",buf);
        
    /* Write directory */
    if (temp_return_value = write_all(sock, directory, lenth) < 0)
    {
        perror("Client Error: write lenth");
        return (EXIT_FAILURE);
    }        
        
    /* Receive reply */
    if (read_all(sock, buf, 4*sizeof(char)) < 0)
    {
        perror("Client Error: read");
        return (EXIT_FAILURE);
    }
    printf("Client: RESPONCE_2: %s\n",buf);
        
   
    /***************************** GET FILE ***********************************/
    long size_of_block = sysconf(_SC_PAGESIZE);
    int n;
    int files = 0;
    int permissions;
    int size_new_path;
    char *read_buffer;
    int file_descriptor;
    int size_read = 0;
    int j,k;  
    int subfolder = 0;
    
    struct stat folder = {0};
    
    /* Create block buffer */
    read_buffer = (char*)malloc(sizeof(char)*size_of_block);
    if (read_buffer == NULL)
    {
        perror("Client Error: malloc buffer");
        return (EXIT_FAILURE);
    }
    
    if (read_all(sock, &files, sizeof(int)) < 0)
    {
        perror("Client Error: read");
        return (EXIT_FAILURE);
    }
    printf("Client: RESPONCE: files = %d\n",files);  
    
    for (j=0; j<files; j++)
    {
        char *new_path;
         
        if (read_all(sock, &n, sizeof(int)) < 0)  //blocks
        {
            perror("Client Error: read");
            return (EXIT_FAILURE);
        }
        printf("Client: RESPONCE_3: n = %d\n",n);    

        if (read_all(sock, &size_new_path, sizeof(int)) < 0)
        {
            perror("Client Error: read");
            return (EXIT_FAILURE);
        }
        printf("Client: RESPONCE_4: size_new_path = %d\n",size_new_path);    

        new_path = (char*)malloc(sizeof(char)*size_new_path);
        if (new_path == NULL)
        {
            perror("Client Error: malloc new_path");
            return (EXIT_FAILURE);
        }

        if (read_all(sock, new_path, size_new_path) < 0)
        {
            perror("Client Error: read");
            return (EXIT_FAILURE);
        }
        new_path[size_new_path] = '\0';  //finallize string
        printf("Client: RESPONCE_5: new_path = %s\n",new_path);

        if (read_all(sock, &permissions, sizeof(int)) < 0)
        {
            perror("Client Error: read");
            return (EXIT_FAILURE);
        }
        printf("Client: RESPONCE_6: permissions = %d\n", permissions); 
        
        /* make file */
        k = 0;   
        while (k < size_new_path)
        {
            if (new_path[k] == '/')
            {
                subfolder++;
                new_path[k] = '\0';  //temporary finallize string
                
                if (stat(new_path, &folder) == -1) // Make folder
                {
                    printf("Client: Making folder: \"%s\".\n",new_path);
                    if (mkdir(new_path, 0700) < 0)
                    {
                        printf("Client Error: Makking folder");
                        return (EXIT_FAILURE);
                    }
                }
                else
                {
                    printf("Client: Folder \"%s\" Exists.\n", new_path); 
                }
                
                new_path[k] = '/';  //revert string to previous state
            }
            
            k++;
        }
            
            
        file_descriptor = open(new_path, O_RDWR|O_CREAT, permissions);
        if (file_descriptor < 0)
        {
            perror("Client Error: create new file");
            return (EXIT_FAILURE);
        }

        free(new_path);

        for (i=0; i<n; i++)
        {
            if (read_all(sock, &size_read, sizeof(int)) < 0)
            {
                perror("Client Error: read size_read");
                return (EXIT_FAILURE);
            }
            printf("Client: RESPONCE read_bytes = %d\n", size_read);

            if (read_all(sock, read_buffer, size_read) < 0)
            {
                perror("Client Error: read data");
                return (EXIT_FAILURE);
            }

            if (temp_return_value = write_all(file_descriptor, read_buffer, size_read) < 0)
            {
                perror("Client Error: write new file");
                return (EXIT_FAILURE);
            }
        }
        
        close(file_descriptor);
        
    }
    /***************************** GET FILE END ***********************************/
    printf("Client: Trying to send END status...\n");
    if (temp_return_value = write_all(sock, "END", strlen("END")) < 0)
    {
        perror("Client Error: write Close message");
        return (EXIT_FAILURE);
    }
    printf("Client: CLOSE CONNECTION.\n");
    
    /* Close socket and exit */
    close(sock);

    return (EXIT_SUCCESS);
}

