#ifndef FLASHFS_H
#define FLASHFS_H

#include <stdint.h>

namespace om {

// FlashFS requires at least 2k x 8 eeproms, since the directory takes already 320 bytes.

// one adress byte inline
// using P0, P1, P2 in device address
#define EEPROMSize2k         (uint32_t(1) << 11)

// two adress bytes inline
#define EEPROMSize4k         (uint32_t(1) << 12)
#define EEPROMSize8k         (uint32_t(1) << 13)
#define EEPROMSize16k        (uint32_t(1) << 14)
#define EEPROMSize32k        (uint32_t(1) << 15)
#define EEPROMSize64k        (uint32_t(1) << 16)

// using P0, P1, P2 in device address
#define EEPROMSize128k       (uint32_t(1) << 17)
#define EEPROMSize256k       (uint32_t(1) << 18)

#ifdef  FLASHFS_SUPPORT_FOR_HIGHCAPACITY 
// Known so far: ATMEL does not provide >256k x 8 EEEPROMS with I2C
#define EEPROMSize512k       (uint32_t(1) << 19)
// three adress bytes inline
#define EEPROMSize1M         (uint32_t(1) << 20)
#define EEPROMSize2M         (uint32_t(1) << 21)
#define EEPROMSize4M         (uint32_t(1) << 22)
#define EEPROMSize8M         (uint32_t(1) << 23)
#define EEPROMSize16M        (uint32_t(1) << 24)

// using P0, P1, P2 in device address
#define EEPROMSize32M        (uint32_t(1) << 25)
#define EEPROMSize64M        (uint32_t(1) << 26)
#define EEPROMSize128M       (uint32_t(1) << 27)
// four adress bytes inline
#define EEPROMSize256M       (uint32_t(1) << 28)
#define EEPROMSize512M       (uint32_t(1) << 29)
#define EEPROMSize1G         (uint32_t(1) << 30)
#define EEPROMSize2G         (uint32_t(1) << 31)
#endif

class FlashFS
{
private:
	// not visible outside.
	static const uint32_t MAGIC_TLFILESYSTEM	= 0x544C4653;
	static const uint32_t FILESYSTEMVERSION		= 0x0100;	// major 01, minor 00
	static const uint32_t MAXFILEENTRIES		= 16;
	static const uint32_t MAXNAMELEN			= 9;
	static const uint32_t DEFAULT_EEPROM_ADDR	= 0x050;

public:
	static const int ERROR_NONE					=  0;
	static const int ERROR_FILE_NOT_FOUND		= -1;
	static const int ERROR_FILE_NOT_OPENED		= -2;
	static const int ERROR_WRITING_BEYOND_EOF	= -3;
	static const int ERROR_READING_BEYOND_EOF	= -4;
	static const int ERROR_POSITION_NEGATIVE	= -5;
	static const int ERROR_POSITION_BEYOND_EOF	= -6;
	static const int ERROR_DIR_TABLE_FULL		= -7;
	static const int ERROR_NOT_ENOUGH_SPACE		= -8;

	struct GapInfo
	{
		int			insertAt;
		uint32_t	startAddress;
		uint32_t	gapSize;
	};

	struct FileEntry
	{
		uint32_t	startAddress;				// 4 bytes
		char		name[MAXNAMELEN+1];			// 10 bytes
		uint32_t	size;						// 4 bytes
	};	// 18 bytes

	FlashFS(uint8_t deviceAddress, uint32_t deviceSize, uint8_t pageSize);

	void setDebugEnable(bool mode);
	int	lastError() const;

	bool openDevice(uint8_t deviceAddress, uint32_t deviceSize, uint8_t pageSize);
	bool openDevice();
	void format(const char* storageName);
	void dir() const;

	// directory:
	const char* storageName() const
	{
		return m_dir.name;
	}

	uint16_t storageVersion() const
	{
		return m_dir.version;
	}

	int	numFiles() const
	{
		return m_dir.numFiles;
	}

	const FileEntry* fileEntry(int idx);

	// files:
	bool exists(const char* fileName);
	int deleteFile(const char* fileName);
	int createFile(const char* fileName, uint32_t size);
	int openFile(const char* fileName);
	int cleanFile(uint32_t fillWord = 0x0);
	void close();

	// data:
	bool eof() const;
	uint32_t pos() const
	{
		return m_filePos;
	}
	uint32_t setPos(int32_t pos);
	uint32_t movePos(int32_t offset);

	// generic: write block of data to sequential file
	int write(const void* data, uint32_t size);

	// usable for mem-copyable data
	template<typename T>
	int write(T data)
	{
		return write(&data, sizeof(T));
	}

	// generic: read block of data from sequential file
	int read(void* data, uint32_t size);
	
	// usable for mem-copyable data
	template<typename T>
	T read()
	{
		T data;
		read(&data, sizeof(T));
		return data;
	}

private:

	struct Directory {
		uint32_t	magicID;				//   4 bytes
		uint16_t	version;				//   2 bytes
		char		name[MAXNAMELEN+1];		//  10 bytes
		uint16_t	reserved[6];			//   2 bytes x 6
		uint32_t	numFiles;				//   4 bytes
		FileEntry	files[MAXFILEENTRIES];	//  18 bytes x 16
	};										// 320 bytes

	// helper
	int latchError(int val) const;
	int findFile(const char* fileName) const;
	GapInfo findBestFittingGap(uint32_t size) const;
	uint32_t pageAlign(uint32_t address, bool upwards) const;
	void insertFilesEntry(int atIdx);
	void removeFilesEntry(int atIdx);

	// doing the IO to the EEPROM
	void writeDirectory();
	uint8_t beginAndWriteAddress(uint32_t address);
	void write(uint32_t address, const char* data, uint32_t size);
	void read(uint32_t address, char* data, uint32_t size);

	bool		m_dbgEnable;
	uint8_t		m_deviceAddress;
	uint32_t	m_deviceSize;
	uint8_t		m_pageSize;

	Directory	m_dir;
	int32_t		m_openFile;
	uint32_t	m_filePos;
};

}

extern om::FlashFS flashFs;

#endif