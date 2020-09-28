
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			      console.c
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

PRIVATE void set_cursor(unsigned int position);
PRIVATE void set_video_start_addr(u32 addr);
PRIVATE void flush(CONSOLE* p_con);

extern  int patternState;//查找模式状态
extern int withdrawState;//标记撤销模式


int TabList[100];//记录是Tab 位置的光标
int EnterList[100];//记录回车位置的光标
int SpaceNumList[100];//记录回车位置光标与上一字符光标的差值
int TypeList[1000];
char DeleteList[500];

void addTab(int pos);//Tab 位置压栈
void addEnter(int pos, int spaceNum);
void addType(int type);
void addDelete(char DeleteChar);
int isTab(int pos);//判断当前光标位置是不是Tab键，1 是，0 不是
int isEnter(int pos);//判断当前光标位置是不是回车键，>0 是，0 不是. 返回spaceNum

int TabSP = -1;//记录输入过的tab的数量
int EnterSP=-1;//记录输入过的回车的数量
int TypeSP=-1;
int DeleteSP=-1;


int FPstrlen=0;//查找模式下需要查找的字符串的长度

/*======================================================================*
			   init_screen
 *======================================================================*/
PUBLIC void init_screen(TTY* p_tty)
{
	int nr_tty = p_tty - tty_table;
	p_tty->p_console = console_table + nr_tty;

	int v_mem_size = V_MEM_SIZE >> 1;	/* 显存总大小 (in WORD) */

	int con_v_mem_size                   = v_mem_size / NR_CONSOLES;
	p_tty->p_console->original_addr      = nr_tty * con_v_mem_size;
	p_tty->p_console->v_mem_limit        = con_v_mem_size;
	p_tty->p_console->current_start_addr = p_tty->p_console->original_addr;

	/* 默认光标位置在最开始处 */
	p_tty->p_console->cursor = p_tty->p_console->original_addr;

	if (nr_tty == 0) {
		/* 第一个控制台沿用原来的光标位置 */
		p_tty->p_console->cursor = disp_pos / 2;
		disp_pos = 0;
	}
	else {
		out_char(p_tty->p_console, nr_tty + '0');
		out_char(p_tty->p_console, '#');
	}

	set_cursor(p_tty->p_console->cursor);
}


/*======================================================================*
			   is_current_console
*======================================================================*/
PUBLIC int is_current_console(CONSOLE* p_con)
{
	return (p_con == &console_table[nr_current_console]);
}


/*======================================================================*
			   out_char
 *======================================================================*/
