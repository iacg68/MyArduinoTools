#include <Arduino.h>
#include <Wire.h>

#include "FlashFS.h"
#include "omMemory.h"

namespace om {

#define DEBUG_BUFLEN 128
char _dbg_buffer[DEBUG_BUFLEN];
int	 _flashFs_lastError = FlashFS::ERROR_NONE;

FlashFS::FlashFS(uint8_t deviceAddress, uint32_t deviceSize, uint8_t pageSize)
	: m_dbgEnable(false)
	, m_deviceAddress(deviceAddress)
	, m_deviceSize(deviceSize)
	, m_pageSize(pageSize)
	, m_openFile(-1) // none
{
}

void FlashFS::setDebugEnable(bool mode)
{
	FlashFS::m_dbgEnable = true;
}

int	FlashFS::lastError() const
{
	return _flashFs_lastError;
}

bool FlashFS::openDevice(uint8_t deviceAddress, uint32_t deviceSize, uint8_t pageSize)
{
	m_deviceAddress = deviceAddress;
	m_deviceSize = deviceSize;
	m_pageSize = pageSize;
	return openDevice();
}

bool FlashFS::openDevice()
{
#ifndef FS_USE_SEPARATE_FILE
	close();
#else
	m_openFile = -1;
#endif
	// read version and directory start
	memset(&m_dir, 0, sizeof(Directory));	// restart from scratch
	read(0x0, reinterpret_cast<char*>(&m_dir), sizeof(Directory));

	return (m_dir.magicID  == MAGIC_TLFILESYSTEM)	// ? not mine
        && (m_dir.version  == FILESYSTEMVERSION)	// ? structure changed
		&& (m_dir.numFiles <= MAXFILEENTRIES);		// ? too many entries
}

void FlashFS::format(const char* storageName)
{
#ifndef FS_USE_SEPARATE_FILE
	close();
#else
	m_openFile = -1;
#endif
	memset(&m_dir, 0, sizeof(Directory));	// restart from scratch

	m_dir.magicID  = MAGIC_TLFILESYSTEM;	// "TLFS", const
	m_dir.version  = FILESYSTEMVERSION;		// for now it's version 1.0
	strncpy(m_dir.name, storageName, MAXNAMELEN);
	m_dir.name[MAXNAMELEN] = '\0';
	m_dir.numFiles = 0;
	writeDirectory();
}

void FlashFS::dir() const
{
	static const char* dash = "------------------------------------";
	uint32_t used  = pageAlign(sizeof(Directory), true);

/*
		------------------------------------
		Flash: Games         Version:  1-000
		Idx File       Size   Start
		  0 Hello         600 0x000140
		  1 Data          202 0x0003c0

		  1216 bytes used,  31552 bytes free
		------------------------------------
*/

	Serial.println(dash);
	snprintf(_dbg_buffer, DEBUG_BUFLEN
		    , "Flash: %-10s    Version: %2u-%03u"
			, storageName(), (storageVersion() >> 8) & 0x0FF
				           ,  storageVersion()       & 0x0FF);
	Serial.println(_dbg_buffer);
	Serial.println("Idx File       Size   Start");
	for(int i = 0; i < flashFs.numFiles(); ++i)
	{
		const auto ep = flashFs.fileEntry(i);
		snprintf(_dbg_buffer, DEBUG_BUFLEN
				, "%3d %-10s %6lu 0x%06lx"
				, i, ep->name, ep->size, ep->startAddress);
		Serial.println(_dbg_buffer);
		used += pageAlign(ep->size, true);
	}
	Serial.println();
	snprintf(_dbg_buffer, DEBUG_BUFLEN
		    , "%6lu bytes used, %6lu bytes free"
	        , used, m_deviceSize - used);
	Serial.println(_dbg_buffer);
	Serial.println(dash);
}

const FlashFS::FileEntry* FlashFS::fileEntry(int idx) const
{
	if ((idx < 0) || (uint32_t(idx) >= m_dir.numFiles))
		return nullptr;
	return m_dir.files + idx;
}

const FlashFS::FileEntry* FlashFS::grantFileAccess()
{
	auto current = fileEntry(m_openFile);
	m_openFile = -1;
	return current;
}

bool FlashFS::exists(const char* fileName) const
{
	return findFile(fileName) >= 0;
}

int FlashFS::deleteFile(const char* fileName)
{
	int idx = findFile(fileName);
	if (idx < 0)
		return latchError(ERROR_FILE_NOT_FOUND);

	if (m_openFile == idx)
#ifndef FS_USE_SEPARATE_FILE
		close(); // forced close.
#else
		m_openFile = -1;
#endif

	removeFilesEntry(idx);
	writeDirectory();

	return latchError(ERROR_NONE);
}

int FlashFS::createFile(const char* fileName, uint32_t size)
{
	// a chance to relocate file, looking for better place
	// then performing deleteFile & createFile in one step:
	int existingFile = findFile(fileName);
	if (existingFile >= 0)
		removeFilesEntry(existingFile);

	if (m_dir.numFiles == MAXFILEENTRIES)
		return latchError(ERROR_DIR_TABLE_FULL);

	GapInfo gap = findBestFittingGap(size);
	if (gap.insertAt < 0)
		return latchError(gap.insertAt);

	insertFilesEntry(gap.insertAt);
	if (m_dbgEnable)
	{
		snprintf(_dbg_buffer, DEBUG_BUFLEN, "cr: [%3d] %s addr 0x%06lx, size %ld of %ld"
									 , gap.insertAt
									 , fileName, gap.startAddress, size, gap.gapSize);
		Serial.println(_dbg_buffer);
	}

	FileEntry& newEntry = m_dir.files[gap.insertAt];
	newEntry.startAddress = gap.startAddress;
	newEntry.size = size;
	strncpy(newEntry.name, fileName, MAXNAMELEN);
	newEntry.name[MAXNAMELEN] = '\0';

	writeDirectory();

	// check, where we've enough space to open the file
	return openFile(fileName);
}

int FlashFS::openFile(const char* fileName)
{
	m_openFile = findFile(fileName);
#ifndef FS_USE_SEPARATE_FILE
	m_filePos = 0;
#endif
	if (m_openFile < 0)
		return latchError(ERROR_FILE_NOT_FOUND);	// not found

	return latchError(int(m_dir.files[m_openFile].size));
}

#ifndef FS_USE_SEPARATE_FILE
int FlashFS::cleanFile(uint32_t fillWord)
{
	if (m_openFile < 0)
		latchError(ERROR_FILE_NOT_OPENED);

	const int tempSize = m_pageSize / sizeof(uint32_t);
	unique_ptr<uint32_t, _array_destructor> temp = new uint32_t[tempSize];
	for(int i = 0; i < tempSize; ++i)
		temp[i] = fillWord;
	
	const uint32_t restorePos = pos();

	setPos(0);
	uint32_t bytesToWrite = m_dir.files[m_openFile].size;
	while (bytesToWrite > 0)
	{
		uint32_t chunkSize = bytesToWrite;
		if (chunkSize > tempSize * sizeof(uint32_t))
			chunkSize = tempSize * sizeof(uint32_t);
		write(temp.get(), chunkSize);
		bytesToWrite -= chunkSize;
	}

	// cleanup:
	setPos(restorePos);
	return latchError(m_dir.files[m_openFile].size);
}
#endif

#ifndef FS_USE_SEPARATE_FILE
void FlashFS::close()
{
	m_openFile = -1;
	m_filePos = 0;
}
#endif

#ifndef FS_USE_SEPARATE_FILE
bool FlashFS::eof() const
{
	return (m_openFile < 0) || (m_filePos >= m_dir.files[m_openFile].size);
}
#endif

#ifndef FS_USE_SEPARATE_FILE
uint32_t FlashFS::setPos(int32_t pos)
{
	if (m_openFile < 0)
		latchError(ERROR_FILE_NOT_OPENED);
	else if (pos < 0)
		latchError(ERROR_POSITION_NEGATIVE);
	else if (uint32_t(pos) < m_dir.files[m_openFile].size)
		m_filePos = uint32_t(pos);
	else
		latchError(ERROR_POSITION_BEYOND_EOF);

	return m_filePos;
}
#endif

#ifndef FS_USE_SEPARATE_FILE
uint32_t FlashFS::movePos(int32_t offset)
{
	return setPos(int(m_filePos) + offset);
}
#endif

#ifndef FS_USE_SEPARATE_FILE
int FlashFS::write(const void* data, uint32_t size)
{
	if (m_openFile < 0)
		return latchError(ERROR_FILE_NOT_OPENED);		// closed

	// check available space
	if (m_filePos + size > m_dir.files[m_openFile].size)
		return latchError(ERROR_WRITING_BEYOND_EOF);	// not enough space

	if (size == 0)
		return latchError(0);

	uint32_t addr = m_dir.files[m_openFile].startAddress + m_filePos;
	
	if (m_dbgEnable)
		Serial.println("flashing data...");
	
	write(addr, reinterpret_cast<const char*>(data), size);
	m_filePos += size;
	return latchError(size);
}
#endif

#ifndef FS_USE_SEPARATE_FILE
int FlashFS::read(void* data, uint32_t size)
{
	if (m_openFile < 0)
		return latchError(ERROR_FILE_NOT_OPENED);		// closed

	// check available space
	if (m_filePos + size > m_dir.files[m_openFile].size)
		return latchError(ERROR_READING_BEYOND_EOF);	// not enough space

	if (size == 0)
		return latchError(0);

	uint32_t addr = m_dir.files[m_openFile].startAddress + m_filePos;
	read(addr, reinterpret_cast<char*>(data), size);
	m_filePos += size;
	return latchError(size);
}
#endif

int FlashFS::latchError(int val) const
{
	_flashFs_lastError = (val < 0) ? val : ERROR_NONE;
	if (m_dbgEnable && (_flashFs_lastError < 0))
	{
		Serial.print("ERRROR FlashFS: ");
		Serial.println(_flashFs_lastError);
	}
	return val;
}

int FlashFS::findFile(const char* fileName) const
{
	for(int i = 0; uint32_t(i) < m_dir.numFiles; ++i)
		if(strncmp(fileName, m_dir.files[i].name, 8) == 0)
			return i;
	return ERROR_FILE_NOT_FOUND;
}

FlashFS::GapInfo FlashFS::findBestFittingGap(uint32_t size) const
{
	// looking for smallest gap, large enough to hold the requested size
	// e.g.
	//	[DIR] <-----> [FILE1] <---> [FILE2] <----------> [FILE3] <---.....----> [END]
	//	         7              5                12                    1000
	// looking for gap to hold size 4 should return the gap between FILE1 and FILE2.
	//
	GapInfo bestFit = {ERROR_NOT_ENOUGH_SPACE, 0, 0};
	for(int insertAt = 0; insertAt <= int(m_dir.numFiles); ++insertAt)
	{
		const uint32_t startSegment = (insertAt == 0)
			? pageAlign(sizeof(Directory), true)	// in front of [FILE1]
			: pageAlign(m_dir.files[insertAt-1].startAddress 
			          + m_dir.files[insertAt-1].size, true);

		const uint32_t endSegment = (insertAt == int(m_dir.numFiles))
			? m_deviceSize							// up to [END]
			: m_dir.files[insertAt].startAddress;	// is page aligned

		const uint32_t gapSize = endSegment - startSegment;
		if (gapSize < size)
			continue;

		if ((bestFit.insertAt < 0) || (gapSize < bestFit.gapSize))
		{
			bestFit.insertAt     = insertAt;
			bestFit.startAddress = startSegment;
			bestFit.gapSize      = gapSize; 
		}
	}
	return bestFit;
}

uint32_t FlashFS::pageAlign(uint32_t address, bool upwards) const
{
	const uint32_t offsetInPage = address % m_pageSize;
	if (upwards & (offsetInPage > 0))
		return address - offsetInPage + m_pageSize;
	else
		return address - offsetInPage;
}

void FlashFS::insertFilesEntry(int atIdx)
{
	for(int i = int(m_dir.numFiles) - 1; i >= atIdx; --i)
		m_dir.files[i+1] = m_dir.files[i];
	++m_dir.numFiles;
}

void FlashFS::removeFilesEntry(int atIdx)
{
	for (int i = atIdx; uint32_t(i) < m_dir.numFiles - 1; ++i)
		m_dir.files[i] = m_dir.files[i+1];
	--m_dir.numFiles;
}

void FlashFS::writeDirectory()
{
	if (m_dbgEnable)
		Serial.println("flashing dir...");
	
	write(0x0, reinterpret_cast<const char*>(&m_dir), sizeof(m_dir));
}

uint8_t FlashFS::beginAndWriteAddress(uint32_t address)
{
	uint8_t modifiedDevAddress = m_deviceAddress;

	// 0x050 is the mandatory EEPROM Address forming I2C device address.
	//
	// If more than one EEPROM is connected, up to three hardware encoded 
	// address pins A0, A1, A2 are available, resulting in addresses
	//	0x050 ... 0x057
	//
	// However, since the number of address bytes used in the communication 
	// should be minimized, memory may be organized in up to 8 memory pages
	// addressed by P2, P1, P0 replacing the external A2 .. A0 pins.
	switch (m_deviceSize)
	{
	case EEPROMSize2k:		// using P0, P1, P2,		e.g. Atmel AT24C16C
		modifiedDevAddress &= ~ 0x07;
		modifiedDevAddress |= (address >> 8) & 0x07;
		break;

	case EEPROMSize128k:	// using P0					e.g. Atmel AT24CM01
		modifiedDevAddress &= ~ 0x01;
		modifiedDevAddress |= (address >> 16) & 0x01;
		break;
	case EEPROMSize256k:	// using P0, P1				e.g. Atmel AT24CM02
		modifiedDevAddress &= ~ 0x03;
		modifiedDevAddress |= (address >> 16) & 0x03;
		break;
#ifdef FLASHFS_SUPPORT_FOR_HIGHCAPACITY
	case EEPROMSize512k:	// using P0, P1, P2			??? 
		modifiedDevAddress &= ~ 0x07;
		modifiedDevAddress |= (address >> 16) & 0x07;
		break;

	case EEPROMSize32M:		// using P0					???
		modifiedDevAddress &= ~ 0x01;
		modifiedDevAddress |= (address >> 16) & 0x01;
		break;
	case EEPROMSize64M:		// using P0, P1				???
		modifiedDevAddress &= ~ 0x03;
		modifiedDevAddress |= (address >> 24) & 0x03;
		break;
	case EEPROMSize128M:	// using P0, P1, P2			???
		modifiedDevAddress &= ~ 0x07;
		modifiedDevAddress |= (address >> 24) & 0x07;
		break;
#endif
	}

	Wire.beginTransmission(modifiedDevAddress);
	// now all remaining address bytes, MSB to LSB: 
#ifdef FLASHFS_SUPPORT_FOR_HIGHCAPACITY
	if (m_deviceSize > EEPROMSize128M)
		Wire.write((int)(address >> 24)	& 0x0FF));
	if (m_deviceSize > EEPROMSize512k)
		Wire.write((int)(address >> 16)	& 0x0FF));
