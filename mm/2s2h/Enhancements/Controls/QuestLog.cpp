/**
 * Quest Log (Remake Controls)
 *
 * Opened with X in the pause menu. Shows the main quest per region: what is done and the next step with a hint.
 * Progress is read straight from the save (quest items, inventory), so it works on existing saves.
 */
#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>
#include <imgui.h>

extern "C" {
#include "variables.h"
#include "functions.h"
}

namespace {

enum class Check { Quest, Item };

struct QuestStep {
    Check type;
    int32_t id; // QuestItem or ItemId
    const char* title;
    const char* hint;
};

struct QuestRegion {
    const char* name;
    std::vector<QuestStep> steps;
};

bool StepDone(const QuestStep& step) {
    if (step.type == Check::Quest) {
        return CHECK_QUEST_ITEM(step.id) != 0;
    }
    return INV_CONTENT(step.id) == step.id;
}

// ponytail: hand-written main quest only, flags for side quests (Bomber's Notebook) if wanted later
const std::vector<QuestRegion> sRegions = {
    { "Unruh-Stadt",
      {
          { Check::Item, ITEM_OCARINA_OF_TIME, "Okarina zur\xC3\xBC""ckholen",
            "Als Deku zur Gro\xC3\x9F""en Fee in Nord-Unruh-Stadt: die verirrte Fee vom Waschplatz zu ihr bringen, dann hast du "
            "Magie. In der Nacht vom 3. Tag um Mitternacht auf den Uhrturm und Horror Kid mit Seifenblasen treffen." },
          { Check::Quest, QUEST_SONG_HEALING, "Lied der Befreiung lernen",
            "Nach der R\xC3\xBC""ckkehr in den 1. Tag spricht der Maskenh\xC3\xA4""ndler im Uhrturm dich an. Spiel ihm vor." },
      } },
    { "S\xC3\xBC""dlicher Sumpf (Waldtempel)",
      {
          { Check::Item, ITEM_MASK_DEKU, "Deku-Maske", "Bekommst du mit dem Lied der Befreiung." },
          { Check::Quest, QUEST_SONG_SONATA, "Sonate des Erwachens lernen",
            "S\xC3\xBC""dlicher Sumpf: Im Hexenladen im Wald Koume finden, rotes Elixier von Kotake holen, Koume heilen, dann "
            "mit dem Boot zum Deku-Palast. Als Deku durch die G\xC3\xA4""rten zum gefangenen Affen, er bringt dir die Sonate "
            "bei." },
          { Check::Item, ITEM_BOW, "Bogen (Waldtempel)",
            "Auf der Deku-Blume vor dem Waldtempel die Sonate spielen, im Tempel gibt es den Bogen." },
          { Check::Quest, QUEST_REMAINS_ODOLWA, "Odolwa besiegen", "Boss vom Waldtempel. Danach die Prinzessin "
                                                                     "in einer Flasche zum Deku-K\xC3\xB6""nig bringen." },
      } },
    { "Berge (Schneegipfel)",
      {
          { Check::Item, ITEM_LENS_OF_TRUTH, "Auge der Wahrheit",
            "Weg nach Norden: Eiszapfen mit dem Bogen abschie\xC3\x9F""en. Im Goronendorf \xC3\xBC""ber die unsichtbaren Plattformen "
            "zur H\xC3\xB6""hle auf dem einsamen Gipfel, dort liegt das Auge der Wahrheit." },
          { Check::Item, ITEM_MASK_GORON, "Goronen-Maske",
            "Mit dem Auge der Wahrheit dem Geist von Darmani folgen (vor dem Goronendorf), bis zu seinem Grab. "
            "Dort das Lied der Befreiung spielen." },
          { Check::Quest, QUEST_SONG_LULLABY, "Goronen-Wiegenlied",
            "Den eingefrorenen Goronen-\xC3\x84""ltesten auf dem Weg zum Schneegipfel auftauen (hei\xC3\x9F""es Quellwasser), "
            "Wiegenlied-Anfang lernen. Im Goronen-Schrein dem weinenden Baby vorspielen." },
          { Check::Quest, QUEST_REMAINS_GOHT, "Goht besiegen",
            "Den schlafenden Riesen-Goronen vor dem Schneegipfel mit dem Wiegenlied einschl\xC3\xA4""fern, Tempel betreten." },
      } },
    { "Gro\xC3\x9F""er Ozean (Ozeantempel)",
      {
          { Check::Quest, QUEST_SONG_EPONA, "Eponas Lied",
            "Romani-Ranch: Der Felsen auf dem Weg wird mit einem Pulverfass gesprengt (Pulverfass-Test bei den "
            "Goronen). Am 1. Tag Romanis Ballon-Spiel mit dem Bogen schaffen." },
          { Check::Item, ITEM_MASK_ZORA, "Zora-Maske",
            "Mit Epona \xC3\xBC""ber den Zaun zum Gro\xC3\x9F""en Ozean. Den treibenden Zora Mikau an Land schieben und ihm das "
            "Lied der Befreiung vorspielen." },
          { Check::Item, ITEM_HOOKSHOT, "Fanghaken",
            "Piratenfestung: als Zora reinschwimmen, im Inneren gibt es den Fanghaken." },
          { Check::Quest, QUEST_SONG_BOSSA_NOVA, "Bossa Nova der Zoras",
            "7 Zora-Eier sammeln (4 in der Piratenfestung, 3 beim Pinnacle-Felsen), ins Aquarium vom "
            "Meeresforschungslabor bringen. Danach f\xC3\xBC""r Lulu in der Zora-Halle spielen." },
          { Check::Quest, QUEST_REMAINS_GYORG, "Gyorg besiegen",
            "Die Schildkr\xC3\xB6""te bringt dich zum Ozeantempel. Die Eispfeile gibt es im Tempel." },
      } },
    { "Ikana",
      {
          { Check::Item, ITEM_MASK_GARO, "Garo-Maske",
            "Gorman-Rennbahn (Milchstra\xC3\x9F""e): das Pferderennen gegen die Gorman-Br\xC3\xBC""der mit Epona gewinnen." },
          { Check::Item, ITEM_MASK_GIBDO, "Gibdo-Maske",
            "Ikana-Schlucht (mit Garo-Maske rein): Pamelas Vater im Musikhaus mit dem Lied der Befreiung heilen." },
          { Check::Quest, QUEST_SONG_ELEGY, "Elegie der Leere",
            "Unter dem Brunnen (Gibdo-Maske) gibt es den Spiegelschild. Damit ins Ikana-Schloss und K\xC3\xB6""nig Igos "
            "besiegen." },
          { Check::Item, ITEM_ARROW_LIGHT, "Lichtpfeile (Felsenturm)",
            "Mit der Elegie Statuen von dir erschaffen, um den Felsenturm hochzukommen. Im Tempel liegen die "
            "Lichtpfeile." },
          { Check::Quest, QUEST_REMAINS_TWINMOLD, "Twinmold besiegen", "Boss vom umgedrehten Felsenturm-Tempel." },
      } },
    { "Finale",
      {
          { Check::Quest, QUEST_SONG_OATH, "Hymne des Himmels", "Bringen dir die Riesen nach dem ersten Boss bei." },
      } },
};

const char* FinalHint() {
    return "Alle vier Riesen frei? Dann in der Nacht vom 3. Tag um Mitternacht auf den Uhrturm und die Hymne des "
           "Himmels spielen.";
}

} // namespace

