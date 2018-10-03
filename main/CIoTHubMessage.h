#ifndef _CIOTMESSAGE_H
#define _CIOTMESSAGE_H

#include <string>
#include <cstdint>

#include "iothub_message.h"
#include "CMapUtil.h"

class CIoTHubMessage
{
private:
    IOTHUB_MESSAGE_HANDLE _handle;
    bool _isOwned;

    CIoTHubMessage();

public:
    CIoTHubMessage(const std::string &message);
    CIoTHubMessage(const char *message);
    CIoTHubMessage(const uint8_t *message, size_t length);
    CIoTHubMessage(IOTHUB_MESSAGE_HANDLE handle);
    CIoTHubMessage(const CIoTHubMessage &other);
    ~CIoTHubMessage();

    IOTHUB_MESSAGE_HANDLE GetHandle() const { return _handle; }
    const char *GetCString() const;
    const std::string GetString() const;
    IOTHUB_MESSAGE_RESULT GetByteArray(const uint8_t **buffer, size_t *size) const;
    IOTHUBMESSAGE_CONTENT_TYPE GetContentType() const;
    IOTHUB_MESSAGE_RESULT SetContentTypeSystemProperty(const char *contentType);
    const char *GetContentTypeSystemProperty() const;
    CMapUtil *GetProperties();
    IOTHUB_MESSAGE_RESULT SetProperty(const char *key, const char *value);
    const char *GetProperty(const char *key) const;
    IOTHUB_MESSAGE_RESULT SetMessageId(const char *messageId);
    const char *GetMessageId() const;
    IOTHUB_MESSAGE_RESULT SetCorrelationId(const char *correlationId);
    const char *GetCorrelationId() const;
};

#endif // _CIOTMESSAGE_H