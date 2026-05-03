#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <Windows.h>
#include "C:\Users\filip\source\repos\TestTemplate\UM\alpc_client.h"

namespace driver {

    struct driver_get_t;
    struct exported_functions_t;
    struct s_dgx_t;

    namespace detail {
        AlpcClient& get_alpc_client();
        driver_get_t& get_get_instance();
        exported_functions_t& get_exported_instance();
        s_dgx_t& get_s_dgx_instance();
    }

    struct vulnerable_t {
        uintptr_t proc_id = 0;
        uintptr_t base_address = 0;

        vulnerable_t* operator()() { return this; }

        driver_get_t& get() { return detail::get_get_instance(); }
        exported_functions_t& exported_functions() { return detail::get_exported_instance(); }
        s_dgx_t& s_dgx() { return detail::get_s_dgx_instance(); }
    };

    inline vulnerable_t vulnerable;

    struct driver_get_t {
        template<typename T>
        T read_physical_memory(uintptr_t address) {
            T value = {};
            if (!vulnerable.proc_id) return value;
            if (address == 0 || address == UINTPTR_MAX) return value;
            if (address < 0x10000ULL || address > 0x7FFFFFFFFFFFULL) return value;

            detail::get_alpc_client().SetTargetProcessId(vulnerable.proc_id);
            detail::get_alpc_client().ReadMemory(address, &value, sizeof(T));
            return value;
        }

        template<typename T>
        bool write_physical_memory(uintptr_t address, const T& value) {
            if (!vulnerable.proc_id) return false;
            if (address == 0 || address == UINTPTR_MAX) return false;
            if (address < 0x10000ULL || address > 0x7FFFFFFFFFFFULL) return false;

            detail::get_alpc_client().SetTargetProcessId(vulnerable.proc_id);
            return detail::get_alpc_client().WriteMemory(address, &value, sizeof(T));
        }

        template<typename T>
        T read_chain(uintptr_t base, const std::vector<uintptr_t>& offsets) {
            uintptr_t cur = base;
            for (size_t i = 0; i < offsets.size(); i++) {
                if (i == offsets.size() - 1)
                    return read_physical_memory<T>(cur + offsets[i]);
                cur = read_physical_memory<uintptr_t>(cur + offsets[i]);
                if (!cur || cur == UINTPTR_MAX) return T{};
            }
            return T{};
        }

        // BATCH_READ_REQUEST structure (from NullKD/shared.h)
        struct BATCH_READ_ENTRY {
            ULONG64 virtualAddress;
            ULONG   size;
            ULONG   outOffset;
        };

        struct BATCH_READ_REQUEST {
            ULONG            count;
            BATCH_READ_ENTRY* entries;
            PUCHAR           outBuffer;
        };

        bool batch_read(BATCH_READ_REQUEST* batchReq, void* outBuf, size_t outBufSize) {
            if (!batchReq || !outBuf || outBufSize == 0) return false;
            if (!vulnerable.proc_id) return false;

            detail::get_alpc_client().SetTargetProcessId(vulnerable.proc_id);

            // Emulate batch read via high-speed ALPC single reads
            for (ULONG i = 0; i < batchReq->count; i++) {
                BATCH_READ_ENTRY& entry = batchReq->entries[i];
                if (!entry.virtualAddress || entry.size == 0) continue;
                if (entry.outOffset + entry.size > outBufSize) continue;

                detail::get_alpc_client().ReadMemory(entry.virtualAddress, (PUCHAR)outBuf + entry.outOffset, entry.size);
            }
            return true;
        }

        uintptr_t refresh(bool);
    };

    struct exported_functions_t {
        uintptr_t get_module_dll(const wchar_t* moduleName);
        uintptr_t get_process_id(const char* processName);
        uintptr_t retrieve_image_base(uintptr_t addr);
    };

    struct s_dgx_t {
        bool get_export();
    };

    std::string ReadChar(uintptr_t address);
    std::string read_wstr(uintptr_t address);
}
