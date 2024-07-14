/*
 * database.h: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _DATABASE_H
#define _DATABASE_H

#include <vdr/channels.h>
#include <vdr/thread.h>
#include <sqlite3.h>
#include "event.h"
#include "source.h"

#define EPG_DB_FILENAME "xmltv4vdr_EPG.db"

class cPictureObject
{
private:
   cString a;
   cString b;
public:
   cPictureObject(const char *A, const char *B) { a = A; b = B;};
   ~cPictureObject() {};
   const char *Source()    { return a;};
   const char *Picture()   { return b;};
};

class cPictureList : public cVector<cPictureObject *> {
public:
   cPictureList(int Allocated = 100): cVector<cPictureObject *>(Allocated) {}
   virtual ~cPictureList();
   void AppendStrings(const char *A, const char *B)  { Append(new cPictureObject(A, B)); };
   int Find(const char *A, const char *B) const;
   virtual void Clear(void);
};

class cXMLTVSQLite
{
protected:
   sqlite3 *DBHandle;
   cString Time2Str(time_t time);
   bool CheckSQLiteSuccess(int SQLrc, int Line, const char * Function = NULL);
   cString SQLescape(const char *s, const char *chars = "'");

   cXMLTVSQLite(void);
   ~cXMLTVSQLite();
   bool OpenDBConnection(const char *DBFile, int OpenFlags);
   bool CloseDBConnection(int Line = 0);
   int ExecDBQuery(const char *Query);
   int ExecDBQueryInt(const char *sqlQuery, uint &Integer);
   enum {
      SQLqryError = -1,
      SQLqryEmpty =  0,
      SQLqryOne   =  1,
      SQLqryMulti =  2,
   };
   bool Transaction_Begin(void);
   bool Transaction_End(bool Commit = true);
   int  Transaction_Changes(void) { return sqlite3_total_changes(DBHandle); }

   int Analyze(const char *dbName);
};

class cEpisodesDB : private cXMLTVSQLite
{
private:
   bool CreateDB(void);
   sqlite3_stmt *stmtQueryEpisodes;
   sqlite3_stmt *stmtQueryAllEpisodes;
   time_t lastUpdate;
public:
   cEpisodesDB(void) { lastUpdate = 0; };
   ~cEpisodesDB() { };
   bool OpenDBConnection(bool Create = false);
   bool CloseDBConnection(int Line = 0);
   bool UpdateDB(void);
   bool QueryEpisode(cXMLTVEvent *xEvent);
};


class cXMLTVDB : private cXMLTVSQLite
{
private:
   cPictureList orphanedPictures;
   cString lastSource;
   sqlite3_stmt *stmtUpdateEventSelect;
   sqlite3_stmt *stmtImportXMLTVEventSelect;
   sqlite3_stmt *stmtImportXMLTVEventUpdate;
   sqlite3_stmt *stmtImportXMLTVEventInsert;

   bool CreateDB(void);
   bool IsNewVersion(const cEvent* Event);
   cXMLTVEvent *FillXMLTVEventFromDB(sqlite3_stmt *stmt);
   int UnlinkPictures(const cXMLTVStringList *Pics, const char *ChannelID, const tEventID EventID);
   int UnlinkPictures(const char *Pics, const char *ChannelID, const tEventID EventID);

public:
   cXMLTVDB(void) { };
   ~cXMLTVDB() { };
   bool OpenDBConnection(bool Create = false);
   bool CloseDBConnection(int Line = 0);
   bool UpgradeDB(bool ForceCreate = false);
   int Analyze(void);

   using cXMLTVSQLite::Analyze;

   bool DropEventList(int *LinksDeleted, int *EventsDeleted, const char * WhereClause);
   void DropOutdatedEvents(time_t EndTime);
   bool CheckConsistency(bool Fix, cXMLTVStringList *CheckResult);

   bool MarkEventsOutdated(cChannelList *ChannelList, const char *SourceName);
   bool DropOutdatedEvents(cChannelList *ChannelList, const char *SourceName, time_t LastEventStarttime);
   bool ImportXMLTVEvent(cXMLTVEvent *xevent, cChannelList *ChannelList, const char *SourceName);
   bool ImportXMLTVEventPrepare(const char *SourceName);
   bool ImportXMLTVEventFinalize();
   bool AddOrphanedPicture(const char *Source, const char *Picture);
   int DeletePictures(void);
   bool FillEventFromXTEvent(cEvent *Event, cXMLTVEvent *xEvent, uint64_t Flags);
   bool UpdateEventPrepare(const char *ChannelID, const char *SourceName);
   bool UpdateEventFinalize(void);
   bool UpdateEvent(cEvent *Event, uint64_t Flags, const char *ChannelName, const char *SourceName, time_t LastEventStarttime);
   bool DropOutdated(cSchedule *Schedule, time_t SegmentStart, time_t SegmentEnd, uchar TableID, uchar Version);
};

// -------------------------------------------------------------
enum eHousekeepingType
{
   HKT_DELETEEXPIREDEVENTS,
   HKT_CHECKCONSISTENCY,
   HKT_CHECKANDFIXCONSISTENCY
};

class cHouseKeeping : public cThread
{
private:
   eHousekeepingType type;
   cXMLTVStringList lastCheckResult;
public:
   cHouseKeeping();
   bool StartHousekeeping(eHousekeepingType Type);
   void StopHousekeeping() { Cancel(3); }
   bool Active() { return cThread::Active(); }
   virtual void Action();
   const char *LastCheckResult(void) { return lastCheckResult.ToString("\n"); }
};
#endif
