/*
 *      Copyright (C) 2013 const86
 *      http://github.com/afedchin/xbmc-addon-p2pproxy/
 *
 *      Copyright (C) 2011 Pulse-Eight
 *      http://www.pulse-eight.com/
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <sstream>
#include <string>
#include <fstream>
#include <map>
#include "zlib.h"
#include "rapidxml/rapidxml.hpp"
#include "PVRIptvData.h"

#define M3U_START_MARKER        "#EXTM3U"
#define M3U_INFO_MARKER         "#EXTINF"
#define TVG_INFO_ID_MARKER      "tvg-id="
#define TVG_INFO_NAME_MARKER    "tvg-name="
#define TVG_INFO_LOGO_MARKER    "tvg-logo="
#define TVG_INFO_SHIFT_MARKER   "tvg-shift="
#define GROUP_NAME_MARKER       "group-title="
#define RADIO_MARKER            "radio="
#define CHANNEL_LOGO_EXTENSION  ".png"
#define SECONDS_IN_DAY          86400

using namespace std;
using namespace ADDON;
using namespace rapidxml;

template<class Ch>
inline bool GetNodeValue(const xml_node<Ch> * pRootNode, const char* strTag, CStdString& strStringValue)
{
  xml_node<Ch> *pChildNode = pRootNode->first_node(strTag);
  if (pChildNode == NULL)
  {
    return false;
  }
  strStringValue = pChildNode->value();
  return true;
}

template<class Ch>
inline bool GetAttributeValue(const xml_node<Ch> * pNode, const char* strAttributeName, CStdString& strStringValue)
{
  xml_attribute<Ch> *pAttribute = pNode->first_attribute(strAttributeName);
  if (pAttribute == NULL)
  {
    return false;
  }
  strStringValue = pAttribute->value();
  return true;
}

PVRIptvData::PVRIptvData(void)
{
  m_strServerUrl     = g_ServerURL;
  m_iEPGTimeShift = g_iEPGTimeShift;
  m_bTSOverride   = g_bTSOverride;
  m_iLastStart    = 0;
  m_iLastEnd      = 0;

  m_bEGPLoaded = false;

  if (LoadPlayList())
  {
    XBMC->QueueNotification(QUEUE_INFO, "%d channels loaded.", m_channels.size());
  }
}

void *PVRIptvData::Process(void)
{
  return NULL;
}

PVRIptvData::~PVRIptvData(void)
{
  m_channels.clear();
  m_groups.clear();
  m_epg.clear();
}

bool PVRIptvData::LoadEPG(time_t iStart, time_t iEnd)
{
	if (m_strServerUrl.IsEmpty())
	{
		XBMC->Log(LOG_NOTICE, "EPG file path is not configured. EPG not loaded.");
		m_bEGPLoaded = true;
		return false;
	}

	std::string data;
	std::string decompressed;

	int iCount = 0;
	CStdString strReq;
	strReq.append(m_strServerUrl);
	strReq.append(P2P_PLUGIN_NAME);
	strReq.append("/");
	strReq.append(TVG_FILE_NAME);
	if (!GetFileContents(strReq, data))
	{
		XBMC->Log(LOG_ERROR, "Unable to load EPG file '%s':  file is missing or empty. :%dth try.", m_strServerUrl.c_str(), ++iCount);
		return false;
	}

	char * buffer = &(data[0]);

	xml_document<> xmlDoc;
	try
	{
		xmlDoc.parse<0>(buffer);
	}
	catch (parse_error p)
	{
		XBMC->Log(LOG_ERROR, "Unable parse EPG XML: %s", p.what());
		m_bEGPLoaded = true;
		return false;
	}

	xml_node<> *pRootElement = xmlDoc.first_node("programms");
	if (!pRootElement)
	{
		XBMC->Log(LOG_ERROR, "Invalid EPG XML: no <tv> tag found");
		m_bEGPLoaded = true;
		return false;
	}

	if (m_epg.size() > 0)
	{
		m_epg.clear();
	}

	int iBroadCastId = 0;
	xml_node<> *pChannelNode = NULL;
	xml_node<> *pEpgNode = NULL;

	CStdString strEmpty = "";
	for(pChannelNode = pRootElement->first_node("channel"); pChannelNode; pChannelNode = pChannelNode->next_sibling("channel"))
	{
		CStdString strName;
		CStdString strId;
		PVRIptvEpgChannel epg;
		GetAttributeValue(pChannelNode, "id", strId);
		GetAttributeValue(pChannelNode, "name", strName);
		epg.strId = strId;
		epg.strName = strName;

		for (pEpgNode = pChannelNode->first_node("program"); pEpgNode; pEpgNode = pEpgNode->next_sibling("program"))
		{
			CStdString strStart;
			CStdString strStop;

			if (!GetAttributeValue(pEpgNode, "start", strStart) || !GetAttributeValue(pEpgNode, "stop", strStop))
			{
				continue;
			}

			int iTmpStart = ParseDateTime(strStart);
			int iTmpEnd = ParseDateTime(strStop);

			if ((iTmpEnd + g_iEPGTimeShift < iStart) || (iTmpStart + g_iEPGTimeShift > iEnd))
			{
				continue;
			}

			CStdString strTitle;
			CStdString strCategory;
			CStdString strDesc;

			GetAttributeValue(pEpgNode, "title", strTitle);
			GetAttributeValue(pEpgNode, "category", strCategory);
			GetNodeValue(pEpgNode, "description", strDesc);

			CStdString strIconPath;
			xml_node<> *pIconNode = pEpgNode->first_node("icon");
			if (pIconNode != NULL)
			{
				if (!GetAttributeValue(pIconNode, "src", strIconPath))
				{
					strIconPath = "";
				}
			}

			PVRIptvEpgEntry entry;
			entry.iBroadcastId = ++iBroadCastId;
			entry.iGenreType = 0;
			entry.iGenreSubType = 0;
			entry.strTitle = strTitle;
			entry.strPlot = strDesc;
			entry.strPlotOutline = strDesc;
			entry.strIconPath = strIconPath;
			entry.startTime = iTmpStart;
			entry.endTime = iTmpEnd;
			entry.strGenreString = strCategory;

			epg.epg.push_back(entry);
		}
		m_epg.push_back(epg);
	}

	xmlDoc.clear();
	m_bEGPLoaded = true;

	XBMC->Log(LOG_NOTICE, "EPG Loaded.");

	return true;
}

bool PVRIptvData::LoadPlayList(void) 
{
  if (m_strServerUrl.IsEmpty())
  {
    XBMC->Log(LOG_NOTICE, "P2pProxy Server URL is not configured. Channels not loaded.");
    return false;
  }

  CStdString strPlaylistContent;
  CStdString strReq;
  strReq.append(m_strServerUrl);
  strReq.append(P2P_PLUGIN_NAME);
  strReq.append("/");
  strReq.append(PLAYLIST_FILE_NAME);
  if (!GetFileContents(strReq, strPlaylistContent))
  {
	  XBMC->Log(LOG_ERROR, "Unable to load playlist file '%s'", m_strServerUrl.c_str());
    return false;
  }

  std::string buf = strPlaylistContent.c_str();
  
  char *buffer = &(buf[0]);
  xml_document<> xmlDoc;
  try
  {
	  xmlDoc.parse<0>(buffer);
  }
  catch (parse_error p)
  {
	  XBMC->Log(LOG_ERROR, "Unable parse channels XML: %s", p.what());
	  return false;
  }

  xml_node<> *pRootElement = xmlDoc.first_node("result");
  if (!pRootElement)
  {
    XBMC->Log(LOG_ERROR, "Invalid Playlist XML: no <result> tag found");
    return false;
  }

  xml_node<> *success = pRootElement->first_node("state")->first_node("success");
  if (success->value()[0] == '0')
  {
	  XBMC->Log(LOG_ERROR, "Error get playlist: %s", pRootElement->first_node("error")->value());
	  return false;
  }

  xml_node<> *xcategories = pRootElement->first_node("categories");
  xml_node<> *pCategoryNode = NULL;
  for (pCategoryNode = xcategories->first_node("category"); pCategoryNode; pCategoryNode = pCategoryNode->next_sibling("category"))
  {
	    CStdString strName;
	    CStdString strId;
		if (!GetAttributeValue(pCategoryNode, "id", strId))
	    {
	      continue;
	    }
		GetAttributeValue(pCategoryNode, "name", strName);

	    if (FindGroup(strName) != NULL)
	    {
	      continue;
	    }

	    PVRIptvChannelGroup group;
		group.iGroupId = atoi(strId.c_str());
		group.bRadio = false;
		group.strGroupName = strName;
		m_groups.push_back(group);
  }

  xml_node<> *xchannels = pRootElement->first_node("channels");
  xml_node<> *pChannelNode = NULL;
  int i = 0;
  for (pChannelNode = xchannels->first_node("channel"); pChannelNode; pChannelNode = pChannelNode->next_sibling("channel"))
  {
	  PVRIptvChannel channel;
	  channel.bRadio = false;
	  channel.iChannelNumber = ++i;
	  CStdString id;
	  if (!GetAttributeValue(pChannelNode, "id", id))
		  continue;
	  channel.iUniqueId = atoi(id);
	  CStdString buf;
	  if (GetAttributeValue(pChannelNode, "epg_id", buf))
	  {
		  if (buf.IsEmpty() || buf[0] == '0')
		  {
			  channel.strTvgId = atoi(buf);
		  }
	  }

	  GetAttributeValue(pChannelNode, "name", buf);
	  channel.strChannelName = buf;
	  channel.strTvgName = buf;
	  channel.strTvgLogo = "http://torrent-tv.ru/uploads/";
	  GetAttributeValue(pChannelNode, "logo", buf);
	  channel.strTvgLogo.append(buf);
	  channel.strLogoPath = channel.strTvgLogo;
	  channel.strStreamURL = m_strServerUrl + "channels/play?id=" + id;
	  channel.iEncryptionSystem = 0;
	  if (!GetAttributeValue(pChannelNode, "group", buf))
	  {
		  continue;
	  }
	  int group = atoi(buf);
	  if (group > 0)
	  {
		  channel.bRadio = m_groups.at(group - 1).bRadio;
		  m_groups.at(group - 1).members.push_back(channel.iChannelNumber-1);
	  }

	  m_channels.push_back(channel);
  }

  if (m_channels.size() == 0)
  {
	  XBMC->Log(LOG_ERROR, "Unable to load channels from server '%s'", m_strServerUrl.c_str());
  }

  XBMC->Log(LOG_NOTICE, "Loaded %d channels.", m_channels.size());
  return true;
}

int PVRIptvData::GetChannelsAmount(void)
{
  return m_channels.size();
}

PVR_ERROR PVRIptvData::GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
  {
    PVRIptvChannel &channel = m_channels.at(iChannelPtr);
    if (channel.bRadio == bRadio)
    {
      PVR_CHANNEL xbmcChannel;
      memset(&xbmcChannel, 0, sizeof(PVR_CHANNEL));

      xbmcChannel.iUniqueId         = channel.iUniqueId;
      xbmcChannel.bIsRadio          = channel.bRadio;
      xbmcChannel.iChannelNumber    = channel.iChannelNumber;
      strncpy(xbmcChannel.strChannelName, channel.strChannelName.c_str(), sizeof(xbmcChannel.strChannelName) - 1);
      strncpy(xbmcChannel.strStreamURL, channel.strStreamURL.c_str(), sizeof(xbmcChannel.strStreamURL) - 1);
      xbmcChannel.iEncryptionSystem = channel.iEncryptionSystem;
      strncpy(xbmcChannel.strIconPath, channel.strLogoPath.c_str(), sizeof(xbmcChannel.strIconPath) - 1);
      xbmcChannel.bIsHidden         = false;

      PVR->TransferChannelEntry(handle, &xbmcChannel);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

bool PVRIptvData::GetChannel(const PVR_CHANNEL &channel, PVRIptvChannel &myChannel)
{
  for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
  {
    PVRIptvChannel &thisChannel = m_channels.at(iChannelPtr);
    if (thisChannel.iUniqueId == (int) channel.iUniqueId)
    {
      myChannel.iUniqueId         = thisChannel.iUniqueId;
      myChannel.bRadio            = thisChannel.bRadio;
      myChannel.iChannelNumber    = thisChannel.iChannelNumber;
      myChannel.iEncryptionSystem = thisChannel.iEncryptionSystem;
      myChannel.strChannelName    = thisChannel.strChannelName;
      myChannel.strLogoPath       = thisChannel.strLogoPath;
      myChannel.strStreamURL      = thisChannel.strStreamURL;

      return true;
    }
  }

  return false;
}

int PVRIptvData::GetChannelGroupsAmount(void)
{
  return m_groups.size();
}

PVR_ERROR PVRIptvData::GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
  for (unsigned int iGroupPtr = 0; iGroupPtr < m_groups.size(); iGroupPtr++)
  {
    PVRIptvChannelGroup &group = m_groups.at(iGroupPtr);
    if (group.bRadio == bRadio)
    {
      PVR_CHANNEL_GROUP xbmcGroup;
      memset(&xbmcGroup, 0, sizeof(PVR_CHANNEL_GROUP));

      xbmcGroup.bIsRadio = bRadio;
      strncpy(xbmcGroup.strGroupName, group.strGroupName.c_str(), sizeof(xbmcGroup.strGroupName) - 1);

      PVR->TransferChannelGroup(handle, &xbmcGroup);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVRIptvData::GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
  PVRIptvChannelGroup *myGroup;
  if ((myGroup = FindGroup(group.strGroupName)) != NULL)
  {
    for (unsigned int iPtr = 0; iPtr < myGroup->members.size(); iPtr++)
    {
      int iIndex = myGroup->members.at(iPtr);
      if (iIndex < 0 || iIndex >= (int) m_channels.size())
        continue;

      PVRIptvChannel &channel = m_channels.at(iIndex);
      PVR_CHANNEL_GROUP_MEMBER xbmcGroupMember;
      memset(&xbmcGroupMember, 0, sizeof(PVR_CHANNEL_GROUP_MEMBER));

      strncpy(xbmcGroupMember.strGroupName, group.strGroupName, sizeof(xbmcGroupMember.strGroupName) - 1);
      xbmcGroupMember.iChannelUniqueId = channel.iUniqueId;
      xbmcGroupMember.iChannelNumber   = channel.iChannelNumber;

      PVR->TransferChannelGroupMember(handle, &xbmcGroupMember);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVRIptvData::GetRecordings(ADDON_HANDLE handle)
{
	if (!LoadRecordings())
	{
		XBMC->Log(LOG_ERROR, "Unable to load records file '%s'", m_strServerUrl.c_str());
		return PVR_ERROR_SERVER_ERROR;
	}

	for (int i = 0; i < m_rec.size(); i++)
	{
		if (m_rec[i].state == PVR_TIMER_STATE_COMPLETED)
			PVR->TransferRecordingEntry(handle, &m_rec[i].rec);
	}

	return PVR_ERROR_NO_ERROR;
}

int PVRIptvData::GetRecordingAmount(void)
{
	int c = 0;
	for (int i = 0; i < m_rec.size(); i++)
	{
		if (m_rec[i].state == PVR_TIMER_STATE_COMPLETED)
			c++;
	}
	return c;
}

PVR_ERROR PVRIptvData::GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  vector<PVRIptvChannel>::iterator myChannel;
  for (myChannel = m_channels.begin(); myChannel < m_channels.end(); myChannel++)
  {
    if (myChannel->iUniqueId != (int) channel.iUniqueId)
      continue;

    if (!m_bEGPLoaded || iStart > m_iLastStart || iEnd > m_iLastEnd) 
    {
      if (LoadEPG(iStart, iEnd))
      {
        m_iLastStart = iStart;
        m_iLastEnd = iEnd;
      }
    }

    PVRIptvEpgChannel *epg;
    if ((epg = FindEpgForChannel(*myChannel)) == NULL || epg->epg.size() == 0)
    {
      return PVR_ERROR_NO_ERROR;
    }

    int iShift = m_bTSOverride ? m_iEPGTimeShift : myChannel->iTvgShift + m_iEPGTimeShift;

    vector<PVRIptvEpgEntry>::iterator myTag;
    for (myTag = epg->epg.begin(); myTag < epg->epg.end(); myTag++)
    {
      if ((myTag->endTime + iShift) < iStart) 
        continue;

      EPG_TAG tag;
      memset(&tag, 0, sizeof(EPG_TAG));

      tag.iUniqueBroadcastId  = myTag->iBroadcastId;
      tag.strTitle            = myTag->strTitle.c_str();
      tag.iChannelNumber      = myTag->iChannelId;
      tag.startTime           = myTag->startTime + iShift;
      tag.endTime             = myTag->endTime + iShift;
      tag.strPlotOutline      = myTag->strPlotOutline.c_str();
      tag.strPlot             = myTag->strPlot.c_str();
      tag.strIconPath         = myTag->strIconPath.c_str();
      tag.iGenreType          = EPG_GENRE_USE_STRING;        //myTag.iGenreType;
      tag.iGenreSubType       = 0;                           //myTag.iGenreSubType;
      tag.strGenreDescription = myTag->strGenreString.c_str();

      PVR->TransferEpgEntry(handle, &tag);

      if ((myTag->startTime + iShift) > iEnd)
        break;
    }

    return PVR_ERROR_NO_ERROR;
  }

  return PVR_ERROR_NO_ERROR;
}

int PVRIptvData::GetFileContents(CStdString& url, std::string &strContent)
{
  strContent.clear();
  void* fileHandle = XBMC->OpenFile(url.c_str(), 0);
  if (fileHandle)
  {
    char buffer[1024];
    while (int bytesRead = XBMC->ReadFile(fileHandle, buffer, 1024))
      strContent.append(buffer, bytesRead);
    XBMC->CloseFile(fileHandle);
  }

  return strContent.length();
}

int PVRIptvData::ParseDateTime(CStdString strDate, bool iDateFormat)
{
  struct tm timeinfo;
  memset(&timeinfo, 0, sizeof(tm));

  if (iDateFormat)
  {
	  sscanf(strDate, "%02d%02d%04d%02d%02d", &timeinfo.tm_mday, &timeinfo.tm_mon, &timeinfo.tm_year, &timeinfo.tm_hour, &timeinfo.tm_min);
  }
  else
  {
    sscanf(strDate, "%02d.%02d.%04d%02d:%02d", &timeinfo.tm_mday, &timeinfo.tm_mon, &timeinfo.tm_year, &timeinfo.tm_hour, &timeinfo.tm_min);
  }

  timeinfo.tm_mon  -= 1;
  timeinfo.tm_year -= 1900;
  timeinfo.tm_isdst = -1;

  return mktime(&timeinfo);
}

CStdString PVRIptvData::DateTimeToString(time_t time)
{
	char buffer[16];
	struct tm * timeinfo;
	timeinfo = localtime(&time);
	strftime(buffer, 16, "%d%m%Y_%H%M%S", timeinfo);
	CStdString result = "";
	result.append(buffer);
	return result;
}

PVRIptvChannel * PVRIptvData::FindChannel(const std::string &strId, const std::string &strName)
{
  CStdString strTvgName = strName;
  strTvgName.Replace(' ', '_');

  vector<PVRIptvChannel>::iterator it;
  for(it = m_channels.begin(); it < m_channels.end(); it++)
  {
    if (it->strTvgId == strId)
    {
      return &*it;
    }
    if (strTvgName == "") 
    {
      continue;
    }
    if (it->strTvgName == strTvgName)
    {
      return &*it;
    }
    if (it->strChannelName == strName)
    {
      return &*it;
    }
  }

  return NULL;
}

PVRIptvChannelGroup * PVRIptvData::FindGroup(const std::string &strName)
{
  vector<PVRIptvChannelGroup>::iterator it;
  for(it = m_groups.begin(); it < m_groups.end(); it++)
  {
    if (it->strGroupName == strName)
    {
      return &*it;
    }
  }

  return NULL;
}

PVRIptvEpgChannel * PVRIptvData::FindEpg(const std::string &strId)
{
  vector<PVRIptvEpgChannel>::iterator it;
  for(it = m_epg.begin(); it < m_epg.end(); it++)
  {
    if (it->strId == strId)
    {
      return &*it;
    }
  }

  return NULL;
}

PVRIptvEpgChannel * PVRIptvData::FindEpgForChannel(PVRIptvChannel &channel)
{
  vector<PVRIptvEpgChannel>::iterator it;
  for(it = m_epg.begin(); it < m_epg.end(); it++)
  {
    if (it->strId == channel.strTvgId)
    {
      return &*it;
    }
    CStdString strName = it->strName;
    strName.Replace(' ', '_');
    if (strName == channel.strTvgName
      || it->strName == channel.strTvgName)
    {
      return &*it;
    }
    if (it->strName == channel.strChannelName)
    {
      return &*it;
    }
  }

  return NULL;
}

/*
 * This method uses zlib to decompress a gzipped file in memory.
 * Author: Andrew Lim Chong Liang
 * http://windrealm.org
 */
