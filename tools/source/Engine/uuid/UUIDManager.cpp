#include <stdafx.h>

#include "UUIDManager.h"

#pragma comment(lib, "rpcrt4.lib")  // UuidCreate - Minimum supported OS Win 2000
#include <rpc.h>
#include <string>
//#include <windows.h>

std::unordered_map<const char*, uint32_t> UUIDManager::myUUIDCache;
uint32_t UUIDManager::myIdCounter;

const char* UUIDManager::CreateUUID()
{
	UUID uuid;
	UuidCreate(&uuid);

	std::string uuid_str;
	UuidToStringA(&uuid, (RPC_CSTR*)uuid_str.data());
	
	myUUIDCache[uuid_str.c_str()] = myIdCounter++;

	return std::move(uuid_str.c_str());
}

const uint32_t UUIDManager::GetIDForUUID(const char *uuid_str)
{
	if (uuid_str == nullptr)
	{
		assert("nothing to do here, you have the incorrect UUID set!");
		return -1;
	}

	if (myUUIDCache.count(uuid_str) == 0)
	{
		//UUID uuid;
		//UuidFromStringA((RPC_CSTR)(uuid_str->c_str()), &uuid);
		//const char* uuid; = CreateUUID(uuid);
		myUUIDCache[uuid_str] = myIdCounter++;
	}

	uint32_t id = myUUIDCache[uuid_str];
	return id;
}