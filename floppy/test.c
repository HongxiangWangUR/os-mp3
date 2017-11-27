#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/wait.h>

/* function decleration goes here */
void doeventopen();
void doeventclose();
void doeventsig();
void doeventwait();
void doeventinfo();
void doeventchown();
void doeventchmode();
void doeventstat();

/*
 * here are syscall numbers
 * 181: doeventopen();
 * 182: doeventclose();
 * 183: doeventwait();
 * 184: doeventsig();
 * 185: doeventinfo();
 * 205: doeventchown();
 * 211: doeventchmode();
 * 214: doeventstat();
 */

int main (int argc, char ** argv) {
	while(1){
		int option=-1;
		printf("*******************************************\n");
		printf("please input the number of test function:\n");
		printf("1.doeventopen()\n");
		printf("2.doeventclose()\n");
		printf("3.doeventsig()\n");
		printf("4.doeventwait()\n");
		printf("5.doeventinfo()\n");
		printf("6.doeventchown()\n");
		printf("7.doeventchmode()\n");
		printf("8.doeventstat()\n");
		printf("9.exit the program\n");
		printf("*******************************************\n");
		scanf("%d",&option);
		if(option<=0||option>9){
			printf("input number error, please try again\n");
			continue;
		}
		switch(option){
			case 1: doeventopen();break;
			case 2: doeventclose();break;
			case 3: doeventsig();break;
			case 4: doeventwait();break;
			case 5: doeventinfo();break;
			case 6: doeventchown();break;
			case 7: doeventchmode();break;
			case 8: doeventstat();break;
			case 9: return 0;
		}
	}
	return 0;
}

void doeventopen(){
	long event_id=syscall(181,NULL);
	if(event_id<0){
		printf("create event error\n");
	}else{
		printf("success, the new event id is %ld\n",event_id);
	}
}

void doeventclose(){
	long event_id;
	printf("please input the event id to be closed:\n");
	scanf("%ld",&event_id);
	long result=syscall(182,event_id);
	if(result<0){
		printf("event close error\n");
	}else{
		printf("event close success, the number fo process waked up is %ld\n",result);
	}
}

void doeventsig(){
	long event_id;
	printf("please input the event id to be signaled:\n");
	scanf("%ld",&event_id);
	long result=syscall(184,event_id);
	if(result<0){
		printf("signal event error\n");
	}else{
		printf("event signal success, the number of process waked up is %ld\n",result);
	}
}

void doeventwait(){
	long event_id;
	printf("please input the event id that you want to wait:\n");
	scanf("%ld",&event_id);
	pid_t pid;
	if((pid=fork())<0){
		printf("fork error, please try again\n");
	}else if(pid==0){//child process, it will wake up parents
		sleep(2);
		int result=syscall(184,event_id);
		if(result<0){
			printf("signal event error\n");
		}
		printf("child process signaled process number is %d\n",result);
		exit(0);//exit from child process
	}else{//parents process, it will wait on specified event
		printf("begin to wait on event about 2 seconds\n");
		int result=syscall(183,event_id);
		int status;
		wait(&status);
		if(result<0){
			printf("doeventwait error, please try again\n");
		}else{
			printf("doeventwait success\n");
		}
	}
}

void doeventinfo(){
	long num_of_event=0;
	int query_num=0;
	int * array=NULL;
	printf("please input the number of array size, which is used to store events id:\n");
	scanf("%d",&query_num);
	array=(int*)malloc(sizeof(int)*query_num);
	num_of_event=syscall(185,query_num,array);
	if(num_of_event<0){
		printf("syscall error, please try again\n");
		return;
	}
	printf("event info success, the number of events is %ld\n",num_of_event);
	if(num_of_event>0){
		for(int i=0;i<num_of_event;i++){
			printf("the event id is: %d\n",array[i]);
		}
	}
	free(array);
}

void doeventchown(){
	int input;
	uid_t uid;
	gid_t gid;
	printf("please input the id of event that you want to change:\n");
	scanf("%d",&input);
	printf("please input new uid:\n");
	scanf("%d",&uid);
	printf("please input new gid:\n");
	scanf("%d",&gid);
	if(syscall(205,input,uid,gid)<0){
		printf("doeventchown error, please try again\n");
		return;
	}
	printf("event change own success\n");
}

void doeventchmode(){
	int input;
	int user_bit;
	int group_bit;
	printf("please input the event id:\n");
	scanf("%d",&input);
	printf("please input the user bit:\n");
	scanf("%d",&user_bit);
	if(user_bit!=0 && user_bit!=1){
		printf("error! the user bit must be 1 or 0, please try again");
		return;
	}
	printf("please input the group bit:\n");
	scanf("%d",&group_bit);
	if(group_bit!=0 && group_bit!=1){
		printf("error! the group bit must be 1 or 0, please try again");
		return;
	}
	if(syscall(211,input,user_bit,group_bit)<0){
		printf("syscall error, please try again\n");
	}else{
		printf("doeventchmode success\n");
	}
}

void doeventstat(){
	int event_id;
	uid_t uid;
	gid_t gid;
	int uid_f;
	int gid_f;

	printf("please input the event id that you want to check\n");
	scanf("%d",&event_id);
	if(syscall(214,event_id,&uid,&gid,&uid_f,&gid_f)<0){
		printf("syscall error, please try again\n");
	}else{
		printf("success, event id=%d, uid=%d, gid=%d, user enable bit=%d, group enable bit=%d\n",event_id,uid,gid,uid_f,gid_f);
	}
}