bool PVRIptvData::GzipInflate( const std::string& compressedBytes, std::string& uncompressedBytes ) {  

#define HANDLE_CALL_ZLIB(status) {   \
  if(status != Z_OK) {        \
    free(uncomp);             \
    return false;             \
  }                           \
}

  if ( compressedBytes.size() == 0 ) 
  {  
    uncompressedBytes = compressedBytes ;  
    return true ;  
  }  
  
  uncompressedBytes.clear() ;  
  
  unsigned full_length = compressedBytes.size() ;  
  unsigned half_length = compressedBytes.size() / 2;  
  
  unsigned uncompLength = full_length ;  
  char* uncomp = (char*) calloc( sizeof(char), uncompLength );  
  
  z_stream strm;  
  strm.next_in = (Bytef *) compressedBytes.c_str();  
  strm.avail_in = compressedBytes.size() ;  
  strm.total_out = 0;  
  strm.zalloc = Z_NULL;  
  strm.zfree = Z_NULL;  
  
  bool done = false ;  
  
  HANDLE_CALL_ZLIB(inflateInit2(&strm, (16+MAX_WBITS)));
  
  while (!done) 
  {  
    // If our output buffer is too small  
    if (strm.total_out >= uncompLength ) 
    {
      // Increase size of output buffer  
      uncomp = (char *) realloc(uncomp, uncompLength + half_length);
      if (uncomp == NULL)
        return false;
      uncompLength += half_length ;  
    }  
  
    strm.next_out = (Bytef *) (uncomp + strm.total_out);  
    strm.avail_out = uncompLength - strm.total_out;  
  
    // Inflate another chunk.  
    int err = inflate (&strm, Z_SYNC_FLUSH);  
    if (err == Z_STREAM_END) 
      done = true;  
    else if (err != Z_OK)  
    {  
      break;  
    }  
  }  
  
  HANDLE_CALL_ZLIB(inflateEnd (&strm));
  
  for ( size_t i=0; i<strm.total_out; ++i ) 
  {  
    uncompressedBytes += uncomp[ i ];  
  }  

  free( uncomp );  
  return true ;  
}  

