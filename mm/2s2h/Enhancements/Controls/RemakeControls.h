#ifndef REMAKE_CONTROLS_H
#define REMAKE_CONTROLS_H

#include <stdint.h>

#define CVAR_REMAKE_CONTROLS "gEnhancements.Controls.RemakeControls"

#ifdef __cplusplus
#include <ship/window/gui/GuiWindow.h>

// Draws the horizontal "Quick Select" item bar (Ocarina of Time Switch 2 / BotW style)
class RemakeQuickSelectWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override {};
    void DrawElement() override {};
    void Draw() override;
    void UpdateElement() override {};
};

extern "C" {
#endif

/**
 * Translates the physical (remake layout) buttons into the N64 buttons the game expects,
 * depending on the current context (gameplay, menu/text, ocarina). Called from padmgr for controller 1.
 */
uint32_t RemakeControls_TranslateButtons(uint32_t buttons, int8_t* stickX, int8_t* stickY, int8_t* rStickX,
                                         int8_t* rStickY);

/** Returns true (once) if the jump button was pressed for the current player update. */
int32_t RemakeControls_ConsumeJump(void);

/**
 * While aiming in first person with a weapon (bow, hookshot, ...): holding ZL locks the aim onto the closest
 * targetable actor in front of Link. play / player are PlayState* / Player*.
 */
void RemakeControls_AimAssist(void* play, void* player, int32_t zHeld);

/** Returns true if the vanilla automatic ledge jump should not happen (jumping is done manually with X). */
int32_t RemakeControls_IsAutoJumpDisabled(void);

/** Returns true if the bow and hookshot are aimed over the shoulder (third person) instead of in first person. */
int32_t RemakeControls_IsThirdPersonAim(void);

#ifdef __cplusplus
}
#endif

#endif // REMAKE_CONTROLS_H
