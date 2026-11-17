// Pruebas unitarias sin dependencias externas (se ejecutan en CI con ctest).
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "jarvis/config.hpp"
#include "jarvis/proceso.hpp"
#include "jarvis/texto.hpp"

namespace {

int fallos = 0;

void comprobar(bool cond, const std::string& nombre) {
    std::cout << (cond ? "[ok]   " : "[FALLA] ") << nombre << "\n";
    if (!cond) ++fallos;
}

}  // namespace

int main() {
    using namespace jarvis;

    comprobar(limpiar_para_tts("<think>pienso</think>Hola, soy **Jarvis-son**.") == "Hola, soy Jarvis-son.",
              "quita think y markdown");
    comprobar(limpiar_para_tts("<think>sin cerrar") == "", "think sin cerrar se elimina");
    comprobar(limpiar_para_tts("Mira https://ejemplo.com/x ahora 😀 [end of text]") == "Mira ahora",
              "quita url, emoji y token de fin");
    comprobar(limpiar_para_tts("- uno\n- dos") == "uno dos", "quita viñetas");
    comprobar(limpiar_para_tts("[Sitio](http://a.b) útil") == "Sitio útil", "enlace markdown");
    const std::string largo(500, 'a');
    comprobar(limpiar_para_tts(largo, 400).size() <= 401, "recorta a max_bytes");
    comprobar(limpiar_para_tts("Primera frase completa. Segunda frase que se corta aquí", 30) ==
                  "Primera frase completa.",
              "recorta en fin de frase");
    comprobar(limpiar_para_tts("Primera frase. Segunda frase. Tercera frase cortada a la mit") ==
                  "Primera frase. Segunda frase.",
              "descarta frase incompleta final");
    comprobar(limpiar_para_tts("Respuesta sin punto final") == "Respuesta sin punto final",
              "conserva respuesta corta sin punto");
    const std::string tildes = "áéíóú ñ áéíóú ñ áéíóú";
    const std::string cortado = limpiar_para_tts(tildes, 9);
    comprobar(contar_caracteres(cortado) > 0 && cortado.find('\xC3') != cortado.size() - 1,
              "no parte caracteres UTF-8");

    comprobar(limpiar_transcripcion(" [BLANK_AUDIO] ") == "", "blank audio es vacío");
    comprobar(limpiar_transcripcion("(música) Hola  Jarvis ") == "Hola Jarvis", "quita marcas STT");
    comprobar(limpiar_transcripcion(" ... ") == "", "solo puntuación es vacío");

    const std::map<std::string, std::string> vars = {{"bin", "/opt/x"}, {"texto", "hola; rm -rf ~ $(id)"}};
    const auto argv = expandir_argv("{bin}/tts --out=a.wav {texto}", vars);
    comprobar(argv.size() == 3 && argv[0] == "/opt/x/tts" && argv[2] == "hola; rm -rf ~ $(id)",
              "texto con metacaracteres queda como un solo argumento");
    bool lanza = false;
    try {
        expandir_argv("{desconocida}", vars);
    } catch (const std::exception&) {
        lanza = true;
    }
    comprobar(lanza, "variable desconocida lanza excepción");

    const auto c = Config::desde_texto("[slm]\nmax = 96\n# comentario\n[general]\ntmp = /tmp/j\n");
    comprobar(c.entero("slm.max", 0) == 96 && c.texto("general.tmp") == "/tmp/j", "config INI");
    const auto c2 =
        Config::desde_texto("[g]\nhome = /h\n[s]\nm = {g.home}/m.bin\ncmd = x -m {s.m} -f {in_wav}\n");
    comprobar(c2.texto("s.m") == "/h/m.bin", "resuelve referencias entre claves");
    const auto c3 =
        Config::desde_texto("[s]\npista = Hola, Jarvis.\ncmd = w --prompt {s.pista} -f {in_wav}\n");
    auto vars3 = c3.valores();
    vars3["in_wav"] = "a.wav";
    const auto argv3 = expandir_argv(c3.texto("s.cmd"), vars3);
    comprobar(argv3.size() == 5 && argv3[2] == "Hola, Jarvis." && argv3[4] == "a.wav",
              "valor con espacios de la config queda como un solo argumento");
    comprobar(limpiar_para_tts("--help me") == "help me", "texto no empieza con guion");

    const auto r_ok = ejecutar({"sh", "-c", "exit 0"}, 5, "", "");
    comprobar(r_ok.ok(), "proceso exitoso");
    const auto r_cod = ejecutar({"sh", "-c", "exit 3"}, 5, "", "");
    comprobar(!r_cod.ok() && r_cod.codigo_salida == 3, "código de salida propagado");
    const auto r_to = ejecutar({"sleep", "5"}, 1, "", "");
    comprobar(r_to.timeout && r_to.ms < 3000, "timeout mata al hijo");
    const auto r_no = ejecutar({"/no/existe/binario"}, 5, "", "");
    comprobar(r_no.no_arranco, "binario inexistente detectado");

    std::cout << (fallos == 0 ? "TODAS LAS PRUEBAS PASARON" : "HAY FALLOS") << "\n";
    return fallos == 0 ? 0 : 1;
}
