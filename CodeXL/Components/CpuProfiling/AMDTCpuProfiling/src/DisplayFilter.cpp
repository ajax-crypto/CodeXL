//==================================================================================
// Copyright (c) 2013-2016 , Advanced Micro Devices, Inc.  All rights reserved.
//
/// \author AMD Developer Tools Team
/// \file DisplayFilter.cpp
///
//==================================================================================

// Qt:
#include <qtIgnoreCompilerWarnings.h>
#include <QtWidgets>

// Infra:
#include <AMDTBaseTools/Include/gtAssert.h>
#include <AMDTOSWrappers/Include/osDirectory.h>
#include <AMDTOSWrappers/Include/osDebuggingFunctions.h>
#include <AMDTOSWrappers/Include/osFilePath.h>
#include <AMDTOSWrappers/Include/osCpuid.h>
#include <AMDTApplicationComponents/Include/acMessageBox.h>
#include <AMDTApplicationComponents/Include/acFunctions.h>

// AMDTApplicationFramework:
#include <AMDTApplicationFramework/Include/afAppStringConstants.h>
#include <AMDTApplicationFramework/src/afUtils.h>

// Backend:
#include <AMDTCpuPerfEventUtils/inc/ViewConfig.h>
#include <AMDTCpuPerfEventUtils/inc/EventEngine.h>

// Local:
#include <inc/CPUProfileUtils.h>
#include <inc/DisplayFilter.h>
#include <inc/StdAfx.h>
#include <inc/StringConstants.h>

// Global:
static bool g_isDisplaySystemModule = false;
static bool g_isSamplePercent = false;

// Static member initialization:
CPUGlobalDisplayFilter* CPUGlobalDisplayFilter::m_psMySingleInstance = nullptr;

TableDisplaySettings::TableDisplaySettings()
{
    m_amountOfItemsInDisplay = -1;
    m_hotSpotIndicatorColumnCaption = "";
    m_filterByPIDsList.clear();
    m_filterByModulePathsList.clear();
    m_allModulesFullPathsList.clear();
    m_shouldDisplaySystemDllInModulesDlg = true;
    m_isModule32BitList.clear();
    m_isSystemDllList.clear();
    m_lastSortOrder = Qt::DescendingOrder;
    m_lastSortColumnCaption = "";
}

TableDisplaySettings::TableDisplaySettings(const TableDisplaySettings& other)
{
    // Copy the displayed columns:
    for (int i = 0; i < (int)other.m_displayedColumns.size(); i++)
    {
        m_displayedColumns.push_back(other.m_displayedColumns[i]);
    }

    m_amountOfItemsInDisplay = other.m_amountOfItemsInDisplay;
    m_hotSpotIndicatorColumnCaption = other.m_hotSpotIndicatorColumnCaption;
    m_filterByPIDsList = other.m_filterByPIDsList;
    m_filterByModulePathsList = other.m_filterByModulePathsList;
    m_allModulesFullPathsList = other.m_allModulesFullPathsList;
    m_isModule32BitList = other.m_isModule32BitList;
    m_isSystemDllList = other.m_isSystemDllList;
    m_shouldDisplaySystemDllInModulesDlg = other.m_shouldDisplaySystemDllInModulesDlg;
}

