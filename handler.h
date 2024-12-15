/*
 * handler.h: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _HANDLER_H
#define _HANDLER_H

#include "maps.h"
#include "database.h"
#include "debug.h"

class cEpgHandlerXMLTV : public cEpgHandler
{
private:
   cEPGChannel *epgChannel;
   cXMLTVDB *xmlTVDB;
   tChannelID channelID;
   cString channelName;
   uint64_t flags;
   time_t sourceLastEventStarttime, epgLingerTime;
#ifdef DBG_EPGHANDLER2
   double segmentStarttime;
#endif
public:
   cEpgHandlerXMLTV(void);
   ~cEpgHandlerXMLTV();

   //virtual bool IgnoreChannel(const cChannel *Channel);
      ///< Before any EIT data for the given Channel is processed, the EPG handlers
      ///< are asked whether this Channel shall be completely ignored. If any of
      ///< the EPG handlers returns true in this function, no EIT data at all will
      ///< be processed for this Channel.
   virtual bool BeginSegmentTransfer(const cChannel *Channel, bool Dummy);
      ///< Called directly after IgnoreChannel() before any other handler method is called.
      ///< Designed to give handlers the possibility to prepare a database transaction.
      ///< If any EPG handler returns false in this function, it is assumed that
      ///< the EPG for the given Channel has to be handled later due to some transaction problems,
      ///< therefore the processing will aborted.
      ///< Dummy is for backward compatibility and may be removed in a future version.
   virtual bool EndSegmentTransfer(bool Modified, bool Dummy);
      ///< Called after the segment data has been processed.
      ///< At this point handlers should close/commit/rollback any pending database transactions.
      ///< Dummy is for backward compatibility and may be removed in a future version.
   virtual bool HandledExternally(const cChannel *Channel);
      ///< If any EPG handler returns true in this function, it is assumed that
      ///< the EPG for the given Channel is handled completely from some external
      ///< source. Incoming EIT data is processed as usual, but any new EPG event
      ///< will not be added to the respective schedule. It's up to the EPG
      ///< handler to take care of this.
   virtual bool IsUpdate(tEventID EventID, time_t StartTime, uchar TableID, uchar Version);
      ///< VDR can't perform the update check (version, tid) for externally handled events,
      ///< therefore the EPG handlers have to take care of this. Otherwise the parsing of
      ///< non-updates will waste a lot of resources.
      ///< only called if handledExternally is set
   //virtual bool SetShortText(cEvent *Event,const char *ShortText);
   //virtual bool SetDescription(cEvent *Event,const char *Description);
   //virtual bool FixEpgBugs(cEvent *Event) { return false; }
      ///< Fixes some known problems with EPG data.
   virtual bool HandleEvent(cEvent *Event);
      ///< After all modifications of the Event have been done, the EPG handler
      ///< can take a final look at it.
   //virtual bool SortSchedule(cSchedule *Schedule);
      ///< Sorts the Schedule after the complete table has been processed.
   virtual bool DropOutdated(cSchedule *Schedule, time_t SegmentStart, time_t SegmentEnd, uchar TableID, uchar Version);
      ///< Takes a look at all EPG events between SegmentStart and SegmentEnd and
      ///< drops outdated events (latest TableID and Version are provided, not neccessarily events in the past!).
};

#endif
