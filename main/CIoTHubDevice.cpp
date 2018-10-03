#include "CIoTHubDevice.h"

#include <esp_log.h>

#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "iothub_client.h"
#include "iothubtransportmqtt.h"
#include "iothubtransportmqtt_websockets.h"
#include "certs.h"
//#include "iothubtransportamqp.h"
//#include "iothubtransportamqp_websockets.h"

using namespace std;

static const char *TAG = "CIoTHubDevice";

CIoTHubDevice::CIoTHubDevice(const string &connectionString, Protocol protocol) :
	_connectionString(connectionString),
	_protocol(protocol),
	_messageCallback(NULL),
    _connectionStatusCallback(NULL),
    _unknownDeviceMethodCallback(NULL),
	_messageCallbackUC(NULL),
    _connectionStatusCallbackUC(NULL),
    _unknownDeviceMethodCallbackUC(NULL),
	_goodStart(false)
{
}

bool CIoTHubDevice::Start()
{
    IOTHUB_CLIENT_RESULT result;

    platform_init();

    _deviceHandle = IoTHubClient_LL_CreateFromConnectionString(_connectionString.c_str(), GetProtocol(_protocol));

    if (_deviceHandle == NULL)
	{
        ESP_LOGE(TAG, "Failed to create IoT hub handle");
		return false;
	}

	result = IoTHubClient_LL_SetConnectionStatusCallback(GetHandle(), InternalConnectionStatusCallback, this);

	if (result != IOTHUB_CLIENT_OK)
	{
		ESP_LOGE(TAG, "Failed to set up connection status callback");
		return false;
	}

    result = IoTHubClient_LL_SetMessageCallback(GetHandle(), InternalMessageCallback, this);

    if (result != IOTHUB_CLIENT_OK)
	{
        ESP_LOGE(TAG, "Failed to set up message callback");
		return false;
	}

    result =  IoTHubClient_LL_SetDeviceMethodCallback(GetHandle(), InternalDeviceMethodCallback, this);

    if (result != IOTHUB_CLIENT_OK)
	{
        ESP_LOGE(TAG, "Failed to set up device method callback");
		return false;
	}

    DList_InitializeListHead(&_outstandingEventList);
	
	_goodStart = true;
	
	ESP_LOGI(TAG, "IoT device started (0x%.8x)", (unsigned int)GetHandle());
	return _goodStart;
}

void CIoTHubDevice::Stop()
{
    IoTHubClient_LL_Destroy(GetHandle());
    platform_deinit();

    while (!DList_IsListEmpty(&_outstandingEventList))
    {
        PDLIST_ENTRY work = _outstandingEventList.Flink;
        DList_RemoveEntryList(work);
        delete work;
    }

    for (map<std::string, DeviceMethodUserContext *>::iterator it = _deviceMethods.begin(); it != _deviceMethods.end(); it++)
    {
        delete it->second;
    }
	
	_goodStart = false;
}

CIoTHubDevice::~CIoTHubDevice()
{
	if (_goodStart)
		Stop();
}

CIoTHubDevice::MessageCallback CIoTHubDevice::SetMessageCallback(MessageCallback messageCallback, void *userContext)
{
    MessageCallback temp = _messageCallback;
    _messageCallback = messageCallback;
    _messageCallbackUC = userContext;

    return temp;
}

CIoTHubDevice::ConnectionStatusCallback CIoTHubDevice::SetConnectionStatusCallback(ConnectionStatusCallback connectionStatusCallback, void *userContext)
{
    ConnectionStatusCallback temp = _connectionStatusCallback;
    _connectionStatusCallback = connectionStatusCallback;
    _connectionStatusCallbackUC = userContext;
	
	return temp;
}

CIoTHubDevice::DeviceMethodCallback CIoTHubDevice::SetDeviceMethodCallback(const char *methodName, DeviceMethodCallback deviceMethodCallback, void *userContext)
{
    DeviceMethodUserContext *deviceMethodUserContext = new DeviceMethodUserContext(deviceMethodCallback, userContext);
    map<std::string, DeviceMethodUserContext *>::iterator it;
    DeviceMethodCallback temp = NULL;

    it = _deviceMethods.find(methodName);

    if (it == _deviceMethods.end())
    {
        _deviceMethods[methodName] = deviceMethodUserContext;
    }
    else
    {
        temp = it->second->deviceMethodCallback;
        delete it->second;

        if (deviceMethodCallback != NULL)
        {
            it->second = deviceMethodUserContext;
        }
        else
        {
            _deviceMethods.erase(it);
        }
    }

    return temp;
}

CIoTHubDevice::UnknownDeviceMethodCallback CIoTHubDevice::SetUnknownDeviceMethodCallback(UnknownDeviceMethodCallback unknownDeviceMethodCallback, void *userContext)
{
    UnknownDeviceMethodCallback temp = _unknownDeviceMethodCallback;
    _unknownDeviceMethodCallback = unknownDeviceMethodCallback;
    _unknownDeviceMethodCallbackUC = userContext;

    return temp;
}
	
void CIoTHubDevice::SetOptionTrustedCertificate(const char *certificates)
{
	(void)IoTHubClient_LL_SetOption(GetHandle(), OPTION_TRUSTED_CERT, certificates);
}

void CIoTHubDevice::SetOptionTrace(bool traceOn)
{
	(void)IoTHubClient_LL_SetOption(GetHandle(), OPTION_LOG_TRACE, &traceOn);
}