int PVRIptvData::GetCachedFileContents(const std::string &strCachedName, const std::string &filePath, 
                                       std::string &strContents, const bool bUseCache /* false */)
{
  bool bNeedReload = false;
  CStdString strCachedPath = GetUserFilePath(strCachedName);
  CStdString strFilePath = filePath;

  // check cached file is exists
  if (bUseCache && XBMC->FileExists(strCachedPath, false)) 
  {
    struct __stat64 statCached;
    struct __stat64 statOrig;

    XBMC->StatFile(strCachedPath, &statCached);
    XBMC->StatFile(strFilePath, &statOrig);

    bNeedReload = statCached.st_mtime < statOrig.st_mtime || statOrig.st_mtime == 0;
  } 
  else 
  {
    bNeedReload = true;
  }

  if (bNeedReload) 
  {
    GetFileContents(strFilePath, strContents);

    // write to cache
    if (bUseCache && strContents.length() > 0) 
    {
      void* fileHandle = XBMC->OpenFileForWrite(strCachedPath, true);
      if (fileHandle)
      {
        XBMC->WriteFile(fileHandle, strContents.c_str(), strContents.length());
        XBMC->CloseFile(fileHandle);
      }
    }
    return strContents.length();
  } 

  return GetFileContents(strCachedPath, strContents);
}

