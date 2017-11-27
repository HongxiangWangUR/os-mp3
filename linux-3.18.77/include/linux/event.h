#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/uidgid.h>
#include <linux/wait.h>

#define INIT_ENABLE_BITS_VALUE 0x3 //initial value of signal enable bits
#define SIG_BITS_TYPE1 0x0 //user bit is 0,group bit is 0
#define SIG_BITS_TYPE2 0x1 //user bit is 1,group bit is 0
#define SIG_BITS_TYPE3 0x2 //user bit is 0,group bit is 1
#define SIG_BITS_TYPE4 0x3 //user bit is 1,group bit is 1

/* the initial list */
struct event_head {
	struct list_head list;
};
/*
* the abstract data type of event
*/
struct event{
	struct list_head list; /* linked event list */
	uid_t uid; /* the effective user id */
	gid_t gid; /* effective group id */
	long event_id; /* event id */
	/* signal enable bits, user enable bit is at lower position
	* group enable bit is at higher bit
	*/
	uint8_t signal_enable;
	long wait_count; /* number of waiting process on this event */
	/* if this event have received a signal. if it have then there should be no
	 * process waiting on it. */
	bool have_signal;
};

void signal_bit_init(struct event * evn);
void doevent_init(void);
struct event* find_event(int eventid);
int get_user_enable_bit(uint8_t sig_enable);
int get_group_enable_bit(uint8_t sig_enable);
bool access_control_check(struct event* p);