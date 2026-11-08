/**
 * Switch 2 Remake Controls
 *
 * Recreates the control scheme of "The Legend of Zelda: Ocarina of Time" (Nintendo Switch 2, 2026) in 2Ship:
 *   Y  = Attack (N64 B)                     X  = Jump
 *   A  = Action, hold A while running = Sprint
 *   B  = Put away / cancel        ZL = Target + Shield (N64 Z + R)
 *   No automatic ledge jumps: X does the same jump the game used to do automatically
 *   ZR = Use the one equipped item (N64 C-Right)
 *   hold L or R = horizontal Quick Select bar (game pauses, release to equip to ZR)
 *   D-pad Left = Ocarina, Up = assigned mask, Down = assigned transformation mask, Right = Tatl (N64 C-Up)
 *   R3 = reset camera, - = map, + = menu
 *   Ocarina notes: L, R, A, Y, X (low to high), B to stop playing
 *
 * The physical buttons are mapped to the BTN_CUSTOM_REMAKE_* bits in the controller config, and translated here
 * into the N64 buttons the game understands, depending on whether we are in gameplay, a menu/textbox, or playing
 * the ocarina.
 */
#include "RemakeControls.h"

#include <chrono>
#include <vector>
#include <imgui.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <fast/Fast3dGui.h>

#include "2s2h/BenPort.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "sfx.h"
extern u8 sIsOcarinaInputEnabled;
extern TexturePtr gItemIcons[131];
}

#define CVAR_REMAKE CVarGetInteger(CVAR_REMAKE_CONTROLS, 0)

#define RM_ALL                                                                                                     \
    (BTN_CUSTOM_REMAKE_B | BTN_CUSTOM_REMAKE_X | BTN_CUSTOM_REMAKE_Y | BTN_CUSTOM_REMAKE_L | BTN_CUSTOM_REMAKE_R | \
     BTN_CUSTOM_REMAKE_ZR | BTN_CUSTOM_REMAKE_R3 | BTN_CUSTOM_REMAKE_MINUS)

// Number of padmgr updates A must be held (while running) before sprinting instead of rolling
#define SPRINT_HOLD_FRAMES 6
#define SPRINT_SPEED_MULTIPLIER 1.5f
// Number of padmgr updates the ZL shield stays lowered after attacking / using an item
#define SHIELD_DROP_FRAMES 12
// Number of padmgr updates after releasing ZR before the bow aim is left, so the arrow is fired first
#define EXIT_AIM_FRAMES 6