#endif
	if (m_deviceSize > EEPROMSize2k)
		Wire.write((int)(address >> 8)	& 0x0FF);	
	Wire.write((int)(address)		& 0x0FF);

	return modifiedDevAddress;
}

void FlashFS::write(uint32_t address, const char* data, uint32_t size)
{
	// keep in mind: 
	//	- don't write blocks crossing page boundaries
	//  - don't write blocks larger than arduinos Wire-lib supports
	while(size > 0)
	{
		bool eop = false;
		uint32_t chunkSize = BUFFER_LENGTH - 2;	// 2 bytes requireed for sending address
		if (chunkSize > size)					// more than required?
			chunkSize = size;
		uint32_t spaceOnPage = m_pageSize - (address % m_pageSize);
		if (chunkSize > spaceOnPage)			// only up to page bounds
		{
			eop = true;
			chunkSize = spaceOnPage;
		}

		if (m_dbgEnable)
		{
			snprintf(_dbg_buffer, DEBUG_BUFLEN, "wr: addr 0x%06lx, size %6ld, chunk %6ld >> "
							   , address, size, chunkSize);
			Serial.print(_dbg_buffer);
		}

		beginAndWriteAddress(address);
		for (uint32_t i = 0; i < chunkSize; ++i)
		{
			Wire.write((int)(data[i]));
			if (m_dbgEnable)
			{
				snprintf(_dbg_buffer, DEBUG_BUFLEN, "%02x ", data[i] & 0x0FF); 
				Serial.print(_dbg_buffer);
			}
		}
		Wire.endTransmission();
		delay(5);	// give EEPROM time to flash the page
		if (m_dbgEnable)
			Serial.println(eop ? "<p>" : "");

		// move to next chunk
		address += chunkSize;
		data	+= chunkSize;
		size	-= chunkSize;
	}
}

