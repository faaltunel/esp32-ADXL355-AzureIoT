#include "CIoTHubMessage.h"

using namespace std;

CIoTHubMessage::CIoTHubMessage()
{
    // Private - should never be called
}

CIoTHubMessage::CIoTHubMessage(const std::string &message) : CIoTHubMessage(message.c_str())
{

}

CIoTHubMessage::CIoTHubMessage(const char *message)
{
    _isOwned = true;
    _handle = IoTHubMessage_CreateFromString(message);
}

CIoTHubMessage::CIoTHubMessage(const uint8_t *message, size_t length)
{
    _isOwned = true;
    _handle = IoTHubMessage_CreateFromByteArray(message, length);
}

CIoTHubMessage::CIoTHubMessage(IOTHUB_MESSAGE_HANDLE handle)
{
    _isOwned = false;
    _handle = handle;
}

CIoTHubMessage::CIoTHubMessage(const CIoTHubMessage &other)
{
    _handle = IoTHubMessage_Clone(other.GetHandle());
}

CIoTHubMessage::~CIoTHubMessage()
{
    if (GetHandle() != NULL && _isOwned)
        IoTHubMessage_Destroy(GetHandle());
}

const char *CIoTHubMessage::GetCString() const
{
	if (GetHandle() == NULL)
		return NULL;

    if (GetContentType() == IOTHUBMESSAGE_STRING)
    {
        return IoTHubMessage_GetString(GetHandle());
    }
    else 
    {
        return "";
    }
}

const string CIoTHubMessage::GetString() const
{
    return (GetHandle() != NULL)? string(IoTHubMessage_GetString(GetHandle())): string();
}

IOTHUB_MESSAGE_RESULT CIoTHubMessage::GetByteArray(const uint8_t **buffer, size_t *size) const
{
	if (GetHandle() != NULL)
	{
		return IoTHubMessage_GetByteArray(GetHandle(), buffer, size);
	}
	else
	{
		*size = 0;
		return IOTHUB_MESSAGE_ERROR;
	}
}

IOTHUBMESSAGE_CONTENT_TYPE CIoTHubMessage::GetContentType() const
{
    return (GetHandle() != NULL)? IoTHubMessage_GetContentType(GetHandle()) : IOTHUBMESSAGE_UNKNOWN;
}

IOTHUB_MESSAGE_RESULT CIoTHubMessage::SetContentTypeSystemProperty(const char *contentType)
{
    return (GetHandle() != NULL)? IoTHubMessage_SetContentTypeSystemProperty(GetHandle(), contentType) : IOTHUB_MESSAGE_ERROR;
}

const char *CIoTHubMessage::GetContentTypeSystemProperty() const
{
    return (GetHandle() != NULL)? IoTHubMessage_GetContentTypeSystemProperty(GetHandle()) : NULL;
}

CMapUtil *CIoTHubMessage::GetProperties()
{
    return (GetHandle() != NULL)? new CMapUtil(IoTHubMessage_Properties(GetHandle())) : NULL;
}

IOTHUB_MESSAGE_RESULT CIoTHubMessage::SetProperty(const char *key, const char *value)
{
    return (GetHandle() != NULL)? IoTHubMessage_SetProperty(GetHandle(), key, value) : IOTHUB_MESSAGE_ERROR;
}

const char *CIoTHubMessage::GetProperty(const char *key) const
{
    return (GetHandle() != NULL)? IoTHubMessage_GetProperty(GetHandle(), key) : NULL;
}

IOTHUB_MESSAGE_RESULT CIoTHubMessage::SetMessageId(const char *messageId)
{
    return (GetHandle() != NULL)? IoTHubMessage_SetMessageId(GetHandle(), messageId) : IOTHUB_MESSAGE_ERROR;
}

const char *CIoTHubMessage::GetMessageId() const
{
    return (GetHandle() != NULL)? IoTHubMessage_GetMessageId(GetHandle()) : NULL;
}

IOTHUB_MESSAGE_RESULT CIoTHubMessage::SetCorrelationId(const char *correlationId)
{
    return (GetHandle() != NULL)? IoTHubMessage_SetCorrelationId(GetHandle(), correlationId) : IOTHUB_MESSAGE_ERROR;
}

const char *CIoTHubMessage::GetCorrelationId() const
{
    return (GetHandle() != NULL)? IoTHubMessage_GetCorrelationId(GetHandle()) : NULL;
}

