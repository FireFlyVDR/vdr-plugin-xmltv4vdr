/*
 * xmltv4vdr.cpp: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "handler.h"
#include "debug.h"

cEpgHandlerXMLTV::cEpgHandlerXMLTV(void)
{  ///< Constructs a new EPG handler and adds it to the list of EPG handlers.
   ///< Whenever an event is received from the EIT data stream, the EPG
   ///< handlers are queried in the order they have been created.
   ///< As soon as one of the EPG handlers returns true in a member function,
   ///< none of the remaining handlers will be queried. If none of the EPG
   ///< handlers returns true in a particular call, the default processing
   ///< will take place.
   ///< EPG handlers will be deleted automatically at the end of the program.

   xmlTVDB = new cXMLTVDB();
   mapping = NULL;
   channelID = tChannelID::InvalidID;
   channelName = NULL;
   flags = USE_NOTHING;
   lastEventStarttime = -1;
}

cEpgHandlerXMLTV::~cEpgHandlerXMLTV(void)
{
   delete xmlTVDB;
}

#ifdef DBG_EPGHANDLER2
static inline double GetTimeMS(void)
{
#ifdef CLOCK_MONOTONIC
    struct timespec tspec;

    clock_gettime(CLOCK_MONOTONIC, &tspec);
    return (tspec.tv_sec * 1000.0 + tspec.tv_nsec / 1000000.0);
#else
    struct timeval tval;

    if (gettimeofday(&tval, NULL) < 0)
        return 0;
    return (tval.tv_sec * 1000.0 + tval.tv_usec / 1000.0);
#endif
}
#endif

bool cEpgHandlerXMLTV::BeginSegmentTransfer(const cChannel *Channel, bool Dummy)
{  ///< Called directly after IgnoreChannel() before any other handler method is called.
   ///< Designed to give handlers the possibility to prepare a database transaction.
   ///< If any EPG handler returns false in this function, it is assumed that
   ///< the EPG for the given Channel has to be handled later due to some transaction problems,
   ///< therefore the processing will be aborted.
   ///< Dummy is for backward compatibility and may be removed in a future version.

   bool success = false;
   channelID = Channel->GetChannelID();
   channelName = Channel->Name();

   if (XMLTVConfig.DBinitialized()) {
      if (!XMLTVConfig.HouseKeepingActive() && !XMLTVConfig.ImportActive())   // not during housekeeping or import from external sources
      {
         cEPGMapping *map = XMLTVConfig.EPGMappings()->GetMap(Channel->GetChannelID());
         if (map && map->EPGSource())     // not if no mapping for this channel exists
         {
            mapping = map;
            flags = mapping->Flags();
            if (mapping->EPGSource()) {
               lastEventStarttime = mapping->EPGSource()->LastEventStarttime();
               lingerTime = time(NULL) - Setup.EPGLinger * 60;
            }
#ifdef DBG_EPGHANDLER2
            tsyslog("BEGIN_SEGMENT %s(%d) (%s) %s (%s)", __FILE__, __LINE__, *Channel->GetChannelID().ToString(), Channel->Name(), XMLTVConfig.EPGMappings()->HandledExternally(Channel->GetChannelID())?"ext":"int");
#endif
            success = xmlTVDB->OpenDBConnection();
            if (success)
            {
#ifdef DBG_EPGHANDLER2
               segmentStarttime = GetTimeMS();
#endif
               success = xmlTVDB->UpdateEventPrepare(*Channel->GetChannelID().ToString(), mapping->EPGSource()->SourceName());
               if (!success) {
                  xmlTVDB->CloseDBConnection();
               }
            }
         }
         else { // no mapping for this channel, let others handle it
            success = true;
         }
#ifdef DBG_EPGHANDLER2
         tsyslog("BEGIN_SEGMENT End   %s(%d) %s", __FILE__, __LINE__, success?"cont":"abort");
#endif
      }
   }
   return success;
}

bool cEpgHandlerXMLTV::EndSegmentTransfer(bool Modified, bool Dummy)
{  ///< Called after the segment data has been processed.
   ///< At this point handlers should close/commit/rollback any pending database transactions.
   /// Modified is true if any modifications to events have been done
   ///< Dummy is for backward compatibility and may be removed in a future version.

   if (mapping) {
      xmlTVDB->UpdateEventFinalize();
      xmlTVDB->CloseDBConnection();
      mapping = NULL;
      lastEventStarttime = -1;
#ifdef DBG_EPGHANDLER2
      double segmentEndtime = GetTimeMS();
      tsyslog("SEGMENT handling time  %4.3f ms %s 0x%06X %s", segmentEndtime-segmentStarttime,
              segmentEndtime-segmentStarttime >=1.0 ? "########":"", flags, *channelName);
#endif
   }
   channelID = tChannelID::InvalidID;
   channelName = NULL;
   flags = USE_NOTHING;
#ifdef DBG_EPGHANDLER2
   tsyslog("END_SEGMENT End   %s(%d) %s", __FILE__, __LINE__, Modified?"modified":"idle");
#endif

   return false;
}

bool cEpgHandlerXMLTV::HandleEvent(cEvent* Event)
{  ///< After all modifications of the Event have been done, the EPG handler
   ///< can take a final look at it.
   // return true if it was handled, otherwise false to allow other Handlers to handle it

   bool handled = false;
   if (mapping && !(channelID == tChannelID::InvalidID)) {
      //NOTE do not use (flags & USE_MASK) in this condition, otherwise for new EIT events with empty flags eventid will not be written into DB and no pictures will be linked
      if (Event && (Event->StartTime() <= lastEventStarttime) && (Event->EndTime() >= lingerTime))
      {  // Event needs to start before last entry of EpgSource and end after current time minus EPGlinger
         handled = xmlTVDB->UpdateEvent(Event, flags, *channelName, mapping->EPGSource()->SourceName(), lastEventStarttime); // search and update/insert event in DB
      }
   }

   return handled;
}

bool cEpgHandlerXMLTV::DropOutdated(cSchedule *Schedule, time_t SegmentStart, time_t SegmentEnd, uchar TableID, uchar Version)
{  ///< Takes a look at all EPG events between SegmentStart and SegmentEnd and
   ///< drops outdated events (latest TableID and Version are provided, not neccessarily events in the past!).

   bool handled = false;
   if (mapping && !(channelID == tChannelID::InvalidID)) {
#ifdef DBG_EPGHANDLER2
      tsyslog("DropOutdated Handler1  %s - %s %2X-%2X %s", *DayDateTime(SegmentStart), *DayDateTime(SegmentEnd), TableID, Version, *Schedule->ChannelID().ToString());
#endif

      handled = xmlTVDB->DropOutdated(Schedule, SegmentStart, SegmentEnd, TableID, Version);
   }
   else
   {
#ifdef DBG_EPGHANDLER2
      cEPGMapping *map = XMLTVConfig.EPGMappings()->GetMap(Schedule->ChannelID());
      if (map && map->EPGSource())
      {  // channel has mapping and an EPGSource is defined
         tsyslog("DropOutdated Handler2  %s - %s %2X-%2X %s", *DayDateTime(SegmentStart), *DayDateTime(SegmentEnd), TableID, Version, *Schedule->ChannelID().ToString());
      }
#endif
   }

   return false; // let other handlers also being executed
}
