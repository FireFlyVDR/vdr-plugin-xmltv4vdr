/*
 * source.cpp: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <ctype.h>
#include <vdr/plugin.h>
#include "source.h"
#include "extpipe.h"
#include "debug.h"

// -------------------------------------------------------------
class cEPGSearch_Client
{
private:
   cPlugin *plugin;
public:
   cEPGSearch_Client() { plugin = cPluginManager::GetPlugin("epgsearch"); };
   bool EnableSearchTimers(bool Activate)
   {
      if (plugin) 
      {
         // Data structure for service "Epgsearch-enablesearchtimers-v1.0"
         struct Epgsearch_enablesearchtimers_v1_0
         {  // in
            bool enable;  // enable/disable search timer
         } serviceData;
         serviceData.enable = Activate;
         if (!plugin->Service("Epgsearch-enablesearchtimers-v1.0", (void*) &serviceData))
            return false;
      }
      return true; // also true if epgsearch is not loaded
   }
};


// -------------------------------------------------------------
cEPGSource::cEPGSource(const char *SourceName)
{
   sourceName = SourceName;
   pin = NULL;
   log = cString("");
   lastEventStartTime = -1;
   lastSuccessfulRun = 0;
   usePipe = false;
   needPin = false;
   running = false;
   hasPics = usePics = false;
   daysInAdvance = 1;
   numExecTimes = 1;
   execTimes[0] = 10;
   execDays = 0x7f; // Mon->Sun
   nextExecTime = 0;
   enabled = true;
   ready2parse = ReadConfig();
   dsyslogs(this, "is %sready2parse", ready2parse ? "" : "not ");
}

cString cEPGSource::GetExecTimesString()
{
   char buffer[5 * MAX_EXEC_TIMES];
   char *q = buffer;
   *q = 0;
   for (int i = 0; i < numExecTimes; i++) {
      q += sprintf(q, "%s%02d%02d", i > 0 ? " ":"", execTimes[i]/100, execTimes[i]%100);
   }

   return buffer;
}

void cEPGSource::ParseExecTimes(const char *ExecTimeString)
{
   char *t;
   numExecTimes = 0;
   const char *p = ExecTimeString;
   while (p && *p && numExecTimes < MAX_EXEC_TIMES) {
      int n = strtol(p, &t, 10);
      if (t != p)
         execTimes[numExecTimes++] = n;
      p = t;
   }
   GetNextExecTime(time(NULL), true);
}


void cEPGSource::SetExecTimes(int numTimes, int *Times)
{
   numExecTimes = numTimes < MAX_EXEC_TIMES ? numTimes : MAX_EXEC_TIMES;
   for(int i = 0; i < numExecTimes; i++)
   {
      execTimes[i] = Times[i];
   }
   GetNextExecTime(time(NULL), true);
}


time_t cEPGSource::GetNextExecTime(time_t CheckTime, bool ForceCalculation)
{  //  0: never
   // >0: time_t exec time equal or greater than CheckTime

   time_t t = 0;
   if (enabled)
   {
      time_t checkTime = CheckTime ? CheckTime : time(NULL);
      checkTime -= checkTime % 60;  // round down to full minutes

      t = nextExecTime;
      if (t < checkTime || ForceCalculation) {
         bool noMatch = true;
         t = cTimer::SetTime(checkTime, 0);
         while (t < checkTime || noMatch)
         {
            // find next exec day
            while ((execDays & (1 << cTimer::GetWDay(t))) == 0)
               t = cTimer::IncDay(t, 1);
            int i = 0;
            t = cTimer::SetTime(t, cTimer::TimeToInt(execTimes[i]));
            // find first time >= checkTime
            while (t < checkTime && ++i < numExecTimes) {
               t = cTimer::SetTime(checkTime, cTimer::TimeToInt(execTimes[i]));
            }
            if ((noMatch = t < checkTime)) {
               t = cTimer::IncDay(t, 1);
               t = cTimer::SetTime(t, 0);
            }
         }
      }
   }
   nextExecTime = t;
   return nextExecTime;
}

bool cEPGSource::ExecuteNow(time_t StartTime)
{
   if (enabled) {
      time_t nextRunTime = GetNextExecTime(StartTime);
      if (nextRunTime > 0 && StartTime == nextRunTime)
         return (XMLTVConfig.EPGChannels()->HasActiveEPGChannels(sourceName));
   }
   return false;
}

bool cEPGSource::ReadConfig(void)
{  // /var/lib/epgsources/*
   // tvsp:
   //    file;05:00;0;1
   //    14
   //    sat1.de
   //    arte.de
   //    ard.de

   // tvm:
   //    pipe;06:00;0;1
   //    1;15
   //    3sat.de;12
   //    anixe.de;269
   //    ard.de;1

   bool success = true;

   // read capability config file in /var/lib/epgsources (available channels)
   cString srcConfig = cString::sprintf("%s/%s", XMLTVConfig.EPGSourcesDir(), *sourceName);

   FILE *f = fopen(*srcConfig, "r");
   if (!f)
   {
      esyslogs(this, "cannot read source config %s", *srcConfig);
      success = false;
   }
   else {
      dsyslogs(this,"reading source config %s", *srcConfig);
      size_t linesize = 0;
      char *line = NULL;
      int lineNo = -1;
      cReadLine ReadLine;
      while (success && ((line = ReadLine.Read(f)) != NULL))
      {
         if (lineNo == -1)
         {
            // file;00:00;0;1
            char inputtype[6] = "pipe";
            int hr = 0, min = 0, pin = 0, pics = 0;
            if ((sscanf(line, "%4s;%2d:%2d;%1d;%1d", &inputtype[0], &hr, &min, &pin, &pics)) > 1) {
               usePipe = strcmp("pipe", inputtype) == 0;
               needPin = pin == 1;
               hasPics = pics == 1;
               dsyslogs(this, "input method: %s, daily updates at %02d:%02d, %spin required, %spictures provided", 
                        usePipe ? "pipe" : "file", hr, min, needPin ? "" : "no ", hasPics ? "" : "no " );

               min += 10;
               if (min >= 60)
               {
                  min -= 60;
                  hr++;
                  if (hr>23) hr = 0;
               }
               execTimes[0] = (hr*100) + min;
            }
            else {
               esyslogs(this, "parsing line 1 failed: %s", line);
               success = false;
            }
            lineNo++;
         }
         else if (lineNo == 0)
         {
            int used, daysmax1 = 0, daysmax2 = 0;
            if ((used = sscanf(line, "%d;%d",  &daysmax1, &daysmax2)) >= 1) {
               // for backward compatibility ignore first value if two are provided
               maxDaysProvided = used == 1 ? daysmax1 : daysmax2; 
               dsyslogs(this, "maximum days provided: %d", maxDaysProvided);
            }
            else {
               esyslogs(this, "parsing line 2 failed: %s", line);
               success = false;
            }
            lineNo++;
         }
         else // linenr > 2
         {  // channels
            char *p = strchr(line, ';');
            if (p) *p = 0;
            if (!isempty(line)) {
               epgChannels.Append(strdup(line));
               lineNo++;
            }
         }
      }
      dsyslogs(this, "added %d channels", lineNo > 0 ? lineNo : 0);
      epgChannels.Sort();
      fclose(f);
   }

   return success;
}


bool cEPGSource::ReadXMLTVfile(char *&xmltv_buffer, size_t &size)
{  // read xmltv file into memory buffer
   bool success = true;
   cString xmltv_file = cString::sprintf("%s/%s.xmltv", XMLTVConfig.EPGSourcesDir(), *sourceName);
   dsyslogs(this,"reading from '%s'", *xmltv_file);

   int xmltv_fd = open(*xmltv_file, O_RDONLY);
   if (xmltv_fd == -1)
   {
      esyslogs(this, "failed to open '%s'", *xmltv_file);
      success = false;
   }
   else
   {
      struct stat statbuf;
      if (fstat(xmltv_fd, &statbuf) == -1)
      {
         esyslogs(this, "failed to stat '%s'", *xmltv_file);
         close(xmltv_fd);
         success = false;
      }
      else 
      {
         size = statbuf.st_size;
         xmltv_buffer = (char *) malloc(size + 1);
         if (!xmltv_buffer)
         {
            close(xmltv_fd);
            esyslogs(this, "out of memory while reading '%s'", *xmltv_file);
            success = false;
         }
         else 
         {
            if (read(xmltv_fd, xmltv_buffer, statbuf.st_size) != statbuf.st_size)
            {
               esyslogs(this, "failed to read '%s'", *xmltv_file);
               success = false;
               free(xmltv_buffer);
               xmltv_buffer = NULL;
            }
            close(xmltv_fd);
         }
      }
   }
   return success;
}


bool cEPGSource::Execute(void)
{  /// fetch external EPG
   if (!ready2parse) return false;

   char *outBuffer = NULL;
   char *errBuffer = NULL;
   size_t outBufferSize = 0;
   size_t errBufferSize = 0;
   bool success = true;
#define MAX_TRIES 3

   cString cmd = cString::sprintf("%s %i '%s' %i ", *sourceName, daysInAdvance, isempty(*pin) ? "" : *pin, usePics);

   cString epgChannels = XMLTVConfig.EPGChannels()->GetActiveEPGChannels(sourceName);
   tsyslog("Execute: %s", *epgChannels);
   if (isempty(*epgChannels))
   {
      esyslogs(this,"no channels, please configure source");
      return false;
   }

   cmd.Append(*epgChannels);
   if (!usePipe) cmd.Append(" >&2"); // redirect stdout for file plugins to stderr

   isyslogs(this,"%s", *cString::sprintf("%s %i '%s' %i %s", *sourceName, daysInAdvance, isempty(*pin) ? "" : "X@@@", usePics, *epgChannels));

   int tries = 0;
   do
   {
      tries++;
      cExtPipe extPipe(sourceName);
      if (!((success = extPipe.Open(cmd))))
      {
         esyslogs(this,"failed to open pipe");
      }
      else
      {
         running = true;
         success = extPipe.GetResult(&outBuffer, outBufferSize, &errBuffer, errBufferSize);
         if (errBuffer && XMLTVConfig.LogFile())
         {  // output all stderr lines to debug log
            char *saveptr;
            char *line = strtok_r(errBuffer, "\n", &saveptr);
            while (line) {
               tsyslogs(this, "%s", line);
               line = strtok_r(NULL, "\n", &saveptr);
            }
         }
         free(errBuffer);
         errBuffer = NULL;
         errBufferSize = 0;
         success &= extPipe.Close();
      }

      if (success)
      {
         if (!usePipe) // use file
            success = ReadXMLTVfile(outBuffer, outBufferSize);  // read xmltv file into memory buffer
      }
      else 
      {
         dsyslogs(this, "waiting 60 seconds");
         int l = 0;
         while (l < 300) {  // TODO
            cCondWait::SleepMs(200);
            if (!XMLTVConfig.EPGSources()->Active()) {
               isyslogs(this, "request to stop from vdr");
               return false;
            }
            l++;
         }
      }
   } while (!success && tries < MAX_TRIES);


   if (!success && tries == MAX_TRIES) // failed
      esyslogs(this, "aborting after %i tries", tries);
   else
   {
      isyslogs(this, "successfully executed after %i %s", tries, tries == 1 ? "try" : "tries");

      if (success && (outBuffer)) {
         success = ParseAndImportXMLTV(outBuffer, outBufferSize, *sourceName);
         XMLTVConfig.StoreSourceParameter(this);  // save lastEventStartTime
         XMLTVConfig.Save();
      }
   }

   running = false;
   return success;
}


time_t cEPGSource::XmltvTime2UTC(char *xmltvtime)
{  // Convert xmltvtime string into time_t format
   time_t uxTime = 0;
   if (xmltvtime) {
      struct tm tm_time;
      int y = 0, m = 0, d = 0, hr = 0, mn = 0, sec = 0;
      int offset = 0;
      int fields = 0;
      char timezone[8] = "";
      if ((fields = sscanf(xmltvtime, "%4d%2d%2d%2d%2d%2d %7s", &y, &m, &d, &hr, &mn, &sec, (char *)&timezone)) >= 5) {
         tm_time.tm_year = y - 1900;
         tm_time.tm_mon = m - 1;
         tm_time.tm_mday = d;
         tm_time.tm_hour = hr;
         tm_time.tm_min = mn;
         tm_time.tm_sec = sec;
         tm_time.tm_isdst = -1;
         int offh = 0, offl = 0;
         char sign = '*';
         if (sscanf(timezone, "%c%2d%2d", &sign, &offh, &offl) == 3 )
            offset = ((sign == '-') ? -60 : 60) * (60 * offh + offl);
         else 
            strptime(xmltvtime, "%4d%2d%2d%2d%2d%2d %Z", &tm_time);
         uxTime = timegm(&tm_time) - offset;
      }
      else
         esyslog("Could not parse time '%s'", xmltvtime);
   }
   return uxTime;
}


bool cEPGSource::ParseAndImportXMLTV(char *buffer, int bufsize, const char *SourceName)
{  // process the buffer with the XMLTV file in memory
   // add episode info
   // import event into DB for all mapped DVB channels
   if (!buffer || !bufsize) return 134;

   isyslogs(this, "parsing xmltv buffer (%.1f MB) and importing events into DB", bufsize/1024./1024.);

   xmlDocPtr xmltv;
   xmltv = xmlReadMemory(buffer, bufsize, NULL, NULL, XML_PARSE_NOENT);
   if (!xmltv)
   {
      esyslogs(this,"failed to parse xmltv");
      return false;
   }

   xmlNodePtr rootnode = xmlDocGetRootElement(xmltv);
   if (!rootnode)
   {
      esyslogs(this,"no rootnode in xmltv");
      xmlFreeDoc(xmltv);
      return false;
   }

   /// - open DB and begin Transaction
   cXMLTVDB *xmlTVDB = new cXMLTVDB();

   if (!xmlTVDB->OpenDBConnection())
   {
      xmlFreeDoc(xmltv);
      return false;
   }

   cEpisodesDB *episodesDB = NULL;
   if (XMLTVConfig.UseEpisodes()) {
      episodesDB = new cEpisodesDB();
      if (!episodesDB->OpenDBConnection())
         DELETENULL(episodesDB);
   }

   if (!xmlTVDB->ImportXMLTVEventPrepare())
   {
      xmlTVDB->CloseDBConnection();
      delete xmlTVDB;
      xmlFreeDoc(xmltv);
      return false;
   }

   time_t begin = EVENT_LINGERTIME;

   int lastError = PARSE_NOERROR;
   int skipped = 0, outdated = 0, faulty = 0, imported = 0;
   int failed = 0;
   xmlChar *lastchannelid = NULL;
   time_t lastStoptime = 0;
   cXMLTVEvent xtEvent;
   cEPGChannel *epgChannel = NULL;

   // loop over XML enodes
   xmlNodePtr node = rootnode->xmlChildrenNode;
   while (node)
   {
      xtEvent.Clear();
      xtEvent.SetSourceName(sourceName);
      if (node->type != XML_ELEMENT_NODE)
      {
         node = node->next;
         continue;
      }
      if ((xmlStrcasecmp(node->name, (const xmlChar *) "programme")))
      {
         node = node->next;
         continue;
      }

      // get channel ID
      xmlChar *channelid = xmlGetProp(node,(const xmlChar *) "channel");
      if (!channelid)
      {
         if (lastError != PARSE_NOCHANNELID)
            esyslogs(this, "missing channelid in xmltv file");
         lastError = PARSE_NOCHANNELID;
         node = node->next;
         faulty++;
         continue;
      }

      if (lastchannelid == NULL || xmlStrcmp(channelid, lastchannelid))
      {  // new channelid
         if (lastchannelid) xmlFree(lastchannelid);
         lastchannelid = xmlStrdup(channelid);
         if (epgChannel && lastError == PARSE_NOERROR) {  // drop superseded events of previous channel
            xmlTVDB->DropOutdatedEvents(epgChannel->ChannelIDList(), lastEventStartTime); // Drop all events not in current XML
            xmlTVDB->Transaction_End();
         }

         if (!XMLTVConfig.EPGChannels()->IsActiveEPGChannel((const char *)channelid, *sourceName)) {
            epgChannel = NULL;
            isyslogs(this,"WARNING: found additional channelid %s", channelid);
            xmlFree(channelid);
            node = node->next;
            skipped++;
            continue;
         }

         epgChannel = XMLTVConfig.EPGChannels()->GetEpgChannel((const char *)channelid); // flags needed for ImportXMLTVEvent
         if (!epgChannel || epgChannel->ChannelIDList().Size() == 0)
         {
            esyslogs(this,"no mapping for channelid %s", channelid);
            lastError = PARSE_NOMAPPING;
            xmlFree(channelid);
            node = node->next;
            skipped++;
            continue;
         }
         lastStoptime = 0;
         lastError = PARSE_NOERROR;
         xmlTVDB->Transaction_Begin();
         xmlTVDB->MarkEventsOutdated(epgChannel->ChannelIDList()); // Mark all events of Channel as outdated;
      }
      xmlFree(channelid);

      if (epgChannel) {
         // get start/stop times
         xmlChar *start = NULL, *stop = NULL;
         time_t starttime = 0;
         time_t stoptime = 0;
         start = xmlGetProp(node, (const xmlChar *)"start");
         if (start)
         {
            starttime = XmltvTime2UTC((char *)start);
            if (starttime)
            {
               stop = xmlGetProp(node, (const xmlChar *)"stop");
               if (stop)
                  stoptime = XmltvTime2UTC((char *)stop);
            }
         }

         if (!starttime)
         {
            if (lastError != PARSE_XMLTVERR)
               esyslogs(this,"no starttime, check xmltv file");
            lastError = PARSE_XMLTVERR;
            node = node->next;
            faulty++;
            if (start) xmlFree(start);
            if (stop) xmlFree(stop);
            continue;
         }

         if (stoptime < begin) // import only events with stoptime later than EPGlinger time (ignore events before)
         {  // if event in XML ends before begintime then parse icon node to find pictures and put them in orphaned list to check if they are obsolete
            xmlNodePtr childNode = node->xmlChildrenNode;
            while (childNode)
            {
               if (node->type == XML_ELEMENT_NODE && !xmlStrcasecmp(childNode->name, (const xmlChar *)"icon")) {
                  xmlChar *src = xmlGetProp(childNode, (const xmlChar *)"src");
                  if (src) {
                     const xmlChar *f = xmlStrstr(src, (const xmlChar *)"://");
                     if (f) {  // url: skip scheme and scheme-specific-part
                        f += 3;
                     }
                     else {  // just try it
                        f = src;
                     }
                     struct stat statbuf;
                     if (stat((const char *) f, &statbuf) != -1) {
                        char *file = strrchr((char *)f, '/');
                        if (file)
                           xmlTVDB->AddOrphanedPicture(SourceName, ++file);
                     }
                     xmlFree(src);
                  }
               }
               childNode = childNode->next;
            }

            node = node->next;
            if (start) xmlFree(start);
            if (stop) xmlFree(stop);
            outdated++; // don't count expired events as "skipped"
            continue;
         }

         xtEvent.SetStartTime(starttime);

         if (stoptime)
         {
            if (stoptime < starttime)
            {
               if (stoptime < starttime) {
                  esyslogs(this,"%s: stoptime (%s) < starttime(%s), check xmltv file", lastchannelid, stop, start);
                  lastError = PARSE_XMLTVERR;
               }
               node = node->next;
               faulty++;
               if (start) xmlFree(start);
               if (stop) xmlFree(stop);
               continue;
            }
            xtEvent.SetDuration(stoptime-starttime);
            lastStoptime = stoptime;
         }

         if (start) xmlFree(start);
         if (stop) xmlFree(stop);

         // fill remaining fields in xEvent from XML node
         if (!FillXTEventFromXmlNode(&xtEvent, node))
         {
            if (lastError != PARSE_FETCHERR)
               esyslogs(this,"failed to fetch event");
            lastError = PARSE_FETCHERR;
            node = node->next;
            failed++;
            continue;
         }

         if(episodesDB)
            episodesDB->QueryEpisode(&xtEvent);

         const xmlError* xmlErr = xmlGetLastError();
         if (xmlErr && xmlErr->code)
         {
            esyslogs(this, "xmlError: %s", xmlErr->message);
         }

         // set xtEventID if empty
         if (!xtEvent.XTEventID())
            xtEvent.SetXTEventID(xtEvent.StartTime());

         // insert xtEvent in DB for all mapped channels
         if (xmlTVDB->ImportXMLTVEvent(&xtEvent, epgChannel->ChannelIDList()))
         {  // successfully imported
            imported++;
            if (lastEventStartTime < starttime)
               lastEventStartTime = starttime;
         }
         else
         {
            if (lastError != PARSE_IMPORTERR)
               esyslogs(this,"failed to import event(s) of %s into DB", epgChannel->EPGChannelName());
            lastError = PARSE_IMPORTERR;
            failed++;
         }
      }

      node = node->next;
      if (!XMLTVConfig.EPGSources()->Active())
      {
         isyslogs(this, "VDR requested to stop");
         break;
      }
   } // end of node loop

   if(episodesDB) {
      episodesDB->CloseDBConnection();
      delete episodesDB;
   }

   if (epgChannel) {
      xmlTVDB->DropOutdatedEvents(epgChannel->ChannelIDList(), lastEventStartTime); // Drop all events not in current XML
      xmlTVDB->Transaction_End();
   }

   xmlTVDB->ImportXMLTVEventFinalize();
   xmlTVDB->Analyze();
   xmlTVDB->CloseDBConnection();

   xmlTVDB->DeleteOrphanedPictures();
   delete xmlTVDB;

   isyslogs(this,"xmltv buffer parsing skipped %i faulty xmltv events, failed %d, outdated %d", faulty, failed, outdated);

   if (lastError == PARSE_NOERROR)
      isyslogs(this,"xmltv buffer parsed, imported %i xmltv events into DB", imported);
   else
      isyslogs(this,"xmltv buffer parsed, imported %i xmltv events into DB - see ERRORs above!", imported);

   xmlFreeDoc(xmltv);
   return true;
}


bool cEPGSource::FillXTEventFromXmlNode(cXMLTVEvent *xtEvent, xmlNodePtr enode)
{  /// fill xEvent from XMLTV node, returns true if HasTitle()
   char *syslang = getenv("LANG");
   xmlNodePtr node = enode->xmlChildrenNode;
   while (node)
   {
      // comment node
      if (node->type == XML_COMMENT_NODE) {
         if (const xmlChar *pid = xmlStrstr(node->content, (const xmlChar *)"pid")) {
            char *eq = strchr((char *)pid, '=');
            if (eq) {
               xtEvent->SetXTEventID((uint64_t)atoll(eq + 1));
            }
         }
         if (const xmlChar *content = xmlStrstr(node->content, (const xmlChar *)"content")) {
            char *eq = strchr((char *) content, '=');
            if (eq)
               xtEvent->AddCategory(eq + 1);
         }
      }

      // element nodes
      if (node->type == XML_ELEMENT_NODE) {
         // title
         if ((!xmlStrcasecmp(node->name, (const xmlChar *)"title"))) {
            xmlChar *lang = xmlGetProp(node, (const xmlChar *)"lang");
            xmlChar *content = xmlNodeListGetString(node->doc, node->xmlChildrenNode,1);
            if (content) {
               if (lang && syslang && !xmlStrncasecmp(lang, (const xmlChar *)syslang, 2))
                  xtEvent->SetTitle(*StringCleanup((const char *)content));
               else {
                  if (!xtEvent->HasTitle())  // set title if not yet set
                     xtEvent->SetTitle(*StringCleanup((const char *)content));
                  else                      // if title exists put it in OrigTitle
                     xtEvent->SetOrigTitle(*StringCleanup((const char *)content));
               }
               xmlFree(content);
            }
            if (lang) xmlFree(lang);
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"sub-title"))) {
            // sub-title - what to do with attribute lang?
            xmlChar *content = xmlNodeListGetString(node->doc, node->xmlChildrenNode, 1);
            if (content) {
               xtEvent->SetShortText(*StringCleanup((const char *)content));
               xmlFree(content);
            }
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"desc"))) {
            // what to do with attribute lang?
            xmlChar *content = xmlNodeListGetString(node->doc, node->xmlChildrenNode, 1);
            if (content) {
               cString description = (const char *)content;
               description.CompactChars(' ');
               description.CompactChars('\n');
               xtEvent->SetDescription(*description);
               xmlFree(content);
            }
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"credits"))) {
            xmlNodePtr vnode = node->xmlChildrenNode;
            while (vnode) {
               if (vnode->type == XML_ELEMENT_NODE) {
                  if ((!xmlStrcasecmp(vnode->name, (const xmlChar *)"actor"))) {
                     xmlChar *content = xmlNodeListGetString(vnode->doc, vnode->xmlChildrenNode, 1);
                     if (content) {
                        xmlChar *actorRole = xmlGetProp(vnode, (const xmlChar *)"role");
                        xtEvent->AddCredits((const char *)vnode->name, (const char *)content, isempty((const char *)actorRole) ? NULL : (const char *)actorRole);
                        if (actorRole) xmlFree(actorRole);
                        xmlFree(content);
                     }
                  }
                  else { // not "actor"
                     xmlChar *content = xmlNodeListGetString(vnode->doc, vnode->xmlChildrenNode, 1);
                     if (content) {
                        if (!isempty(stripspace((char *)content)))
                           xtEvent->AddCredits((const char *)vnode->name, (const char *)content);
                        xmlFree(content);
                     }
                  }
               }
               vnode = vnode->next;
            }
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"date"))) {
            xmlChar *content = xmlNodeListGetString(node->doc, node->xmlChildrenNode, 1);
            if (content) {
               xtEvent->SetYear(atoi((const char *)content));
               xmlFree(content);
            }
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"category"))) {
            // what to do with attribute lang?
            xmlChar *content = xmlNodeListGetString(node->doc, node->xmlChildrenNode, 1);
            if (content) {
               if (!isdigit(content[0])) {
                  cString category = (const char *)content;
                  category.CompactChars(' ');
                  category.CompactChars('\n');
                  xtEvent->AddCategory(*category);
               }
               xmlFree(content);
            }
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"country"))) {
            xmlChar *content = xmlNodeListGetString(node->doc, node->xmlChildrenNode, 1);
            if (content) {
               cString country = (const char *)content;
               country.CompactChars(' ');
               xtEvent->SetCountry(*country);
               xmlFree(content);
            }
         }

         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"rating"))) {
            xmlChar *system = xmlGetProp(node,(const xmlChar *)"system");
            if (system) {
               xmlNodePtr vnode = node->xmlChildrenNode;
               while (vnode) {
                  if (vnode->type == XML_ELEMENT_NODE) {
                     if ((!xmlStrcasecmp(vnode->name, (const xmlChar *)"value"))) {
                        xmlChar *content = xmlNodeListGetString(vnode->doc, vnode->xmlChildrenNode, 1);
                        if (content) {
                           xtEvent->AddParentalRating((const char *)system, (const char *)content);
                           xmlFree(content);
                        }
                     }
                  }
                  vnode = vnode->next;
               }
               xmlFree(system);
            }
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"star-rating"))) {
            xmlChar *system = xmlGetProp(node,(const xmlChar *)"system");
            xmlNodePtr vnode = node->xmlChildrenNode;
            while (vnode) {
               if (vnode->type == XML_ELEMENT_NODE) {
                  if ((!xmlStrcasecmp(vnode->name, (const xmlChar *)"value"))) {
                     xmlChar *content = xmlNodeListGetString(vnode->doc, vnode->xmlChildrenNode, 1);
                     if (content) {
                        xtEvent->AddStarRating((const char *)system, (const char *)content);
                        xmlFree(content);
                     }
                  }
               }
               vnode = vnode->next;
            }
            if (system) xmlFree(system);
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"review"))) {
            xmlChar *type = xmlGetProp(node, (const xmlChar *)"type");
            if (type && !xmlStrcasecmp(type, (const xmlChar *)"text")) {
               xmlChar *content = xmlNodeListGetString(node->doc, node->xmlChildrenNode, 1);
               if (content) {
                  cString review = (const char *)content;
                  review.CompactChars(' ');
                  review.CompactChars('\n');
                  xtEvent->AddReview(*review);
                  xmlFree(content);
               }
               xmlFree(type);
            }
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"icon"))) {
            xmlChar *src = xmlGetProp(node, (const xmlChar *)"src");
            if (src) {
               const xmlChar *f = xmlStrstr(src, (const xmlChar *)"://");
               if (f) {  // url: skip scheme and scheme-specific-part
                  f += 3;
               }
               else
                  f = src;

               struct stat statbuf;
               if (stat((const char *) f, &statbuf) != -1) {
                  char *file = strrchr((char *)f, '/');
                  if (file) {
                     xtEvent->AddPic(++file);
                  }
               }
               xmlFree(src);
            }
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"episode-num"))) {
            xmlChar *system = xmlGetProp(node,(const xmlChar *)"system");
            if (system) {
               if (!xmlStrcasecmp(system, (const xmlChar *)"xmltv_ns")) {
                  xmlChar *content = xmlNodeListGetString(node->doc, node->xmlChildrenNode, 1);
                  if (content) {
                     // <episode-num system="xmltv_ns">1 . 1 . 0/1</episode-num>
                     // format is:  season[/max_season].episode[/max_episode_in_season].part[/max_part]
                     //             all numbers are zero based, overallepisode is not representable,
                     //             one or two (or even three?) numbers may be omitted, e.g. '0.5.'
                     //             means episode 6 in season 1
                     cString xmltv_ns = (const char *)content;
                     if (!isempty(*xmltv_ns)) {
                        xmltv_ns = (const char *)compactspace((char *)*xmltv_ns);
                        if (strlen(*xmltv_ns) > 1) {
                           cString before, middle, after;
                           if (3 == strsplit(*xmltv_ns, '.', before, middle, after)) {
                              int season = atoi(*before);
                              if (season > 0) {
                                 xtEvent->SetSeason(++season);
                              }
                              int episode = atoi(*middle);
                              if (episode > 0) {
                                 xtEvent->SetEpisode(++episode);
                              }
                              int overall = atoi(*after);
                              if (overall > 0) {
                                 xtEvent->SetEpisodeOverall(++overall);
                              }
                           }
                        }
                     }
                     xmlFree(content);
                  }
               }
               else if (!xmlStrcasecmp(system, (const xmlChar *)"onscreen")) {
                  xmlChar *content = xmlNodeListGetString(node->doc, node->xmlChildrenNode, 1);
                  if (content) {
                     // <episode-num system="onscreen">S01E01</episode-num> ==> not yet implemented
                     // <episode-num system="onscreen">7/8</episode-num>
                     cString before, middle, after;
                     if (2 == strsplit((const char *)content, '/', before, middle, after)) {
                        int episode = atoi(*before);
                        if (episode > 0) {
                           xtEvent->SetEpisode(episode);
                        }
                     }
                     xmlFree(content);
                  }
               }
               xmlFree(system);
            }
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"video"))) {
            // video info like "HDTV" -> just ignore
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"audio"))) {
            // audio info like "Dolby Digital" -> just ignore
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"length"))) {
            // length without advertisements -> just ignore
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"subtitles"))) {
            // info about subtitles -> just ignore (till now)
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"new"))) {
            // info if it's new -> just ignore (till now)
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"live"))) {
            // info if it's live -> just ignore (till now)
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"premiere"))) {
            // premiere info -> just ignore (till now)
         }
         else if ((!xmlStrcasecmp(node->name, (const xmlChar *)"previously-shown"))) {
            // info if it's old ;) -> just ignore (till now)
         }
         else {
            esyslogs(this,"unknown element %s, please report!", node->name);
         }
      }
      node = node->next;
   }

   int season = xtEvent->Season(), episode = xtEvent->Episode(), episodeoverall = 0;

   return xtEvent->HasTitle();
}


bool cEPGSource::ProvidesChannel(const char *EPGchannelName)
{
   return(epgChannels.Find(EPGchannelName) > -1);
}

void cEPGSource::Add2Log(struct tm *Tm, const char Prefix, const char *Line)
{  // append Line to Log
   if (Line) {
      char dt[30];
      strftime(dt, sizeof(dt)-1, "%H:%M", Tm);
      log.Append(cString::sprintf("%c%s %s\n", Prefix, dt, Line));
   }
}

// -------------------------------------------------------------
cEPGSources::cEPGSources() : cThread("xmltv4vdr Importer", true)
{
   manualStart = false;
   epgImportSource = NULL;
   isImporting = false;
}

cEPGSources::~cEPGSources()
{
   RemoveAll();
}

cEPGSource *cEPGSources::GetSource(const char* SourceName)
{  /// get pointer to source with "SourceName"
   if (SourceName && Count())
   {
      for (cEPGSource *source = First(); source; source = Next(source))
         if (!strcmp(SourceName, source->SourceName()))
            return source;
   }
   return NULL;
}

void cEPGSources::RemoveAll()
{
   cEPGSource *source;
   while ((source = Last()) != NULL)
      Del(source);
}

bool cEPGSources::ExecuteNow(time_t StartTime)
{  // return true if any Source needs to be executed
   for (cEPGSource *source = First(); source; source = Next(source)) {
      if (source->ExecuteNow(StartTime)) return true;
   }

   return false;
}

time_t cEPGSources::NextRunTime()
{
   time_t nextRunTime = 0;

   for (cEPGSource *source = First(); source; source = Next(source))
   {
      time_t srcNext = source->GetNextExecTime();
      if (srcNext > 0 && (nextRunTime == 0 || srcNext < nextRunTime))
      {
         nextRunTime = srcNext;
      }
   }
   return nextRunTime;
}

void cEPGSources::ReadAllConfigs(void)
{  /// read configs of all defined sources in EPG sources dir
   RemoveAll();
   cReadDir dir(XMLTVConfig.EPGSourcesDir());
   struct dirent *entry;
   while ((entry = dir.Next()) != NULL) {
      if (entry->d_type != DT_DIR && !GetSource(entry->d_name)) {  // TASK if source exists: delete and reload ?
         cString fileName = AddDirectory(XMLTVConfig.EPGSourcesDir(), entry->d_name);
         int fd = open(*fileName, O_RDONLY);
         if (fd == -1) {
            esyslog("Error %d opening config file '%s'", errno, *fileName);
         }
         else
         {
            char magic[5];
            if (read(fd, magic, 4) != 4)
               esyslog("Error reading config file '%s'", *fileName);
            else
            {
               magic[4] = 0;
               if (!strcmp(magic, "file") || !strcmp(magic, "pipe"))
                  Add(new cEPGSource(entry->d_name));
            }
            close(fd);
         }
      }
   }

#ifdef DEBUG
   tsyslog("ReadAllConfigs %d sources", XMLTVConfig.EPGSources() ? XMLTVConfig.EPGSources()->Count() : 0);
   for (cEPGSource *source = First(); source; source = Next(source))
      tsyslog("   src %s", source->SourceName());
#endif
}

void cEPGSources::ResetLastEventStarttimes()
{
   for (cEPGSource *source = First(); source; source = Next(source))
      source->SetLastEventStarttime(-1);
}

void cEPGSources::StartImport(cEPGSource *EpgSource)
{
   cMutexLock mtxImportLock(&mtxImport);
   epgImportSource = EpgSource;
   manualStart = true;
   cwDelay.Signal();
   cvBlock.Broadcast();
}

void cEPGSources::StopThread()
{
   if (Running()) {
      Cancel(-1);
      cwDelay.Signal();
      cvBlock.Broadcast();
      Cancel(10);
   }
}

void cEPGSources::Action()
{  /// fetch external EPG info
   cTimeMs timer;
   uint64_t delay = -1;

   while (Running())
   {
      mtxImport.Lock();
      time_t starttime = time(NULL);
      starttime -= starttime%60;
      bool timerStart = false;
      while (Running() && !(manualStart || (timerStart = ExecuteNow(starttime))))
      {  // give mtxImport back until condition is met, re-gain lock automatically
         bool manual = cvBlock.TimedWait(mtxImport, 60000);
         starttime = time(NULL);
         starttime -= starttime%60;
      }

      if (manualStart || timerStart)
      {
         isImporting = true;
         tsyslog("cEPGSources::Action Start: %s", manualStart?"manualStart":"timerStart");
         cVector<cEPGSource *> sourceList;

         cEPGSearch_Client epgsearch;
         if (!epgsearch.EnableSearchTimers(false))
            esyslog("failed to disable epgsearch searchtimers");

         // updated Episodes DB
         if (XMLTVConfig.UseEpisodes()) {
            cEpisodesDB *episodesDB = new cEpisodesDB();
            if (episodesDB) {
               if (episodesDB->OpenDBConnection()) {
                  episodesDB->UpdateDB();
                  episodesDB->CloseDBConnection();
               }
               delete episodesDB;
            }
         }

         // let each EPGsource fetch its content
         if (manualStart && epgImportSource) {
            if (epgImportSource->Enabled() && epgImportSource->Execute())
               sourceList.Append(epgImportSource);
         }
         else {
            for (cEPGSource *source = First(); source; source = Next(source)) {
               if (source->Enabled() && (manualStart || source->ExecuteNow(starttime))) {
                  if (source->Execute())
                     sourceList.Append(source);
               }
            }
         }

         manualStart = false;
         epgImportSource = NULL;

         bool success = true;
         if (sourceList.Size() > 0)
         {  // append external events if configured
            cXMLTVDB *xmlTVDB = new cXMLTVDB();
            if (xmlTVDB && xmlTVDB->OpenDBConnection())
            {
               int totalSchedules = 0, totalEvents = 0;
               for (int s = 0; s < sourceList.Size(); s++)
               {
                  cEPGSource *source = sourceList.At(s);
                  const cStringList *epgChannelList = source->EpgChannelList();
                  for (int i = 0; i < epgChannelList->Size(); i++) {
                     const char *epgChannelName = epgChannelList->At(i);
                     if (cEPGChannel *epgChannel = XMLTVConfig.EPGChannels()->GetEpgChannel(epgChannelName)) { // has epgChannel
                        if (epgChannel && epgChannel->EPGSource() && epgChannel->EPGSource()->SourceName() && !strcmp(epgChannel->EPGSource()->SourceName(), source->SourceName())) {
                           if ((epgChannel->Flags() & USE_APPEND_EXT_EVENTS) >> SHIFT_APPEND_EXT_EVENTS >= APPEND_EXT_EVENTS) {
                              for (int c = 0; c < epgChannel->ChannelIDList().Size(); c++) {
                                 tChannelID channelID = epgChannel->ChannelIDList().At(c)->GetChannelID();
                                 success =  success && xmlTVDB->AppendEvents(channelID, epgChannel->Flags(), &totalSchedules, &totalEvents);
                              }
                           }
                        }
                     }
                  }
               }
               xmlTVDB->CloseDBConnection();
               delete xmlTVDB;
               isyslog("Appended %d events to %d channels", totalEvents, totalSchedules);
            }
         } // end of Append

         if (success) {
            for (int i = 0; i < sourceList.Size(); i++) {
               sourceList[i]->SetLastSuccessfulRun(starttime);
            }
         }

         if (!epgsearch.EnableSearchTimers(true))
            esyslog("failed to enable epgsearch searchtimers");

         isImporting = false;
         mtxImport.Unlock();

         tsyslog("cEPGSources::Action End");
      }
      //tsyslog("cEPGSources::Action LoopEnd");
      delay = time(NULL) - starttime;
      if (delay > 0 && delay < 60 && Running())
      {  // avoid multiple timer calls
         cwDelay.Wait(delay*1000);
      }
   }
}
