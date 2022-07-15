#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "log.h"

#define high    0
#define medium  1
#define low     2

#define hpq_times   2
#define mpq_times   4

int logindex = 0;
int* logi = &logindex;

pid_t create_job(int i);
void siga_handler();   // handler for signal SIGALRM
void sigc_handler();   // handler for signal SIGCHLD
sigset_t mymask1;      // normal signal process mask when 
                       // all signals are free
                       // but SIGALRM and SIGCHLD are blocked
sigset_t mymask2;      // special signal process mask when 
                       // all signals are free
sigset_t jobmask;      // job signal process mask blocking 
                       // all signals, leavingonly SIGUSR2 
                       // unblocked
struct sigaction sa_alarm;  // disposition for SIGALRM
struct sigaction sa_chld;   // disposition for SIGCHLD

int number_of_jobs;

typedef struct QUEUE_STRUCT {
  int queue[6];     // there are at most 6 jobs
  int first;
  int last;
} QUEUE;
QUEUE* CreateEmptyQueue();
void InsertIntoQueue(QUEUE* qu,int index);
int RemoveFromQueue(QUEUE* qu);

QUEUE *hp=NULL;  // pointer to high-priority queue
QUEUE *mp=NULL;  // pointer to midle-priority queue
QUEUE *lp=NULL;  // pointer to low-priority queue

typedef struct JOB_STRUCT {
  pid_t pid;
  int nof_runs_hp;
  int nof_runs_mp;
  int done;
  int priority;
} JOB;

JOB job_table[6]; // there are at most 6 jobs
int current=-1;    // index of current job


