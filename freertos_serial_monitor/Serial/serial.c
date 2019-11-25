/**
 * @file aardvark.c
 *
 * @brief register access APIs
 *
 *****************************************************************************/

/***** #include statements ***************************************************/

#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <tchar.h>
#include <setupapi.h>
#include <locale.h>
#include <sys/stat.h>
#include <winreg.h>
#include <devguid.h>    // for GUID_DEVCLASS_CDROM etc
#include <cfgmgr32.h>   // for MAX_DEVICE_ID_LEN, CM_Get_Parent and CM_Get_Device_ID
#include <tchar.h>
#include <psapi.h>
#include <stdbool.h>
#pragma comment( lib, "advapi32" )

#include "serial.h"

/***** local macro definitions ***********************************************/
#define INITGUID
#ifdef DEFINE_DEVPROPKEY
#undef DEFINE_DEVPROPKEY
#endif
#ifdef INITGUID
#define DEFINE_DEVPROPKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const DEVPROPKEY DECLSPEC_SELECTANY name = { { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }, pid }
#else
#define DEFINE_DEVPROPKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const DEVPROPKEY name
#endif // INITGUID

#define MAX_NUM_OF_COMP			10

// include DEVPKEY_Device_BusReportedDeviceDesc from WinDDK\7600.16385.1\inc\api\devpkey.h
DEFINE_DEVPROPKEY(DEVPKEY_Device_BusReportedDeviceDesc, 0x540b947e, 0x8b40, 0x45bc, 0xa8, 0xa2, 0x6a, 0x0b, 0x89, 0x4c, 0xbd, 0xa2, 4);     // DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_Device_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);    // DEVPROP_TYPE_STRING

#define ARRAY_SIZE(arr)     (sizeof(arr)/sizeof(arr[0]))

#pragma comment (lib, "setupapi.lib")

typedef BOOL(WINAPI *FN_SetupDiGetDevicePropertyW)(
	__in       HDEVINFO DeviceInfoSet,
	__in       PSP_DEVINFO_DATA DeviceInfoData,
	__in       const DEVPROPKEY *PropertyKey,
	__out      DEVPROPTYPE *PropertyType,
	__out_opt  PBYTE PropertyBuffer,
	__in       DWORD PropertyBufferSize,
	__out_opt  PDWORD RequiredSize,
	__in       DWORD Flags
	);


#define MAX_ID_SIZE		64

typedef struct {
	bool		sema;
	bool		supspense;
	bool		port_avail;
	uint8_t		port_num;
	char		port_name[MAX_ID_SIZE];
} Comport_Id_t;

typedef struct {
	uint8_t port_cnt;
	Comport_Id_t port_id[MAX_NUM_OF_COMP];
	uint8_t port_update;
} Comport_List_t;

static Comport_List_t port_list;

/***** local type definitions ************************************************/

enum reg_access_status
{
    REG_ACCESS_OK,
    REG_ACCESS_ERROR
};


/***** local data objects ****************************************************/
static unsigned int		unique_ids[255];			//serial id number for the adapter
static unsigned short	ports_id[255];				//port id start from 0...num
static bool				available[255];				//status of ports available (true mean available, false mean has been used.)
//static int			avail_port = 0;				//number of port has not been used
static unsigned short	avail_adapters_cnt = 0;		//how many adapters
static bool				avail_port_visited = false;
/***** local prototypes ******************************************************/
static void ListDevices();

/***** local data objects ****************************************************/
bool UpdateDevices(char ***p_connection) {

	if (port_list.port_cnt != port_list.port_update) {
		//copy the name of the comport to upstream.
		for (int i = 0; i < port_list.port_cnt; i++) {
			//uint8_t port_num = port_list.port_id[i].port_num;
			if (port_list.port_id[i].port_avail) {
				*p_connection = (char **)malloc(sizeof(char) * MAX_ID_SIZE);
				memcpy(*p_connection, port_list.port_id[i].port_name, MAX_ID_SIZE);
				p_connection++;
			}
		}
		*p_connection = NULL;
		port_list.port_update = port_list.port_cnt;
		return true;
	}
	*p_connection = NULL;
	return false;
}

bool serial_devices_get(char ***p_connection) {

	//get the list of comports
	ListDevices();
	return UpdateDevices(p_connection);
}

