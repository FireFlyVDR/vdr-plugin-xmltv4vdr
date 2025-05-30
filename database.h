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
#include "maps.h"

#define EPG_DB_FILENAME "xmltv4vdr_EPG.db"

class cMapObject
{
private:
   cString a;
   cString b;
public:
   cMapObject(const char *A, const char *B) { a = A; b = B;};
   ~cMapObject() {};
   const char *A()   { return *a;};  // Source
   const char *B()   { return *b;};  // Picture
};

class cMapList : public cVector<cMapObject *> {
public:
   cMapList(int Allocated = 100): cVector<cMapObject *>(Allocated) {}
   virtual ~cMapList();
   void AppendStrings(const char *A, const char *B)  { Append(new cMapObject(A, B)); };
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
   //int  Transaction_Changes(void) { return sqlite3_total_changes(DBHandle); }

   int Analyze(const char *dbName);
};

class cEpisodesDB : private cXMLTVSQLite
{
private:
   bool CreateDB(void);
   sqlite3_stmt *stmtQueryEpisodes;
   sqlite3_stmt *stmtQueryAllEpisodes;
   time_t lastUpdate;
   bool UpdateDBFromFiles(void);
   bool UpdateDBFromINet(void);
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
   cMapList orphanedPictures;
   cString lastSource;
   sqlite3_stmt *stmtUpdateEventSelect;
   sqlite3_stmt *stmtImportXMLTVEventSelect;
   sqlite3_stmt *stmtImportXMLTVEventReplace;

   bool CreateDB(void);
   bool IsNewVersion(const cEvent* Event);
   cXMLTVEvent *FillXMLTVEventFromDB(sqlite3_stmt *stmt);
   int UnlinkPictures(const cXMLTVStringList *Pics, const char *ChannelID, const tEventID EventID);
   int UnlinkPictures(const char *Pics, const char *ChannelID, const tEventID EventID);
   cString ChannelListToString(const cChannelIDList &ChannelIDList);
   bool DropEventList(int *LinksDeleted, int *EventsDeleted, const char *WhereClause);

public:
   cXMLTVDB(void) { };
   ~cXMLTVDB();
   bool OpenDBConnection(bool Create = false);
   bool CloseDBConnection(int Line = 0);
   bool UpgradeDB(bool ForceCreate = false);
   int Analyze(void);

   using cXMLTVSQLite::Analyze;
   using cXMLTVSQLite::Transaction_Begin;
   using cXMLTVSQLite::Transaction_End;

   void DropOutdatedEvents(time_t EndTime);
   bool CheckConsistency(bool Fix, cXMLTVStringList *CheckResult);

   bool MarkEventsOutdated(const cChannelIDList &ChannelIDList);
   bool DropOutdatedEvents(const cChannelIDList &ChannelIDList, time_t LastEventStarttime);
   bool ImportXMLTVEventPrepare(void);
   bool ImportXMLTVEvent(cXMLTVEvent *xevent, const cChannelIDList &ChannelIDList);
   bool ImportXMLTVEventFinalize();
   bool AddOrphanedPicture(const char *Source, const char *Picture);
   int DeleteOrphanedPictures(void);
   bool FillEventFromXTEvent(cEvent *Event, cXMLTVEvent *xEvent, uint64_t Flags);
   bool UpdateEventPrepare(const char *ChannelID);
   bool UpdateEvent(cEvent *Event, uint64_t Flags);
   bool UpdateEventFinalize(void);
   bool AppendEvents(tChannelID channelID, uint64_t Flags, int *totalSchedules, int *totalEvents);
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