namespace {

enum class BMode { None, Shield, Cancel, PutAway };

struct QuickSelectEntry {
    u8 item;
    u8 slot;
};

struct QuickSelectState {
    bool open = false;
    std::vector<QuickSelectEntry> entries;
    int cursor = 0;
    int heldDir = 0;
    std::chrono::steady_clock::time_point nextRepeat;
};

uint32_t sPrevRaw = 0;
uint32_t sSuppressUntilReleased = 0;
BMode sBMode = BMode::None;
bool sJumpQueued = false;
bool sJumpThisFrame = false;
bool sAWithheld = false;
bool sSprinting = false;
int sAHoldFrames = 0;
int sAPulse = 0;
int sShieldDropFrames = 0;
Actor* sAimTarget = nullptr;
bool sAimZWasHeld = false;
bool sAimZL = false;
bool sZRWasHeld = false;
int sExitAimFrames = 0;
QuickSelectState sQS;

const char* GetItemNameDE(u8 item) {
    switch (item) {
        case ITEM_OCARINA_OF_TIME:
            return "Okarina der Zeit";
        case ITEM_BOW:
            return "Heroen-Bogen";
        case ITEM_ARROW_FIRE:
            return "Feuerpfeil";
        case ITEM_ARROW_ICE:
            return "Eispfeil";
        case ITEM_ARROW_LIGHT:
            return "Lichtpfeil";
        case ITEM_BOMB:
            return "Bomben";
        case ITEM_BOMBCHU:
            return "Krabbelminen";
        case ITEM_DEKU_STICK:
            return "Deku-Stab";
        case ITEM_DEKU_NUT:
            return "Deku-Nuss";
        case ITEM_MAGIC_BEANS:
            return "Wundererbse";
        case ITEM_POWDER_KEG:
            return "Pulverfass";
        case ITEM_PICTOGRAPH_BOX:
            return "Bildbox";
        case ITEM_LENS_OF_TRUTH:
            return "Auge der Wahrheit";
        case ITEM_HOOKSHOT:
            return "Fanghaken";
        case ITEM_SWORD_GREAT_FAIRY:
            return "Feenschwert";
        case ITEM_BOTTLE:
            return "Leere Flasche";
        case ITEM_POTION_RED:
            return "Rotes Elixier";
        case ITEM_POTION_GREEN:
            return "Gr\xC3\xBCnes Elixier";
        case ITEM_POTION_BLUE:
            return "Blaues Elixier";
        case ITEM_FAIRY:
            return "Fee";
        case ITEM_DEKU_PRINCESS:
            return "Deku-Prinzessin";
        case ITEM_MILK_BOTTLE:
            return "Milch";
        case ITEM_MILK_HALF:
            return "Milch (halb)";
        case ITEM_FISH:
            return "Fisch";
        case ITEM_BUG:
            return "K\xC3\xA4"
                   "fer";
        case ITEM_BLUE_FIRE:
            return "Blaues Feuer";
        case ITEM_POE:
            return "Irrlicht";
        case ITEM_BIG_POE:
            return "Nachtschw\xC3\xA4rmer";
        case ITEM_SPRING_WATER:
            return "Quellwasser";
        case ITEM_HOT_SPRING_WATER:
            return "Hei\xC3\x9F"
                   "es Quellwasser";
        case ITEM_ZORA_EGG:
            return "Zora-Ei";
        case ITEM_GOLD_DUST:
            return "Goldstaub";
        case ITEM_MUSHROOM:
            return "Magischer Pilz";
        case ITEM_SEAHORSE:
            return "Seepferdchen";
        case ITEM_CHATEAU:
            return "Chateau Romani";
        case ITEM_HYLIAN_LOACH:
            return "Hylia-Schmerle";
        case ITEM_OBABA_DRINK:
            return "Omas Trank";
        case ITEM_MOONS_TEAR:
            return "Mondtr\xC3\xA4ne";
        case ITEM_DEED_LAND:
            return "Grundbuch (Stadt)";
        case ITEM_DEED_SWAMP:
            return "Grundbuch (Sumpf)";
        case ITEM_DEED_MOUNTAIN:
            return "Grundbuch (Berg)";
        case ITEM_DEED_OCEAN:
            return "Grundbuch (Meer)";
        case ITEM_ROOM_KEY:
            return "Zimmerschl\xC3\xBCssel";
        case ITEM_LETTER_MAMA:
            return "Expresspost an Mama";
        case ITEM_LETTER_TO_KAFEI:
            return "Brief an Kafei";
        case ITEM_PENDANT_OF_MEMORIES:
            return "Erinnerungsanh\xC3\xA4nger";
        default:
            return "";
    }
}

bool ItemHasAmmo(u8 item) {
    switch (item) {
        case ITEM_BOW:
        case ITEM_BOMB:
        case ITEM_BOMBCHU:
        case ITEM_DEKU_STICK:
        case ITEM_DEKU_NUT:
        case ITEM_MAGIC_BEANS:
        case ITEM_POWDER_KEG:
            return true;
        default:
            return false;
    }
}

bool IsGameplayContext() {
    if (gPlayState == nullptr) {
        return false;
    }
    if (IS_PAUSED(&gPlayState->pauseCtx)) {
        return false;
    }
    if (gPlayState->msgCtx.msgMode != MSGMODE_NONE) {
        return false;
    }
    return true;
}

bool CanOpenQuickSelect() {
    if (!IsGameplayContext()) {
        return false;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return false;
    }
    if ((gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) || (gPlayState->transitionMode != TRANS_MODE_OFF)) {
        return false;
    }
    if (gPlayState->csCtx.state != CS_STATE_IDLE) {
        return false;
    }
    if (Player_InBlockingCsMode(gPlayState, player)) {
        return false;
    }
    if (player->stateFlags1 & PLAYER_STATE1_DEAD) {
        return false;
    }
    return true;
}

void SetGamePaused(bool paused) {
    if (gPlayState != nullptr) {
        gPlayState->frameAdvCtx.enabled = paused;
        gPlayState->frameAdvCtx.timer = 0;
    }
}

void EquipToZR(const QuickSelectEntry& entry) {
    u8 equipItem = entry.item;

    // Magic arrows are equipped as the bow + arrow combination, just like the pause menu does
    if ((entry.item >= ITEM_ARROW_FIRE) && (entry.item <= ITEM_ARROW_LIGHT)) {
        if (INV_CONTENT(ITEM_BOW) != ITEM_BOW) {
            Audio_PlaySfx(NA_SE_SY_ERROR);
            return;
        }
        equipItem = ITEM_BOW_FIRE + (entry.item - ITEM_ARROW_FIRE);
    }

    // Remove the item from the other (hidden) C buttons so it is only on ZR
    for (s32 btn = EQUIP_SLOT_C_LEFT; btn <= EQUIP_SLOT_C_DOWN; btn++) {
        if (C_SLOT_EQUIP(0, btn) == entry.slot) {
            BUTTON_ITEM_EQUIP(0, btn) = ITEM_NONE;
            C_SLOT_EQUIP(0, btn) = SLOT_NONE;
        }
    }

    BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_RIGHT) = equipItem;
    C_SLOT_EQUIP(0, EQUIP_SLOT_C_RIGHT) = entry.slot;
    Interface_LoadItemIconImpl(gPlayState, EQUIP_SLOT_C_RIGHT);
    Audio_PlaySfx(NA_SE_SY_DECIDE);
}

