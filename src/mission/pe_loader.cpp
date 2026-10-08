#include "pe_loader.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace op2 {

namespace {
template <typename T> const T* at(const std::vector<uint8_t>& f, size_t off) {
    return off + sizeof(T) <= f.size() ? reinterpret_cast<const T*>(f.data() + off) : nullptr;
}
} // namespace

bool PeModule::load(const std::string& path, const ImportResolver& resolve, std::string& error) {
    unload();
    std::ifstream in(path, std::ios::binary);
    if (!in) { error = "arquivo nao encontrado: " + path; return false; }
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    const IMAGE_DOS_HEADER* dos = at<IMAGE_DOS_HEADER>(file, 0);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) { error = "nao e PE"; return false; }
    const IMAGE_NT_HEADERS32* nt = at<IMAGE_NT_HEADERS32>(file, static_cast<size_t>(dos->e_lfanew));
    if (!nt || nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        error = "nao e PE32 x86"; return false;
    }
    const IMAGE_OPTIONAL_HEADER32& oh = nt->OptionalHeader;

    // Mapeia (tenta a base preferida; senão onde der e reloca).
    uint8_t* base = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(static_cast<uintptr_t>(oh.ImageBase)),
                                                       oh.SizeOfImage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!base) base = static_cast<uint8_t*>(VirtualAlloc(nullptr, oh.SizeOfImage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!base) { error = "VirtualAlloc falhou"; return false; }
    std::memcpy(base, file.data(), oh.SizeOfHeaders);
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(reinterpret_cast<const IMAGE_NT_HEADERS32*>(file.data() + dos->e_lfanew));
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const uint32_t n = sec[i].SizeOfRawData < sec[i].Misc.VirtualSize || sec[i].Misc.VirtualSize == 0
                         ? sec[i].SizeOfRawData : sec[i].Misc.VirtualSize;
        if (n && sec[i].PointerToRawData + n <= file.size())
            std::memcpy(base + sec[i].VirtualAddress, file.data() + sec[i].PointerToRawData, n);
    }
    m_base = base;
    m_size = oh.SizeOfImage;

    // Relocações (só HIGHLOW em DLLs Win32).
    const int32_t delta = static_cast<int32_t>(reinterpret_cast<uintptr_t>(base) - oh.ImageBase);
    const IMAGE_DATA_DIRECTORY& rd = oh.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (delta && rd.Size) {
        uint32_t off = 0;
        while (off < rd.Size) {
            auto* blk = reinterpret_cast<IMAGE_BASE_RELOCATION*>(base + rd.VirtualAddress + off);
            if (!blk->SizeOfBlock) break;
            const uint32_t count = (blk->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2;
            auto* ent = reinterpret_cast<uint16_t*>(blk + 1);
            for (uint32_t k = 0; k < count; ++k) {
                const int type = ent[k] >> 12;
                if (type == IMAGE_REL_BASED_HIGHLOW)
                    *reinterpret_cast<uint32_t*>(base + blk->VirtualAddress + (ent[k] & 0xFFF)) += delta;
            }
            off += blk->SizeOfBlock;
        }
    }

    // Imports.
    const IMAGE_DATA_DIRECTORY& id = oh.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (id.Size) {
        for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + id.VirtualAddress); d->Name; ++d) {
            const std::string dll = reinterpret_cast<const char*>(base + d->Name);
            auto* names = reinterpret_cast<uint32_t*>(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
            auto* iat = reinterpret_cast<uint32_t*>(base + d->FirstThunk);
            for (; *names; ++names, ++iat) {
                std::string name;
                if (*names & IMAGE_ORDINAL_FLAG32) name = "#" + std::to_string(*names & 0xFFFF);
                else name = reinterpret_cast<const char*>(base + *names + 2);
                void* p = resolve(dll, name);
                if (!p) {
                    error = "import nao resolvido: " + dll + "!" + name;
                    unload();
                    return false;
                }
                *iat = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
            }
        }
    }

    // Exports.
    const IMAGE_DATA_DIRECTORY& ed = oh.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (ed.Size) {
        auto* ex = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(base + ed.VirtualAddress);
        auto* fn = reinterpret_cast<uint32_t*>(base + ex->AddressOfFunctions);
        auto* nm = reinterpret_cast<uint32_t*>(base + ex->AddressOfNames);
        auto* ord = reinterpret_cast<uint16_t*>(base + ex->AddressOfNameOrdinals);
        for (uint32_t i = 0; i < ex->NumberOfNames; ++i)
            m_exports[reinterpret_cast<const char*>(base + nm[i])] = fn[ord[i]];
    }

    // DllMain(DLL_PROCESS_ATTACH): inicializa a CRT da DLL.
    m_entry = oh.AddressOfEntryPoint;
    if (m_entry) {
        using DllMainFn = BOOL(WINAPI*)(HINSTANCE, DWORD, LPVOID);
        auto entry = reinterpret_cast<DllMainFn>(base + m_entry);
        if (!entry(reinterpret_cast<HINSTANCE>(base), DLL_PROCESS_ATTACH, nullptr)) {
            error = "DllMain falhou";
            m_entry = 0;
            unload();
            return false;
        }
        m_attached = true;
    }
    return true;
}

void PeModule::unload() {
    if (!m_base) return;
    if (m_attached && m_entry) {
        using DllMainFn = BOOL(WINAPI*)(HINSTANCE, DWORD, LPVOID);
        reinterpret_cast<DllMainFn>(m_base + m_entry)(reinterpret_cast<HINSTANCE>(m_base), DLL_PROCESS_DETACH, nullptr);
    }
    VirtualFree(m_base, 0, MEM_RELEASE);
    m_base = nullptr;
    m_size = 0;
    m_entry = 0;
    m_attached = false;
    m_exports.clear();
}

void* PeModule::proc(const std::string& name) const {
    auto it = m_exports.find(name);
    return (it == m_exports.end() || !m_base) ? nullptr : m_base + it->second;
}

} // namespace op2