bool TableDisplaySettings::colTypeAsString(ProfileDataColumnType column, QString& colStr, QString& tooltip)
{
    bool retVal = true;

    switch (column)
    {
        case MODULE_NAME_COL:
            colStr = CP_colCaptionModuleName;
            tooltip = CP_colCaptionModuleNameTooltip;
            break;


        case PROCESS_NAME_COL:
            colStr = CP_colCaptionProcessName;
            tooltip = CP_colCaptionProcessNameTooltip;
            break;

        case FUNCTION_NAME_COL:
            colStr = CP_colCaptionFunctionName;
            tooltip = CP_colCaptionFunctionNameTooltip;
            break;

        case PID_COL:
            colStr = CP_colCaptionPID;
            tooltip = CP_colCaptionPIDTooltip;
            break;

        case TID_COL:
            colStr = CP_colCaptionTID;
            tooltip = CP_colCaptionTIDTooltip;
            break;

        case SAMPLES_COUNT_COL:
            colStr = CP_colCaptionSamples;
            tooltip = QString(CP_colCaptionSamplesTooltip).arg(m_hotSpotIndicatorColumnCaption);
            break;

        case SAMPLES_PERCENT_COL:
            colStr = CP_colCaptionSamplesPercent;
            tooltip = QString(CP_colCaptionSamplesPercentTooltip).arg(m_hotSpotIndicatorColumnCaption);

            break;

        case MODULE_SYMBOLS_LOADED:
            colStr = CP_colCaptionModuleSymbolsLoaded;
            tooltip = CP_colCaptionModuleSymbolsLoadedTooltip;
            break;

        case SOURCE_FILE_COL:
            colStr = CP_colCaptionSourceFile;
            tooltip = CP_colCaptionSourceFileTooltip;
            break;

        case MODULE_ID:
            colStr = CP_colCaptionModuleId;
            tooltip = CP_colCaptionModuleIdTooltip;
            break;

        case FUNCTION_ID:
            colStr = CP_colCaptionFuncId;
            tooltip = CP_colCaptionFuncIdTooltip;
            break;

        default:
            GT_ASSERT_EX(false, L"Unsupported column type");
            retVal = false;
            break;
    }

    return retVal;
}

CPUGlobalDisplayFilter& CPUGlobalDisplayFilter::instance()
{
    // If this class single instance was not already created:
    if (nullptr == m_psMySingleInstance)
    {
        // Create it:
        m_psMySingleInstance = new CPUGlobalDisplayFilter;
    }

    return *m_psMySingleInstance;
}

CPUGlobalDisplayFilter::~CPUGlobalDisplayFilter()
{
}

void CPUGlobalDisplayFilter::reset()
{
    // "if" to avoid infinite loop
    if (nullptr != m_psMySingleInstance)
    {
        delete m_psMySingleInstance;
        m_psMySingleInstance = nullptr;
    }
}

CPUGlobalDisplayFilter::CPUGlobalDisplayFilter()
{
    m_displaySystemDLLs = false;
    m_displayPercentageInColumn = false;
}

DisplayFilter::DisplayFilter()
{
    m_CLUOVHdrName = acGTStringToQString(L"Cache Line Utilization Percentage");
}

bool DisplayFilter::CreateConfigCounterMap()
{
    bool ret = false;

    m_reportConfigs.clear();

    if (nullptr != m_pProfDataReader.get())
    {
        ret = m_pProfDataReader->GetReportConfigurations(m_reportConfigs);

        if (true == ret)
        {
            m_configCounterMap.clear();

            for (const auto& config : m_reportConfigs)
            {
                CounterNameIdVec counters;
                counters.clear();

                for (const auto& counter : config.m_counterDescs)
                {
                    auto nameIdDesc = std::make_tuple(counter.m_name,
                                                      counter.m_abbrev,
                                                      counter.m_description,
                                                      counter.m_id,
                                                      counter.m_type);
                    counters.push_back(nameIdDesc);
                    m_counterNameIdMap.insert(std::make_pair(counter.m_name, counter.m_id));
                    m_counterIdNameMap.insert(std::make_pair(counter.m_id, counter.m_name));
                }

                m_configCounterMap.insert(cofigNameCounterPair(config.m_name, counters));
                m_configNameList.push_back(config.m_name);
            }
        }
    }

    return ret;
}

bool DisplayFilter::GetConfigCounters(const QString& configName,
                                      CounterNameIdVec& counterDetails)
{
    bool ret = false;

    // set the member variable
    m_configurationName = configName;

    auto itr = m_configCounterMap.find(acQStringToGTString(configName));

    if (m_configCounterMap.end() != itr)
    {
        counterDetails = itr->second;
        ret = true;
    }

    return ret;
}

bool DisplayFilter::SetProfileDataOptions(AMDTProfileDataOptions opts)
{
    m_options.m_coreMask = opts.m_coreMask;
    m_options.m_doSort = opts.m_doSort;
    m_options.m_ignoreSystemModules = opts.m_ignoreSystemModules;
    m_options.m_isSeperateByCore = opts.m_isSeperateByCore;
    m_options.m_isSeperateByNuma = opts.m_isSeperateByNuma;
    m_options.m_othersEntryInSummary = opts.m_othersEntryInSummary;
    m_options.m_summaryCount = opts.m_summaryCount;
    m_options.m_counters = opts.m_counters;

    return true;
}