void PVRIptvData::ReaplyChannelsLogos(const char * strNewPath)
{
  if (strlen(strNewPath) > 0)
  {
    //ApplyChannelsLogos();

    PVR->TriggerChannelUpdate();
    PVR->TriggerChannelGroupsUpdate();
  }
}

CStdString PVRIptvData::ReadMarkerValue(std::string &strLine, const char* strMarkerName)
{
  int iMarkerStart = (int) strLine.find(strMarkerName);
  if (iMarkerStart >= 0)
  {
    std::string strMarker = strMarkerName;
    iMarkerStart += strMarker.length();
    if (iMarkerStart < (int)strLine.length())
    {
      char cFind = ' ';
      if (strLine[iMarkerStart] == '"')
      {
        cFind = '"';
        iMarkerStart++;
      }
      int iMarkerEnd = (int)strLine.find(cFind, iMarkerStart);
      if (iMarkerEnd < 0)
      {
        iMarkerEnd = strLine.length();
      }
      return strLine.substr(iMarkerStart, iMarkerEnd - iMarkerStart);
    }
  }

  return std::string("");
}

int PVRIptvData::GetChannelId(const char * strChannelName, const char * strStreamUrl) 
{
  std::string concat(strChannelName);
  concat.append(strStreamUrl);

  const char* strString = concat.c_str();
  int iId = 0;
  int c;
  while (c = *strString++)
    iId = ((iId << 5) + iId) + c; /* iId * 33 + c */

  return abs(iId);
}

