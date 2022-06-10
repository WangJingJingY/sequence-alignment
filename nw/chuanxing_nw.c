//0号进程负责文件的读、写以及回溯，所有进程共同负责打分矩阵的计算
//主核负责IO、文件的读写、储存空间的开辟、打分矩阵的回溯以及打分矩阵第1行/列的计算
//概念：
//打分矩阵元：一个124*124的打分矩阵部分
//打分矩阵元组：由1~64个打分矩阵元组成的元组，一个元组会在一个计算周期被一个进程计算完毕
//打分矩阵超元组：由1~64*P个打分矩阵元组成，一个超元组会在一个计算周期被所有进程共同计算完毕
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<time.h>
#include<getopt.h>


//124本也该定义为宏
#define I 50000					//I为比对序列的最大长度
#define d -5					//d为空位罚分, 三个预设参数之一
#define d1 5					//d为match得分, 三个预设参数之一
#define d2 -4					//d为mismatch得分, 三个预设参数之一
#define max_line 1024			//规定读取文件时每次读取的一行中最大的字符数

int matrix[I][I];	
char a1[I],a2[I];			//为全局变量，存储在主存中用来传递参数
volatile char l1[I*2];					//l1为s1的比对结果序列
volatile char l2[I*2];					//l2为s2的比对结果序列
int start_num,loop;
int row,col;

static void fun(char *str)		//序列倒置，用于将比对结果序列的顺序纠正
{
	int len=strlen(str);
	char temp;
	int i;
	for(i=0;i<len/2;i++)
	{
		temp=str[i];
		str[i]=str[len-1-i];
		str[len-1-i]=temp;
	}
}

void write(char *line1,char *line2,int count_num)      //输出比对文件
{
    FILE *fq=NULL;
        fq=fopen("result.txt","a+");
    //char *first="The result is:\n";
    if(fq==NULL)
    {
        printf("can't open file\n");
    }
    else{
        //char *first= "The result is:\n";
        fputs("The result is:\n",fq);
        int num,len1,len2,sub;
        int cut_n=count_num/64+1;
        int local;
        for(num=1;num<=cut_n;num++){
            int sub;
            char p1[65],p2[65];
            local=(num-1)*64;
            int cut=64;
            if(num*64>count_num)
            {
                cut=count_num-local;
            }
            char chip_s[cut+1];
            for(sub=0;sub<cut;sub++)
            {
                chip_s[sub]='|';
            }
            chip_s[cut]='\0';
            strncpy(p1,line1+local,cut);
            p1[cut]='\0';
            strncpy(p2,line2+local,cut);
            p2[cut]='\0';
            fputs(p1,fq);
            fputs("\n",fq);
            fputs(chip_s,fq);
            fputs("\n",fq);
            fputs(p2,fq);
            fputs("\n\n",fq);
        }
    }
    fclose(fq);
}
static int full_num(char *line)
{
	int full_len=strlen(line);
	int full;
	for(full=0;full_len%4!=0;full++)
	{
		full_len=full_len+full;
	}
	return full_len;
}

void read(char *sub1,char *sub2,int *s1,int *s2,char *filename)//读取txt文件的内容函数
{
	FILE *file;
	char buf[max_line];
	int p1,p2,i=1;
	file=fopen(filename,"r");//打开TXST.TxT文件
	char name='1';
	if(!file)
	{
		printf("can't open file\n");
	}
	else{
		while(fgets(buf,max_line,file)!=NULL)//读取TXT中字符
		{
			if(i==1)
			{
				sscanf(buf,"%d",&p1);
				*s1=p1;
				i++;
			}
			else if(i==2)
			{
				sscanf(buf,"%d",&p2);
				*s2=p2;
				i++;
			}
			else {
				if(buf[1]=='2')
				{
					name='2';
					continue;
				}
				else if (buf[1]=='1') {
					continue;
				}
				else{
					int len=strlen(buf);
					if(buf[len-1]=='\n')
					{
						buf[len-1]='\0';
					}
					if(name=='1')
					{
						strcat(sub1,buf);
					}
					else {
						strcat(sub2,buf);
					}
				}
			}
		}
	}
	fclose(file);
}

