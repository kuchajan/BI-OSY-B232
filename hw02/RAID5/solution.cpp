#ifndef __PROGTEST__
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
using namespace std;

constexpr int SECTOR_SIZE = 512;
constexpr int MAX_RAID_DEVICES = 16;
constexpr int MAX_DEVICE_SECTORS = 1024 * 1024 * 2;
constexpr int MIN_DEVICE_SECTORS = 1 * 1024 * 2;

constexpr int RAID_STOPPED = 0;
constexpr int RAID_OK = 1;
constexpr int RAID_DEGRADED = 2;
constexpr int RAID_FAILED = 3;

struct TBlkDev {
	int m_Devices;
	int m_Sectors;
	// int device, int sectorNr, void *data, int sectorCnt
	int (*m_Read)(int, int, void *, int);
	// int device, int sectorNr, const void *data, int sectorCnt
	int (*m_Write)(int, int, const void *, int);
};
#endif /* __PROGTEST__ */

class CStatus {
private:
	uint16_t m_status;

public:
	CStatus(uint16_t status = 0) : m_status(status) {}
	bool getStatus(int dev) const {
		return ((m_status >> dev) & 0b1);
	}
	void setStatus(int dev, bool bit) {
		if (bit)
			m_status |= (0b1 << dev);
		else
			m_status &= !(0b1 << dev);
	}
	bool operator==(const CStatus &other) const {
		return m_status == other.m_status;
	}
	bool operator!=(const CStatus &other) const {
		return m_status != other.m_status;
	}
};

struct SOverhead {
	size_t m_version;
	CStatus m_status;
	SOverhead(size_t version = 0, int diskCount = 0) : m_version(version), m_status(0xffff >> (16 - diskCount)) {}
	SOverhead(size_t version, const CStatus &status) : m_version(version), m_status(status) {}

	bool operator==(const SOverhead &other) const {
		return m_version == other.m_version && m_status == other.m_status;
	}
	bool operator!=(const SOverhead &other) const {
		return m_version != other.m_version || m_status != other.m_status;
	}
};

class CRaidVolume {
protected:
	TBlkDev m_dev;
	bool m_hasDev;
	SOverhead m_overhead;
	int m_RAIDStatus;

	void markFailDisk(int disk) {
		m_overhead.m_status.setStatus(disk, false);
		m_RAIDStatus = m_RAIDStatus == RAID_OK ? RAID_DEGRADED : RAID_FAILED;
	}

	/**
	 * @brief: Reads data from a sector of a device, no matter if it's overhead or parity
	 * @note: buffer has to be provided
	 */
	bool readSector(int dev, int row, uint8_t *buf, unsigned int length = 1) {
		return m_dev.m_Read(dev, row, buf, length) == length * SECTOR_SIZE;
	}
	/**
	 * @brief: Writes data from buffer to a sector of a device, no matter if it's overhead or parity
	 * @note: buffer has to be provided
	 */
	bool writeSector(int dev, int row, const uint8_t *buf, unsigned int length = 1) {
		return m_dev.m_Write(dev, row, buf, length) == length * SECTOR_SIZE;
	}

	int getRow(int sector) const {
		return sector / (m_dev.m_Devices - 1);
	}

	int getParityDevByRow(int row) const {
		return row % m_dev.m_Devices;
	}

	int getParityDevBySector(int sector) const {
		return getParityDevByRow(getRow(sector));
	}

	int getDevice(int sector) const {
		int device = sector % (m_dev.m_Devices - 1);
		return device >= getParityDevBySector(sector) ? device + 1 : device;
	}

	bool getOverhead(int dev, SOverhead &overhead) {
		uint8_t buf[SECTOR_SIZE];
		bool result = readSector(dev, m_dev.m_Sectors - 1, buf);
		if (!result)
			return false;
		memcpy(&overhead, buf, sizeof(SOverhead));
		return true;
	}
	bool setOverhead(int dev) {
		uint8_t buf[SECTOR_SIZE];
		memcpy(buf, &m_overhead, sizeof(SOverhead));
		return writeSector(dev, m_dev.m_Sectors - 1, buf);
	}