void QuestLog_Draw() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
    ImFont* font = ImGui::GetFont();
    const float scale = viewport->Size.y / 480.0f;
    const float pad = 12.0f * scale;
    const float titleSize = 18.0f * scale;
    const float headSize = 14.0f * scale;
    const float textSize = 11.5f * scale;
    const float panelW = std::min(viewport->Size.x * 0.9f, 560.0f * scale);
    const float wrapW = panelW - pad * 3.0f;

    // The recommended next region is the first one with something left to do
    int nextRegion = -1;
    for (size_t r = 0; r < sRegions.size() && nextRegion < 0; r++) {
        for (const QuestStep& step : sRegions[r].steps) {
            if (!StepDone(step)) {
                nextRegion = (int)r;
                break;
            }
        }
    }

    drawList->AddRectFilled(viewport->Pos,
                            ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
                            IM_COL32(0, 0, 0, 150));

    // Lay out first to know the panel height, then draw
    struct Line {
        float size;
        ImU32 color;
        float indent;
        const char* text;
        std::string owned;
    };
    std::vector<Line> lines;
    lines.push_back({ titleSize, IM_COL32(255, 225, 100, 255), 0, "Quest-Log", {} });

    for (size_t r = 0; r < sRegions.size(); r++) {
        const QuestRegion& region = sRegions[r];
        int done = 0;
        const QuestStep* next = nullptr;
        for (const QuestStep& step : region.steps) {
            if (StepDone(step)) {
                done++;
            } else if (next == nullptr) {
                next = &step;
            }
        }
        bool isNext = ((int)r == nextRegion);
        std::string head = std::string(region.name) + "   " + std::to_string(done) + "/" +
                           std::to_string(region.steps.size()) + (next == nullptr ? "   erledigt" : "") +
                           (isNext ? "   << als N\xC3\xA4""chstes" : "");
        lines.push_back({ headSize,
                          next == nullptr ? IM_COL32(120, 200, 120, 255)
                                          : (isNext ? IM_COL32(255, 225, 100, 255) : IM_COL32(230, 230, 230, 255)),
                          0, nullptr, head });
        if (next != nullptr && (isNext || r + 1 == sRegions.size())) {
            // Active region: every step with its state, the current one with its hint
            for (const QuestStep& step : region.steps) {
                if (&step == next) {
                    lines.push_back({ textSize, IM_COL32(255, 225, 100, 255), pad, nullptr,
                                      std::string("> ") + step.title + "   (aktiv)" });
                    lines.push_back({ textSize, IM_COL32(190, 190, 200, 255), pad * 2.0f, step.hint, {} });
                } else if (StepDone(step)) {
                    lines.push_back(
                        { textSize, IM_COL32(120, 200, 120, 255), pad, nullptr, std::string("[x] ") + step.title });
                } else {
                    lines.push_back(
                        { textSize, IM_COL32(150, 150, 160, 255), pad, nullptr, std::string("[  ] ") + step.title });
                }
            }
        } else if (next != nullptr) {
            lines.push_back({ textSize, IM_COL32(170, 170, 180, 255), pad, nullptr, std::string("> ") + next->title });
        }
    }
    if (nextRegion < 0 || nextRegion + 1 == (int)sRegions.size()) {
        lines.push_back({ textSize, IM_COL32(255, 225, 100, 255), 0, FinalHint(), {} });
    }
    lines.push_back({ textSize * 0.9f, IM_COL32(150, 150, 160, 255), 0, "X / B: schlie\xC3\x9F""en", {} });

    float h = pad;
    for (const Line& line : lines) {
        const char* text = line.text ? line.text : line.owned.c_str();
        h += font->CalcTextSizeA(line.size, FLT_MAX, wrapW - line.indent, text).y + 4.0f * scale;
    }
    h += pad;

    const float x0 = viewport->Pos.x + (viewport->Size.x - panelW) * 0.5f;
    const float y0 = viewport->Pos.y + std::max(10.0f * scale, (viewport->Size.y - h) * 0.5f);
    drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + panelW, y0 + h), IM_COL32(12, 14, 24, 235), 10.0f * scale);
    drawList->AddRect(ImVec2(x0, y0), ImVec2(x0 + panelW, y0 + h), IM_COL32(200, 190, 140, 140), 10.0f * scale, 0,
                      1.5f * scale);

    float y = y0 + pad;
    for (const Line& line : lines) {
        const char* text = line.text ? line.text : line.owned.c_str();
        drawList->AddText(font, line.size, ImVec2(x0 + pad + line.indent, y), line.color, text, nullptr,
                          wrapW - line.indent);
        y += font->CalcTextSizeA(line.size, FLT_MAX, wrapW - line.indent, text).y + 4.0f * scale;
    }
}
