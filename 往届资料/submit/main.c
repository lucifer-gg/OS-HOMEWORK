#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FALSE 0
#define TRUE 1
#define ROOT_START 0x2600 //19*512=0x2600 根目录区起始位置
#define DIRNAMELENGTH 15
#define RESERVENUM 10
#define MAXDIRLENGTH 70
#define MAXDIRNUM 50

#pragma pack (1)

struct BPB {
	unsigned short BPB_BytsPerSec;//每个扇区的字节数
	unsigned char BPB_SecPerClus;//每簇扇区数
	unsigned short BPB_RsvdSecCnt;//boot记录共占用多少扇区
	unsigned char BPB_NumFATs;//共有多少FAT表
	unsigned short BPB_RootEntCnt;//根目录文件数最大值
	unsigned short BPB_TotSec16;//扇区总数
	unsigned char BPB_Media;//介质描述符
	unsigned short BPB_FATSz16;//每FAT扇区数
	unsigned short BPB_SecPerTrk;//每磁道扇区数
	unsigned short BPB_NumHeads;//磁头数（面数）
	unsigned int BPB_HiddSec;//隐藏扇区数
	unsigned int BPB_TotSec32;//BPB_TotSec16为0时记录扇区数
};

//#pragma pack (1)

struct RootEntry {    
	char DIR_Name[11];//文件名8字节，扩展名3字节
	char DIR_Attr;//文件属性
	char reserved[RESERVENUM];//保留位（10位）
	short  DIR_WrtTime;//最后一次写入时间
	short  DIR_WrtDate;//最后一次写入日期
	short  DIR_FstClus;//此条目对应的开始簇号
	int  DIR_FileSize;//文件大小
};

#pragma pack (1)

struct File {
	char fileName[MAXDIRLENGTH];
	int isFile;
	int startClus;
};

struct Count {
	char dirName[12];
	int fileNum;
	int dirNum;
};

int fileNum = 0;
int BytsPerSec=512;
int SecPerClus=1;
int ResvdSecCnt=1;
int NumFATs=2;
int RootEntCnt=14;
int FATSz16=9;
int BytsPerClus=512;
int dataStartPlace;//数据区开始位置
struct File files[MAXDIRNUM];

FILE* fat12;

void my_print(char* c,int length, int color);
void loadInBPB(FILE* file, struct BPB* bpb_ptr);
void readFiles(FILE* file);
int isValidName(char name[]);
void readDirName(char dir[], char DIR_Name[]);
void readFileName(char fileName[], char DIR_Name[]);
void printDir(FILE* file, char dir[], int DIR_FstClus);
void searchDir();
void addFile(char fileName[], int DIR_FstClus, int isFile);
void readFileContent(FILE* file, int DIR_FstClus);
int getNextClus(int clus, FILE* file);
void countDir_root(char toFind[]);
void countDir_sub(char toFind[], char dir[], int DIR_FstClus, int setBack, char sub[]);
char* itoa(int num);

int main(){
	fat12 = fopen("a.img", "rb");
	struct BPB bpb;
	struct BPB* bpb_ptr = &bpb;
	loadInBPB(fat12, bpb_ptr);//加载文件到BPB中
	readFiles(fat12);//读取所有文件和目录
	searchDir();//用户输入部分
}


//加载文件
void loadInBPB(FILE* file, struct BPB* bpb_ptr) {
	fseek(file, 11, SEEK_SET);//从第11偏移量开始读（没有跳转指令和厂商名）
	fread(bpb_ptr, 1, 25, file);//每次读1个单元，一共读25个偏移量，读到指针所指的地方（即BPB）
	
	//读完文件后，给变量赋值
	BytsPerSec = bpb_ptr -> BPB_BytsPerSec;
	SecPerClus = bpb_ptr -> BPB_SecPerClus;
	RsvdSecCnt = bpb_ptr -> BPB_RsvdSecCnt;
	NumFATs = bpb_ptr -> BPB_NumFATs;
	RootEntCnt = bpb_ptr -> BPB_RootEntCnt;
	FATSz16 = bpb_ptr -> BPB_FATSz16;
	BytsPerClus = BytsPerSec * SecPerClus;
	if(FATSz16 == 0) {
		//BPB_TotSec16为0时，记录扇区数
		FATSz16 = bpb_ptr -> BPB_TotSec32;
	}
}

