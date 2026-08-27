#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/Serialization.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <utility>

using namespace Aevrynd;

static bool Near(float left, float right) { return std::abs(left - right) < .0001f; }

int main() {
    SerializationDocument source;
    SerializationObject player;
    if (!source.root.Write("alive", true) || !source.root.Write("score", std::int64_t{-42}) ||
        !source.root.Write("coins", std::uint64_t{9007199254740993ull}) || !source.root.Write("speed", 3.5f) ||
        !source.root.Write("name", "裂隙守望者") || !source.root.Write("spawn", Vec2{12.5f, -3.f}) ||
        !player.Write("transform", Transform3D{{1, 2, 3}, {.1f, .2f, .3f}, {2, 2, 2}}) ||
        !player.Write("futureField", "ignored") || !source.root.Write("player", player)) {
        std::cerr << "Writing supported values failed\n"; return 1;
    }

    std::string encoded; SerializationError error;
    if (!source.Encode(encoded, &error) || encoded.find("aevrynd.serialization") == std::string::npos) {
        std::cerr << "Encoding versioned document failed\n"; return 2;
    }
    SerializationDocument decoded;
    if (!SerializationDocument::Decode(encoded, decoded, &error) || decoded.version != SerializationDocument::CurrentVersion) {
        std::cerr << "Decoding versioned document failed\n"; return 3;
    }

    bool alive{}; std::int64_t score{}; std::uint64_t coins{}; float speed{}; std::string name;
    Vec2 spawn{}; SerializationObject decodedPlayer; Transform3D transform;
    if (!decoded.root.Read("alive", alive) || !decoded.root.Read("score", score) || !decoded.root.Read("coins", coins) ||
        !decoded.root.Read("speed", speed) || !decoded.root.Read("name", name) || !decoded.root.Read("spawn", spawn) ||
        !decoded.root.Read("player", decodedPlayer) || !decodedPlayer.Read("transform", transform) || !alive || score != -42 ||
        coins != 9007199254740993ull || !Near(speed, 3.5f) || name != "裂隙守望者" || !Near(spawn.x, 12.5f) ||
        !Near(transform.position.z, 3.f) || !Near(transform.rotation.y, .2f) || !Near(transform.scale.x, 2.f)) {
        std::cerr << "Supported value round trip failed\n"; return 4;
    }

    int missingDefault = 77;
    if (!decoded.root.Read("missing", missingDefault) || missingDefault != 77 || !decodedPlayer.Contains("futureField")) {
        std::cerr << "Schema evolution behavior failed\n"; return 5;
    }

    SerializationDocument unchanged; unchanged.version = 1; unchanged.root.Write("sentinel", std::int32_t{9});
    if (SerializationDocument::Decode("{\"format\":\"aevrynd.serialization\",\"version\":1,\"root\":{\"bad\":[1,}}", unchanged, &error)) {
        std::cerr << "Malformed input was accepted\n"; return 6;
    }
    std::int32_t sentinel{};
    if (!unchanged.root.Read("sentinel", sentinel) || sentinel != 9) {
        std::cerr << "Malformed decode was not atomic\n"; return 7;
    }

    std::int32_t atomicNumber = 31;
    if (decoded.root.Read("name", atomicNumber) || atomicNumber != 31) {
        std::cerr << "Type mismatch changed destination\n"; return 8;
    }
    if (source.root.Write("invalid", std::numeric_limits<float>::infinity())) {
        std::cerr << "Non-finite number was accepted\n"; return 9;
    }

    SerializationLimits tiny; tiny.maxDocumentBytes = 32;
    std::string preserved = "preserved";
    if (source.Encode(preserved, &error, tiny) || preserved != "preserved" || error.code != SerializationErrorCode::DocumentTooLarge) {
        std::cerr << "Encode size limit was not atomic\n"; return 10;
    }
    if (SerializationDocument::Decode(encoded, unchanged, &error, tiny) || error.code != SerializationErrorCode::DocumentTooLarge) {
        std::cerr << "Decode size limit failed\n"; return 11;
    }
    if (SerializationDocument::Decode("{\"format\":\"aevrynd.serialization\",\"version\":2,\"root\":{}}", unchanged, &error) || error.code != SerializationErrorCode::UnsupportedVersion) {
        std::cerr << "Future version was accepted\n"; return 12;
    }
    if (SerializationDocument::Decode("{\"format\":\"aevrynd.serialization\",\"version\":1,\"root\":{\"x\":1,\"x\":2}}", unchanged, &error) || error.code != SerializationErrorCode::DuplicateField) {
        std::cerr << "Duplicate field was accepted\n"; return 13;
    }
    SerializationDocument bounded;bounded.root.Write("value",std::int32_t{1});SerializationLimits exactFields;exactFields.maxFields=4;std::string boundedText;if(!bounded.Encode(boundedText,&error,exactFields)||!SerializationDocument::Decode(boundedText,unchanged,&error,exactFields)){std::cerr<<"Field limit encode/decode symmetry failed\n";return 14;}exactFields.maxFields=3;if(bounded.Encode(boundedText,&error,exactFields)){std::cerr<<"Field limit boundary was not enforced\n";return 15;}
    SerializationLimits exactDepth;exactDepth.maxDepth=2;if(!bounded.Encode(boundedText,&error,exactDepth)||!SerializationDocument::Decode(boundedText,unchanged,&error,exactDepth)){std::cerr<<"Depth limit encode/decode symmetry failed\n";return 16;}exactDepth.maxDepth=1;if(bounded.Encode(boundedText,&error,exactDepth)){std::cerr<<"Depth limit boundary was not enforced\n";return 17;}
    SerializationObject movable;movable.Write("value",std::int32_t{7});SerializationObject moved=std::move(movable);if(!movable.Write("reused",std::int32_t{9})||!movable.Contains("reused")||!moved.Contains("value")){std::cerr<<"Moved-from object invariant failed\n";return 18;}
    const std::string invalidUtf8="{\"format\":\"aevrynd.serialization\",\"version\":1,\"root\":{\"text\":\"\xC0\xAF\"}}";if(SerializationDocument::Decode(invalidUtf8,unchanged,&error)||error.code!=SerializationErrorCode::InvalidUtf8){std::cerr<<"Invalid UTF-8 was accepted\n";return 19;}
    return 0;
}
