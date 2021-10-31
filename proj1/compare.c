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

#include "gen_queue.h"

#define DEBUG_INPUT 0
#define DEBUG_DIR_FUNC 0
#define DEBUG_FILE_FUNC 0
#define DEBUG_THREAD_WAITING 0
#define DEBUG_ANAL_FUNC 0
#define QUICK_TEST 0

#define BUFFER_SIZE 50

//TODO:Much testing

//DONE: REWRITE INPUT PARSER TO BE MORE ROBUST ACCORDING TO PROJECT WRITEUP
//DONE: print output in a proper order

  //this seems to work but i want more testing

//DONE: ERROR CONDITION IF less then 2 FILES DISCOVERED
//DONE: might need to do some weird stuff like:
//-l<library_name> ex:
//-lm
//needs to be included after code, so
//gcc $CFLAGS.... compare.c -lm
//weird stuff
//more information in lecture nodes
//need to use on the make part that does not include -c
//-c means compile, do not link


//define any structs 
struct JSD_data {
  char* fileName1;
  char* fileName2;
  double JSD_value;
  int wordCountCombined;
};


int compareJSD(const void * a, const void * b)
{
  struct JSD_data * obj1 = *((struct JSD_data **) a);
  struct JSD_data * obj2 = *((struct JSD_data **) b);
  

  if (obj1->wordCountCombined < obj2->wordCountCombined) return 1;
  if (obj1->wordCountCombined > obj2->wordCountCombined) return -1;
  return 0;
}





struct analArgs {
  struct WFD_data** WFD_arr;
  struct gen_queue* tasks_q;
  struct gen_queue* JSD_q;
  int* waitingCount;
  bool* killCommand;
};

struct anal_task {
  int file1;
  int file2;
};



struct argumentInformation {
  int directoryThreads;
  int fileThreads;
  int analysisThreads;
  bool freeFileExtension;
  char* fileExtension;
  //ONLY FREE THE FIRST FILENAMES *, the rest are from argv
  char** fileNames; //can be files or directories
  int fileCount;
};
//used by dirfunc and filefunc
struct explorerArguments {
  int* waitingCount;
  bool* killCommand;
  struct gen_queue * dir_q;
  struct gen_queue * file_q;
  struct gen_queue * WFD_q;
  char* fileExtension; //file func does not use this but whatever
};

struct word_freq_data{
  char* wordName;
  int wordCount;
};

struct WFD_data {
  char* fileName;
  struct gen_queue* words;
  int totalWords;
};

//parses command line arguments into the struct arugment information
void parseArguments(int argc, char ** argv, struct argumentInformation * argInfo);
//organises the files contained in argumentInformation into a dirQ or a fileQ
void sortInputFiles(struct argumentInformation* argInfo, 
                    struct gen_queue* dir_q, 
                    struct gen_queue * file_q
                    );

void * directoryFunc ( void* void_args);
void * fileFunc(void* void_args);
void * analFunc(void* void_args);

void insertWord(char * word, struct gen_queue* words);
//used to quickly test things
void quick_test();