void readFiles(FILE* file) {
	struct RootEntry rootEntry;
	struct RootEntry* root_ptr = &rootEntry;
	
	//计算出开始位置
	int startPlace = (RsvdSecCnt + NumFATs * FATSz16) * BytsPerSec;
	//printf("%d\n", startPlace);
	
	int offset = 0;
	int isEmpty = TRUE;
	while(offset <= RootEntCnt) {
		fseek(file, startPlace + offset * 32, SEEK_SET);
		fread(root_ptr, 1, 32, file);//根目录中每个条目占32个字节，每次读1个字节
		//printf("%s\n", root_ptr -> DIR_Name);
		
		if(!isValidName(root_ptr -> DIR_Name)) {
			//如果不是有效文件名，则跳过继续读下一个
			offset++;
			continue;
		}
		
		if(root_ptr -> DIR_Attr == 0x10) {
			//DIR_Attr为0x10表示有子目录，读取路径名
			
			isEmpty = FALSE;
			char dir[15];
			readDirName(dir, root_ptr -> DIR_Name);
			char temp[MAXDIRLENGTH];					
			printDir(file, dir, root_ptr -> DIR_FstClus);	
		} else {
			//没有子目录，则直接读取文件名
			isEmpty = FALSE;
			char fileName[15];
			readFileName(fileName, root_ptr -> DIR_Name);
			strcat(fileName, "\0");
				
			addFile(fileName, root_ptr -> DIR_FstClus, TRUE);
			//printf("%s\n", fileName);
			/*for(int i = 0; i < strlen(fileName); i++) {
				my_print(&fileName[i], 1);
			}*/
			my_print(fileName, strlen(fileName), 1);
			my_print("\n", 2, 0);
		}
		offset++;		
	}
		
	if(isEmpty) {
		char dir[12];
		readDirName(dir, root_ptr -> DIR_Name);
		addFile(dir, root_ptr -> DIR_FstClus, FALSE);
		/*for(int i = 0; i < strlen(dir); i++) {
			my_print(&dir[i], 1);
		}*/
		//printf("%s\n", dir);
		my_print(dir, strlen(dir), 1);
		my_print("\n", 2, 0);
	}	
}

int isValidName(char name[]) {
	//如果不是数字或字母，则不是有效文件名
	for(int i = 0; i < 11; i++) {
		if( !((name[i] >= '0' && name[i] <= '9') || (name[i] >= 'a' && name[i] <= 'z') || (name[i] >= 'A' && name[i] <= 'Z') || name[i] == 0x20)) {
			return FALSE;
		}
	}	
	return TRUE;
}

void readDirName(char dir[], char DIR_Name[]) {
	//读取路径名到dir数组中，直到空格为止
	for(int i = 0; i < 12; i++) {
		if(DIR_Name[i] == 0x20) {
			dir[i] = '\0';
			return;
		} else {
			dir[i] = DIR_Name[i];
		}
	}
}

void readFileName(char fileName[], char DIR_Name[]) {
	//读取文件名到fileName数组中，直到空格为止
	//其中8位为文件名，3位为扩展名
	int i = 0;
	for(; i < 8; i++){
		if(DIR_Name[i] == 0x20) {
			//遇到空格跳出
			break;
		} else {
			fileName[i] = DIR_Name[i];
		}
	}
	fileName[i] = '.';
	for(int j = 8; j < 11; j++){
		//添加扩展名
		fileName[i + 1] = DIR_Name[j];
		i++;
	}
	fileName[i + 1] = '\0';
}

