#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

#include "jarvis/config.hpp"
#include "jarvis/orquestador.hpp"

namespace {

void uso() {
    std::cout << "Jarvis-son V1 - asistente de voz local por turnos\n\n"
                 "Uso: jarvis [--config RUTA] [--wav ARCHIVO [--repetir N]] [--mock]\n"
                 "  --config RUTA   configuración INI (por defecto ~/jarvis/config.ini)\n"
                 "  --wav ARCHIVO   omite la grabación y procesa este WAV (16 kHz mono)\n"
                 "  --repetir N     con --wav: ejecuta N turnos consecutivos\n"
                 "  --mock          simula todas las etapas (CI, sin modelos)\n"
                 "Modo interactivo: Enter para hablar, q + Enter para salir.\n";
}

// Descarta lo tecleado mientras Jarvis-son procesaba o hablaba (no hay barge-in en V1).
void descartar_entrada_pendiente() {
    if (isatty(STDIN_FILENO)) tcflush(STDIN_FILENO, TCIFLUSH);
}

}  // namespace

int main(int argc, char** argv) {
    const char* home = std::getenv("HOME");
    std::string ruta_config = std::string(home ? home : ".") + "/jarvis/config.ini";
    std::string wav;
    int repetir = 1;
    bool mock = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--config" && i + 1 < argc) {
            ruta_config = argv[++i];
        } else if (a == "--wav" && i + 1 < argc) {
            wav = argv[++i];
        } else if (a == "--repetir" && i + 1 < argc) {
            repetir = std::atoi(argv[++i]);
        } else if (a == "--mock") {
            mock = true;
        } else if (a == "-h" || a == "--help") {
            uso();
            return 0;
        } else {
            std::cerr << "argumento desconocido: " << a << "\n";
            uso();
            return 64;
        }
    }

    jarvis::Config config;
    try {
        config = jarvis::Config::desde_archivo(ruta_config);
    } catch (const std::exception& e) {
        if (!mock) {
            std::cerr << e.what() << std::endl;
            return 66;
        }
        // Termux no tiene /tmp: se usa $TMPDIR si existe.
        const char* tmpdir = std::getenv("TMPDIR");
        const std::string base = std::string(tmpdir ? tmpdir : "/tmp") + "/jarvis-mock";
        config.poner("general.tmp", base);
        config.poner("general.logs", base);
    }

    jarvis::Orquestador orq(config, mock);
    orq.preparar();

    // Modo no interactivo: --wav (pruebas automáticas) o --mock sin terminal.
    if (!wav.empty() || (mock && !isatty(STDIN_FILENO))) {
        int fallos = 0;
        for (int i = 0; i < std::max(1, repetir); ++i) {
            const auto t = orq.turno(wav);
            if (t.codigo == 1) ++fallos;
        }
        return fallos == 0 ? 0 : 1;
    }

    std::cout << "Jarvis-son V1. Enter para hablar, q + Enter para salir." << std::endl;
    std::cout << "[" << jarvis::nombre_estado(orq.estado()) << "]" << std::endl;
    std::string linea;
    while (std::getline(std::cin, linea)) {
        if (linea == "q" || linea == "Q") break;
        orq.turno("");
        descartar_entrada_pendiente();
        std::cout << "Enter para hablar, q + Enter para salir." << std::endl;
    }
    return 0;
}
