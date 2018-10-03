#include "CMapUtil.h"

using namespace std;

CMapUtil *CMapUtil::CreateMap()
{
    MAP_HANDLE work;

    if (NULL == (work = Map_Create(NULL)))
        return NULL;
    
    return new CMapUtil(work, true);
}

CMapUtil::CMapUtil(MAP_HANDLE handle, bool isOwned)
{
    _isOwned = isOwned;
    _handle = handle;
}

CMapUtil::CMapUtil(const CMapUtil &other)
{
    _isOwned = true;
    _handle = Map_Clone(other.GetHandle());
}

CMapUtil::~CMapUtil()
{
    if (_isOwned) 
        Map_Destroy(GetHandle());
}

MAP_RESULT CMapUtil::Add(const char *key, const char *value)
{
	return (GetHandle() != NULL)? Map_Add(GetHandle(), key, value) : MAP_ERROR;
}

MAP_RESULT CMapUtil::AddOrUpdate(const char *key, const char *value)
{
    return (GetHandle() != NULL)? Map_AddOrUpdate(GetHandle(), key, value) : MAP_ERROR;
}

bool CMapUtil::ContainsKey(const char *key) const
{
    bool found;
	
	if (GetHandle() == NULL)
		return MAP_ERROR;
	
    MAP_RESULT result = Map_ContainsKey(GetHandle(), key, &found);

    if (result != MAP_OK)
        return false;
    else
        return found;
}

bool CMapUtil::ContainsValue(const char *value) const
{
    bool found;
	
	if (GetHandle() == NULL)
		return MAP_ERROR;

    MAP_RESULT result = Map_ContainsValue(GetHandle(), value, &found);

    if (result != MAP_OK)
        return false;
    else
        return found;
}

const char *CMapUtil::GetValue(const char *key) const
{
    return Map_GetValueFromKey(GetHandle(), key);
}

