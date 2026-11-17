#pragma once

#include <map>
#include <string>
#include <vector>

namespace jarvis {

// Limpia la salida de STT: quita marcas como [BLANK_AUDIO], (música), espacios repetidos.
std::string limpiar_transcripcion(const std::string& texto);

// Limpia la respuesta del SLM antes del TTS: elimina bloques <think>...</think>, tokens de
// fin ([end of text], <|im_end|>), markdown, URLs y emojis; recorta a max_bytes respetando UTF-8
// y, si es posible, el final de una frase.
std::string limpiar_para_tts(const std::string& texto, std::size_t max_bytes = 400);

// Divide una plantilla de argv por espacios y sustituye {variables} DENTRO de cada token.
// El valor sustituido nunca se vuelve a dividir: un texto con espacios o metacaracteres de
// shell queda como un único argumento literal. Variables desconocidas lanzan excepción.
std::vector<std::string> expandir_argv(const std::string& plantilla,
                                       const std::map<std::string, std::string>& variables);

// Utilidades de archivo.
std::string leer_archivo(const std::string& ruta);
bool escribir_archivo(const std::string& ruta, const std::string& contenido);

// Número de caracteres (puntos de código) UTF-8.
std::size_t contar_caracteres(const std::string& texto);

}  // namespace jarvis
