#include<string.h>
#include<stdio.h>
#include<stdlib.h>		//各种功能函数库
#include <iostream>
#include "hip/hip_runtime.h"
#include <hip/hip_runtime_api.h>
#include <hip/hip_ext.h>

#include<sys/types.h>	//基本系统数据类型
#include<sys/stat.h>	//获取文件的状态
#include<fcntl.h>		//文件的打开、数据写入、数据读取、关闭文件的操作

#define I 30000
#define d -5
#define max_line 1024

//int matrix[I][I];	//volatile是一个关键字，标志该变量是一个易变变量，保证了每一次取该数组的值时都会重新从内存中取值而不是放到寄存器中，保证了正确性
char a1[I],a2[I];			//为全局变量，存储在主存中用来传递参数
int start_num,loop;
int *d_matrix;

static void fun(char *str)//序列倒置
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
}

//#########计算打分矩阵
__global__  void  func(int row,int col,int match,int mis_match ,int start_num,int loop,char *d_a1,char *d_a2,int *d_matrix){	//把计算密集区的代码写成一个从核函数

    int matrix_slave[124][124];
    char b1[125],b2[125];
    int start_slave,loop_slave;
    int row_slave,col_slave;
    int mat_s,mis_s;
    int matrix_firstup[125];
    int matrix_firstleft[124];
    int i,j,o;
    int sn_r,sn_c;
    
    start_slave=start_num;
    loop_slave=loop;
    mat_s=match;
    mis_s=mis_match;
    row_slave=row;
    col_slave=col;
 
    int tid = threadIdx.x + blockIdx.x * blockDim.x;        //grid划分成1维，block划分成1维
    
	if(tid<loop)
	{    
	    int b1_start=(loop-tid-1)*124;
        int b2_start=(start_num-1)*124*64+tid*124;
  
        memcpy(&b1,&d_a1[b1_start],124);
        memcpy(&b2,&d_a2[b2_start],124);
        b1[124]='\0';
        b2[124]='\0';

   		int row_id_up=b2_start;
        int col_id_up=b1_start;
        sn_r=row-b2_start;   //剩余的行数
        sn_c=col-b1_start;    //剩余的列数
        
        memcpy(&matrix_firstup,&d_matrix[row_id_up*(col+1)+col_id_up], 125*4);      

         if(sn_r>=124){
               for(o=1;o<125;o++){
                      matrix_firstleft[o-1]=d_matrix[(b2_start+o)*(col+1)+col_id_up];                           
                }
          }else {
                   for(o=1;o<=sn_r;o++){
                   matrix_firstleft[o-1]=d_matrix[(b2_start+o)*(col+1)+col_id_up];                           
                    }  
           }

		for(i=0;i<124;i++)
		{
			int t1,t2,t3,t_max;
			if(b2_start+i>=row_slave){
				break;
			}
			else {
				for(j=0;j<124;j++){
					int t1,t2,t3,t3_score,t_max;
					if(b1_start+j>=col_slave){
							break;
					}
					else{
						if(b1[j]==b2[i]){
							t3_score=mat_s;
						}
						else{
							t3_score=mis_s;
						}
                        
                        //########################################
						if(i==0){
							if(j==0){
								t3=matrix_firstup[0]+t3_score;
								t2=matrix_firstleft[0]+d;
							}
							else{
								t2=matrix_slave[0][j-1]+d;
								t3=matrix_firstup[j]+t3_score;
							}
							t1=matrix_firstup[j+1]+d;
						}
						else{
							if(j==0){
								t2=matrix_firstleft[i]+d;
								t3=matrix_firstleft[i-1]+t3_score;
							}
							else{
								t2=matrix_slave[i][j-1]+d;
								t3=matrix_slave[i-1][j-1]+t3_score;
							}
							t1=matrix_slave[i-1][j]+d;
						}
                        //############################################
                   
						t_max=t1;
						if(t2>t1)
						{
							t_max=t2;
						}
						if(t3>t_max)
						{
							t_max=t3;
						}
						matrix_slave[i][j]=t_max;
					}
				}
                
				if(j!=0)
                {
                	memcpy(&d_matrix[(b2_start+i+1)*(col+1)+b1_start+1],&matrix_slave[i][0],j*4);			
				}
            }
        } 
        
	}
   
}


