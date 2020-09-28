
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                               proc.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "tty.h"
#include "console.h"
#include "string.h"
#include "proc.h"
#include "global.h"
#include "proto.h"

/*======================================================================*
                              schedule
 *======================================================================*/
SEMAPHORE semaphore1;
SEMAPHORE semaphore2;
SEMAPHORE semaphore3;
SEMAPHORE semaphore4;
SEMAPHORE semaphore5;
SEMAPHORE semaphore6;
SEMAPHORE * rmutex;
SEMAPHORE * rw;
SEMAPHORE * w;
SEMAPHORE * x;
SEMAPHORE * y;
SEMAPHORE * z;
int readcount=0;
int readsum=0;
int writecount=0;
int writesum=0;


int state=0;

PUBLIC void schedule()
{
	PROCESS* p;
	int	 greatest_ticks = 0;
	for (p = proc_table; p < proc_table+NR_TASKS; p++) {
		if(p -> sleep > 0){
			p -> sleep--;
		}
	}
	while (!greatest_ticks) {
		for (p = proc_table; p < proc_table+NR_TASKS; p++) {
			if (p -> wait>0 || p -> sleep>0){
				continue;
			}
			if (p->ticks > greatest_ticks) {
				greatest_ticks = p->ticks;
				p_proc_ready = p;
			}
		}

		if (!greatest_ticks) {

			for (p = proc_table; p < proc_table+NR_TASKS; p++) {
				if (p -> wait>0 || p -> sleep>0){
					continue;
				}
				p->ticks = p->priority;
			}
		}
	}
}

/*======================================================================*
                           sys_get_ticks
 *======================================================================*/
PUBLIC int sys_get_ticks()
{

	return ticks;
}

PUBLIC int  sys_disp_str(char * str){
	TTY*	p_tty=tty_table;
	int i=0;
    	int z=p_proc_ready - proc_table;
	while(str[i]!='\0'){
	    out_char_color(p_tty->p_console,str[i],z);
	    i++;
	}

}

PUBLIC int sys_process_sleep(int k){
	p_proc_ready -> sleep=k;
	p_proc_ready->ticks=0;
    	schedule();
	return 0;
}

PUBLIC int sys_P(SEMAPHORE* t){

   t->s--;
   if(t->s<0){
	   p_proc_ready ->wait=1;
	   t->x[t->ptr]=p_proc_ready;
	   t->ptr++;
	   p_proc_ready->ticks=0;
	   schedule();
   }
}

PUBLIC int sys_V(SEMAPHORE* t){
	PROCESS* p;
   t->s++;
   if(t->s<=0){
   	t->x[0]->wait=0;
	t->x[0]->ticks=1;
   	for(int i=0;i<t->ptr;i++){
   	    t->x[i]=t->x[i+1];
   	}
   	t->ptr--;
	schedule();
   }



}

PUBLIC void initSemaphore(int b) {
    semaphore1.s = 1;
    semaphore2.s = 1;
    semaphore3.s = b;
    semaphore4.s = 1;
    semaphore5.s = 1;
    semaphore6.s = 1;
    readcount=0;
    writecount=0;
    readsum=0;
    writesum=0;
    rmutex = &semaphore1;
    rw = &semaphore2;
    w = &semaphore3;
    x = &semaphore4;
    y = &semaphore5;
    z = &semaphore6;

}

PUBLIC void read(char* name,int time){
int temptime=time;
while(1){
temptime=time;
if(readsum>=2){
continue;
}


system_P(w);
system_P(rmutex);
if(readcount==0)system_P(rw);
readcount++;
system_disp_str(name);
system_disp_str(" begin reading\n");
state=0;
system_V(rmutex);

while(temptime!=0){
system_disp_str(name);
system_disp_str(" is reading\n");
system_process_sleep(2000);

temptime--;
}

//system_process_sleep(2000*time);
system_disp_str(name);
system_disp_str(" stop reading\n");

readsum+=1;

system_V(w);
system_process_sleep(1);
system_P(rmutex);
readcount--;
if(readcount==0){system_V(rw);}
system_V(rmutex);
system_process_sleep(10);


}
}


PUBLIC void write(char* name,int time){
int temptime=time;

while(1){
temptime=time;
system_P(rw);
system_disp_str(name);
system_disp_str(" begin writing\n");
state=1;
readsum=0;
//system_process_sleep(2000*time);
while(temptime!=0){
system_disp_str(name);
system_disp_str(" is writing\n");
system_process_sleep(2000);

temptime--;
}


system_disp_str(name);
system_disp_str(" stop writing\n");
system_V(rw);


}
}

PUBLIC void printState(){
while(1){
if(state==0){
system_disp_str("process now:reading, and the num of reading process is ");
char c=readcount+'0';
char* d[2];
d[0]=c;
d[1]='\0';
system_disp_str(d);
system_disp_str("\n");
}else{
system_disp_str("process now:writing\n");
}
system_process_sleep(2000);

}
}



PUBLIC void read1(char* name,int time){
int temptime=time;
while(1){
temptime=time;

system_P(w);
system_P(z);
system_P(rmutex);
system_P(x);
readcount++;
if(readcount==1)system_P(rw);
system_disp_str(name);
system_disp_str(" begin reading\n");
state=0;
system_V(x);
system_V(rmutex);
system_V(z);
writesum=0;

while(temptime!=0){
system_disp_str(name);
system_disp_str(" is reading\n");
system_process_sleep(2000);

temptime--;
}

//system_process_sleep(2000*time);
system_disp_str(name);
system_disp_str(" stop reading\n");

system_V(w);


system_P(x);
readcount--;
if(readcount==0){system_V(rw);}
system_V(x);

}
}


PUBLIC void write1(char* name,int time){

int temptime=time;
while(1){
temptime=time;

if(writesum>=1){
continue;
}


system_P(y);
writecount++;
if(writecount==1)system_P(rmutex);
system_V(y);
system_P(rw);
system_disp_str(name);
system_disp_str(" begin writing\n");
state=1;

while(temptime!=0){
system_disp_str(name);
system_disp_str(" is writing\n");
system_process_sleep(2000);

temptime--;
}

//system_process_sleep(2000*time);
system_disp_str(name);
system_disp_str(" stop writing\n");
writesum+=1;
system_V(rw);

system_P(y);
writecount--;
if(writecount==0)system_V(rmutex);
system_V(y);


}
}


