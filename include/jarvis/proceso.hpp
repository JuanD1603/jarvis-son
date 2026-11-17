#pragma once

#include <string>
#include <vector>

namespace jarvis {

struct ResultadoProceso {
    int codigo_salida = -1;  // código de exit(); -1 si terminó por señal o no arrancó
    int senal = 0;           // señal que terminó al hijo (0 si ninguna)
    bool timeout = false;
    bool no_arranco = false;  // execvp falló (binario inexistente, sin permisos)
    long ms = 0;
    long rss_pico_kb = 0;  // ru_maxrss de wait4 (kB en Linux/Android)

    bool ok() const { return !timeout && !no_arranco && senal == 0 && codigo_salida == 0; }
    std::string describir() const;
};

// Ejecuta argv[0] con argv fijo (fork + execvp + wait4). Nunca pasa por una shell.
// stdout_ruta / stderr_ruta: archivo destino ("" = /dev/null). Se trunca antes de escribir.
// timeout_s <= 0 desactiva el límite. Al vencer, se envía SIGKILL a todo el grupo del hijo.
ResultadoProceso ejecutar(const std::vector<std::string>& argv, int timeout_s, const std::string& stdout_ruta,
                          const std::string& stderr_ruta);

}  // namespace jarvis