const AMDTProfileDataOptions& DisplayFilter::GetProfileDataOptions() const
{
    return m_options;
}

bool DisplayFilter::SetReportConfig()
{
    bool ret = false;

    if (nullptr != m_pProfDataReader.get())
    {
        SetProfileDataOption();
        ret = m_pProfDataReader->SetReportOption(m_options);
    }

    return ret;
}

const gtVector<AMDTProfileReportConfig>& DisplayFilter::GetReportConfig() const
{
    return m_reportConfigs;
}

bool DisplayFilter::InitToDefault()
{
    bool retVal = true;
    m_reportConfigs.clear();
    m_selectedCountersIdList.clear();

    if (nullptr != m_pProfDataReader.get())
    {
        retVal = m_pProfDataReader->GetReportConfigurations(m_reportConfigs);

        AMDTProfileSessionInfo sessionInfo;
        m_pProfDataReader->GetProfileSessionInfo(sessionInfo);

        m_options.m_coreMask = sessionInfo.m_coreAffinity;
        m_options.m_doSort = true;
        m_options.m_summaryCount = 5;
        m_options.m_isSeperateByCore = false;
        m_options.m_isSeperateByNuma = false;
        m_options.m_ignoreSystemModules = !g_isDisplaySystemModule;

        //setting all counters for config "All Data"
        for (auto const& counter : m_reportConfigs[0].m_counterDescs)
        {
            m_options.m_counters.push_back(counter.m_id);
            m_selectedCountersIdList.push_back(std::make_tuple(counter.m_name,
                                                               counter.m_abbrev,
                                                               counter.m_description,
                                                               counter.m_id,
                                                               counter.m_type));
        }

        retVal = m_pProfDataReader->SetReportOption(m_options);

        // Set core count
        m_coreCount = sessionInfo.m_coreCount;
    }

    return retVal;
}

void DisplayFilter::GetSupportedCountersList(CounterNameIdVec& counterList) const
{
    counterList = m_selectedCountersIdList;
}

AMDTUInt64 DisplayFilter::GetCounterId(const QString& counterName) const
{
    AMDTUInt64 id = 0;
    auto idItr = m_counterNameIdMap.find(acQStringToGTString(counterName));

    if (m_counterNameIdMap.end() != idItr)
    {
        id = idItr->second;
    }

    return id;
}

gtString DisplayFilter::GetCounterName(AMDTUInt64 counterId) const
{
    gtString counterName;

    auto nameItr = m_counterIdNameMap.find(counterId);

    if (m_counterIdNameMap.end() != nameItr)
    {
        counterName = nameItr->second;
    }

    return counterName;
}

void DisplayFilter::SetProfileDataOption()
{
    m_options.m_counters.clear();

    for (auto const& selCounter : m_selectedCountersIdList)
    {
        auto foundCountId = m_counterNameIdMap.find(std::get<0>(selCounter));

        if (m_counterNameIdMap.end() != foundCountId)
        {
            m_options.m_counters.push_back(foundCountId->second);
        }
    }
}

bool DisplayFilter::IsSystemModuleIgnored() const
{
    return m_options.m_ignoreSystemModules;
}

void DisplayFilter::SetSamplePercent(bool isSet)
{
    g_isSamplePercent = isSet;
}

void DisplayFilter::setIgnoreSysDLL(bool isChecked)
{
    m_options.m_ignoreSystemModules = isChecked;
    g_isDisplaySystemModule = !isChecked;
}

bool DisplayFilter::GetSamplePercent() const
{
    return g_isSamplePercent;
}

AMDTUInt32 DisplayFilter::GetCoreCount() const
{
    return m_coreCount;
}

void DisplayFilter::SetCLUOVHdrName(const QString& name)
{
    m_CLUOVHdrName = name;
    m_isCLUPercent = false;

    if (acGTStringToQString(CLU_PERCENTAGE_CAPTION) == name)
    {
        m_isCLUPercent = true;
    }
}