uint8_t convert_to_int(char *ptr) {
	uint8_t val = 0;
	while (*ptr) {
		if (isdigit(*ptr)) {
			val *= 10;
			val += *ptr - '0';
		}
		ptr++;
	}
	return val;
}

static uint8_t get_port_num(char *p_szBuffer) {
	uint8_t val = 0;
	char *ptr1;
	if ((ptr1 = strstr(p_szBuffer, "(COM")) != NULL) {
		val = convert_to_int(ptr1);
	}
	return val;
}

/* 
* check the port to see has been taken
* if not open then the handle should be greater than 0.
*
*/
static bool s_port_available(uint8_t port_id) {
	char port[32];
	int handle;

	sprintf(port, "\\\\.\\COM%d", port_id);
	handle = (uint32_t)CreateFileA(port,
		GENERIC_READ | GENERIC_WRITE,
		0,    // must be opened with exclusive-access
		NULL, // no security attributes
		OPEN_EXISTING, // must use OPEN_EXISTINGq
		0,
		NULL  // hTemplate must be NULL for comm devices
	);
	//handle has not been taken so close it.
	//let the user choose which port to open.
	if (handle > 0)
		CloseHandle((HANDLE)handle);
	return (handle > 0) ? true : false;
}

/*
* check if port is ready in the list
*/
static bool is_port_in_the_list(uint8_t portnum) {
	for (int i = 0; i < port_list.port_cnt; i++) {
		if (port_list.port_id[i].port_num == portnum)
			return true;
	}
	return false;
}

static void serial_port_add(char *portName) {
	uint8_t port = get_port_num(portName);
	//if port is not taken then add to the list.
	if (s_port_available(port)) {
		port_list.port_id[port_list.port_cnt].port_num = port;
		port_list.port_id[port_list.port_cnt].port_avail = true;
		sprintf(port_list.port_id[port_list.port_cnt].port_name, "%s", portName);
		port_list.port_cnt++;
	}
}

// List all USB devices with some additional information
static void ListDevices()
{
	CONST GUID *pClassGuid = NULL;
	unsigned i;
	DWORD dwSize;
	DEVPROPTYPE ulPropertyType;
	HDEVINFO hDevInfo;
	SP_DEVINFO_DATA DeviceInfoData;
	const static LPCTSTR arPrefix[3] = { TEXT("VID_"), TEXT("PID_"), TEXT("MI_") };
	WCHAR szBuffer[4096];
	char szBufferId[4096];

	char szBufferVcp[] = "STMicroelectronics Virtual";

	FN_SetupDiGetDevicePropertyW fn_SetupDiGetDevicePropertyW = (FN_SetupDiGetDevicePropertyW)
		GetProcAddress(GetModuleHandle(TEXT("Setupapi.dll")), "SetupDiGetDevicePropertyW");

	// List all connected USB devices
	hDevInfo = SetupDiGetClassDevs(pClassGuid, "USB", NULL,
		pClassGuid != NULL ? DIGCF_PRESENT : DIGCF_ALLCLASSES | DIGCF_PRESENT);
	if (hDevInfo == INVALID_HANDLE_VALUE)
		return;

	port_list.port_cnt = 0;

	for (i = 0; ; i++) {
		DeviceInfoData.cbSize = sizeof(DeviceInfoData);
		if (!SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData))
			break;

		if (fn_SetupDiGetDevicePropertyW && fn_SetupDiGetDevicePropertyW(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_BusReportedDeviceDesc,
			&ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0)) {
			if (fn_SetupDiGetDevicePropertyW(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_FriendlyName,
				&ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0)) {

				if (strstr(szBufferVcp, (char*)szBuffer)) {
					sprintf(szBufferId, "%ws", szBuffer);
					if (fn_SetupDiGetDevicePropertyW(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_FriendlyName,
						&ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0)) {
						//_tprintf(TEXT("    Device Friendly Name: \"%ls\"\n"), szBuffer);
						if (strstr(szBufferVcp, (char *)szBuffer)) {
							sprintf(szBufferId, "%ws", szBuffer);
							serial_port_add(szBufferId);
						}
					}
				}
			}
		}
	}
	if (hDevInfo) {
		SetupDiDestroyDeviceInfoList(hDevInfo);
	}
	return;
}

/***** end of file ***********************************************************/