int main(int argc, char** argv)
{
	clock_t time_start1,time_over1;	//计时1包含了打分矩阵的计算与回溯以及文件的读写的耗时
	clock_t time_start2,time_over2;	//计时2包含了打分矩阵的计算与回溯的耗时
	clock_t time_start3,time_over3;	//计时3只包含了打分矩阵的计算部分用时
	double run_time,run_time1,run_time2,run_time3;
    char *input_file;
    int match,mis_match,gap;
    int opt;
    int option_index = 0;
    char* string = "i:maD";
    
    static struct option long_options[] =
	{
		{"match", optional_argument,NULL, '1'},
		{"mismatch", optional_argument,NULL, '2'},
		{"gap",  optional_argument,      NULL,'3'},
		{NULL,     0,                NULL, 0},
	};
    while ((opt = getopt_long(argc,argv, string, long_options, &option_index)) != -1)
	{
		switch (opt)
		{
        	case '1':
				match = atoi(optarg);
				break;
			case '2':
				mis_match = atoi(optarg);
				break;
			case '3':
				gap = atoi(optarg);
				break;
			case 'i':
				input_file = optarg;
				break;
		}
	}	
    
    time_start1=clock();			//计时1开始
    read(a1,a2,&match,&mis_match, input_file);
    time_start2=clock();			//计时2开始
    char l1[I];
	char l2[I];
	col=strlen(a1);
	row=strlen(a2);
   
    printf("row=%d\tcol=%d\n",row,col);
		
    time_start3=clock();            //计时3开始
	int o;
        for(o=0;o<=col;o++)			//计算打分矩阵的第1行的值
        {
            matrix[0][o]=o*d;
        }
        for(o=0;o<=row;o++)			//计算打分矩阵的第1列的值
        {
            matrix[o][0]=o*d;
        }
        int i,j;
        for(i=1;i<=row;i++)	//开始计算打分矩阵元，i为行
        {
            for(j=1; j<=col; j++)	//j为列
            {
                int t1,t2,t3,t3_score,t_max;	//t3_score为比对的分值（匹配为5，未匹配为-4），t1/2/3分别表示由上/左/对角方向得到的值，tmax为t1/2/3中的最大值
                if(a2[i-1]== a1[j-1])
                {
                    t3_score = match;
                }
                else
                {
                    t3_score = mis_match;
                }
                t2=matrix[i][j-1]+d;
                t3=matrix[i-1][j-1]+t3_score;
                t1=matrix[i-1][j]+d;
                t_max=t3;		//比较t1/2/3
                if(t2>t3)
                {
                    t_max=t2;	//比较t1/2/3
                }
                if(t1>t_max)	//比较t1/2/3
                {
                    t_max=t1;
                }
                matrix[i][j]=t_max;	//打分矩阵元中第(i,j)的元素计算完成
            }
        }
    printf("计算完成!\n");
    time_over3=clock();                     //计时3结束
    printf("match=%d\tmis_match=%d\tgap=%d\n",match,mis_match,d);
    printf("score:%d\n",matrix[row][col]);
    
    int recall_x,recall_y;
	int s1_l=0,s2_l=0,count=0;
	
	for(recall_x=row,recall_y=col;recall_x+recall_y>0;)
	{
		int d_score;
		if(a1[recall_y-1]==a2[recall_x-1])
			d_score=5;
		else 
			d_score=-4;
		if(matrix[recall_x-1][recall_y-1]+d_score==matrix[recall_x][recall_y])
		{
			l1[s1_l]=a1[recall_y-1];
			l2[s2_l]=a2[recall_x-1];
			recall_x--;
			recall_y--;
			count++;
			s1_l++;
			s2_l++;
			continue;
		}
		else{
			if(matrix[recall_x-1][recall_y]+d==matrix[recall_x][recall_y])
			{
				l1[s1_l]='_';
				l2[s2_l]=a2[recall_x-1];
				recall_x--;
				count++;
				s1_l++;
				s2_l++;
				continue;
			}
			else{
				l1[s1_l]=a1[recall_y-1];
				l2[s2_l]='_';
				recall_y--;
				count++;
				s1_l++;
				s2_l++;
				continue;
			}
		}
	}
	l1[count]='\0';
	l2[count]='\0'; 
	fun(l1);
	fun(l2);
    time_over2=clock();			//计时2结束
    
	FILE *fp;
	fp=fopen("result.txt","w");
	write(l1,l2,count);  
    time_over1=clock();			//计时1结束
    
    run_time1=(double)(time_over1-time_start1)/CLOCKS_PER_SEC;
    run_time2=(double)(time_over2-time_start2)/CLOCKS_PER_SEC;
	run_time3=(double)(time_over3-time_start3)/CLOCKS_PER_SEC;
    printf("the total run time:%fs\n",run_time1);	
    printf("the running time of the algorithm=%fs\n",run_time2);	
	printf("the running time of calculating matrix=%fs\n",run_time3);	//计时3只包含了打分矩阵的计算部分用时

    
}



