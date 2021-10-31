#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <math.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>

#include "packet_queue.h"

#define QUICK_TEST 0

#define QUEUE_LENGTH 5 

//DONE: might need to do some weird stuff like:
//-l<library_name> ex:
//-lm
//needs to be included after code, so
//gcc $CFLAGS.... compare.c -lm
//weird stuff
//more information in lecture nodes
//need to use on the make part that does not include -c
//-c means compile, do not link

//TODO: what if try to set the same key twice? overwrite?
//currenly just gonna overwrite

//TODO: TODO: TODO: where I left off:
//accept needs to return -1 instead of resuming the block
//i could make accept non-blocking with a sleep(1) call but that is lame
//ill save that for if and when i have to
typedef void (*sighandler_t)(int);



void sigint_handler(int sig);
void cleanup();
void* converse_funct(void* void_args);

struct converse_data {
	int connection;
	struct packet_queue* Q;
};
void quick_test();

int kill_server = 0;
struct packet_queue* storage;

int main(int argc, char * argv[]) {
	if(QUICK_TEST) {
		quick_test();
	}
	if(argc != 2) {
		printf("ERROR: INVALID NUMBER OF ARGUMENTS\n");
		abort();
	}
	int port_number = atoi(argv[1]);
	if(port_number == 0 || port_number < 5000 || port_number > 65536) {
		printf("ERROR: INVALID PORT NUMBER\n");
		abort();
	}
	char* port = argv[1];
	struct addrinfo hints;
	struct addrinfo* info_list;
	memset(&hints, 0, sizeof(struct addrinfo)); //0 out bytes
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	//Null because port on this host
	//info_list is a linked list of results
	int error = getaddrinfo(NULL, port, &hints, &info_list);
	if(error != 0) {
		fprintf(stderr, "%s\n", gai_strerror(error));
		printf("Initial getaddrinfo failed\n");
		exit(EXIT_FAILURE);
	}
	struct addrinfo* info;
	int listener;
	//we are gonna try all connection methods here
	for(info = info_list; info != NULL; info = info->ai_next) {
		listener = socket(info->ai_family, info->ai_socktype, info->ai_protocol);

		if (listener < 0) continue;

		if( bind(listener, info->ai_addr, info->ai_addrlen) != 0 ) {
			//we failed
			close(listener);
			continue;
		}

		if( listen(listener, QUEUE_LENGTH) != 0 ) {
			//we failed
			close(listener);
			continue;
		}
		//neither of the above failed, so we can break
		break;

	}
	

	if(info == NULL) {
		printf("server failed to listen on that port number\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(info_list);

	atexit(cleanup);

	storage = packet_queue_init();

	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction)); //0 out bytes
	sa.sa_handler = sigint_handler;
	sigemptyset(&sa.sa_mask);

	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);

	if(sigaction(SIGINT, &sa, NULL) == -1) {
		printf("signal action failed to change\n");
		exit(EXIT_FAILURE);
	}

	while(!kill_server) {
		//blocks until a connection is recieved
		int connection = accept(listener, NULL, NULL);
		if(connection < 0) {
			break;
		} else {
			printf("connection was recieved.\n");
		}

		pthread_t tid;


		struct converse_data* args = malloc(sizeof(struct converse_data));
		args->connection = connection;
		args->Q = storage;

		pthread_sigmask(SIG_BLOCK, &set, NULL);
		int ret = pthread_create(&tid, NULL, converse_funct, args);
		pthread_sigmask(SIG_UNBLOCK, &set, NULL);

		ret = ret || pthread_detach(tid);
		if(ret != 0) {
			printf("thread failed to create\n");
			exit(EXIT_FAILURE);
		}
		if(error != 0) {
			printf("Signal masks failed to change\n");
		}
	}
	printf("shutting down server...\n");

	pthread_detach(pthread_self());
	pthread_exit(NULL);
	//will never get where
	return 0;
}

//this is called and works
void cleanup() {
	while(storage->number_of_nodes > 0) {
		packet_pop(storage);
	}
	packet_queue_destroy(storage);
}

//this is called and works
void sigint_handler(int sig) {
	printf("sing int called\n");
	kill_server = 1;
}