int main(int argc, char * argv[]) {

  if(QUICK_TEST){
    quick_test();
    return EXIT_SUCCESS;
  }
  struct argumentInformation argInfo;
  //remember to dealloc
  argInfo.fileNames = (char**)malloc(sizeof(char*) * argc);

  parseArguments(argc, argv, &argInfo);

  struct gen_queue* dir_q = gen_queue_init(); //contains char* file names
  struct gen_queue* file_q = gen_queue_init(); //contains char* file names
  struct gen_queue* WFD_q = gen_queue_init(); //contains struct WFD_data* for a file

  //initializing arguments for the directory function
  bool killDirFunc = false;
  int waitingCountDirFunc = 0;
  struct explorerArguments* dirFuncArgs = malloc(sizeof(struct explorerArguments) * argInfo.directoryThreads);
  pthread_t* dirTid = malloc(sizeof(pthread_t) * argInfo.directoryThreads);

  //initializing arguments for the file function
  bool killFileFunc = false;
  int waitingCountFileFunc = 0;
  struct explorerArguments* fileFuncArgs = malloc(sizeof(struct explorerArguments) * argInfo.fileThreads);
  pthread_t* fileTid = malloc(sizeof(pthread_t) * argInfo.fileThreads);

  //creates all of the directory threads
  for (int i = 0; i < argInfo.directoryThreads; i++){
    struct explorerArguments* tempArgs = &(dirFuncArgs[i]);
    tempArgs->waitingCount = &waitingCountDirFunc;
    tempArgs->dir_q = dir_q;
    tempArgs->file_q = file_q;
    tempArgs->killCommand = &killDirFunc;
    tempArgs->fileExtension = argInfo.fileExtension;
    int ret = pthread_create(&(dirTid[i]), NULL, directoryFunc, tempArgs);
    if(ret !=0){
      printf("directory thread failed to create, quitting");
      abort();
    }
  }

  //creates all of the file threads
  for(int i = 0; i < argInfo.fileThreads; i++){
    struct explorerArguments* tempArgs = &(fileFuncArgs[i]);
    tempArgs->waitingCount = &waitingCountFileFunc;
    tempArgs->file_q = file_q;
    tempArgs->WFD_q = WFD_q;
    tempArgs->killCommand = &killFileFunc;
    int ret = pthread_create(&(fileTid[i]), NULL, fileFunc, tempArgs);
    if(ret !=0){
      printf("file thread failed to create, quitting");
      abort();
    }
  }

  //start input shit after starting the threads
  sortInputFiles(&argInfo, dir_q, file_q);

  //we are new finished with the argInfo file list
  free(argInfo.fileNames);


  //collects all of the directory threads
  while(true){
    sleep(1);
    if(DEBUG_THREAD_WAITING){
      printf("waitingCountValue: %i\n", waitingCountDirFunc);
    }
    if(waitingCountDirFunc == argInfo.directoryThreads){
      killDirFunc = true;
      for(int i = 0; i < argInfo.directoryThreads; i++){
        pthread_cond_signal(&(dir_q->pop_ready) );
      }
      
      break;
    }
  }
  
  for(int i = 0; i < argInfo.directoryThreads; i++){
    pthread_join(dirTid[i], NULL);
  }

  //collects all of the file threads
  while(true){
    sleep(1);
    if(DEBUG_THREAD_WAITING){
      printf("waitingCountValue: %i\n", waitingCountFileFunc);
    }
    if(waitingCountFileFunc == argInfo.fileThreads){
      killFileFunc = true;
      for(int i = 0; i < argInfo.fileThreads; i++){
        pthread_cond_signal(&(file_q->pop_ready) );
      }
      
      break;
    }
  }

  for(int i = 0; i < argInfo.fileThreads; i++){
    pthread_join(fileTid[i], NULL);
  }
  //free funct and dir arguements
  free(dirTid);
  free(dirFuncArgs);
  free(fileTid);
  free(fileFuncArgs); 

  //clean up the gen_q's
  
  gen_queue_destroy(dir_q);
  gen_queue_destroy(file_q);
  //dealloc some old argInfo stuff
  if(argInfo.freeFileExtension){
    free(argInfo.fileExtension);
  }

  //quick testing:
  //this might be usefull for deallocation at the end
  /*
  while(!gen_is_empty(WFD_q)){
    struct WFD_data* ptr = gen_pop(WFD_q, NULL, NULL);
    printf("WFD for file %s:\n", ptr->fileName);
    printf("Total words: %i\n", ptr->totalWords);
    while(!gen_is_empty(ptr->words)){
      struct word_freq_data* temp = gen_pop(ptr->words, NULL, NULL);
      printf("%s, with a freq: %i\n", temp->wordName, temp->wordCount);
      free(temp->wordName);
      free(temp);
    }
    gen_queue_destroy(ptr->words);
    free(ptr->fileName);
    free(ptr);
    printf("-----------------------------------------\n");
  }
  */
  //-----------------------------------------------------------------------------
  //analysis threads starts here
  if(WFD_q->number_of_nodes < 2) {
    printf("ERROR: less than 2 files discovered\n");
    exit(1);
  }

  //This transforms the WFD_q into a WFD_arr
  int WFD_arr_size = WFD_q->number_of_nodes;
  struct WFD_data** WFD_arr = malloc(sizeof(struct WFD_data*)* WFD_arr_size);
  for(int i = 0; i < WFD_arr_size; i++) {
    WFD_arr[i] = gen_pop(WFD_q, NULL,NULL);
  }
  gen_queue_destroy(WFD_q);

  struct gen_queue* tasks_q = gen_queue_init();
  for(int i = 0; i < WFD_arr_size; i++){
    for(int j = i + 1; j < WFD_arr_size; j++ ){
      struct anal_task * temp = malloc(sizeof(struct anal_task));
      temp->file1 = i;
      temp->file2 = j;
      gen_append(tasks_q, temp);
    }
  }
  struct gen_queue* JSD_q = gen_queue_init();

  bool killAnalFunc = false;
  int waitingCountAnalFunc = 0;
  struct analArgs* analFuncArgs = malloc(sizeof(struct analArgs) * argInfo.analysisThreads);
  pthread_t* analTid = malloc(sizeof(pthread_t) * argInfo.analysisThreads);

  //creates all of the anal threads
  for(int i = 0; i < argInfo.analysisThreads; i++){
    struct analArgs* tempArgs = &(analFuncArgs[i]);
    tempArgs->waitingCount = &waitingCountAnalFunc;
    tempArgs->killCommand = &killAnalFunc;
    tempArgs->JSD_q = JSD_q;
    tempArgs->tasks_q = tasks_q;
    tempArgs->WFD_arr = WFD_arr;
    int ret = pthread_create(&(analTid[i]), NULL, analFunc, tempArgs);
    if(ret !=0){
      printf("anal thread failed to create, quitting");
      abort();
    }
  }
  //collects all of the anal threads
  while(true){
    sleep(1);
    if(DEBUG_THREAD_WAITING){
      printf("waitingCountValue: %i\n", waitingCountAnalFunc);
    }
    if(waitingCountAnalFunc == argInfo.analysisThreads){
      killAnalFunc = true;
      for(int i = 0; i < argInfo.analysisThreads; i++){
        pthread_cond_signal(&(tasks_q->pop_ready) );
      }
      
      break;
    }
  }

  for(int i = 0; i < argInfo.analysisThreads; i++){
    pthread_join(analTid[i], NULL);
  }
  //tasks have now all been used up
  free(analFuncArgs);
  free(analTid);
  gen_queue_destroy(tasks_q);
  


 

  
  int JSD_arr_size = JSD_q->number_of_nodes;
  struct JSD_data** JSD_arr = malloc(sizeof(struct JSD_data*)* JSD_arr_size);
  for(int i=0; i<JSD_arr_size;i++)
  {
    JSD_arr[i] = gen_pop(JSD_q, NULL, NULL);
  }
  gen_queue_destroy(JSD_q);

  qsort(JSD_arr, JSD_arr_size, sizeof(struct JSD_data*), compareJSD);

  //
  for(int i = 0; i < JSD_arr_size; i++) {
    printf("%f %s %s\n", JSD_arr[i]->JSD_value, JSD_arr[i]->fileName1, JSD_arr[i]->fileName2);
    free(JSD_arr[i]);
  }
  free(JSD_arr);


  //This deallocates all of the leftoever pointers
  for(int i = 0; i < WFD_arr_size; i++){
    struct WFD_data* ptr = WFD_arr[i];
    //printf("WFD for file %s:\n", ptr->fileName);
    //printf("Total words: %i\n", ptr->totalWords);
    while(!gen_is_empty(ptr->words)){
      struct word_freq_data* temp = gen_pop(ptr->words, NULL, NULL);
      //printf("%s, with a freq: %i\n", temp->wordName, temp->wordCount);
      free(temp->wordName);
      free(temp);
    }
    gen_queue_destroy(ptr->words);
    free(ptr->fileName);
    free(ptr);
    //printf("-----------------------------------------\n");
  }
  free(WFD_arr);


  return 0;
}