void FlashFS::read(uint32_t address, char* data, uint32_t size)
{
	// keep in mind: 
	//  - don't read blocks larger than arduinos Wire-lib supports
	while(size > 0)
	{
		uint32_t chunkSize = BUFFER_LENGTH;
		if (chunkSize > size)					// more than required?
			chunkSize = size;
		
		uint8_t modifiedDevAddress = beginAndWriteAddress(address);
		Wire.endTransmission();	// terminating pseudo write, switch back to read
		Wire.requestFrom(modifiedDevAddress, chunkSize);

		if (m_dbgEnable)
		{
			snprintf(_dbg_buffer, DEBUG_BUFLEN
					, "rd: addr 0x%06lx, size %6ld, chunk %6ld << "
					, address, size, chunkSize);
			Serial.print(_dbg_buffer);
		}

		for (uint32_t i = 0; i < chunkSize; ++i)
		{
			if (Wire.available())
			{
				data[i] = Wire.read();
				if (m_dbgEnable)
				{
					snprintf(_dbg_buffer, DEBUG_BUFLEN, "%02x ", data[i] & 0x0FF); 
					Serial.print(_dbg_buffer);
				}
			}
			else if (m_dbgEnable)
				Serial.print("__ ");
		}
		if (m_dbgEnable)
			Serial.println();

		// move to next chunk
		address += chunkSize;
		data	+= chunkSize;
		size	-= chunkSize;
	}
}