PUBLIC void out_char(CONSOLE* p_con, char ch)
{
	int tag = 0;//退格时tab和enter需要特殊考虑，1代表待退格的是tab，2代表enter
	int spaceStart = 0;
	int spaceEnd = 0;
	int spaceNum = 0;

	u8* p_vmem = (u8*)(V_MEM_BASE + p_con->cursor * 2);
	switch(ch) {
	case '\t': 
		for (int j = 0; j < 4; j++) {
			out_char(p_con, ' ');
		}
		addTab(p_con->cursor);
		if(withdrawState==0)
		addType(0);
		break;
	
	case '\n':
	    spaceStart=p_con->cursor;
		if (p_con->cursor < p_con->original_addr +
		    p_con->v_mem_limit - SCREEN_WIDTH) {
			p_con->cursor = p_con->original_addr + SCREEN_WIDTH * 
				((p_con->cursor - p_con->original_addr) /
				 SCREEN_WIDTH + 1);
		}
		spaceEnd=p_con->cursor;
		addEnter(spaceEnd,spaceEnd-spaceStart);
		if(withdrawState==0)
		addType(0);
		break;
	case '\b':
	    if(TabSP!=-1&&isTab(p_con->cursor)){
	        tag=1;
		if(withdrawState==0){
		addType(1);
		withdrawState=2;}
	    }else if(EnterSP!=-1&&(spaceNum=isEnter(p_con->cursor))>0) {
            	tag=2;
		if(withdrawState==0){
		addType(2);
		withdrawState=2;
}
       		}else{
			if(withdrawState==0)
			addType(3);
			char tempChar=*(p_vmem-2);
			addDelete(tempChar);		
		}

		if (p_con->cursor > p_con->original_addr) {
			p_con->cursor--;
			*(p_vmem-2) = ' ';
			*(p_vmem-1) = DEFAULT_CHAR_COLOR;
		}//无论什么情况先退一格

		if(tag==1){
	        for(int j=0;j<3;j++){
	        out_char(p_con,'\b');
		}
		if(withdrawState==2){
		withdrawState=0;		
		}


	    }else if(tag==2){ 
	        for(int j=0;j<spaceNum-1;j++){
                out_char(p_con,'\b');}
		if(withdrawState==2){
		withdrawState=0;		
		}

	    }
		break;
	default:
		if (p_con->cursor <
			p_con->original_addr + p_con->v_mem_limit - 1) {
			*p_vmem++ = ch;
			if (patternState == 0) {
				*p_vmem++ = DEFAULT_CHAR_COLOR;
			}
			else {
				*p_vmem++ = SPECIAL_CHAR_COLOR; //查找模式下，使用特殊颜色输出
			}
			p_con->cursor++;
		}
		if(withdrawState==0)
		addType(0);
		break;
	
	}

	while (p_con->cursor >= p_con->current_start_addr + SCREEN_SIZE) {
		scroll_screen(p_con, SCR_DN);
	}

	flush(p_con);
}

/*======================================================================*
                           flush
*======================================================================*/
PRIVATE void flush(CONSOLE* p_con)
{
        set_cursor(p_con->cursor);
        set_video_start_addr(p_con->current_start_addr);
}

/*======================================================================*
			    set_cursor
 *======================================================================*/
PRIVATE void set_cursor(unsigned int position)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, CURSOR_H);
	out_byte(CRTC_DATA_REG, (position >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, CURSOR_L);
	out_byte(CRTC_DATA_REG, position & 0xFF);
	enable_int();
}

/*======================================================================*
			  set_video_start_addr
 *======================================================================*/
PRIVATE void set_video_start_addr(u32 addr)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, START_ADDR_H);
	out_byte(CRTC_DATA_REG, (addr >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, START_ADDR_L);
	out_byte(CRTC_DATA_REG, addr & 0xFF);
	enable_int();
}



/*======================================================================*
			   select_console
 *======================================================================*/
PUBLIC void select_console(int nr_console)	/* 0 ~ (NR_CONSOLES - 1) */
{
	if ((nr_console < 0) || (nr_console >= NR_CONSOLES)) {
		return;
	}

	nr_current_console = nr_console;

	set_cursor(console_table[nr_console].cursor);
	set_video_start_addr(console_table[nr_console].current_start_addr);
}
/*======================================================================*
			   clean_screen
 *======================================================================*/
PUBLIC void clean_screen(CONSOLE* p_con)
{
    p_con->cursor = p_con->original_addr;
    u8* p_vmem = (u8*)(V_MEM_BASE + p_con->cursor * 2);
    while (p_con->cursor < p_con->original_addr + p_con->v_mem_limit - 1) {
        *p_vmem++ = '\0';
        *p_vmem++ = DEFAULT_CHAR_COLOR;
        p_con->cursor ++;
    }
    p_con->cursor = p_con->original_addr;
    p_con->current_start_addr = p_con->original_addr;
    flush(p_con);
}


/*======================================================================*
			   scroll_screen
 *----------------------------------------------------------------------*
 滚屏.
 *----------------------------------------------------------------------*
 direction:
	SCR_UP	: 向上滚屏
	SCR_DN	: 向下滚屏
	其它	: 不做处理
 *======================================================================*/