void * analFunc(void* void_args){
  struct analArgs* args = (struct analArgs*)void_args;
  while(true) {
    struct anal_task * task = gen_pop(args->tasks_q, args->waitingCount, args->killCommand);
    if(*(args->killCommand)){
      return NULL;
    }
    if(DEBUG_ANAL_FUNC) {
      printf("task: %i, %i, popped\n", task->file1, task->file2);
    }
    struct WFD_data* file1_WFD = args->WFD_arr[task->file1];
    struct WFD_data* file2_WFD = args->WFD_arr[task->file2];
    struct JSD_data* current_JSD = malloc(sizeof(struct JSD_data));
    current_JSD->fileName1 = file1_WFD->fileName;
    current_JSD->fileName2 = file2_WFD->fileName;
    current_JSD->wordCountCombined = file1_WFD->totalWords + file2_WFD->totalWords;
    //now all we need is the JSD_value
    struct gen_queue_node * file1_word_node = file1_WFD->words->head;
    struct gen_queue_node * file2_word_node = file2_WFD->words->head;

    double file1_KLD_sum = 0;
    double file2_KLD_sum = 0;

    while( (file1_word_node != NULL) || (file2_word_node != NULL) ){
      double file1_word_freq;
      double file2_word_freq;
      //either file1 or file2 can be null
      if(file1_word_node == NULL){
        //then we just have file2 words left
        file1_word_freq = 0;
        struct word_freq_data* file2_word_data = file2_word_node->data;
        file2_word_freq = ((double)file2_word_data->wordCount) / ((double)file2_WFD->totalWords);
        //move the pointer on
        file2_word_node = file2_word_node->next;

      } else if(file2_word_node == NULL) {
        //then we just have file1 words left
        file2_word_freq = 0;
        struct word_freq_data* file1_word_data = file1_word_node->data;
        file1_word_freq = ((double)file1_word_data->wordCount) / ((double)file1_WFD->totalWords);
        //move the pointer one
        file1_word_node = file1_word_node->next;

      } else {
        //neither file node is null
        struct word_freq_data* file1_word_data = file1_word_node->data;
        struct word_freq_data* file2_word_data = file2_word_node->data;

        int cmpVal = strcmp(file1_word_data->wordName, file2_word_data->wordName);
        
        if(cmpVal == 0) {
          //both nodes are pointing to the same word
          file1_word_freq = ((double)file1_word_data->wordCount) / ((double)file1_WFD->totalWords);
          file2_word_freq = ((double)file2_word_data->wordCount) / ((double)file2_WFD->totalWords);
          //move pointers
          file1_word_node = file1_word_node->next;
          file2_word_node = file2_word_node->next;
        } else if (cmpVal < 0) {
          //file 1 word comes brefore file 2 word
          file2_word_freq = 0;
          struct word_freq_data* file1_word_data = file1_word_node->data;
          file1_word_freq = ((double)file1_word_data->wordCount) / ((double)file1_WFD->totalWords);
          //move the pointer one
          file1_word_node = file1_word_node->next;
        } else {
          //then we just have file2 words left
          file1_word_freq = 0;
          struct word_freq_data* file2_word_data = file2_word_node->data;
          file2_word_freq = ((double)file2_word_data->wordCount) / ((double)file2_WFD->totalWords);
          //move the pointer on
          file2_word_node = file2_word_node->next;
        }
        
      }
      
      double word_freq_average = 0.5 * (file1_word_freq + file2_word_freq);
      if(file1_word_freq != 0) {
        file1_KLD_sum += file1_word_freq * log2(file1_word_freq/word_freq_average);
      }
      if(file2_word_freq != 0) {
        file2_KLD_sum += file2_word_freq * log2(file2_word_freq/word_freq_average);
      }

      //printf("iteration freq1: %f, freq2: %f\n", file1_word_freq, file2_word_freq);
      //printf("log values: %f\n", log2(file1_word_freq/word_freq_average));
      //printf("log values: %f\n", log2(file2_word_freq/word_freq_average));
    }
    double JSD_val = sqrt( .5 * (file1_KLD_sum + file2_KLD_sum));
    current_JSD->JSD_value = JSD_val;
    gen_append(args->JSD_q, current_JSD);

    free(task);
  }
}


