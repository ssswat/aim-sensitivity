// First-Person Aim Sensitivity (Dusklight native mod)
//
// Scales how fast the camera/aim moves while Link aims in first person with the Bow, Slingshot,
// Hookshot or Double Clawshots. Nothing else is touched.
//
// How it works (and why it plays nice with other mods):
//   * The game funnels every first-person aim source (analog stick, mouse, gyro, touch) through
//     daAlink_c::setBodyAngleToCamera(). We attach a PRE and a POST hook to it; we never replace
//     it, so other mods can hook the same function without conflicts.
//   * PRE remembers Link's aim angles. POST measures how much the original function (plus anything
//     other mods did in between) moved them this frame and rescales that delta.
//   * At 100% (the default) the hooks return immediately: the game behaves exactly like vanilla.

// INCLUDE ORDER MATTERS: standard library and mod SDK headers first, game headers last.
// Game headers (dolphin/types.h) define `nullptr` as a macro when the compiler reports an old
// __cplusplus value (MSVC does by default). Any standard header included AFTER that point breaks
// on Windows, so every standard header we (or the game headers) might need is pulled in up front.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "mods/service.hpp"
#include "mods/svc/config.h"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.h"
#include "mods/svc/ui.h"

// Game headers
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"

DEFINE_MOD();
IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(ConfigService, svc_config);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(UiService, svc_ui);

DEFINE_HOOK(&daAlink_c::setBodyAngleToCamera, BodyAngleToCamera);

namespace {

constexpr int64_t kDefaultPercent = 100;
constexpr int64_t kMinPercent = 10;   // below this the aim would feel frozen
constexpr int64_t kMaxPercent = 300;  // above this a tiny nudge becomes a spin

// Run after other mods' pre-hooks (so our baseline includes their changes) and after their
// post-hooks (so we rescale the final result of the frame).
constexpr int32_t kHookPriority = -1000;

ConfigVarHandle g_cvarSensitivity = 0;
int64_t g_percent = kDefaultPercent;  // cached copy of the config var, always clamped

struct PendingAim {
    bool valid = false;
    const daAlink_c* link = nullptr;
    s16 yaw = 0;
    s16 pitch = 0;
    float scale = 1.0f;
};
PendingAim g_pending;

void set_percent(int64_t value) {
    g_percent = std::clamp(value, kMinPercent, kMaxPercent);
}

// Only the first-person aiming items the mod is meant for.
bool is_supported_item(u16 item) {
    return item == dItemNo_BOW_e || item == dItemNo_PACHINKO_e || item == dItemNo_HOOKSHOT_e ||
           item == dItemNo_W_HOOKSHOT_e;
}

HookAction on_aim_pre(ModContext*, void* args, void*, void*) {
    g_pending.valid = false;

    if (g_percent == kDefaultPercent) {
        return HOOK_CONTINUE;  // vanilla behaviour, zero work
    }

    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || !is_supported_item(link->mEquipItem)) {
        return HOOK_CONTINUE;
    }
    // Same condition the original uses before touching the aim angles (first-person camera on).
    if (!dComIfGp_checkCameraAttentionStatus(link->field_0x317c, 0x10)) {
        return HOOK_CONTINUE;
    }

    g_pending.valid = true;
    g_pending.link = link;
    g_pending.yaw = link->shape_angle.y;
    g_pending.pitch = link->mBodyAngle.x;
    g_pending.scale = static_cast<float>(g_percent) / 100.0f;
    return HOOK_CONTINUE;
}

int scale_delta(int delta, float scale) {
    const long scaled = std::lround(static_cast<float>(delta) * scale);
    return static_cast<int>(std::clamp<long>(scaled, INT16_MIN, INT16_MAX));
}