void OpenQuickSelect() {
    sQS.entries.clear();
    // Item page slots, the Ocarina lives on D-pad Left and is therefore not part of the bar
    for (u8 slot = SLOT_OCARINA + 1; slot <= SLOT_BOTTLE_6; slot++) {
        u8 item = gSaveContext.save.saveInfo.inventory.items[slot];
        if (item == ITEM_NONE) {
            continue;
        }
        sQS.entries.push_back({ item, slot });
    }

    if (sQS.entries.empty()) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }

    sQS.cursor = 0;
    u8 equippedSlot = C_SLOT_EQUIP(0, EQUIP_SLOT_C_RIGHT);
    for (size_t i = 0; i < sQS.entries.size(); i++) {
        if (sQS.entries[i].slot == equippedSlot) {
            sQS.cursor = (int)i;
            break;
        }
    }

    sQS.heldDir = 0;
    sQS.open = true;
    SetGamePaused(true);
    Audio_PlaySfx(NA_SE_SY_WIN_OPEN);
}

void CloseQuickSelect(bool equip, uint32_t raw) {
    if (!sQS.open) {
        return;
    }
    sQS.open = false;
    SetGamePaused(false);

    if (gPlayState != nullptr) {
        if (equip && (sQS.cursor >= 0) && (sQS.cursor < (int)sQS.entries.size())) {
            EquipToZR(sQS.entries[sQS.cursor]);
        } else {
            Audio_PlaySfx(NA_SE_SY_WIN_CLOSE);
        }
    }

    // Do not let the buttons that closed the menu leak into gameplay
    sSuppressUntilReleased |= raw & (BTN_A | RM_ALL);
}

uint32_t UpdateQuickSelect(uint32_t raw, uint32_t pressed, int8_t* stickX, int8_t* stickY, int8_t* rStickX,
                           int8_t* rStickY) {
    if (!IsGameplayContext()) {
        CloseQuickSelect(false, raw);
        return 0;
    }

    int dir = 0;
    if ((raw & BTN_DLEFT) || (*stickX < -40) || (*rStickX < -40)) {
        dir = -1;
    } else if ((raw & BTN_DRIGHT) || (*stickX > 40) || (*rStickX > 40)) {
        dir = 1;
    }

    auto now = std::chrono::steady_clock::now();
    int count = (int)sQS.entries.size();
    if (dir != 0 && count > 0) {
        bool move = false;
        if (dir != sQS.heldDir) {
            move = true;
            sQS.nextRepeat = now + std::chrono::milliseconds(300);
        } else if (now >= sQS.nextRepeat) {
            move = true;
            sQS.nextRepeat = now + std::chrono::milliseconds(110);
        }
        if (move) {
            sQS.cursor = (sQS.cursor + dir + count) % count;
            Audio_PlaySfx(NA_SE_SY_CURSOR);
        }
    }
    sQS.heldDir = dir;

    if (pressed & BTN_CUSTOM_REMAKE_B) {
        CloseQuickSelect(false, raw);
    } else if ((pressed & BTN_A) || !(raw & (BTN_CUSTOM_REMAKE_L | BTN_CUSTOM_REMAKE_R))) {
        CloseQuickSelect(true, raw);
    }

    *stickX = 0;
    *stickY = 0;
    *rStickX = 0;
    *rStickY = 0;
    return 0;
}