void * fileFunc(void* void_args){
  struct explorerArguments * args = (struct explorerArguments*) void_args;
  while(true){
    char* fileName = gen_pop(args->file_q, args->waitingCount, args->killCommand);
    if(*(args->killCommand)){
      return NULL;
    }
    if(DEBUG_FILE_FUNC){
      printf("%s popped from file_q\n", fileName);
    }
    struct WFD_data* file_WFD = malloc(sizeof(struct WFD_data));
    
    struct gen_queue* words = gen_queue_init();
    //so now we read a word and then attempt to insert said word
    //start PASTE
    int fd = open(fileName, O_RDONLY);
    int initial_word_length = 20; //this can be any constant > 0
    int numberOfWords = 0;
    int readBits = 0;
    int wordLength = 0;
    int maxWordLength = initial_word_length; 
    char * currentWord = malloc(sizeof(char) * maxWordLength);

    char* buffer = malloc(sizeof(char) * BUFFER_SIZE);
    while( (readBits = read(fd, buffer, BUFFER_SIZE * sizeof(char))) ) {
      for(int i = 0; i < readBits; i++) {
        char c = tolower(buffer[i]);
        if(isspace(c)) {
          if(wordLength == 0){
            //we have nothing to insert
            continue;
          }
          currentWord[wordLength] = '\0';
          //insert takes ownership of the pointer passed to it
          insertWord(currentWord, words);
          numberOfWords++;
        
          wordLength = 0;
          maxWordLength = initial_word_length;
          currentWord = malloc(sizeof(char) * maxWordLength);
          continue;
        }
        //if it is not a number or a hyphen
        if(!(isalnum(c) || (c == '-')) ){
          //is alpha numeric,
          //we dont care about anything that is not an alpha numeric
          continue;
        }
        currentWord[wordLength] = c;
        wordLength++;
        //always want to leave room to add a null terminator
        if(wordLength == maxWordLength - 1) {
          maxWordLength = (maxWordLength + 1) * 2;
          currentWord = realloc(currentWord, sizeof(char) * maxWordLength );
        }
      }
    }
    //there may be a word left in the buffer
    if(wordLength != 0){
        currentWord[wordLength] = '\0';
          //insert takes ownership of the pointer passed to it
          insertWord(currentWord, words);
          numberOfWords++;
    } else {
      //then the pointer will not be used
      free(currentWord);
    }
    free(buffer);
    file_WFD->totalWords = numberOfWords;
    file_WFD->words = words;
    file_WFD->fileName = fileName;
    gen_append(args->WFD_q, file_WFD);
  }
  return NULL;
}




