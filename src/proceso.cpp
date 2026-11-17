#include "jarvis/proceso.hpp"

#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <thread>

namespace jarvis {

namespace {

// Código reservado para indicar al padre que execvp falló en el hijo.
constexpr int kCodigoExecFallido = 127;

void redirigir(int fd_destino, const std::string& ruta) {
    const char* r = ruta.empty() ? "/dev/null" : ruta.c_str();
    const int fd = open(r, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd >= 0) {
        dup2(fd, fd_destino);
        close(fd);
    }
}

}  // namespace

std::string ResultadoProceso::describir() const {
    if (no_arranco) return "no se pudo ejecutar el binario (ruta o permisos)";
    if (timeout) return "tiempo agotado";
    if (senal != 0) {
        std::string s = "terminado por señal " + std::to_string(senal);
        if (senal == SIGKILL) s += " (posible cierre por memoria: lmkd/OOM)";
        return s;
    }
    return "código de salida " + std::to_string(codigo_salida);
}

ResultadoProceso ejecutar(const std::vector<std::string>& argv, int timeout_s, const std::string& stdout_ruta,
                          const std::string& stderr_ruta) {
    ResultadoProceso r;
    if (argv.empty()) {
        r.no_arranco = true;
        return r;
    }
    std::vector<char*> cargv;
    cargv.reserve(argv.size() + 1);
    for (const auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
    cargv.push_back(nullptr);

    const auto inicio = std::chrono::steady_clock::now();
    const pid_t pid = fork();
    if (pid < 0) {
        r.no_arranco = true;
        return r;
    }
    if (pid == 0) {
        // Grupo propio: permite matar también a los nietos (p. ej. ffmpeg lanzado por un script).
        setpgid(0, 0);
        const int nulo = open("/dev/null", O_RDONLY);
        if (nulo >= 0) {
            dup2(nulo, STDIN_FILENO);
            close(nulo);
        }
        redirigir(STDOUT_FILENO, stdout_ruta);
        redirigir(STDERR_FILENO, stderr_ruta);
        execvp(cargv[0], cargv.data());
        _exit(kCodigoExecFallido);
    }
    setpgid(pid, pid);

    int estado = 0;
    struct rusage uso = {};
    for (;;) {
        const pid_t w = wait4(pid, &estado, WNOHANG, &uso);
        if (w == pid) break;
        if (w < 0 && errno != EINTR) break;
        const auto transcurrido = std::chrono::steady_clock::now() - inicio;
        if (timeout_s > 0 && transcurrido > std::chrono::seconds(timeout_s)) {
            r.timeout = true;
            kill(-pid, SIGKILL);
            kill(pid, SIGKILL);
            while (wait4(pid, &estado, 0, &uso) < 0 && errno == EINTR) {
            }
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    r.ms = static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - inicio)
            .count());
    r.rss_pico_kb = uso.ru_maxrss;
    if (WIFEXITED(estado)) {
        r.codigo_salida = WEXITSTATUS(estado);
        if (r.codigo_salida == kCodigoExecFallido && access(cargv[0], X_OK) != 0 &&
            std::strchr(cargv[0], '/') != nullptr) {
            r.no_arranco = true;
        }
    } else if (WIFSIGNALED(estado)) {
        r.senal = WTERMSIG(estado);
    }
    return r;
}

}  // namespace jarvis
