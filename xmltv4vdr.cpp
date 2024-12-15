/*
 * xmltv4vdr.cpp: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/plugin.h>
#include <getopt.h>
#include "setup.h"
#include "debug.h"

#if defined(APIVERSNUM) && APIVERSNUM < 20405
#error THIS PLUGIN REQUIRES AT LEAST VDR 2.4.5
#endif

static const char *VERSION        = "0.4.5-Beta";
static const char *DESCRIPTION    = trNOOP("Imports EPG data in XMLTV format into VDR");

// -------------------------------------------------------------
class cPluginXmltv4vdr : public cPlugin
{
private:
   time_t last_housekeepingtime;
   time_t last_timerchecktime;
   bool deferred;
   cEpgHandlerXMLTV *epghandler;
public:
   cPluginXmltv4vdr(void);
   virtual ~cPluginXmltv4vdr();
   virtual const char *Version(void)     { return VERSION; }
   virtual const char *Description(void) { return tr(DESCRIPTION); }
   virtual const char *CommandLineHelp(void);
   virtual bool ProcessArgs(int argc, char *argv[]);
   virtual bool Initialize(void);
   virtual bool Start(void);
   virtual void Stop(void);
   virtual void Housekeeping(void);
   //virtual void MainThreadHook(void);
   virtual cString Active(void);
   virtual time_t WakeupTime(void);
   //virtual const char *MainMenuEntry(void);
   //virtual cOsdObject *MainMenuAction(void);
   virtual cMenuSetupPage *SetupMenu(void);
   virtual const char **SVDRPHelpPages(void);
   virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode);
};

cPluginXmltv4vdr::cPluginXmltv4vdr(void)
{  // Initialize any member variables here.
   // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
   // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
   last_timerchecktime = 0;
   last_housekeepingtime = time(NULL) - DELTA_HOUSEKEEPINGTIME/2; // start this threads later!
   deferred = false;
}

cPluginXmltv4vdr::~cPluginXmltv4vdr()
{  // Clean up after yourself!
   if (XMLTVConfig.LogFile())
      fclose(XMLTVConfig.LogFile());
}

const char *cPluginXmltv4vdr::CommandLineHelp(void)
{  // Return a string that describes all known command line options.
   return 
   "  -d FILE,  --epgdatabase=FILE     write the EPG database into the given FILE\n"
   "                                   default: <Plugin-CacheDir>/xmltv4vdr_EPG.db\n"
   "  -i DIR,   --images=DIR           location where EPG images are stored\n"
   "                                   default: <Plugin-CacheDir>/epgimages\n"
   "  -s DIR,   --epgsources=DIR       location where EPG Sources are stored\n"
   "                                   default: " EPGSOURCESDIR "\n"
   "  -E FILE,  --episodesdb=FILE      read and writes the Episodes DB from the given SQLite FILE\n"
   "                                   default: none\n"
   "  -e DIR,   --episodes=DIR         location of episode files (UTF-8 format only)\n"
   "                                   default is <Plugin-ResourceDir>/episodes\n"
   "  -h HOST,  --episodesserver=HOST  name of the episode server\n"
   "                                   default: none\n"
   "  -p PORT,  --episodesport=PORT    port of the episode server\n"
   "                                   default: 2006\n"
   "  -l FILE,  --logfile=FILE         write trace logs into the given FILE\n"
   "                                   default: no trace log written\n";
}

bool cPluginXmltv4vdr::ProcessArgs(int argc, char *argv[])
{  // Command line argument processing
   static struct option long_options[] =
   {
      { "epgdatabase",    required_argument, NULL, 'd'},
      { "images",         required_argument, NULL, 'i'},
      { "epgsources",     required_argument, NULL, 's'},
      { "episodesdb",     required_argument, NULL, 'E'},
      { "episodes",       required_argument, NULL, 'e'},
      { "episodesserver", required_argument, NULL, 'h'},
      { "episodesport",   required_argument, NULL, 'p'},
      { "logfile",        required_argument, NULL, 'l'},
      { 0,0,0,0 }
   };

   int c;
   while ((c = getopt_long(argc, argv, "d:i:s:E:e:h:p:l:", long_options, NULL)) != -1)
   {
      switch (c)
      {
         case 'd':
            XMLTVConfig.SetEPGDBFile(optarg);
            break;
         case 'i':
            XMLTVConfig.SetImageDir(optarg);
            break;
         case 's':
            XMLTVConfig.SetEPGSourcesDir(optarg);
            break;
         case 'E':
            XMLTVConfig.SetEpisodesDBFile(optarg);
            break;
         case 'e':
            XMLTVConfig.SetEpisodesDir(optarg);
            break;
         case 'h':
            XMLTVConfig.SetEpisodesServer(optarg);
            break;
         case 'p':
            if (isnumber(optarg))
               XMLTVConfig.SetEpisodesServerPort(atoi(optarg));
            break;
         case 'l':
            XMLTVConfig.SetLogFilename(optarg);
            break;
         default:
            return false;
      }
   }
   return true;
}

bool cPluginXmltv4vdr::Initialize(void)
{  // Initialize any background activities the plugin shall perform.

   // open logfile
   if (!isempty(XMLTVConfig.LogFilename())) {
      FILE *lf = fopen(XMLTVConfig.LogFilename(), "a+");
      if (lf)
         XMLTVConfig.SetLogfile(lf);
   }

   // EPG Sources directory
   isyslog("using EPG sources dir '%s'", XMLTVConfig.EPGSourcesDir());

   // EPG image directory
   if (isempty(XMLTVConfig.ImageDir()))
      XMLTVConfig.SetImageDir(AddDirectory(cPlugin::CacheDirectory(), "epgimages"));
   MakeDirs(XMLTVConfig.ImageDir(), true);
   isyslog("using EPG image directory '%s'", XMLTVConfig.ImageDir());
   if (access(XMLTVConfig.ImageDir(), R_OK|W_OK) == -1) {
      esyslog("Could not access EPG image directory '%s': %d", XMLTVConfig.ImageDir(), errno);
   }

   // EPG database
   if (isempty(XMLTVConfig.EPGDBFile()))
      XMLTVConfig.SetEPGDBFile(AddDirectory(cPlugin::CacheDirectory(PLUGIN_NAME_I18N), EPG_DB_FILENAME));
   isyslog("using EPG database '%s' with SQLite version %s", XMLTVConfig.EPGDBFile(), sqlite3_libversion());
   if (sqlite3_threadsafe() == 0) esyslog("SQLite3 not threadsafe!");

   // Episodes database
   if (isempty(XMLTVConfig.EpisodesDBFile())) {
      isyslog("Episodes support disabled: Parameter --episodesdb for Episodes DB not set");
   }
   else
   {
      isyslog("using Episodes database '%s' with SQLite version %s", XMLTVConfig.EpisodesDBFile(), sqlite3_libversion());
      if (!isempty(XMLTVConfig.EpisodesServer())) {
         isyslog("using Episodes Server: %s:%d for Updates", XMLTVConfig.EpisodesServer(), XMLTVConfig.EpisodesServerPort());
      }
      else
      {  // episode directory
         if (isempty(XMLTVConfig.EpisodesDir()))
            XMLTVConfig.SetEpisodesDir(AddDirectory(cPlugin::ResourceDirectory(PLUGIN_NAME_I18N), "episodes"));
         MakeDirs(XMLTVConfig.EpisodesDir(), true);
         isyslog("using episodes directory '%s' with UTF-8 for Updates", XMLTVConfig.EpisodesDir());
         if (access(XMLTVConfig.EpisodesDir(), R_OK) == -1) {
            esyslog("Could not access episodes directory '%s'", XMLTVConfig.EpisodesDir());
         }
      }
   }

   // read mappings first followed by config
   XMLTVConfig.EPGSources()->ReadAllConfigs();
   XMLTVConfig.Load(AddDirectory(ConfigDirectory(PLUGIN_NAME_I18N), "xmltv4vdr.conf"));

   return true;
}

#ifdef DEBUG
#include <signal.h>

void sig_handler(int sig)
{
   if (sig == SIGABRT || sig == SIGSEGV) {
      isyslog("xmltv4vdr received signal %d %s", sig, sig==6?"SIGABRT":(sig==11?"SIGSEGV":""));
      cBackTrace::BackTrace();
      exit(1);
   }
}
#endif

bool cPluginXmltv4vdr::Start(void)
{  // Start any background activities the plugin shall perform.

   // prepare xmlTV DB
   cXMLTVDB *xmlTVDB = new cXMLTVDB();
   if (!xmlTVDB)
      return false;

   XMLTVConfig.SetDBinitialized(xmlTVDB->UpgradeDB());
   delete xmlTVDB;
   if (!XMLTVConfig.DBinitialized()) {
      esyslog("initializing SQLite DB for xmlTV failed, aborting!");
      return false;
   }
   isyslog("EPG database ready");

   // prepare episodes DB
   if (!isempty(XMLTVConfig.EpisodesDBFile())) {
      cEpisodesDB *episodesDB = new cEpisodesDB();
      if (episodesDB) {
         if (!episodesDB->OpenDBConnection(true)) {
            esyslog("initializing SQLite DB for episodes failed, aborting!");
            return false;
         }
         else {
            episodesDB->CloseDBConnection();
         }
         delete episodesDB;
         isyslog("Episodes DB ready");
      }
   }

   // setup housekeeping
   XMLTVConfig.SetHouseKeeping(new cHouseKeeping());

   // start EPG import thread
   XMLTVConfig.EPGSources()->Start();

   // register new EPG handler to VDR
   epghandler = new cEpgHandlerXMLTV();

#ifdef DEBUG
   if (signal(SIGABRT, sig_handler) == SIG_ERR)
      isyslog("xmltv4vdr: can't catch SIGABRT\n");
   if (signal(SIGSEGV, sig_handler) == SIG_ERR)
      isyslog("xmltv4vdr: can't catch SIGSEGV\n");
#endif

   return true;
}

void cPluginXmltv4vdr::Stop(void)
{  // Stop any background activities the plugin is performing.
   XMLTVConfig.EPGSources()->StopThread();
   delete epghandler;
   XMLTVConfig.HouseKeeping()->StopHousekeeping();
   if (XMLTVConfig.LogFile()) {
      fclose(XMLTVConfig.LogFile());
      XMLTVConfig.SetLogfile(NULL);
   }
}

void cPluginXmltv4vdr::Housekeeping(void)
{  // Perform any cleanup or other regular tasks.
   time_t now = time(NULL);
   if (now >= (last_housekeepingtime + DELTA_HOUSEKEEPINGTIME))
   {
      if (XMLTVConfig.HouseKeeping()) {
         if (XMLTVConfig.HouseKeeping()->StartHousekeeping(HKT_DELETEEXPIREDEVENTS))
            last_housekeepingtime = now;
         else
            last_housekeepingtime += 80;  // try again in 80 sec
      }
      else
         last_housekeepingtime += 120;  // try again in 120 sec
   }
}

cString cPluginXmltv4vdr::Active(void)
{  // Return a message string if shutdown should be postponed
   if (XMLTVConfig.ImportActive() || XMLTVConfig.HouseKeepingActive())
      return tr("xmltv4vdr plugin still busy");

   return NULL;
}

time_t cPluginXmltv4vdr::WakeupTime(void)
{  // Return custom wakeup time for shutdown script
   time_t nextruntime = 0;
   if (XMLTVConfig.WakeUp())
   {
      nextruntime = XMLTVConfig.EPGSources()->NextRunTime();
      if (nextruntime) 
         nextruntime -= (time_t) 180;
      tsyslog("reporting wakeuptime %s", ctime(&nextruntime));
   }
   return nextruntime;
}

cMenuSetupPage *cPluginXmltv4vdr::SetupMenu(void)
{  // Return a setup menu in case the plugin supports one.
   return new cMenuSetupXmltv4vdr();
}

const char **cPluginXmltv4vdr::SVDRPHelpPages(void)
{  // Returns help text
   static const char *HelpPages[]=
   {
      "UPDT [source]\n"
      "    Fetch EPG data from all external sources [or only from given source]\n",
      "UPDE\n"
      "    Update Episodes DB from configured source\n",
      "HOUS\n"
      "    Start housekeeping manually\n",
      "CHEK [FIX]\n"
      "    Start consistency check, optionally with \"FIX\" to delete unused pictures and links\n",
      "CHEK RESULT\n"
      "    Print result of previous CHEK run\n",
      NULL
   };
   return HelpPages;
}

cString cPluginXmltv4vdr::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode)
{  // Process SVDRP commands
   cString output = NULL;
   if (!strcasecmp(Command,"UPDT"))
   {
      if (!XMLTVConfig.EPGSources()->Count())
      {
         ReplyCode = 550;
         output = "no epg sources defined\n";
      }
      else
      {
         if (XMLTVConfig.ImportActive())
         {
            ReplyCode = 550;
            output = "update already in progress\n";
         }
         else
         {
            if (isempty(Option)) {
               XMLTVConfig.EPGSources()->StartImport();
               ReplyCode = 250;
               output = "update started\n";
            }
            else {
               cEPGSource *source = XMLTVConfig.EPGSources()->GetSource(compactspace((char *)Option));
               if (source && source->Enabled()) {
                  XMLTVConfig.EPGSources()->StartImport(source);
                  ReplyCode = 250;
                  output = cString::sprintf("update of source '%s' started\n", Option);
               }
               else {
                  ReplyCode = 550;
                  output = cString::sprintf("source '%s' not defined or disabled\n", Option);
               }
            }
         }
      }
   }

   if (!strcasecmp(Command,"HOUS"))
   {  // delete outdated entries and pictures
      if (XMLTVConfig.HouseKeeping() && XMLTVConfig.HouseKeeping()->StartHousekeeping(HKT_DELETEEXPIREDEVENTS))
      {
         last_housekeepingtime = time(NULL);
         last_housekeepingtime -= last_housekeepingtime % 60;
         ReplyCode = 250;
         output = "housekeeping started";
      }
      else
      {
         ReplyCode = 550;
         output = "import or housekeeping is currently busy";
      }
   }

   if (!strcasecmp(Command,"UPDE"))
   {  // update episodes lists
      cEpisodesDB *episodesDB = new cEpisodesDB();
      if (episodesDB) {
         if (!episodesDB->OpenDBConnection()) {
            ReplyCode = 550;
            output = "failed to open or create Episodes DB";
         }
         else {
            episodesDB->UpdateDB();
            episodesDB->CloseDBConnection();
            ReplyCode = 250;
            output = "Episodes Update finished";
         }
         delete episodesDB;
      }
      else {
         ReplyCode = 550;
         output = "failed to create Episodes DB object";
      }
   }

   if (!strcasecmp(Command,"CHEK"))
   {  // check consistency of DB and pictures
      if (!XMLTVConfig.HouseKeeping()) {
         ReplyCode = 550;
         output = "Error querying housekeeping";
      }
      else
      {
         if (!strcasecmp(Option, "RESULT"))
         {
            ReplyCode = 250;
            output = XMLTVConfig.HouseKeeping()->LastCheckResult();
         }
         else
         {
            if (XMLTVConfig.HouseKeeping()->StartHousekeeping((!isempty(Option) && !strcasecmp(Option, "FIX")) ? HKT_CHECKANDFIXCONSISTENCY : HKT_CHECKCONSISTENCY))
            {
               ReplyCode = 250;
               output = "consistency check started";
            }
            else
            {
               ReplyCode = 550;
               output = "import or housekeeping is currently busy";
            }
         }
      }
   }
   return output;
}

// -------------------------------------------------------------
void logger(cEPGSource *source, char logtype, const char* format, ...)
{
   cString sourceName = "";
   va_list ap;
   va_start(ap, format);
   char *line;
   if (vasprintf(&line, format, ap) == -1) return;
   va_end(ap);

   struct tm tm;
   if (XMLTVConfig.LogFile() || source)
   {
      time_t now = time(NULL);
      localtime_r(&now, &tm);
   }

   if (source) {
      source->Add2Log(&tm, logtype, line);
      sourceName = cString::sprintf("'%s' ", source->SourceName());
   }

   if (XMLTVConfig.LogFile())
   {
      char dt[30];
      strftime(dt, sizeof(dt)-1, "%b %d %H:%M:%S", &tm);

      fprintf(XMLTVConfig.LogFile(), "%s [%i] %s%s\n", dt, cThread::ThreadId(), *sourceName, line);
      fflush(XMLTVConfig.LogFile());
   }

   switch (logtype)
   {
      case 'E':
         if (SysLogLevel > 0) syslog_with_tid(LOG_ERR, "xmltv4vdr: %sERROR %s", *sourceName, line);
         break;
      case 'I':
         if (SysLogLevel > 1) syslog_with_tid(LOG_ERR, "xmltv4vdr: %s%s", *sourceName, line);
         break;
      case 'D':
         if (SysLogLevel > 2) syslog_with_tid(LOG_ERR, "xmltv4vdr: %s%s", *sourceName, line);
         break;
      default:
         break;
   }

   free(line);
}

VDRPLUGINCREATOR(cPluginXmltv4vdr) // Don't touch this!