//this should be in full working order now
void * directoryFunc ( void* void_args){
  struct explorerArguments* args = (struct explorerArguments*) void_args;
  while(true)
  {
    char * fileName = gen_pop(args->dir_q, args->waitingCount, args->killCommand);
    if(*(args->killCommand)){
      return NULL;
    }
    if(DEBUG_DIR_FUNC){
      printf("%s, popped from dir_q\n", fileName);
    }
    DIR * folder = opendir(fileName);
    if(!folder) {
				printf("ERROR: directory '%s' could not be opened\n", fileName);
        continue;
		}
    
    
    struct dirent* item;
		while ( (item = readdir(folder)) ) {
			//file is a directory
      if( item->d_name[0] == '.') {
        //skip any directory that starts with a .
        //printf("skipping . or .. \n");
        continue;
      }
			if(item->d_type == 4) {
        //2 null terminators means 2 '+1's
        char* dirPath = (char*)malloc(sizeof(char)* (strlen(item->d_name) + 1 + strlen(fileName) + 1));

				//cpy the directory name into dirPath
				strcpy(dirPath, fileName);
				//should insert just one '/'
				dirPath[strlen(fileName)] = '/';
				//add back the null terminator
				dirPath[strlen(fileName) + 1] = '\0';
				//at the name of the file
				strcat(dirPath, item->d_name);
        gen_append(args->dir_q, dirPath);
        if(DEBUG_DIR_FUNC){
          printf("%s added to dir_q\n", dirPath);
        }
			} else {
        char * suffix = &(item->d_name[strlen(item->d_name) - strlen(args->fileExtension)]);
        if(strcmp(suffix, args->fileExtension) == 0) {

          char* filePath = (char*)malloc(sizeof(char)* (strlen(item->d_name) + 1 + strlen(fileName) + 1));

          //cpy the directory name into dirPath
          strcpy(filePath, fileName);
          //should insert just one '/'
          filePath[strlen(fileName)] = '/';
          //add back the null terminator
          filePath[strlen(fileName) + 1] = '\0';
          //at the name of the file
          strcat(filePath, item->d_name);
          gen_append(args->file_q, filePath);
          if(DEBUG_DIR_FUNC){
            printf("%s added to file_q\n", filePath);
          }
        }
      }

    }
    closedir(folder);
    //free directory no longer in use
    free(fileName);  
  } 
  return NULL;
}


