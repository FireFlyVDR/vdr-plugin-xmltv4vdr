/*
 * extpipe.cpp: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <signal.h>
#include <sys/wait.h>
#include <vdr/tools.h>
#include "extpipe.h"
#include "debug.h"


cExtPipe::cExtPipe(cString SourceName)
{
   sourceName = SourceName;
   pid = -1;
   f_stderr = -1;
   f_stdout=  -1;
}

cExtPipe::~cExtPipe()
{
   if (pid > 0)
      Close();
}


bool cExtPipe::Open(cString Command)
{
   int fd_stdout[2];
   int fd_stderr[2];

   if (pipe(fd_stdout) < 0)
   {
      LOG_ERROR;
      return false;
   }

   if (pipe(fd_stderr) < 0)
   {
      close(fd_stdout[0]);
      close(fd_stdout[1]);
      LOG_ERROR;
      return false;
   }

   if ((pid = fork()) < 0)
   {
      LOG_ERROR;       // fork failed
      close(fd_stdout[0]);
      close(fd_stdout[1]);
      close(fd_stderr[0]);
      close(fd_stderr[1]);
      return false;
   }

   if (pid > 0)   // parent process
   {
      close(fd_stdout[1]); // close write fd, we need only read fd
      close(fd_stderr[1]); // close write fd, we need only read fd
      f_stdout = fd_stdout[0];
      f_stderr = fd_stderr[0];
      isyslog("'%s' executing EPGsource with pid %d", *sourceName, pid);
      return true;
   }
   else   // child process
   {
      close(fd_stdout[0]); // close read fd, we need only write fd
      close(fd_stderr[0]); // close read fd, we need only write fd

      if (dup2(fd_stdout[1], STDOUT_FILENO) == -1)   // redirect stdout to pipe
      {
         LOG_ERROR;
         close(fd_stderr[1]);
         close(fd_stdout[1]);
         _exit(-1);
      }

      if (dup2(fd_stderr[1], STDERR_FILENO) == -1)   // redirect sterr to pipe
      {
         LOG_ERROR;
         close(fd_stderr[1]);
         close(fd_stdout[1]);
         _exit(-1);
      }

      int MaxPossibleFileDescriptors = getdtablesize();
      for (int i = STDERR_FILENO + 1; i < MaxPossibleFileDescriptors; i++)
         close(i); //close all dup'ed filedescriptors
#if 1
      if (execl("/bin/sh", "sh", "-c", *Command, NULL) == -1)
      {
         LOG_ERROR_STR(*Command);
         close(fd_stderr[1]);
         close(fd_stdout[1]);
         _exit(-1);
      }
#else
#warning External Exeuction disabled
      //tsyslog("SOURCE EXECUTION DISABLED IN %s:%d", __FILE__, __LINE__);
      LOG_ERROR_STR("***** SOURCE EXECUTION DISABLED *****");
#endif
      close(fd_stderr[1]);
      close(fd_stdout[1]);
      _exit(0);
   }
}

bool cExtPipe::GetResult(char **outBuffer, size_t &outBufferSize, char **errBuffer, size_t &errBufferSize)
{
#define CHUNKSIZE 4095
   size_t outBufferOffset = 0;
   size_t errBufferOffset = 0;
   int fdsopen = 2;
   struct pollfd fds[2];
   fds[0].events = POLLIN;
   fds[1].events = POLLIN;
   int rc = 0;
   do {
      fds[0].fd = f_stdout;
      fds[1].fd = f_stderr;
      rc = poll(fds, 2, 250);
      if (rc > 0)  // 0 = timeout
      {
         if (fds[0].revents & POLLIN)
         {  // StdOut
            int len = 0;
            do {
               if (outBufferOffset + CHUNKSIZE + 1 > outBufferSize) {
                  outBufferSize += CHUNKSIZE + 1;
                  char *newBuffer = (char *)realloc(*outBuffer, outBufferSize);
                  if (!newBuffer) {
                     free(*outBuffer);
                     *outBuffer = NULL;
                     outBufferSize = 0;
                     break;
                  }
                  else
                     *outBuffer = newBuffer;
               }

               len = read(f_stdout, *outBuffer + outBufferOffset, CHUNKSIZE);
               if (len <= 0) {
                  break;
               }
               outBufferOffset += len;
            } while (len == CHUNKSIZE && *outBuffer);
            if (!*outBuffer)
               break;
         }

         if (fds[1].revents & POLLIN)
         {  // StdErr
            int len = 0;
            do {
               if (errBufferOffset + CHUNKSIZE + 1 > errBufferSize) {
                  errBufferSize += CHUNKSIZE + 1;
                  char *newBuffer = (char *)realloc(*errBuffer, errBufferSize);
                  if (!newBuffer) {
                     free(*errBuffer);
                     *errBuffer = NULL;
                     errBufferSize = 0;
                     break;
                  }
                  else
                     *errBuffer = newBuffer;
               }

               len = read(f_stderr, *errBuffer + errBufferOffset, CHUNKSIZE);
               if (len <= 0) {
                  break;
               }
               errBufferOffset += len;
            } while (len == CHUNKSIZE && *errBuffer);
            if (!*errBuffer) 
               break;
         }

         if (fds[0].revents & POLLHUP)
            fdsopen--;

         if (fds[1].revents & POLLHUP)
            fdsopen--;
      }

      // check if we should abort
      if (!XMLTVConfig.EPGSources()->ImportIsRunning())
      {
         rc = -1;
      }
   } while (fdsopen > 0 && rc >= 0);

   if (*outBuffer) 
      (*outBuffer)[outBufferOffset] = 0;
   if (*errBuffer) 
      (*errBuffer)[errBufferOffset] = 0;

   return rc >= 0;
}

int cExtPipe::Close(void)
{
   // returns: 
   // nur wenn rc > 0 wird weiter gemacht d.h. <=0 ist ein Fehler
   // d.h. rc muss größer 0 sein und WEXITSTATUS == 0
   //
   // -1   if aborted or error occurred
   // >=0  exit code of ecxternal script

   int ret = -1;

   int wstatus = -1;
   if (pid > 0)     // if child still exists
   {
      int i = 5;
      do
      {   // loop 5x 100ms and check if child still exists
         ret = waitpid(pid, &wstatus, WNOHANG);  // returns pid if terminated, 0 pid still exists, -1 on error
         if (ret < 0)  // Error
         {
            LOG_ERROR;
            break;
         }
         i--;
         cCondWait::SleepMs(100);
      } while (ret != pid && i > 0);

      if (ret != pid)
      {
         kill(pid, SIGKILL);
         ret = 9;
      }
      //else {
      if (WIFEXITED(wstatus)) {
         ret = WEXITSTATUS(wstatus);
         isyslog("'%s' EPGsource finished with return code %d", *sourceName, ret);
      }
      else if (WIFSIGNALED(wstatus)) {
         ret = WTERMSIG(wstatus);
         isyslog("'%s' EPGsource received signal %d", *sourceName, ret);
      }
      pid = -1;
   }

   // close stderr
   if (f_stderr != -1)
   {
      close(f_stderr);
      f_stderr = -1;
   }

   // close stdout
   if (f_stdout != -1)
   {
      close(f_stdout);
      f_stdout = -1;
   }


   return ret;
}
