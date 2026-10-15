#pragma once

#include <cstdint>
#include <cstring>
#include <deque>
#include <initializer_list>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

namespace op2 {

// Fluxo do jogo salvo (o "StreamIO" que sub_48A120 passa a cada subsistema).
// O mesmo código grava e lê: cada módulo tem um xxxSerialize(SaveStream&)
// simétrico. O conteúdo é o estado do PORT (as estruturas não têm o layout de
// memória do original); a ordem das seções segue sub_48A120.
class SaveStream {
public:
    explicit SaveStream(bool loading) : m_loading(loading) {}
    bool loading() const { return m_loading; }
    bool ok() const { return m_ok; }
    void fail() { m_ok = false; }
    std::vector<uint8_t>& buffer() { return m_buf; }
    void setBuffer(std::vector<uint8_t> b) { m_buf = std::move(b); m_pos = 0; }

    void raw(void* p, size_t n) {
        if (m_loading) {
            if (m_pos + n > m_buf.size()) { m_ok = false; std::memset(p, 0, n); return; }
            std::memcpy(p, m_buf.data() + m_pos, n);
            m_pos += n;
        } else {
            const uint8_t* b = static_cast<const uint8_t*>(p);
            m_buf.insert(m_buf.end(), b, b + n);
        }
    }
    template <class T> void io(T& v) {
        static_assert(std::is_trivially_copyable<T>::value, "io: tipo trivial");
        raw(&v, sizeof(T));
    }
    template <class T> void ioVec(std::vector<T>& v) {
        static_assert(std::is_trivially_copyable<T>::value, "ioVec: tipo trivial");
        uint32_t n = static_cast<uint32_t>(v.size());
        io(n);
        if (m_loading) { if (!m_ok || n > 0x4000000) { m_ok = false; return; } v.resize(n); }
        if (n) raw(v.data(), n * sizeof(T));
    }
    void ioStr(std::string& s) {
        uint32_t n = static_cast<uint32_t>(s.size());
        io(n);
        if (m_loading) { if (!m_ok || n > 0x100000) { m_ok = false; return; } s.resize(n); }
        if (n) raw(&s[0], n);
    }
    template <class T> void ioSet(std::set<T>& s) {
        std::vector<T> v(s.begin(), s.end());
        ioVec(v);
        if (m_loading) s = std::set<T>(v.begin(), v.end());
    }
    template <class T> void ioDeque(std::deque<T>& d) {
        std::vector<T> v(d.begin(), d.end());
        ioVec(v);
        if (m_loading) d = std::deque<T>(v.begin(), v.end());
    }
    // Marca de seção: grava / confere.
    void magic(uint32_t m) {
        uint32_t v = m;
        io(v);
        if (m_loading && v != m) m_ok = false;
    }

    // Bytes de um objeto, menos os membros não triviais (vetores, strings): na
    // leitura esses membros ficam como estão no destino e são lidos à parte.
    struct Skip { size_t off, len; };
    template <class T> void ioExcept(T& obj, std::initializer_list<Skip> skips) {
        unsigned char tmp[sizeof(T)];
        unsigned char* self = reinterpret_cast<unsigned char*>(&obj);
        if (!m_loading) {
            std::memcpy(tmp, self, sizeof(T));
            for (const Skip& k : skips) std::memset(tmp + k.off, 0, k.len);
            raw(tmp, sizeof(T));
            return;
        }
        raw(tmp, sizeof(T));
        if (!m_ok) return;
        size_t at = 0;
        std::vector<Skip> ks(skips);
        for (size_t i = 0; i < ks.size(); ++i)
            for (size_t j = i + 1; j < ks.size(); ++j)
                if (ks[j].off < ks[i].off) std::swap(ks[i], ks[j]);
        for (const Skip& k : ks) {
            if (k.off > at) std::memcpy(self + at, tmp + at, k.off - at);
            at = k.off + k.len;
        }
        if (at < sizeof(T)) std::memcpy(self + at, tmp + at, sizeof(T) - at);
    }
    template <class T, class M> static Skip member(const T& obj, const M& m) {
        return Skip{static_cast<size_t>(reinterpret_cast<const unsigned char*>(&m) -
                                        reinterpret_cast<const unsigned char*>(&obj)),
                    sizeof(M)};
    }

private:
    bool m_loading;
    bool m_ok = true;
    std::vector<uint8_t> m_buf;
    size_t m_pos = 0;
};

} // namespace op2
