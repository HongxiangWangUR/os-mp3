#include <linux/event.h>

struct event_head init_event; //the initial event head
int num_of_event; //the current number of event
int current_event_id; //the current event id
wait_queue_head_t wait_queue; //the process wait queue
rwlock_t event_lock=__RW_LOCK_UNLOCKED(event_lock); /* read and write lock, initially set to unlock*/

/* event initialization function */
void doevent_init(){
	num_of_event=0;//set the number of event to 0
	current_event_id=0;// set current event id
	INIT_LIST_HEAD(&init_event.list);//init event list
	init_waitqueue_head(&wait_queue);//init wait queue
}

/*
 * the definition of doeventopen
 */
SYSCALL_DEFINE0(doeventopen){
	int result=-1; //this is the return value
	uid_t event_uid=current_euid().val; //get the user id of current process
	gid_t event_gid=current_egid().val; //get the current process group id
	unsigned long irq_flags;

	/* allocate space for the event */
	struct event *event_pointer=kmalloc(sizeof(struct event),GFP_KERNEL);

	if(event_pointer){//if allocate space success

		/* initialize event */
		event_pointer->uid=event_uid;
		event_pointer->gid=event_gid;
		signal_bit_init(event_pointer);
		event_pointer->have_signal=false;
		event_pointer->wait_count=0;

		/* lock critical section to prevent race condition on 
		 * global variables
		 */
		write_lock_irqsave(&event_lock,irq_flags);
		/* modify global values */
		event_pointer->event_id= ++current_event_id;
		/* add new event to event list */
		list_add(&(event_pointer->list),&(init_event.list));
		num_of_event++;
		//unlock write lock
		write_unlock_irqrestore(&event_lock,irq_flags);
		result=current_event_id;
	}

	return result;
}

/*
 * the definition of doeventclose()
 */
SYSCALL_DEFINE1(doeventclose,int, eventID){
	long result=-1; //return value
	unsigned long irq_flags;
	struct event *p=NULL; //event pointer
	write_lock_irqsave(&event_lock,irq_flags);//lock the critical section
	/* find the event according to id */
	p=find_event(eventID);

	if(p!=NULL && access_control_check(p)){
		result=p->wait_count;//number of process wait on this event
		wake_up_all(&wait_queue);//wake up all process wait on this event
		list_del(&(p->list));//delete event from event list
		kfree(p);//free space
		num_of_event--;
	}
	write_unlock_irqrestore(&event_lock,irq_flags);
	return result;
}

/*
 * the definition of doeventwait()
 */
SYSCALL_DEFINE1(doeventwait,int, eventID){
	long result=-1;
	unsigned long irq_flags;
	struct event *p=NULL;
	struct event *exist=NULL; //check if the event exists
	/* begin enter to the critical section */
	entry_point:
	write_lock_irqsave(&event_lock,irq_flags);
	/* find event according to event id */
	p=find_event(eventID);

	if(p!=NULL && access_control_check(p)){
		result=1;// if we find the event and we can access it
		/*
		 * If the event has a signal and there are still some process not
		 * leave the event then just yeild CPU and wait until all of the 
		 * process leave the event
		 */
		while(p->have_signal && p->wait_count>0){
			write_unlock_irqrestore(&event_lock,irq_flags);
			schedule();
			goto entry_point;
		}
		if(p->have_signal){
			p->have_signal=false;
		}
		p->wait_count++;
		/*
		 * begin to block process on wait queue
		 */
		wait_event_cmd(wait_queue,!(exist=find_event(eventID)) || p->have_signal,
			write_unlock_irqrestore(&event_lock,irq_flags),
			write_lock_irqsave(&event_lock,irq_flags));
		/* when the process is wake up */
		if(exist){
			p->wait_count--;
		}
	}

	/* leave the critical section */
	write_unlock_irqrestore(&event_lock,irq_flags);

	return result;
}

/*
 * the definition of doeventsig
 */
SYSCALL_DEFINE1(doeventsig,int, eventID){
	long result=-1;
	unsigned long irq_flags;
	struct event *p=NULL;
	/* enter critical section */
	write_lock_irqsave(&event_lock,irq_flags);
	p=find_event(eventID);
	if(p && access_control_check(p)){
		result=p->wait_count;
		if(result>0){
			p->have_signal=true;
			wake_up_all(&wait_queue);
		}
	}
	write_unlock_irqrestore(&event_lock,irq_flags);
	/*leave the critical section*/
	return result;
}

/*
 * the definition of doeventinfo
 */
SYSCALL_DEFINE2(doeventinfo,int , num, int*, events_id){
	long result=-1;
	long event_number; //the number of event
	unsigned long irq_flags; 
	struct list_head *pos=NULL;
	int *ids_array=NULL; //the events id array
	int index=0;
	/*entry to the critical section*/
	read_lock_irqsave(&event_lock,irq_flags);
	event_number=num_of_event;
	/* if input pointer is null or there is no event */
	if(!events_id || event_number==0){
		result=event_number;
		read_unlock_irqrestore(&event_lock,irq_flags);
		return result;
	}
	/* if num is smaller than the number of events */
	if(num<event_number){
		read_unlock_irqrestore(&event_lock,irq_flags);
		return result;
	}
	/*allocate space*/
	ids_array=kmalloc(sizeof(int)*event_number,GFP_KERNEL);
	if(!ids_array){//if allocate space failed
		read_unlock_irqrestore(&event_lock,irq_flags);
		return result;
	}
	/* begin to store event id in kernel space */
	list_for_each(pos,&(init_event.list)){
		ids_array[index++]=list_entry(pos,struct event,list)->event_id;
	}
	/* begin to copy kernel space data into user space */
	if(!copy_to_user(events_id,ids_array,sizeof(int)*event_number)){
		result=event_number;
	}
	kfree(ids_array);//free kernel space
	/* begin to leave the critical section */
	read_unlock_irqrestore(&event_lock,irq_flags);
	return result;
}

