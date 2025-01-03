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
   int exec_days;
   int exec_time;
   int maxDaysProvided;
   cStringList epgChannels;
   cString log;
   time_t lastEventStartTime;
   time_t lastSuccessfulRun;
   bool ReadConfig(void);
   time_t XmltvTime2UTC(char *xmltvtime);
   int ReadXMLTVfile(char *&xmltv_buffer, size_t &size);
   int ParseAndImportXMLTV(char *buffer, int bufsize, const char *SourceName);
   bool FillXTEventFromXmlNode(cXMLTVEvent *xtEvent, xmlNodePtr node);
public:
   cEPGSource(const char *Name);
   cEPGSource();
   ~cEPGSource();
   const char *SourceName()    { return *sourceName; }
   bool Execute(void);
   bool ExecuteNow(time_t time);
   time_t NextRunTime(time_t Now = (time_t)0);
   time_t LastSuccessfulRun()  { return lastSuccessfulRun; };
   void SetLastSuccessfulRun(time_t RunTime) { lastSuccessfulRun = RunTime; };
   const char * GetLog()       { return *log; }
   bool Enabled()              { return enabled; }
   void Enable(bool Enable )   { enabled = Enable; }
   const cStringList *EpgChannelList() const { return &epgChannels; }
   int ExecTime()              { return exec_time; }
   void SetExecTime(int Time)  { exec_time = Time; }
   int ExecDays()              { return exec_days; }
   void SetExecDays(int Days)  { exec_days = Days; }
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
