#include "MistwatchAudio.h"
#include "MistwatchGraphicsSettings.h"
#include "MistwatchSaveFile.h"
#include "MistwatchSettingsFile.h"

#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
void Require(bool condition, const char* detail) {
    if (!condition) throw std::runtime_error(detail);
}
}

int main() {
    using namespace Mistwatch;
    namespace fs = std::filesystem;
    try {
        const auto root = fs::current_path() / ("profile-check-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
        Require(fs::create_directory(root), "isolated profile fixture already exists");
        const auto local = root / "recovered";
        const auto primary = local / "Aevrynd" / "Mistwatch";
        const auto alternate = local / "MistwatchRiverbankPlaytest" / "Aevrynd" / "Mistwatch";
        const auto filename = "save-forest-grove-v13-fixture.txt";
        fs::create_directories(alternate / "valley-sessions" / "startup-only");
        Require(bool(std::ofstream(alternate / "save-forest-grove-v13-fixture.txt.1.tmp") << "incomplete write"),
                "could not prepare leftover temporary save");
        SaveGame primarySave;
        primarySave.session.health = 83;
        fs::create_directories(primary);
        const auto temporaryPrefix = std::string(filename) + "." + std::to_string(GetCurrentProcessId()) + ".";
        const auto firstTemporary = primary / (temporaryPrefix + "1.tmp");
        const auto secondTemporary = primary / (temporaryPrefix + "2.tmp");
        for (const auto& stale : {firstTemporary, secondTemporary})
            Require(bool(std::ofstream(stale) << "interrupted prior save"), "prepare reused PID temporary file");
        Require(WriteSaveFile(primary / filename, primarySave) == SaveFileResult::Saved,
                "REUSED_PID_TEMPORARY_BLOCKED_CHECKPOINT");
        for (const auto& stale : {firstTemporary, secondTemporary}) {
            std::string contents;
            std::ifstream pending(stale);
            Require(bool(std::getline(pending, contents)) && contents == "interrupted prior save",
                    "checkpoint changed a temporary file it did not own");
        }
        std::cout << "RIVERBANK_SAVE_TEMP_COLLISION_PASS collisions=2 preserved=2\n";

        const auto deep = root / "deep-checkpoint";
        fs::create_directories(deep);
        constexpr std::size_t checkpointLength = 253;
        Require(deep.native().size() + 10 < checkpointLength, "long checkpoint fixture root is too deep");
        const auto nameLength = checkpointLength - deep.native().size() - 1;
        const auto longCheckpoint = deep / (L"save-" + std::wstring(nameLength - 9, L'x') + L".txt");
        Require(longCheckpoint.native().size() == checkpointLength, "long checkpoint fixture length differs");
        const auto shortCollision = deep / (L".mw-save." + std::to_wstring(GetCurrentProcessId()) + L".4.tmp");
        Require(bool(std::ofstream(shortCollision) << "previous temporary file"), "prepare short temporary collision");
        Require(WriteSaveFile(longCheckpoint, primarySave) == SaveFileResult::Saved, "NEAR_MAX_PATH_CHECKPOINT_WRITE_FAILED");
        SaveGame longRestored;
        Require(ReadSaveFile(longCheckpoint, longRestored) && longRestored.Serialize() == primarySave.Serialize(),
                "near MAX_PATH checkpoint did not roundtrip");
        longRestored.session.health = 59;
        Require(WriteSaveFile(longCheckpoint, longRestored) == SaveFileResult::Saved, "near MAX_PATH checkpoint replace failed");
        SaveGame replacedLongCheckpoint;
        Require(ReadSaveFile(longCheckpoint, replacedLongCheckpoint) && replacedLongCheckpoint.Serialize() == longRestored.Serialize(),
                "near MAX_PATH checkpoint replacement did not roundtrip");
        std::string collisionContents;
        Require(bool(std::getline(std::ifstream(shortCollision), collisionContents)) && collisionContents == "previous temporary file",
                "short temporary fallback overwrote an unowned file");
        for (const auto& entry : fs::directory_iterator(deep))
            Require(entry.path() == longCheckpoint || entry.path() == shortCollision, "unexpected temporary checkpoint file");
        std::cout << "RIVERBANK_SAVE_LONG_TEMP_PASS target_chars=253 collision_preserved=1\n";

        Require(SelectRiverbankSaveDirectory(local) == primary,
                "empty fallback hides recovered primary checkpoint");
        SaveGame restored;
        Require(ReadSaveFile(primary / filename, restored) && restored.Serialize() == primarySave.Serialize(),
                "selecting the recovered folder changed the real checkpoint");

        SaveGame alternateSave;
        alternateSave.session.health = 64;
        Require(WriteSaveFile(alternate / filename, alternateSave) == SaveFileResult::Saved, "fallback fixture save failed");
        Require(SelectRiverbankSaveDirectory(local) == alternate, "a used fallback lost priority after primary recovered");
        Require(ReadSaveFile(alternate / filename, restored) && restored.Serialize() == alternateSave.Serialize(),
                "used fallback changed while selecting the directory");
        Require(ReadSaveFile(primary / filename, restored) && restored.Serialize() == primarySave.Serialize(),
                "used fallback overwrote primary progress");

        const std::wstring exportName = L"last-playtest-summary.txt";
        constexpr std::size_t exportPathLength = 253;
        Require(root.native().size() + exportName.size() + 2 < exportPathLength, "long export fixture root is too deep");
        const auto exportFolder = root / std::wstring(exportPathLength - root.native().size() - exportName.size() - 2, L'p');
        Require(fs::create_directory(exportFolder), "could not prepare long export folder");
        const auto exportFile = exportFolder / exportName;
        Require(exportFile.native().size() == exportPathLength, "long export fixture length differs");
        const auto exportCollision = exportFolder / (L".mw-profile." + std::to_wstring(GetCurrentProcessId()) + L".0.tmp");
        Require(bool(std::ofstream(exportCollision) << "interrupted export"), "could not prepare stale export temporary");
        const std::string firstSummary = "MISTWATCH_PLAYTEST 1\nactive_s=600\nbridge_s=210\nencounter_s=70\npower_s=150\nsignal_s=170\n";
        const std::string nextSummary = "MISTWATCH_PLAYTEST 1\nactive_s=601\nbridge_s=211\nencounter_s=70\npower_s=150\nsignal_s=170\n";
        const auto readExport = [](const fs::path& path) {
            std::ifstream input(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
        };
        Require(WriteSettingsFile(exportFile, firstSummary), "NEAR_MAX_PATH_PLAYTEST_EXPORT_WRITE_FAILED");
        Require(readExport(exportFile) == firstSummary, "near MAX_PATH export did not roundtrip");
        Require(WriteSettingsFile(exportFile, nextSummary) && readExport(exportFile) == nextSummary,
                "near MAX_PATH export replacement did not roundtrip");
        HANDLE lockedExport = CreateFileW(exportFile.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                                         FILE_ATTRIBUTE_NORMAL, nullptr);
        Require(lockedExport != INVALID_HANDLE_VALUE, "could not lock existing playtest export");
        const bool replacedLockedExport = WriteSettingsFile(exportFile, firstSummary);
        const bool closedLockedExport = CloseHandle(lockedExport) != 0;
        Require(closedLockedExport && !replacedLockedExport && readExport(exportFile) == nextSummary,
                "failed export changed the prior report");
        Require(readExport(exportCollision) == "interrupted export", "export overwrote an unowned temporary");
        for (const auto& entry : fs::directory_iterator(exportFolder))
            Require(entry.path() == exportFile || entry.path() == exportCollision, "unexpected temporary playtest export");
        std::cout << "RIVERBANK_PLAYTEST_LONG_EXPORT_PASS target_chars=253 collision_preserved=1 failed_replace_preserved=1\n";

        const auto settingsLocal = root / "settings";
        const auto settingsAlternate = settingsLocal / "MistwatchRiverbankPlaytest" / "Aevrynd" / "Mistwatch";
        fs::create_directories(settingsAlternate);
        Require(bool(std::ofstream(settingsAlternate / "camera-settings.txt") << "player setting"),
                "could not prepare existing player settings");
        Require(SelectRiverbankSaveDirectory(settingsLocal) == settingsAlternate,
                "existing fallback player settings lost priority");

        const auto graphicsLocal = root / "graphics";
        const auto graphicsAlternate = graphicsLocal / "MistwatchRiverbankPlaytest" / "Aevrynd" / "Mistwatch";
        const auto graphics = graphicsAlternate / "graphics-settings.txt";
        for (bool sun : {false, true}) {
            for (bool ambient : {false, true}) {
                GraphicsSettings selected;
                Require(selected.Toggle(!sun, !ambient) == (!sun || !ambient), "independent graphics switch failed");
                Require(WriteSettingsFile(graphics, selected.Serialize()), "write graphics profile");
                GraphicsSettings restoredGraphics{!sun, !ambient};
                Require(ReadGraphicsSettingsFile(graphics, restoredGraphics) &&
                        restoredGraphics.sunShadows == sun && restoredGraphics.ambientShading == ambient,
                        "graphics settings changed across reload");
            }
        }
        const auto graphicsPrimary = graphicsLocal / "Aevrynd" / "Mistwatch";
        auto graphicsWorldSave = primarySave;
        constexpr std::uint64_t world = 101;
        graphicsWorldSave.worldIdentity = world;
        graphicsWorldSave.motion.emplace();
        graphicsWorldSave.motion->controller.feet = graphicsWorldSave.session.feet;
        graphicsWorldSave.motion->controller.grounded = true;
        Require(WriteSaveFile(graphicsPrimary / filename, graphicsWorldSave) == SaveFileResult::Saved,
                "write recovered primary save beside graphics-only fallback");
        const GraphicsSettings selectedPreference{false, true};
        Require(WriteSettingsFile(graphics, selectedPreference.Serialize()), "write nondefault fallback preference");
        const auto selectedGraphics = SelectRiverbankSaveDirectory(graphicsLocal);
        Require(selectedGraphics == graphicsAlternate, "GRAPHICS_ONLY_FALLBACK_PREFERENCE_LOST");
        GraphicsSettings persisted;
        Require(ReadGraphicsSettingsFile(selectedGraphics / "graphics-settings.txt", persisted) &&
                persisted.Serialize() == selectedPreference.Serialize(), "retained graphics preference cannot be read");
        const auto recovered = RecoveredRiverbankCheckpoint(graphicsLocal, selectedGraphics / filename, world);
        Require(recovered == graphicsPrimary / filename &&
                ReadSaveFile(*recovered, restored) && restored.Serialize() == graphicsWorldSave.Serialize() &&
                !fs::exists(graphicsAlternate / filename), "graphics-only fallback hid or changed the recovered save");
        Require(!RecoveredRiverbankCheckpoint(graphicsLocal, selectedGraphics / filename, world + 1),
                "a different world checkpoint took priority");
        auto preferredSave = graphicsWorldSave;
        preferredSave.session.health = 52;
        Require(WriteSaveFile(graphicsAlternate / filename, preferredSave) == SaveFileResult::Saved &&
                !RecoveredRiverbankCheckpoint(graphicsLocal, selectedGraphics / filename, world) &&
                ReadSaveFile(graphicsAlternate / filename, restored) && restored.Serialize() == preferredSave.Serialize(),
                "an existing fallback checkpoint lost priority");
        GraphicsSettings intact{false, true};
        Require(!ReadGraphicsSettingsFile(graphicsLocal / "missing.txt", intact), "accepted missing graphics file");
        for (const auto* invalid : {"MISTWATCH_GRAPHICS 2 0 0","MISTWATCH_GRAPHICS 1 0 2",
                                   "MISTWATCH_GRAPHICS 1 0 1 trailing data"}) {
            Require(WriteSettingsFile(graphics, invalid), "write malformed graphics fixture");
            Require(!ReadGraphicsSettingsFile(graphics, intact) && !intact.sunShadows && intact.ambientShading,
                    "malformed graphics file changed the current settings");
        }

        const auto audio = settingsAlternate / "audio-settings.txt";
        const auto otherWrite = settingsAlternate / "audio-settings.txt.tmp";
        Require(bool(std::ofstream(otherWrite) << "pending audio preference"), "prepare independent write");
        AudioOptions selected;
        selected.volume = .625f;
        Require(WriteSettingsFile(audio, selected.Serialize()), "save the selected volume");
        std::string line;
        {
            std::ifstream pending(otherWrite);
            Require(bool(std::getline(pending, line)) && line == "pending audio preference",
                    "audio settings consumed another writer's temporary file");
        }

        HANDLE busy = CreateFileW(audio.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        Require(busy != INVALID_HANDLE_VALUE, "lock the existing volume");
        const bool replaced = WriteSettingsFile(audio, "MISTWATCH_AUDIO 1 0.25 1");
        const bool closed = CloseHandle(busy) != 0;
        Require(closed && !replaced, "a locked preference must reject replacement");
        {
            std::ifstream current(audio);
            AudioOptions restoredAudio;
            Require(bool(std::getline(current, line)) && restoredAudio.Restore(line) &&
                    restoredAudio.Serialize() == selected.Serialize(), "failed write changed the existing volume");
        }
        for (const auto& entry : fs::directory_iterator(settingsAlternate))
            Require(!entry.path().filename().wstring().starts_with(L"audio-settings.txt.tmp-"),
                    "failed write left a transaction file");

        const auto offline = root / "offline";
        fs::create_directory(offline);
        Require(bool(std::ofstream(offline / "Aevrynd") << "primary is inaccessible as a directory"),
                "could not prepare offline directory fixture");
        const auto expected = offline / "MistwatchRiverbankPlaytest" / "Aevrynd" / "Mistwatch";
        Require(SelectRiverbankSaveDirectory(offline) == expected && fs::is_directory(expected),
                "unavailable primary stopped a first fallback save");
        Require(WriteSaveFile(expected / filename, alternateSave) == SaveFileResult::Saved &&
                ReadSaveFile(expected / filename, restored) && restored.Serialize() == alternateSave.Serialize(),
                "fallback did not support a real save and readback");
        Require(SelectRiverbankSaveDirectory("relative-profile").empty(), "relative profile escaped the safety check");
        std::cout << "RIVERBANK_PROFILE_SETTINGS_TRANSACTION_PASS retained_peer=1 preserved_locked=1\n";
        std::cout << "RIVERBANK_GRAPHICS_SETTINGS_PASS combinations=4 malformed=3 primary_preserved=1 graphics_fallback=1\n";
        std::cout << "RIVERBANK_PROFILE_RECOVERY_PASS recovered=1 used_fallback=1 settings=1 offline=1 preserved=1\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