bool CanSprint(Player* player, int8_t stickX, int8_t stickY) {
    if (player == nullptr) {
        return false;
    }
    if ((player->transformation != PLAYER_FORM_HUMAN) && (player->transformation != PLAYER_FORM_FIERCE_DEITY)) {
        return false;
    }
    if (!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        return false;
    }
    if (player->stateFlags1 & (PLAYER_STATE1_8000000 | PLAYER_STATE1_Z_TARGETING | PLAYER_STATE1_CARRYING_ACTOR |
                               PLAYER_STATE1_100000 | PLAYER_STATE1_FRIENDLY_ACTOR_FOCUS | PLAYER_STATE1_800000)) {
        return false;
    }
    if ((stickX * stickX + stickY * stickY) < (30 * 30)) {
        return false;
    }
    u16 doAction = gPlayState->interfaceCtx.aButtonDoAction;
    return (doAction == DO_ACTION_NONE) || (doAction == DO_ACTION_PUTAWAY) || (doAction == DO_ACTION_ATTACK);
}

uint32_t TranslateGameplay(uint32_t raw, uint32_t pressed, uint32_t out, int8_t* stickX, int8_t* stickY) {
    Player* player = GET_PLAYER(gPlayState);

    if (raw & BTN_CUSTOM_REMAKE_Y) {
        out |= BTN_B;
    }
    if (raw & BTN_CUSTOM_REMAKE_ZR) {
        out |= BTN_CRIGHT;
    }
    if (raw & BTN_CUSTOM_REMAKE_R3) {
        out |= BTN_Z;
    }
    // - opens the pause menu on the map page, + on the item page
    if (pressed & BTN_CUSTOM_REMAKE_MINUS) {
        gPlayState->pauseCtx.pageIndex = PAUSE_MAP;
        out |= BTN_START;
    } else if (pressed & BTN_START) {
        gPlayState->pauseCtx.pageIndex = PAUSE_ITEM;
    }
    // D-pad Right talks to Tatl
    if (raw & BTN_DRIGHT) {
        out = (out & ~BTN_DRIGHT) | BTN_CUP;
    }

    // B: decided when pressed and kept while held
    bool putAwayA = false;
    if (pressed & BTN_CUSTOM_REMAKE_B) {
        if (raw & BTN_Z) {
            sBMode = BMode::Shield;
        } else if ((player != nullptr) && (player->stateFlags1 & PLAYER_STATE1_100000)) {
            sBMode = BMode::Cancel;
        } else if (gPlayState->interfaceCtx.aButtonDoAction == DO_ACTION_PUTAWAY) {
            sBMode = BMode::PutAway;
        } else {
            sBMode = BMode::None;
        }
    }
    if (raw & BTN_CUSTOM_REMAKE_B) {
        switch (sBMode) {
            case BMode::Shield:
                out |= BTN_R;
                break;
            case BMode::Cancel:
                out |= BTN_B;
                break;
            case BMode::PutAway:
                putAwayA = true;
                break;
            default:
                break;
        }
    } else {
        sBMode = BMode::None;
    }

    // A: tap = action/roll, hold while running = sprint
    out &= ~BTN_A;
    if (pressed & BTN_A) {
        sAWithheld = CanSprint(player, *stickX, *stickY);
        sAHoldFrames = 0;
        sSprinting = false;
    }
    if (raw & BTN_A) {
        if (sAWithheld) {
            if (++sAHoldFrames >= SPRINT_HOLD_FRAMES) {
                sSprinting = true;
            }
        } else {
            out |= BTN_A;
        }
    } else {
        if (sAWithheld && !sSprinting) {
            sAPulse = 2; // short tap -> send A so Link rolls
        }
        sAWithheld = false;
        sSprinting = false;
        sAHoldFrames = 0;
    }
    if (sAPulse > 0) {
        out |= BTN_A;
        sAPulse--;
    }
    if (putAwayA) {
        out |= BTN_A;
    }

    // ZL: targeting raises the shield (human / Fierce Deity). It is lowered briefly to attack, use items or jump.
    bool shieldForm = (player != nullptr) && ((player->transformation == PLAYER_FORM_HUMAN) ||
                                              (player->transformation == PLAYER_FORM_FIERCE_DEITY));
    if (shieldForm && (raw & BTN_Z) && !(player->stateFlags1 & PLAYER_STATE1_100000)) {
        if ((raw & (BTN_CUSTOM_REMAKE_Y | BTN_CUSTOM_REMAKE_ZR)) ||
            (pressed & (BTN_A | BTN_CUSTOM_REMAKE_X | BTN_DPAD))) {
            sShieldDropFrames = SHIELD_DROP_FRAMES;
        }
        if (sShieldDropFrames > 0) {
            sShieldDropFrames--;
        } else {
            out |= BTN_R;
        }
    } else {
        sShieldDropFrames = 0;
    }

    // X: jump, consumed by the player actor
    if (pressed & BTN_CUSTOM_REMAKE_X) {
        sJumpQueued = true;
    }

    // First person aiming: ZL locks onto a target instead of the vanilla switch to Z-target aiming
    bool firstPerson = (player != nullptr) && (player->stateFlags1 & PLAYER_STATE1_100000);
    sAimZL = firstPerson && (raw & BTN_Z);
    if (sAimZL) {
        out &= ~BTN_Z;
    }

    // Bow: releasing ZR shoots, then leaves the aim (vanilla stays in first person until A is pressed)
    bool zrHeld = (raw & BTN_CUSTOM_REMAKE_ZR) != 0;
    if (firstPerson && sZRWasHeld && !zrHeld && (player->heldItemAction >= PLAYER_IA_BOW) &&
        (player->heldItemAction <= PLAYER_IA_BOW_LIGHT)) {
        sExitAimFrames = EXIT_AIM_FRAMES;
    }
    sZRWasHeld = zrHeld;
    if (sExitAimFrames > 0) {
        if (zrHeld || !firstPerson) {
            sExitAimFrames = 0;
        } else if (--sExitAimFrames < 2) {
            out |= BTN_A; // "Return"
        }
    }

    return out;
}