//------------------------------------------------------------
//EVERYTHING BELOW THIS IS PARSING INPUTS 
//------------------------------------------------------------


void sortInputFiles(struct argumentInformation* argInfo, 
                    struct gen_queue* dir_q, 
                    struct gen_queue * file_q
                    ) 
  {
    for(int i = 0; i < argInfo->fileCount; i++){
      struct stat fileInfo;
      stat(argInfo->fileNames[i], &fileInfo);

      if(S_ISDIR(fileInfo.st_mode)){
        char * temp = malloc(sizeof(char) * (strlen(argInfo->fileNames[i])  + 1));
        strcpy(temp, argInfo->fileNames[i]);
        gen_append(dir_q, temp);
      } else if(S_ISREG(fileInfo.st_mode)){
        char * temp = malloc(sizeof(char) * (strlen(argInfo->fileNames[i]) +  1));
        strcpy(temp, argInfo->fileNames[i]);
        gen_append(file_q, temp);
      } else {
        //this file is not a regular file nor a directoryThreads
        printf("ERROR: %s is not a regular or a directory, skipping over it.\n", argInfo->fileNames[i]);
      }
    }
    if(DEBUG_INPUT){
      struct gen_queue_node * temp = dir_q->head; 
      //printing out the Q's like this is destructive
      printf("intital values of dir_q and file_q:\n");
      while(temp != NULL){
        char* dataTemp = temp->data;
        printf("this is a directory: %s\n", dataTemp);
        temp = temp->next;
      }
      temp = file_q->head;
      while(temp != NULL){
        char* dataTemp = temp->data;
        printf("this is a file: %s\n", dataTemp);
        temp = temp->next;
      }
      printf("-------------------------------------\n");
    }
  }

