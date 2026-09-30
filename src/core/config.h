#pragma once

#include <string>
#include <vector>

namespace op2 {

// Leitor/gravador do OUTPOST2.INI, equivalente à classe CConfig do jogo original
// (que era um wrapper sobre GetPrivateProfileInt/String + WritePrivateProfileString
// do Windows). Esta versão é cross-platform e reproduz a mesma semântica:
//   * nomes de seção e chave são case-INSENSITIVE (como no Windows);
//   * getInt lê os dígitos iniciais do valor e cai no default se a chave faltar;
//   * setInt/setString criam a seção/chave se não existirem, ou atualizam no lugar.
// As seções/chaves usadas pelo jogo estão em GameSettings (abaixo).
class Config {
public:
    Config() = default;
    explicit Config(std::string path);

    // Carrega o arquivo INI. Retorna false se não conseguir abrir (mantém vazio,
    // o que é aceitável: os getters devolvem os defaults).
    bool load();
    bool load(const std::string& path);

    // Grava o INI de volta no arquivo (formato compatível com o original).
    bool save() const;
    bool saveAs(const std::string& path) const;

    int getInt(const std::string& section, const std::string& key, int def = 0) const;
    std::string getString(const std::string& section, const std::string& key,
                          const std::string& def = "") const;

    void setInt(const std::string& section, const std::string& key, int value);
    void setString(const std::string& section, const std::string& key,
                   const std::string& value);

    // Equivale a CConfig::SetInt: aceita "Seção\\Chave" ou "Seção/Chave".
    void setIntPath(const std::string& sectionKeyPath, int value);

    bool hasKey(const std::string& section, const std::string& key) const;
    const std::string& path() const { return m_path; }

private:
    struct Entry {
        std::string key;
        std::string value;
    };
    struct Section {
        std::string name;
        std::vector<Entry> entries;
    };

    std::vector<Section> m_sections;
    std::string m_path;

    Section* findSection(const std::string& name);
    const Section* findSection(const std::string& name) const;
    Section& ensureSection(const std::string& name);
    static bool iEquals(const std::string& a, const std::string& b);
};

// Configurações do jogo (seção [Game] e afins), com os defaults do OUTPOST2.INI.
// Espelha exatamente as chaves lidas pelo executável original.
struct GameSettings {
    // [Game]
    int music = 1;
    int musicVol = 1;
    int sound = 1;
    int soundVol = 0;
    int gameSpeed = 5;
    int scrollRate = 4;
    int overlay = 1;
    int ambient = 1;
    int stationaryShadows = 0;
    int mobileShadows = 0;
    int introAVI = 1;
    int showMovies = 1;
    int spiffyDraw = 1;
    int demo = 1;
    std::string name = "";

    // [WindowInfo]
    int miniMapButtons = 0;

    // [DESKTOP]
    int desktopWidth = 640;
    int desktopHeight = 480;

    // Lê de um Config já carregado (usa os defaults acima quando a chave falta).
    void loadFrom(const Config& cfg);
    // Escreve de volta no Config (para depois chamar Config::save()).
    void saveTo(Config& cfg) const;
};

} // namespace op2
