#pragma once
#include "os_.hpp"
namespace cyh::os {
	class SharedMemoryManager {
	public:
		class SharedMemoryHolder;
	private:
		static void _ReleaseHolder(SharedMemoryHolder* pholder);
		static void _MoveHolder(SharedMemoryHolder* pdst, SharedMemoryHolder* psrc);
		static SharedMemoryHolder _OpenSharedMemory(uint accessFlag, const std::string& name, nuint openSize);
	public:

		class SharedMemoryHolder final {
			friend class SharedMemoryManager;
			void* m_handle{};
			void* m_block{};
			nuint m_size{};
#ifdef __WINDOWS_PLATFORM__
#else
			// for unix
			std::string fname{};
#endif
		public:
			template<class T>
			T* GetPtr() {
				if (this->m_block) {
					auto temp = (char*)m_block;
					return (T*)(temp + sizeof(nuint));
				}
				return nullptr;
			}
			nuint size() const { return this->m_size; }
#ifdef __WINDOWS_PLATFORM__
			SharedMemoryHolder(void* handle, void* block);
#else
			SharedMemoryHolder(void* handle, void* block, const std::string& _fname);
#endif
			SharedMemoryHolder(const SharedMemoryHolder&) = delete;
			SharedMemoryHolder& operator=(const SharedMemoryHolder&) = delete;
			SharedMemoryHolder(SharedMemoryHolder&& other) noexcept;
			SharedMemoryHolder& operator=(SharedMemoryHolder&& other) noexcept;
			~SharedMemoryHolder();
		};

		static uint ACCESS_READONLY;
		static uint ACCESS_READWRITE;

		static SharedMemoryHolder InvalidHolder();

		static SharedMemoryHolder CreateSharedMemory(const std::string& name, nuint byteSize);

		static SharedMemoryHolder OpenSharedMemory(uint accessFlag, const std::string& name);

	};
};