void* converse_funct(void* void_args) {
	struct converse_data* args = (struct converse_data*) void_args;

	int connection = args->connection;
	struct packet_queue* storage = args->Q;
	
	FILE* fin = fdopen(dup(connection), "r");
	FILE* fout = fdopen(connection, "w");
	
	//first we have to read in the command
	//commands are GET, SET, DEL
	char* command_set = "SET";
	char* command_del = "DEL";
	char* command_get = "GET";
	//the command that we should be building towards
	char* command_goal;
	//TODO: exit command?
	while(true) {
		char c = getc(fin);

		if(c == command_set[0]) {
			command_goal = command_set;

		} else if ( c == command_del[0]) {
			command_goal = command_del;

		} else if ( c == command_get[0]) {
			command_goal = command_get;

		} else {
			fprintf(fout, "ERR\nBAD\n");
			fflush(fout);
			//TODO: replace me for a thread-specific fail
			//this works?
			fclose(fin);
			fclose(fout);
			free(args);
			return NULL;
		}

		//read in the next two characters
		for(int i = 0; i < 2; i++) {
			c = getc(fin);
			if(command_goal[i+1] != c) {
				fprintf(fout, "ERR\nBAD\n");
				fflush(fout);
				//TODO: replace me for a thread-specific fail
				//this works?
				free(args);
				fclose(fin);
				fclose(fout);
				close(connection);
				return NULL;
			}
		}
		//now the next chracter to be read in has to be a newline
		
		if(getc(fin) != '\n') {
			fprintf(fout, "ERR\nBAD\n");
			fflush(fout);
			//TODO: replace me for a thread-specific fail
			//this works?
			free(args);
			fclose(fin);
			fclose(fout);
			close(connection);
			return NULL;
		}
		//WE CAN READ COMMANDS YAY!!!
		//this reads in the string length
		int string_length;
		int values_written = fscanf(fin, "%d", &string_length);
		if(string_length < 1 || values_written < 1) {
			fprintf(fout, "ERR\nBAD\n");
			fflush(fout);
			//TODO: replace me for a thread-specific fail
			//this works?
			fclose(fin);
			fclose(fout);
			free(args);
			close(connection);
			return NULL;
		}
		//now the next character MUST be \n
		if(getc(fin) != '\n') {
			fprintf(fout, "ERR\nBAD\n");
			fflush(fout);
			//TODO: replace me for a thread-specific fail
			//this works?
			free(args);
			fclose(fin);
			fclose(fout);
			close(connection);
			return NULL;
		}
		//now we read in the payload
		//strlen INCLUDES the \n, so 
		//this uses up twice as much memory as it needs to, oh well
		char* key = malloc(sizeof(char) * string_length);	
		char* value = malloc(sizeof(char) * string_length);
		char* building = key;
		int ptr = 0;
		//if a return has already been seen
		bool returnSeen = false;
		//strlen -1 should be \n
		//this is such shit code =D
		for(int i = 0; i < string_length-1; i++) {
			c = getc(fin);
			if(command_goal == command_set && c == '\n') {
				if(returnSeen){
					fprintf(fout, "ERR\nBAD\n");
					fflush(fout);
					//TODO: replace me for a thread-specific fail
					//this works?
					free(args);
					free(key);
					free(value);
					fclose(fin);
					fclose(fout);
					close(connection);
					return NULL;
				}
				returnSeen = true;
				building[ptr] = '\0';
				building = value;
				ptr = 0;
				continue;
			}
			building[ptr] = c;
			ptr++;
			//too soon for this
			if(c == '\n') {
				fprintf(fout, "ERR\nBAD\n");
				fflush(fout);
				//TODO: replace me for a thread-specific fail
				//this works?
				free(args);
				free(key);
				free(value);
				fclose(fin);
				fclose(fout);
				close(connection);
				return NULL;
			}
		}
		building[ptr] = '\0';

		if(getc(fin) != '\n') {
			fprintf(fout, "ERR\nBAD\n");
			fflush(fout);
			//TODO: replace me for a thread-specific fail
			//this works?
			free(args);
			free(key);
			free(value);
			fclose(fin);
			fclose(fout);
			close(connection);
			return NULL;
		}



		if(command_goal == command_set) {
			/*
			fprintf(fout, "I would insert a node with the values: \n");
			fprintf(fout, "key: %s\n", key);
			fprintf(fout, "value: %s\n", value);
			*/
			if(packet_find(storage, key) != NULL) {
				//this key was already defined
				//We will overwrite the old key
				packet_remove(storage, key);
			}
			packet_append(storage, key,value);
			fprintf(fout, "OKS\n");
		}
		if(command_goal == command_get) {
			/*
			fprintf(fout, "I am going to get the node with the key %s\n", key);
			*/
			char * temp = packet_find(storage, key);
			if(temp == NULL) {
				fprintf(fout, "KNF\n");
			} else {
				fprintf(fout, "OKG\n%ld\n%s\n", strlen(temp) + 1, temp);
			}
		}
		if(command_goal == command_del) {
			/*
			fprintf(fout, "I am going to remove the node with the key %s\n", key);
			*/
			char * temp = packet_find(storage, key);
			if(temp == NULL) {
				fprintf(fout, "KNF\n");
			} else {
				fprintf(fout, "OKG\n%ld\n%s\n", strlen(temp) + 1, temp);
				packet_remove(storage, key);
			}
		}
		fflush(fout);

		//only free if not set
		if(command_goal != command_set) {
			free(key);
			free(value);
		}
	}
	fclose(fin);
	fclose(fout);
	free(args);
	close(connection);
	return NULL;
}

void quick_test() {
	int a = 4;
	int b = 7;
	double c = a/b;
	double d = ((double) a )/b;
	printf("value c: %f,\n", c);
	printf("value d: %f,\n", d);
}
 
