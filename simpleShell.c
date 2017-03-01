#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */
#define CREATE_FLAGS (O_WRONLY | O_CREAT | O_APPEND)
#define CREATE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
//Ismet redirection
#define CREATE_FLAGS1 (O_WRONLY | O_CREAT | O_TRUNC)
#define CREATE_FLAGS2 (O_WRONLY | O_CREAT | O_APPEND)
#define CREATE_FLAGS3 (O_RDONLY | O_CREAT )

/* Background processes are stored in this struct. (Linked List)
 */
typedef struct process_node{
    int id;
    char pname[80];
    pid_t pid;
    pid_t return_pid;
    int is_prompted;
    int status;
    struct process_node *next;
}pnode;

//Background process insertion method prototype
pnode* insertBgList(pnode**, pid_t, char[]);

/* The setup function below will not return any value, but it will just: read
in the next command line; separate it into distinct arguments (using blanks as
delimiters), and set the args array entries to point to the beginning of what
will become null-terminated, C-style strings. */

void setup(char inputBuffer[], char *args[],int *background)
{
    int length, /* # of characters in the command line */
        i,      /* loop index for accessing inputBuffer array */
        start,  /* index where beginning of next command parameter is */
        ct;     /* index of where to place the next parameter into args[] */

    ct = 0;

    /* read what the user enters on the command line */
    length = read(STDIN_FILENO,inputBuffer,MAX_LINE);

    /* 0 is the system predefined file descriptor for stdin (standard input),
       which is the user's screen in this case. inputBuffer by itself is the
       same as &inputBuffer[0], i.e. the starting address of where to store
       the command that is read, and length holds the number of characters
       read in. inputBuffer is not a null terminated C-string. */

    start = -1;
    if (length == 0)
        exit(0);            /* ^d was entered, end of user command stream */

/* the signal interrupted the read system call */
/* if the process is in the read() system call, read returns -1
  However, if this occurs, errno is set to EINTR. We can check this  value
  and disregard the -1 value */
    if ( (length < 0) && (errno != EINTR) ) {
        perror("error reading the command");
	exit(-1);           /* terminate with error code of -1 */
    }

	printf(">>%s<<",inputBuffer);
    for (i=0;i<length;i++){ /* examine every character in the inputBuffer */

        switch (inputBuffer[i]){
	    case ' ':
	    case '\t' :               /* argument separators */
		if(start != -1){
                    args[ct] = &inputBuffer[start];    /* set up pointer */
		    ct++;
		}
                inputBuffer[i] = '\0'; /* add a null char; make a C string */
		start = -1;
		break;

            case '\n':                 /* should be the final char examined */
		if (start != -1){
                    args[ct] = &inputBuffer[start];
		    ct++;
		}
                inputBuffer[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
		break;

	    default :             /* some other character */
		if (start == -1)
		    start = i;
                if (inputBuffer[i] == '&' && inputBuffer[i-1] != '>'){
		    *background  = 1;
                    inputBuffer[i-1] = '\0';
		}
	} /* end of switch */
     }    /* end of for */
     args[ct] = NULL; /* just in case the input line was > 80 */

	for (i = 0; i <= ct; i++)
		printf("args %d = %s\n",i,args[i]);
} /* end of setup routine */


/*This method returns the path of the command which is 
 * provided by inputBuffer argument. 
 * The method uses "which" command hard coded in path "/usr/bin/which"
 * It simply creates a child and executes which with given command.
 * The child returns the path of given command.
 * This relation was established by pipe() command.
 */
int getPath(char inputBuffer[], char whichpath[]){
    //Pipe için gerekli değişkenler
    int fd[2];
    pid_t childpid;
    int tempstdin = dup(STDIN_FILENO); //SDTDIN ve STDOUT un yedeğini alıyorum.
    int tempstdout = dup(STDOUT_FILENO);
    
    //Copied from lecture slides. Establishing pipe and error checking.
    if ((pipe(fd) == -1) || ((childpid = fork()) == -1)) {
        perror("Failed to setup pipeline");
        return 1;
        }
        if (childpid == 0) { /* 'which' command is called in this child */
            if (dup2(fd[1], STDOUT_FILENO) == -1)
                perror("Failed to redirect stdout of which");
            else if ((close(fd[0]) == -1) || (close(fd[1]) == -1))
                perror("Failed to close extra pipe descriptors on which");
            else {
                char* parmList[] = {"/usr/bin/which", inputBuffer, NULL};
                execv("/usr/bin/which", parmList); //Execution of which command.
                perror("Failed to exec which");
            }
            return 1;
        }
        if (dup2(fd[0], STDIN_FILENO) == -1) /* This is the parent */
            perror("Failed to redirect stdin of sort");
        else if ((close(fd[0]) == -1) || (close(fd[1]) == -1))
            perror("Failed to close extra pipe file descriptors on sort");
        else {
            read(STDIN_FILENO, whichpath, 250); //Reading from child process.
            dup2(tempstdin, STDIN_FILENO); //Copying the original fd
            return 0;
        }
}

/* This method inserts background process into linked list.
 * Returns the inserted node.
 */

pnode* insertBgList(pnode** sPtr, pid_t pid, char pname[]){
    pnode* newPtr;
    pnode* prevPtr;
    pnode* curPtr;
    
    int id=0;
    
    //Finding smallest id.
    id=returnSmallestInt(&(*sPtr));
    
    newPtr=malloc(sizeof(pnode));
    
    if(newPtr != NULL){ 
        newPtr->pid = pid; //Initializing the new node.
        newPtr->id = id;
        memset(newPtr->pname, '\0', 80);
        strncpy(newPtr->pname, pname, strlen(pname));
        newPtr->next = NULL;
        newPtr->is_prompted=0;
        
        prevPtr = NULL;
        curPtr = *sPtr;
        
        while(curPtr!= NULL && id > curPtr->id){
            prevPtr = curPtr;
            curPtr = curPtr->next;
        }
        
        if(prevPtr == NULL){
            newPtr->next = *sPtr;
            *sPtr = newPtr;
        }else{
            prevPtr->next = newPtr;
            newPtr->next=curPtr;
        }
        
        return newPtr;
    }else{
        perror("Not enough memory!\n");
    } 
}

/* This function returns the smallest 
 * id from given linked list.
 */
int returnSmallestInt(pnode** sPtr){
    int id=1;
    pnode* newPtr;
    pnode* prevPtr;
    pnode* curPtr;
    
    curPtr = *sPtr;
    if(*sPtr == NULL){
        return 1;
    }else{
        
        while(curPtr != NULL && id == curPtr->id){
            id++;
            prevPtr = curPtr;
            curPtr = curPtr->next;
        }
        
        return id;
    }
}

/*This function updates states of programs in background.
 * waitpid function provides this information. With the help of
 * WNOHANG option we can learn about the process state without waiting its termination.
 */
void updateList(pnode** curPtr){
    int i=0;
    pnode* dummy1=*curPtr;
    pnode* dummy2=*curPtr;
    
    if(dummy1 ==  NULL){
        return;
    }else{
        while(dummy1 != NULL){
            dummy1->return_pid = waitpid(dummy1->pid, &(dummy1->status), WNOHANG);
            dummy1 = dummy1->next;
        }
    }
}

/* This function writes states of background processes.
 * First traversing is for the Running state.
 * Second traversing is for the Finished state.
 * After the second traversal, program calls the removePromptedFromBg
 * for removing finished and prompted processes.
 */
void writeBgList(pnode** curPtr){
    int i=0;
    pnode* dummy1=*curPtr;
    pnode* dummy2=*curPtr;
    
    if(dummy1 == NULL){
        perror("List is empty!\n");
    }else{
        updateList(&(*curPtr));
        printf("Running\n");
        while(dummy1 != NULL){ //First traversal.
            if(dummy1->return_pid==0){
                printf("\t[%d] %s (pid=%d)\n", dummy1->id, dummy1->pname, dummy1->pid);
            }
            
            i++;
            dummy1=dummy1->next;
        }
        printf("Finished\n");
        while(dummy2 != NULL){ // Second traversal.
            if(dummy2->return_pid==dummy2->pid || (dummy2->return_pid==-1)){ 
                
                printf("\t[%d] %s (pid=%d)\n", dummy2->id, dummy2->pname, dummy2->pid);
                dummy2->is_prompted=1;
            }
            i++;
            dummy2=dummy2->next;
        }
        //Removes prompted nodes from linked list.
        removeTool(&(*curPtr));
    }
}

/* This function removes processes which are prompted to the screen.
 * It checks returnpid and pid fields equality and is_prompted field of 
 * each node in the linked list.
 */

void removeTool(pnode** sPtr){
    pnode* dummy1=*sPtr;
    pnode* dummy2=*sPtr;
    
    while(dummy1 != NULL){
        if(((dummy1->return_pid == dummy1->pid) || (dummy1->return_pid == -1) ) && (dummy1->is_prompted==1)){
            dummy2=dummy1->next;
            removeFromBg(&(*sPtr), dummy1->pid);
            dummy1=dummy2;
        }else{dummy1=dummy1->next;}
    }
}



void removePromptedFromBg(pnode** sPtr){
    pnode* tempPtr;
    pnode* prevPtr;
    pnode* curPtr;
    curPtr=*sPtr;
    int Header=1;
    if(*sPtr == NULL){return;}
    else{
        while(curPtr != NULL){
            if(((curPtr->return_pid == curPtr->pid) || (curPtr->return_pid == -1) ) && (curPtr->is_prompted==1)){
                if(Header==1){
                    tempPtr = curPtr;
                    curPtr=(curPtr)->next;
                    free(tempPtr);
                    continue;
                }

                tempPtr = curPtr;
                prevPtr->next = curPtr->next;
                free(tempPtr);


            }
            else{
                prevPtr=curPtr;
                curPtr=curPtr->next;
                Header=0;

            }
            
        }
    }
}

/* This method removes a node with given bgpid from bg linked list.
 * Used in fg built-in command.
 */
void removeFromBg(pnode** sPtr, pid_t bgpid){
    pnode* tempPtr;
    pnode* prevPtr;
    pnode* curPtr;
    
    if(*sPtr == NULL){return;}
    else{
        if((bgpid == (*sPtr)->pid)){
            tempPtr = *sPtr;
            *sPtr=(*sPtr)->next;
            free(tempPtr);
        }else{
            prevPtr=*sPtr;
            curPtr= (*sPtr)->next;
            while(curPtr != NULL && curPtr->pid != bgpid){
                prevPtr=curPtr;
                curPtr=curPtr->next;
            }

            if(curPtr != NULL){
                tempPtr = curPtr;
                prevPtr->next = curPtr->next;
                free(tempPtr);
                return;
            }
        }
    }
}

/* This method returns pid of a node which has an id of "val"
 * 
 */
pid_t returnCpidFromBg(pnode* sPtr, int val){
    while(sPtr!=NULL){
        if(val == sPtr->id) return sPtr->pid;
        sPtr = sPtr->next;
    }
    
    return 0;
}

//Simply removes the ampersand from arg list.
void removeAmpersandFromArgs(char ** args){
    int i=0;
    while(args[i] != NULL){
        if(strcmp("&", args[i])==0){
            args[i]=NULL;
        }
        i++;
    }
}

/* This method controls input output redirections and
 * applies changes into the file descriptor.
 * This method called from child processes
 * and built-in "ps_all" command.
 */
int chechIORedirection(char *args[],int *i1){
        int fd;
	int count=0, 
	    io1=0,
	    io2=0,
	    aSize=0,
	    i=0;

	while (args[i] != NULL) {aSize++;i++;}
	
	//printf("%d  ",aSize);
	
	for(i=0;i<aSize;i++){
            if(strcmp(args[i],">")==0 || strcmp(args[i],">>")==0 || strcmp(args[i],">&")==0 || strcmp(args[i],"<")==0 ){
                if(count==0) {io1=i;
                        count++;}
                else{
                        io2=i;
                        count++;}
            } 
        }
	//printf("%s  %d   %d   ",args[io1+1],count,io1);
	*i1=io1;
				
	//return 0;	//to ignore rest of the method

	if(count==0){
            return 1;
	}else if(count==1){				//1 tane ıo redirection varsa

            if(strcmp(args[io1],">")==0 || strcmp(args[io1],">>")==0){		//output redirection
                if(strcmp(args[io1],">")==0){
                    fd = open(args[io1+1], CREATE_FLAGS1, CREATE_MODE);
                }
                if(strcmp(args[io1],">>")==0){
                    fd = open(args[io1+1], CREATE_FLAGS2, CREATE_MODE);
                }
                
                if (fd == -1) {
                    perror("Failed to open file");
                    return 1;
                }
                if (dup2(fd, STDOUT_FILENO) == -1) {
                    perror("Failed to redirect standard outpu 	t");
                    return 1;
                }
                if (close(fd) == -1) {
                    perror("Failed to close the file");
                    return 1;
                }


            }


            if(strcmp(args[io1],">&")==0){					//standart error redirection

                fd = open(args[io1+1], CREATE_FLAGS1, CREATE_MODE);

                if (fd == -1) {
                    perror("Failed to open file");
                    return 1;
                }
                if (dup2(fd, STDERR_FILENO) == -1) {
                    perror("Failed to redirect standard error");
                    return 1;
                }
                if (close(fd) == -1) {
                    perror("Failed to close the file");
                    return 1;
                }


            }


            if(strcmp(args[io1],"<")==0){
                fd = open(args[io1+1], CREATE_FLAGS3, CREATE_MODE);

                if (fd == -1){
                    perror("Failed to open file");
                    return 1;
                }
                if (dup2(fd, STDIN_FILENO) == -1){
                    perror("Failed to redirect standard input");
                    return 1;
                }
                if (close(fd) == -1){
                    perror("Failed to close the file");
                    return 1;
                }
             }
	

        }else if(count==2){			//2 io redirection
            if(strcmp(args[io1],"<")==0 && (strcmp(args[io2],">")==0 || strcmp(args[io2],">>")==0 || strcmp(args[io2],">&")==0)){	// io redirection syntax 
                fd = open(args[io1+1], CREATE_FLAGS3, CREATE_MODE);					//önce std input u güncellle

                if (fd == -1) {
                    perror("Failed to open file");
                    return 1;
                }
                if (dup2(fd, STDIN_FILENO) == -1) {
                    perror("Failed to redirect standard input");
                    return 1;
                }
                if (close(fd) == -1) {
                    perror("Failed to close the file");
                    return 1;
                }

                if(strcmp(args[io2],">&")==0){			//standart error redirection
                    fd = open(args[io2+1], CREATE_FLAGS1, CREATE_MODE);

                    if (fd == -1) {
                    perror("Failed to open file");
                        return 1;
                    }
                    if (dup2(fd, STDERR_FILENO) == -1) {
                    perror("Failed to redirect standard error");
                        return 1;
                    }
                    if (close(fd) == -1) {
                    perror("Failed to close the file");
                        return 1;
                    }
                }



                if(strcmp(args[io2],">")==0 || strcmp(args[io2],">>")==0){		//STD output redirection
                    if(strcmp(args[io2],">")==0){
                        fd = open(args[io2+1], CREATE_FLAGS1, CREATE_MODE);
                    }
                    if(strcmp(args[io2],">>")==0){
                        fd = open(args[io2+1], CREATE_FLAGS2, CREATE_MODE);
                    }
                    if (fd == -1) {
                        perror("Failed to open file");
                        return 1;
                    }
                    if (dup2(fd, STDOUT_FILENO) == -1) {
                        perror("Failed to redirect standard output");
                        return 1;
                    }
                    if (close(fd) == -1) {
                        perror("Failed to close the file");
                        return 1;
                    }
                }
            }
            else if(strcmp(args[io2],"<")==0 && (strcmp(args[io1],">")==0 || strcmp(args[io1],">>")==0 || strcmp(args[io1],">&")==0)){	// io redirection syntax 
                fd = open(args[io2+1], CREATE_FLAGS3, CREATE_MODE);					//önce std input u güncellle

                if (fd == -1) {
                    perror("Failed to open file");
                    return 1;
                }
                if (dup2(fd, STDIN_FILENO) == -1) {
                    perror("Failed to redirect standard input");
                    return 1;
                }
                if (close(fd) == -1) {
                    perror("Failed to close the file");
                    return 1;
                }

                if(strcmp(args[io1],">&")==0){			//standart error redirection
                    fd = open(args[io1+1], CREATE_FLAGS1, CREATE_MODE);

                    if (fd == -1) {
                    perror("Failed to open file");
                        return 1;
                    }
                    if (dup2(fd, STDERR_FILENO) == -1) {
                    perror("Failed to redirect standard error");
                        return 1;
                    }
                    if (close(fd) == -1) {
                    perror("Failed to close the file");
                        return 1;
                    }
                }



                if(strcmp(args[io1],">")==0 || strcmp(args[io1],">>")==0){		//STD output redirection
                    if(strcmp(args[io1],">")==0){
                        fd = open(args[io1+1], CREATE_FLAGS1, CREATE_MODE);
                    }
                    if(strcmp(args[io1],">>")==0){
                        fd = open(args[io1+1], CREATE_FLAGS2, CREATE_MODE);
                    }
                    if (fd == -1) {
                        perror("Failed to open file");
                        return 1;
                    }
                    if (dup2(fd, STDOUT_FILENO) == -1) {
                        perror("Failed to redirect standard output");
                        return 1;
                    }
                    if (close(fd) == -1) {
                        perror("Failed to close the file");
                        return 1;
                    }
                }
            }
            else{
                perror("Wrong I/O redirection syntax.");
                return 1;
            }
        }
        
	else{ 
            perror("Max of 2 I/O redirections are allowed.");
            return 1;
        }
        
	return 0;
		
}


/* This method applies pipe operation. Unlike the IO redirection method, 
 * this method additionally performs fork operations inside.
 */
int checkPipe(char *args[],char inputBuffer[],pnode** curPtr){
    char whichpath1[250];
    char whichpath2[250];
    memset(whichpath1, '\0', 250);
    memset(whichpath2, '\0', 250);
    int whichstatus=0;
    char* argsI[MAX_LINE/2 + 1];
    char* argsO[MAX_LINE/2 + 1];
    int aSize=0,
        i=0,k,
        count=0; 
	int pp=0;		//birden fazla pipe uygulamak icin pipe locasyonlarını arrayde tut
    size_t ln;
    int PipeKey=0;
    
    while(args[i] != NULL) {aSize++;i++;}
    
    for(i=0;i<aSize;i++){
	argsI[i]=args[i];
        argsO[i]=args[i];
        //printf("%s\n",argsI[i]);			//CONTROLLER ICIN PRINTFLER
    
        if(strcmp(args[i],"|")==0 ){
            pp=i;		//birden fazla pipe icin array kullan
        	count++;
    		//remove for more than 1 loops
        }
    }
    if(strcmp(argsI[0], "ps_all")==0 ){
        PipeKey=1;
        argsI[1]=NULL;
    }
    
	
    if(count==0 || count > 1) return 1;	
    
    if(PipeKey==0){
    whichstatus=getPath(inputBuffer, whichpath1);
    if(whichstatus==1){ perror("Error! getPath function failed!"); return 1; }
    //Which boş döndüyse komut bulunamadı uyarısı
    if(whichpath1[0] == '\0'){ printf("****%s command not found!\n", inputBuffer); return 1; }
    //Sondaki \n newline karakterini siler.
    ln = strlen(whichpath1) - 1;
    if (whichpath1[ln] == '\n')
        whichpath1[ln] = '\0';
	
    strncpy(argsI[0], whichpath1, whichpath1[strlen(whichpath1)]); //execv için ilk arg pathli olmalı.
    argsI[pp]=NULL; 

    /*printf("%s\n\n",whichpath1);				//CONTROLLER ICIN PRINTFLER
    for(i=0;i<pp;i++){		
        printf("%s\n",argsI[i]);
    }*/
    }
	
	
    int k1=0,k2=pp+1;

    while(k2 < aSize){
    	argsO[k1]=args[k2];			//BURDA PIPETAN args[] ın SONUNA KADAR OLAN KISMI argsO[] ın BAŞINA ATIYORUM
    	k1++;k2++;				//k1 0 DA BAŞLIYOR ,k2 pp DEN
    }
    
    argsO[k1]=NULL;
				
    whichstatus=getPath(args[pp+1], whichpath2);					//BURDA args[PP+1] PİPE TAN HEMEN SONRAKİ KOMUT ONUN PATH İNİ BULMASINI İSTİYORUM
        										//NORMALDE inputBuffer I YOLLARDIK AMA BUDA CALISIYOR SIKINTI YOK SERBEST ^^
    if(whichstatus==1){ perror("Error! getPath function failed!"); return 1; }	
                //Which boş döndüyse komut bulunamadı uyarısı
    if(whichpath2[0] == '\0'){ printf("****%s command not found!\n", args[pp+1]); return 1; }
                //Sondaki \n newline karakterini siler.
    ln = strlen(whichpath2) - 1;
    if (whichpath2[ln] == '\n')
        whichpath2[ln] = '\0';
    strncpy(argsO[0], whichpath2, whichpath2[strlen(whichpath2)]); //execv için ilk arg pathli olmalı.


    /*printf("%s\n\n",whichpath2);				//CONTROLLER ICIN PRINTFLER
    i=0;	
    while(argsO[i]!=NULL){	
        printf("%s\n",argsO[i]);
	i++;
    }*/

	
	

    pid_t childpid;
    if ((childpid = fork()) == -1) {
        perror("Failed to setup");
	return 1;
    }
    if (childpid == 0) { 
    	pid_t childpid2;
	int fd[2];
        int temp;
	if ((pipe(fd) == -1) || ((childpid2 = fork()) == -1)) {			//BURASI LAB ÖRNEĞİNİN HEMEN HEMEN AYNISI SATEN
            perror("Failed to setup pipe");						//SADECE EXTRA FORK() VAR
            return 1;
	}
	if (childpid2 == 0) { 
				
            if (dup2(fd[1], STDOUT_FILENO) == -1){
		perror("Failed to redirect stdout of first command");
		return 1;
            }
            else if ((close(fd[0]) == -1) || (close(fd[1]) == -1)){
                perror("Failed to close extra pipe descriptors");
		return 1;
            }
            else {
                if(strcmp(argsI[0], "ps_all")==0 && argsI[1]==NULL){
                    printf("asdasd");
		//writeBgList(curPtr);
		}
                else{
              	execv(whichpath1, argsI);
                perror("Child failed when running execv");
                }
            }
	}
	if (dup2(fd[0], STDIN_FILENO) == -1){ 
            perror("Failed to redirect stdin of 2nd command");
            return 1;
	}
	else if ((close(fd[0]) == -1) || (close(fd[1]) == -1)){
            perror("Failed to close extra pipe file descriptors");
            return 1;
	}
	else {	
            execv(whichpath2, argsO);
            perror("Child failed when running execv");
	}
			
			 
	
    }
    
    wait(NULL);
    return 0;
}

int main(void){          
    
            char inputBuffer[MAX_LINE]; /*buffer to hold command entered */
            int background; /* equals 1 if a command is followed by '&' */
            char* args[MAX_LINE/2 + 1]; /*command line arguments */
            char whichpath[250];
            
            
            pnode* bgHdrPtr = NULL;
            int whichstatus=0,i1,temp,checkP,i,pCount,aSize;
            
            while (1){
                        memset(whichpath, '\0', 250);
                        background = 0;
                        printf(" 333sh: ");
                        /*setup() calls exit() when Control-D is entered */
                        setup(inputBuffer, args, &background);
                        
                        
                        /******************************************************************/
                        /* Searching pipe operations immediately after input*/
			i=0;
			pCount=0;
			aSize=0;
			while(args[i] != NULL) {aSize++;i++;}
	
			for(i=0;i<aSize;i++){
				if(strcmp(args[i],"|")==0 ){					//PIPE A BAKTIGI KISIM
					pCount++;						//1 TANE PIPE SEMBOLU VARSA METHODA GİDER
				}					//ATIYORUM SÖYLE Bİ İNPUT GİRDİ   "LS -L | |A WC" BURDA args[2] DE"|" (PIPE SEMBOLU) OLDUGU İCİN METHODA GİDİCEK
									//args[3]="|A" OLDUGU İCİN EXECV DE KLASIK KOMUT BULUNAMADI HATASI  VERICEK	
			}						//SYNTAX DA HATA VARSA ZATEN EXECV DE HATA VERİCEK VE KALDIGI YERDEN DEVAM EDİCEK
			if(pCount==1){
			checkP=checkPipe(args,inputBuffer,&bgHdrPtr);
				if(checkP==0) continue;
				else{
					perror("Failed to pipe");	//checkP 0 DÖNERSE PIPETA SIKINTI YOK,1 OLDUGUNDA FAİLED TO PİPE PRİNTF İNİ YAPIP YİNE CONTİNUE
					continue;
				}
			}
			
			
			
			/********************************************************************/
                        
                       
                        /* Writing built-in commands in if-else clause */
                        if(strcmp(inputBuffer, "ps_all")==0){
                            //Storing copies of original file descriptors.
                            int tempstdout=dup(STDOUT_FILENO);
                            int tempstderr=dup(STDERR_FILENO);
                            if(args[1] != NULL){ 
                                if(args[3] == NULL && args[2] != NULL && (strcmp(args[1], ">") || strcmp(args[1], ">>") || strcmp(args[1], ">&"))){
                                    //If there is a output redirection. 
                                    temp=chechIORedirection(args,&i1);
                                    
                                    writeBgList(&bgHdrPtr);
                                    
                                    dup2(tempstdout, STDOUT_FILENO);
                                    dup2(tempstderr, STDERR_FILENO);
                                    continue;
                                }else{
                                    perror("Invalid argument Usage: ps_all\n");
                                    continue;
                                }
                            }else{
                                writeBgList(&bgHdrPtr);
                                continue;	
                            }
                        }else if(strcmp(inputBuffer, "kill")==0){
                            int val;
                            if(*(args[1])== '%'){ //Checks whether there is a percent sign.
                                val=atoi((args[1]+1));
                                pid_t cipid;
                                if(val==0){ printf("Please enter an integer value!\n"); }
                                else{
                                    if((cipid=returnCpidFromBg(bgHdrPtr, val)) == 0){
                                        printf("Process cannot be found!");
                                    }else{
                                        kill(cipid, SIGTERM);
                                    }
                                }
                            }else if((val=atoi(args[1])) != 0){
                                kill(val, SIGTERM);
                            }else{
                                printf("Please enter a valid PID!\n");
                            }
                            continue;
                        }else if(strcmp(inputBuffer, "fg")==0){
                            int val;
                            if(*(args[1])== '%'){
                                val=atoi((args[1]+1));
                                pid_t cipid;
                                if(val==0){ printf("Please enter an integer value!\n"); }
                                else{
                                    if((cipid=returnCpidFromBg(bgHdrPtr, val)) == 0){
                                        printf("Process cannot be found!");
                                    }else{
                                        removeFromBg(&bgHdrPtr, cipid);
                                        waitpid(cipid, NULL, WUNTRACED);
                                    }
                                }
                            }else{
                                printf("Usage: fg %%num where num is the id of process that run on background\n");
                            }
                            continue;
                        }else if(strcmp(inputBuffer, "exit")==0){
                            printf("Exiting...");
                            sleep(5);
                            exit(0);
                        }
                        
                        //If it's not a pipe operation or built-in command, this function finds command's path.
                        whichstatus=getPath(inputBuffer, whichpath);
                        if(whichstatus==1){ perror("Error! getPath function failed!"); continue; }
                        //If which command doesn't find the command
                        if(whichpath[0] == '\0'){ printf("****%s command not found!\n", inputBuffer); continue; }
                         //It simply removes the newline char of path.
                        size_t ln = strlen(whichpath) - 1;
                        if (whichpath[ln] == '\n')
                            whichpath[ln] = '\0';
                        strncpy(args[0], whichpath, whichpath[strlen(whichpath)]); //execv için ilk arg pathli olmalı.
                        
                        /** the steps are:
                        (1) fork a child process using fork()
                        (2) the child process will invoke execv()
                        (3) if background == 0, the parent will wait,
                        otherwise it will invoke the setup() function again. */
                        
                        /**
                         * If it a background command, it simply forks a child 
                         * and executes execv function.
                         */
                        
                        //Executing commands.
                        if(background==0){ //If it is a fg process
                            pid_t childpid;
                            
                            
                            childpid=fork();
                            
                            if(childpid == -1){
                                perror("Failed to fork");
                                return 1;
                            }
                            if(childpid == 0){ //Child code
                                
                                temp=chechIORedirection(args,&i1);//TEMP 0 SA REDİRECTİON VAR
					
				if(temp == 0){
                                    args[i1]=NULL;	//args da '>' redirection ı NULL yapma
				}
                                
                                execv(whichpath, args);
                                perror("Child failed when running execv");
                            }else{ //Parent code
                                waitpid(childpid, NULL, WUNTRACED);
                            }
                            
                        }else if(background==1){ // If it is a bg process
                            pid_t childpid;
                            
                            removeAmpersandFromArgs(args);
                            
                            childpid=fork();
                            
                            if(childpid == -1){
                                perror("Failed to fork");
                                return 1;
                            }
                            if(childpid == 0){ //Child code
                                //Changing fd if there is a redirection.
                                temp=chechIORedirection(args,&i1);//TEMP 0 SA REDİRECTİON VAR
					
				if(temp == 0){
                                    args[i1]=NULL;	//args da '>' redirection ı NULL yapma
				}
                                
                                execv(whichpath, args);
                                perror("Child failed when running execv");
                            }else{ //Parent code
                                pnode* temp = insertBgList(&bgHdrPtr, childpid, inputBuffer); //ps_all linked list'ine eklendi
                                continue;
                            }
                        }
                        
            }
}