void read(char *sub1,char *sub2,int *s1,int *s2)//读取txt文件的内容函数
{
	FILE *file;
	char buf[max_line];
	int p1,p2,i=1;
	file=fopen("text.txt","r");//打开TXST.TxT文件
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

int main()
{
    int row,col,match,mis_match;
    char *d_a1,*d_a2;
    int loop_r,loop_c,i,j;
   
    hipEvent_t start1, stop1,start2,stop2,start3,stop3;

	//对已声明事件进行创建
	hipEventCreate(&start1);
	hipEventCreate(&stop1);
    hipEventCreate(&start2);
	hipEventCreate(&stop2);
    hipEventCreate(&start3);
	hipEventCreate(&stop3);

	float eventMs1 = 1.0f;
    float eventMs2 = 1.0f;
    float eventMs3 = 1.0f;

    hipEventRecord(start1, NULL);

  
	read(a1,a2,&match,&mis_match);
   
    hipEventRecord(start2, NULL);
    
   
	char l1[I];
	char l2[I];
	col=strlen(a1);
	row=strlen(a2);
     int M=(row+1)*(col+1);
     int matrix[M];
     for(i=0;i<20;i++){
     printf("%c, ",a1[i]);
     }
     printf("\n");
    
     hipEventRecord(start3, NULL);
     
     
	int o;
	for(o=0;o<col+1;o++)
	{
		matrix[o]=o*d;
	}
	for(o=0;o<row+1;o++)
	{
		matrix[o*(col+1)]=o*d;
	}

    hipMalloc((void **)&d_a1, col*sizeof(char));
    hipMalloc((void **)&d_a2, row*sizeof(char));
    hipMalloc((void **)&d_matrix, M*sizeof(int));
    hipMemcpy(&d_a1[0],&a1 , sizeof(char) *col, hipMemcpyHostToDevice);
    hipMemcpy(&d_a2[0],&a2 , sizeof(char) * row, hipMemcpyHostToDevice);
    hipMemcpy(d_matrix,matrix,sizeof(int) * M , hipMemcpyHostToDevice);

	dim3 blocksize(64,1);
    dim3 gridsize(1,1);
    
	if(col<=124*63)
	{
		loop_c=(col/124+1)*2-1;
	}
	else {
		loop_c=(col/124+1)+63;
	}
    
    //loop_c=(col/124+1)+63;
	loop_r=row/(124*64)+1;
    
    for(start_num=1;start_num<=loop_r;start_num++)
	{
    
    	if(start_num==loop_r){
        	int sn;
            sn=row%(124*64)/124+1;
            loop_c=(col/124+1)+sn;
            dim3 blocksize1(sn,1);
            for(loop=1;loop<=loop_c;loop++){               
              func<<< gridsize, blocksize1 >>> (row, col,match,mis_match, start_num,loop,d_a1,d_a2,d_matrix);		//从核线程启动     
            }
        }else{
            for(loop=1;loop<=loop_c;loop++){             
              func<<< gridsize, blocksize >>> (row, col,match,mis_match, start_num,loop,d_a1,d_a2,d_matrix);		//从核线程启动       
            }
        }
            
	}
    hipEventRecord(stop3, NULL);
    hipEventSynchronize(stop3);

    hipMemcpy(matrix,d_matrix,sizeof(int) * M , hipMemcpyDeviceToHost);
    printf("score:%d\n",matrix[M-1]);
    std::cout<<"row:"<<row<<", cow:"<<col<<std::endl;
    
    for(i=0;i<20;i++){
    	for(j=0;j<20;j++){
			printf("%d,",matrix[(row-i)*(col+1)+col-j]);
        }
        printf("\n");
	}


    //################################################################
    
	int recall_x,recall_y;
	int s1_l=0,s2_l=0,count=0;
	for(recall_x=row,recall_y=col;recall_x>0&&recall_y>0;)
	{
		int d_score;
		if(a1[recall_y-1]==a2[recall_x-1])
			d_score=5;
		else {
			d_score=-4;
		}
		if(matrix[(recall_x-1)*(col+1)+recall_y-1]+d_score==matrix[recall_x*(col+1)+recall_y])
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
			if(matrix[(recall_x-1)*(col+1)+recall_y]+d==matrix[recall_x*(col+1)+recall_y])
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
   
    hipEventRecord(stop2, NULL);
    hipEventSynchronize(stop2);
    
    
	FILE *fp;
	fp=fopen("result.txt","w");
	fclose(fp);
	write(l1,l2,count); 
    hipEventRecord(stop1, NULL);
    hipEventSynchronize(stop1);
    
 

	hipFree(d_a1);
    hipFree(d_a1);
    hipFree(d_matrix);
	
   
    hipEventElapsedTime(&eventMs1, start1, stop1);
    hipEventElapsedTime(&eventMs2, start2, stop2);
    hipEventElapsedTime(&eventMs3, start3, stop3);
  
    std::cout<<"######################"<<std::endl;
    
    //std::cout<<"the manycore counter="<<ed2-st2<<std::endl;
    std::cout<<"the total run time:"<<eventMs1<<" ms"<<std::endl;
    std::cout<<"the running time of the algorithm="<<eventMs2<<" ms"<<std::endl;
    std::cout<<"the running time of calculating matrix="<<eventMs3<<" ms"<<std::endl;
    
    hipEventDestroy(start1);
    hipEventDestroy(stop1);
    hipEventDestroy(start2);
    hipEventDestroy(stop2);
    hipEventDestroy(start3);
    hipEventDestroy(stop3);

}
