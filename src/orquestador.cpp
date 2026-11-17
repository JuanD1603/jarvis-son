#include "jarvis/orquestador.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

#include "jarvis/texto.hpp"

namespace jarvis {

namespace {

const char* const kNombreEtapa[kNumEtapas] = {"captura", "stt", "slm", "tts", "reproduccion"};

const char* const kDiagnostico[kNumEtapas] = {
    "Verifica que Termux:API esté instalada y con permiso de micrófono; revisa logs/captura.err",
    "Verifica la ruta de whisper-cli y del modelo en config.ini; revisa logs/stt.err",
    "Verifica el modelo GGUF y la memoria libre (free -m); revisa logs/slm.err",
    "Verifica sherpa-onnx, LD_LIBRARY_PATH y los archivos de la voz; revisa logs/tts.err",
    "Verifica play-audio y el volumen multimedia; revisa logs/reproduccion.err",
};

const char* const kMensajeVacio = "No te escuché, intenta de nuevo.";

std::string expandir_escapes(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'n') {
            out.push_back('\n');
            ++i;
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

std::string marca_tiempo() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S", &tm);
    return buf;
}

bool existe(const std::string& ruta) {
    struct stat st{};
    return stat(ruta.c_str(), &st) == 0;
}

long tamano(const std::string& ruta) {
    struct stat st{};
    return stat(ruta.c_str(), &st) == 0 ? static_cast<long>(st.st_size) : -1;
}

}  // namespace

const char* nombre_estado(Estado e) {
    switch (e) {
        case Estado::Listo: return "LISTO";
        case Estado::Grabando: return "GRABANDO";
        case Estado::Transcribiendo: return "TRANSCRIBIENDO";
        case Estado::Generando: return "GENERANDO";
        case Estado::Hablando: return "HABLANDO";
        case Estado::Error: return "ERROR";
    }
    return "?";
}

Orquestador::Orquestador(Config config, bool mock) : config_(std::move(config)), mock_(mock) {
    const std::string tmp = config_.texto("general.tmp", "/tmp/jarvis");
    mkdir(tmp.c_str(), 0700);
    mkdir(config_.texto("general.logs", tmp).c_str(), 0700);
    // Los id continúan la numeración del CSV existente (una fila por turno, más la cabecera).
    std::ifstream csv(config_.texto("general.csv", ruta_tmp("turnos.csv")));
    std::string linea;
    long filas = 0;
    while (std::getline(csv, linea)) ++filas;
    siguiente_id_ = filas > 0 ? filas : 1;
}

std::string Orquestador::prefijo_sistema() const {
    return "<|im_start|>system\n" + config_.texto("slm.sistema") + "<|im_end|>\n<|im_start|>user\n";
}

bool Orquestador::preparar() {
    // Caché de prompt (solo lectura en cada turno): contiene ÚNICAMENTE el prompt de sistema, nunca
    // texto del usuario. Se regenera si el prompt de sistema cambió desde la última vez.
    const std::string cache = config_.texto("slm.cache");
    if (mock_ || cache.empty() || !config_.tiene("slm.cmd_preparar_cache")) return true;
    const std::string copia = cache + ".prompt.txt";
    const std::string prefijo = prefijo_sistema();
    if (existe(cache) && leer_archivo(copia) == prefijo) return true;

    std::cout << "Preparando caché del prompt de sistema (solo la primera vez)..." << std::endl;
    if (!escribir_archivo(copia, prefijo)) return false;
    std::map<std::string, std::string> vars = config_.valores();
    vars["prompt_sistema"] = copia;
    std::vector<std::string> argv;
    try {
        argv = expandir_argv(config_.texto("slm.cmd_preparar_cache"), vars);
    } catch (const std::exception& e) {
        std::cerr << "config inválida en slm.cmd_preparar_cache: " << e.what() << std::endl;
        return false;
    }
    const auto r = ejecutar(argv, config_.entero("slm.timeout", 120), "",
                            config_.texto("general.logs", "/tmp") + "/slm_cache.err");
    if (!r.ok() || !existe(cache)) {
        std::cerr << "No se pudo crear la caché del SLM (" << r.describir()
                  << "); se continúa sin ella. Revisa logs/slm_cache.err" << std::endl;
        std::remove(copia.c_str());
        return false;
    }
    return true;
}

std::string Orquestador::ruta_tmp(const std::string& nombre) const {
    return config_.texto("general.tmp", "/tmp/jarvis") + "/" + nombre;
}

void Orquestador::cambiar_estado(Estado e) {
    estado_ = e;
    std::cout << "[" << nombre_estado(e) << "]" << std::endl;
}

void Orquestador::reportar_error(Etapa et, const ResultadoProceso& r, Turno& t) {
    cambiar_estado(Estado::Error);
    std::cout << "  etapa: " << kNombreEtapa[et] << "\n"
              << "  causa: " << r.describir() << "\n"
              << "  siguiente paso: " << kDiagnostico[et] << std::endl;
    t.codigo = 1;
    t.etapa_error = kNombreEtapa[et];
}

// Simula la salida de cada etapa para CI y pruebas sin modelos.
static ResultadoProceso simular(Etapa et, const std::map<std::string, std::string>& vars,
                                const std::string& stdout_ruta) {
    ResultadoProceso r;
    r.codigo_salida = 0;
    switch (et) {
        case kCaptura: escribir_archivo(vars.at("in_wav"), "RIFF-mock"); break;
        case kStt: escribir_archivo(vars.at("stt_base") + ".txt", " Hola Jarvis, ¿qué hora es? "); break;
        case kSlm:
            escribir_archivo(stdout_ruta,
                             "<think>razonamiento</think>Son las **diez** de la mañana 😀. "
                             "Más en https://ejemplo.com [end of text]");
            break;
        case kTts: escribir_archivo(vars.at("out_wav"), "RIFF-mock"); break;
        default: break;
    }
    return r;
}

bool Orquestador::etapa(Etapa et, const std::string& clave_cmd, int timeout_s,
                        const std::map<std::string, std::string>& vars, const std::string& stdout_ruta,
                        Turno& t) {
    ResultadoProceso r;
    if (mock_) {
        r = simular(et, vars, stdout_ruta);
    } else {
        std::vector<std::string> argv;
        try {
            argv = expandir_argv(config_.texto(clave_cmd), vars);
        } catch (const std::exception& e) {
            std::cerr << "config inválida en " << clave_cmd << ": " << e.what() << std::endl;
            r.no_arranco = true;
            reportar_error(et, r, t);
            return false;
        }
        const std::string err = config_.texto("general.logs", "/tmp") + "/" + kNombreEtapa[et] + ".err";
        r = ejecutar(argv, timeout_s, stdout_ruta, err);
    }
    t.ms[et] += r.ms;
    t.rss_kb[et] = std::max(t.rss_kb[et], r.rss_pico_kb);
    if (!r.ok()) {
        reportar_error(et, r, t);
        return false;
    }
    return true;
}

void Orquestador::decir(const std::string& texto, Turno& t) {
    std::map<std::string, std::string> vars = config_.valores();
    vars["texto"] = texto;
    vars["out_wav"] = ruta_tmp("out.wav");
    cambiar_estado(Estado::Hablando);
    if (!etapa(kTts, "tts.cmd", config_.entero("tts.timeout", 60), vars, "", t)) return;
    if (!mock_ && tamano(vars["out_wav"]) <= 44) {
        ResultadoProceso r;
        r.codigo_salida = 0;
        r.no_arranco = false;
        std::cout << "  (TTS no generó audio)" << std::endl;
        reportar_error(kTts, r, t);
        return;
    }
    etapa(kReproduccion, "reproduccion.cmd", config_.entero("reproduccion.timeout", 60), vars, "", t);
}

Turno Orquestador::turno(const std::string& wav_entrada) {
    Turno t;
    t.id = siguiente_id_++;
    t.modo = mock_ ? "mock" : (wav_entrada.empty() ? "voz" : "wav");
    const auto inicio = std::chrono::steady_clock::now();

    std::map<std::string, std::string> vars = config_.valores();
    vars["in_wav"] = wav_entrada.empty() ? ruta_tmp("in.wav") : wav_entrada;
    vars["stt_base"] = ruta_tmp("stt");
    vars["prompt"] = ruta_tmp("prompt.txt");
    vars["out_wav"] = ruta_tmp("out.wav");
    const std::string slm_salida = ruta_tmp("slm.txt");

    auto finalizar = [&]() {
        t.ms_total = static_cast<long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - inicio)
                .count());
        registrar(t);
        limpiar_temporales(wav_entrada.empty());
        cambiar_estado(Estado::Listo);
        return t;
    };

    // 1. Captura
    if (wav_entrada.empty()) {
        cambiar_estado(Estado::Grabando);
        // El aviso "HABLA AHORA" lo da capturar.sh cuando el micrófono ya graba (evita cortar la
        // primera palabra por la latencia de arranque de Termux:API).
        std::cout << "  Preparando micrófono... espera el aviso HABLA AHORA" << std::endl;
        if (!etapa(kCaptura, "captura.cmd", config_.entero("captura.timeout", 30), vars, "", t)) {
            return finalizar();
        }
    } else if (!mock_ && !existe(wav_entrada)) {
        std::cout << "  no existe el WAV de entrada: " << wav_entrada << std::endl;
        ResultadoProceso r;
        r.no_arranco = true;
        reportar_error(kCaptura, r, t);
        return finalizar();
    }

    // 2. STT
    cambiar_estado(Estado::Transcribiendo);
    if (!etapa(kStt, "stt.cmd", config_.entero("stt.timeout", 60), vars, "", t)) return finalizar();
    const std::string transcripcion = limpiar_transcripcion(leer_archivo(vars["stt_base"] + ".txt"));
    t.chars_entrada = contar_caracteres(transcripcion);
    if (transcripcion.empty()) {
        std::cout << "  " << kMensajeVacio << std::endl;
        t.codigo = 2;
        decir(kMensajeVacio, t);
        return finalizar();
    }
    std::cout << "  Tú: " << transcripcion << std::endl;

    // 3. SLM: el prompt va por archivo; el texto del usuario nunca forma parte de argv ni de una shell.
    cambiar_estado(Estado::Generando);
    const std::string prompt = prefijo_sistema() + transcripcion + "<|im_end|>\n<|im_start|>assistant\n" +
                               expandir_escapes(config_.texto("slm.prefijo_asistente"));
    if (!escribir_archivo(vars["prompt"], prompt)) {
        ResultadoProceso r;
        r.no_arranco = true;
        reportar_error(kSlm, r, t);
        return finalizar();
    }
    if (!etapa(kSlm, "slm.cmd", config_.entero("slm.timeout", 120), vars, slm_salida, t)) {
        return finalizar();
    }
    const auto max_bytes = static_cast<std::size_t>(config_.entero("general.max_bytes_tts", 400));
    std::string respuesta = limpiar_para_tts(leer_archivo(slm_salida), max_bytes);
    if (respuesta.empty()) respuesta = "No tengo una respuesta para eso.";
    t.chars_salida = contar_caracteres(respuesta);
    std::cout << "  Jarvis-son: " << respuesta << std::endl;

    // 4-5. TTS y reproducción (bloqueante: no se acepta voz nueva mientras habla).
    decir(respuesta, t);
    return finalizar();
}