void printDir(FILE* file, char dir[], int DIR_FstClus) {
	//读取此路径下的文件名
	int isEmpty = TRUE;
	
	//printf("%d\n", isFinding);
	
	//计算开始位置
	dataStartPlace = (RsvdSecCnt + NumFATs * FATSz16 + ((RootEntCnt * 32) + (BytsPerSec - 1)) / BytsPerSec) * BytsPerSec;
	
	struct RootEntry rootEntry;
	struct RootEntry* root_ptr = &rootEntry;
	
	int offset = 0;
	while(offset < BytsPerClus / 32) {		
		fseek(file, dataStartPlace + (DIR_FstClus - 2) * BytsPerClus + (offset * 32), SEEK_SET);
		fread(root_ptr, 1, 32, file);
		
		if(!isValidName(root_ptr -> DIR_Name)) {
			//如果不是有效路径名，则跳过继续读下一个
			offset++;
			continue;
		}
		
		if(root_ptr -> DIR_Attr == 0x10) {
			//DIR_Attr为0x10表示有子目录，读取子路径名
			isEmpty = FALSE;
			char subDir[15];
			readDirName(subDir, root_ptr -> DIR_Name);
			
			//把路径前缀连上去
			char temp[70];
			strcpy(temp, dir);
			strcat(temp, "/");
			strcat(temp, subDir);
			//strcat(temp, "/");

			printDir(file, temp, root_ptr -> DIR_FstClus);		
				
		} else {
			isEmpty = FALSE;
			//没有子目录，则直接读取文件名
			char fileName[15];
			
			readFileName(fileName, root_ptr -> DIR_Name);
			
			char temp[70];
			strcpy(temp, dir);
			strcat(temp, "/");
			strcat(temp, fileName);
			//strcat(temp, "/");
			strcat(temp, "\0");
			//printf("%s\n", temp);
			addFile(temp, root_ptr -> DIR_FstClus, TRUE);
			/*for(int i = 0; i < strlen(temp); i++) {
				my_print(&temp[i], 1);
			}*/
			my_print(temp, strlen(temp), 1);
			my_print("\n", 2, 0);
		}
		offset++;	
	}
	
	if(isEmpty) {
		//如果是空目录，则直接打印
		//printf("%s\n", "***");
		char subDir[15];
		readDirName(subDir, root_ptr -> DIR_Name);
			
		//把路径前缀连上去
		char temp[70];
		strcpy(temp, dir);
		strcat(temp, "/");
		strcat(temp, subDir);
		//strcat(temp, "/");
		strcat(temp, "\0");		
		//printf("%s\n", temp);
		
		addFile(temp, root_ptr -> DIR_FstClus, FALSE);
		/*for(int i = 0; i < strlen(temp); i++) {
			my_print(&temp[i], 1);
		}*/
		my_print(temp, strlen(temp), 1);
		my_print("\n", 2, 0);
	}	
}

int countFound = FALSE;
void searchDir() {
	//提示用户输入命令
	char instruction[MAXDIRLENGTH];//保存用户输入的指令
	char outPut[] = "Please input your instructions: ";
	char notDir[] = "Not a directory!";
	char unknownPath[] = "Unknown path!";
	
	while(TRUE) {
		//printf("%s", "Please input your instructions: ");
		my_print(outPut, strlen(outPut), 0);
		scanf("%s", instruction);
		
		if(strcmp("esc", instruction) == 0) {
			//退出指令
			break;
		} else if(strcmp(instruction, "count") == 0) {
			//count指令
			countFound = FALSE;
			char tempIns[MAXDIRLENGTH];
			scanf("%s", tempIns);
			if(strstr(tempIns, ".TXT") != NULL) {
				//printf("%s\n", "Not a directory!");
				my_print(notDir, strlen(notDir), 2);
				my_print("\n", 2, 0);
			} else {
				countDir_root(tempIns);
				if(countFound == FALSE) {
					//printf("%s\n", "Not a directory!");
					my_print(notDir, strlen(notDir), 2);
					my_print("\n", 2, 0);
				}	
			}
			
		} else {
			//readFiles(fat12, TRUE, instruction);
			int found = FALSE;
			for(int i = 0; i < fileNum; i++) {
				if(strstr(files[i].fileName, instruction) != NULL) {
					found = TRUE;
					if(strstr(instruction, ".TXT") != NULL) {
						readFileContent(fat12, files[i].startClus);
					} else {
						//printf("%s\n", files[i].fileName);
						my_print(files[i].fileName, strlen(files[i].fileName), 1);
						my_print("\n", 2, 0);
					}					
				}
			}
			if(!found) {
				//printf("%s\n", "Unknown path");
				my_print(unknownPath, strlen(unknownPath), 2);
				my_print("\n", 2, 0);
			}
		}
	}
}

