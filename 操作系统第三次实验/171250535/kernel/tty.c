
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                               tty.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"



#define TTY_FIRST	(tty_table)
#define TTY_END		(tty_table + NR_CONSOLES)

PRIVATE void init_tty(TTY* p_tty);
PRIVATE void tty_do_read(TTY* p_tty);
PRIVATE void tty_do_write(TTY* p_tty);
PRIVATE void put_key(TTY* p_tty, u32 key);


int patternState;//0表示非查找模式，1表示查找模式状态下输入，2表示输入回车后进行查找
int withdrawState;//0 no,1 yes.

/*======================================================================*
                           task_tty
 *======================================================================*/
PUBLIC void task_tty()
{
	TTY*	p_tty;

	init_keyboard();

	for (p_tty=TTY_FIRST;p_tty<TTY_END;p_tty++) {
		init_tty(p_tty);
	}
	select_console(0);
    patternState=0;
withdrawState=0;


	int t = get_ticks();
	int count=0;
	p_tty=TTY_FIRST;
	while (1) {
		
			tty_do_read(p_tty);
			tty_do_write(p_tty);
		    	if(((get_ticks() - t) * 1000 / HZ) > 135000||count==0){
				if(patternState==0){
				count=1;
				t=get_ticks();
				clean_screen(p_tty->p_console);
				}else{
				count=1;
				t=get_ticks();
				}
		
		    	}
		


		}
}

/*======================================================================*
			   init_tty
 *======================================================================*/
PRIVATE void init_tty(TTY* p_tty)
{
	p_tty->inbuf_count = 0;
	p_tty->p_inbuf_head = p_tty->p_inbuf_tail = p_tty->in_buf;
	init_screen(p_tty);
}

/*======================================================================*
				in_process
 *======================================================================*/
PUBLIC void in_process(TTY* p_tty, u32 key)
{
    char output[2] = {'\0', '\0'};
    if((key & MASK_RAW)==ESC){
            if (patternState == 0) {
                patternState = 1;//进入查找模式
            } else if (patternState > 0) {
                patternState = 0;//退出查找模式
                outPattern(p_tty->p_console);
            }
    }

    if(patternState!=2) {//进入查找显示查找结果时屏蔽除esc外的所有状态
        if (!(key & FLAG_EXT)) {
            put_key(p_tty, key);
        } else {
            int raw_code = key & MASK_RAW;
            switch (raw_code) {
		case TAB:
			put_key(p_tty, '\t');
			break;
                case ENTER:
                    if(patternState==0){
                    put_key(p_tty, '\n');
                    }else{//查找输入结束，进入输出查找结果状态
                        patternState=2;
                        findPattern(p_tty->p_console);
                    }
                    break;
                case BACKSPACE:
                    put_key(p_tty, '\b');
                    break;
                case UP:
                    if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R)) {
                        scroll_screen(p_tty->p_console, SCR_DN);
                    }
                    break;
                case DOWN:
                    if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R)) {
                        scroll_screen(p_tty->p_console, SCR_UP);
                    }
                    break;
                case F1:
                case F2:
                case F3:
                case F4:
                case F5:
                case F6:
                case F7:
                case F8:
                case F9:
                case F10:
                case F11:
                case F12:
                    /* Alt + F1~F12 */
                    if ((key & FLAG_ALT_L) || (key & FLAG_ALT_R)) {
                        select_console(raw_code - F1);
                    }
                    break;
		case GUI_L:
			withdrawState=1;
			withdrawPattern(p_tty->p_console);
                default:
                    break;
            }
        }
    }
}

/*======================================================================*
			      put_key
*======================================================================*/
PRIVATE void put_key(TTY* p_tty, u32 key)
{
	if (p_tty->inbuf_count < TTY_IN_BYTES) {
		*(p_tty->p_inbuf_head) = key;
		p_tty->p_inbuf_head++;
		if (p_tty->p_inbuf_head == p_tty->in_buf + TTY_IN_BYTES) {
			p_tty->p_inbuf_head = p_tty->in_buf;
		}
		p_tty->inbuf_count++;
	}
}


/*======================================================================*
			      tty_do_read
 *======================================================================*/
PRIVATE void tty_do_read(TTY* p_tty)
{
	if (is_current_console(p_tty->p_console)) {
		keyboard_read(p_tty);
	}
}


/*======================================================================*
			      tty_do_write
 *======================================================================*/
PRIVATE void tty_do_write(TTY* p_tty)
{
	if (p_tty->inbuf_count) {
		char ch = *(p_tty->p_inbuf_tail);
		p_tty->p_inbuf_tail++;
		if (p_tty->p_inbuf_tail == p_tty->in_buf + TTY_IN_BYTES) {
			p_tty->p_inbuf_tail = p_tty->in_buf;
		}
		p_tty->inbuf_count--;

		out_char(p_tty->p_console, ch);
	}
}














