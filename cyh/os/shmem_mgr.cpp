#include "shmem_mgr.hpp"
#include "common_internal.hpp"
#include "res_mon.hpp"

namespace cyh::os {
	static void write_size_header(void* head_of_block, nuint size) {
		if (!head_of_block) { return; }
		nuint* pSize = (nuint*)head_of_block;
		*pSize = size;
	}
	static nuint read_size_header(void* head_of_block) {
		if (!head_of_block) { return 0; }
		nuint* pSize = (nuint*)head_of_block;
		return *pSize;
	}

	void SharedMemoryManager::_ReleaseHolder(SharedMemoryHolder* pholder) {
		if (!pholder) { return; }

#ifdef __WINDOWS_PLATFORM__
		if (pholder->m_handle) {
			CloseHandle(pholder->m_handle);
		}
#else
		if ((int)pholder->m_handle > 0) {
			shm_unlink(pholder->fname.c_str());
		}
		pholder->fname.clear();
#endif
		pholder->m_size = 0;
		pholder->m_handle = 0;
		pholder->m_block = 0;
	}

	void SharedMemoryManager::_MoveHolder(SharedMemoryHolder* pdst, SharedMemoryHolder* psrc) {
		if (!pdst) { return; }
		_ReleaseHolder(pdst);
		if (!psrc) { return; }
		if (pdst == psrc) { return; }
		pdst->m_handle = psrc->m_handle;
		pdst->m_block = psrc->m_block;
		pdst->m_size = psrc->m_size;
		psrc->m_handle = 0;
		psrc->m_block = 0;
		psrc->m_size = 0;
#ifdef __WINDOWS_PLATFORM__
#else
		pdst->fname = std::move(psrc->fname);

#endif
	}
	SharedMemoryManager::SharedMemoryHolder SharedMemoryManager::_OpenSharedMemory(uint accessFlag, const std::string& name, nuint openSz) {
#ifdef __WINDOWS_PLATFORM__
		auto handle = OpenFileMapping(accessFlag, FALSE, name.c_str());
		if (handle) {
			return SharedMemoryHolder{ handle, MapViewOfFile(handle, accessFlag, 0, 0, openSz) };
		}
		return SharedMemoryHolder{ 0, 0 };
#else
		int fd = shm_open(name.c_str(), O_RDWR, 0666);
		if (fd > 0) {
			void* ptr = mmap(NULL, openSz, accessFlag, MAP_SHARED, fd, 0);
			if (ptr) {
				close(fd);
				return SharedMemoryHolder{ 0, ptr, openSz, name };
			}
			close(fd);
		}
		return SharedMemoryHolder{ 0, 0, 0, "" };
#endif
	}
#ifdef __WINDOWS_PLATFORM__
	SharedMemoryManager::SharedMemoryHolder::SharedMemoryHolder(void* handle, void* block) : m_handle(handle), m_block(block) {
		if (block) {
			this->m_size = read_size_header(block);
		}
	}
#else
	SharedMemoryManager::SharedMemoryHolder::SharedMemoryHolder(void* handle, void* block, const std::string& _fname) : m_handle(handle), m_block(block), fname(_fname) {
		if (block) {
			this->m_size = read_size_header(block);
		}
	}
#endif
	SharedMemoryManager::SharedMemoryHolder::SharedMemoryHolder(SharedMemoryHolder&& other) noexcept {
		SharedMemoryManager::_MoveHolder(this, &other);
	}
	SharedMemoryManager::SharedMemoryHolder& SharedMemoryManager::SharedMemoryHolder::operator=(SharedMemoryHolder&& other) noexcept {
		SharedMemoryManager::_MoveHolder(this, &other);
		return *this;
	}
	SharedMemoryManager::SharedMemoryHolder::~SharedMemoryHolder() {
		SharedMemoryManager::_ReleaseHolder(this);
	}

#ifdef __WINDOWS_PLATFORM__
	uint SharedMemoryManager::ACCESS_READONLY = FILE_MAP_READ;
	uint SharedMemoryManager::ACCESS_READWRITE = FILE_MAP_ALL_ACCESS;
#else
	uint SharedMemoryManager::ACCESS_READONLY = PROT_READ;
	uint SharedMemoryManager::ACCESS_READWRITE = PROT_READ | PROT_WRITE;
#endif

	SharedMemoryManager::SharedMemoryHolder SharedMemoryManager::InvalidHolder() {
#ifdef __WINDOWS_PLATFORM__
		return SharedMemoryHolder{ 0, 0 };
#else
		return SharedMemoryHolder{ 0, 0, "" };
#endif
	}

	SharedMemoryManager::SharedMemoryHolder SharedMemoryManager::CreateSharedMemory(const std::string& name, nuint byteSize) {
#ifdef __WINDOWS_PLATFORM__
		auto mstmt = ResourceMonitor::GetMemoryStatus();
		if (mstmt.Physical.avail > byteSize) {
			auto handle = CreateFileMappingA(
				INVALID_HANDLE_VALUE,
				NULL,
				PAGE_READWRITE,
				0,
				byteSize,
				name.c_str()
			);
			if (handle) {
				return SharedMemoryHolder{ handle, MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, 0) };
			}
		}
#else
		auto fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
		if (fd > 0) {
			ftruncate(fd, byteSize + sizeof(nuint));
			void* ptr = mmap(0, byteSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
			if (ptr) {
				close(fd);
				return SharedMemoryHolder{ (void*)fd, ptr, name };
			}
			close(fd);
		}
#endif
		return InvalidHolder();
	}

	SharedMemoryManager::SharedMemoryHolder SharedMemoryManager::OpenSharedMemory(uint accessFlag, const std::string& name) {
		auto tempHolder = _OpenSharedMemory(accessFlag, name, sizeof(nuint));
		if (tempHolder.m_size) {
			return _OpenSharedMemory(accessFlag, name, tempHolder.m_size + sizeof(nuint));
		}
		return InvalidHolder();
	}
};