#ifndef _CMAPUTIL_H
#define _CMAPUTIL_H

#include "azure_c_shared_utility/map.h"

class CMapUtil
{
private:
    MAP_HANDLE _handle;
    bool _isOwned;

    CMapUtil() {}
public:
    static CMapUtil *CreateMap();
    CMapUtil(MAP_HANDLE handle, bool isOwned = false);
    CMapUtil(const CMapUtil &other);
    ~CMapUtil();

    MAP_HANDLE GetHandle() const { return _handle; }
    MAP_RESULT Add(const char *key, const char *value);
    MAP_RESULT AddOrUpdate(const char *key, const char *value);
    bool ContainsKey(const char *key) const;
    bool ContainsValue(const char *value) const;
    const char *GetValue(const char *key) const;
};

#endif // _CMAPUTIL_H
