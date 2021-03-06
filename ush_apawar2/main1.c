/******************************************************************************
 *
 *  File Name........: main.c
 *
 *  Description......: Simple driver program for ush's parser
 *
 *  Author...........: Vincent W. Freeh
 *
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include "parse.h"
#include<sys/stat.h>
#include<string.h>
#include<signal.h>
#include<fcntl.h>
#include<unistd.h>
int pipeA[2],pipeB[2];
int lockA, lockB;
int fil;
int suc;
int rc;
int fd;
char host1[20],*host;
char * find_cmd_type(Cmd c);
char * find_cmd_type_where(char * k);
void echoexe(Cmd c);
void whereexe(Cmd c);
void pwdexe(Cmd c);
void cdexe(Cmd c);
void setexe(Cmd c);
void Execute_cmd(Cmd d);
void gotorescue(int sig);
void getenviron(void);
char * pPath;
void unsetenvexe(Cmd c);
int Execute_exe(Cmd a,char *pat);

static void prPipe(Pipe p)
{
	int i = 0;
	Cmd c;

	if ( p == NULL )
		return;

	//printf("Begin pipe%s\n", p->type == Pout ? "" : " Error");
	for ( c = p->head; c != NULL; c = c->next ) {
		//printf("  Cmd #%d: ", ++i);
		Execute_cmd(c);
		//prCmd(c);
	}
	// printf("End pipe\n");
	lockA=0;
	lockB=1;
	if(pipe(pipeA)==-1)perror("PIPE:");
	if(pipe(pipeB)==-1)perror("PIPE:");
	prPipe(p->next);
}

int main(int argc, char *argv[])
{
	Pipe p,q;
	size_t len;
	char *host_n=malloc(100),*host_p;
	int fil;
	gethostname(host1,20);
	host_n=getenv("HOME");

	strcat(host_n,"/.ushrc");
	if((fil=open(host_n, O_RDONLY))==-1)
		perror("OPEN USHRC"); 

	int oldStdIn=dup(STDIN_FILENO); 
	dup2(fil,STDIN_FILENO);  
	close(fil);
	lockA=0;
	lockB=1;
	if(pipe(pipeA)==-1)perror("PIPE:");
	if(pipe(pipeB)==-1)perror("PIPE:");

	p = parse();
	prPipe(p);
	freePipe(p);
	dup2(oldStdIn,STDIN_FILENO); //restore STDIN
	close(oldStdIn); //don't need old STDIN handle

	while ( 1 ) 
	{
		lockA=0;
		lockB=1;
		if(pipe(pipeA)==-1)perror("PIPE:");
		if(pipe(pipeB)==-1)perror("PIPE:");
		printf("%s%% ", host1);
		fflush(stdout);
		//fflush(stdin);
		p = parse();
		prPipe(p);
		freePipe(p);
	}
}

void Execute_cmd(Cmd d)
{
	int check;
	char *pat,*cmdch;
	//check=0;
	//check=find_cmd_type(d);

	if(strcmp(d->args[0],"cd")==0)
		cdexe(d);
	else if(strcmp(d->args[0],"echo")==0)
		echoexe(d);
	else if(strcmp(d->args[0],"setenv")==0)
		setexe(d);
	else if(strcmp(d->args[0],"unsetenv")==0)
		unsetenvexe(d);
	else if(strcmp(d->args[0],"pwd")==0)
		pwdexe(d);
	else if(strcmp(d->args[0],"where")==0)
		whereexe(d);
	else if(strcmp(d->args[0],"logout")==0)
		exit(0);
	else
	{
		pat=find_cmd_type(d); 
		suc=Execute_exe(d,pat);
	}

}

int Execute_exe(Cmd a,char *pat)
{
	int a1=0,b=0;
	int fdi;
	fdi=0;
	fd=0;

	char buffer[10];
	pid_t pID=fork();
	if(pID==0)
	{
		if(pat==NULL)
		{
			fd=0;
			//Error redirection to stdout
			if(a->out==ToutErr||a->out==TappErr)
			{
				dup2(1,2);
			}
			if(a->out==ToutErr)
			{

				fd=open(a->outfile,O_WRONLY|O_TRUNC|O_CREAT,0660);
				dup2(fd,1);
			}
			if(a->out==TappErr)
			{
				fflush(stdout);
				fd=open(a->outfile,O_WRONLY|O_APPEND|O_CREAT,0660);
				dup2(fd,1);
			}
			int len;
			len=strlen("Command not found in Executables and Built-in: \n");
			write(1,"Command not found in Executables and Built-in: \n",len);
			if(fd>0)
			{
				close(fd);
				clearerr(stdout);
				close(1);
				close(2);
			}

			exit(0);
		}
		if(a->in==Tin)
		{
			fdi=open(a->infile,O_RDONLY,0660);
			dup2(fdi,0);
		}
		if(pat!=NULL)
		{
			if(a->in==Tpipe || a->in ==TpipeErr)
			{

				if(lockA==0)
				{
					close(pipeA[1]);
					dup2(pipeA[0],0);
					//close(pipeA[0]);
					a1=1;
				}
				else if(lockB==0)
				{
					close(pipeB[1]);
					dup2(pipeB[0],0);
					//close(pipeB[0]);
					b=1;
				}
			}

			if(a->next!=NULL && (a->next->in==Tpipe || a->next->in== TpipeErr))
			{
				if(a->next->in==TpipeErr)
					dup2(1,2);

				if(lockA==1)//&&a1!=1)
				{
					//printf("Write in A");
					close(pipeA[0]);
					//fcntl(pipeA[1], F_SETFL, O_TRUNC);
					//fflush(pipeA[1]);
					dup2(pipeA[1],1);
					close(pipeA[1]);
				}
				else if(lockB==1)//&&b!=1)
				{
					close(pipeB[0]);

					dup2(pipeB[1],1);
					close(pipeB[1]);
				}

			}
			if(a->out==ToutErr||a->out==TappErr)
			{
				dup2(1,2);
			}
			if(a->out==Tout || a->out==ToutErr)
			{
				fflush(stdout);
				fd=open(a->outfile,O_WRONLY|O_TRUNC|O_CREAT,0660);
				dup2(fd,1);
			}
			if(a->out==Tapp)
			{
				fd=open(a->outfile,O_WRONLY|O_APPEND|O_CREAT,0660);
				dup2(fd,1);
			}

			if(!execv(pat,a->args))
			{
				perror("EXCEV");
			}

			if(fd>0)
			{
				close(fd);
				close(1);

				close(2);

			}
			if(fdi>0)
			{
				close(fdi);
				close(0);
			}

		}
		exit(0);
		//goback:
	}
	else
	{
		wait();
		if(a->next!=NULL)
		{
			if((a->in==Tnil) && (a->next->in==Tpipe || a->next->in== TpipeErr))
			{

				close(pipeB[1]);
				lockA=1;
				lockB=0;
			}
			else if(a->in==Tpipe || a->in== TpipeErr)
			{
				if(lockA==1 && lockB==0)
				{

					close(pipeA[1]);
					lockA=0;
					lockB=1;
				}
				else if(lockA==0 && lockB==1)
				{
					close(pipeB[1]);
					lockA=1;
					lockB=0;
				}
			}
		}

		if(a->next!=NULL)
		{	if((a->in==Tpipe||a->in==TpipeErr) && (a->next->in==Tpipe|| a->next->in==TpipeErr))
			{
				if(lockA==1)//&&a1!=1)
				{

					if(pipe(pipeA)==-1)perror("PIPE:");
				}
				else if(lockB==1)//&&b!=1)
				{
					if(pipe(pipeB)==-1)perror("PIPE:");
				}
			}
		}
		return 1;
	}
}
char * find_cmd_type(Cmd k)
{
	char *str=malloc(100),buffer[10];
	char delims[] = ":";
	struct stat st; 
	int l;
	char *result = malloc(100);
	pPath=malloc(100);
	char *cmd=malloc(100),*cm=malloc(100);
	char env[100];
	getenviron();
	strcpy(cmd,"/");
	strcpy(env,pPath);
	strcat(cmd,k->args[0]);
	result = strtok( env, delims );
	while( result != NULL )
	{
		strcpy(str,result);
		strcat(str,cmd);
		if(stat(str,&st)==0)
		{       // printf(" /CMD is present= %s\n",str);
			return str;
		}

		result = strtok( NULL, delims );
	}
	return NULL;
}
void getenviron(void)
{
	pPath = getenv ("PATH");

}
void whereexe(Cmd c)
{
	char *pat;
	char *comm;
	int i=0, fdcd;
	char dir[100]="",buf;
	pid_t pid=fork();
	if(pid==0)
	{
		if(c->in==Tpipe || c->in==TpipeErr)
		{
			if(lockA==0)
			{
				close(pipeA[1]);
				dup2(pipeA[0],0);
				//close(pipeA[0]);
			}
			else if(lockB==0)
			{
				close(pipeB[1]);
				dup2(pipeB[0],0);
			}
		}

		if(c->next!=NULL && (c->next->in==Tpipe||c->next->in==TpipeErr))
		{
			if(c->next->in==TpipeErr)
				dup2(1,2);

			if(lockA==1)//&&a1!=1)
			{
				close(pipeA[0]);
				dup2(pipeA[1],1);

			}
			else if(lockB==1)//&&b!=1)
			{
				close(pipeB[0]);
				dup2(pipeB[1],1);

			}
		}
		if(c->out==ToutErr||c->out==TappErr)
		{
			dup2(1,2);
		}
		if(c->out==Tout || c->out==ToutErr)
		{
			fflush(stdout);
			fd=open(c->outfile,O_WRONLY|O_TRUNC|O_CREAT,0660);
			dup2(fd,1);
		}
		if(c->out==Tapp)
		{
			fd=open(c->outfile,O_WRONLY|O_APPEND|O_CREAT,0660);
			dup2(fd,1);
		}

		if(c->in==Tpipe || c->in==TpipeErr)
		{
			i=0;
			while(read(0,&buf,1)>0&&buf!='\n'&&buf!='\0')
			{
				dir[i]=buf;
				i++;
			}
			comm=dir;
		}

		if(c->in==Tin)
		{
			fdcd=open(c->infile,O_RDONLY,0660);
			i=0;
			while(read(fdcd,&buf,1)>0&&buf!='\n'&&buf!='\0')
			{
				dir[i]=buf;
				i++;
			}
			comm=dir;
			close(fdcd);
		}

		if(c->in==Tnil)
		{
			comm=c->args[1];
		}
		pat=find_cmd_type_where(comm); 
		if(pat!=NULL)
			printf("%s : %s : Executable \n",comm,pat);
		if(strcmp(comm,"cd")==0)
			printf("%s: Built-in Command\n",comm);
		else if(strcmp(comm,"echo")==0)
			printf("%s: Built-in Command\n",comm);
		else if(strcmp(comm,"setenv")==0)
			printf("%s: Built-in Command\n",comm);
		else if(strcmp(comm,"unsetenv")==0)
			printf("%s: Built-in Command\n",comm);
		else if(strcmp(comm,"pwd")==0)
			printf("%s: Built-in Command\n",comm);
		else if(strcmp(comm,"where")==0)
			printf("%s: Built-in Command\n",comm);
		else if(strcmp(comm,"logout")==0)
			printf("%s: Built-in Command\n",comm);
		else if(pat==NULL)
			printf("%s: Command not found\n",comm);
		if(fd>0)
		{
			close(fd);
		}

		exit(0);
	}
	else
	{
		wait();
		if(c->next!=NULL)
		{
			if((c->in==Tnil) && (c->next->in==Tpipe || c->next->in== TpipeErr))
			{
				close(pipeB[1]);
				lockA=1;
				lockB=0;
			}
			else if(c->in==Tpipe || c->in== TpipeErr)
			{
				if(lockA==1 && lockB==0)
				{
					close(pipeA[1]);
					lockA=0;
					lockB=1;
				}
				else if(lockA==0 && lockB==1)
				{
					close(pipeB[1]);

					lockA=1;
					lockB=0;
				}
			}
		}

		if(c->next!=NULL)
			if((c->in==Tpipe||c->in==TpipeErr) && (c->next->in==Tpipe|| c->next->in==TpipeErr))
			{
				if(lockA==1)//&&a1!=1)
				{

					if(pipe(pipeA)==-1)perror("PIPE:");
				}
				else if(lockB==1)//&&b!=1)
				{
					if(pipe(pipeB)==-1)perror("PIPE:");
				}
			}
	}
}


char * find_cmd_type_where(char * k)
{
	char *str=malloc(100),buffer[10];
	char delims[] = ":";
	struct stat st; 
	int l;
	char *result = malloc(100);
	pPath=malloc(100);
	char *cmd=malloc(100),*cm=malloc(100);
	char env[100];
	getenviron();
	strcpy(cmd,"/");
	strcpy(env,pPath);
	strcat(cmd,k);
	result = strtok( env, delims );


	while( result != NULL )
	{
		strcpy(str,result);
		strcat(str,cmd);
		if(stat(str,&st)==0)
		{
			return str;
		}

		result = strtok( NULL, delims );
	}
	return NULL;
}



void setexe(Cmd c)
{
	int che;
	int fdset,i;
	char buf='a',dir[100]="";
	char *env,var[100],wor[100];
	pid_t pid=fork();
	if(pid==0)
	{
		if(c->in==Tpipe||c->in==TpipeErr)
		{
			if(lockA==0)
			{
				close(pipeA[1]);
				dup2(pipeA[0],0);
			}
			else if(lockB==0)
			{
				close(pipeB[1]);
				dup2(pipeB[0],0);

			}
		}

		if(c->next!=NULL && (c->next->in==Tpipe|| c->next->in==TpipeErr))
		{
			if(c->next->in==TpipeErr)
				dup2(1,2);
			if(lockA==1)
			{

				close(pipeA[0]);

				dup2(pipeA[1],1);
				close(pipeA[1]);
			}
			else if(lockB==1)
			{
				close(pipeB[0]);
				dup2(pipeB[1],1);
				close(pipeB[1]);
			}
		}
		if(c->in==Tpipe||c->in==TpipeErr)
		{
			i=0;
			while(read(0,&buf,1)>0&&buf!=' ')
			{
				dir[i]=buf;
				i++;
			}
			strcpy(var,dir);
			i=0;
			while(read(0,&buf,1)>0&&buf!='\0'&&buf!='\n')
			{
				dir[i]=buf;
				i++;
			}
			strcpy(wor,dir);

			if(setenv(var,wor,1)!=0)
				perror("SETENV");
		}
		if(c->in==Tin)
		{
			fdset=open(c->infile,O_RDONLY,0660);
			i=0;
			while(read(fdset,&buf,1)>0&&buf!=' ')
			{
				dir[i]=buf;
				i++;
			}
			strcpy(var,dir);
			i=0;
			while(read(fdset,&buf,1)>0&&buf!='\0'&&buf!='\n')
			{
				dir[i]=buf;
				i++;
			}
			strcpy(wor,dir);
			if(setenv(var,wor,1)!=0)
				perror("SETENV");
		}
		else if(c->in==Tnil)
		{
			if(c->args[1]==NULL)
			{
				env=getenv("PATH");
				printf("PATH: %s\n",env);
				env=getenv("HOME");
				printf("HOME: %s\n",env);
				env=getenv("TERM");
				printf("TERM: %s\n",env);
				env=getenv("PS1");
				printf("PS1: %s\n",env);
			}
			else
			{
				if(che=setenv(c->args[1], c->args[2],1)!=0)
					perror("SETENV");
			}
		}

	}
	else
	{
		wait();
		if(c->next!=NULL)
		{
			if((c->in==Tnil) && (c->next->in==Tpipe || c->next->in== TpipeErr))
			{
				close(pipeB[1]);
				lockA=1;
				lockB=0;
			}
			else if(c->in==Tpipe || c->in== TpipeErr)
			{
				if(lockA==1 && lockB==0)
				{
					close(pipeA[1]);
					lockA=0;
					lockB=1;
				}
				else if(lockA==0 && lockB==1)
				{
					close(pipeB[1]);
					lockA=1;
					lockB=0;
				}
			}
		}

		if(c->next!=NULL)
			if((c->in==Tpipe||c->in==TpipeErr) && (c->next->in==Tpipe|| c->next->in==TpipeErr))
			{
				if(lockA==1)//&&a1!=1)
				{

					if(pipe(pipeA)==-1)perror("PIPE:");
				}
				else if(lockB==1)//&&b!=1)
				{
					if(pipe(pipeB)==-1)perror("PIPE:");
				}
			}

		return;

	}
}
void unsetenvexe(Cmd c)
{
	int fdset;
	char buf, dir[100]="";
	int i=0;
	pid_t pid=fork();
	if(pid==0)
	{
		if(c->in==Tin)
		{
			fdset=open(c->infile,O_RDONLY,0660);
			while(read(fdset,&buf,1)>0&&buf!='\n'&&buf!='\0')
			{
				dir[i]=buf;
				i++;
			}

			close(fdset);
			if(unsetenv(dir)!=0)
				perror("UNSETENV");

		}
		else
		{

			if(c->args[1]!=NULL)
				if(unsetenv(c->args[1])!=0)
					perror("UNSETENV");
		}
		exit(0);
	}
	else
		wait();
}
void echoexe(Cmd c)
{
	int i=0;
	int fdcd;
	char buf;
	pid_t pid;
	if(pid=fork()==0)
	{
		/*
		   if(c->in==Tpipe)
		   {
		   if(lockA==0)
		   {
		   close(pipeA[1]);
		   dup2(pipeA[0],0);
		//close(pipeA[0]);
		}
		else if(lockB==0)
		{
		close(pipeB[1]);
		dup2(pipeB[0],0);
		//close(pipeB[0]);

		}
		}

		if(c->next!=NULL && c->next->in==Tpipe)
		{
		if(lockA==1)//&&a1!=1)
		{
		close(pipeA[0]);
		dup2(pipeA[1],1);

		}
		else if(lockB==1)//&&b!=1)
		{
		close(pipeB[0]);
		dup2(pipeB[1],1);

		}
		}
		if(c->out==ToutErr||c->out==TappErr)
		{
		dup2(1,2);
		}
		if(c->out==Tout || c->out==ToutErr)
		{
		fflush(stdout);
		fd=open(c->outfile,O_WRONLY|O_TRUNC|O_CREAT,0660);
		dup2(fd,1);
		}
		if(c->out==Tapp)
		{
		fd=open(c->outfile,O_WRONLY|O_APPEND|O_CREAT,0660);
		dup2(fd,1);
		}

		if(c->in==Tpipe)
		{
		while(read(0,&buf,1)>0&&buf!='\0')
		{
		printf("%c",buf);
		}
		}*/
		if(c->in==Tin)
		{
			fdcd=open(c->infile,O_RDONLY,0660);
			while(read(fdcd,&buf,1)>0&&buf!='\0')
			{
				printf("%c",buf);
			}
			close(fdcd);
		}
		if(c->in == Tnil)
		{
			for(i=1;i<c->nargs;i++)
			{
				printf("%s",c->args[i]);
				printf(" ");
			}
			printf("\n");
		}
		if(fd>0)
		{
			close(fd);
		}

		exit(0);
	}
	else
	{
		wait();
		if(c->next!=NULL)
		{
			if((c->in==Tnil) && (c->next->in==Tpipe || c->next->in== TpipeErr))
			{
				close(pipeB[1]);
				lockA=1;
				lockB=0;
			}
			else if(c->in==Tpipe || c->in== TpipeErr)
			{
				if(lockA==1 && lockB==0)
				{
					close(pipeA[1]);
					lockA=0;
					lockB=1;
				}
				else if(lockA==0 && lockB==1)
				{
					close(pipeB[1]);
					lockA=1;
					lockB=0;
				}
			}
		}

		if(c->next!=NULL)
			if((c->in==Tpipe||c->in==TpipeErr) && (c->next->in==Tpipe|| c->next->in==TpipeErr))
			{
				if(lockA==1)//&&a1!=1)
				{

					if(pipe(pipeA)==-1)perror("PIPE:");
				}
				else if(lockB==1)//&&b!=1)
				{
					if(pipe(pipeB)==-1)perror("PIPE:");
				}
			}
		return;
	}
}
void pwdexe(Cmd c)
{
	char *pw;
	long size;
	char *ptr;
	fd=0;
	pid_t pid=fork();
	if(pid==0)
	{
		if(c->next!=NULL && (c->next->in==Tpipe|| c->next->in==TpipeErr))
		{
			if(c->next->in==TpipeErr)
				dup2(1,2);
			if(lockA==1)
			{
				close(pipeA[0]);
				dup2(pipeA[1],1);
				close(pipeA[1]);
			}
			else if(lockB==1)
			{
				close(pipeB[0]);
				dup2(pipeB[1],1);
				close(pipeB[1]);
			}
		}
		if(c->out==ToutErr||c->out==TappErr)
		{
			dup2(1,2);
		}
		if(c->out==Tout || c->out==ToutErr)
		{
			//printf("HERE: %s",c->outfile);
			fflush(stdout);
			fd=open(c->outfile,O_WRONLY|O_TRUNC|O_CREAT,0660);
			if(dup2(fd,1)==-1)
			{
				perror("DUP: ");
				exit(0);
			}
		}
		if(c->out==Tapp)
		{
			fd=open(c->outfile,O_WRONLY|O_APPEND|O_CREAT,0660);
			dup2(fd,1);
		}
		//printf("HELLO");
		size = pathconf(".", _PC_PATH_MAX);
		if ((pw = (char *)malloc((size_t)size)) != NULL)
			ptr=getcwd(pw, (size_t)size);
		printf("%s\n", pw);

		if(fd>0)
		{
			close(fd);
		}
		exit(0);
	}
	else
	{
		wait();
		if(c->next!=NULL)
		{
			if((c->in==Tnil) && (c->next->in==Tpipe || c->next->in== TpipeErr))
			{
				close(pipeB[1]);
				lockA=1;
				lockB=0;
			}
			else if(c->in==Tpipe || c->in== TpipeErr)
			{
				if(lockA==1 && lockB==0)
				{
					close(pipeA[1]);
					lockA=0;
					lockB=1;
				}
				else if(lockA==0 && lockB==1)
				{
					close(pipeB[1]);
					lockA=1;
					lockB=0;
				}
			}
		}

		if(c->next!=NULL)
			if((c->in==Tpipe||c->in==TpipeErr) && (c->next->in==Tpipe|| c->next->in==TpipeErr))
			{
				if(lockA==1)//&&a1!=1)
				{

					if(pipe(pipeA)==-1)perror("PIPE:");
				}
				else if(lockB==1)//&&b!=1)
				{
					if(pipe(pipeB)==-1)perror("PIPE:");
				}
			}

	}

}
void cdexe(Cmd c)
{
	char *ch;
	int see;
	char *ptr;
	long size;
	char dir[100]="",*dirch=NULL,buf;
	int fdcd;
	char cha;
	int l;
	int i=0;
	if(c->next==NULL)
	{
		if(c->in==Tin)
		{
			fdcd=open(c->infile,O_RDONLY,0660);
			while(read(fdcd,&buf,1)>0&&buf!='\n'&&buf!='\0')
			{
				dir[i]=buf;
				i++;
			}

			close(fdcd);

			see=chdir(dir);
			l=strlen(dir);
			if (see != 0)
				perror("chdir() to /chdir/error failed");
			if(see==0)
				size = pathconf(".", _PC_PATH_MAX);
			if ((host = (char *)malloc((size_t)size)) != NULL)
				ptr=getcwd(host, (size_t)size);
		}
		else
		{
			see=chdir(c->args[1]);
			l=strlen(c->args[1]);
			if (see != 0)
				perror("chdir() to /chdir/error failed");
			if(see==0)
				size = pathconf(".", _PC_PATH_MAX);
			if ((host = (char *)malloc((size_t)size)) != NULL)
				ptr=getcwd(host, (size_t)size);
		}
	}
	else if(c->next!=NULL)
	{
		pid_t pid=fork();
		if(pid==0)
		{
			if(c->in==Tin)
			{
				fdcd=open(c->infile,O_RDONLY,0660);
				while(read(fdcd,&buf,1)>0&&buf!='\n'&&buf!='\0')
				{
					dir[i]=buf;
					i++;
				}

				close(fdcd);

				see=chdir(dir);
				l=strlen(dir);
				if (see != 0)
					perror("chdir() to /chdir/error failed");
				if(see==0)
					size = pathconf(".", _PC_PATH_MAX);
				if ((host = (char *)malloc((size_t)size)) != NULL)
					ptr=getcwd(host, (size_t)size);
			}
			else
			{
				see=chdir(c->args[1]);
				l=strlen(c->args[1]);
				if (see != 0)
					perror("chdir() to /chdir/error failed");
				if(see==0)
					size = pathconf(".", _PC_PATH_MAX);
				if ((host = (char *)malloc((size_t)size)) != NULL)
					ptr=getcwd(host, (size_t)size);
			}
			exit(0);
		}
		else
			wait();
	}
}
/*........................ end of main.c ....................................*/
