#include "pch.h"
#include "AirOrientationTrainer.h"

void AirOrientationTrainer::RenderSettings() {
    ImGui::TextUnformatted("Align the front of your car with the yellow arrow, wheels toward orange.\
        \nThe target 2D lines are a little confusing at first but make more sense on repetition, ball cam may help.");

    CVarWrapper enableCvar = cvarManager->getCvar("AOT_enabled");
    if (!enableCvar) { return; }
    bool enabled = enableCvar.getBoolValue();
    if (ImGui::Checkbox("Enable plugin", &enabled)) {
        enableCvar.setValue(enabled);
        if (enabled) {
            gameWrapper->Execute([this](GameWrapper* gw) {
                cvarManager->executeCommand("airOrientationTrainer");
                });
        }
        else {
            gameWrapper->Execute([this](GameWrapper* gw) {
                cvarManager->executeCommand("AOT_disable");
                });
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Toggle Aerial Orientation Trainer");
    }
    
    ImGui::Separator();
    ImGui::TextUnformatted("");
    
    ImGui::SetNextItemWidth(150);
    CVarWrapper trainModeCVar = cvarManager->getCvar("AOT_training_mode");
    if (!trainModeCVar) { return; }
    static int item_current = 0;
    if (ImGui::Combo("Training Mode", &item_current, "Default Pack\0Endless Random")) {
        trainModeCVar.setValue(item_current);
        if (enabled) {
            gameWrapper->Execute([this](GameWrapper* gw) {
                cvarManager->executeCommand("airOrientationTrainer");
                });
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Select training mode");
    }
    
    ImGui::SetNextItemWidth(300);
    CVarWrapper difficultyCVar = cvarManager->getCvar("AOT_difficulty");
    if (!difficultyCVar) { return; }
    int difficulty = difficultyCVar.getIntValue();
    if (ImGui::SliderInt("Difficulty: % error margin, lower is harder", &difficulty, 5, 25)) {
        difficultyCVar.setValue(difficulty);
    }
    if (ImGui::IsItemHovered()) {
        std::string hoverText = "Error margin is " + std::to_string(difficulty) + "%%";
        ImGui::SetTooltip(hoverText.c_str());
    }

    ImGui::TextUnformatted("\nThe following options decide when rounds are replayed");
    ImGui::SetNextItemWidth(200);
    CVarWrapper roundTimerLimitCVar = cvarManager->getCvar("AOT_round_time_limit");
    if (!roundTimerLimitCVar) { return; }
    int roundTimerLimit = roundTimerLimitCVar.getIntValue();
    if (ImGui::SliderInt("Replay round untill it's done faster than this", &roundTimerLimit, 1, 8)) {
        roundTimerLimitCVar.setValue(roundTimerLimit);
    }
    if (ImGui::IsItemHovered()) {
        std::string hoverText = "Round limit is " + std::to_string(roundTimerLimit) + "s";
        ImGui::SetTooltip(hoverText.c_str());
    }
    CVarWrapper lockTrainingCvar = cvarManager->getCvar("AOT_lock_training");
    if (!lockTrainingCvar) { return; }
    bool lockTraining = lockTrainingCvar.getBoolValue();
    if (ImGui::Checkbox("Endlessly replay current round", &lockTraining)) {
        lockTrainingCvar.setValue(lockTraining);
    }
    
    ImGui::Separator();

    ImGui::TextUnformatted("To reset high scores, open plugin manager, toggle this plugin off and on again");
}