uint32_t TranslateMenu(uint32_t raw, uint32_t pressed, uint32_t out) {
    if (raw & BTN_CUSTOM_REMAKE_B) {
        out |= BTN_B;
    }
    if (raw & BTN_CUSTOM_REMAKE_L) {
        out |= BTN_Z; // previous page
    }
    if (raw & BTN_CUSTOM_REMAKE_R) {
        out |= BTN_R; // next page
    }
    if (raw & BTN_CUSTOM_REMAKE_ZR) {
        out |= BTN_CRIGHT; // equip to ZR
    }
    // - closes the pause menu again
    if ((pressed & BTN_CUSTOM_REMAKE_MINUS) && (gPlayState != nullptr) && IS_PAUSED(&gPlayState->pauseCtx)) {
        out |= BTN_START;
    }
    return out;
}

uint32_t TranslateOcarina(uint32_t raw, uint32_t out) {
    out &= ~(BTN_A | BTN_B | BTN_CUP | BTN_CDOWN | BTN_CLEFT | BTN_CRIGHT);
    if (raw & BTN_CUSTOM_REMAKE_L) {
        out |= BTN_A; // D4
    }
    if (raw & BTN_CUSTOM_REMAKE_R) {
        out |= BTN_CDOWN; // F4
    }
    if (raw & BTN_A) {
        out |= BTN_CRIGHT; // A4
    }
    if (raw & BTN_CUSTOM_REMAKE_Y) {
        out |= BTN_CLEFT; // B4
    }
    if (raw & BTN_CUSTOM_REMAKE_X) {
        out |= BTN_CUP; // D5
    }
    if (raw & BTN_CUSTOM_REMAKE_B) {
        out |= BTN_B; // stop playing
    }
    return out;
}

