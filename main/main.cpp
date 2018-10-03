#include <driver/i2c.h>

//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <esp_log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <string>
#include <algorithm>

#include "nvs_flash.h"
#include "sdkconfig.h"
#include "ADXL355.h"
#include "iothub_client.h"
#include "iothub_message.h"

#include "CIoTHubDevice.h"
#include "CIoTHubMessage.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

// TODO: Should be configurable
#define SDA_PIN 23
#define SCL_PIN 22

using namespace std;

static const char *TAG = "azureADXL355";

// TODO: Need to be configured somehow
static const string connectionString = "HostName=MarkRadHub2.azure-devices.net;DeviceId=TestDevice1;SharedAccessKey=9lbM21JSkEVal2y/y/NfBldWUMKixrBWz/aj3vCLss8=";
static CIoTHubDevice::Protocol protocol = CIoTHubDevice::Protocol::MQTT;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

#ifndef BIT0
#define BIT0 (0x1 << 0)
#endif

#define WIFI_SSID "MSFTGUEST"
#define WIFI_PASS ""

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

using namespace std;

static void calibrateSensor(ADXL355 &sensor, int fifoReadCount);
static void generateMessage(string &out, long fifoData[32][3], int entryCount, const string &deviceId);
static void task_iotsender(void *ignore);
static void initialise_wifi();
static string getDeviceId(const string &connectionString);
static string uppercase(const string &str);
static char op_upper(char in);
static void appendArray(string &out, long fifoData[32][3], int entryCount, int index);

extern "C" void app_main()
{
    nvs_flash_init();
    initialise_wifi();

    if ( xTaskCreate(&task_iotsender, "iotsender_task", 1024 * 10, NULL, 5, NULL) != pdPASS ) {
        printf("Creation of IoT sender task failed\r\n");
    }
}

static void connectionStatusCallback(CIoTHubDevice &iotHubDevice, IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void *userContext)
{
	ESP_LOGI(TAG, "Connection status result=%d;reason=%d", result, reason);
}

static void eventConfirmationCallback(CIoTHubDevice &iotHubDevice, IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContext)
{
    ESP_LOGI(TAG, "Message confirmed with %d", result);
}

static void initializeSensor(ADXL355 &sensor)
{
	sensor.Stop();

	sensor.SetRange(ADXL355::RANGE_VALUES::RANGE_2G);

	ADXL355::RANGE_VALUES rangeValue = sensor.GetRange();
	
	switch (rangeValue)
	{
	case ADXL355::RANGE_VALUES::RANGE_2G:
		ESP_LOGI(TAG, "Range 2g");
		break;
	case ADXL355::RANGE_VALUES::RANGE_4G:
		ESP_LOGI(TAG, "Range 4g");
		break;
	case ADXL355::RANGE_VALUES::RANGE_8G:
		ESP_LOGI(TAG, "Range 8g");
		break;
	default:
		ESP_LOGI(TAG, "Unknown range");
		break;
	}

	sensor.SetOdrLpf(ADXL355::ODR_LPF::ODR_31_25_AND_7_813);
	ESP_LOGI(TAG, "Low pass filter = %d", sensor.GetOdrLpf());
	ESP_LOGI(TAG, "High pass filter = %d", sensor.GetHpfCorner());
}

static void task_iotsender(void *ignore) 
{
    CIoTHubDevice *device = new CIoTHubDevice(connectionString, protocol);
	string deviceId = getDeviceId(connectionString);
	
	device->SetConnectionStatusCallback(connectionStatusCallback, (void *)255);
	device->Start();

	// TODO: Can't assume that the device is on 0x1d
	ADXL355 sensor(0x1d, (gpio_num_t)SDA_PIN, (gpio_num_t)SCL_PIN);
	
	ESP_LOGI(TAG, "Analog Devices ID = %d", (int)sensor.GetAnalogDevicesID());             // Always 173
	ESP_LOGI(TAG, "Analog Devices MEMS ID = %d", (int)sensor.GetAnalogDevicesMEMSID());    // Always 29
	ESP_LOGI(TAG, "Device ID = %d", (int)sensor.GetDeviceId());                            // Always 237 (355 octal)
	ESP_LOGI(TAG, "Revision = %d", (int)sensor.GetRevision());

	initializeSensor(sensor);
	calibrateSensor(sensor, 5);

	sensor.StartTempSensor();
	
	ESP_LOGI(TAG, "Temperature is %.2f", sensor.GetTemperatureF());
				
	long fifoOut[32][3];
	string message;
	int result;
	IOTHUB_CLIENT_RESULT clientResult;
	uint32_t timestamp;
	uint32_t lastTimestamp = 0;
	ADXL355::STATUS_VALUES status;
	
	while (true)
	{
		timestamp = esp_log_timestamp();
		
		status = sensor.GetStatus();

		ESP_LOGD(TAG, "Status=0x%.2x", status);
		ESP_LOGD(TAG, "FIFO count = %d", sensor.GetFifoCount());
		
		if (status & ADXL355::STATUS_VALUES::FIFO_FULL)
		{
			if (status & ADXL355::STATUS_VALUES::FIFO_OVERRUN)
			{
				ESP_LOGW(TAG, "FIFO was overrun");
			}

			if (-1 != (result = sensor.ReadFifoEntries((long *)fifoOut)))
			{
				ESP_LOGD(TAG, "Retrieved %d FIFO entries", result);

				generateMessage(message, fifoOut, result, deviceId);
				ESP_LOGI(TAG, "Sending message");
				ESP_LOGV(TAG, "%s", message.c_str());
				clientResult = device->SendEventAsync(message, eventConfirmationCallback);
				
				if (clientResult == 0)
					ESP_LOGD(TAG, "%d: Message sent", timestamp);
				else
					ESP_LOGW(TAG, "Failed to send message: %d", clientResult);
			}
			else
			{
				ESP_LOGE(TAG, "Fifo read failed");
			}
		}

		if (timestamp - lastTimestamp > 500)
		{
			device->DoWork();
		}
		
		//vTaskDelay(10 / portTICK_RATE_MS);
	}

	vTaskDelete(NULL);
}
static string getDeviceId(const string &connectionString)
{
    static const string DEVICEID = "DEVICEID=";
	
   // Use the device id from the connection string
	size_t start = uppercase(connectionString).find(DEVICEID);

	if (start == string::npos)
	{
		ESP_LOGE(TAG, "Bad connection string");
		return string("");
	}

	start += DEVICEID.length();

	size_t end = connectionString.find(';', start);

	if (end == string::npos)
	{
		ESP_LOGE(TAG, "Bad connection string");
		return string("");
	}

	return connectionString.substr(start, end - start);
}