bool PVRIptvData::LoadRecordings(void)
{
	m_rec.clear();
	if (m_strServerUrl.IsEmpty())
	{
		XBMC->Log(LOG_NOTICE, "P2pProxy Server URL is not configured. Channels not loaded.");
		return false;
	}

	CStdString strRecordContent;
	CStdString strReq;
	strReq.append(m_strServerUrl);
	strReq.append(P2P_PLUGIN_NAME);
	strReq.append("/");
	strReq.append(RECORD_FILE_NAME);
	if (!GetFileContents(strReq, strRecordContent))
	{
		XBMC->Log(LOG_ERROR, "Unable to load records file '%s'", m_strServerUrl.c_str());
		return false;
	}

	std::string buf = strRecordContent.c_str();
	char *buffer = &(buf[0]);

	xml_document<> xmlDoc;
	try
	{
		xmlDoc.parse<0>(buffer);
	}
	catch (parse_error p)
	{
		XBMC->Log(LOG_ERROR, "Unable parse records XML: %s", p.what());
		return false;
	}

	xml_node<> *pRootElement = xmlDoc.first_node("records");
	if (!pRootElement)
	{
		XBMC->Log(LOG_ERROR, "Invalid records XML: no <result> tag found");
		return false;
	}

	xml_node<> *pRecordNode = NULL;
	int i = 0;
	std::vector<PVR_RECORDING> recs;
	for (pRecordNode = pRootElement->first_node("record"); pRecordNode; pRecordNode = pRecordNode->next_sibling("record"))
	{
		PVR_RECORDING rec;
		memset(&rec, 0, sizeof(PVR_RECORDING));

		CStdString buf;

		GetAttributeValue(pRecordNode, "id", buf);
		strncpy(rec.strRecordingId, buf.c_str(), sizeof(rec.strRecordingId) - 1);
		strncpy(rec.strDirectory, buf, sizeof(rec.strDirectory) - 1);

		GetAttributeValue(pRecordNode, "title", buf);
		strncpy(rec.strTitle, buf.c_str(), sizeof(rec.strRecordingId) - 1);

		GetAttributeValue(pRecordNode, "url", buf);
		strncpy(rec.strStreamURL, buf.c_str(), sizeof(rec.strStreamURL) - 1);

		GetAttributeValue(pRecordNode, "start", buf);
		int iTmpStart = ParseDateTime(buf);
		rec.recordingTime = iTmpStart;

		GetAttributeValue(pRecordNode, "duration", buf);
		int duration = atoi(buf);
		rec.iDuration = duration;

		PVRIptvRecording xRec;
		xRec.rec = rec;

		GetAttributeValue(pRecordNode, "channel_id", buf);
		xRec.iChannelUid = atoi(buf);
		
		GetAttributeValue(pRecordNode, "status", buf);
		if (buf.Equals("Init"))
			xRec.state = PVR_TIMER_STATE_NEW;
		else if (buf.Equals("Wait"))
			xRec.state = PVR_TIMER_STATE_SCHEDULED;
		else if (buf.Equals("Start") || buf.Equals("Starting"))
			xRec.state = PVR_TIMER_STATE_RECORDING;
		else if (buf.Equals("Finished"))
			xRec.state = PVR_TIMER_STATE_COMPLETED;
		else if (buf.Equals("Error"))
			xRec.state = PVR_TIMER_STATE_ERROR;
		else
			xRec.state = PVR_TIMER_STATE_SCHEDULED;

		m_rec.push_back(xRec);
	}
}