void EnforceDpadLayout() {
    if (gPlayState == nullptr) {
        return;
    }
    // D-pad Left is always the Ocarina
    if ((gSaveContext.save.saveInfo.inventory.items[SLOT_OCARINA] == ITEM_OCARINA_OF_TIME) &&
        (DPAD_GET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_D_LEFT) != ITEM_OCARINA_OF_TIME)) {
        DPAD_SET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_D_LEFT, ITEM_OCARINA_OF_TIME);
        DPAD_SET_CUR_FORM_BTN_SLOT(EQUIP_SLOT_D_LEFT, SLOT_OCARINA);
        Interface_Dpad_LoadItemIconImpl(gPlayState, EQUIP_SLOT_D_LEFT);
    }
    // D-pad Right is Tatl, never an item
    if (DPAD_GET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_D_RIGHT) != ITEM_NONE) {
        DPAD_SET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_D_RIGHT, ITEM_NONE);
        DPAD_SET_CUR_FORM_BTN_SLOT(EQUIP_SLOT_D_RIGHT, SLOT_NONE);
    }
}

} // namespace

extern "C" uint32_t RemakeControls_TranslateButtons(uint32_t raw, int8_t* stickX, int8_t* stickY, int8_t* rStickX,
                                                    int8_t* rStickY) {
    if (!CVAR_REMAKE) {
        if (sQS.open) {
            CloseQuickSelect(false, raw);
        }
        sPrevRaw = raw;
        return raw;
    }

    uint32_t pressed = raw & ~sPrevRaw;
    sPrevRaw = raw;

    // Buttons that closed the quick select stay ignored until they are let go
    sSuppressUntilReleased &= raw;
    raw &= ~sSuppressUntilReleased;
    pressed &= ~sSuppressUntilReleased;

    if (sQS.open) {
        return UpdateQuickSelect(raw, pressed, stickX, stickY, rStickX, rStickY);
    }

    uint32_t out = raw & ~RM_ALL;

    if ((gPlayState != nullptr) && sIsOcarinaInputEnabled) {
        sAWithheld = false;
        sSprinting = false;
        return TranslateOcarina(raw, out);
    }

    if (!IsGameplayContext()) {
        sBMode = BMode::None;
        sAWithheld = false;
        sSprinting = false;
        return TranslateMenu(raw, pressed, out);
    }

    if ((pressed & (BTN_CUSTOM_REMAKE_L | BTN_CUSTOM_REMAKE_R)) && CanOpenQuickSelect()) {
        OpenQuickSelect();
        if (sQS.open) {
            sAWithheld = false;
            sSprinting = false;
            *stickX = 0;
            *stickY = 0;
            return 0;
        }
    }

    return TranslateGameplay(raw, pressed, out, stickX, stickY);
}

static bool ActorStillExists(PlayState* play, Actor* target) {
    for (s32 cat = 0; cat < ACTORCAT_MAX; cat++) {
        for (Actor* actor = play->actorCtx.actorLists[cat].first; actor != nullptr; actor = actor->next) {
            if (actor == target) {
                return actor->update != nullptr;
            }
        }
    }
    return false;
}

static Vec3f GetAimOrigin(Player* player) {
    Vec3f origin;
    origin.x = player->actor.world.pos.x;
    origin.y = player->bodyPartsPos[PLAYER_BODYPART_HEAD].y + 3.0f;
    origin.z = player->actor.world.pos.z;
    return origin;
}

