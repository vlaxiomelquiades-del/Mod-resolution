#pragma once

#include <optional>
#include <string_view>

#include <pl/Config.hpp>

namespace resscale {

// Config persistido em config/config.json pelo pl::config::ConfigFile.
struct ResScaleConfig {
  int version = 1;
  int percent = 100; // 30-100. 100 = resolução nativa (sem downscale).
};

inline constexpr int kMinPercent = 30;
inline constexpr int kMaxPercent = 100;

} // namespace resscale

namespace pl::config {

// Metadados usados pelo gerador/editor de config do launcher.
// Nesta versão o valor é controlado pelo config.json para evitar a ponte ABI
// do Mod Menu que causa crash em alguns Preloaders recentes.
template <> struct Schema<resscale::ResScaleConfig> {
  static constexpr std::string_view title = "Resolução do Jogo";
  static constexpr std::string_view description =
      "Escala a resolução interna de renderização entre 30% e 100% da "
      "resolução nativa da tela.";

  static constexpr FieldSchema field(std::string_view name) {
    if (name == "version") {
      return {"Versão", "Versão do schema de config (gerenciada pelo mod).",
              std::nullopt, std::nullopt, true};
    }
    if (name == "percent") {
      return {"Resolução (%)",
              "Percentual da resolução nativa usado para renderizar o jogo.",
              resscale::kMinPercent, resscale::kMaxPercent, false};
    }
    return {};
  }
};

} // namespace pl::config
