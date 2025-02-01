/*
 * source.h: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __source_h
#define __source_h

#include <atomic>
#include <libxml/parser.h>
#include "event.h"

#define TOKEN_DELIMITER '~'
#define EVENT_LINGERTIME  (time(NULL) - Setup.EPGLinger * 60)
#define MAX_EXEC_TIMES 12

// --------------------------------------------------------------------------------------------------------
class cEPGSource : public cListObject
{
private:
   cString sourceName;
   cString confdir;
   cString pin;
   bool ready2parse;
   bool usePipe;
   bool needPin;
   bool running;
   bool enabled;
   bool hasPics;
   bool usePics;
   int daysInAdvance;
   int execDays;
   int numExecTimes;
   int execTimes[MAX_EXEC_TIMES];
   time_t nextExecTime;
   int maxDaysProvided;
   cStringList epgChannels;
   cString log;
   time_t lastEventStartTime;
   time_t lastSuccessfulRun;
   bool ReadConfig(void);
   time_t XmltvTime2UTC(char *xmltvtime);
   bool ReadXMLTVfile(char *&xmltv_buffer, size_t &size);
   bool ParseAndImportXMLTV(char *buffer, int bufsize, const char *SourceName);
   bool FillXTEventFromXmlNode(cXMLTVEvent *xtEvent, xmlNodePtr node);
public:
   cEPGSource(const char *Name);
   cEPGSource();
   ~cEPGSource() {};
   const char *SourceName()    { return *sourceName; };
   bool Execute(void);
   bool ExecuteNow(time_t time);
   time_t GetNextExecTime(time_t Now = 0, bool ForceCalculation = false);
   time_t LastSuccessfulRun()  { return lastSuccessfulRun; };
   void SetLastSuccessfulRun(time_t RunTime) { lastSuccessfulRun = RunTime; };
   const char * GetLog()       { return *log; }
   bool Enabled()              { return enabled; }
   void Enable(bool Enable )   { enabled = Enable; }
   const cStringList *EpgChannelList() const { return &epgChannels; }
   cString GetExecTimesString(void);
   const int *GetExecTimes(int *NumExecTimes) { *NumExecTimes = numExecTimes; return execTimes; }
   void SetExecTime(int Time)  { execTimes[0] = Time; }  // deprecated
   void SetExecTimes(int numTimes, int *Times);
   void ParseExecTimes(const char *ExecTimeString);
   int ExecDays()              { return execDays; }
   void SetExecDays(int Days)  { execDays = Days; }
   int MaxDaysProvided()       { return maxDaysProvided; }
   int DaysInAdvance()         { return daysInAdvance; }
   void SetDaysInAdvance(int NewDaysInAdvance)  { daysInAdvance = NewDaysInAdvance; }
   bool NeedPin()              { return needPin; }
   void SetPin(const char *NewPin) { pin = NewPin; }
   const char *Pin()           { return *pin; }
   bool HasPics()              { return hasPics; }
   bool UsePics()              { return usePics; }
   void SetUsePics(bool NewVal) { usePics = NewVal; }
   void Add2Log(struct tm *Tm, const char Prefix, const char *Line);
   bool ImportActive()         { return running; }
   bool ProvidesChannel(const char *EPGchannel);
   time_t LastEventStarttime() { return lastEventStartTime; }
   void SetLastEventStarttime(time_t Starttime) { lastEventStartTime = Starttime; }

   enum  
   {
      PARSE_NOERROR = 0,
      PARSE_XMLTVERR,
      PARSE_NOMAPPING,
      PARSE_NOCHANNELID,
      PARSE_FETCHERR,
      PARSE_SQLERR,
      PARSE_NOEVENTID,
      PARSE_IMPORTERR
   };
};

// --------------------------------------------------------------------------------------------------------
class cEPGSources : public cList<cEPGSource>, public cThread
{
private:
   cMutex mtxImport;
   cCondVar cvBlock;
   cCondWait cwDelay;
   bool manualStart;
   std::atomic<bool> isImporting;
   cEPGSource *epgImportSource;
   void RemoveAll();
public:
   cEPGSources();
   ~cEPGSources();
   void ReadAllConfigs();
   bool ExecuteNow(time_t time);
   time_t NextRunTime();
   cEPGSource *GetSource(const char *SourceName);
   void ResetLastEventStarttimes();
   void StartImport(cEPGSource *Source = NULL);
   void StopThread();
   bool ThreadIsRunning()     { return Running(); }
   bool ImportIsRunning()     { return isImporting; }
   virtual void Action();
};

#endif