void addFile(char fileName[], int DIR_FstClus, int isFile) {
	//把路径名存入数组
	strcpy(files[fileNum].fileName, fileName);
	files[fileNum].startClus = DIR_FstClus;
	files[fileNum].isFile = isFile;
	fileNum++;
}

void readFileContent(FILE* file, int DIR_FstClus) {
	//读取文件内容
	unsigned int clus = DIR_FstClus;
	long ptr = ftell(file);//获取文件当前读取位置
	//存储每一个簇里的内容
	char* content = (char*)malloc(512);
	
	while(clus < 0xFF8) {
		if(clus == 0xFF7) {
			printf("%s\n", "Bad Classter!");
		}
		int seek = dataStartPlace + (clus - 2) * 512;
		fseek(file, seek, SEEK_SET);
		fread(content, 1, 512, file);
		
		int offset = 0;
		while(offset < 512) {
			if(content[offset] == '\0' || content[offset] == '\r') {
				break;
			}
			//printf("%c", content[offset]);
			my_print(&content[offset], 1, 0);
			offset++;
		}
		clus = getNextClus(clus, file);
	}
	
	free(content);
	fseek(file, ptr, SEEK_SET);
	
}

int getNextClus(int clus, FILE* file) {
	//获得下一个簇号
	int FATbegin = 512;//FAT1的开始偏移
	//一个FAT表项存储为3个字节，若为奇数，则取第二个字节起始位置。若为偶数则取第一个字节起始地址
	int clusPos = FATbegin + clus * 3 / 2;
	//读出该FAT项所在的两个字节
	unsigned short readOut;
	unsigned short* read_ptr = &readOut;
	
	fseek(file, clusPos, SEEK_SET);
	fread(read_ptr, 1, 2, file);
	
	//如果簇号是偶数，取两个字节的低12位
	//如果簇号是奇数，取两个字节的高12位
	if(clus % 2 == 0) {
		return readOut & 0xFFF;
	} else {
		return readOut >> 4;
	}
}

void countDir_root(char toFind[]) {
	//count指令
	struct RootEntry rootEntry;
	struct RootEntry* root_ptr = &rootEntry;
	
	//计算出开始位置
	int startPlace = (RsvdSecCnt + NumFATs * FATSz16) * BytsPerSec;
	
	int isEmpty = TRUE;
	
	int offset = 0;
	while(offset <= RootEntCnt) {
		fseek(fat12, startPlace + offset * 32, SEEK_SET);
		fread(root_ptr, 1, 32, fat12);//根目录中每个条目占32个字节，每次读1个字节
		
		if(!isValidName(root_ptr -> DIR_Name)) {
			//如果不是有效文件名，则跳过继续读下一个
			offset++;
			continue;
		}
		if(root_ptr -> DIR_Attr == 0x10) {
			//DIR_Attr为0x10表示有子目录，读取路径名
			isEmpty = FALSE;
			char dir[11];
			readDirName(dir, root_ptr -> DIR_Name);
			char temp[MAXDIRLENGTH];				
			
			countDir_sub(toFind, dir, root_ptr -> DIR_FstClus, 0, dir);		
		}
		offset++;		
	}
	
	if(isEmpty) {
		char dir[11];
		readDirName(dir, root_ptr -> DIR_Name);
		if(strcmp(dir, toFind) == 0) {
			//printf("%s", dir);
			my_print(dir, strlen(dir), 0);
			//printf("%s\n", ": 0 file, 0 directory");
			char temp[] = ": 0 file, 0 directory";
			my_print(temp, strlen(temp), 0);
			my_print("\n", 2, 0);
		}
		//printf("%s\n", dir);
	}
}