// ==================================================================

File::File()
{
}

File::File(const File& other)
	: m_lastError{other.m_lastError}
	, m_address{other.m_address}
	, m_filePos{other.m_filePos}
	, m_fileSize{other.m_fileSize}
{
}

File::File(const char* fileName)
{
	openFile(fileName);
}

File::File(const char* fileName, uint32_t size)
{
	createFile(fileName, size);
}

int File::createFile(const char* fileName, uint32_t size)
{
	const auto result = flashFs.createFile(fileName, size);
	if (result < 0)
		return latchError(result);

	const auto entry = flashFs.grantFileAccess();
	m_address  = entry->startAddress;
	m_fileSize = entry->size;
	return latchError(result);
}

int File::openFile(const char* fileName)
{
	const auto result = flashFs.openFile(fileName);
	if (result < 0)
		return latchError(result);
	
	const auto entry = flashFs.grantFileAccess();
	m_address  = entry->startAddress;
	m_fileSize = entry->size;
	return latchError(result);
}

int File::cleanFile(uint32_t fillWord)
{
	if (m_address == 0x0)
		latchError(FlashFS::ERROR_FILE_NOT_OPENED);

	const int tempSize = flashFs.pageSize() / sizeof(uint32_t);
	unique_ptr<uint32_t, _array_destructor> temp = new uint32_t[tempSize];
	for(int i = 0; i < tempSize; ++i)
		temp[i] = fillWord;
	
	const uint32_t restorePos = pos();

	setPos(0);
	uint32_t bytesToWrite = m_fileSize;
	while (bytesToWrite > 0)
	{
		uint32_t chunkSize = bytesToWrite;
		if (chunkSize > tempSize * sizeof(uint32_t))
			chunkSize = tempSize * sizeof(uint32_t);
		write(temp.get(), chunkSize);
		bytesToWrite -= chunkSize;
	}

	// cleanup:
	setPos(restorePos);
	return latchError(m_fileSize);
}

