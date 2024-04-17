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
			m_status &= (0b1 << dev) ^ 0xffff;
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

void XORSector(uint8_t *sec1, const uint8_t *sec2) {
	for (int i = 0; i < SECTOR_SIZE; ++i)
		sec1[i] ^= sec2[i];
}

class CRaidVolume {
protected:
	TBlkDev m_dev;
	bool m_hasDev;
	SOverhead m_overhead;
	int m_RAIDStatus;

	void markFailDisk(int disk) {
		if (m_overhead.m_status.getStatus(disk)) {
			m_overhead.m_status.setStatus(disk, false);
			m_RAIDStatus = m_RAIDStatus == RAID_OK ? RAID_DEGRADED : RAID_FAILED;
		}
	}

	/**
	 * @brief: Reads data from a sector of a device, no matter if it's overhead or parity
	 * @note: buffer has to be provided
	 */
	bool readSector(int dev, int row, uint8_t *buf, int length = 1) {
		if (!m_overhead.m_status.getStatus(dev))
			return false;
		bool toRet = m_dev.m_Read(dev, row, buf, length) == length;
		if (!toRet)
			markFailDisk(dev);
		return toRet;
	}
	/**
	 * @brief: Writes data from buffer to a sector of a device, no matter if it's overhead or parity
	 * @note: buffer has to be provided
	 */
	bool writeSector(int dev, int row, const uint8_t *buf, int length = 1) {
		if (!m_overhead.m_status.getStatus(dev))
			return false;
		bool toRet = m_dev.m_Write(dev, row, buf, length) == length;
		if (!toRet)
			markFailDisk(dev);
		return toRet;
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

	bool calculateParity(uint8_t *buf, int row, int skipDevDest = -1, int skipDevFail = -1, const uint8_t *failData = nullptr) {
		if (skipDevDest == -1)
			skipDevDest = getParityDevByRow(row);
		if (m_RAIDStatus == RAID_FAILED)
			return false;
		uint8_t tmpBuf[SECTOR_SIZE];
		bool first = failData == nullptr;
		if (!first)
			memcpy(buf, failData, SECTOR_SIZE);
		for (int disk = 0; disk < m_dev.m_Devices; ++disk) {
			if (disk == skipDevDest || disk == skipDevFail)
				continue;
			if (first) {
				first = false;
				if (!readSector(disk, row, buf))
					return false;
			} else {
				if (!readSector(disk, row, tmpBuf))
					return false;
				XORSector(buf, tmpBuf);
			}
		}
		return true;
	}

	bool writeRAID_OK(int disk, int row, int parityDisk, const uint8_t *data) {
		uint8_t newParity[SECTOR_SIZE];
		if (!readSector(parityDisk, row, newParity))
			return false;
		uint8_t buf[SECTOR_SIZE];
		if (!readSector(disk, row, buf))
			return false;

		XORSector(newParity, buf);
		XORSector(newParity, data);

		if (!writeSector(disk, row, data))
			return false;

		return writeSector(parityDisk, row, newParity);
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
			SOverhead overhead(1, dev.m_Devices);
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
		// ? why do I want to do that?
		/*
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
		*/

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

	int resync() {
		if (m_RAIDStatus != RAID_DEGRADED)
			return m_RAIDStatus;

		int toRecover = -1;
		for (int disk = 0; disk < m_dev.m_Devices; ++disk)
			if (!m_overhead.m_status.getStatus(disk)) {
				toRecover = disk;
				break;
			}
		if (toRecover == -1)
			throw logic_error("RAID says degraded, but all disks are ok, which is not possible");

		m_overhead.m_status.setStatus(toRecover, true);
		for (int row = 0; row < m_dev.m_Sectors - 1; ++row) {
			uint8_t buf[SECTOR_SIZE];
			if (!calculateParity(buf, row, toRecover)) {
				// since we couldn't calculate the parity (other disk failed), then we can't resync anymore
				markFailDisk(toRecover);
				return RAID_FAILED;
			}
			if (!writeSector(toRecover, row, buf))
				return RAID_DEGRADED;
		}

		m_RAIDStatus = RAID_OK;
		return RAID_OK;
	}

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

	bool read(int secNr, void *data, int secCnt) {
		if (m_RAIDStatus != RAID_OK && m_RAIDStatus != RAID_DEGRADED)
			return false;
		for (int secD = 0; secD < secCnt; ++secD) {
			int disk = getDevice(secNr + secD);
			int row = getRow(secNr + secD);
			// int parityDisk = getParityDevByRow(row);
			uint8_t *currentData = (uint8_t *)data + (secD * SECTOR_SIZE);

			if (m_RAIDStatus == RAID_OK) {
				if (readSector(disk, row, currentData))
					continue;
			}
			if (m_RAIDStatus == RAID_DEGRADED) {
				if (m_overhead.m_status.getStatus(disk) && readSector(disk, row, currentData))
					continue;
				else if (calculateParity(currentData, row, disk)) // inverse of xor is xor, recover data that way
					continue;
			}
			// m_RAIDStatus == RAID_FAILED
			return false;
		}
		return true;
	}

	bool write(int secNr, const void *data, int secCnt) {
		if (m_RAIDStatus != RAID_OK && m_RAIDStatus != RAID_DEGRADED)
			return false;

		for (int secD = 0; secD < secCnt; ++secD) {
			int disk = getDevice(secNr + secD);
			int row = getRow(secNr + secD);
			int parityDisk = getParityDevByRow(row);
			const uint8_t *currentData = (const uint8_t *)data + (secD * SECTOR_SIZE);

			if (m_RAIDStatus == RAID_OK) {
				if (writeRAID_OK(disk, row, parityDisk, currentData))
					continue;
			}
			if (m_RAIDStatus == RAID_DEGRADED) {
				// multiple ways to do this
				if (!m_overhead.m_status.getStatus(disk)) {
					// disk is failed
					// only update parity
					uint8_t newParity[SECTOR_SIZE];
					if (calculateParity(newParity, row, parityDisk, disk, currentData) && writeSector(parityDisk, row, newParity))
						continue;

				} else if (!m_overhead.m_status.getStatus(parityDisk)) {
					// parity is failed
					if (writeSector(disk, row, currentData))
						continue;
				} else {
					// both ok
					if (writeRAID_OK(disk, row, parityDisk, currentData))
						continue;
				}
			}
			// m_RAIDSTATUS == RAID_FAILED
			return false;
		}
		return true;
	}
};

#ifndef __PROGTEST__
#include "tests.inc"
#endif /* __PROGTEST__ */
