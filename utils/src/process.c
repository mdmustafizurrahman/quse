/* -*- C++ -*- */

#include <process.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

/* ��������� �������, ������������ ��� ������ �����,
   ������ � ������. fds[0] - stdin, fds[1] - stdout,
   fds[2] - stderr. ���� �� ���� ����������������� -
   OSP_FILE_BAD. */
osp_pid_t
osp_spawnl_ex(const char* const args[],
              osp_file_t fds[], intp_ot* pret)
{
  pid_t pid;
  int code;
  
  pid = vfork();
  switch (pid) {
  case -1:
    /* ������� �� ���������� */
    return OSP_PID_BAD;
  case 0:
    if ( fds )
      {
        /* ����������������� �������� ����������� */
        if ( fds[0] != OSP_FILE_BAD )
          dup2((int)fds[0], 0);
        if ( fds[1] != OSP_FILE_BAD )
          dup2((int)fds[1], 1);
        if ( fds[2] != OSP_FILE_BAD )
          dup2((int)fds[2], 2);
      }
    
    /* ��������� �������� �������� ����������� */
    for ( code=getdtablesize()-1; code>2; code-- )
      close(code);
    
    execv(args[0], (char* const *)args);
    /* ���� �� ���� ���������, ������ ��� ������ */
    _exit(127);
  };
  /* ���� ���� fork �������, �� ������ ��������� ��������������
     ������� �� ��������. ���� �� ����� �� ����������, ��� �������
     ����. */
  switch ( osp_testpid((osp_pid_t)pid, pret) )
    {
    case -1:
      return OSP_PID_BAD;
    case 1:
      if (*pret==127)
        return OSP_PID_BAD;
      return OSP_PID_FINISHED;
    }
  return (osp_pid_t)pid;
}

/* ��������� ��������� �������� ��������. ���� ���������� - ��������
   ��� �������� � ret � ������� 1. ����� ������� 0. ��� ������ -1.
   ��� ������ ��������� ����������� PID. */
intp_ot osp_testpid(osp_pid_t pidX, intp_ot* pret)
{
  int retCode;
  pid_t pid;
  
  if (pret==NULL)
    pret = &retCode;
  *pret = 0;
  pid = (pid_t)pidX;
  while (1) {
    pid_t p = waitpid(pid, pret, WNOHANG);
    if ( p==-1 )
      {
        if ( errno!=EINTR )
          return -1;
      }
    if ( p==pid )
      {
        *pret = WEXITSTATUS(*pret);
        return 1;
      }
    if ( p==0 )
      return 0;
  }
  return -1;
}

/* ��������� ���������� �������� � ������� ��� ���.
   ��������� ����������� PID. */
intp_ot osp_waitpid(osp_pid_t pidX, intp_ot* pret, intp_ot wait)
{
  intp_ot ret;
  intp_ot nanoCnt;
  pid_t pid;
  
  if (pret==NULL)
    pret = &ret;
  *pret = -1;
  pid = (pid_t)pidX;
  
  if ( wait < 0 )
    nanoCnt = 0x7FFFFFFF;
  else
    nanoCnt = wait / 200;
  
  *pret = 0;
  while (nanoCnt >= 0)
    {
      pid_t p = waitpid(pid, pret, WNOHANG);
      if ( p==-1 )
        {
          if ( errno!=EINTR )
            return -1;
        }
      if ( p==pid )
        {
          *pret = WEXITSTATUS(*pret);
          return 1;
        }
      osp_sleep(200, 0);
      nanoCnt--;
    }
  return 0;
}

/* ����� ������� �����. ��������� ����������� PID. */
intp_ot osp_termpid(osp_pid_t pid)
{
  intp_ot ret;
  if ( pid==OSP_PID_BAD || pid==OSP_PID_FINISHED )
    return -1;
  ret = kill((pid_t)pid, SIGKILL);
  if ( ret==0 )
    osp_closepid(pid);
  return ret;
}

intp_ot osp_huppid(osp_pid_t pid)
{
  intp_ot ret;
  if ( pid==OSP_PID_BAD || pid==OSP_PID_FINISHED )
    return -1;
  ret = kill((pid_t)pid, SIGINT);
  if ( ret==0 )
    osp_closepid(pid);
  return ret;
}

/* ������� ������������ PID. */
void osp_closepid(osp_pid_t pid)
{
  int ret = 0;
  waitpid((pid_t)pid, &ret, WNOHANG);
}

/* �������� ���� PID. */
osp_pid_t osp_getpid(void)
{
  return (osp_pid_t)getpid();
}


/* ������� � ��������� "������" */
int osp_daemonize(const char* wpath, int flags)
{
  int code;
  
  if ( getppid() < 2 )
    return -1; /* � ��� ��� �����, ������� ���������� ������. */
  
  /* �������� ������� � ��������� �������� */
  code = (int)fork();
  if ( code<0 ) return code; /* ������ ��� �����������. */
  if ( code>0 ) _exit(0); /* ��������� ������������ �������. */
  
  /* ���� ������ ������� �������, ��������� � ����. */
  if (wpath)
    {
      if ( 0 != chdir(wpath) )
        return -1;
    }
  
  /* ������� ����� ������ ��������� */
  if ( setsid() == (pid_t)-1 )
    return -1;
  
  /* �������� ����� �� ����������� �����. */
  umask(027);
  
  if ( flags )
    {
      /* ��������� ��� �������� �������� ����������� */
      for ( code=getdtablesize()-1; code>=0; code-- )
        close(code);
    }
  else
    {
      /* ��������� *������* stdin, stdout � stderr. */
      close(2); close(1); close(0);
    }
  
  /* ��������� stdin, stdout � stderr � /dev/null. */
  flags = open("/dev/null", O_RDWR);
  dup(flags); dup(flags);
  
  return 0;
}

/* ����� �� msec ���������� */
void osp_sleep(int msec, int alert)
{
#ifdef OSP_FUNC_NANOSLEEP
  struct timespec tsA, tsB;
  tsA.tv_sec = msec/1000;
  tsA.tv_nsec = 1000000 * (msec % 1000);
  while (1)
    {
      if (nanosleep(&tsA, &tsB)==0)
        return;
      if ( alert != 0 ) return;
      tsA = tsB;
    }
#else
  int tmp = msec / 1000;
  if ( tmp>0 )
    sleep(tmp);
  tmp = msec - tmp*1000;
  if ( tmp>0 )
    usleep(tmp*1000);
#endif
}

osp_pid_t
osp_spawnl(const char* const args[],
           intp_ot* pret)
{
  return osp_spawnl_ex(args, NULL, pret);
}


intp_ot
osp_runprc(const char* const args[],
           u_intp_ot wait,
           intp_ot* pret)
{
  intp_ot ret;
  osp_pid_t pid;
  
  if ( pret==0 )
    pret = &ret;
  *pret = 0;
  
  pid = osp_spawnl_ex(args, NULL, pret);
  if ( pid==OSP_PID_BAD )
    return -1;
  if ( pid==OSP_PID_FINISHED )
    return 1;
  
  switch (osp_waitpid(pid, pret, wait))
    {
    case 0:
      osp_termpid(pid);
      return 0;
    case -1:
      osp_termpid(pid);
      return -1;
    }
  return 1;
}


/* End Of File */
