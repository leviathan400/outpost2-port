#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>

namespace op2 {

// Carregador manual de DLL de missão (PE32). O original faz LoadLibraryA na
// T##/E##/P##.DLL (sub_402A10) e o Windows resolve os imports "Outpost2.exe"
// contra o próprio processo. Aqui o processo é o port, então a DLL é mapeada à
// mão: seções, relocações, imports (Outpost2.exe → funções-ponte do port com a
// mesma ABI; KERNEL32/USER32 → sistema) e DllMain. O código da DLL roda nativo
// (o port é x86, como o jogo).
class PeModule {
public:
    using ImportResolver = std::function<void*(const std::string& dll, const std::string& name)>;

    ~PeModule() { unload(); }
    bool load(const std::string& path, const ImportResolver& resolve, std::string& error);
    void unload();
    bool ok() const { return m_base != nullptr; }

    // GetProcAddress pelo nome do export (função ou dado). nullptr se não existe.
    void* proc(const std::string& name) const;
    uint8_t* base() const { return m_base; }
    uint32_t size() const { return m_size; }

private:
    uint8_t* m_base = nullptr;
    uint32_t m_size = 0;
    uint32_t m_entry = 0;
    std::map<std::string, uint32_t> m_exports;
    bool m_attached = false;
};

} // namespace op2