/*
 * the definition of doeventchown
 */
SYSCALL_DEFINE3(doeventchown,int, input_id,uid_t, UID,gid_t, GID){
	long result=-1;
	unsigned long irq_flags;
	struct event* p=NULL;
	//CS section
	write_lock_irqsave(&event_lock,irq_flags);
	p=find_event(input_id);
	if(p!=NULL && access_control_check(p)){
		p->uid=UID;
		p->gid=GID;
		//if we find the event and we can get access to it then change return value
		result=0;
	}
	write_unlock_irqrestore(&event_lock,irq_flags);
	return result;
}

/*
 * the definition of doeventchmode
 */
SYSCALL_DEFINE3(doeventchmode,int, eventID,int, UIDFlag,int, GIDFlag){
	long result=-1;
	unsigned long irq_flags;
	struct event* p=NULL;
	//enter to critical section
	write_lock_irqsave(&event_lock,irq_flags);
	p=find_event(eventID);
	if(p!=NULL && access_control_check(p)){
		/* begin to change signal enable bits */
		if(UIDFlag==1 && GIDFlag==1){
			p->signal_enable=SIG_BITS_TYPE4;
			result=0;
		}else if(UIDFlag==0 && GIDFlag==1){
			p->signal_enable=SIG_BITS_TYPE3;
			result=0;
		}else if(UIDFlag==1 && GIDFlag==0){
			p->signal_enable=SIG_BITS_TYPE2;
			result=0;
		}else if(UIDFlag==0 && GIDFlag==0){
			p->signal_enable=SIG_BITS_TYPE1;
			result=0;
		}
	}
	write_unlock_irqrestore(&event_lock,irq_flags);
	return result;
}

/*
 * the definition of doeventstat
 */
SYSCALL_DEFINE5(doeventstat, int, eventID,uid_t*,UID,gid_t*,GID,int*, UIDFlag,int*, GIDFlag){
	long result=-1;
	unsigned long irq_flags;
	struct event* event_p=NULL;
	int uid_f=0;
	int gid_f=0;

	//entry to cirtical section
	write_lock_irqsave(&event_lock,irq_flags);

	event_p=find_event(eventID);
	//if we can't find the event
	if(event_p==NULL){
		write_unlock_irqrestore(&event_lock,irq_flags);
		return result;
	}
	//copy the uid to user space
	if(copy_to_user(UID,&(event_p->uid),sizeof(uid_t))){
		write_unlock_irqrestore(&event_lock,irq_flags);
		return result;
	}
	//copy the gid to user space
	if(copy_to_user(GID,&(event_p->gid),sizeof(gid_t))){
		write_unlock_irqrestore(&event_lock,irq_flags);
		return result;
	}
	//get the user enable bit and group enable bit
	uid_f=get_user_enable_bit(event_p->signal_enable);
	gid_f=get_group_enable_bit(event_p->signal_enable);
	//copy user enable bit to user space
	if(copy_to_user(UIDFlag,&uid_f,sizeof(int))){
		write_unlock_irqrestore(&event_lock,irq_flags);
		return result;
	}
	//copy group enable bit to user space
	if(copy_to_user(GIDFlag,&gid_f,sizeof(int))){
		write_unlock_irqrestore(&event_lock,irq_flags);
		return result;
	}
	result=0;
	write_unlock_irqrestore(&event_lock,irq_flags);
	//leave the CS
	return result;

}

//initialize signal enable bits
void signal_bit_init(struct event * evn){
	evn->signal_enable=INIT_ENABLE_BITS_VALUE;
}

//find event according to event id
struct event* find_event(int eventid){
	struct event *p=NULL;//return value
	struct list_head *pos=NULL;
	/* begin to traverse list */
	list_for_each(pos,&(init_event.list)){
		if(list_entry(pos,struct event,list)->event_id==eventid){
			p=list_entry(pos,struct event,list);
			break;
		}
	}
	return p;
}

//check the access control permission
bool access_control_check(struct event* p){
	uid_t uid=current_euid().val; //get current process uid;
	gid_t gid=current_egid().val; //get current process gid;

	if(uid==0){//if this is a root user
		return 1;
	}
	if(uid==p->uid && get_user_enable_bit(p->signal_enable)){
		return 1;
	}
	if(gid==p->gid && get_group_enable_bit(p->signal_enable)){
		return 1;
	}
	return 0;
}

//get user signal enable bit
int get_user_enable_bit(uint8_t sig_enable){
	return (sig_enable&1);
}

//get group signal enable bit
int get_group_enable_bit(uint8_t sig_enable){
	return ((sig_enable>>1)&1);
}