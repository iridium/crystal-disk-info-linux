// Win32 storage structures referenced by AtaSmart.h declarations.
#pragma once
#include "windows.h"

#define IDENTIFY_BUFFER_SIZE        512
#define READ_ATTRIBUTE_BUFFER_SIZE  512
#define READ_THRESHOLD_BUFFER_SIZE  512
#define SMART_CMD                   0xB0
#define ID_CMD                      0xEC
#define READ_ATTRIBUTES             0xD0
#define READ_THRESHOLDS             0xD1
#define ENABLE_SMART                0xD8
#define DISABLE_SMART               0xD9
#define RETURN_SMART_STATUS         0xDA
#define SMART_CYL_LOW               0x4F
#define SMART_CYL_HI                0xC2

#define FILE_DEVICE_CONTROLLER      0x00000004
#define FILE_DEVICE_MASS_STORAGE    0x0000002d
#define METHOD_BUFFERED             0
#define FILE_ANY_ACCESS             0
#define FILE_READ_ACCESS            0x0001
#define FILE_WRITE_ACCESS           0x0002
#define CTL_CODE(t, f, m, a)        (((t) << 16) | ((a) << 14) | ((f) << 2) | (m))

#pragma pack(push, 1)
typedef struct _IDEREGS {
	BYTE bFeaturesReg;
	BYTE bSectorCountReg;
	BYTE bSectorNumberReg;
	BYTE bCylLowReg;
	BYTE bCylHighReg;
	BYTE bDriveHeadReg;
	BYTE bCommandReg;
	BYTE bReserved;
} IDEREGS, *PIDEREGS, *LPIDEREGS;

typedef struct _SENDCMDINPARAMS {
	DWORD   cBufferSize;
	IDEREGS irDriveRegs;
	BYTE    bDriveNumber;
	BYTE    bReserved[3];
	DWORD   dwReserved[4];
	BYTE    bBuffer[1];
} SENDCMDINPARAMS, *PSENDCMDINPARAMS, *LPSENDCMDINPARAMS;

typedef struct _DRIVERSTATUS {
	BYTE  bDriverError;
	BYTE  bIDEError;
	BYTE  bReserved[2];
	DWORD dwReserved[2];
} DRIVERSTATUS, *PDRIVERSTATUS, *LPDRIVERSTATUS;

typedef struct _SENDCMDOUTPARAMS {
	DWORD        cBufferSize;
	DRIVERSTATUS DriverStatus;
	BYTE         bBuffer[1];
} SENDCMDOUTPARAMS, *PSENDCMDOUTPARAMS, *LPSENDCMDOUTPARAMS;
#pragma pack(pop)

typedef struct _DISK_EXTENT {
	DWORD    DiskNumber;
	LONGLONG StartingOffset;
	LONGLONG ExtentLength;
} DISK_EXTENT, *PDISK_EXTENT;