void parseArguments(int argc, char** argv, struct argumentInformation* argInfo){
    //default values for all of the options
  int directoryThreads = 1;
  int fileThreads = 1;
  int analysisThreads = 1;
  //this initialization might be an issue?
  bool freeFileExtension = false;
  char* fileExtension = ".txt";
  int fileCount = 0;

  //first thing we shall do is check for any arguements
  for(int i = 1; i < argc; i++){
    //if temp is the same as argv[i] then argv[i] began with -d
    char * temp = strstr(argv[i], "-d");
    if(temp == argv[i]){
      char * argument = (char*)malloc(sizeof(char) * strlen(argv[i]));
      //this ommits the first 2 characters
      strcpy(argument, &(argv[i][2]));
      int numThreads = atoi(argument);
      if(numThreads < 1) {
        printf("ERROR: Directory threads NAN or < 1\n");
        exit(1);
      }
      directoryThreads = numThreads;
      free(argument);
      continue;
    }
    temp = strstr(argv[i], "-f");
    if(temp == argv[i]){
      char * argument = (char*)malloc(sizeof(char) * strlen(argv[i]));
      //this ommits the first 2 characters
      strcpy(argument, &(argv[i][2]));
      int numThreads = atoi(argument);
      if(numThreads < 1) {
        printf("ERROR: file threads NAN or < 1\n");
        exit(1);
      }
      fileThreads = numThreads;
      free(argument);
      continue;
    }
    temp = strstr(argv[i], "-a");
    if(temp == argv[i]){
      char * argument = (char*)malloc(sizeof(char) * strlen(argv[i]));
      //this ommits the first 2 characters
      strcpy(argument, &(argv[i][2]));
      int numThreads = atoi(argument);
      if(numThreads < 1) {
        printf("ERROR: analysis threads NAN or < 1\n");
        exit(1);
      }
      analysisThreads = numThreads;
      free(argument);
      continue;
    }
    temp = strstr(argv[i], "-s");
    if(temp == argv[i]){
      char * argument = (char*)malloc(sizeof(char) * strlen(argv[i]));
      //this ommits the first 2 characters
      strcpy(argument, &(argv[i][2]));
      fileExtension = argument;
      freeFileExtension = true;
      //we dont free here
      continue;
    }
    if(argv[i][0] == '-')
    {
      printf("Invalid parameter option\n");
      exit(1);
    }
    
    //if still ahve hyphen, out put error
    //so at this point we know that this index does not contain an argument, we have a file
    argInfo->fileNames[fileCount] = argv[i];
    fileCount++;
  }
  //now we update all the information in the struct
  argInfo->directoryThreads = directoryThreads;
  argInfo->fileThreads = fileThreads;
  argInfo->analysisThreads = analysisThreads;
  argInfo->fileCount = fileCount;
  argInfo->fileExtension = fileExtension;
  argInfo->freeFileExtension = freeFileExtension;

  if(DEBUG_INPUT) { 
    printf("Arguments parsed at end of parseArguments: \n");
    printf("dir threads: %i\n", argInfo->directoryThreads);
    printf("anal-ISIS threads (teehee): %i\n", argInfo->analysisThreads);
    printf("file threads: %i\n", argInfo->fileThreads);
    printf("file extension: %s\n", argInfo->fileExtension);
    printf("should file extension be freed?: %i\n", argInfo->freeFileExtension);
    printf("files passed:\n");
    for(int i = 0; i < argInfo->fileCount; i++){
      printf("file: %s\n", argInfo->fileNames[i]);
    }
  }
}

void insertWord(char * word, struct gen_queue* words){
  struct gen_queue_node * temp = words->head;
  struct gen_queue_node * prev = NULL;
  //printing out the Q's like this is destructive
  while(temp != NULL){
    struct word_freq_data* dataTemp = temp->data;
    //found a node with the same word
    if(strcmp(word, dataTemp->wordName) == 0) {
      dataTemp->wordCount++;
      //we can free word
      free(word);
      return;
    }
    //there was no node with the same word, we must create a new one 
    //and insert in alphabetical order
    if(strcmp(word, dataTemp->wordName) < 0) {
      temp = malloc(sizeof(struct gen_queue_node));
      struct word_freq_data* new_word_freq = malloc(sizeof(struct word_freq_data));
      new_word_freq->wordName = word;
      new_word_freq->wordCount = 1;
      temp->data = new_word_freq;
      //this we are adding this node to the frind of the list
      if(prev != NULL){
        temp->next = prev->next;
        prev->next = temp;
      } else {
        temp->next = words->head;
        words->head = temp;
      }
      words->number_of_nodes++;
      return;
    }
    
    prev = temp;
    temp = temp->next;
  }
  //we just add the word to the end of the list
  struct word_freq_data* new_word_freq = malloc(sizeof(struct word_freq_data));
  new_word_freq->wordName = word;
  new_word_freq->wordCount = 1;
  gen_append(words, new_word_freq);
}


void quick_test() {
  int a = 4;
  int b = 7;
  double c = a/b;
  double d = ((double) a )/b;
  printf("value c: %f,\n", c);
  printf("value d: %f,\n", d);
}
 