PVR_ERROR PVRIptvData::GetTimers(ADDON_HANDLE handle)
{
	if (!LoadRecordings())
	{
		XBMC->Log(LOG_ERROR, "Unable to load records file '%s'", m_strServerUrl.c_str());
		return PVR_ERROR_SERVER_ERROR;
	}

	bool needUpdate = false;
	time_t now;
	time(&now);
	for (int i = 0; i < m_rec.size(); i++)
	{
		if (m_rec[i].state == PVR_TIMER_STATE_COMPLETED)
			continue;
		if (m_rec[i].state == PVR_TIMER_STATE_SCHEDULED && m_rec[i].rec.recordingTime < now)
			needUpdate = true;
		PVR_TIMER timer;
		timer.iClientIndex = i;
		timer.iClientChannelUid = m_rec[i].iChannelUid;
		timer.startTime = m_rec[i].rec.recordingTime;
		timer.endTime = m_rec[i].rec.recordingTime + m_rec[i].rec.iDuration;
		timer.state = m_rec[i].state;
		strncpy(timer.strDirectory, m_rec[i].rec.strDirectory, sizeof(timer.strDirectory) - 1);
		strncpy(timer.strTitle, m_rec[i].rec.strTitle, sizeof(timer.strTitle) - 1);

		PVR->TransferTimerEntry(handle, &timer);
	}
	if (needUpdate)
	{
		PVR->TriggerRecordingUpdate();
		PVR->TriggerTimerUpdate();
	}

	return PVR_ERROR_NO_ERROR;
}