PUBLIC void scroll_screen(CONSOLE* p_con, int direction)
{
	if (direction == SCR_UP) {
		if (p_con->current_start_addr > p_con->original_addr) {
			p_con->current_start_addr -= SCREEN_WIDTH;
		}
	}
	else if (direction == SCR_DN) {
		if (p_con->current_start_addr + SCREEN_SIZE <
		    p_con->original_addr + p_con->v_mem_limit) {
			p_con->current_start_addr += SCREEN_WIDTH;
		}
	}
	else{
	}

	set_video_start_addr(p_con->current_start_addr);
	set_cursor(p_con->cursor);
}



void addTab(int pos){
    TabSP++;
    TabList[TabSP]=pos;

}
void addEnter(int pos, int spaceNum) {
	EnterSP++;
	EnterList[EnterSP] = pos;
	SpaceNumList[EnterSP] = spaceNum;
}
void addType(int type){
	TypeSP++;
	TypeList[TypeSP]=type;
}
void addDelete(char delChar){
    DeleteSP++;
    DeleteList[DeleteSP]=delChar;

}
int isTab(int pos){//0表示不为tab，1表示为tab
	int tag = 0;
	if(TabList[TabSP]==pos){
		TabSP--;
		tag = 1;
	 }
	return tag;
}
int isEnter(int pos){
    int tag=0;
    if(EnterList[EnterSP]==pos){
        tag=SpaceNumList[EnterSP];
		EnterSP--;
    }
        return tag;
}

PUBLIC void outPattern(CONSOLE* p_con){
    //恢复字体颜色并输入对应数量的退格
withdrawState=2;

    u8* p_vmem = (u8*)(V_MEM_BASE + p_con->cursor * 2);
    for(int i=0;i<p_con->cursor-p_con->original_addr;i++){
        if(*(p_vmem-2*i-1)==SPECIAL_CHAR_COLOR){
            *(p_vmem-2*i-1)=DEFAULT_CHAR_COLOR;
        }
    }
int i=0;
	while( i < FPstrlen) {
		if(TabSP!=-1&&isTab(p_con->cursor)){
			 for(int j=0;j<4;j++){
	        out_char(p_con,'\b');
		}
			i+=4;	
		}else{
			out_char(p_con,'\b');	
		i+=1;	
		}

	}
withdrawState=0;
}
PUBLIC void findPattern(CONSOLE* p_con){
	//根据特殊颜色判断要查找的字符的长度，并循环比对
    char FPstrList[500];
	int  index=0;
	u8* p_vmem = (u8*)(V_MEM_BASE + p_con->cursor * 2);
    for(int i=0;i<p_con->cursor-p_con->original_addr;i++){
        if(*(p_vmem-2*i-1)==SPECIAL_CHAR_COLOR){
            FPstrList[index]=*(p_vmem-2*i-2);
            index++;
        }
    }
    FPstrlen=index;
    for(int i=0;i<p_con->cursor-p_con->original_addr;i++){
    	int tag=1;
         for(int j=0;j<index;j++){
         	if(*(p_vmem-2*j-2*i-2)!=FPstrList[j]){
         		tag=0;
         		break;
         	}
         }
         if(tag==1){
         	for(int j=0;j<index;j++){
         		*(p_vmem-2*j-2*i-1)=SPECIAL_CHAR_COLOR;
         	}
         }
    }

}


PUBLIC void withdrawPattern(CONSOLE* p_con){
	//查找上一步的类型，如果是输入就执行回退，如果是删除，那么从删除列表读出被删掉的字符，并且输入。
	if(TypeSP!=-1){
		int tempTypeState=TypeList[TypeSP];
		TypeSP--;
		if(tempTypeState==0){
			out_char(p_con,'\b');
		}else if(tempTypeState==1){
			out_char(p_con,'\t');


		}else if(tempTypeState==2){
			out_char(p_con,'\n');


		}else{
			char delChar=DeleteList[DeleteSP];
			DeleteSP--;
			out_char(p_con,delChar);

		}

	}
withdrawState=0;    

}