static Actor* FindAimTarget(PlayState* play, Player* player) {
    Vec3f origin = GetAimOrigin(player);
    Actor* best = nullptr;
    s32 bestScore = 0x7FFFFFFF;

    for (s32 cat = 0; cat < ACTORCAT_MAX; cat++) {
        if (cat == ACTORCAT_PLAYER) {
            continue;
        }
        for (Actor* actor = play->actorCtx.actorLists[cat].first; actor != nullptr; actor = actor->next) {
            if ((actor->update == nullptr) || !(actor->flags & ACTOR_FLAG_ATTENTION_ENABLED) ||
                (actor->draw == nullptr)) {
                continue;
            }
            f32 dist = Math_Vec3f_DistXYZ(&origin, &actor->focus.pos);
            if ((dist < 20.0f) || (dist > 2000.0f)) {
                continue;
            }
            s16 yawDiff = (s16)(Math_Vec3f_Yaw(&origin, &actor->focus.pos) - player->actor.focus.rot.y);
            s16 pitchDiff = (s16)(Math_Vec3f_Pitch(&origin, &actor->focus.pos) - player->actor.focus.rot.x);
            s32 angle = ABS_ALT(yawDiff) + ABS_ALT(pitchDiff);
            // Only things roughly in front of the crosshair
            if ((ABS_ALT(yawDiff) > 0x2000) || (ABS_ALT(pitchDiff) > 0x1800)) {
                continue;
            }
            s32 score = angle + (s32)(dist * 2.0f);
            if (score < bestScore) {
                bestScore = score;
                best = actor;
            }
        }
    }
    return best;
}

extern "C" void RemakeControls_AimAssist(void* playPtr, void* playerPtr, int32_t zHeld) {
    PlayState* play = (PlayState*)playPtr;
    Player* player = (Player*)playerPtr;

    // ZL is taken away from the game while aiming in first person, see TranslateGameplay
    zHeld = zHeld || sAimZL;
    if (!CVAR_REMAKE || !zHeld) {
        sAimTarget = nullptr;
        sAimZWasHeld = false;
        return;
    }

    if (!sAimZWasHeld) {
        sAimTarget = FindAimTarget(play, player);
        if (sAimTarget != nullptr) {
            Audio_PlaySfx(NA_SE_SY_LOCK_ON);
        }
    }
    sAimZWasHeld = true;

    if ((sAimTarget == nullptr) || !ActorStillExists(play, sAimTarget)) {
        sAimTarget = nullptr;
        return;
    }

    Vec3f origin = GetAimOrigin(player);
    s16 yawTarget = Math_Vec3f_Yaw(&origin, &sAimTarget->focus.pos);
    s16 pitchTarget = Math_Vec3f_Pitch(&origin, &sAimTarget->focus.pos);

    // Keep the aim inside the range first person aiming allows
    s16 relYaw = CLAMP((s16)(yawTarget - player->actor.shape.rot.y), -0x4AAA, 0x4AAA);
    yawTarget = relYaw + player->actor.shape.rot.y;
    pitchTarget = CLAMP(pitchTarget, -0x36B0, 0x36B0);

    Math_SmoothStepToS(&player->actor.focus.rot.y, yawTarget, 2, 0x2000, 0);
    Math_SmoothStepToS(&player->actor.focus.rot.x, pitchTarget, 2, 0x2000, 0);
}

extern "C" int32_t RemakeControls_IsAutoJumpDisabled(void) {
    return CVAR_REMAKE != 0;
}

extern "C" int32_t RemakeControls_ConsumeJump(void) {
    if (!CVAR_REMAKE) {
        return false;
    }
    bool jump = sJumpThisFrame;
    sJumpThisFrame = false;
    return jump;
}

