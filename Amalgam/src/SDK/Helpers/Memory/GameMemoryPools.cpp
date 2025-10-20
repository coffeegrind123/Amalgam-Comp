#include "GameMemoryPools.h"
#include "KeyValuesPool.h"

// Static member definitions
KeyValues* CGameMemoryPools::s_pKeyValuesPool[KEYVALUES_POOL_SIZE] = {nullptr};
std::bitset<CGameMemoryPools::KEYVALUES_POOL_SIZE> CGameMemoryPools::s_bKeyValuesPool = 0;
char CGameMemoryPools::s_sStringPool[STRING_POOL_SIZE][MAX_STRING_LENGTH] = {0};
std::bitset<CGameMemoryPools::STRING_POOL_SIZE> CGameMemoryPools::s_bStringUsed = 0;
std::unordered_map<std::string_view, char*> CGameMemoryPools::s_mStringCache;
float CGameMemoryPools::s_fVectorPool[VECTOR_POOL_SIZE * 4] = {0.0f};
std::bitset<CGameMemoryPools::VECTOR_POOL_SIZE> CGameMemoryPools::s_bVectorUsed = 0;
uint32_t CGameMemoryPools::s_nColorPool[COLOR_POOL_SIZE] = {0};
std::bitset<CGameMemoryPools::COLOR_POOL_SIZE> CGameMemoryPools::s_bColorUsed = 0;

// Statistics
uint32_t CGameMemoryPools::s_nKeyValuesAllocs = 0;
uint32_t CGameMemoryPools::s_nStringAllocs = 0;
uint32_t CGameMemoryPools::s_nVectorAllocs = 0;
uint32_t CGameMemoryPools::s_nColorAllocs = 0;