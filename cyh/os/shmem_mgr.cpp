#include "shmem_mgr.hpp"
#include "os_internal.hpp"
#include "res_mon.hpp"

namespace cyh::os {
	struct _MemBlock {
		nuint m_size{};
		void* m_data{};
	};
	// Write the size of block to it's first address
	static void write_size_header(void* head_of_block, nuint size) {
		if (!head_of_block) { return; }
		nuint* pSize = (nuint*)head_of_block;
		*pSize = size;
	}
	// Read the size stored in the first address of block
	static nuint read_size_header(void* head_of_block) {
		if (!head_of_block) { return 0; }
		nuint* pSize = (nuint*)head_of_block;
		return *pSize;
	}
	// For windows, this only work if the input handle is the last handle link to the shared memory in global
	static void try_stop_sharing(void* handle, const char* name) {
#ifdef __WINDOWS_PLATFORM__
		CloseHandle(handle);
#else
		shm_unlink(name);
#endif
	}
	// Unmap the address of current process which is mapped to the shared memory
	static void unmap_address(void* head_of_block, nuint size) {
		if (head_of_block) {
#ifdef __WINDOWS_PLATFORM__
			UnmapViewOfFile(head_of_block);
#else
			munmap(head_of_block, size);
#endif
		}
	}
	// Indicate wether the input handle is valid
	static bool is_valid_shm_handle(void* handle) {
		return
#ifdef __WINDOWS_PLATFORM__
			handle != nullptr;
#else
			(int)handle > 0;
#endif
	}
	// Create a shared memory in host os and create a handle link to the shared memory
	static void* create_shared_memory(const char* name, nuint alloc_size) {
		return
#ifdef __WINDOWS_PLATFORM__
			CreateFileMappingA(
				INVALID_HANDLE_VALUE,
				NULL,
				PAGE_READWRITE,
				0,
				static_cast<DWORD>(alloc_size),
				name
			);
#else
			(void*)shm_open(name, O_CREAT | O_RDWR, 0666);
#endif
	}
	// Create a handle to an exists shared memory
	static void* get_exist_shm_handle(const char* name, uint accessFlag) {
		return
#ifdef __WINDOWS_PLATFORM__
			OpenFileMapping(accessFlag, FALSE, name);
#else
			shm_open(name, O_RDWR, 0666);
#endif
	}
	// For windows, this will stop sharing memory if this is the last handle bound to the shared memory
	static void close_shm_handle(void* handle) {
#ifdef __WINDOWS_PLATFORM__
		CloseHandle(handle);
#else
		close((int)handle);
#endif
	}
	// This function will mark the alloc_size at the starting address of shared memory if the input handle existing a bound shared memory
	static void* init_and_get_shm_address(void* handle, nuint alloc_size) {
		void* ptr;
#ifdef __WINDOWS_PLATFORM__
		ptr = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, alloc_size);
#else
		ftruncate((int)handle, alloc_size);
		ptr = mmap(0, alloc_size, PROT_READ | PROT_WRITE, MAP_SHARED, (int)handle, 0);
#endif
		if (ptr) {
			write_size_header(ptr, alloc_size);
		}
		return ptr;
	}
	// Map the shared memory to the address in current process and get the pointer
	static void* get_shm_address(void* handle, uint accessFlag, nuint size_to_mapping) {
		void* ptr;
#ifdef __WINDOWS_PLATFORM__
		ptr = MapViewOfFile(handle, accessFlag, 0, 0, size_to_mapping);
#else
		ptr = mmap(0, size_to_mapping, accessFlag, MAP_SHARED, (int)handle, 0);
#endif
		return ptr;
	}
	// Create an object automatically managed the lifetime of shared memory
	// The input handle and block will managed by SharedMemoryHolder after calling this function
	static SharedMemoryManager::SharedMemoryHolder create_shm_holder(void*& handle, void* block, const char* name, bool is_owner) {
		void* temp = handle;
		handle = 0;
		return SharedMemoryManager::SharedMemoryHolder(temp, block, name, is_owner);
	}
	// Create an useless holder
	static SharedMemoryManager::SharedMemoryHolder create_invalid_holder() {
		return SharedMemoryManager::SharedMemoryHolder{ 0, 0, "", false };
	}

	void SharedMemoryManager::_ReleaseHolder(SharedMemoryHolder* pholder) {
		if (!pholder) { return; }
		if (pholder->m_handle) {
			try_stop_sharing(pholder->m_handle, pholder->fname.c_str());
		}
		if (pholder->m_block) {
			auto alloc_size = read_size_header(pholder->m_block);
			unmap_address(pholder->m_block, alloc_size);
		}
		pholder->fname.clear();
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
	void* SharedMemoryManager::SharedMemoryHolder::data() const {
		if (this->m_block) {
			return ((_MemBlock*)this->m_block)->m_data;
		}
		return nullptr;
	}
	nuint SharedMemoryManager::SharedMemoryHolder::capacity() const {
		return this->m_size;
	}
	SharedMemoryManager::SharedMemoryHolder::SharedMemoryHolder(void* handle, void* block, const std::string& name, bool is_owner) : m_block(block), fname(name) {
#ifdef __WINDOWS_PLATFORM__
		this->m_handle = handle;
#else
		close((int)handle);
		if (is_owner) { this->m_handle = handle; }
#endif
		if (block) {
			auto alloc_size = read_size_header(block);
			if (alloc_size > sizeof(nuint)) {
				this->m_size = alloc_size - sizeof(nuint);
			}
		}
	}
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

	SharedMemoryManager::SharedMemoryHolder SharedMemoryManager::CreateSharedMemory(const std::string& name, nuint byteSize) {
		nuint alloc_size = byteSize + sizeof(nuint);
		auto mstmt = ResourceMonitor::GetMemoryStatus();
		if (mstmt.Physical.avail > alloc_size) {
			auto handle = create_shared_memory(name.c_str(), alloc_size);
			if (is_valid_shm_handle(handle)) {
				auto addr = init_and_get_shm_address(handle, alloc_size);
				if (addr) {
					return create_shm_holder(handle, addr, name.c_str(), true);
				}
				close_shm_handle(handle);
			}
		}
		return create_invalid_holder();
	}
	SharedMemoryManager::SharedMemoryHolder SharedMemoryManager::OpenSharedMemory(uint accessFlag, const std::string& name) {
		auto handle = get_exist_shm_handle(name.c_str(), accessFlag);
		if (is_valid_shm_handle(handle)) {
			auto ptr = get_shm_address(handle, SharedMemoryManager::ACCESS_READONLY, sizeof(nuint));
			if (ptr) {
				auto alloc_size = *((nuint*)ptr);
				unmap_address(ptr, sizeof(nuint));

				if (alloc_size) {	
					ptr = get_shm_address(handle, accessFlag, alloc_size);
					if (ptr) {
						return create_shm_holder(handle, ptr, name.c_str(), false);
					}
				}	
			}
			close_shm_handle(handle);
		}
		return create_invalid_holder();
	}
};