static string uppercase(const string &str)
{
    string upper;

    upper.resize(str.length());
    transform(str.begin(), str.end(), upper.begin(), op_upper);

    return upper;
}

static char op_upper(char in)
{
    return toupper(in);
}

static void calibrateSensor(ADXL355 &sensor, int fifoReadCount)
{
	long fifoOut[32][3];
	int result;
	int readings = 0;
	long totalx = 0;
	long totaly = 0;
	long totalz = 0;

	memset(fifoOut, 0, sizeof(fifoOut));

	ESP_LOGI(TAG, "Calibrating device with %d FIFO reads\r\n", fifoReadCount);

	sensor.Stop();
	sensor.SetTrim(0, 0, 0);
	sensor.Start();
    vTaskDelay(2000 / portTICK_RATE_MS);

	for (int j = 0; j < fifoReadCount; j++)
	{
		ESP_LOGI(TAG, "Fifo read number %d", j + 1);

		while (!sensor.IsFifoFull())
		{
			vTaskDelay(10 / portTICK_RATE_MS);
		}

		if (-1 != (result = sensor.ReadFifoEntries((long *)fifoOut)))
		{
			ESP_LOGI(TAG, "Retrieved %d entries", result);
			readings += result;

			for (int i = 0; i < result; i++)
			{
				totalx += fifoOut[i][0];
				totaly += fifoOut[i][1];
				totalz += fifoOut[i][2];
			}
		}
		else
		{
			ESP_LOGW(TAG, "Fifo read failed");
		}
	}

	long avgx = totalx / readings;
	long avgy = totaly / readings;
	long avgz = totalz / readings;

	ESP_LOGI(TAG, "\r\nTotal/Average X=%ld/%ld; Y=%ld/%ld; Z=%ld/%ld\r\n",
		totalx, avgx,
		totaly, avgy,
		totalz, avgz);

	sensor.Stop();
	sensor.SetTrim(avgx, avgy, avgz);
	sensor.Start();
    vTaskDelay(2000 / portTICK_RATE_MS);
}

static void generateMessage(string &out, long fifoData[32][3], int entryCount, const string &deviceId)
{
    struct timeval tp;
	char buffer[20];
    
    gettimeofday(&tp, NULL);
    string seconds = itoa(tp.tv_sec, buffer, 10);
    string fraction = itoa(tp.tv_usec, buffer, 10);

    out = "[{ \"deviceid\" : \"";
    out += deviceId;
    out += "\", \"magictime\" : ";
    out += seconds;
    out += ".";
    out += fraction;
    out += ", \"magicx\" : ";
    appendArray(out, fifoData, entryCount, 0);
    out += ", \"magicy\" : ";
    appendArray(out, fifoData, entryCount, 1);
    out += ", \"magicz\" : ";
    appendArray(out, fifoData, entryCount, 2);

    out += "}]";

    return;
}

static void appendArray(string &out, long fifoData[32][3], int entryCount, int index)
{
	// stringstream ss;
    out += "[";
	int64_t gal;
	int32_t galInt;
	int32_t galDec;
	char buffer[20];
    
    for (int i = 0; i < entryCount; i++)
    {
		// ss << ADXL355::ValueToGals(fifoData[i][index]);
        // out += ss.str();
		
		gal = ADXL355::ValueToGalsInt(fifoData[i][index]);
		galInt = gal / 1000;
		galDec = abs(gal - (galInt * 1000));
		
		if (gal < 0)
			out += "-";
		
		out += itoa(galInt, buffer, 10);
		out += ".";
		out += itoa(galDec, buffer, 10);
		
        if (entryCount - i != 1)
            out += ", ";
    }

    out += "]";
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP platform WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config;
	memset(&wifi_config, 0, sizeof(wifi_config));
	strcpy((char *)wifi_config.sta.ssid, WIFI_SSID);
	strcpy((char *)wifi_config.sta.password, WIFI_PASS);
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}
