#include "ResScaleConfig.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#include <vulkan/vulkan.h>

#include <pl/Config.hpp>
#include <pl/Mod.hpp>
#include <pl/memory/Hook.hpp>
#include <pl/memory/Signature.hpp>

// ============================================================================
// ResScale
//
// Hook direto em vkCreateSwapchainKHR (libvulkan.so, API estável do NDK) que
// reduz VkSwapchainCreateInfoKHR::imageExtent antes de repassar pra função
// real. Não usa root/su, não usa Game Mode do Android — é tudo feito de
// dentro do próprio processo do jogo via hook nativo.
//
// (v1 tentou ANativeWindow_setBuffersGeometry e não funcionou porque o
// RenderDragon nessa build usa Vulkan e decide o extent do swapchain
// sozinho, sem olhar pro buffer da window por fora.)
//
// LIMITAÇÃO: como não dá pra redimensionar um VkSwapchainKHR já criado,
// mudar o slider no meio do jogo só tem efeito visual na PRÓXIMA vez que o
// jogo recriar o swapchain (entrar/sair de um mundo, alternar o app, girar
// a tela). Isso é esperado, não é bug do hook.
// ============================================================================

namespace {

using resscale::ResScaleConfig;

using CreateSwapchainFn = VkResult (*)(VkDevice, const VkSwapchainCreateInfoKHR *,
                                       const VkAllocationCallbacks *, VkSwapchainKHR *);


} // namespace

class ResScaleMod {
public:
  static ResScaleMod &instance() {
    static ResScaleMod mod;
    return mod;
  }

  ResScaleMod() : mSelf(*ll::mod::NativeMod::current()) {}

  [[nodiscard]] ll::mod::NativeMod &getSelf() const { return mSelf; }

  bool load() {
    auto &self = getSelf();
    mConfigFile.emplace();
    if (!mConfigFile->load()) {
      self.getLogger().error("Falha ao carregar config do ResScale");
      mConfigFile.reset();
      return false;
    }
    normalizeConfig();
    mConfigFile->save();
    return true;
  }

  bool enable() {
    auto &self = getSelf();
    const int percent = mConfigFile ? mConfigFile->value().percent : 100;

    // IMPORTANTE: esta versão não registra ModuleBuilder/Mod Menu.
    // O LeviLaunchroid possui uma regressão de ABI em versões recentes do
    // Preloader que pode derrubar mods compilados contra um SDK anterior
    // justamente durante registerModule(). O hook de resolução não depende
    // desse caminho, então mantemos o controle pelo config.json.
    const uintptr_t target =
        pl::memory::resolveSignature("vkCreateSwapchainKHR", "libvulkan.so");
    if (target == 0) {
      self.getLogger().error(
          "vkCreateSwapchainKHR não foi encontrada em libvulkan.so");
      return false;
    }

    mHook = pl::memory::HookHandle(
        reinterpret_cast<void *>(target),
        reinterpret_cast<void *>(&detourCreateSwapchain),
        reinterpret_cast<void **>(&mOriginal));

    if (!mHook.installed() || mOriginal == nullptr) {
      self.getLogger().error(
          "Falha ao instalar hook em vkCreateSwapchainKHR (target={})",
          reinterpret_cast<void *>(target));
      mHook.reset();
      mOriginal = nullptr;
      return false;
    }

    self.getLogger().info(
        "ResScale ativo. Resolução: {}%. Configure percent em config/config.json.",
        percent);
    return true;
  }

  bool disable() {
    mHook.reset();
    mOriginal = nullptr;
    getSelf().getLogger().info(
        "ResScale desativado (efeito volta ao normal na próxima recriação do swapchain)");
    return true;
  }

  bool unload() {
    mConfigFile.reset();
    return true;
  }

private:
  ll::mod::NativeMod &mSelf;
  std::optional<pl::config::ConfigFile<ResScaleConfig>> mConfigFile;
  pl::memory::HookHandle mHook;
  CreateSwapchainFn mOriginal{};

  std::mutex mStateMutex;
  uint32_t mLastRequestedWidth{0};
  uint32_t mLastRequestedHeight{0};

  void normalizeConfig() {
    if (!mConfigFile) {
      return;
    }
    auto &cfg = mConfigFile->value();
    cfg.percent =
        std::clamp(cfg.percent, resscale::kMinPercent, resscale::kMaxPercent);
  }

  // --- Hook: vkCreateSwapchainKHR -------------------------------------------

  static VkResult detourCreateSwapchain(VkDevice device,
                                        const VkSwapchainCreateInfoKHR *pCreateInfo,
                                        const VkAllocationCallbacks *pAllocator,
                                        VkSwapchainKHR *pSwapchain) {
    return instance().handleCreateSwapchain(device, pCreateInfo, pAllocator,
                                            pSwapchain);
  }

  VkResult handleCreateSwapchain(VkDevice device,
                                 const VkSwapchainCreateInfoKHR *pCreateInfo,
                                 const VkAllocationCallbacks *pAllocator,
                                 VkSwapchainKHR *pSwapchain) {
    if (pCreateInfo == nullptr) {
      return mOriginal(device, pCreateInfo, pAllocator, pSwapchain);
    }

    const int percent = mConfigFile ? mConfigFile->value().percent : 100;

    {
      std::lock_guard<std::mutex> lock(mStateMutex);
      mLastRequestedWidth = pCreateInfo->imageExtent.width;
      mLastRequestedHeight = pCreateInfo->imageExtent.height;
    }

    if (percent >= resscale::kMaxPercent || pCreateInfo->imageExtent.width == 0 ||
        pCreateInfo->imageExtent.height == 0) {
      return mOriginal(device, pCreateInfo, pAllocator, pSwapchain);
    }

    // Copia o struct (é POD, os ponteiros que ele carrega já são externos)
    // pra poder mexer no imageExtent sem tocar no struct do chamador.
    VkSwapchainCreateInfoKHR scaled = *pCreateInfo;
    uint32_t w = (pCreateInfo->imageExtent.width * static_cast<uint32_t>(percent)) / 100;
    uint32_t h = (pCreateInfo->imageExtent.height * static_cast<uint32_t>(percent)) / 100;
    w = std::max(w, 1u);
    h = std::max(h, 1u);
    scaled.imageExtent.width = w;
    scaled.imageExtent.height = h;

    getSelf().getLogger().info(
        "ResScale: swapchain {}x{} -> {}x{} ({}%)", pCreateInfo->imageExtent.width,
        pCreateInfo->imageExtent.height, w, h, percent);

    return mOriginal(device, &scaled, pAllocator, pSwapchain);
  }

};

PL_REGISTER_MOD(ResScaleMod, ResScaleMod::instance())
