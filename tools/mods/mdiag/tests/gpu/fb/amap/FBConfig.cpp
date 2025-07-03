#include "FBConfig.h"

extern FBConfig fbConfig;  // setting FBConfig from global defined in amap.cpp

FBConfig *FBConfig::m_fbconfig = 0L;

FBConfig *FBConfig::getFBConfig()
{
    if (m_fbconfig) return m_fbconfig;
    m_fbconfig = new FBConfig(fbConfig);
    return m_fbconfig;
}

FBConfig::FBConfig()
{
    m_partitions                    = 8;
    m_subpartitions                 = 2;
    m_simplifiedMapping             = 0;
    m_sets                          = 16;
    m_slices                        = 4;
    m_sectors                       = 4;
    m_L2Banks                       = 4;
    m_banks                         = 8;
    m_experimentalMapping           = 0;
    m_experimentalMappingYRotation  = 0;
    m_experimentalMappingXRotation  = 0;
 m_mapSliceToSubPartition =0;
}

