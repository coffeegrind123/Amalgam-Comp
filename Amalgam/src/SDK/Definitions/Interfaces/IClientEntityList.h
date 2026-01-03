#pragma once
#include "Interface.h"
#include "../Main/CBaseHandle.h"

class IClientEntity;
class ClientClass;
class IClientNetworkable;
class IClientUnknown;

class IClientEntityList
{
public:
	virtual IClientNetworkable* GetClientNetworkable(int entnum) = 0;
	virtual IClientNetworkable* GetClientNetworkableFromHandle(CBaseHandle hEnt) = 0;
	virtual IClientUnknown* GetClientUnknownFromHandle(CBaseHandle hEnt) = 0;
	virtual IClientEntity* GetClientEntity(int entnum) = 0;
	virtual IClientEntity* GetClientEntityFromHandle(CBaseHandle hEnt) = 0;
	virtual int NumberOfEntities(bool bIncludeNonNetworkable) = 0;
	virtual int GetHighestEntityIndex(void) = 0;
	virtual void SetMaxEntities(int maxents) = 0;
	virtual int GetMaxEntities() = 0;
};

MAKE_INTERFACE_VERSION(IClientEntityList, ClientEntityList, "client.dll", "VClientEntityList003");

// Include IClientEntity to get GetDummyEntity
#include "../Main/IClientEntity.h"

// Safe wrappers that never return nullptr
namespace SafeEntityAccess {
	inline IClientEntity* GetEntity(int index) {
		auto entity = I::ClientEntityList->GetClientEntity(index);
		return entity ? entity : GetDummyEntity();
	}

	inline IClientEntity* GetEntityFromHandle(CBaseHandle handle) {
		auto entity = I::ClientEntityList->GetClientEntityFromHandle(handle);
		return entity ? entity : GetDummyEntity();
	}
}

inline IHandleEntity* CBaseHandle::Get() const
{
	return reinterpret_cast<IHandleEntity*>(I::ClientEntityList->GetClientEntityFromHandle(m_Index));
}