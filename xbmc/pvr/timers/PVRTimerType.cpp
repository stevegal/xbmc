/*
 *      Copyright (C) 2012-2015 Team Kodi
 *      http://kodi.tv
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
 *  along with Kodi; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "addons/include/xbmc_pvr_types.h"
#include "guilib/LocalizeStrings.h"
#include "pvr/timers/PVRTimerType.h"
#include "pvr/addons/PVRClients.h"
#include "pvr/PVRManager.h"
#include "settings/Settings.h"
#include "utils/log.h"
#include "utils/StringUtils.h"

using namespace PVR;

// static
const std::vector<CPVRTimerTypePtr> CPVRTimerType::GetAllTypes()
{
  std::vector<CPVRTimerTypePtr> allTypes;

  PVR_CLIENTMAP clients;
  g_PVRClients->GetConnectedClients(clients);
  for (auto cit = clients.begin(); cit != clients.end(); ++cit)
  {
    const PVR_TIMER_TYPES *types = g_PVRClients->GetTimerTypes((*cit).first);
    for (auto tit = types->begin(); tit != types->end(); ++tit)
    {
      if ((*tit).iId != PVR_TIMER_TYPE_NONE)
        allTypes.push_back(CPVRTimerTypePtr(new CPVRTimerType(*tit, (*cit).first)));
    }
  }

  return allTypes;
}

// static
const CPVRTimerTypePtr CPVRTimerType::GetFirstAvailableType()
{
  PVR_CLIENTMAP clients;
  g_PVRClients->GetConnectedClients(clients);
  for (auto cit = clients.begin(); cit != clients.end(); ++cit)
  {
    const PVR_TIMER_TYPES *types = g_PVRClients->GetTimerTypes((*cit).first);
    for (auto tit = types->begin(); tit != types->end(); ++tit)
    {
      if ((*tit).iId != PVR_TIMER_TYPE_NONE)
        return CPVRTimerTypePtr(new CPVRTimerType(*tit, (*cit).first));
    }
  }
  return CPVRTimerTypePtr();
}

// static
CPVRTimerTypePtr CPVRTimerType::CreateFromIds(unsigned int iTypeId, int iClientId)
{
  const PVR_TIMER_TYPES *types = g_PVRClients->GetTimerTypes(iClientId);
  if (types)
  {
    for (auto it = types->begin(); it != types->end(); ++it)
    {
      if (it->iId == iTypeId)
      {
        CPVRTimerTypePtr newType(new CPVRTimerType());

        newType->m_iClientId      = iClientId;
        newType->m_iTypeId        = iTypeId;
        newType->m_iAttributes    = it->iAttributes;
        newType->m_strDescription = it->strDescription;
        newType->InitAttributeValues(*it);

        return newType;
      }
    }
  }

  CLog::Log(LOGERROR, "CPVRTimerType::CreateFromIds unable to resolve numeric timer type (%d, %d)", iTypeId, iClientId);
  return CPVRTimerTypePtr();
}

// static
CPVRTimerTypePtr CPVRTimerType::CreateFromAttributes(
  unsigned int iMustHaveAttr, unsigned int iMustNotHaveAttr, int iClientId)
{
  const PVR_TIMER_TYPES *types = g_PVRClients->GetTimerTypes(iClientId);
  if (types)
  {
    for (auto it = types->begin(); it != types->end(); ++it)
    {
      if (((it->iAttributes & iMustHaveAttr)    == iMustHaveAttr) &&
          ((it->iAttributes & iMustNotHaveAttr) == 0))
      {
        CPVRTimerTypePtr newType(new CPVRTimerType());

        newType->m_iClientId      = iClientId;
        newType->m_iTypeId        = it->iId;
        newType->m_iAttributes    = it->iAttributes;
        newType->m_strDescription = it->strDescription;
        newType->InitAttributeValues(*it);

        return newType;
      }
    }
  }

  CLog::Log(LOGERROR, "CPVRTimerType::CreateFromAttributes unable to resolve timer type (0x%x, 0x%x, %d)", iMustHaveAttr, iMustNotHaveAttr, iClientId);
  return CPVRTimerTypePtr();
}

CPVRTimerType::CPVRTimerType() :
  m_iClientId(0),
  m_iTypeId(PVR_TIMER_TYPE_NONE),
  m_iAttributes(PVR_TIMER_TYPE_ATTRIBUTE_NONE),
  m_iPriorityDefault(50),
  m_iLifetimeDefault(365),
  m_iPreventDupEpisodesDefault(0)
{
}

CPVRTimerType::CPVRTimerType(const PVR_TIMER_TYPE &type, int iClientId) :
  m_iClientId(iClientId),
  m_iTypeId(type.iId),
  m_iAttributes(type.iAttributes),
  m_strDescription(type.strDescription)
{
  InitAttributeValues(type);
}

// virtual
CPVRTimerType::~CPVRTimerType()
{
}

bool CPVRTimerType::operator ==(const CPVRTimerType& right) const
{
  return (m_iClientId                  == right.m_iClientId   &&
          m_iTypeId                    == right.m_iTypeId  &&
          m_iAttributes                == right.m_iAttributes &&
          m_strDescription             == right.m_strDescription &&
          m_priorityValues             == right.m_priorityValues &&
          m_iPriorityDefault           == right.m_iPriorityDefault &&
          m_lifetimeValues             == right.m_lifetimeValues &&
          m_iLifetimeDefault           == right.m_iLifetimeDefault &&
          m_preventDupEpisodesValues   == right.m_preventDupEpisodesValues &&
          m_iPreventDupEpisodesDefault == right.m_iPreventDupEpisodesDefault);
}

bool CPVRTimerType::operator !=(const CPVRTimerType& right) const
{
  return !(*this == right);
}

void CPVRTimerType::InitAttributeValues(const PVR_TIMER_TYPE &type)
{
  InitPriorityValues(type);
  InitLifetimeValues(type);
  InitPreventDuplicateEpisodesValues(type);

}

void CPVRTimerType::InitPriorityValues(const PVR_TIMER_TYPE &type)
{
  if (type.iPrioritiesSize > 0)
  {
    for (unsigned int i = 0; i < type.iPrioritiesSize; ++i)
    {
      std::string strDescr(type.priorities[i].strDescription);
      if (strDescr.size() == 0)
      {
        // No description given by addon. Create one from value.
        strDescr = StringUtils::Format("%d", type.priorities[i].iValue);
      }
      m_priorityValues.push_back(std::make_pair(strDescr, type.priorities[i].iValue));
    }

    m_iPriorityDefault = type.iPrioritiesDefault;
  }
  else if (SupportsPriority())
  {
    // No values given by addon, but priority supported. Use default values 1..100
    for (int i = 1; i < 101; ++i)
      m_priorityValues.push_back(std::make_pair(StringUtils::Format("%d", i), i));

    m_iPriorityDefault = CSettings::Get().GetInt("pvrrecord.defaultpriority");
  }
  else
  {
    // No priority supported.
    m_iPriorityDefault = CSettings::Get().GetInt("pvrrecord.defaultpriority");
  }
}

void CPVRTimerType::GetPriorityValues(std::vector< std::pair<std::string, int> > &list) const
{
  for (auto it = m_priorityValues.begin(); it != m_priorityValues.end(); ++it)
    list.push_back(*it);
}

void CPVRTimerType::InitLifetimeValues(const PVR_TIMER_TYPE &type)
{
  if (type.iLifetimesSize > 0)
  {
    for (unsigned int i = 0; i < type.iLifetimesSize; ++i)
    {
      std::string strDescr(type.lifetimes[i].strDescription);
      if (strDescr.size() == 0)
      {
        // No description given by addon. Create one from value.
        strDescr = StringUtils::Format("%d", type.lifetimes[i].iValue);
      }
      m_lifetimeValues.push_back(std::make_pair(strDescr, type.lifetimes[i].iValue));
    }

    m_iLifetimeDefault = type.iLifetimesDefault;
  }
  else if (SupportsLifetime())
  {
    // No values given by addon, but lifetime supported. Use default values 1..365
    for (int i = 1; i < 366; ++i)
    {
      m_lifetimeValues.push_back(std::make_pair(StringUtils::Format(g_localizeStrings.Get(17999).c_str(), i), i)); // "%s days"
    }
    m_iLifetimeDefault = CSettings::Get().GetInt("pvrrecord.defaultlifetime");
  }
  else
  {
    // No lifetime supported.
    m_iLifetimeDefault = CSettings::Get().GetInt("pvrrecord.defaultlifetime");
  }
}

void CPVRTimerType::GetLifetimeValues(std::vector< std::pair<std::string, int> > &list) const
{
  for (auto it = m_lifetimeValues.begin(); it != m_lifetimeValues.end(); ++it)
    list.push_back(*it);
}

void CPVRTimerType::InitPreventDuplicateEpisodesValues(const PVR_TIMER_TYPE &type)
{
  if (type.iPreventDuplicateEpisodesSize > 0)
  {
    for (unsigned int i = 0; i < type.iPreventDuplicateEpisodesSize; ++i)
    {
      std::string strDescr(type.preventDuplicateEpisodes[i].strDescription);
      if (strDescr.size() == 0)
      {
        // No description given by addon. Create one from value.
        strDescr = StringUtils::Format("%d", type.preventDuplicateEpisodes[i].iValue);
      }
      m_preventDupEpisodesValues.push_back(std::make_pair(strDescr, type.preventDuplicateEpisodes[i].iValue));
    }

    m_iPreventDupEpisodesDefault = type.iPreventDuplicateEpisodesDefault;
  }
  else if (SupportsRecordOnlyNewEpisodes())
  {
    // No values given by addon, but prevent duplicate episodes supported. Use default values 0..1
    m_preventDupEpisodesValues.push_back(std::make_pair(g_localizeStrings.Get(815), 0)); // "Record all episodes"
    m_preventDupEpisodesValues.push_back(std::make_pair(g_localizeStrings.Get(816), 1)); // "Record only new episodes"
    m_iPreventDupEpisodesDefault = CSettings::Get().GetInt("pvrrecord.preventduplicateepisodes");
  }
  else
  {
    // No prevent duplicate episodes supported.
    m_iPreventDupEpisodesDefault = CSettings::Get().GetInt("pvrrecord.preventduplicateepisodes");
  }
}

void CPVRTimerType::GetPreventDuplicateEpisodesValues(std::vector< std::pair<std::string, int> > &list) const
{
  for (auto it = m_preventDupEpisodesValues.begin(); it != m_preventDupEpisodesValues.end(); ++it)
    list.push_back(*it);
}
