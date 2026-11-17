#pragma once

#include <map>
#include <string>

namespace jarvis {

// Configuración tipo INI. Las claves dentro de "[seccion]" se exponen como "seccion.clave".
class Config {
   public:
    static Config desde_archivo(const std::string& ruta);
    static Config desde_texto(const std::string& texto);

    std::string texto(const std::string& clave, const std::string& por_defecto = "") const;
    int entero(const std::string& clave, int por_defecto) const;
    bool tiene(const std::string& clave) const;
    void poner(const std::string& clave, const std::string& valor);
    const std::map<std::string, std::string>& valores() const { return valores_; }

   private:
    void resolver_referencias();
    std::map<std::string, std::string> valores_;
};

}  // namespace jarvis