void File::close()
{
	m_address = 0x0;
	m_filePos = 0x0;
	m_fileSize = 0x0;
	latchError(FlashFS::ERROR_NONE);
}

// data:
bool File::eof() const
{
	return (m_address == 0x0) 
		|| (m_filePos >= m_fileSize);
}

uint32_t File::setPos(int32_t pos)
{
	if (m_address == 0x0)
		latchError(FlashFS::ERROR_FILE_NOT_OPENED);
	else if (pos < 0)
		latchError(FlashFS::ERROR_POSITION_NEGATIVE);
	else if (uint32_t(pos) < m_fileSize)
		m_filePos = uint32_t(pos);
	else
		latchError(FlashFS::ERROR_POSITION_BEYOND_EOF);

	return m_filePos;
}

uint32_t File::movePos(int32_t offset)
{
	return setPos(int(m_filePos) + offset);
}

// generic: write block of data to sequential file
int File::write(const void* data, uint32_t size)
{
	if (m_address == 0x0)
		return latchError(FlashFS::ERROR_FILE_NOT_OPENED);		// closed

	// check available space
	if (m_filePos + size > m_fileSize)
		return latchError(FlashFS::ERROR_WRITING_BEYOND_EOF);	// not enough space

	if (size == 0)
		return latchError(0);

	uint32_t addr = m_address + m_filePos;
	flashFs.write(addr, reinterpret_cast<const char*>(data), size);
	return latchError(size);
}

// generic: read block of data from sequential file
int File::read(void* data, uint32_t size)
{
	if (m_address == 0x0)
		return latchError(FlashFS::ERROR_FILE_NOT_OPENED);		// closed

	// check available space
	if (m_filePos + size > m_fileSize)
		return latchError(FlashFS::ERROR_READING_BEYOND_EOF);	// not enough space

	if (size == 0)
		return latchError(0);

	uint32_t addr = m_address + m_filePos;
	flashFs.read(addr, reinterpret_cast<char*>(data), size);
	m_filePos += size;
	return latchError(size);
}

int File::latchError(int val)
{
	m_lastError = (val < 0) ? val : FlashFS::ERROR_NONE;
	return val;
}

} // namespace

// provide singleton
om::FlashFS flashFs(0x50, EEPROMSize32k, 64);