	bool calculateParity(uint8_t *buf, int row, int skipDev = -1) {
		if (skipDev == -1)
			skipDev = getParityDevByRow(row);
		if ((m_RAIDStatus == RAID_DEGRADED && m_overhead.m_status.getStatus(skipDev) == true) || (m_RAIDStatus == RAID_FAILED))
			return false;
		uint8_t tmpBuf[SECTOR_SIZE];
		bool first = true;
		for (int disk = 0; disk < m_dev.m_Devices; ++disk) {
			if (disk == skipDev)
				continue;
			if (first) {
				first = false;
				if (!readSector(disk, row, buf)) {
					markFailDisk(disk);
					return false;
				}
			} else {
				if (!readSector(disk, row, tmpBuf)) {
					markFailDisk(disk);
					return false;
				}
				for (int byte = 0; byte < SECTOR_SIZE; ++byte)
					buf[byte] ^= tmpBuf[byte];
			}
		}
		return true;
	}

public:
	/**
	 * @brief: Writes initialization data to a potential RAID device
	 * @returns: True when the device is valid and was successfully created
	 */
	static bool create(const TBlkDev &dev) {
		// Check disk count
		if (dev.m_Devices < 3 || dev.m_Devices > MAX_RAID_DEVICES)
			return false;
		// Check if overhead can fit
		if (sizeof(SOverhead) > SECTOR_SIZE)
			return false;

		uint8_t buf[SECTOR_SIZE];
		{
			SOverhead overhead(1, 0xffff >> (MAX_RAID_DEVICES - dev.m_Devices));
			memcpy(buf, &overhead, sizeof(SOverhead));
		}

		bool err = false;

		for (int disk = 0; disk < dev.m_Devices; ++disk)
			if (dev.m_Write(disk, dev.m_Sectors - 1, buf, 1) != 1)
				err = true; // even though I know an error occured, I still want to initialize rest of the devices, just in case

		return !err;
	}

	CRaidVolume() : m_overhead() {
		m_RAIDStatus = RAID_STOPPED;

		m_hasDev = false;
		m_dev.m_Devices = 0;
		m_dev.m_Read = nullptr;
		m_dev.m_Sectors = 0;
		m_dev.m_Write = nullptr;
	}

	/**
	 * @brief: Initializes a created or a stopped RAID
	 * @returns: The status of the started RAID device
	 */
	int start(const TBlkDev &dev) {
		if (m_RAIDStatus != RAID_STOPPED)
			return m_RAIDStatus;
		m_dev = TBlkDev(dev);
		m_hasDev = true;

		// get the status of the device
		int fail = 0;
		m_overhead = SOverhead(0, m_dev.m_Devices);
		for (int disk = 0; disk < m_dev.m_Devices; ++disk) {
			SOverhead loaded;
			if (!m_overhead.m_status.getStatus(disk) || !getOverhead(disk, loaded) || loaded.m_version == 0 || loaded.m_version < m_overhead.m_version) {
				m_overhead.m_status.setStatus(disk, false);
				++fail;
				continue;
			}
			if (loaded.m_version > m_overhead.m_version) {
				// trust this overhead
				m_overhead = loaded;
				// set previous disks as failed - so don't trust
				for (int prevDisk = 0; prevDisk < disk; ++prevDisk)
					m_overhead.m_status.setStatus(prevDisk, false);
				// calculate current count of successes and fails
				fail = disk; // because disks from 0 to disk is failed
				continue;
			}
			// success
		}

		// update version
		++m_overhead.m_version;
		{ // over-complicated for-like while loop which resets on fail, you love to see it
			int disk = 0;
			while (disk < m_dev.m_Devices) {
				if (m_overhead.m_status.getStatus(disk)) {
					if (!setOverhead(disk)) {
						// I am so tired
						m_overhead.m_status.setStatus(disk, false);
						++m_overhead.m_version;
						++fail;
						// start loop again
						disk = 0;
						continue;
					}
				}
				++disk;
			}
		}

		if (fail == 0)
			m_RAIDStatus = RAID_OK;
		else if (fail == 1)
			m_RAIDStatus = RAID_DEGRADED;
		else
			m_RAIDStatus = RAID_FAILED;

		return m_RAIDStatus;
	}