// function main -----------------------------------------
int main(int argc,char** argv) {
  int i, j;
  pid_t pid;
  

  if (argc!=2)
    msg_exit("usage --- %s <number-of-jobs>\n",argv[0]);
  number_of_jobs=atoi(argv[1]);
  if (number_of_jobs < 3 || number_of_jobs > 6)
    msg_exit("incorrect number of jobs\n");
  
  create_log("assgn1.log");

  sigemptyset(&mymask1);   // mymask1 -- SIGCHLD and 
                           // SIGALRM blocked
  sigaddset(&mymask1,SIGCHLD);
  sigaddset(&mymask1,SIGALRM);

  sigprocmask(SIG_SETMASK,&mymask1,NULL);
  

  sigemptyset(&mymask2); // mymask2 -- all signals free

  sigfillset(&jobmask);
  sigdelset(&jobmask,SIGUSR2); // all signals but SIGUSR2 
                               // blocked

  sa_alarm.sa_handler = siga_handler;
  sigfillset(&sa_alarm.sa_mask);
  sa_alarm.sa_flags = SA_RESTART;
  
  sigaction(SIGALRM,&sa_alarm,NULL);
  

  sa_chld.sa_handler = sigc_handler;
  sigfillset(&sa_chld.sa_mask);
  sa_chld.sa_flags = SA_RESTART;


  sigaction(SIGCHLD,&sa_chld,NULL);


  hp=CreateEmptyQueue();
  mp=CreateEmptyQueue();
  lp=CreateEmptyQueue();
  

  for(i = 0; i < number_of_jobs; i++) {
    pid = create_job(i);
    job_table[i].pid=pid;
    job_table[i].nof_runs_hp=0;
    job_table[i].nof_runs_mp=0;
    job_table[i].done=0;
    job_table[i].priority=high;
    InsertIntoQueue(hp,i);
  }

  while(1) {
    if ((current = RemoveFromQueue(hp)) < 0) {
      if ((current = RemoveFromQueue(mp)) < 0) {
        if ((current = RemoveFromQueue(lp)) < 0) {
          Msg("All jobs done");
	  break;
	}
      }
    }
    // current is the job to be switched on
    if (job_table[current].priority==high) {
      Msg("Switched on high-priority job %d",current); 
      msg("Switched on high-priority job %d",current); 
    }else if (job_table[current].priority==medium) {
      Msg("Switched on medium-priority job %d",current); 
      msg("Switched on medium-priority job %d",current); 
    }else{
      Msg("Switched on low-priority job %d",current); 
      msg("Switched on low-priority job %d",current); 
    }
    kill(job_table[current].pid,SIGUSR1);
    // now we unblock SIGCHLD and SIGALRM and wait
    alarm(1);
    sigsuspend(&mymask2);
  }
    
  return 0;
}// end function main
  
  
// function create_job -----------------------------------
pid_t create_job(int i) {
  pid_t pid;
  char argv0[10];
  char argv1[10];
  
  
  sigprocmask(SIG_SETMASK,&jobmask,NULL);
  
  if ((pid = fork()) < 0) 
    Sys_exit("fork error\n");
  if (pid == 0) { // child process
    strcpy(argv0,"job");
    sprintf(argv1,"%d",i);
    execl("job",argv0,argv1,NULL);
  }
  
  // parent process
  
  sigprocmask(SIG_SETMASK,&mymask1,NULL); 
  return pid;
}// end function create_job
  
  
  
  
// function siga_handler ----------------------------------
void siga_handler() {
  int i;
  
  i = current;
  current = -1;
  kill(job_table[i].pid,SIGUSR1);
  if (job_table[i].priority==high) {
    Msg("Switched off high-priority job %d",i);
    msg("Switched off high-priority job %d",i);
  }else if (job_table[i].priority==medium) {
    Msg("Switched off medium-priority job %d",i);
    msg("Switched off medium-priority job %d",i);
  }else{
    Msg("Switched off low-priority job %d",i);
    msg("Switched off low-priority job %d",i);
  }
  if (job_table[i].priority==high) {
    job_table[i].nof_runs_hp++;
    if (job_table[i].nof_runs_hp < hpq_times) {  // keep it high
      job_table[i].priority=high;
      InsertIntoQueue(hp,i);
    }else{                                       // demote it
      job_table[i].priority=medium;
      InsertIntoQueue(mp,i);
    }
  }else if (job_table[i].priority==medium) {    
    job_table[i].nof_runs_mp++;
    if (job_table[i].nof_runs_mp < mpq_times) {  // keep it medium
      job_table[i].priority=medium;
      InsertIntoQueue(mp,i);
    }else{                                       // demote it
      job_table[i].priority=low;
      InsertIntoQueue(lp,i);
    }
  }else
    InsertIntoQueue(lp,i);        // keep it low
  return;
}// end function siga_handler
  
  
// function sigc_handler ----------------------------------
void sigc_handler() {

  alarm(0); 
  if (current==-1) return;      // nothing to do

  // so current is done
  Msg("job %d done",current);
  job_table[current].done=1;
  current = -1; // no current
}// end function sigc_handler




// function CreateEmptyQueue -----------------------------
QUEUE* CreateEmptyQueue() {
  QUEUE* q;
  q=(QUEUE*)malloc(sizeof(QUEUE));
  if (q==NULL)
     Msg_exit("[CreateEmptyQueue] memory allocation error\n");
  q->first=q->last=-1;
  return q;
}// end function CreateEmpty Queue


// function InsertIntoQueue -------------------------------
void InsertIntoQueue(QUEUE* qu,int index) {
  if (qu->first == -1) {
    qu->queue[0]=index;
    qu->last=qu->first=0;
    return;
  }
  if (qu->last==5) 
    qu->queue[qu->last=0]=index; 
  else 
    qu->queue[++(qu->last)]=index;
}// end function InsertIntoQueue


// function RemoveFromQueue -------------------------------
int RemoveFromQueue(QUEUE* qu) {
  int index;

  if (qu->first==-1) return -1;
  if (qu->first==qu->last) {
    index = qu->queue[qu->last];
    qu->last=qu->first=-1;
    return index;
  }
  if (qu->first==5){  
     index=qu->queue[5]; 
     qu->first=0; 
     return index;
  }
  index=qu->queue[qu->first];
  (qu->first)++; 
  return index;
}// end function RemoveFromQueue