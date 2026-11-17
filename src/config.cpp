#include "jarvis/config.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace jarvis {

namespace {

std::string recortar(const std::string& s) {
    const auto ini = s.find_first_not_of(" \t\r\n");
    if (ini == std::string::npos) return "";
    const auto fin = s.find_last_not_of(" \t\r\n");
    return s.substr(ini, fin - ini + 1);
}

}  // namespace

Config Config::desde_texto(const std::string& texto) {
    Config c;
    std::istringstream in(texto);
    std::string linea, seccion;
    int numero = 0;
    while (std::getline(in, linea)) {
        ++numero;
        linea = recortar(linea);
        if (linea.empty() || linea[0] == '#' || linea[0] == ';') continue;
        if (linea.front() == '[' && linea.back() == ']') {
            seccion = recortar(linea.substr(1, linea.size() - 2));
            continue;
        }
        const auto igual = linea.find('=');
        if (igual == std::string::npos) {
            throw std::runtime_error("config: línea " + std::to_string(numero) + " sin '='");
        }
        std::string clave = recortar(linea.substr(0, igual));
        if (!seccion.empty()) clave = seccion + "." + clave;
        c.valores_[clave] = recortar(linea.substr(igual + 1));
    }
    c.resolver_referencias();
    return c;
}

void Config::resolver_referencias() {
    // Sustituye {seccion.clave} por su valor. Las variables de turno ({in_wav}, {texto}, ...)
    // no existen aquí y se dejan intactas para expandir_argv. Límite de pasadas contra ciclos.
    for (int pasada = 0; pasada < 8; ++pasada) {
        bool cambio = false;
        for (auto& [clave, valor] : valores_) {
            // Las claves "cmd*" son plantillas de argv: se expanden token a token en expandir_argv,
            // después de dividir por espacios. Resolverlas aquí partiría valores con espacios.
            const auto punto = clave.rfind('.');
            if (clave.compare(punto == std::string::npos ? 0 : punto + 1, 3, "cmd") == 0) continue;
            std::string res;
            for (std::size_t i = 0; i < valor.size();) {
                const auto cierre = valor[i] == '{' ? valor.find('}', i) : std::string::npos;
                if (cierre != std::string::npos) {
                    const auto it = valores_.find(valor.substr(i + 1, cierre - i - 1));
                    if (it != valores_.end() && it->first != clave) {
                        res += it->second;
                        i = cierre + 1;
                        cambio = true;
                        continue;
                    }
                }
                res.push_back(valor[i++]);
            }
            valor = res;
        }
        if (!cambio) return;
    }
}

Config Config::desde_archivo(const std::string& ruta) {
    std::ifstream f(ruta);
    if (!f) throw std::runtime_error("no se pudo abrir la configuración: " + ruta);
    std::ostringstream ss;
    ss << f.rdbuf();
    return desde_texto(ss.str());
}

std::string Config::texto(const std::string& clave, const std::string& por_defecto) const {
    const auto it = valores_.find(clave);
    return it == valores_.end() ? por_defecto : it->second;
}

int Config::entero(const std::string& clave, int por_defecto) const {
    const auto it = valores_.find(clave);
    if (it == valores_.end()) return por_defecto;
    try {
        return std::stoi(it->second);
    } catch (const std::exception&) {
        throw std::runtime_error("config: '" + clave + "' no es un entero: " + it->second);
    }
}

bool Config::tiene(const std::string& clave) const { return valores_.count(clave) > 0; }

void Config::poner(const std::string& clave, const std::string& valor) { valores_[clave] = valor; }

}  // namespace jarvis