	/**
	 * @brief: Stops a RAID device and writes overhead information to valid disks
	 * @returns: RAID_STOPPED
	 */
	int stop() {
		if (m_RAIDStatus == RAID_STOPPED)
			return RAID_STOPPED;
		++m_overhead.m_version;
		for (int disk = 0; disk < m_dev.m_Devices; ++disk) {
			if (m_overhead.m_status.getStatus(disk)) {
				if (!setOverhead(disk)) {
					// update overhead and start over
					m_overhead.m_status.setStatus(disk, false);
					return stop(); // ? potential point of optimalization: maybe reset for loop instead of recursion
				}
			}
		}
		m_RAIDStatus = RAID_STOPPED;
		return RAID_STOPPED;
	}

	int resync();

	/**
	 * @return: The current status of the RAID device
	 */
	int status() const {
		return m_RAIDStatus;
	}

	/**
	 * @return: The ammount of sectors available to tester
	 */
	int size() const {
		// m_dev.m_Sectors - 1: all sectors of singular device, except one for overhead
		// m_dev.m_Devices - 1: all devices, except one for parity
		return m_hasDev && (m_RAIDStatus == RAID_OK || m_RAIDStatus == RAID_DEGRADED) ? (m_dev.m_Sectors - 1) * (m_dev.m_Devices - 1) : 0;
	}

	bool read(int secNr, void *data, int secCnt);
	bool write(int secNr, const void *data, int secCnt);
};

#ifndef __PROGTEST__
/* SW RAID5 - basic test
 *
 * The testing of the RAID driver requires a backend (simulating the underlying disks).
 * Next, the tests of your RAID implemetnation are needed. To help you with the implementation,
 * a sample backend is implemented in this file. It provides a quick-and-dirty
 * implementation of the underlying disks (simulated in files) and a few Raid... function calls.
 *
 * The implementation in the real testing environment is different. The sample below is a
 * minimalistic disk backend which matches the required interface. The backend, for instance,
 * cannot simulate a crashed disk. To test your Raid implementation, you will have to modify
 * or extend the backend.
 *
 * Next, you will have to add some raid testing. There is a few Raid... functions called from within
 * main(), however, the tests are incomplete. For instance, RaidResync () is not tested here. Once
 * again, this is only a starting point.
 */

constexpr int RAID_DEVICES = 4;
constexpr int DISK_SECTORS = 8192;
static FILE *g_Fp[RAID_DEVICES];

//-------------------------------------------------------------------------------------------------
/** Sample sector reading function. The function will be called by your Raid driver implementation.
 * Notice, the function is not called directly. Instead, the function will be invoked indirectly
 * through function pointer in the TBlkDev structure.
 */
int diskRead(int device,
			 int sectorNr,
			 void *data,
			 int sectorCnt) {
	if (device < 0 || device >= RAID_DEVICES)
		return 0;
	if (g_Fp[device] == nullptr)
		return 0;
	if (sectorCnt <= 0 || sectorNr + sectorCnt > DISK_SECTORS)
		return 0;
	fseek(g_Fp[device], sectorNr * SECTOR_SIZE, SEEK_SET);
	return fread(data, SECTOR_SIZE, sectorCnt, g_Fp[device]);
}
//-------------------------------------------------------------------------------------------------
/** Sample sector writing function. Similar to diskRead
 */
int diskWrite(int device,
			  int sectorNr,
			  const void *data,
			  int sectorCnt) {
	if (device < 0 || device >= RAID_DEVICES)
		return 0;
	if (g_Fp[device] == NULL)
		return 0;
	if (sectorCnt <= 0 || sectorNr + sectorCnt > DISK_SECTORS)
		return 0;
	fseek(g_Fp[device], sectorNr * SECTOR_SIZE, SEEK_SET);
	return fwrite(data, SECTOR_SIZE, sectorCnt, g_Fp[device]);
}
//-------------------------------------------------------------------------------------------------
/** A function which releases resources allocated by openDisks/createDisks
 */
