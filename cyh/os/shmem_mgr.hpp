#pragma once
#include "os_.hpp"
namespace cyh::os {

	class SharedMemoryManager {
	public:
		class SharedMemoryHolder;
	private:
		static void _ReleaseHolder(SharedMemoryHolder* pholder);
		static void _MoveHolder(SharedMemoryHolder* pdst, SharedMemoryHolder* psrc);
	public:

		// An object automatically managed the lifetime of shared memory
		class SharedMemoryHolder final {
			friend class SharedMemoryManager;
			void* m_handle{};
			void* m_block{};
			nuint m_size{};
			std::string fname{};
		public:
			// The address mapped to the shared memory
			void* data() const;
			// The capacity of memory in bytes
			nuint capacity() const;

			template<class T>
			T* get() const { return (T*)(this->data()); }

			SharedMemoryHolder(void* handle, void* block, const std::string& name, bool is_owner);
			SharedMemoryHolder(const SharedMemoryHolder&) = delete;
			SharedMemoryHolder& operator=(const SharedMemoryHolder&) = delete;
			SharedMemoryHolder(SharedMemoryHolder&& other) noexcept;
			SharedMemoryHolder& operator=(SharedMemoryHolder&& other) noexcept;
			~SharedMemoryHolder();
		};
		static uint ACCESS_READONLY;
		static uint ACCESS_READWRITE;		
		static SharedMemoryHolder CreateSharedMemory(const std::string& name, nuint byteSize);
		static SharedMemoryHolder OpenSharedMemory(uint accessFlag, const std::string& name);
	};
};
