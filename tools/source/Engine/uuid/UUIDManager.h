#pragma once

#include <unordered_map>

class UUIDManager
{
public:
	static const char *CreateUUID();
	static const uint32_t GetIDForUUID(const char *uuid);

private:
	static std::unordered_map<const char*, uint32_t> myUUIDCache;
	static uint32_t myIdCounter;
};