#ifndef _CIOTHUBDEVICE_H
#define _CIOTHUBDEVICE_H

#include <string>
#include <map>

#include "CIoTHubMessage.h"

#include "azure_c_shared_utility/doublylinkedlist.h"
#include "iothub.h"
#include "iothub_device_client_ll.h"
#include "iothub_client_options.h"

class CIoTHubDevice
{
public:
    typedef IOTHUBMESSAGE_DISPOSITION_RESULT (*MessageCallback)(CIoTHubDevice &iotHubDevice, CIoTHubMessage &iotHubMessage, void *userContext);
    typedef void (*EventConfirmationCallback)(CIoTHubDevice &iotHubDevice, IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContext);
    typedef void (*ConnectionStatusCallback)(CIoTHubDevice &iotHubDevice, IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void *userContext);
	typedef int (*DeviceMethodCallback)(CIoTHubDevice &iotHubDevice, const unsigned char *payload, size_t size, unsigned char** response, size_t* resp_size, void* userContext);
	typedef int (*UnknownDeviceMethodCallback)(CIoTHubDevice &iotHubDevice, const char *methodName, const unsigned char *payload, size_t size, unsigned char** response, size_t* resp_size, void* userContext);

    enum Protocol
    {
        // AMQP,
        // AMQP_WebSockets,
        MQTT,
        MQTT_WebSockets
    };

private:
	// typedef struct _UserContext
	// {
	// 	CIoTHubDevice *iotHubDevice;
	// 	void *userContext;
	// } UserContext;

    struct MessageUserContext
    {
        DLIST_ENTRY dlistEntry;
		CIoTHubDevice *iotHubDevice;
        EventConfirmationCallback eventConfirmationCallback;
        void *userContext;
        MessageUserContext(CIoTHubDevice *iotHubDevice, EventConfirmationCallback eventConfirmationCallback, void *userContext) :
            iotHubDevice(iotHubDevice), eventConfirmationCallback(eventConfirmationCallback), userContext(userContext)
        {
            dlistEntry = { 0, 0 };
        }
    };

    struct DeviceMethodUserContext
    {
        DeviceMethodCallback deviceMethodCallback;
        void *userContext;
        DeviceMethodUserContext(DeviceMethodCallback deviceMethodCallback, void *userContext) :
            deviceMethodCallback(deviceMethodCallback), userContext(userContext)
        {
        }
    };
	
    IOTHUB_DEVICE_CLIENT_LL_HANDLE _deviceHandle;
	std::string _connectionString;
	Protocol _protocol;

    MessageCallback _messageCallback;
	ConnectionStatusCallback _connectionStatusCallback;
    UnknownDeviceMethodCallback _unknownDeviceMethodCallback;

	void * _messageCallbackUC;
	void * _connectionStatusCallbackUC;
    void * _unknownDeviceMethodCallbackUC;

    DLIST_ENTRY _outstandingEventList;
    std::map<std::string, DeviceMethodUserContext *> _deviceMethods;
	bool _goodStart;

public:
    CIoTHubDevice(const std::string &connectionString, CIoTHubDevice::Protocol protocol);
    ~CIoTHubDevice();

	bool Start();
	void Stop();
    MessageCallback SetMessageCallback(MessageCallback messageCallback, void *userContext = NULL);
    ConnectionStatusCallback SetConnectionStatusCallback(ConnectionStatusCallback ConnectionStatusCallback, void *userContext = NULL);
	DeviceMethodCallback SetDeviceMethodCallback(const char *methodName, DeviceMethodCallback deviceMethodCallback, void *userContext = NULL);
    UnknownDeviceMethodCallback SetUnknownDeviceMethodCallback(UnknownDeviceMethodCallback unknownDeviceMethodCallback, void *userContext = NULL);
	void SetOptionTrustedCertificate(const char *certificates);
	void SetOptionTrace(bool traceOn);

    IOTHUB_DEVICE_CLIENT_LL_HANDLE GetHandle() const { return _deviceHandle; }
    bool WaitingEvents() { return !DList_IsListEmpty(&_outstandingEventList); }
    int WaitingEventsCount();
    IOTHUB_CLIENT_RESULT SendEventAsync(const std::string &message, EventConfirmationCallback eventConfirmationCallback, void *userContext = NULL);
    IOTHUB_CLIENT_RESULT SendEventAsync(const char *message, EventConfirmationCallback eventConfirmationCallback, void *userContext = NULL);
    IOTHUB_CLIENT_RESULT SendEventAsync(const uint8_t *message, size_t length, EventConfirmationCallback eventConfirmationCallback, void *userContext = NULL);
    IOTHUB_CLIENT_RESULT SendEventAsync(const CIoTHubMessage *message, EventConfirmationCallback eventConfirmationCallback, void *userContext = NULL);

    void DoWork();
	bool IsConnectionGood() { return _goodStart; }
private:
    // Cloud to device messages
    static IOTHUBMESSAGE_DISPOSITION_RESULT InternalMessageCallback(IOTHUB_MESSAGE_HANDLE message, void *userContext);

    // Connection status callback
    static void InternalConnectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* userContext);

    // Event confirmation callback (for each message sent)
    static void InternalEventConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContext);

    // Device method callback
    static int InternalDeviceMethodCallback(const char *methodName, const unsigned char *payload, size_t size, unsigned char **response, size_t *responseSize, void *userContext);
    
    static IOTHUB_CLIENT_TRANSPORT_PROVIDER GetProtocol(CIoTHubDevice::Protocol protocol);
};

#endif
