#pragma once

#include <array>
#include <map>
#include <string>

#include "jarvis/config.hpp"
#include "jarvis/proceso.hpp"

namespace jarvis {

enum class Estado { Listo, Grabando, Transcribiendo, Generando, Hablando, Error };

const char* nombre_estado(Estado e);

// Etapas medidas por turno (orden fijo; define las columnas de turnos.csv).
enum Etapa { kCaptura = 0, kStt, kSlm, kTts, kReproduccion, kNumEtapas };

struct Turno {
    long id = 0;
    std::string modo;  // "voz", "wav" o "mock"
    std::array<long, kNumEtapas> ms{};
    std::array<long, kNumEtapas> rss_kb{};
    long ms_total = 0;
    std::size_t chars_entrada = 0;
    std::size_t chars_salida = 0;
    int codigo = 0;           // 0 ok, 1 error, 2 entrada vacía
    std::string etapa_error;  // vacío si no hubo error
};

class Orquestador {
   public:
    Orquestador(Config config, bool mock);

    // Tareas previas al primer turno (caché del prompt de sistema). Un fallo no es fatal.
    bool preparar();

    // Ejecuta un turno completo. Si wav_entrada no está vacío, se omite la grabación.
    Turno turno(const std::string& wav_entrada);

    Estado estado() const { return estado_; }

   private:
    void cambiar_estado(Estado e);
    bool etapa(Etapa et, const std::string& clave_cmd, int timeout_s,
               const std::map<std::string, std::string>& vars, const std::string& stdout_ruta, Turno& t);
    void reportar_error(Etapa et, const ResultadoProceso& r, Turno& t);
    void decir(const std::string& texto, Turno& t);
    void registrar(const Turno& t);
    void limpiar_temporales(bool borrar_entrada);
    std::string ruta_tmp(const std::string& nombre) const;
    std::string prefijo_sistema() const;

    Config config_;
    bool mock_;
    Estado estado_ = Estado::Listo;
    long siguiente_id_ = 1;
};

}  // namespace jarvis