int PVRIptvData::GetTimersAmount()
{
	int c = 0;
	for (int i = 0; i < m_rec.size(); i++)
	{
		if (m_rec[i].state == PVR_TIMER_STATE_COMPLETED)
			continue;
		c++;
	}
	return c;
}

string to_string(int val)
{
	char buf[2 * 32];
	snprintf(buf, sizeof(buf), "%d", val);
	return (string(buf));
}

PVR_ERROR PVRIptvData::AddTimer(const PVR_TIMER &timer)
{
	if (m_strServerUrl.IsEmpty())
	{
		XBMC->Log(LOG_NOTICE, "P2pProxy Server URL is not configured. Channels not loaded.");
		return PVR_ERROR_FAILED;
	}

	CStdString request;
	request.append(m_strServerUrl);
	request.append("records/add?");
	request.append("channel_id=");
	
	//request.append(std::to_string((_Longlong)timer.iClientChannelUid));
	request.append(to_string(timer.iClientChannelUid));
	request.append("&start=");
	request.append(DateTimeToString(timer.startTime));
	request.append("&end=");
	request.append(DateTimeToString(timer.endTime));
	request.append("&name=");
	request.append(url_encode(timer.strTitle));
	
	CStdString strRecordContent;
	if (!GetFileContents(request, strRecordContent))
	{
		XBMC->Log(LOG_ERROR, "Unable to add records '%s'", m_strServerUrl.c_str());
		return PVR_ERROR_FAILED;
	}
	if (!strRecordContent.Equals("OK"))
	{
		XBMC->Log(LOG_ERROR, "Unable to add records '%s'", request.c_str());
		return PVR_ERROR_UNKNOWN;
	}
	PVR->TriggerTimerUpdate();
	return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVRIptvData::UpdateTimer(const PVR_TIMER &timer)
{
	PVRIptvRecording rec;
	bool finded = false;
	CStdString strRecordContent;

	if (!LoadRecordings())
	{
		XBMC->Log(LOG_ERROR, "Unable to load records file '%s'", m_strServerUrl.c_str());
		return PVR_ERROR_SERVER_ERROR;
	}

	for (int i = 0; i < m_rec.size(); i++)
	{
		CStdString recId = m_rec[i].rec.strRecordingId;
		if (recId.Equals(timer.strDirectory))
		{
			rec = m_rec[i];
			finded = true;
			break;
		}
	}
	if (!finded)
	{
		PVR->TriggerRecordingUpdate();
		PVR->TriggerTimerUpdate();
		return PVR_ERROR_INVALID_PARAMETERS;
	}
		
	if (rec.state == PVR_TIMER_STATE_RECORDING)
	{
		if (timer.state == PVR_TIMER_STATE_CANCELLED)
		{
			PVR->TriggerRecordingUpdate();
			PVR->TriggerTimerUpdate();
			DeleteTimer(timer);
			return PVR_ERROR_NO_ERROR;
		}
		return PVR_ERROR_NOT_IMPLEMENTED;
	}
	else
	{
		PVR->TriggerRecordingUpdate();
		PVR->TriggerTimerUpdate();
		return PVR_ERROR_NOT_IMPLEMENTED;
	}
}

PVR_ERROR PVRIptvData::DeleteTimer(const PVR_TIMER &timer)
{
	if (m_strServerUrl.IsEmpty())
	{
		XBMC->Log(LOG_NOTICE, "P2pProxy Server URL is not configured. Channels not loaded.");
		return PVR_ERROR_FAILED;
	}

	if (!LoadRecordings())
	{
		XBMC->Log(LOG_ERROR, "Unable to load records file '%s'", m_strServerUrl.c_str());
		return PVR_ERROR_SERVER_ERROR;
	}

	PVRIptvRecording rec;
	bool finded;

	for (int i = 0; i < m_rec.size(); i++)
	{
		CStdString recId = m_rec[i].rec.strRecordingId;
		if (recId.Equals(timer.strDirectory))
		{
			rec = m_rec[i];
			finded = true;
			break;
		}
	}

	CStdString request;
	request.append(m_strServerUrl);
	if (rec.state == PVR_TIMER_STATE_RECORDING)
		request.append("records/stop?");
	else
		request.append("records/del?");
	request.append("id=");
	request.append(timer.strDirectory);

	CStdString strRecordContent;
	if (!GetFileContents(request, strRecordContent))
	{
		XBMC->Log(LOG_ERROR, "Unable to delete records '%s'", m_strServerUrl.c_str());
		return PVR_ERROR_FAILED;
	}

	if (!strRecordContent.Equals("OK"))
	{
		XBMC->Log(LOG_ERROR, "Unable to delete records '%s'", request.c_str());
		return PVR_ERROR_UNKNOWN;
	}
	PVR->TriggerTimerUpdate();
	return PVR_ERROR_NO_ERROR;
}

string PVRIptvData::url_encode(const string &value) {
	ostringstream escaped;
	escaped.fill('0');
	escaped << hex;

	for (string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
		string::value_type c = (*i);

		//// Keep alphanumeric and other accepted characters intact
		//if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
		//	escaped << c;
		//	continue;
		//}

		// Any other characters are percent-encoded
		escaped << '%'  << int((unsigned char)c);
	}

	return escaped.str();
}