void Orquestador::registrar(const Turno& t) {
    const std::string ruta = config_.texto("general.csv", ruta_tmp("turnos.csv"));
    const bool nuevo = !existe(ruta);
    std::ofstream f(ruta, std::ios::app);
    if (!f) {
        std::cerr << "no se pudo escribir " << ruta << std::endl;
        return;
    }
    if (nuevo) {
        f << "id,timestamp,modo";
        for (const auto* n : kNombreEtapa) f << ",ms_" << n;
        f << ",ms_total,chars_entrada,chars_salida,codigo,etapa_error";
        for (const auto* n : kNombreEtapa) f << ",rss_kb_" << n;
        f << "\n";
    }
    f << t.id << "," << marca_tiempo() << "," << t.modo;
    for (long v : t.ms) f << "," << v;
    f << "," << t.ms_total << "," << t.chars_entrada << "," << t.chars_salida << "," << t.codigo << ","
      << t.etapa_error;
    for (long v : t.rss_kb) f << "," << v;
    f << "\n";
}

void Orquestador::limpiar_temporales(bool borrar_entrada) {
    // Privacidad: no se conservan audios, transcripciones ni respuestas entre turnos.
    for (const char* n : {"stt.txt", "prompt.txt", "slm.txt", "out.wav"}) std::remove(ruta_tmp(n).c_str());
    if (borrar_entrada) std::remove(ruta_tmp("in.wav").c_str());
}

}  // namespace jarvis