void RemakeQuickSelectWindow::Draw() {
    if (!sQS.open || (gPlayState == nullptr) || sQS.entries.empty()) {
        return;
    }

    auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    if (gui == nullptr) {
        return;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);

    const int count = (int)sQS.entries.size();
    const float baseIcon = 48.0f;
    const float basePad = 12.0f;
    float scale = viewport->Size.y / 480.0f;
    float maxScale = (viewport->Size.x * 0.92f) / (count * (baseIcon + basePad) + basePad);
    if (scale > maxScale) {
        scale = maxScale;
    }
    const float icon = baseIcon * scale;
    const float pad = basePad * scale;

    // Dim the game a bit, like in BotW
    drawList->AddRectFilled(viewport->Pos,
                            ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
                            IM_COL32(0, 0, 0, 110));

    const float barWidth = count * (icon + pad) + pad;
    const float barHeight = icon + pad * 2.0f;
    const float barX = viewport->Pos.x + (viewport->Size.x - barWidth) * 0.5f;
    const float barY = viewport->Pos.y + viewport->Size.y * 0.60f;

    drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barWidth, barY + barHeight), IM_COL32(12, 14, 24, 215),
                            10.0f * scale);
    drawList->AddRect(ImVec2(barX, barY), ImVec2(barX + barWidth, barY + barHeight), IM_COL32(200, 190, 140, 120),
                      10.0f * scale, 0, 1.5f * scale);

    ImFont* font = ImGui::GetFont();

    for (int i = 0; i < count; i++) {
        const QuickSelectEntry& entry = sQS.entries[i];
        float x = barX + pad + i * (icon + pad);
        float y = barY + pad;
        bool selected = (i == sQS.cursor);

        if (selected) {
            float grow = 5.0f * scale;
            drawList->AddRectFilled(ImVec2(x - grow, y - grow), ImVec2(x + icon + grow, y + icon + grow),
                                    IM_COL32(255, 230, 120, 70), 6.0f * scale);
            drawList->AddRect(ImVec2(x - grow, y - grow), ImVec2(x + icon + grow, y + icon + grow),
                              IM_COL32(255, 225, 100, 255), 6.0f * scale, 0, 2.5f * scale);
        }

        if (entry.item < 131) {
            ImTextureID tex = gui->GetTextureByName((const char*)gItemIcons[entry.item]);
            if (tex) {
                ImU32 tint = selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 220);
                drawList->AddImage(tex, ImVec2(x, y), ImVec2(x + icon, y + icon), ImVec2(0, 0), ImVec2(1, 1), tint);
            }
        }

        if (ItemHasAmmo(entry.item)) {
            char ammo[8];
            snprintf(ammo, sizeof(ammo), "%d", (int)AMMO(entry.item));
            float fontSize = 13.0f * scale;
            ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, ammo);
            ImVec2 pos(x + icon - size.x, y + icon - size.y);
            drawList->AddText(font, fontSize, ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0, 0, 0, 255), ammo);
            drawList->AddText(font, fontSize, pos, IM_COL32(255, 255, 255, 255), ammo);
        }
    }

    // Name of the highlighted item
    const char* name = GetItemNameDE(sQS.entries[sQS.cursor].item);
    if (name[0] != '\0') {
        float fontSize = 22.0f * scale;
        ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, name);
        ImVec2 pos(viewport->Pos.x + (viewport->Size.x - size.x) * 0.5f, barY + barHeight + 10.0f * scale);
        drawList->AddText(font, fontSize, ImVec2(pos.x + 2, pos.y + 2), IM_COL32(0, 0, 0, 200), name);
        drawList->AddText(font, fontSize, pos, IM_COL32(255, 255, 255, 255), name);
    }

    // Hint
    const char* hint = "Loslassen: auf ZR legen    B: Abbrechen";
    float hintSize = 14.0f * scale;
    ImVec2 hintDim = font->CalcTextSizeA(hintSize, FLT_MAX, 0.0f, hint);
    drawList->AddText(font, hintSize,
                      ImVec2(viewport->Pos.x + (viewport->Size.x - hintDim.x) * 0.5f, barY - hintDim.y - 8.0f * scale),
                      IM_COL32(230, 230, 230, 230), hint);
}

void RegisterRemakeControls() {
    COND_HOOK(OnPassPlayerInputs, CVAR_REMAKE, [](Input* input) {
        sJumpThisFrame = sJumpQueued;
        sJumpQueued = false;
        EnforceDpadLayout();
    });

    COND_VB_SHOULD(VB_SPEED_MODIFIER_WALK, CVAR_REMAKE, {
        f32* speedTarget = va_arg(args, f32*);
        Player* player = GET_PLAYER(gPlayState);
        if (sSprinting && (player != nullptr) &&
            ((player->transformation == PLAYER_FORM_HUMAN) || (player->transformation == PLAYER_FORM_FIERCE_DEITY))) {
            *speedTarget *= SPRINT_SPEED_MULTIPLIER;
        }
    });

    COND_HOOK(OnPlayDestroy, CVAR_REMAKE, []() {
        sQS.open = false;
        sJumpQueued = false;
        sJumpThisFrame = false;
    });
}

static RegisterShipInitFunc initFunc(RegisterRemakeControls, { CVAR_REMAKE_CONTROLS });