void doneDisks() {
	for (int i = 0; i < RAID_DEVICES; i++)
		if (g_Fp[i]) {
			fclose(g_Fp[i]);
			g_Fp[i] = nullptr;
		}
}
//-------------------------------------------------------------------------------------------------
/** A function which creates the files needed for the sector reading/writing functions above.
 * This function is only needed for the particular implementation above.
 */
TBlkDev createDisks() {
	char buffer[SECTOR_SIZE];
	TBlkDev res;
	char fn[100];

	memset(buffer, 0, sizeof(buffer));
	for (int i = 0; i < RAID_DEVICES; i++) {
		snprintf(fn, sizeof(fn), "/tmp/%04d", i);
		g_Fp[i] = fopen(fn, "w+b");
		if (!g_Fp[i]) {
			doneDisks();
			throw std::runtime_error("Raw storage create error");
		}

		for (int j = 0; j < DISK_SECTORS; j++)
			if (fwrite(buffer, sizeof(buffer), 1, g_Fp[i]) != 1) {
				doneDisks();
				throw std::runtime_error("Raw storage create error");
			}
	}

	res.m_Devices = RAID_DEVICES;
	res.m_Sectors = DISK_SECTORS;
	res.m_Read = diskRead;
	res.m_Write = diskWrite;
	return res;
}
//-------------------------------------------------------------------------------------------------
/** A function which opens the files needed for the sector reading/writing functions above.
 * This function is only needed for the particular implementation above.
 */
TBlkDev openDisks() {
	TBlkDev res;
	char fn[100];

	for (int i = 0; i < RAID_DEVICES; i++) {
		snprintf(fn, sizeof(fn), "/tmp/%04d", i);
		g_Fp[i] = fopen(fn, "r+b");
		if (!g_Fp[i]) {
			doneDisks();
			throw std::runtime_error("Raw storage access error");
		}
		fseek(g_Fp[i], 0, SEEK_END);
		if (ftell(g_Fp[i]) != DISK_SECTORS * SECTOR_SIZE) {
			doneDisks();
			throw std::runtime_error("Raw storage read error");
		}
	}
	res.m_Devices = RAID_DEVICES;
	res.m_Sectors = DISK_SECTORS;
	res.m_Read = diskRead;
	res.m_Write = diskWrite;
	return res;
}
//-------------------------------------------------------------------------------------------------
void test1() {
	/* create the disks before we use them
	 */
	TBlkDev dev = createDisks();
	/* The disks are ready at this moment. Your RAID-related functions may be executed,
	 * the disk backend is ready.
	 *
	 * First, try to create the RAID:
	 */

	assert(CRaidVolume::create(dev));

	/* start RAID volume */

	CRaidVolume vol;

	assert(vol.start(dev) == RAID_OK);
	assert(vol.status() == RAID_OK);

	/* your raid device shall be up.
	 * try to read and write all RAID sectors:
	 */

	for (int i = 0; i < vol.size(); i++) {
		char buffer[SECTOR_SIZE];

		assert(vol.read(i, buffer, 1));
		assert(vol.write(i, buffer, 1));
	}

	/* Extensive testing of your RAID implementation ...
	 */

	/* Stop the raid device ...
	 */
	assert(vol.stop() == RAID_STOPPED);
	assert(vol.status() == RAID_STOPPED);

	/* ... and the underlying disks.
	 */

	doneDisks();
}
//-------------------------------------------------------------------------------------------------
void test2() {
	/* The RAID as well as disks are stopped. It corresponds i.e. to the
	 * restart of a real computer.
	 *
	 * after the restart, we will not create the disks, nor create RAID (we do not
	 * want to destroy the content). Instead, we will only open/start the devices:
	 */

	TBlkDev dev = openDisks();
	CRaidVolume vol;

	assert(vol.start(dev) == RAID_OK);

	/* some I/O: RaidRead/RaidWrite
	 */

	vol.stop();
	doneDisks();
}
//-------------------------------------------------------------------------------------------------
int main() {
	test1();
	test2();
	return EXIT_SUCCESS;
}
#endif /* __PROGTEST__ */