void countDir_sub(char toFind[], char dir[], int DIR_FstClus, int setBack, char sub[]) {
	//计数子目录
	int countDir = 0;
	int countFile = 0;
	
	struct RootEntry rootEntry;
	struct RootEntry* root_ptr = &rootEntry;
	
	int offset = 0;
	char currentDir[MAXDIRLENGTH];
	char subDir[11];
	while(offset < BytsPerClus / 32) {		
		fseek(fat12, dataStartPlace + (DIR_FstClus - 2) * BytsPerClus + (offset * 32), SEEK_SET);
		fread(root_ptr, 1, 32, fat12);
			
		if(!isValidName(root_ptr -> DIR_Name)) {
			//如果不是有效路径名，则跳过继续读下一个
			offset++;
			continue;
		}
		
		if(root_ptr -> DIR_Attr == 0x10) {
			//DIR_Attr为0x10表示有子目录，读取子路径名
			countDir++;
			//isEmpty = FALSE;
			//char subDir[11];
			readDirName(subDir, root_ptr -> DIR_Name);
			
			
			//把路径前缀连上去
			char temp[MAXDIRLENGTH];
			strcpy(temp, dir);
			strcat(temp, "/");
			strcat(temp, subDir);
			strcpy(currentDir, temp);
				
		} else if(root_ptr -> DIR_Attr == 0x20) {
			countFile++;
		}
		
		//printf("%s\n", currentDir);

		offset++;	
	}
	
	if(strstr(dir, toFind) != NULL) {
		countFound = TRUE;
		for(int i = 0; i < setBack; i++) {
			//printf("\t");
			my_print("\t", 2, 0);
		}

		char* fileNum_str;
		fileNum_str = itoa(countFile);

		char* dirNum_str;
		dirNum_str = itoa(countDir);

		//printf("%s", sub);
		//printf("%s", ": ");
		//printf("%d", countFile);
		//printf("%s", " file, ");
		//printf("%d", countDir);
		//printf("%s\n", " directory");
		my_print(sub, strlen(sub), 1);
                my_print(": ", 2, 1);
		my_print(fileNum_str, strlen(fileNum_str), 1);
		my_print(" file, ", 7, 1);
		my_print(dirNum_str, strlen(dirNum_str), 1);
		my_print(" directory\n", 12, 1);
		setBack++;
	}
		
	offset = 0;
	while(offset < BytsPerClus / 32) {		
		fseek(fat12, dataStartPlace + (DIR_FstClus - 2) * BytsPerClus + (offset * 32), SEEK_SET);
		fread(root_ptr, 1, 32, fat12);
		
		if(!isValidName(root_ptr -> DIR_Name)) {
			//如果不是有效路径名，则跳过继续读下一个
			offset++;
			continue;
		}
		
		if(root_ptr -> DIR_Attr == 0x10) {
			//DIR_Attr为0x10表示有子目录，读取子路径名
			//char subDir[11];
			readDirName(subDir, root_ptr -> DIR_Name);
			//把路径前缀连上去
			char temp[MAXDIRLENGTH];
			strcpy(temp, dir);
			strcat(temp, "/");
			strcat(temp, subDir);
			strcpy(currentDir, temp);
			//strcat(temp, "/");

			//printDir(file, temp, root_ptr -> DIR_FstClus);*/	
			countDir_sub(toFind, currentDir, root_ptr -> DIR_FstClus, setBack, subDir);
		}
		offset++;
	}	
}

char* itoa(int num) {
	//把整数转化成字符串的方法
	char temp[5];
	char* res = (char*)malloc(5);
	int len = 0;
	int q = num;
	while(q != 0) {
		temp[len] = q % 10 + '0';
		len++;
		q = q / 10;
	}
	for(int i = 0; i < len; i++) {
		res[i] = temp[len-1-i];
	}
	if(len == 0) {
		res[0] = '0';
		res[1] = '\0';
	} else {
		res[len] == '\0';
	}
	return res;
}









