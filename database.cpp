/*
 * database.h: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <string>
//#include <unistd.h>
#include <vdr/channels.h>

#include "debug.h"
#include "database.h"


// -------------------------------------------------------------
cPictureList::~cPictureList()
{
  Clear();
}

int cPictureList::Find(const char *A, const char *B) const
{
   for (int i = 0; i < Size(); i++) {
      if (!strcmp(A, At(i)->Source()) && !strcmp(B, At(i)->Picture()))
         return i;
      }
  return -1;
}

void cPictureList::Clear(void)
{
   for (int i = 0; i < Size(); i++)
      delete(At(i));
   cVector<cPictureObject *>::Clear();
}

// -------------------------------------------------------------
cXMLTVSQLite::cXMLTVSQLite(void)
{  /// cXMLTVSQLite Constructor
   DBHandle = NULL;
}

cXMLTVSQLite::~cXMLTVSQLite()
{  /// cXMLTVSQLite Destructor
   if (DBHandle)
      CloseDBConnection(__LINE__);
}

bool cXMLTVSQLite::OpenDBConnection(const char *DBFile, int OpenFlags)
{  /// open a DB connection and return true on success
   if (!XMLTVConfig.DBinitialized() || !DBHandle) 
   {
      int SQLrc = sqlite3_open_v2(DBFile, &DBHandle, OpenFlags, NULL);
      if (SQLrc != SQLITE_OK)
      {
         esyslog("Error opening DB '%s': %s (%i)", DBFile, sqlite3_errstr(SQLrc), SQLrc );
         DBHandle = NULL;
      }
   }

   return (DBHandle != NULL);
}

bool cXMLTVSQLite::CloseDBConnection(int Line)
{  /// Close DB connection
   int SQLrc = sqlite3_close(DBHandle);
   sqlite3* oldHandle = DBHandle;
   DBHandle = NULL;
   bool result = CheckSQLiteSuccess(SQLrc, Line);
   return result;
}

bool cXMLTVSQLite::Transaction_Begin(void)
{  /// BEGIN a SQL transaction
   if (!DBHandle) {
      esyslog("sqlite3 E18: BEGIN - no valid DB handle found");
      return false;
   }

   if ( ExecDBQuery("BEGIN") != SQLITE_OK)
      return false;

   return true;
}

bool cXMLTVSQLite::Transaction_End(bool Commit)
{  /// COMMIT a SQL transaction or Rollback
   /// returns true on success or if no transaction was pending
   if (!DBHandle) {
      esyslog("sqlite3 E21: END - no valid DB handle found");
      return false;
   }

   // ignore error 1=cannot commit - no transaction is active
   if (ExecDBQuery(Commit ? "COMMIT" : "ROLLBACK") >= SQLITE_INTERNAL)
      return false;

   return true;
}

int cXMLTVSQLite::ExecDBQuery(const char *sqlQuery)
{  /// execute SQL query and return SQLite error code
   /// used only for single commands without output
   int SQLrc = SQLITE_OK;
   char *errmsg = NULL;
   if (!DBHandle)
   {
      esyslog("Error: no DB connection established (handle %8X)", (ulong)DBHandle);
      SQLrc = SQLITE_ERROR;
   }
   else
   {
      SQLrc = sqlite3_exec(DBHandle, sqlQuery, NULL, NULL, &errmsg);
      if (SQLrc != SQLITE_OK || errmsg)
      {
         esyslog("sqlite3: '%i %s' (eRC %d, %8X) executing query '%s'", SQLrc, errmsg ? errmsg : "none", sqlite3_extended_errcode(DBHandle), DBHandle, sqlQuery);
         sqlite3_free(errmsg);
      }
   }
   return SQLrc;
}

int cXMLTVSQLite::ExecDBQueryInt(const char *sqlQuery, uint &Integer)
{  /// execute DB query with single Integer (like count(*)) and return number in &Integer
   int result = SQLqryError;
   const char *errmsg = NULL;
   sqlite3_stmt *stmt = NULL;

   int SQLrc = sqlite3_prepare_v2(DBHandle, sqlQuery, -1, &stmt, NULL);
   if (SQLrc != SQLITE_OK) {
      result = SQLqryError;
      errmsg = sqlite3_errmsg(DBHandle);
      if (errmsg) {
         esyslog("sqlite3: '%i %s' (eRC %d, handle %8X) executing query '%s'", SQLrc, errmsg, sqlite3_extended_errcode(DBHandle), DBHandle, sqlQuery);
      }
   }
   else
   {  // get result
      SQLrc = sqlite3_step(stmt);
      if (SQLrc == SQLITE_ROW) {
         Integer = sqlite3_column_int(stmt, 0);
         result = SQLqryOne;
         SQLrc = sqlite3_step(stmt);
         if (SQLrc != SQLITE_DONE) {
            result = SQLqryMulti;
         }
      }
      else {
         CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);
         result = SQLqryEmpty;
      }
   }
   sqlite3_finalize(stmt);

   return result;
}

bool cXMLTVSQLite::CheckSQLiteSuccess(int SQLrc, int Line, const char * Function)
{
   bool success = true;
   switch(SQLrc) {
      case SQLITE_OK:
      case SQLITE_ROW:
      case SQLITE_DONE:
         break;
      default:
         esyslog("%s:%s %s SQLite Error: '%d-%s', %d-%s", __FILE__, Line ? *itoa(Line) : "", Function ? Function : "",
                 SQLrc, sqlite3_errstr(SQLrc), sqlite3_extended_errcode(DBHandle), sqlite3_errmsg(DBHandle));
         success = false;
         break;
   }
   return success;
}

int cXMLTVSQLite::Analyze(const char *dbName)
{  // speed up queries by updating statistics
   if (isempty(dbName))
      return -1;
   else
      return ExecDBQuery(cString::sprintf("ANALYZE %s;", dbName));
}

cString cXMLTVSQLite::SQLescape(const char *s, const char *chars)
{  /// make a string SQL compliant by
   if (isempty(s))
      return cString("NULL");
   else
   {
      char *buffer = MALLOC(char, 2* strlen(s) + 3);
      const char *p = s;
      char *t = buffer;
      *t++ = '\'';
      while (*p) {
         if (strchr(chars, *p)) {
            *t++ = '\'';
         }
         *t++ = *p++;
      }
      *t++ = '\'';
      *t = 0;
      return cString(buffer, true);
   }
}

cString cXMLTVSQLite::Time2Str(time_t t)
{
   char buf[25];
   struct tm tm_r;
   tm *tm = localtime_r(&t, &tm_r);
   strftime(buf, sizeof(buf), "%b %d %H:%M", tm);
   return buf;
}

// ================================ cEpisodesDB ==============================================
bool cEpisodesDB::OpenDBConnection(bool Create)
{  /// open a DB connection an return true on success
   if (!isempty(XMLTVConfig.EpisodesDBFile()) && !DBHandle)
   {
      if (cXMLTVSQLite::OpenDBConnection(XMLTVConfig.EpisodesDBFile(), SQLITE_OPEN_READWRITE | (Create ? SQLITE_OPEN_CREATE : 0)))
      {  // Enable memory mapping
         cXMLTVSQLite::ExecDBQuery(*cString::sprintf("pragma mmap_size = %llu;", MEGABYTE(16)));

         uint version = 0;
         int result = ExecDBQueryInt("pragma user_version", version);
         lastUpdate = version;
         if (!lastUpdate)
            CreateDB();
      }
      cString sqlQueryEpisodes = cString::sprintf("SELECT season, episode, episodeoverall FROM episodes, series "
                                                  "WHERE episodes.seriesid=series.id AND series.name = ?1 AND episodes.name = ?2;");
      stmtQueryEpisodes = NULL;
      int SQLrc = sqlite3_prepare_v2(DBHandle, *sqlQueryEpisodes, -1, &stmtQueryEpisodes, NULL);
      bool success = CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);

      cString sqlQueryAllEpisodes = cString::sprintf("SELECT episodes.name, season, episode, episodeoverall FROM episodes, series "
                                                  "WHERE episodes.seriesid=series.id AND series.name = ?1;");
      stmtQueryAllEpisodes = NULL;
      SQLrc = sqlite3_prepare_v2(DBHandle, *sqlQueryAllEpisodes, -1, &stmtQueryAllEpisodes, NULL);
      success &= CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);
   }

   return (DBHandle != NULL);
}


bool cEpisodesDB::CloseDBConnection(int Line)
{  /// Close DB connection
   int SQLrc = sqlite3_finalize(stmtQueryEpisodes);
   bool success = CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);

   SQLrc = sqlite3_finalize(stmtQueryAllEpisodes);
   success &= CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);

   return cXMLTVSQLite::CloseDBConnection(Line);
}

bool cEpisodesDB::CreateDB(void)
{  /// create DB with table "epg"
   isyslog("creating new episodes DB '%s'", XMLTVConfig.EpisodesDBFile());

   cString sqlCreate = cString::sprintf(
      "DROP TABLE IF EXISTS series; "
      "CREATE TABLE series (id INT NOT NULL, "
                           "name TEXT NOT NULL, "
                           "PRIMARY KEY(name));"
      "DROP TABLE IF EXISTS episodes; "
      "CREATE TABLE episodes (seriesid INT NOT NULL, "
                             "name TEXT NOT NULL, "
                             "season INT, "
                             "episode INT, "
                             "episodeoverall INT, "
                             "PRIMARY KEY(seriesid, name));");

   int SQLrc = ExecDBQuery(*sqlCreate);
   if (SQLrc != SQLITE_OK)
      esyslog("ERROR creating episodes DB - SQL RC %d", SQLrc);

   return SQLrc == SQLITE_OK;
}

bool stripend(const char *s, const char *p)
{
   char *se = (char *)s + strlen(s) - 1;
   const char *pe = p + strlen(p) - 1;
   while (pe >= p) {
      if (*pe-- != *se-- || (se < s && pe >= p))
         return false;
   }
   *se = 0;
   return true;
}

bool cEpisodesDB::UpdateDB()
{
   bool success = false;

   if (!isempty(XMLTVConfig.EpisodesDir())) {
      time_t newestFileTime = lastUpdate;
      cReadDir episodesDir(XMLTVConfig.EpisodesDir());
      if (episodesDir.Ok()) {
         isyslog("Episodes DB: importing Files newer than %s", *TimeToString(lastUpdate));
         struct dirent *e;
         while ((e = episodesDir.Next()) != NULL) {
            cString epFilename = AddDirectory(XMLTVConfig.EpisodesDir(), e->d_name);
            struct stat st;
            if ((lstat(*epFilename, &st) == 0) && ((st.st_mode & S_IFMT) == S_IFREG) && (lastUpdate < st.st_mtime))
            {  // not a symlink
               cString epTitle = cString(e->d_name, strstr(e->d_name, ".episodes"));
               stripend(*epTitle, ".en");
               stripend(*epTitle, ".de");
               uint seriesId = 0;
               int result = ExecDBQueryInt(*cString::sprintf("SELECT id FROM series WHERE name = %s;", *SQLescape(*epTitle)), seriesId);
               if (result == SQLqryEmpty) {
                  ExecDBQueryInt("SELECT MAX(id) from series;", seriesId);
                  seriesId += 1;
                  ExecDBQuery(*cString::sprintf("INSERT INTO series VALUES(%d, %s)", seriesId, *SQLescape(*epTitle)));
               }

               FILE *epFile = fopen(*epFilename, "r");
               if (!epFile) {
                  esyslog("opening %s failed", *epFilename);
               }
               else
               {
                  uint season, episode, episodeOverall;
                  cReadLine ReadLine;
                  char *line;
                  char episodeName[201];
                  char extraCols[201];
                  Transaction_Begin();
                  uint cnt = 0;
                  while (((line = ReadLine.Read(epFile)) != NULL)) {
                     if (line[0] == '#') continue;
                     if (sscanf(line, "%d\t%d\t%d\t%200[^\t\n]\t%200[^]\n]", &season, &episode, &episodeOverall, episodeName, extraCols) >= 4)
                     {
                        cString nrmEpisodeName = StringNormalize(episodeName);
                        if (!isempty(*nrmEpisodeName)) {
                           result = ExecDBQuery(*cString::sprintf("INSERT OR REPLACE INTO episodes VALUES(%d, %s, %d, %d, %d)", seriesId, *SQLescape(*nrmEpisodeName), season, episode, episodeOverall));
                           cnt++;
                        }
                     }
                  }
                  Transaction_End();
                  if (newestFileTime < st.st_mtime)
                     newestFileTime = st.st_mtime;
#ifdef DBG_EPISODES
                  tsyslog("EpisodesUpdate8: %4d %3d %s %s", seriesId, cnt, *DayDateTime(newestFileTime), *epTitle);
#endif
               }
               fclose(epFile);
            }
         }
         ExecDBQuery(cString::sprintf("PRAGMA user_version = %lu", newestFileTime));
      }

      cReadDir episodesDir2(XMLTVConfig.EpisodesDir());
      if (episodesDir2.Ok()) {
         struct dirent *e;
         while ((e = episodesDir2.Next()) != NULL) {
            cString epFilename = AddDirectory(XMLTVConfig.EpisodesDir(), e->d_name);
            struct stat st;
            if ((lstat(*epFilename, &st) == 0) && ((st.st_mode & S_IFMT) == S_IFLNK))
            {
               char buff[PATH_MAX];
               ssize_t len = readlink(*epFilename, buff, sizeof(buff) - 1);
               if (len != -1)
               {  // link exists and target was successfully read
                  buff[len] = 0;
                  cString epTarget = cString(buff, strstr(buff, ".episodes"));
                  cString epTitle = cString(e->d_name, strstr(e->d_name, ".episodes"));
                  stripend(*epTitle, ".en");
                  stripend(*epTitle, ".de");
                  if (!isempty(*epTitle)) {
                     uint seriesId = 0;
                     int result = ExecDBQueryInt(*cString::sprintf("SELECT id FROM series WHERE name = %s;", *SQLescape(epTarget)), seriesId);
                     if (result == SQLqryOne) {
                        ExecDBQuery(*cString::sprintf("INSERT OR REPLACE INTO series VALUES(%d, %s)", seriesId, *SQLescape(*epTitle)));
                     }
                  }
               }
            }
         }
      }
      lastUpdate = newestFileTime;
      Analyze("episodes");
      isyslog("Episodes DB: imported files until %s", *TimeToString(lastUpdate));

      success = true; // ??
   }
   return success;
}

bool cEpisodesDB::QueryEpisode(cXMLTVEvent *xtEvent)
{
   bool found = false;
   if (stmtQueryEpisodes && xtEvent && !isempty(xtEvent->Title()))
   {
      cString eventTitle = xtEvent->Title();
      strreplace((char *)*eventTitle, '*', '.');
      cString eventSplitTitle, eventSplitShortText;
      cString eventShortText = xtEvent->ShortText();
      if (isempty(*eventShortText) && !isempty(xtEvent->Description()) && strlen(xtEvent->Description()) < 100)
         eventShortText = xtEvent->Description();

      if (!isempty(eventShortText))
      {  // try complete shorttext
         uint l;
         eventShortText = StringCleanup((char*)*eventShortText, true, true);
         for (uint i=0; l = Utf8CharLen((*eventShortText)+i), i<strlen(*eventShortText); i += l)
            if (l == 1) {
               char *symbol = (char*)(*eventShortText)+i;
               *symbol = tolower(*symbol);
            }
         sqlite3_bind_text(stmtQueryEpisodes, 1, eventTitle, -1, SQLITE_STATIC);
         sqlite3_bind_text(stmtQueryEpisodes, 2, eventShortText, -1, SQLITE_STATIC);

         int SQLrc1 = sqlite3_step(stmtQueryEpisodes);
         CheckSQLiteSuccess(SQLrc1, __LINE__, __FUNCTION__);
         if (SQLrc1 == SQLITE_ROW)
         {
            found = true;
            xtEvent->SetSeason(sqlite3_column_int(stmtQueryEpisodes, 0));
            xtEvent->SetEpisode(sqlite3_column_int(stmtQueryEpisodes, 1));
            xtEvent->SetEpisodeOverall(sqlite3_column_int(stmtQueryEpisodes, 2));
         }
         sqlite3_clear_bindings(stmtQueryEpisodes);
         sqlite3_reset(stmtQueryEpisodes);
      }

      char *p = (char *)strchrn(*eventTitle, ':', 1);
      if (!found && p && !isempty(skipspace(p+1)))
      {  // try with title split at colon
         eventSplitTitle = cString(*eventTitle, p);
         eventSplitShortText = StringCleanup(skipspace(p+1), true, true);
         uint l;
         for (uint i=0; l = Utf8CharLen((*eventSplitShortText)+i), i<strlen(*eventSplitShortText); i += l)
            if (l == 1) {
               char *symbol = (char*)(*eventSplitShortText)+i;
               *symbol = tolower(*symbol);
            }
         sqlite3_bind_text(stmtQueryEpisodes, 1, eventSplitTitle, -1, SQLITE_STATIC);
         sqlite3_bind_text(stmtQueryEpisodes, 2, eventSplitShortText, -1, SQLITE_STATIC);

         int SQLrc1 = sqlite3_step(stmtQueryEpisodes);
         CheckSQLiteSuccess(SQLrc1, __LINE__, __FUNCTION__);
         if (SQLrc1 == SQLITE_ROW)
         {
            found = true;
            xtEvent->SetSeason(sqlite3_column_int(stmtQueryEpisodes, 0));
            xtEvent->SetEpisode(sqlite3_column_int(stmtQueryEpisodes, 1));
            xtEvent->SetEpisodeOverall(sqlite3_column_int(stmtQueryEpisodes, 2));
         }
         sqlite3_clear_bindings(stmtQueryEpisodes);
         sqlite3_reset(stmtQueryEpisodes);
      }

      if(!found && !isempty(xtEvent->ShortText()))
      {  // if still not found try all episodes with Levenshtein
         sqlite3_bind_text(stmtQueryAllEpisodes, 1, eventTitle, -1, SQLITE_STATIC);

         int season = 0;
         int episode = 0;
         int episodeOverall = 0;
         cString episodeName;
         eventShortText = StringNormalize(*eventShortText);

         uint l = strlen(*eventShortText) + 1;
         uint *s1 = new uint[l];
         uint l1 = Utf8ToArray(*eventShortText, s1, l);
         int dist = l1/3;

         int curDist = 0;
         int SQLrc1 = sqlite3_step(stmtQueryAllEpisodes);
         CheckSQLiteSuccess(SQLrc1, __LINE__, __FUNCTION__);
         while (dist > 0 && SQLrc1 == SQLITE_ROW)
         {
            const char *dbEpisode = (const char *) sqlite3_column_text(stmtQueryAllEpisodes, 0);
            l = strlen(dbEpisode) + 1;
            uint *s2 = new uint[l];
            uint l2 = Utf8ToArray(dbEpisode, s2, l);

            curDist = LevenshteinDistance(s1, l1, s2, l2);
            if (curDist < dist) {
               found = true;
               dist = curDist;
               season         = sqlite3_column_int(stmtQueryAllEpisodes, 1);
               episode        = sqlite3_column_int(stmtQueryAllEpisodes, 2);
               episodeOverall = sqlite3_column_int(stmtQueryAllEpisodes, 3);
               episodeName = dbEpisode;
            }
            delete[] s2;
            SQLrc1 = sqlite3_step(stmtQueryAllEpisodes);
         }
         sqlite3_clear_bindings(stmtQueryAllEpisodes);
         sqlite3_reset(stmtQueryAllEpisodes);
         delete[] s1;

         if (found) {
            xtEvent->SetSeason(season);
            xtEvent->SetEpisode(episode);
            xtEvent->SetEpisodeOverall(episodeOverall);
         }
      }

      if (!found && p && !isempty(skipspace(p+1)))
      {  // try Levenshtein with split title
         sqlite3_bind_text(stmtQueryAllEpisodes, 1, eventSplitTitle, -1, SQLITE_STATIC);

         int season = 0;
         int episode = 0;
         int episodeOverall = 0;
         cString episodeName;
         eventSplitShortText = StringNormalize(*eventSplitShortText);

         uint l = strlen(*eventSplitShortText);
         uint *s1 = new uint[l];
         uint l1 = Utf8ToArray(*eventSplitShortText, s1, l);
         int dist = l1/3;

         int curDist = 0;
         int SQLrc1 = sqlite3_step(stmtQueryAllEpisodes);
         CheckSQLiteSuccess(SQLrc1, __LINE__, __FUNCTION__);
         while (dist > 0 && SQLrc1 == SQLITE_ROW)
         {
            const char *dbEpisode = (const char *) sqlite3_column_text(stmtQueryAllEpisodes, 0);
            l = strlen(dbEpisode);
            uint *s2 = new uint[l];
            uint l2 = Utf8ToArray(dbEpisode, s2, l);

            curDist = LevenshteinDistance(s1, l1, s2, l2);
            if (dist > curDist) {
               found = true;
               dist = curDist;
               season         = sqlite3_column_int(stmtQueryAllEpisodes, 1);
               episode        = sqlite3_column_int(stmtQueryAllEpisodes, 2);
               episodeOverall = sqlite3_column_int(stmtQueryAllEpisodes, 3);
               episodeName = dbEpisode;
            }
            delete[] s2;
            SQLrc1 = sqlite3_step(stmtQueryAllEpisodes);
         }
         sqlite3_clear_bindings(stmtQueryAllEpisodes);
         sqlite3_reset(stmtQueryAllEpisodes);
         delete[] s1;

         if (found) {
            xtEvent->SetSeason(season);
            xtEvent->SetEpisode(episode);
            xtEvent->SetEpisodeOverall(episodeOverall);
         }
      }
   }

   return found;
}

#define TABLEVERSION_UNSEEN 0x7FFF
#define DELETE_FLAG         0x8000
#define SCHEMA_VERSION 21
#define TIMERANGE (2*60*59)  // nearly 2 hrs

// ================================ cXMLTVDB ==============================================
bool cXMLTVDB::OpenDBConnection(bool Create)
{  /// open a DB connection an return true on success
   if (!XMLTVConfig.DBinitialized() || !DBHandle)
   {
      if (cXMLTVSQLite::OpenDBConnection(XMLTVConfig.EPGDBFile(), SQLITE_OPEN_READWRITE | (Create ? SQLITE_OPEN_CREATE : 0)))
      {  // Enable memory mapping
         cXMLTVSQLite::ExecDBQuery(*cString::sprintf("pragma mmap_size = %llu;", MEGABYTE(256)));
      }
   }
   return (DBHandle != NULL);
}


bool cXMLTVDB::CloseDBConnection(int Line)
{  /// Close DB connection
   return cXMLTVSQLite::CloseDBConnection(Line);
}

bool cXMLTVDB::UpgradeDB(bool ForceCreate)
{  // check if user_version is correct, otherwise create new DB
   bool success = false;
   bool create = ForceCreate;

   success = OpenDBConnection(true);
   if (success) {
      if (!ForceCreate) {
         uint version = 0;
         int result = ExecDBQueryInt("pragma user_version", version);
         if (result == 0 || (result == 1 && version != SCHEMA_VERSION))
         {  // schema was updated
            esyslog("xmltv4vdr SQLite schema has wrong version %d or is missing, expected %d - re-creating DB", version, SCHEMA_VERSION);
            create = true;
         }
      }
      if (create) {
         success = CreateDB();
         if (success)
            isyslog("SQLite DB successfully created");
         else
            esyslog("SQLite DB creation failed");
      }
      CloseDBConnection(__LINE__);
   }
   return success;
}

bool cXMLTVDB::CreateDB(void)
{  /// create DB with table "epg"
   isyslog("creating new EPG DB '%s'", XMLTVConfig.EPGDBFile());

   cString sqlCreate = cString::sprintf(
      "DROP TABLE IF EXISTS epg; "
      "CREATE TABLE epg (src TEXT           NOT NULL, " //  0 PK
                        "channelid TEXT     NOT NULL, " //  1 PK
                        "starttime DATETIME NOT NULL, " //  2 PK
                        "eventid INT, "                 //  3 // EIT ID is 16 Bit but VDR stores 32Bit, so self-created IDs for new events can be > 0xFFFF
                        "xteventid INT, "               //  4
                        "tableversion INT, "            //  5
                        "duration INT, "                //  6
                        "title TEXT, "                  //  7
                        "shorttext TEXT, "              //  8
                        "description TEXT, "            //  9
                        "season INT, "                  // 10
                        "episode INT, "                 // 11
                        "episodeoverall INT, "          // 12
                        "origtitle TEXT, "              // 13
                        "country TEXT, "                // 14
                        "year INT, "                    // 15
                        "credits TEXT, "                // 16
                        "category TEXT, "               // 17
                        "review TEXT, "                 // 18
                        "parentalrating TEXT, "         // 19
                        "starrating TEXT, "             // 20
                        "pics TEXT, "                   // 21
                        "PRIMARY KEY(src, channelid, starttime));" // avoid duplicate events
      "CREATE INDEX IF NOT EXISTS ndx1 ON epg (eventid, channelid, src); "
      "CREATE INDEX IF NOT EXISTS ndx3 ON epg (src, pics);"
      "PRAGMA user_version = %d; ", SCHEMA_VERSION);

   int SQLrc = ExecDBQuery(*sqlCreate);
   if (SQLrc != SQLITE_OK)
      esyslog("ERROR creating DB - SQL RC %d", SQLrc);

   XMLTVConfig.EPGSources()->ResetEventStarttimes();

   return SQLrc == SQLITE_OK;
}

int cXMLTVDB::Analyze(void)
{
   return Analyze("epg");
}


bool cXMLTVDB::MarkEventsOutdated(cChannelList *ChannelList, const char *SourceName)
{
   cString channelList = "";
   for (int i = 0; i < ChannelList->Size(); i++) {
      if (i)
         channelList.Append(", ");
      channelList.Append(SQLescape(ChannelList->At(i)->GetChannelIDString()));
   }

   bool success = ExecDBQuery(*cString::sprintf("UPDATE epg SET tableversion = tableversion | %d WHERE src = '%s' AND channelid IN (%s);", DELETE_FLAG, SourceName, *channelList)) == SQLITE_OK;

   return success;
}


bool cXMLTVDB::DropOutdatedEvents(cChannelList *ChannelList, const char *SourceName, time_t LastEventStarttime)
{
   int linksDeleted = 0, picsDeleted = 0, eventsDeleted = 0;
   cString channelList = "";
   for (int i = 0; i < ChannelList->Size(); i++) {
      if (i)
         channelList.Append(", ");
      channelList.Append(SQLescape(ChannelList->At(i)->GetChannelIDString()));
   }

   bool success = true;
   success = DropEventList(&linksDeleted, &eventsDeleted, *cString::sprintf("WHERE src = '%s' AND channelid IN (%s) AND tableversion >= %d AND starttime <= %lu;", SourceName, *channelList, DELETE_FLAG, LastEventStarttime));

   return success;
}


bool cXMLTVDB::ImportXMLTVEventPrepare(const char *SourceName)
{
   cString sqlImportXMLTVEventSelect = cString::sprintf("SELECT eventid, pics FROM epg WHERE src='%s' AND channelid=?1 AND starttime = ?2;", SourceName);

   stmtImportXMLTVEventSelect = NULL;
   int SQLrc = sqlite3_prepare_v2(DBHandle, *sqlImportXMLTVEventSelect, -1, &stmtImportXMLTVEventSelect, NULL);
   bool success = CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);

   cString sqlImportXMLTVEventUpdate = cString::sprintf("UPDATE epg SET xteventid=?11, tableversion=%u, duration=?12, title=?13, origtitle=?14, "
                                     "shorttext=?21, description=?22, country=?23, year=?24, credits=?25, "
                                     "category=?31, review=?32, parentalrating=?33, starrating=?34, "
                                     "season=?41, episode=?42, episodeoverall=?43, pics=?44 "
                                     "WHERE src='%s' AND channelid=?91 AND starttime=?92;", TABLEVERSION_UNSEEN, SourceName);
   stmtImportXMLTVEventUpdate = NULL;
   SQLrc = sqlite3_prepare_v2(DBHandle, *sqlImportXMLTVEventUpdate, -1, &stmtImportXMLTVEventUpdate, NULL);
   success = success && CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);

   cString sqlImportXMLTVEventInsert = cString::sprintf("INSERT INTO epg (src, channelid, xteventid, starttime, duration, title, "
                                     "origtitle, shorttext, description, country, "
                                     "year, credits, category, review, parentalrating, "
                                     "starrating, season, episode, episodeoverall, "
                                     "pics, tableversion) VALUES "
                                     "('%s', ?11, ?12, ?13, ?14, ?15, "
                                     "?21, ?22, ?23, ?24, "
                                     "?31, ?32, ?33, ?34, ?35, "
                                     "?41, ?42, ?43, ?44, "
                                     "?51, %u);", SourceName, TABLEVERSION_UNSEEN);
   stmtImportXMLTVEventInsert = NULL;
   SQLrc = sqlite3_prepare_v2(DBHandle, *sqlImportXMLTVEventInsert, -1, &stmtImportXMLTVEventInsert, NULL);
   success = success && CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);

   return success;
}

bool cXMLTVDB::ImportXMLTVEventFinalize()
{
   int SQLrc = sqlite3_finalize(stmtImportXMLTVEventSelect);
   bool success = CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);

   SQLrc = sqlite3_finalize(stmtImportXMLTVEventUpdate);
   success = success && CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);

   SQLrc = sqlite3_finalize(stmtImportXMLTVEventInsert);
   success = success && CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);

   return success;
}

bool cXMLTVDB::ImportXMLTVEvent(cXMLTVEvent *xtEvent, cChannelList *ChannelList, const char *SourceName)
{  /// insert xtEvent in DB for all channels in ChannelList
   /// - rows identified by SourceName, channnelID and xtEventID

#define DELTATIME (120*60)
   bool success = true;
   int cl = 0;
   while (cl < ChannelList->Size() && success)
   {
      cString channelID = ChannelList->At(cl)->GetChannelIDString();
      sqlite3_bind_text(stmtImportXMLTVEventSelect, 1, *channelID, -1, SQLITE_STATIC);
      sqlite3_bind_int (stmtImportXMLTVEventSelect, 2, xtEvent->StartTime());

      tEventID dbEventID = 0;
      cString dbPictures;

      int SQLrc1 = sqlite3_step(stmtImportXMLTVEventSelect);
      CheckSQLiteSuccess(SQLrc1, __LINE__, __FUNCTION__);
      if (SQLrc1 == SQLITE_ROW)
      {  // update
         dbEventID  = sqlite3_column_int(stmtImportXMLTVEventSelect, 0);
         dbPictures = (const char *) sqlite3_column_text(stmtImportXMLTVEventSelect, 1);

         if ((!isempty(*dbPictures) && xtEvent->Pics()->Size() == 0) ||
             (!isempty(*dbPictures) && xtEvent->Pics()->Size() > 0 && strcmp(*dbPictures, xtEvent->Pics()->ToString())))
         {
            cXMLTVStringList dbPictureList;
            dbPictureList.SetStringList(dbPictures);
            for (int p = 0; p < dbPictureList.Size(); p++)
               AddOrphanedPicture(SourceName, dbPictureList.At(p));

            UnlinkPictures(&dbPictureList, *channelID, dbEventID);  // required to avoid links to wrong pictures
         }

         sqlite3_bind_int64(stmtImportXMLTVEventUpdate, 11, xtEvent->XTEventID());
         sqlite3_bind_int  (stmtImportXMLTVEventUpdate, 12, xtEvent->Duration());
         sqlite3_bind_text (stmtImportXMLTVEventUpdate, 13, xtEvent->Title(), -1, SQLITE_STATIC);
         sqlite3_bind_text (stmtImportXMLTVEventUpdate, 14, xtEvent->OrigTitle(),-1, SQLITE_STATIC);

         sqlite3_bind_text (stmtImportXMLTVEventUpdate, 21, xtEvent->ShortText(), -1, SQLITE_STATIC);
         sqlite3_bind_text (stmtImportXMLTVEventUpdate, 22, xtEvent->Description(), -1, SQLITE_STATIC);
         sqlite3_bind_text (stmtImportXMLTVEventUpdate, 23, xtEvent->Country(), -1, SQLITE_STATIC);
         sqlite3_bind_int  (stmtImportXMLTVEventUpdate, 24, xtEvent->Year());
         sqlite3_bind_text (stmtImportXMLTVEventUpdate, 25, xtEvent->Credits()->ToString(), -1, SQLITE_STATIC);

         sqlite3_bind_text (stmtImportXMLTVEventUpdate, 31, xtEvent->Category()->ToString(), -1, SQLITE_STATIC);
         sqlite3_bind_text (stmtImportXMLTVEventUpdate, 32, xtEvent->Review()->ToString(),-1, SQLITE_STATIC);
         sqlite3_bind_text (stmtImportXMLTVEventUpdate, 33, xtEvent->ParentalRating()->ToString(), -1, SQLITE_STATIC);
         sqlite3_bind_text (stmtImportXMLTVEventUpdate, 34, xtEvent->StarRating()->ToString(), -1, SQLITE_STATIC);

         sqlite3_bind_int  (stmtImportXMLTVEventUpdate, 41, xtEvent->Season());
         sqlite3_bind_int  (stmtImportXMLTVEventUpdate, 42, xtEvent->Episode());
         sqlite3_bind_int  (stmtImportXMLTVEventUpdate, 43, xtEvent->EpisodeOverall());
         sqlite3_bind_text (stmtImportXMLTVEventUpdate, 44, xtEvent->Pics()->ToString(), -1, SQLITE_STATIC);

         sqlite3_bind_text (stmtImportXMLTVEventUpdate, 91, *channelID, -1, SQLITE_STATIC);
         sqlite3_bind_int  (stmtImportXMLTVEventUpdate, 92, xtEvent->StartTime());

         int SQLrc = sqlite3_step(stmtImportXMLTVEventUpdate);
         CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);
         success = (SQLrc == SQLITE_DONE);
         if(!success) esyslog("ImportXMLTVEvent Update failed: %d %lu %s %s %s", SQLrc, xtEvent->XTEventID(), SourceName, *channelID, *Time2Str(xtEvent->StartTime()));

         sqlite3_clear_bindings(stmtImportXMLTVEventUpdate);
         sqlite3_reset(stmtImportXMLTVEventUpdate);
      }
      else if (SQLrc1 == SQLITE_DONE)
      {  // entry does not yet exist
         sqlite3_bind_text (stmtImportXMLTVEventInsert, 11, *channelID, -1, SQLITE_STATIC);
         sqlite3_bind_int64(stmtImportXMLTVEventInsert, 12, xtEvent->XTEventID());
         sqlite3_bind_int  (stmtImportXMLTVEventInsert, 13, xtEvent->StartTime());
         sqlite3_bind_int  (stmtImportXMLTVEventInsert, 14, xtEvent->Duration());
         sqlite3_bind_text (stmtImportXMLTVEventInsert, 15, xtEvent->Title(), -1, SQLITE_STATIC);

         sqlite3_bind_text (stmtImportXMLTVEventInsert, 21, xtEvent->OrigTitle(), -1, SQLITE_STATIC);
         sqlite3_bind_text (stmtImportXMLTVEventInsert, 22, xtEvent->ShortText(),-1, SQLITE_STATIC);
         sqlite3_bind_text (stmtImportXMLTVEventInsert, 23, xtEvent->Description(), -1, SQLITE_STATIC);
         sqlite3_bind_text (stmtImportXMLTVEventInsert, 24, xtEvent->Country(), -1, SQLITE_STATIC);

         sqlite3_bind_int  (stmtImportXMLTVEventInsert, 31, xtEvent->Year());
         sqlite3_bind_text (stmtImportXMLTVEventInsert, 32, xtEvent->Credits()->ToString(), -1, SQLITE_STATIC);
         sqlite3_bind_text (stmtImportXMLTVEventInsert, 33, xtEvent->Category()->ToString(), -1, SQLITE_STATIC);
         sqlite3_bind_text (stmtImportXMLTVEventInsert, 34, xtEvent->Review()->ToString(), -1, SQLITE_STATIC);
         sqlite3_bind_text (stmtImportXMLTVEventInsert, 35, xtEvent->ParentalRating()->ToString(),-1, SQLITE_STATIC);

         sqlite3_bind_text (stmtImportXMLTVEventInsert, 41, xtEvent->StarRating()->ToString(), -1, SQLITE_STATIC);
         sqlite3_bind_int  (stmtImportXMLTVEventInsert, 42, xtEvent->Season());
         sqlite3_bind_int  (stmtImportXMLTVEventInsert, 43, xtEvent->Episode());
         sqlite3_bind_int  (stmtImportXMLTVEventInsert, 44, xtEvent->EpisodeOverall());

         sqlite3_bind_text (stmtImportXMLTVEventInsert, 51, xtEvent->Pics()->ToString(), -1, SQLITE_STATIC);

         int SQLrc = sqlite3_step(stmtImportXMLTVEventInsert);
         CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);
         success = (SQLrc == SQLITE_DONE);
         if(!success) esyslog("ImportXMLTVEvent Insert failed: %d %lu %s %s %s", SQLrc, xtEvent->XTEventID(), SourceName, *channelID, *Time2Str(xtEvent->StartTime()));

         sqlite3_clear_bindings(stmtImportXMLTVEventInsert);
         sqlite3_reset(stmtImportXMLTVEventInsert);
      }
      else {
         // SQL Error
      }
      cl++;
   }
   sqlite3_reset(stmtImportXMLTVEventSelect);

   return success;
}

int cXMLTVDB::UnlinkPictures(const char *Pictures, const char *ChannelID, const tEventID EventID)
{
   cXMLTVStringList pictureList;
   pictureList.SetStringList(Pictures);

   return UnlinkPictures(&pictureList, ChannelID, EventID);
}

int cXMLTVDB::UnlinkPictures(const cXMLTVStringList *Pics, const char *ChannelID, const tEventID EventID)
{  /// delete links to the pictures of supplied channelID and eventID
   int deleted = 0;

   if (EventID > 0) {
#ifdef DBG_DROPEVENTS2
      tsyslog("UnlinkPictures: unlinking %s_%u_[0-%u].jpg", ChannelID, EventID, Pics->Size()-1);
#endif
      for (int i = 0; i < Pics->Size(); i++)
      {
         char *pic = (*Pics)[i];
         char *ext = strrchr((*Pics)[i], '.');
         if (!ext) continue;
         ext++;
         struct stat statbuf;

         // unlink EventID.jpg
         cString lnk = cString::sprintf("%s/%u_%u.%s", XMLTVConfig.ImageDir(), EventID, i, ext);
         if (unlink(*lnk) == 0) {
            deleted++;
         }

         // unlink channelID_EventID.jpg
         lnk = cString::sprintf("%s/%s_%u_%u.%s", XMLTVConfig.ImageDir(), ChannelID, EventID, i, ext);
         if (unlink(*lnk) == 0) {
#ifdef DBG_DROPEVENTS2_X
            tsyslog("UnlinkPictures: unlinked %s_%u_%u.%s", ChannelID, EventID, i, ext);
#endif
            deleted++;
         }
#ifdef DBG_DROPEVENTS2
         else
            if (errno != ENOENT)
               tsyslog("UnlinkPictures: unlinking failed: %d %s", errno, *lnk);
#endif
      }
   }
   return deleted;
}

bool cXMLTVDB::AddOrphanedPicture(const char *Source, const char *Picture)
{
   bool added = false;
   if ((added = (-1 == orphanedPictures.Find(Source, Picture))))
      orphanedPictures.AppendStrings(Source, Picture);

   return added;
}

int cXMLTVDB::DeletePictures(void)
{  // delete pics if no additional reference in DB
   int pics_deleted = 0, links_deleted = 0;
   OpenDBConnection();
   cXMLTVStringList linkList;
   for (int i = 0; i < orphanedPictures.Size(); i++) {
      cString sqlQueryPics = cString::sprintf("SELECT count(*) FROM epg WHERE src='%s' AND pics LIKE '%%%s%%';",
                                              orphanedPictures.At(i)->Source(), orphanedPictures.At(i)->Picture());

      uint picCount = 0;
      int result = ExecDBQueryInt(*sqlQueryPics, picCount);  //TODO precompile SQL query
      if (result != 1) {
         tsyslog("DeletePictures: SQL-Error %d (0x%08X)", result, DBHandle);
      }
      else {
         if (picCount == 0) {
            cString picFilename = cString::sprintf("%s/%s-img/%s", XMLTVConfig.EPGSourcesDir(), orphanedPictures.At(i)->Source(), orphanedPictures.At(i)->Picture());
            struct stat statbuf;
            if ((stat(*picFilename, &statbuf) == 0) && (statbuf.st_mode & (S_IFLNK | S_IFREG))) {  // regular file or link
               if (unlink(*picFilename) == 0) {
#ifdef DBG_DROPEVENTS2
                  tsyslog("DeletePictures: deleted %s %s", orphanedPictures.At(i)->Source(), orphanedPictures.At(i)->Picture());
#endif
                  pics_deleted++;
               }
            }
         }
         else {
#ifdef DBG_DROPEVENTS2
            tsyslog("DeletePictures: NOT deleted (%d) %s %s", picCount, orphanedPictures.At(i)->Source(), orphanedPictures.At(i)->Picture());
#endif
         }
      }
   }
   CloseDBConnection(__LINE__);

#ifdef DBG_DROPEVENTS2
      tsyslog("DeletePictures: deleted %d pics", pics_deleted);
#endif

   orphanedPictures.Clear();
   return pics_deleted;
}

cXMLTVEvent *cXMLTVDB::FillXMLTVEventFromDB(sqlite3_stmt *stmt)
{  // fill xEvent with supplied row from SQL query
   // Returns pointer to cXMLTVEvent, must be deleted by caller
   bool success = false;
   cXMLTVEvent *xtEvent = NULL;
   if (stmt)
   {
      xtEvent = new cXMLTVEvent();
      int cols = sqlite3_column_count(stmt);
      for (int col = 0; col < cols; col++)
      {
         switch (col)
         {
            case  0: xtEvent->SetSource((const char *) sqlite3_column_text(stmt, col)); break;
            case  1: xtEvent->SetChannelID((const char *) sqlite3_column_text(stmt, col)); break;
            case  2: xtEvent->SetStartTime(sqlite3_column_int(stmt, col)); break;
            case  3: xtEvent->SetEventID(sqlite3_column_int(stmt, col)); break;
            case  4: xtEvent->SetXTEventID((uint64_t)sqlite3_column_int64(stmt, col)); break;
            case  5: { uint tableVersion = sqlite3_column_int(stmt, col);
                       xtEvent->SetTableID((tableVersion >> 8) & 0x00FF);
                       xtEvent->SetVersion(tableVersion & 0x00FF); break; }
            case  6: xtEvent->SetDuration(sqlite3_column_int(stmt, col)); break;
            case  7: xtEvent->SetTitle((const char *) sqlite3_column_text(stmt, col)); break;
            case  8: xtEvent->SetShortText((const char *) sqlite3_column_text(stmt, col)); break;
            case  9: xtEvent->SetDescription((const char *) sqlite3_column_text(stmt, col)); break;
            case 10: xtEvent->SetSeason(sqlite3_column_int(stmt, col)); break;
            case 11: xtEvent->SetEpisode(sqlite3_column_int(stmt, col)); break;
            case 12: xtEvent->SetEpisodeOverall(sqlite3_column_int(stmt, col)); break;
            case 13: xtEvent->SetOrigTitle((const char *) sqlite3_column_text(stmt, col)); break;
            case 14: xtEvent->SetCountry((const char *) sqlite3_column_text(stmt, col)); break;
            case 15: xtEvent->SetYear(sqlite3_column_int(stmt, col)); break;
            case 16: xtEvent->SetCredits((const char *) sqlite3_column_text(stmt, col)); break;
            case 17: xtEvent->SetCategories((const char *) sqlite3_column_text(stmt, col)); break;
            case 18: xtEvent->SetReviews((const char *) sqlite3_column_text(stmt, col)); break;
            case 19: xtEvent->SetParentalRating((const char *) sqlite3_column_text(stmt, col)); break;
            case 20: xtEvent->SetStarRating((const char *) sqlite3_column_text(stmt, col)); break;
            case 21: xtEvent->SetPics((const char *) sqlite3_column_text(stmt, col)); break;
         }
      }
      success = true;
   }
   else 
      tsyslog("FillXMLTVEventFromDB stmt is NULL");
   return xtEvent;
}

bool cXMLTVDB::IsNewVersion(const cEvent* Event)
{  // do not call with empty Event->Aux()
   const char *strStart = strstr(Event->Aux(), xmlTagTVStart);
   if (strStart) {
      uint16_t tableVersion = atoi(strStart + strlen(xmlTagTVStart));
      return tableVersion != (((uint16_t)Event->TableID() << 8) | Event->Version());
   }
   else
      return true;
}

bool cXMLTVDB::UpdateEventPrepare(const char *ChannelID, const char *SourceName)
{
   cString sqlUpdateEventSelect = cString::sprintf("SELECT src, channelid, starttime, eventid, xteventid, tableversion, duration, title, "
                                   "shorttext, description, season, episode, episodeoverall, origtitle, "
                                   "country, year, credits, category, review, parentalrating, starrating, pics FROM epg "
                                   "WHERE src = '%s' AND channelid = '%s' AND eventid = ?1;",
                                   SourceName, ChannelID);
   stmtUpdateEventSelect = NULL;
   int SQLrc = sqlite3_prepare_v2(DBHandle, *sqlUpdateEventSelect, -1, &stmtUpdateEventSelect, NULL);
   return CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);
}

bool cXMLTVDB::UpdateEventFinalize()
{
   int SQLrc = sqlite3_finalize(stmtUpdateEventSelect);
   return CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);
}


bool cXMLTVDB::UpdateEvent(cEvent *Event, uint64_t Flags, const char *ChannelName, const char *SourceName, time_t LastEventStarttime)
{  /// search in DB for Event and fill it with content of xtEvent
   /// called by HandleEvent() ONLY

   bool handled = false;
   cXMLTVEvent *xtEvent = NULL;

   if (Event)
   {
      // EPGFIX: remove repeated Title at beginning of ShortText
      // ATTENTION: Does NOT work for e.g. KiKa
      if (XMLTVConfig.FixDuplTitleInShortttext() && Event->Title() && Event->ShortText() && (strstr(Event->ShortText(), Event->Title()) == Event->ShortText())) {
         size_t titleLen = strlen(Event->Title()) + 1;
         while (isspace(*(Event->ShortText() + titleLen)) && strlen(Event->ShortText()) > titleLen) titleLen++;
         if (strlen(Event->ShortText()) > titleLen)
            strshift((char *)Event->ShortText(), titleLen);
      }
      cString sqlSelect;

      // search by eventID in DB
      sqlite3_bind_int(stmtUpdateEventSelect, 1, Event->EventID());

      if (sqlite3_step(stmtUpdateEventSelect) == SQLITE_ROW) {
         xtEvent = FillXMLTVEventFromDB(stmtUpdateEventSelect);
      }

      if (xtEvent)
      {  // found by eventID (already seen and eventID was set)
         if (((xtEvent->TableID() & 0x7F) != Event->TableID()) || (xtEvent->Version() != Event->Version()))
         {  // if provided Event is newer then update tableVersion in DB
#ifdef DBG_EPGHANDLER
            tsyslog("UpdateEvent1: %s %5d E:%02X%02X X:%02X%02X %s-%s (%s) %s~%s", *Event->ChannelID().ToString(), Event->EventID(),
                    Event->TableID(), Event->Version(), xtEvent->TableID(), xtEvent->Version(),
                    *Time2Str(Event->StartTime()), *TimeString(Event->StartTime()+Event->Duration()), ChannelName, Event->Title(), Event->ShortText());
#endif
            cString sqlUpdate = cString::sprintf("UPDATE epg SET tableversion=%u "
                                                 "WHERE src='%s' AND channelid = '%s' AND eventid = %u;",
                                                 (Event->TableID() << 8) | Event->Version(),
                                                 xtEvent->Source(), *Event->ChannelID().ToString(), Event->EventID());
            int sqlrc = ExecDBQuery(*sqlUpdate);
            if (sqlrc == SQLITE_OK) {
               xtEvent->SetTableID(Event->TableID());
               xtEvent->SetVersion(Event->Version());
            }
         }
         // do update Event any time with same content from DB else it will be updated by DVB content
         xtEvent->FillEventFromXTEvent(Event, Flags); // fills Event description and links pictures (does NOT exchange eventID / start time etc.)
         DELETENULL(xtEvent);
         handled = true;
      }
      else
      {  // not found by eventID (because not seen yet or no match found)
         // therefore search for nearest matching event if TableVersion changed
         if (isempty(Event->Aux()) || IsNewVersion(Event))
         {
         sqlite3_stmt *stmt;
         sqlSelect = cString::sprintf("SELECT src, channelid, starttime, eventid, xteventid, tableversion, duration, title, "
            "shorttext, description, season, episode, episodeoverall, origtitle, "
            "country, year, credits, category, review, parentalrating, starrating, pics, ABS(starttime - %lu) AS deltaT FROM epg "
            "WHERE starttime >= %lu AND starttime <= %lu AND channelid='%s' AND src = '%s' "
            "ORDER BY deltaT ASC;",
            Event->StartTime(), Event->StartTime() - TIMERANGE, Event->StartTime() + TIMERANGE, *Event->ChannelID().ToString(), SourceName);

         int SQLrc = sqlite3_prepare_v2(DBHandle, *sqlSelect, -1, &stmt, NULL);
         if (!CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__)) {
            CloseDBConnection(__LINE__);
            return false;
         }

         SQLrc = sqlite3_step(stmt);
         cXMLTVEvent *DBEvent = NULL;
         int bestMatch = 255;
         while (SQLrc == SQLITE_ROW)
         {  // fetch one row from DB
            DBEvent = FillXMLTVEventFromDB(stmt); // fill XMLTV Event from DB row
            if (DBEvent) {
               int newMatch = DBEvent->CompareEvent((cEvent *)Event);
               if (newMatch < bestMatch && newMatch > 0 && (abs(Event->StartTime() - DBEvent->StartTime()) < TIMERANGE)) {
                  bestMatch = newMatch;
                  DELETENULL(xtEvent);
                  xtEvent = DBEvent;
                  tsyslog("new event found: %02X %s E:%5d (%s) E:%s X:%s E:'%s~%s' X:'%s~%s'", newMatch, *Event->ChannelID().ToString(), Event->EventID(), ChannelName,
                          *Time2Str(Event->StartTime()),  *Time2Str(xtEvent->StartTime()), Event->Title(), Event->ShortText(),
                          xtEvent->Title(), xtEvent->ShortText());
               }
               else
                  DELETENULL(DBEvent);
            }
            SQLrc = sqlite3_step(stmt);
         }
         CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);

         sqlite3_finalize(stmt);

         if (xtEvent)
         {  // update eventID in DB, NOTE do NOT check if timestamp is beyond read-ahead time of source !
            cString eventTitle = Event->Title();
            if (strlen(*eventTitle) > 2 && xtEvent->EpisodeOverall() == 0) {
               // if Title contains overall episode but it's not in xtEvent then add it
               char *s = (char *)*eventTitle;
               char *p = s + strlen(s) - 1;
               if (*p-- == ')') {
                  while (strchr("0123456789/( ", *p) && p > s)
                     p--;
                  if (*++p == '(') {
                     *p = '\0';  //TODO do not cut off non-numeric content
                     int episodeOverall = atoi(p+2);
#ifdef DBG_EPISODES2
                     tsyslog("EOverall: %s %d %s~%s", p+1, episodeOverall, Event->Title(),Event->ShortText());
#endif
                     if (episodeOverall > 0)
                        xtEvent->SetEpisodeOverall(episodeOverall);
                  }
               }
            }

            if (xtEvent->EventID() != 0) {
               UnlinkPictures(xtEvent->Pics(), *xtEvent->ChannelID().ToString(), xtEvent->EventID());
            }
#ifdef DBG_EPGHANDLER
            tsyslog("UpdateEvent2: %s %s %5d (%s) E:%02X%02X X:%02X%02X E:%s~%s X:%s~%s", *Time2Str(Event->StartTime()), *Event->ChannelID().ToString(), Event->EventID(), ChannelName,
                    Event->TableID(), Event->Version(), xtEvent->TableID(), xtEvent->Version(),
                    Event->Title(), Event->ShortText(), xtEvent->Title(), xtEvent->ShortText());
#endif
            cString sqlUpdate = cString::sprintf("UPDATE epg SET eventid=%u, tableversion=%u, episodeoverall=%u "
                                                 "WHERE src='%s' AND channelid='%s' AND starttime=%lu;",
                                                 Event->EventID(), (Event->TableID() << 8) | Event->Version(), xtEvent->EpisodeOverall(),
                                                 xtEvent->Source(), *Event->ChannelID().ToString(), xtEvent->StartTime());
            ExecDBQuery(*sqlUpdate);
            // Update DVB Event from xtEvent
            xtEvent->FillEventFromXTEvent(Event, Flags);
            DELETENULL(xtEvent);
            handled = true;
         }
         else
         {
#ifdef DBG_EPGHANDLER
            tsyslog("UpdateEvent3: %s %s %d %02X%02X (%s) \"%s~%s\" no matching event found", *Time2Str(Event->StartTime()),*Event->ChannelID().ToString(), Event->EventID(), Event->TableID(), Event->Version(), ChannelName, Event->Title(), Event->ShortText());
#endif
            // remove Aux additions if any
            cString xmlAuxOthers;
            if (Event->Aux()) {
               char *xmlBegin = strstr((char *)Event->Aux(), xmlTagStart);
               if (xmlBegin) {
                  xmlAuxOthers = cString(Event->Aux(), xmlBegin);
                  char *xmlEnd = strstr((char *)Event->Aux(), xmlTagEnd);
                  if (xmlEnd)
                     xmlAuxOthers.Append(cString(xmlEnd + strlen(xmlTagEnd)));
               }
            }

            if (!isempty(*xmlAuxOthers)) {
               cString xmlTmp = xmlAuxOthers;
               char *xmlBegin = strstr((char *)*xmlTmp, xmlTagTVStart);
               if (xmlBegin) {
                  xmlAuxOthers = cString(*xmlTmp, xmlBegin);
                  char *xmlEnd = strstr((char *)*xmlTmp, xmlTagTVEnd);
                  if (xmlEnd)
                     xmlAuxOthers.Append(cString(xmlEnd + strlen(xmlTagTVEnd)));
               }
            }
            xmlAuxOthers.Append(*cString::sprintf("%s%d%s", xmlTagTVStart, (Event->TableID() << 8) | Event->Version(), xmlTagTVEnd));
            Event->SetAux(*xmlAuxOthers);
         }
       }
      } // END not found by eventID
      sqlite3_clear_bindings(stmtUpdateEventSelect);
      sqlite3_reset(stmtUpdateEventSelect);
   }

   return handled;
}

bool cXMLTVDB::DropOutdated(cSchedule *Schedule, time_t SegmentStart, time_t SegmentEnd, uchar TableID, uchar Version)
{  // called by EPG Handler but only for tuned channels - so does not drop all
   bool handled = false;
   bool eventsShown = false;
   cString channelName;
   tChannelID channelID;
   {
      LOCK_CHANNELS_READ
      channelID = Schedule->ChannelID();
      const cChannel *channel = Channels->GetByChannelID(channelID);
      channelName = channel->Name();
   }

   cString sqlQuery = NULL;
   // first delete unused pictures (later the names are unknown and they are zombies)
   sqlQuery = cString::sprintf("SELECT src, eventid, xteventid, tableversion, starttime, duration, title, shorttext, pics FROM epg "
                               "WHERE channelid='%s' AND starttime >= %lu AND (starttime+duration) < %lu;",
                               *channelID.ToString(), SegmentStart, SegmentEnd);

   sqlite3_stmt *stmt;
   int SQLrc = sqlite3_prepare_v2(DBHandle, *sqlQuery, -1, &stmt, NULL);
   if (!CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__)) {
      return false;
   }

   SQLrc = sqlite3_step(stmt);
   while (SQLrc == SQLITE_ROW) // fetch one row from DB
   {
      cString src        = (char *)sqlite3_column_text(stmt, 0);
      tEventID eventID   = sqlite3_column_int(stmt, 1);
      uint64_t xtEventID = (uint64_t)sqlite3_column_int64(stmt, 2);
      int tableversion   = sqlite3_column_int(stmt, 3);
      time_t starttime   = sqlite3_column_int(stmt, 4);
      int duration       = sqlite3_column_int(stmt, 5);
      cString title      = (char *)sqlite3_column_text(stmt, 6);
      cString shorttext  = (char *)sqlite3_column_text(stmt, 7);
      cString dbPictures = (const char *) sqlite3_column_text(stmt, 8);

      if (eventID > 0 && tableversion != TABLEVERSION_UNSEEN) {
         uchar tableID = (tableversion >> 8) & 0x00FF;
         uchar tableID2 = tableID & 0x7F;
         uint version = tableversion & 0x00FF;
         if ((tableID2 > 0x4E || TableID == 0x4E) && (tableID2 != TableID || version != Version)) {
            if (!eventsShown) {
               tsyslog("DropOutdated2 Segment Window: %s-%s(%lu) %02X%02X %s (%s)", *Time2Str(SegmentStart), *TimeString(SegmentEnd), SegmentEnd, TableID, Version, *channelID.ToString(), *channelName);
               cEvent *event = (cEvent *)Schedule->GetEventAround(SegmentStart);
               while (event != NULL && event->StartTime() < SegmentEnd) {
                  tsyslog("DropOutdated2: %5d %s-%s E:%02X%02X P:%02X%02X (%s) %s~%s", event->EventID(), *Time2Str(event->StartTime()), *TimeString(event->StartTime()+event->Duration()),
                        event->TableID(), event->Version(), TableID, Version, *channelName, event->Title(), event->ShortText());
                  event = (cEvent *)event->Next();
               }
               eventsShown = true;
            }
            tsyslog("DropOutdated3: %5d DB:%04X P:%02X%02X %s-%s#%lu (%s) %s~%s %lu", eventID, tableversion, TableID, Version, *Time2Str(starttime), *TimeString(starttime+duration), starttime, *channelName, *title, *shorttext, xtEventID);
            cString sqlQuery = cString::sprintf("UPDATE epg SET tableversion=%u WHERE src='%s' AND channelid = '%s' AND eventid = %u;",
                                                DELETE_FLAG | tableversion,  // do NOT update tableversion
                                                *src, *channelID.ToString(), eventID);
            ExecDBQuery(*sqlQuery);
            UnlinkPictures(*dbPictures, *channelID.ToString(), eventID);
         }
      }

      SQLrc = sqlite3_step(stmt);
   }
   CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);
   sqlite3_finalize(stmt);

   return handled;
}

bool cXMLTVDB::DropEventList(int *LinksDeleted, int *EventsDeleted, const char * WhereClause)
{
   cXMLTVStringList picList;

   if (isempty(WhereClause)) {
      esyslog("DropEventList: empty where clause");
      return false;
   }
   cString sqlSelect = cString::sprintf("SELECT src, channelid, starttime, eventid, xteventid, pics FROM epg %s;", WhereClause);

   sqlite3_stmt *stmt;
   int SQLrc = sqlite3_prepare_v2(DBHandle, *sqlSelect, -1, &stmt, NULL);
   if (!CheckSQLiteSuccess(SQLrc, __LINE__,__FUNCTION__)) {
      CloseDBConnection(__LINE__);
      return false;
   }

   SQLrc = sqlite3_step(stmt);
   while (SQLrc == SQLITE_ROW) {
      cString src, channelID;
      time_t startTime = 0;
      tEventID eventID = 0;
      uint64_t xtEventID = 0;

      src          = (const char *)sqlite3_column_text(stmt, 0);
      channelID    = (const char *)sqlite3_column_text(stmt, 1);
      startTime    = sqlite3_column_int(stmt, 2);
      eventID      = sqlite3_column_int(stmt, 3);
      xtEventID    = sqlite3_column_int64(stmt, 4);
      cString pics = (const char *)sqlite3_column_text(stmt, 5);
      picList.SetStringList(pics);
      if (eventID == 0)
         tsyslog("DropEvent: WARNING Event %s %d %lu %s Pics %d: %s", *channelID, eventID, xtEventID, *Time2Str(startTime),picList.Size(), *pics);
#ifdef DBG_DROPEVENTS
      tsyslog("DropEvent: Event %s %d %s Pics %d: %s", *channelID, eventID, *Time2Str(startTime), picList.Size(), *pics);
#endif

      // delete Links to Event
      *LinksDeleted += UnlinkPictures(&picList, *channelID, eventID);

      // remember referenced pictures
      for (int i = 0; i < picList.Size(); i++)
         orphanedPictures.AppendStrings(*src, picList.At(i));

      // delete event entry
      cString sqlDelete = cString::sprintf("DELETE FROM epg WHERE src='%s' AND channelid='%s' AND starttime=%lu;",
                                           *src, *channelID, startTime);
#ifdef DBG_DROPEVENTS
      tsyslog("DropEvent: deleting from DB: %s %s_%d %lu", *src, *channelID, eventID, xtEventID);
#endif
      ExecDBQuery(*sqlDelete);
      *EventsDeleted += sqlite3_changes(DBHandle);
      SQLrc = sqlite3_step(stmt);
   }
   bool success = CheckSQLiteSuccess(SQLrc, __LINE__, "DropEventsFromDB()");

   sqlite3_finalize(stmt);

   return success;
}

void cXMLTVDB::DropOutdatedEvents(time_t EndTime)
{  // drop all events in the time range in DB and unlink pictures
   cXMLTVStringList picList;
   int links_deleted = 0, pics_deleted = 0, events_deleted = 0;

   if (OpenDBConnection()) {
      Transaction_Begin();

      DropEventList(&links_deleted, &events_deleted, *cString::sprintf("WHERE (starttime+duration) < %lu", EndTime));

      Transaction_End();
      ExecDBQuery("VACUUM;");

      CloseDBConnection();

      pics_deleted = DeletePictures();
      isyslog("DropOutdatedEvents deleted %d links, %d pics and %d events", links_deleted, pics_deleted, events_deleted);
   }

   return;
}


bool cXMLTVDB::CheckConsistency(bool Fix, cXMLTVStringList *CheckResult)
{  // check consistency by verifying each picture has a reference in the DB
   // and each link points to a picture
   bool failures = false;
   bool success = true;
   CheckResult->Clear();
   cString msg = cString::sprintf("CheckConsistency: result of last run at %s", *TimeToString(time(NULL)));
   CheckResult->Append(strdup(*msg));
   if ((success = OpenDBConnection()))
   {  // check for all pics if they are referenced in the DB
      // build list of all pictures per source referenced in DB 
      // loop over image dir of source and check if picture is in list, otherwise report it or delete it if fix==true
      for (cEPGSource *source = XMLTVConfig.EPGSources()->First(); source != NULL && success; source = XMLTVConfig.EPGSources()->Next(source)) {
         cString sqlSelect = cString::sprintf("SELECT pics FROM epg WHERE src='%s';", source->SourceName());
         cStringList pictureList;

         sqlite3_stmt *stmt;
         int SQLrc = sqlite3_prepare_v2(DBHandle, *sqlSelect, -1, &stmt, NULL);
         if ((success = CheckSQLiteSuccess(SQLrc, __LINE__, "CheckConsistency()")))
         {  // get one row/pic line
            cXMLTVStringList picList;
            SQLrc = sqlite3_step(stmt);
            while (SQLrc == SQLITE_ROW)
            {
               cString pics = (const char *)sqlite3_column_text(stmt, 0);
               picList.SetStringList(pics);
               for (int i = 0; i < picList.Size(); i++) {
                  if (pictureList.Find(picList[i]) == -1)
                     pictureList.Append(strdup(picList[i]));
               }
               SQLrc = sqlite3_step(stmt);
            }
            success = CheckSQLiteSuccess(SQLrc, __LINE__, "CheckConsistency()");
            pictureList.Sort();

            msg = cString::sprintf("CheckConsistency: %s %d pics referenced in DB", source->SourceName(), pictureList.Size());
            isyslog(*msg);
            CheckResult->Append(strdup(*msg));
         }
         sqlite3_finalize(stmt);

         if (success) {
            // loop over all images in source image dir
            int notfound = 0;
            struct stat statbuf;
            cString dirName = cString::sprintf("%s/%s-img", XMLTVConfig.EPGSourcesDir(), source->SourceName());
            cReadDir imageDir(*dirName);
            if (imageDir.Ok()) {
               struct dirent *e;
               while ((e = imageDir.Next()) != NULL) {
                  if (pictureList.Find(e->d_name) == -1) {
                     notfound++;
                     tsyslog("CheckConsistency: %s not referenced%s", e->d_name, Fix ? ", deleting" : "");
                     if (Fix) {
                        unlink(*AddDirectory(dirName, e->d_name));
                     }
                  }
               }
            }
            msg = cString::sprintf("CheckConsistency: %s %d unused pics %s", source->SourceName(), notfound, Fix ? "deleted" : "found");
            isyslog(*msg);
            CheckResult->Append(strdup(*msg));
            failures |= notfound > 0;
         }
      } //end source

      if (success)
      {  // check if links point to a picture
         cReadDir linkDir(XMLTVConfig.ImageDir());
         if (linkDir.Ok()) {
            int deadLinks = 0;
            int linkNotInDB = 0;
            struct dirent *e;
            while ((e = linkDir.Next()) != NULL) {
               cString lnk = AddDirectory(XMLTVConfig.ImageDir(), e->d_name);
               struct stat statbuf;
               if (stat(*lnk, &statbuf) == -1 && errno == ENOENT) {
                  if (deadLinks <= 50)
                     tsyslog("dead link: %s", *lnk);
                  else if (deadLinks == 51)
                     tsyslog("more than 50 dead links - skipping output");
                  deadLinks++;
                  if (Fix)
                     unlink(*lnk);
               }

               // check if channelID & EventID of Link still exist in DB
               char channelID[30];
               uint eventid;
               uint no;
               char ext[4];
               int used;
               if ((used = sscanf(e->d_name, "%30[SEWTCI0123456789.-]_%d_%d.%3s", channelID, &eventid, &no, ext)) == 4) {
                  cString sqlSelect = cString::sprintf("SELECT src, starttime, pics FROM epg WHERE channelid='%s' AND eventid=%u;", channelID, eventid);
                  sqlite3_stmt *stmt2;
                  int SQLrc = sqlite3_prepare_v2(DBHandle, *sqlSelect, -1, &stmt2, NULL);
                  if ((success = CheckSQLiteSuccess(SQLrc, __LINE__, "CheckConsistency()")))
                  {  // get one row/pic line
                     SQLrc = sqlite3_step(stmt2);
                     int entries = 0;
                     while (SQLrc == SQLITE_ROW)
                     {
                        entries++;
                        SQLrc = sqlite3_step(stmt2);
                     }
                     success = CheckSQLiteSuccess(SQLrc, __LINE__, "CheckConsistency()");
                     if (success && entries == 0) {
                        linkNotInDB++;
                        tsyslog("CheckConsistency: linked event not found in DB: %s %d %s", channelID, eventid, e->d_name);
                        if (Fix)
                           unlink(*AddDirectory(XMLTVConfig.ImageDir(), e->d_name));
                     }
                  }
                  sqlite3_finalize(stmt2);
               }
            }
            if (success) {
               msg = cString::sprintf("CheckConsistency: %d links not in DB %s", linkNotInDB, Fix ? "deleted" : "found");
               isyslog(*msg);
               CheckResult->Append(strdup(*msg));
               failures |= linkNotInDB > 0;

               // if a check is executed and then immediately a check-fix run then the number of
               // deleted links is higher than the previously reported number of found dead links
               // because delete first deletes pictures which likely produces orphaned links.
               // So it's ok if there are more deleted than previously found links :-)
               msg = cString::sprintf("CheckConsistency: %d dead links %s", deadLinks, Fix ? "deleted" : "found");
               isyslog(*msg);
               CheckResult->Append(strdup(*msg));
               failures |= deadLinks > 0;
            }
            else {
               msg = cString::sprintf("CheckConsistency: DB Error - DB might be busy. Try again later");
               isyslog(*msg);
               CheckResult->Append(strdup(*msg));
               failures = true;
            }
         }
      }
      CloseDBConnection();
   }
   else {
      msg = cString::sprintf("CheckConsistency: DB Error - DB might be busy. Try again later");
      isyslog(*msg);
      CheckResult->Append(strdup(*msg));
      failures = true;
   }

   return failures | !success;
}

// -------------------------------------------------------------
cHouseKeeping::cHouseKeeping(): cThread("xmltv4vdr Housekeeping", true)
{
   lastCheckResult.Append(strdup("No check has run so far"));
}

bool cHouseKeeping::StartHousekeeping(eHousekeepingType Type)
{
   type = Type;
   bool started = false;
   Lock();
   if (!Active() && !XMLTVConfig.EPGSources()->Active()) {
      started = cThread::Start();
   }
   Unlock();

   return started;
}

void cHouseKeeping::Action()
{
   cXMLTVDB *xmlTVDB = new cXMLTVDB();

   if (type == HKT_DELETEEXPIREDEVENTS) {
      time_t endTime = EVENT_LINGERTIME;
      isyslog("Starting Housekeeping, cleaning up events before %s", *DayDateTime(endTime));
      xmlTVDB->DropOutdatedEvents(endTime);
      tsyslog("Housekeeping finished");
   }
   else {
      isyslog("Starting Consistency Check%s", type == HKT_CHECKANDFIXCONSISTENCY ? " with fix" : "");
      bool failures = xmlTVDB->CheckConsistency(type == HKT_CHECKANDFIXCONSISTENCY, &lastCheckResult);
      tsyslog("Consistency Check finished %s", failures ? "with failures":"successfully");
   }

   delete xmlTVDB;
}
