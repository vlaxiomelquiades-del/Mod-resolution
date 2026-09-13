# ResScale 1.1.0

Reduz a resolução interna do swapchain Vulkan do Minecraft Bedrock entre 30% e 100%.

## Por que esta versão não usa o Mod Menu?

O tombstone observado em LeviLaunchroid 1.5.23 cai dentro de `pl::modmenu::registerModule()`.
Há um problema de compatibilidade ABI documentado no ecossistema do LeviLaunchroid em que um mod compilado contra um layout antigo de `ModuleInfo` pode crashar no registro.

Por isso esta versão remove completamente `ModuleBuilder` do caminho de inicialização. Isso deixa o carregamento do mod independente do registro do Mod Menu.

## Configuração

O valor é persistido em:

`config/config.json`

Exemplo:

```json
{
  "version": 1,
  "percent": 60
}
```

Valores aceitos: `30` a `100`.

- `100` = resolução nativa
- `70` = 70% da largura/altura do swapchain
- `50` = metade da largura/altura

A alteração entra na próxima recriação do swapchain.

## Build

O workflow de GitHub Actions cria uma build ARM64 (`arm64-v8a`) e o `.levipack`.

Para máxima compatibilidade com o launcher instalado, compile contra o mesmo Preloader usado pelo seu LeviLaunchroid. O workflow aceita `PRELOADER_REF` manualmente; por padrão usa `main`.