IOTHUB_CLIENT_RESULT CIoTHubDevice::SendEventAsync(const string &message, EventConfirmationCallback eventConfirmationCallback, void *userContext)
{
    CIoTHubMessage *hubMessage = new CIoTHubMessage(message);
    IOTHUB_CLIENT_RESULT result;

    result = SendEventAsync(hubMessage, eventConfirmationCallback, userContext);

    delete hubMessage;

    return result;
}

IOTHUB_CLIENT_RESULT CIoTHubDevice::SendEventAsync(const char *message, EventConfirmationCallback eventConfirmationCallback, void *userContext)
{
    CIoTHubMessage *hubMessage = new CIoTHubMessage(message);
    IOTHUB_CLIENT_RESULT result;

    result =  SendEventAsync(hubMessage, eventConfirmationCallback, userContext);

    delete hubMessage;

    return result;
}

IOTHUB_CLIENT_RESULT CIoTHubDevice::SendEventAsync(const uint8_t *message, size_t length, EventConfirmationCallback eventConfirmationCallback, void *userContext)
{
    CIoTHubMessage *hubMessage = new CIoTHubMessage(message, length);
    IOTHUB_CLIENT_RESULT result;

    result =  SendEventAsync(hubMessage, eventConfirmationCallback, userContext);

    delete hubMessage;

    return result;
}

IOTHUB_CLIENT_RESULT CIoTHubDevice::SendEventAsync(const CIoTHubMessage *message, EventConfirmationCallback eventConfirmationCallback, void *userContext)
{
	if (!_goodStart)
		return IOTHUB_CLIENT_ERROR;
	
    MessageUserContext *messageUC = new MessageUserContext(this, eventConfirmationCallback, userContext);
    IOTHUB_CLIENT_RESULT result;

    result = IoTHubClient_LL_SendEventAsync(GetHandle(), message->GetHandle(), InternalEventConfirmationCallback, messageUC);

    if (result == IOTHUB_CLIENT_OK)
    {
        DList_InsertTailList(&_outstandingEventList, &(messageUC->dlistEntry));
    }

    return result;
}

void CIoTHubDevice::DoWork()
{
	if (!_goodStart)
	{
		ESP_LOGE(TAG, "Device is in failed state");
		return;
	}

    IoTHubClient_LL_DoWork(GetHandle());
}

int CIoTHubDevice::WaitingEventsCount()
{
    PDLIST_ENTRY listEntry = &_outstandingEventList;
    
    int count = 0;

    while (listEntry->Flink != &_outstandingEventList)
    {
        count++;
        listEntry = listEntry->Flink;
    }

    return count;
}


IOTHUBMESSAGE_DISPOSITION_RESULT CIoTHubDevice::InternalMessageCallback(IOTHUB_MESSAGE_HANDLE message, void *userContext)
{
    CIoTHubDevice *that = (CIoTHubDevice *)userContext;
    IOTHUBMESSAGE_DISPOSITION_RESULT result = IOTHUBMESSAGE_REJECTED;

    if (that->_messageCallback != NULL)
    {
        CIoTHubMessage *msg = new CIoTHubMessage(message);

        result = that->_messageCallback(*that, *msg, that->_messageCallbackUC);

        delete msg;
    }

    return result;
}

void CIoTHubDevice::InternalConnectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* userContext)
{
    CIoTHubDevice *that = (CIoTHubDevice *)userContext;

    if (that->_connectionStatusCallback != NULL)
    {
        that->_connectionStatusCallback(*that, result, reason, that->_connectionStatusCallbackUC);
    }
}

void CIoTHubDevice::InternalEventConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContext)
{
    MessageUserContext *messageUC = (MessageUserContext *)userContext;

    if (messageUC->eventConfirmationCallback != NULL)
    {
        messageUC->eventConfirmationCallback(*(messageUC->iotHubDevice), result, messageUC->userContext);
    }

    DList_RemoveEntryList(&(messageUC->dlistEntry));    
    delete messageUC;
}

int CIoTHubDevice::InternalDeviceMethodCallback(const char *methodName, const unsigned char *payload, size_t size, unsigned char **response, size_t *responseSize, void *userContext)
{
    CIoTHubDevice *that = (CIoTHubDevice *)userContext;
    map<std::string, DeviceMethodUserContext *>::iterator it;
    int result = -1;

    it = that->_deviceMethods.find(methodName);

    if (it != that->_deviceMethods.end())
    {
        result = it->second->deviceMethodCallback(*that, payload, size, response, responseSize, it->second->userContext);
    }
    else if (that->_unknownDeviceMethodCallback != NULL)
    {
        result = that->_unknownDeviceMethodCallback(*that, methodName, payload, size, response, responseSize, that->_unknownDeviceMethodCallbackUC);
    }
    else
    {
        *responseSize = 0;
        *response = NULL;
    }

    return result;    
}

IOTHUB_CLIENT_TRANSPORT_PROVIDER CIoTHubDevice::GetProtocol(CIoTHubDevice::Protocol protocol)
{
    IOTHUB_CLIENT_TRANSPORT_PROVIDER result = NULL;

    switch (protocol)
    {
    case Protocol::MQTT:
        result = MQTT_Protocol;
        break;
// TODO: It appears currently the ESP32 build only supports MQTT		
    // case Protocol::MQTT_WebSockets:
        // result = MQTT_WebSocket_Protocol;
        // break;
    // case Protocol::AMQP:
    //     result = AMQP_Protocol;
    //     break;
    // case Protocol::AMQP_WebSockets:
    //     result = AMQP_Protocol_over_WebSocketsTls;
    //     break;
    default:
        break;
    }

    return result;
}
