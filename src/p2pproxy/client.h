#pragma once
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

#include "libXBMC_addon.h"
#include "libXBMC_pvr.h"
#include "libXBMC_gui.h"

#define PVR_CLIENT_VERSION     "0.0.2"
#define PLAYLIST_FILE_NAME          "playlist"
#define TVG_FILE_NAME          "epg"
#define RECORD_FILE_NAME "records"
#define P2P_PLUGIN_NAME		"xbmc.pvr"

/*!
 * @brief PVR macros for string exchange
 */
#define PVR_STRCPY(dest, source) do { strncpy(dest, source, sizeof(dest)-1); dest[sizeof(dest)-1] = '\0'; } while(0)
#define PVR_STRCLR(dest) memset(dest, 0, sizeof(dest))

extern bool                          m_bCreated;
extern std::string                   g_strUserPath;
extern std::string                   g_strClientPath;
extern ADDON::CHelper_libXBMC_addon *XBMC;
extern CHelper_libXBMC_pvr          *PVR;

extern std::string g_ServerURL;
extern int         g_iEPGTimeShift;
extern int         g_iStartNumber;
extern bool        g_bTSOverride;
extern bool        g_bCacheM3U;
extern bool        g_bCacheEPG;

extern std::string PathCombine(const std::string &strPath, const std::string &strFileName);
extern std::string GetClientFilePath(const std::string &strFileName);
extern std::string GetUserFilePath(const std::string &strFileName);
