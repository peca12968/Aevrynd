#include "RiverbankSettingsMenu.h"
#include "MistwatchAudio.h"
#include "MistwatchCameraSettingsFile.h"
#include "MistwatchGraphicsSettings.h"
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    using namespace Mistwatch;
    if (argc != 2) return 1;
    RiverbankSettingsMenu menu;
    if (menu.Update(false, true, false, false).consumesInput) return 2;
    if (menu.Update(true, true, false, true).consumesInput) return 3;
    const auto opening = menu.Update(true, true, false, false);
    if (!opening.consumesInput || opening.editable || !menu.Open()) return 4;
    const auto active = menu.Update(true, false, false, false);
    if (!active.consumesInput || !active.editable) return 5;
    const auto unfocused = menu.Update(false, true, true, false);
    if (!unfocused.consumesInput || unfocused.editable || !menu.Open()) return 6;
    const auto closing = menu.Update(true, false, true, false);
    if (!closing.consumesInput || closing.editable || menu.Open()) return 7;
    if (menu.Update(true, false, false, false).consumesInput) return 8;

    AudioOptions volume;
    CameraSettings camera;
    GraphicsSettings graphics;
    if (!volume.Adjust(true, false, true, true) || !camera.Adjust(true, false, true, true, false)
        || !graphics.Toggle(true, true)) return 9;
    const std::filesystem::path data = std::filesystem::absolute(argv[1]);
    if (!WriteSettingsFile(data / "audio.txt", volume.Serialize())
        || !WriteCameraSettingsFile(data / "camera.txt", camera)
        || !WriteSettingsFile(data / "graphics.txt", graphics.Serialize())) return 10;
    std::string audioText;
    std::ifstream audioFile(data / "audio.txt");
    std::getline(audioFile, audioText);
    AudioOptions restoredVolume;
    CameraSettings restoredCamera;
    GraphicsSettings restoredGraphics;
    if (!restoredVolume.Restore(audioText) || restoredVolume.Gain() != 0 ||
        std::abs(restoredVolume.volume - .55f) > .001f ||
        !ReadCameraSettingsFile(data / "camera.txt", restoredCamera) ||
        restoredCamera.sensitivityPercent != 125 || !restoredCamera.invertVertical ||
        !ReadGraphicsSettingsFile(data / "graphics.txt", restoredGraphics) ||
        restoredGraphics.sunShadows || restoredGraphics.ambientShading) return 11;

    AudioDirector director;
    AudioState listening;
    listening.active = listening.grounded = true;
    const auto first = director.Tick(listening, .05f);
    if (!first.startWind || !first.water || first.step) return 12;
    bool moved = false;
    for (int step = 0; step < 10; ++step) {
        listening.session.feet.x += .1f;
        moved |= director.Tick(listening, .05f).step;
    }
    if (!moved) return 13;
    listening.active = false;
    if (!director.Tick(listening, .05f).stopWind) return 14;
    std::cout << "RIVERBANK_MULTIPLAYER_SETTINGS_PASS modal-input preference-roundtrip ambience and footsteps\n";
}
