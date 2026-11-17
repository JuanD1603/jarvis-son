#include "jarvis/texto.hpp"

#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace jarvis {

namespace {

std::string colapsar_espacios(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool espacio = false;
    for (char c : s) {
        const bool es_esp = c == ' ' || c == '\t' || c == '\n' || c == '\r';
        if (es_esp) {
            espacio = !out.empty();
            continue;
        }
        if (espacio) out.push_back(' ');
        espacio = false;
        out.push_back(c);
    }
    return out;
}

// Decodifica el punto de código en s[i]; devuelve su longitud en bytes (1 si es inválido).
std::size_t leer_utf8(const std::string& s, std::size_t i, char32_t& cp) {
    const auto b = static_cast<unsigned char>(s[i]);
    std::size_t n = 1;
    if (b < 0x80) {
        cp = b;
        return 1;
    } else if ((b >> 5) == 0x6) {
        cp = b & 0x1F;
        n = 2;
    } else if ((b >> 4) == 0xE) {
        cp = b & 0x0F;
        n = 3;
    } else if ((b >> 3) == 0x1E) {
        cp = b & 0x07;
        n = 4;
    } else {
        cp = 0xFFFD;
        return 1;
    }
    if (i + n > s.size()) {
        cp = 0xFFFD;
        return 1;
    }
    for (std::size_t k = 1; k < n; ++k) {
        const auto c = static_cast<unsigned char>(s[i + k]);
        if ((c >> 6) != 0x2) {
            cp = 0xFFFD;
            return 1;
        }
        cp = (cp << 6) | (c & 0x3F);
    }
    return n;
}

bool es_emoji_o_simbolo(char32_t cp) {
    return (cp >= 0x1F000 && cp <= 0x1FAFF) ||  // emojis, pictogramas
           (cp >= 0x2600 && cp <= 0x27BF) ||    // símbolos varios y dingbats
           (cp >= 0x2190 && cp <= 0x21FF) ||    // flechas
           (cp >= 0x2B00 && cp <= 0x2BFF) ||    // flechas y estrellas
           (cp >= 0xFE00 && cp <= 0xFE0F) ||    // selectores de variación
           cp == 0x200D || cp == 0xFFFD;        // unión de ancho cero, inválido
}

std::string quitar_emojis(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        char32_t cp = 0;
        const std::size_t n = leer_utf8(s, i, cp);
        if (!es_emoji_o_simbolo(cp)) out.append(s, i, n);
        i += n;
    }
    return out;
}

// Corta en max_bytes sin partir un carácter UTF-8; prefiere el último fin de frase.
std::string recortar_utf8(const std::string& s, std::size_t max_bytes) {
    if (s.size() <= max_bytes) {
        // Respuesta cortada por el límite de tokens: se descarta la frase incompleta final
        // si queda al menos una frase completa razonable.
        if (s.empty() || s.find_last_of(".!?") == s.size() - 1) return s;
        const auto fin = s.find_last_of(".!?");
        if (fin != std::string::npos && fin + 1 >= s.size() / 3) return s.substr(0, fin + 1);
        return s;
    }
    std::size_t corte = max_bytes;
    while (corte > 0 && (static_cast<unsigned char>(s[corte]) & 0xC0) == 0x80) --corte;
    std::string base = s.substr(0, corte);
    const auto fin_frase = base.find_last_of(".!?");
    if (fin_frase != std::string::npos && fin_frase >= max_bytes / 3) {
        return base.substr(0, fin_frase + 1);
    }
    const auto esp = base.find_last_of(' ');
    if (esp != std::string::npos && esp > 0) base = base.substr(0, esp);
    return base + ".";
}

}  // namespace

std::string limpiar_transcripcion(const std::string& texto) {
    static const std::regex marcas(R"(\[[^\]]*\]|\([^)]*\)|\*[^*]*\*)");
    std::string s = std::regex_replace(texto, marcas, " ");
    s = colapsar_espacios(s);
    // Si solo quedan signos de puntuación, se considera vacío.
    if (s.find_first_not_of(" .,;:!?¡¿-\"'") == std::string::npos) return "";
    return s;
}

std::string limpiar_para_tts(const std::string& texto, std::size_t max_bytes) {
    static const std::regex think(R"(<think>[\s\S]*?</think>)");
    static const std::regex think_abierto(R"(<think>[\s\S]*$)");
    static const std::regex tokens_fin(R"(\[end of text\]|<\|im_end\|>|<\|endoftext\|>|</?s>)");
    static const std::regex url(R"((https?://|www\.)\S+)");
    static const std::regex enlace_md(R"(\[([^\]]*)\]\([^)]*\))");
    static const std::regex markdown(R"([*_`#>|~]+)");
    static const std::regex vineta(R"((^|\n)\s*([-+]|\d+\.)\s+)");

    std::string s = std::regex_replace(texto, think, " ");
    s = std::regex_replace(s, think_abierto, " ");
    s = std::regex_replace(s, tokens_fin, " ");
    s = std::regex_replace(s, enlace_md, "$1");
    s = std::regex_replace(s, url, " ");
    s = std::regex_replace(s, vineta, "$1");
    s = std::regex_replace(s, markdown, " ");
    s = quitar_emojis(s);
    s = colapsar_espacios(s);
    static const std::regex espacio_puntuacion(R"( +([.,;:!?]))");
    s = std::regex_replace(s, espacio_puntuacion, "$1");
    // Un argumento que empieza por '-' podría interpretarse como opción del binario de TTS.
    const auto inicio = s.find_first_not_of("-+ ");
    s = inicio == std::string::npos ? "" : s.substr(inicio);
    return recortar_utf8(s, max_bytes);
}

std::vector<std::string> expandir_argv(const std::string& plantilla,
                                       const std::map<std::string, std::string>& variables) {
    std::vector<std::string> out;
    std::istringstream in(plantilla);
    std::string token;
    while (in >> token) {
        std::string res;
        for (std::size_t i = 0; i < token.size();) {
            if (token[i] == '{') {
                const auto cierre = token.find('}', i);
                if (cierre == std::string::npos) {
                    throw std::runtime_error("plantilla: llave sin cerrar en '" + token + "'");
                }
                const std::string nombre = token.substr(i + 1, cierre - i - 1);
                const auto it = variables.find(nombre);
                if (it == variables.end()) {
                    throw std::runtime_error("plantilla: variable desconocida {" + nombre + "}");
                }
                res += it->second;
                i = cierre + 1;
            } else {
                res.push_back(token[i++]);
            }
        }
        out.push_back(res);
    }
    return out;
}

std::string leer_archivo(const std::string& ruta) {
    std::ifstream f(ruta, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool escribir_archivo(const std::string& ruta, const std::string& contenido) {
    std::ofstream f(ruta, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << contenido;
    return static_cast<bool>(f);
}

std::size_t contar_caracteres(const std::string& texto) {
    std::size_t n = 0;
    for (unsigned char c : texto) {
        if ((c & 0xC0) != 0x80) ++n;
    }
    return n;
}

}  // namespace jarvis
