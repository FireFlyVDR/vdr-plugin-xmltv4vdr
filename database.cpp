/*
 * database.h: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <string>
#include <netdb.h>
#include <vdr/channels.h>

#include "debug.h"
#include "database.h"


// ================================ cMapList ==============================================
cMapList::~cMapList()
{
  Clear();
}

int cMapList::Find(const char *A, const char *B) const
{
   for (int i = 0; i < Size(); i++) {
      if (!strcmp(A, At(i)->A()) && !strcmp(B, At(i)->B()))
         return i;
      }
  return -1;
}

void cMapList::Clear(void)
{
   for (int i = 0; i < Size(); i++)
      delete(At(i));
   cVector<cMapObject *>::Clear();
}

// ================================ cXMLTVSQLite ==============================================
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

const char *stripend(const char *s, const char *p)
{
   char *se = (char *)s + strlen(s) - 1;
   const char *pe = p + strlen(p) - 1;
   while (pe >= p) {
      if (*pe-- != *se-- || (se < s && pe >= p))
         return NULL;
   }
   *++se = 0;
   return s;
}

// ================================ cEpLists ==============================================
class cEpLists
{
private:
   cString host;
   int port;
   int sock;
   cFile file;
   int length;
   char *inBuffer;

   int connect();
   int ReadLine(cStringList *Response, bool Log, int timeoutMs);

public:
   cEpLists(const char *Address, const int Port);
   ~cEpLists();

   bool Open();
   void Close();
   bool Send(const char *Command);
   int Receive(cStringList *Response, bool Log = false, int timeoutMs = 20 * 1000);
};


cEpLists::cEpLists(const char *Host, const int Port)
{
   host = Host;
   port = Port;
   sock = -1;
   length = BUFSIZ;
   inBuffer = MALLOC(char, length);
}

cEpLists::~cEpLists(void)
{
   Close();
   free(inBuffer);
}

bool cEpLists::Open()
{
   in_addr_t serverAddr;
   char addrstr[INET_ADDRSTRLEN];
   struct addrinfo hints, *serverInfo;

   memset (&hints, 0, sizeof(hints));
   hints.ai_family = PF_INET; // only IPv4 for now, was PF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags |= AI_CANONNAME;

   int errorcode = getaddrinfo (*host, NULL, &hints, &serverInfo);
   if (errorcode || serverInfo->ai_family != PF_INET) {
      LOG_ERROR;
      return false;
   }

   struct sockaddr_in * p = (struct sockaddr_in *)(serverInfo->ai_addr);
   serverAddr = p->sin_addr.s_addr;
   if (inet_ntop(p->sin_family, &p->sin_addr, addrstr, INET_ADDRSTRLEN) == NULL)
      LOG_ERROR;
#ifdef DBG_EPISODES2
   else
      isyslog("EPL10: %s IPv%d address: %s (%s)\n", *host, serverInfo->ai_family == PF_INET6 ? 6 : 4, addrstr, serverInfo->ai_canonname);
#endif
   freeaddrinfo(serverInfo);

   if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
      LOG_ERROR;
      return false;
   }

   sockaddr_in Addr;
   memset(&Addr, 0, sizeof(Addr));
   Addr.sin_family = AF_INET;
   Addr.sin_port = htons(port);
   Addr.sin_addr.s_addr = serverAddr;

   isyslog("Episodes DB: connecting to eplists server %s (%s:%d)", *host, addrstr, port);
   if (::connect(sock, (sockaddr *)&Addr, sizeof(Addr)) < 0 && errno != EINPROGRESS) {
      close(sock);
      LOG_ERROR;
      return false;
   }

   // make it non-blocking:
   int Flags = ::fcntl(sock, F_GETFL, 0);
   if (Flags < 0) {
      LOG_ERROR;
      return false;
   }
   if (::fcntl(sock, F_SETFL, Flags |= O_NONBLOCK) < 0) {
      LOG_ERROR;
      return false;
   }

   if (!file.Open(sock)) {
      LOG_ERROR;
      return false;
   }
   isyslog("Episodes DB: connected to eplists server %s (%s:%d)", *host, addrstr, port);

   cStringList greetings;
   int rc = Receive(&greetings);
   if (rc != 220) {
      esyslog("EPL11: Did not receive greeting from %s, aborting", *host);
      return false;
   }

   greetings.Clear();
   Send("CHARSET utf-8");
   if ((rc = Receive(&greetings)) != 225) {
      esyslog("EPL12: Could not set charset, aborting");
      return false;
   }

   return true;
}

void cEpLists::Close()
{
   if (file.IsOpen()) {
      file.Close();
   }

   if (sock >= 0) {
      close(sock);
      sock = -1;
      isyslog("Episodes DB: disconnected from eplists server %s", *host);
   }
}

bool cEpLists::Send(const char *Command)
{
   if (safe_write(file, Command, strlen(Command)) < 0 || safe_write(file, "\n", 1) < 0) {
     LOG_ERROR;
     return false;
   }

   return true;
}

int cEpLists::Receive(cStringList *Responses, bool Log, int timeoutMs)
{
   int rc = 0;
   if (!Responses || !file.IsOpen())
      return -1;

   Responses->Clear();
   while ((rc = ReadLine(Responses, Log, timeoutMs)) > 0) {
#ifdef DBG_EPISODES2
      isyslog("EPL31 % 4d Response: %2d lines", rc, Responses->Size());
#endif
   }

#ifdef DBG_EPISODES2
   isyslog("EPL32 %03d Response: %2d lines", abs(rc), Responses->Size());
#endif
   return (abs(rc));
}


#define SVDRPResonseTimeout 5000 // ms
int cEpLists::ReadLine(cStringList *Response, bool Log, int timeoutMs)
{
   int numChars = 0;
   int rc = 0;
   cTimeMs Timeout(SVDRPResonseTimeout);
   for (;;) {
      if (!file.Ready(false)) {
         if (Timeout.TimedOut()) {
            isyslog("EPL43: timeout while waiting for response from '%s'", *host);
            return -2;
         }
      }
      else {
         unsigned char c;
         int r = safe_read(file, &c, 1);
         if (r > 0) {
            if (c == '\n' || c == 0x00) {
               // strip trailing whitespace:
               while (numChars > 0 && strchr(" \r\n", inBuffer[numChars - 1]))
                  inBuffer[--numChars] = 0;
               inBuffer[numChars] = 0;
               Response->Append(strdup(inBuffer+4));
               if (Log) isyslog(inBuffer);
               rc = atoi(inBuffer);
               if (numChars >= 4 && inBuffer[3] != '-') { // no more lines will follow
                  rc = -rc;
               }
               break;
            }
            else if ((c <= 0x1F || c == 0x7F) && c != 0x09) {} // ignore
            else {
               if (numChars >= length - 1) {
                  int NewLength = length + BUFSIZ;
                  if (char *NewBuffer = (char *)realloc(inBuffer, NewLength)) {
                     length = NewLength;
                     inBuffer = NewBuffer;
                  }
                  else {
                     esyslog("EPL44: ERROR: out of memory");
                     Close();
                     break;
                  }
               }
               inBuffer[numChars++] = c;
               inBuffer[numChars] = 0;
            }
            Timeout.Set(SVDRPResonseTimeout);
         }
         else if (r <= 0) {
            esyslog("EPL47: lost connection %d to remote server '%s'", r, *host);
            return -3;
         }
      }
   }

#ifdef DBG_EPISODES2
   isyslog("EPL49: %03d Response: %2d lines", rc, Response->Size());
#endif
   return rc;
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
         XMLTVConfig.SetLastEpisodesUpdate(lastUpdate);
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
      "CREATE INDEX IF NOT EXISTS series_ndx1 ON series (id); "
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

bool cEpisodesDB::UpdateDB()
{
   bool success = false;
   // use INET if episodes server is set
   if (!isempty(XMLTVConfig.EpisodesServer()))
      success = UpdateDBFromINet();
   else
      success = UpdateDBFromFiles();
   if (success)
   {
      Analyze("series");
      Analyze("episodes");
   }
   return success;
}

bool cEpisodesDB::UpdateDBFromINet()
{
   bool success = true;
   cEpLists *eplists = new cEpLists(XMLTVConfig.EpisodesServer(), XMLTVConfig.EpisodesServerPort());
   if (!(success = eplists->Open()))
   {
      esyslog("Episodes DB: open failed, aborting episodes update");
   }
   else
   {
      //define time range of updates
      cString eplistQuery;
      time_t currentTime = time(NULL);
      uint updateAgo = 0;
      if (lastUpdate) {
         updateAgo = currentTime - lastUpdate + 90*60; // for summer/winter time change
         isyslog("Episodes DB: importing episodes newer than %s", *TimeToString(lastUpdate - 90*60));
         eplistQuery = cString::sprintf("TGET newer than %u minutes", updateAgo/60);
      }
      else {
         isyslog("Episodes DB: No timestamp found - importing ALL episodes from episodes server");
         eplistQuery = "GET all";
      }

      if (eplists->Send(*eplistQuery))
      {
         int reply = 0;
         bool abort = false;
         bool isLink = false;
         cStringList response;
         cString seriesName, linkedTo;
         int numSeries = 0, numLinks = 0;
         cMapList links;
         Transaction_Begin();
         while(!abort && (reply = eplists->Receive(&response)) != 217)
         {
            if (reply == 218) {
               if (response.Size() != 2)
               {
                  esyslog("EPL61: FileInfo protocol violation, aborting");
                  abort = true;
               }
               else
               {
                  seriesName = StringNormalize(response[0]);
                  isLink   = response[1] && strcmp(response[1], "not a link");
                  linkedTo = StringNormalize(response[1]);
                  numSeries++;
                  if (isLink && !isempty(*seriesName) && (-1 == links.Find(*seriesName, *linkedTo))) {
                     links.AppendStrings(*seriesName, *linkedTo);
                     numLinks++;
                  }
               }
            }
            else if (reply == 216)
            {
               if (isempty(*seriesName))
                  abort = true;
               else {
                  if (!isLink) {
                     uint seriesId = 0;
                     int result = ExecDBQueryInt(*cString::sprintf("SELECT id FROM series WHERE name = %s;", *SQLescape(*seriesName)), seriesId);
                     if (result == SQLqryEmpty) {
                        ExecDBQueryInt("SELECT MAX(id) from series;", seriesId);
                        ExecDBQuery(*cString::sprintf("INSERT INTO series VALUES(%d, %s)", ++seriesId, *SQLescape(*seriesName)));
                     }

                     uint season, episode, episodeOverall;
                     char *line;
                     char episodeName[201];
                     char extraCols[201];
                     uint cnt = 0;
                     uint added = 0;
                     for (int i = 0; i < response.Size()-1; i++)
                     {
                        line = response[i];
                        if (line[0] == '#') continue;
                        cnt++;
                        if (sscanf(line, "%d\t%d\t%d\t%200[^\t\n]\t%200[^]\n]", &season, &episode, &episodeOverall, episodeName, extraCols) >= 4)
                        {
                           cString nrmEpisodeName = StringNormalize(episodeName);
                           if (!isempty(*nrmEpisodeName)) {
                              if (!strcmp(*nrmEpisodeName, "nn") || !strcmp(*nrmEpisodeName, "ka"))
                                 continue;
                              result = ExecDBQuery(*cString::sprintf("INSERT OR REPLACE INTO episodes VALUES(%d, %s, %d, %d, %d)", seriesId, *SQLescape(*nrmEpisodeName), season, episode, episodeOverall));
                              added++;
                           }
                        }
                        else
                           isyslog("EPL62: not added: '%s'", line);
                     }
#ifdef DBG_EPISODES2
                     tsyslog("Episodes DB: added %3d of %3d lines to series '%s'", added, cnt, *seriesName);
#endif
                  }
                  seriesName = NULL;
                  linkedTo   = NULL;
               }
            }
            else
            {
               esyslog("EPL66: unexpected reply: %d %s", reply, response[0]);
               abort = true;
            }
         }

         // import links
         for (int i = 0; i < links.Size(); i++)
         {
            uint seriesId = 0;
            int result = ExecDBQueryInt(*cString::sprintf("SELECT id FROM series WHERE name = %s;", *SQLescape(links[i]->B())), seriesId);
            if (result == SQLqryOne) {
               ExecDBQuery(*cString::sprintf("INSERT OR REPLACE INTO series VALUES(%d, %s)", seriesId, *SQLescape(links[i]->A())));
#ifdef DBG_EPISODES2
               tsyslog("Episodes DB: added alias '%s' (%d) for series '%s'", *SQLescape(links[i]->B()), seriesId, *SQLescape(links[i]->A()));
#endif
            }
         }

         if (!abort) {
            ExecDBQuery(cString::sprintf("PRAGMA user_version = %lu", currentTime));
            XMLTVConfig.SetLastEpisodesUpdate(currentTime);
            isyslog("Episodes DB: updated %u series and %u links", numSeries, numLinks);
         }

         Transaction_End(!abort);
      }
      else {
         esyslog("EPL69: Error sending query");
      }


      eplists->Send("QUIT");
      cStringList response;
      int rc = eplists->Receive(&response);
   }
   eplists->Close();

   delete eplists;
   return success;
}

bool cEpisodesDB::UpdateDBFromFiles()
{
   bool success = false;

   if (!isempty(XMLTVConfig.EpisodesDir())) {
      time_t newestFileTime = 0;
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
               epTitle = StringNormalize(*epTitle);
               uint seriesId = 0;
               int result = ExecDBQueryInt(*cString::sprintf("SELECT id FROM series WHERE name = %s;", *SQLescape(*epTitle)), seriesId);
               if (result == SQLqryEmpty) {
                  ExecDBQueryInt("SELECT MAX(id) from series;", seriesId);
                  seriesId += 1;
                  ExecDBQuery(*cString::sprintf("INSERT INTO series VALUES(%d, %s)", seriesId, *SQLescape(*epTitle)));
               }
#ifdef DBG_EPISODES2
               isyslog("EPL80: seriesID: %4d: %s (%s, %s)", seriesId, *epTitle, *epFilename, *TimeToString(st.st_mtime));
#endif
               FILE *epFile = fopen(*epFilename, "r");
               if (!epFile) {
                  esyslog("EPL81: opening %s failed", *epFilename);
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
                           if (!strcmp(*nrmEpisodeName, "nn") || !strcmp(*nrmEpisodeName, "ka"))
                              continue;
                           result = ExecDBQuery(*cString::sprintf("INSERT OR REPLACE INTO episodes VALUES(%d, %s, %d, %d, %d)", seriesId, *SQLescape(*nrmEpisodeName), season, episode, episodeOverall));
                           cnt++;
                        }
                     }
#ifdef DBG_EPISODES2
                     else
                        isyslog("EPL83: line not recognized: %4d:  '%s'", seriesId, line);
#endif
                  }
                  Transaction_End();
                  if (newestFileTime < st.st_mtime)
                     newestFileTime = st.st_mtime;
#ifdef DBG_EPISODES
                  tsyslog("EEPL84: %4d %3d %s %s", seriesId, cnt, *DayDateTime(newestFileTime), *epTitle);
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
                  epTarget = StringNormalize(*epTarget);
                  cString epTitle = cString(e->d_name, strstr(e->d_name, ".episodes"));
                  stripend(*epTitle, ".en");
                  stripend(*epTitle, ".de");
                  epTitle = StringNormalize(*epTitle);
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
      isyslog("Episodes DB: imported files until %s", *TimeToString(lastUpdate));

      success = true;
   }
   return success;
}

bool cEpisodesDB::QueryEpisode(cXMLTVEvent *xtEvent)
{
   bool found = false;
   if (stmtQueryEpisodes && xtEvent && !isempty(xtEvent->Title()))
   {
      cString eventTitle = StringNormalize(xtEvent->Title());
      cString eventSplitTitle, eventSplitShortText;
      cString eventShortText = xtEvent->ShortText();
      if (isempty(*eventShortText) && !isempty(xtEvent->Description()) && strlen(xtEvent->Description()) < 100)
         eventShortText = xtEvent->Description();

      if (!isempty(*eventShortText))
      {  // try complete shorttext
         uint l;
         eventShortText = StringNormalize(*eventShortText);
         for (uint i=0; l = Utf8CharLen((*eventShortText)+i), i<strlen(*eventShortText); i += l) {
            if (l == 1) {
               char *symbol = (char*)(*eventShortText)+i;
               *symbol = tolower(*symbol);
            }
         }

         sqlite3_bind_text(stmtQueryEpisodes, 1, *eventTitle, -1, SQLITE_STATIC);
         sqlite3_bind_text(stmtQueryEpisodes, 2, *eventShortText, -1, SQLITE_STATIC);

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

      char *p = (char *)strchrn(xtEvent->Title(), ':', 1);
      if (!found && p && !isempty(skipspace(p+1)))
      {  // try with title split at colon
         eventSplitTitle = cString(xtEvent->Title(), p);
         eventSplitTitle = StringNormalize(*eventSplitTitle);
         eventSplitShortText = StringNormalize(skipspace(p+1));
         uint l;
         for (uint i=0; l = Utf8CharLen((*eventSplitShortText)+i), i<strlen(*eventSplitShortText); i += l)
            if (l == 1) {
               char *symbol = (char*)(*eventSplitShortText)+i;
               *symbol = tolower(*symbol);
            }
         sqlite3_bind_text(stmtQueryEpisodes, 1, *eventSplitTitle, -1, SQLITE_STATIC);
         sqlite3_bind_text(stmtQueryEpisodes, 2, *eventSplitShortText, -1, SQLITE_STATIC);

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

// ================================ cXMLTVDB ==============================================
#define TABLEVERSION_UNSEEN 0x7FFF
#define DELETE_FLAG         0x8000
#define XMLTVDB_SCHEMA_VERSION 23
#define TIMERANGE (2*60*59)  // nearly 2 hrs

cXMLTVDB::~cXMLTVDB()
{
   if (orphanedPictures.Size() > 0)
      esyslog("~cXMLTVDB: %d orphaned Pictures found", orphanedPictures.Size());
   //DeleteOrphanedPictures();
}

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
         if (result == 0 || (result == 1 && version != XMLTVDB_SCHEMA_VERSION))
         {  // schema was updated
            esyslog("xmltv4vdr SQLite schema has wrong version %d or is missing, expected %d - re-creating DB", version, XMLTVDB_SCHEMA_VERSION);
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
      "CREATE TABLE epg (channelid TEXT     NOT NULL, " //  0 PK
                        "starttime INT      NOT NULL, " //  1 PK
                        "eventid INT, "                 //  2 // EIT ID is 16 Bit but VDR stores 32Bit, so self-created IDs for new events can be > 0xFFFF
                        "tableversion INT, "            //  3
                        "xteventid TEXT, "              //  4
                        "src TEXT, "                    //  5
                        "title TEXT, "                  //  6
                        "shorttext TEXT, "              //  7
                        "description TEXT, "            //  8
                        "duration INT, "                //  9
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
                        "PRIMARY KEY(channelid, starttime)); " // avoid duplicate events
      "CREATE INDEX IF NOT EXISTS ndx1 ON epg (channelid, eventid); "
      "CREATE INDEX IF NOT EXISTS ndx2 ON epg (src, pics); "
      "PRAGMA user_version = %d; ", XMLTVDB_SCHEMA_VERSION);

   int SQLrc = ExecDBQuery(*sqlCreate);
   if (SQLrc != SQLITE_OK)
      esyslog("ERROR creating DB - SQL RC %d", SQLrc);

   XMLTVConfig.EPGSources()->ResetLastEventStarttimes();

   return SQLrc == SQLITE_OK;
}

int cXMLTVDB::Analyze(void)
{
   return Analyze("epg");
}

cString cXMLTVDB::ChannelListToString(const cChannelIDList &ChannelIDList)
{
   cString channelList = "";
   for (int i = 0; i < ChannelIDList.Size(); i++) {
      if (i)
         channelList.Append(", ");
      channelList.Append(SQLescape(ChannelIDList.At(i)->GetChannelIDString()));
   }
   return cString(channelList);
}

bool cXMLTVDB::MarkEventsOutdated(const cChannelIDList &ChannelIDList)
{
   cString channelList = ChannelListToString(ChannelIDList);
   bool success = ExecDBQuery(*cString::sprintf("UPDATE epg SET tableversion = tableversion | %d WHERE channelid IN (%s);", DELETE_FLAG, *channelList)) == SQLITE_OK;

   return success;
}


bool cXMLTVDB::DropOutdatedEvents(const cChannelIDList &ChannelIDList,  time_t LastEventStarttime)
{
   int linksDeleted = 0, picsDeleted = 0, eventsDeleted = 0;
   cString channelList = ChannelListToString(ChannelIDList);

   bool success = true;
   success = DropEventList(&linksDeleted, &eventsDeleted, *cString::sprintf("WHERE channelid IN (%s) AND tableversion >= %d", *channelList, DELETE_FLAG));
#ifdef DBG_DROPEVENTS2
   cString sqlQuery = cString::sprintf("SELECT count(*) FROM epg WHERE channelid IN (%s) AND tableversion >= %d", *channelList, DELETE_FLAG);
   uint cnt = 0;
   ExecDBQueryInt(*sqlQuery, cnt);
   isyslog("DropOutdatedEvents skipped %d: WHERE channelid IN (%s) AND tableversion >= %d", cnt, *channelList, DELETE_FLAG);
#endif

   return success;
}


bool cXMLTVDB::ImportXMLTVEventPrepare(void)
{
   cString sqlImportXMLTVEventSelect = "SELECT eventid, src, pics FROM epg WHERE channelid=?1 AND starttime = ?2;";

   stmtImportXMLTVEventSelect = NULL;
   int SQLrc = sqlite3_prepare_v2(DBHandle, *sqlImportXMLTVEventSelect, -1, &stmtImportXMLTVEventSelect, NULL);
   bool success = CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);

   cString sqlImportXMLTVEventReplace = cString::sprintf("REPLACE INTO epg (channelid, starttime, eventid, tableversion, xteventid, "
                                     "src, title, shorttext, description, duration, "
                                     "season, episode, episodeoverall, origtitle, country, "
                                     "year, credits, category, review, parentalrating, "
                                     "starrating, pics) VALUES "
                                     "(?11, ?12, ?13, %u, ?14, "
                                     "?21, ?22, ?23, ?24, ?25, "
                                     "?31, ?32, ?33, ?34, ?35, "
                                     "?41, ?42, ?43, ?44, ?45, "
                                     "?51, ?52);", TABLEVERSION_UNSEEN);
   stmtImportXMLTVEventReplace = NULL;
   SQLrc = sqlite3_prepare_v2(DBHandle, *sqlImportXMLTVEventReplace, -1, &stmtImportXMLTVEventReplace, NULL);
   success = success && CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);

   return success;
}

bool cXMLTVDB::ImportXMLTVEventFinalize()
{
   int SQLrc = sqlite3_finalize(stmtImportXMLTVEventSelect);
   bool success = CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);

   SQLrc = sqlite3_finalize(stmtImportXMLTVEventReplace);
   success = success && CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);

   return success;
}

bool cXMLTVDB::ImportXMLTVEvent(cXMLTVEvent *xtEvent, const cChannelIDList &ChannelIDList)
{  /// insert xtEvent in DB for all channels in ChannelList

#define DELTATIME (120*60)
   //bool success = ChannelIDList != NULL && ChannelIDList.Size() > 0;
   bool success = true;
   int cl = 0;
   while (success && cl < ChannelIDList.Size())
   {
      cString channelID = ChannelIDList.At(cl)->GetChannelIDString();
      sqlite3_bind_text(stmtImportXMLTVEventSelect, 1, *channelID, -1, SQLITE_STATIC);
      sqlite3_bind_int (stmtImportXMLTVEventSelect, 2, xtEvent->StartTime());

      tEventID dbEventID = 0;
      cString dbPictures, dbSourceName;

      int SQLrc1 = sqlite3_step(stmtImportXMLTVEventSelect);
      CheckSQLiteSuccess(SQLrc1, __LINE__, __FUNCTION__);

      if (SQLrc1 == SQLITE_ROW)
      {  // update
         dbEventID  = sqlite3_column_int(stmtImportXMLTVEventSelect, 0);
         dbSourceName = (const char *) sqlite3_column_text(stmtImportXMLTVEventSelect, 1);
         dbPictures = (const char *) sqlite3_column_text(stmtImportXMLTVEventSelect, 2);

         if ((!isempty(*dbPictures) && xtEvent->Pics()->Size() == 0) ||
             (!isempty(*dbPictures) && xtEvent->Pics()->Size() > 0 && strcmp(*dbPictures, xtEvent->Pics()->ToString())))
         {
            cXMLTVStringList dbPictureList;
            dbPictureList.SetStringList(dbPictures);
            for (int p = 0; p < dbPictureList.Size(); p++)
               AddOrphanedPicture(dbSourceName, dbPictureList.At(p));

            UnlinkPictures(&dbPictureList, *channelID, dbEventID);  // required to avoid links to wrong pictures
         }
      }

      // entry does not yet exist
      sqlite3_bind_text (stmtImportXMLTVEventReplace, 11, *channelID, -1, SQLITE_STATIC);
      sqlite3_bind_int  (stmtImportXMLTVEventReplace, 12, xtEvent->StartTime());
      sqlite3_bind_int  (stmtImportXMLTVEventReplace, 13, dbEventID);
      sqlite3_bind_text (stmtImportXMLTVEventReplace, 14, xtEvent->XTEventID(), -1, SQLITE_STATIC);

      sqlite3_bind_text (stmtImportXMLTVEventReplace, 21, xtEvent->SourceName(), -1, SQLITE_STATIC);
      sqlite3_bind_text (stmtImportXMLTVEventReplace, 22, xtEvent->Title(), -1, SQLITE_STATIC);
      sqlite3_bind_text (stmtImportXMLTVEventReplace, 23, xtEvent->ShortText(),-1, SQLITE_STATIC);
      sqlite3_bind_text (stmtImportXMLTVEventReplace, 24, xtEvent->Description(), -1, SQLITE_STATIC);
      sqlite3_bind_int  (stmtImportXMLTVEventReplace, 25, xtEvent->Duration());

      sqlite3_bind_int  (stmtImportXMLTVEventReplace, 31, xtEvent->Season());
      sqlite3_bind_int  (stmtImportXMLTVEventReplace, 32, xtEvent->Episode());
      sqlite3_bind_int  (stmtImportXMLTVEventReplace, 33, xtEvent->EpisodeOverall());
      sqlite3_bind_text (stmtImportXMLTVEventReplace, 34, xtEvent->OrigTitle(), -1, SQLITE_STATIC);
      sqlite3_bind_text (stmtImportXMLTVEventReplace, 35, xtEvent->Country(), -1, SQLITE_STATIC);

      sqlite3_bind_int  (stmtImportXMLTVEventReplace, 41, xtEvent->Year());
      sqlite3_bind_text (stmtImportXMLTVEventReplace, 42, xtEvent->Credits()->ToString(), -1, SQLITE_STATIC);
      sqlite3_bind_text (stmtImportXMLTVEventReplace, 43, xtEvent->Category()->ToString(), -1, SQLITE_STATIC);
      sqlite3_bind_text (stmtImportXMLTVEventReplace, 44, xtEvent->Review()->ToString(), -1, SQLITE_STATIC);
      sqlite3_bind_text (stmtImportXMLTVEventReplace, 45, xtEvent->ParentalRating()->ToString(),-1, SQLITE_STATIC);

      sqlite3_bind_text (stmtImportXMLTVEventReplace, 51, xtEvent->StarRating()->ToString(), -1, SQLITE_STATIC);
      sqlite3_bind_text (stmtImportXMLTVEventReplace, 52, xtEvent->Pics()->ToString(), -1, SQLITE_STATIC);

      //tsyslog("ImportXMLTVEvent: %s %s %s", xtEvent->Title(), xtEvent->ShortText(), *xtEvent->Pics()->ToString());
      int SQLrc = sqlite3_step(stmtImportXMLTVEventReplace);
      CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);
      success = (SQLrc == SQLITE_DONE);
      if(!success) esyslog("ImportXMLTVEvent Replace: RC=%d XTEventID=%s %s %s %s", SQLrc, xtEvent->XTEventID(), xtEvent->SourceName(), *channelID, *Time2Str(xtEvent->StartTime()));

      sqlite3_clear_bindings(stmtImportXMLTVEventReplace);
      sqlite3_reset(stmtImportXMLTVEventReplace);
      cl++;
   }
   sqlite3_clear_bindings(stmtImportXMLTVEventSelect);
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

int cXMLTVDB::DeleteOrphanedPictures(void)
{  // delete pics if no additional reference in DB
   int pics_deleted = 0, links_deleted = 0;
   OpenDBConnection();
   cXMLTVStringList linkList;
   for (int i = 0; i < orphanedPictures.Size(); i++) {
      cString sqlQueryPics = cString::sprintf("SELECT count(*) FROM epg WHERE src='%s' AND pics LIKE '%%%s%%';",
                                              orphanedPictures.At(i)->A(), orphanedPictures.At(i)->B());

      uint picCount = 0;
      int result = ExecDBQueryInt(*sqlQueryPics, picCount);  //TODO precompile SQL query
      if (result != 1) {
         tsyslog("DeletePictures: SQL-Error %d (0x%08X)", result, DBHandle);
      }
      else {
         if (picCount == 0) {
            cString picFilename = cString::sprintf("%s/%s-img/%s", XMLTVConfig.EPGSourcesDir(), orphanedPictures.At(i)->A(), orphanedPictures.At(i)->B());
            struct stat statbuf;
            if ((stat(*picFilename, &statbuf) == 0) && (statbuf.st_mode & (S_IFLNK | S_IFREG))) {  // regular file or link
               if (unlink(*picFilename) == 0) {
#ifdef DBG_DROPEVENTS2
                  tsyslog("DeletePictures: deleted %s %s", orphanedPictures.At(i)->A(), orphanedPictures.At(i)->B());
#endif
                  pics_deleted++;
               }
            }
         }
         else {
#ifdef DBG_DROPEVENTS2
            tsyslog("DeletePictures: NOT deleted (%d) %s %s", picCount, orphanedPictures.At(i)->A(), orphanedPictures.At(i)->B());
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
            case  0: xtEvent->SetChannelID((const char *) sqlite3_column_text(stmt, col)); break;
            case  1: xtEvent->SetStartTime(sqlite3_column_int(stmt, col)); break;
            case  2: xtEvent->SetEventID(sqlite3_column_int(stmt, col)); break;
            case  3: { uint tableVersion = sqlite3_column_int(stmt, col);
                       xtEvent->SetTableID((tableVersion >> 8) & 0x00FF);
                       xtEvent->SetVersion(tableVersion & 0x00FF); break; }
            case  4: xtEvent->SetXTEventID((const char *)sqlite3_column_text(stmt, col)); break;

            case  5: xtEvent->SetSourceName((const char *) sqlite3_column_text(stmt, col)); break;
            case  6: xtEvent->SetTitle((const char *) sqlite3_column_text(stmt, col)); break;
            case  7: xtEvent->SetShortText((const char *) sqlite3_column_text(stmt, col)); break;
            case  8: xtEvent->SetDescription((const char *) sqlite3_column_text(stmt, col)); break;
            case  9: xtEvent->SetDuration(sqlite3_column_int(stmt, col)); break;

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

bool cXMLTVDB::UpdateEventPrepare(const char *ChannelID)
{
   Transaction_Begin();
   cString sqlUpdateEventSelect = cString::sprintf("SELECT channelid, starttime, eventid, tableversion, xteventid, "
                                   "src, title, shorttext, description, duration, season, episode, episodeoverall, origtitle, "
                                   "country, year, credits, category, review, parentalrating, starrating, pics FROM epg "
                                   "WHERE channelid = '%s' AND eventid = ?1;", ChannelID);
   stmtUpdateEventSelect = NULL;
   int SQLrc = sqlite3_prepare_v2(DBHandle, *sqlUpdateEventSelect, -1, &stmtUpdateEventSelect, NULL);
   return CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);
}

bool cXMLTVDB::UpdateEventFinalize()
{
   int SQLrc = sqlite3_finalize(stmtUpdateEventSelect);
   Transaction_End();
   return CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);
}

cString GetChannelName(tChannelID channelID)
{
   LOCK_CHANNELS_READ
   const cChannel *channel = Channels->GetByChannelID(channelID);
   return channel ? channel->Name() : "'unknown channel name'";
}

bool cXMLTVDB::UpdateEvent(cEvent *Event, uint64_t Flags) //, const char *ChannelName), time_t LastEventStarttime)
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
                    *Time2Str(Event->StartTime()), *TimeString(Event->StartTime()+Event->Duration()), *GetChannelName(Event->ChannelID()), Event->Title(), Event->ShortText());
#endif
            cString sqlUpdate = cString::sprintf("UPDATE epg SET tableversion=%u "
                                                 "WHERE channelid = '%s' AND eventid = %u;",
                                                 (Event->TableID() << 8) | Event->Version(),
                                                 *Event->ChannelID().ToString(), Event->EventID());
            int sqlrc = ExecDBQuery(*sqlUpdate);
            if (sqlrc == SQLITE_OK) {
               xtEvent->SetTableID(Event->TableID());
               xtEvent->SetVersion(Event->Version());
            }
         }
         // do update Event anytime with same content from DB else it will be updated by DVB content
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
         cString sqlSelect = cString::sprintf("SELECT channelid, starttime, eventid, tableversion, xteventid, "
            "src, title, shorttext, description, duration, season, episode, episodeoverall, origtitle, "
            "country, year, credits, category, review, parentalrating, starrating, pics, ABS(starttime - %lu) AS deltaT FROM epg "
            "WHERE channelid='%s' AND starttime >= %lu AND starttime <= %lu "
            "ORDER BY deltaT ASC;",
             Event->StartTime(), *Event->ChannelID().ToString(), Event->StartTime() - TIMERANGE, Event->StartTime() + TIMERANGE);

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
                  //tsyslog("new event found: %02X %s E:%5d (%s) E:%s X:%s E:'%s~%s' X:'%s~%s'", newMatch, *Event->ChannelID().ToString(), Event->EventID(), *GetChannelName(Event->ChannelID()),
                  //        *Time2Str(Event->StartTime()),  *Time2Str(xtEvent->StartTime()), Event->Title(), Event->ShortText(),
                  //        xtEvent->Title(), xtEvent->ShortText());
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
            tsyslog("UpdateEvent2: %s %s %5d (%s) E:%02X%02X X:%02X%02X E:%s~%s X:%s~%s", *Time2Str(Event->StartTime()), *Event->ChannelID().ToString(), Event->EventID(), *GetChannelName(Event->ChannelID()),
                    Event->TableID(), Event->Version(), xtEvent->TableID(), xtEvent->Version(),
                    Event->Title(), Event->ShortText(), xtEvent->Title(), xtEvent->ShortText());
#endif
            cString sqlUpdate = cString::sprintf("UPDATE epg SET eventid=%u, tableversion=%u, episodeoverall=%u "
                                                 "WHERE channelid='%s' AND starttime=%lu;",
                                                 Event->EventID(), (Event->TableID() << 8) | Event->Version(), xtEvent->EpisodeOverall(),
                                                 *Event->ChannelID().ToString(), xtEvent->StartTime());
            ExecDBQuery(*sqlUpdate);
            // Update DVB Event from xtEvent
            xtEvent->FillEventFromXTEvent(Event, Flags);
            DELETENULL(xtEvent);
            handled = true;
         }
         else
         {
#ifdef DBG_EPGHANDLER
            tsyslog("UpdateEvent3: %s %s %d %02X%02X (%s) \"%s~%s\" no matching event found", *Time2Str(Event->StartTime()),*Event->ChannelID().ToString(), Event->EventID(), Event->TableID(), Event->Version(), *GetChannelName(Event->ChannelID()), Event->Title(), Event->ShortText());
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


bool cXMLTVDB::AppendEvents(tChannelID channelID, uint64_t Flags, int *totalSchedules, int *totalEvents)
{
   bool success = true;
   tEventID eventIDOffset = ((Flags & USE_APPEND_EXT_EVENTS) >> SHIFT_APPEND_EXT_EVENTS == ONLY_EXT_EVENTS) ? 0 : EXTERNAL_EVENT_OFFSET;
   Flags = (Flags & ~(USE_TITLE | USE_SHORTTEXT | USE_DESCRIPTION)) | USE_ALWAYS << SHIFT_TITLE | USE_ALWAYS << SHIFT_SHORTTEXT  | USE_ALWAYS << SHIFT_DESCRIPTION;
   const cEvent *lastEvent = NULL;
   cString channelName = GetChannelName(channelID);

   // get last Event of DVB schedule (or NULL if only external)
   cStateKey SchedulesStateKey;
   const cSchedules *Schedules = cSchedules::GetSchedulesRead(SchedulesStateKey);
   if (Schedules) {
      cSchedule *schedule = (cSchedule *)Schedules->GetSchedule(channelID);
      if (schedule) {
         lastEvent = schedule->Events()->Last();
#ifdef DBG_APPENDEVENTS
         cString LastDate = lastEvent ? DayDateTime(lastEvent->EndTime()) : "";
#endif
         while (lastEvent && lastEvent->EventID() >= eventIDOffset) {
            cEvent *prevEvent = (cEvent *)lastEvent->Prev();
            lastEvent = prevEvent;
         }
      }
      else
         esyslog("AppendEvents: Couldn't get or create schedule for %s", *channelName);
   }
   SchedulesStateKey.Remove(false);

   cVector<cEvent *> eventCache(200);
   sqlite3_stmt *stmt;
   cString sqlSelect = cString::sprintf("SELECT channelid, starttime, eventid, tableversion, xteventid, "
      "src, title, shorttext, description, duration, season, episode, episodeoverall, origtitle, "
      "country, year, credits, category, review, parentalrating, starrating, pics FROM epg "
      "WHERE channelid='%s' AND starttime > %lu AND tableversion = %d "
      "ORDER BY starttime ASC;",
      *channelID.ToString(), lastEvent?lastEvent->StartTime() : 0, TABLEVERSION_UNSEEN);

   int SQLrc = sqlite3_prepare_v2(DBHandle, *sqlSelect, -1, &stmt, NULL);
   if (!(success = CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__))) {
      return false;
   }

   SQLrc = sqlite3_step(stmt);
   success = CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);

   time_t lastEndTime = lastEvent ? lastEvent->EndTime() : 0;
   int cnt = 0;
   while (SQLrc == SQLITE_ROW)
   {
      cXMLTVEvent *xtEvent = FillXMLTVEventFromDB(stmt);
      time_t nextStartTime;
      if (xtEvent) {
         tEventID newEventId = xtEvent->GenerateEventID(xtEvent->StartTime(), EXTERNAL_EVENT_OFFSET);   //TODO
         xtEvent->SetEventID(newEventId);
         cEvent *newEvent = new cEvent(newEventId);
         newEvent->SetTableID(0x6F);
         newEvent->SetVersion(0xFF);
         nextStartTime = xtEvent->StartTime();
         int nextDuration = xtEvent->Duration();
         int deltaT = 0;
         if (lastEndTime && lastEndTime > xtEvent->StartTime())
         {  // avoid overlapping events by shortening next event
            deltaT = nextStartTime - lastEndTime;
            nextStartTime = lastEndTime;
            nextDuration -= lastEndTime - xtEvent->StartTime() - 60;
            nextDuration -= nextDuration % 60; // round down to full minutes
         }
         if (nextDuration > xtEvent->Duration() - 300) {  // not more than 5 min delta
#ifdef DBG_APPENDEVENTS
            if (cnt < 4)
               tsyslog("AppendEvents %d add: %s End:%s Start:%s%s Dur:%u", cnt, *channelID.ToString(), *DayDateTime(lastEndTime), *DayDateTime(nextStartTime),
                       deltaT ? *cString::sprintf(" (%+02d)", deltaT/60) : "", nextDuration/60);
#endif
            newEvent->SetStartTime(nextStartTime);
            newEvent->SetDuration(nextDuration);
            xtEvent->FillEventFromXTEvent(newEvent, Flags);
            eventCache.Append(newEvent);
            lastEndTime = xtEvent->StartTime() + xtEvent->Duration();
#ifdef DBG_APPENDEVENTS
            if (cnt < 4)
               tsyslog("new event appended: %s %5u (%s) %s %s~%s", *channelID.ToString(), newEvent->EventID(), xtEvent->XTEventID(),
                       *Time2Str(newEvent->StartTime()), newEvent->Title(), newEvent->ShortText());
#endif
            cString sqlUpdate = cString::sprintf("UPDATE epg SET eventid='%u', tableversion=0x6fff WHERE channelid='%s' AND starttime=%lu;",
                                                 xtEvent->EventID(), *channelID.ToString(), xtEvent->StartTime());
            ExecDBQuery(*sqlUpdate);
            cnt++;
         }
         else
            if (cnt < 4)
               tsyslog("AppendEvents %d skip: %s End:%s Start:%s Dur:%u", cnt, *channelID.ToString(), *DayDateTime(lastEndTime), *DayDateTime(nextStartTime), nextDuration/60);
         DELETENULL(xtEvent);
      }
      SQLrc = sqlite3_step(stmt);
   }
   success = CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__);
   sqlite3_finalize(stmt);

   if (success & cnt > 0)
   {
      {
         LOCK_CHANNELS_WRITE;
         LOCK_SCHEDULES_WRITE;
         const cChannel *channel = Channels->GetByChannelID(channelID);
         if (!channel)
            success = false;
         else {
            const cSchedule *schedule = Schedules->GetSchedule(channel, true);
            if (!schedule || (lastEvent != NULL && !schedule->Events()->Contains(lastEvent))) {
               esyslog("Failed to get schedule or event for %s (%s) %08X", *channelName, *channelID.ToString(), schedule);
               success = false;
            }
         }
      }
      if (success)
      {
         LOCK_SCHEDULES_WRITE;
         cSchedule *schedule = (cSchedule *)Schedules->GetSchedule(channelID);
         if (!schedule) {  // this should never fail but who knows ....
            esyslog("Failed to get schedule for %s (%s) %08X", *channelName, *channelID.ToString(), schedule);
            success = false;
         }
         else {
#ifdef DBG_APPENDEVENTS
            cString LastDate = lastEvent ? DayDateTime(lastEvent->EndTime()) : "";
#endif
            int deleted = 0;
            lastEvent = schedule->Events()->Last();
            while (lastEvent && lastEvent->EventID() >= eventIDOffset) {
               cEvent *prevEvent = (cEvent *)lastEvent->Prev();
               schedule->DelEvent((cEvent *)lastEvent);
               lastEvent = prevEvent;
               deleted++;
            }
            time_t lastEndTime = lastEvent ? lastEvent->EndTime() : 0;
#ifdef DBG_APPENDEVENTS
            if (deleted)
               tsyslog("AppendEvents: Channel ID: %s %2d %s - %s deleted", *channelID.ToString(), deleted, lastEvent?*DayDateTime(lastEvent->EndTime()):"Start", *LastDate);

            if (lastEvent)
               tsyslog("AppendEvents: last event: %s %u %s-%s %lu", *channelID.ToString(), lastEvent->EventID(), *DayDateTime(lastEvent->StartTime()), *TimeString(lastEvent->EndTime()), lastEvent->EndTime());
            else
               tsyslog("AppendEvents: last event: %s %u %s-%s %s", *channelID.ToString(), 0, "none", "none", "none");
#endif
            for (int i = 0; i < eventCache.Size(); i++) {
               cEvent *newEvent = eventCache.At(i);
               if (newEvent->StartTime() >= lastEndTime)
                  schedule->AddEvent(newEvent);
            }

            schedule->Sort();
            ++*totalSchedules;
            *totalEvents += cnt;
#ifdef DBG_APPENDEVENTS
            isyslog("AppendEvents added %3d events to %s (%s)", cnt, *channelID.ToString(), *channelName);
#endif
         }
      }
   }

   return success;
}

cString DayTime(time_t t)
{
  char buffer[32];
  struct tm tm_r;
  tm *tm = localtime_r(&t, &tm_r);
  strftime(buffer, sizeof(buffer), "%d.%m.%y %H:%M:%S", tm);
  return buffer;
}

cString TimeHMS(time_t t)
{
  char buffer[32];
  struct tm tm_r;
  tm *tm = localtime_r(&t, &tm_r);
  strftime(buffer, sizeof(buffer), "%H:%M:%S", tm);
  return buffer;
}

#define MAX_EVENT_DTIME (3*60+20)
#define IS_DVB_EVENT(event) (event->EventID() < EXTERNAL_EVENT_OFFSET)

bool cXMLTVDB::DropOutdated(cSchedule *Schedule, time_t SegmentStart, time_t SegmentEnd, uchar TableID, uchar Version)
{  // called by EPG Handler but only for tuned channels - so does not drop all
   // should drop all additional events with outdated TableID or Version (no cleanup of events before linger time)
   bool handled = false;
   bool eventsShown = false;

   cString channelName;
   tChannelID channelID;
   {
      LOCK_CHANNELS_READ
      channelID = Schedule->ChannelID();
      channelName = Channels->GetByChannelID(channelID)->Name();
   }

#ifdef DBG_DROPEVENTS2
   tsyslog("DropOutdated1  %s - %s %2X-%02X %s %s", *DayTime(SegmentStart), *TimeHMS(SegmentEnd), TableID, Version, *Schedule->ChannelID().ToString(), *channelName);
#endif
   cEvent *e = (cEvent *)Schedule->Events()->First();
   while (e && (abs(e->StartTime() - SegmentStart) > MAX_EVENT_DTIME)) // goto first Event of Segment
      e = (cEvent *)e->Next();

   time_t lastEndTime = e ? e->EndTime() : 0;
   while (e && ((IS_DVB_EVENT(e) && e->EndTime() <= SegmentEnd) || (!IS_DVB_EVENT(e) && (e->StartTime() >= SegmentStart - MAX_EVENT_DTIME) && (e->EndTime() <= SegmentEnd + MAX_EVENT_DTIME))))
   {  // if it's an xtEvent and is completely in segment => delete it because a corresponding DVB event exists
      if (!IS_DVB_EVENT(e)) {
         cEvent *n = (cEvent *)e->Next();
#ifdef DBG_DROPEVENTS2
         tsyslog("  %s-%s %6d %2X-%02X DELETED %s~%s", *DayTime(e->StartTime()), *TimeHMS(e->EndTime()), e->EventID(), e->TableID(), e->Version(), e->Title(), e->ShortText());
#endif
         Schedule->DelEvent((cEvent *)e);
         e = n;
      }
      else {
#ifdef DBG_DROPEVENTS2
         tsyslog("  %s-%s %6d %2X-%02X %s~%s", *DayTime(e->StartTime()), *TimeHMS(e->EndTime()), e->EventID(), e->TableID(), e->Version(), e->Title(), e->ShortText());
#endif
         lastEndTime = e->EndTime();
         e = (cEvent *)e->Next();
      }
   }

   int dTime = e ? lastEndTime - e->StartTime() : 0;
   if (dTime != 0 && e && !IS_DVB_EVENT(e) && (abs(dTime) < MAX_EVENT_DTIME)) {
      e->SetDuration(e->Duration() - dTime); //lastEndTime + e->StartTime());
      e->SetStartTime(lastEndTime);
#ifdef DBG_DROPEVENTS2
      tsyslog("**%s-%s %6d %2X-%02X ADAPTED (%+d:%02u) %s~%s", *DayTime(e->StartTime()), *TimeHMS(e->EndTime()), e->EventID(), e->TableID(), e->Version(), dTime/60, abs(dTime)%60, e->Title(), e->ShortText());
#endif
   }

   cString sqlQuery = NULL;
   // first delete unused pictures (later the names are unknown and they are orphaned)
   sqlQuery = cString::sprintf("SELECT eventid, xteventid, tableversion, starttime, duration, title, shorttext, pics FROM epg "
                               "WHERE channelid='%s' AND starttime >= %lu AND (starttime+duration) < %lu;",
                               *channelID.ToString(), SegmentStart, SegmentEnd);

   sqlite3_stmt *stmt;
   int SQLrc = sqlite3_prepare_v2(DBHandle, *sqlQuery, -1, &stmt, NULL);
   if (!CheckSQLiteSuccess(SQLrc, __LINE__, __FUNCTION__)) {
      return false;
   }

   SQLrc = sqlite3_step(stmt);
   while (SQLrc == SQLITE_ROW)
   {  // fetch one row from DB
      tEventID eventID   = sqlite3_column_int(stmt, 0);
      cString xtEventID = (const char *)sqlite3_column_text(stmt, 1);
      uint tableversion   = sqlite3_column_int(stmt, 2);
      time_t starttime   = sqlite3_column_int(stmt, 3);
      int duration       = sqlite3_column_int(stmt, 4);
      cString title      = (char *)sqlite3_column_text(stmt, 5);
      cString shorttext  = (char *)sqlite3_column_text(stmt, 6);
      cString dbPictures = (const char *) sqlite3_column_text(stmt, 7);

      if (eventID > 0 && tableversion != TABLEVERSION_UNSEEN) {
         uchar dbTableID = (tableversion >> 8) & 0x7F;
         uchar dbVersion = tableversion & 0x00FF;
         if ((dbTableID > 0x4E || TableID == 0x4E) && (dbTableID != TableID || dbVersion != Version)) {
            if (!eventsShown) {
               cEvent *event = (cEvent *)Schedule->GetEventAround(SegmentStart);
               while (event != NULL && event->StartTime() < SegmentEnd) {
                  event = (cEvent *)event->Next();
               }
               eventsShown = true;
            }
            cString sqlQuery = cString::sprintf("UPDATE epg SET tableversion=%u WHERE channelid = '%s' AND eventid = %u;",
                                                DELETE_FLAG | tableversion,  // do NOT update tableversion
                                                *channelID.ToString(), eventID);
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

bool cXMLTVDB::DropEventList(int *LinksDeleted, int *EventsDeleted, const char *WhereClause)
{  // Drop events from DB depeding on WHERE clause
   // called from import: delete all events in DB where tableversion has delete flag (set before import) => delete superseded events
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
      cString xtEventID = NULL;

      src          = (const char *)sqlite3_column_text(stmt, 0);
      channelID    = (const char *)sqlite3_column_text(stmt, 1);
      startTime    = sqlite3_column_int(stmt, 2);
      eventID      = sqlite3_column_int(stmt, 3);
      xtEventID    = (const char *)sqlite3_column_text(stmt, 4);
      cString pics = (const char *)sqlite3_column_text(stmt, 5);
      picList.SetStringList(pics);

      // delete Links to Event
      *LinksDeleted += UnlinkPictures(&picList, *channelID, eventID);

      // remember referenced pictures
      for (int i = 0; i < picList.Size(); i++)
         orphanedPictures.AppendStrings(*src, picList.At(i));

      // delete event entry
      cString sqlDelete = cString::sprintf("DELETE FROM epg WHERE channelid='%s' AND starttime=%lu;",
                                           *channelID, startTime);
#ifdef DBG_DROPEVENTS
      tsyslog("DropEvent: deleting from DB: %s %s_%d %s", *src, *channelID, eventID, xtEventID);
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

      pics_deleted = DeleteOrphanedPictures();
      isyslog("DropOutdatedEvents before %s deleted %d links, %d pics and %d events", *DayDateTime(EndTime), links_deleted, pics_deleted, events_deleted);
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

   msg = cString::sprintf("CheckConsistency: completed %s errors", (failures || !success) ? "with": "without");
   isyslog(*msg);
   CheckResult->Append(strdup(*msg));

   return !failures && success;
}

// ================================ cHouseKeeping ==============================================
cHouseKeeping::cHouseKeeping(): cThread("xmltv4vdr Housekeeping", true)
{
   lastCheckResult.Append(strdup("No check has run so far"));
}

bool cHouseKeeping::StartHousekeeping(eHousekeepingType Type)
{
   type = Type;
   bool started = false;
   Lock();
   if (!Active() && !XMLTVConfig.EPGSources()->ImportIsRunning()) {
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
      xmlTVDB->DropOutdatedEvents(endTime);
   }
   else {
      isyslog("Starting Consistency Check%s", type == HKT_CHECKANDFIXCONSISTENCY ? " with fix" : "");
      bool success = xmlTVDB->CheckConsistency(type == HKT_CHECKANDFIXCONSISTENCY, &lastCheckResult);
      isyslog("Consistency Check finished %s", success ? "successfully": "with failures");
   }

   delete xmlTVDB;
}