void on_aim_post(ModContext*, void* args, void*, void*) {
    if (!g_pending.valid) {
        return;
    }
    const PendingAim pending = g_pending;
    g_pending.valid = false;  // consume: every post pairs with at most one pre

    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    if (link == nullptr || link != pending.link) {
        return;
    }
    // The item can change inside the original call (e.g. aiming was cancelled); then do nothing.
    if (!is_supported_item(link->mEquipItem)) {
        return;
    }

    // Wrap-safe angle deltas (angles are 16-bit, 0x10000 = full turn).
    const int deltaYaw = static_cast<s16>(link->shape_angle.y - pending.yaw);
    const int deltaPitch = static_cast<s16>(link->mBodyAngle.x - pending.pitch);
    if (deltaYaw == 0 && deltaPitch == 0) {
        return;
    }

    const s16 postPitch = link->mBodyAngle.x;

    const s16 newYaw = static_cast<s16>(pending.yaw + scale_delta(deltaYaw, pending.scale));

    int newPitch = pending.pitch + scale_delta(deltaPitch, pending.scale);
    // Keep the pitch inside the limits the game itself uses, so that a sensitivity above 100%
    // can never aim further than vanilla allows.
    const daAlinkHIO_c* hio = link->mpHIO;
    if (hio != nullptr && hio->mItem.m.mItemFPUpMaxUnk <= hio->mItem.m.mItemFPMaxUnk) {
        newPitch =
            std::clamp<int>(newPitch, hio->mItem.m.mItemFPUpMaxUnk, hio->mItem.m.mItemFPMaxUnk);
    } else {
        // Limits unavailable: never go beyond what the original already produced.
        newPitch = std::clamp<int>(newPitch, std::min<int>(pending.pitch, postPitch),
            std::max<int>(pending.pitch, postPitch));
    }
    // Same "item sink" rule as the original: don't tilt further down than the previous frame.
    if (link->checkNotItemSinkLimit() && newPitch > 0 && newPitch > pending.pitch) {
        newPitch = pending.pitch;
    }

    link->shape_angle.y = newYaw;
    link->mBodyAngle.x = static_cast<s16>(newPitch);
    // The original mirrors the final angles into these (read back by the camera next frame).
    link->field_0x310a = link->mBodyAngle.x;
    link->field_0x310c = link->shape_angle.y;

    if (link->mBodyAngle.x != postPitch) {
        link->checkBodyAngleX(link->mBodyAngle.x);  // wall push-out, as the original does
    }
}

void on_config_changed(ModContext*, ConfigVarHandle, const ConfigVarValue* value,
    const ConfigVarValue*, void*) {
    if (value != nullptr) {
        set_percent(value->int_value);
    }
}

ModResult build_panel(ModContext*, UiElementHandle panel, void*, ModError*) {
    svc_ui->pane_add_section(mod_ctx, panel, "First-person aiming");
    svc_ui->pane_add_text(mod_ctx, panel,
        "Speed of the aim in first person with the Bow, Slingshot, Hookshot and Double "
        "Clawshots. 100% is the vanilla feel.",
        nullptr);

    UiControlDesc control = UI_CONTROL_DESC_INIT;
    control.kind = UI_CONTROL_NUMBER;
    control.label = "Aim sensitivity";
    control.help_rml =
        "Lower = slower and more precise, higher = faster. Applies to stick, mouse, gyro and "
        "touch aiming. Other items (boomerang, ball and chain, etc.) are not affected.";
    control.binding = UI_BINDING_CONFIG_VAR;
    control.config_var = g_cvarSensitivity;
    control.min = kMinPercent;
    control.max = kMaxPercent;
    control.step = 5;
    control.suffix = "%";
    return svc_ui->pane_add_control(mod_ctx, panel, &control, nullptr);
}

}  // namespace

extern "C" {

MOD_EXPORT ModResult mod_initialize(ModError* error) {
    g_pending = {};
    set_percent(kDefaultPercent);

    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
    desc.name = "sensitivity";
    desc.type = CONFIG_VAR_INT;
    desc.default_int = kDefaultPercent;
    ModResult result = svc_config->register_var(mod_ctx, &desc, &g_cvarSensitivity);
    if (result != MOD_OK) {
        return mods::set_error(error, result, "failed to register the sensitivity setting");
    }

    // Values loaded from config.json do not fire callbacks: read the starting value explicitly.
    int64_t stored = kDefaultPercent;
    if (svc_config->get_int(mod_ctx, g_cvarSensitivity, &stored) == MOD_OK) {
        set_percent(stored);
    }
    svc_config->subscribe(mod_ctx, g_cvarSensitivity, on_config_changed, nullptr, nullptr);

    HookOptions options{};
    options.struct_size = sizeof(HookOptions);
    options.priority = kHookPriority;
    options.replace_policy = HOOK_REPLACE_CONFLICT;

    result = mods::hook::add_pre<BodyAngleToCamera>(on_aim_pre, &options);
    if (result != MOD_OK) {
        return mods::set_error(
            error, result, "could not hook the first-person aim (Dusklight version mismatch?)");
    }
    result = mods::hook::add_post<BodyAngleToCamera>(on_aim_post, &options);
    if (result != MOD_OK) {
        return mods::set_error(error, result, "could not hook the first-person aim (post)");
    }

    UiModsPanelDesc panel = UI_MODS_PANEL_DESC_INIT;
    panel.build = build_panel;
    svc_ui->register_mods_panel(mod_ctx, &panel);

    svc_log->info(mod_ctx, "first-person aim sensitivity ready");
    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    // Hooks, config subscriptions and the panel are removed by the loader.
    g_pending = {};
    g_cvarSensitivity = 0;
    set_percent(kDefaultPercent);
    return MOD_OK;
}

}  // extern "C"
