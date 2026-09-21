#include "rbx_instance.h"
#include <unordered_map>
#include <chrono>
#include <random>
#include <algorithm>
#include <limits>
#include <cstring>

namespace pulseblockz::rbx {

// ---- Value -------------------------------------------------------------------------
bool Value::operator==(const Value& o) const {
    if (type != o.type) return false;
    switch (type) {
    case Nil: return true;
    case Bool: return b == o.b;
    case Number: return n == o.n;
    case String: return s == o.s;
    case Vector3: case Vector2: return v == o.v;
    case UDim: return u[0] == o.u[0] && u[1] == o.u[1];
    case UDim2: return u[0] == o.u[0] && u[1] == o.u[1] && u[2] == o.u[2] && u[3] == o.u[3];
    case Color3: return c == o.c;
    case Ref: return ref == o.ref;
    case Enum: return n == o.n;
    case Font: return s == o.s && n == o.n && b == o.b;
    case NumberRange: return u[0] == o.u[0] && u[1] == o.u[1];
    case PhysProps: return u[0] == o.u[0] && u[1] == o.u[1] && u[2] == o.u[2] && u[3] == o.u[3] && n == o.n;
    case NumberSequence: case ColorSequence: return kp == o.kp;
    case Content: return n == o.n && s == o.s && ref == o.ref;
    }
    return false;
}

float Value::sampleNumber(float t) const {
    if (type == Number) return (float)n;
    if (type == NumberRange) return u[0];
    if (type != NumberSequence || kp.size() < 3) return 0;
    size_t cnt = kp.size() / 3;
    if (t <= kp[0]) return kp[1];
    for (size_t k = 1; k < cnt; k++) {
        if (t <= kp[k * 3]) {
            float t0 = kp[(k - 1) * 3], t1 = kp[k * 3], f = t1 > t0 ? (t - t0) / (t1 - t0) : 0;
            return kp[(k - 1) * 3 + 1] + (kp[k * 3 + 1] - kp[(k - 1) * 3 + 1]) * f;
        }
    }
    return kp[(cnt - 1) * 3 + 1];
}

Col3 Value::sampleColor(float t) const {
    if (type == Color3) return c;
    if (type != ColorSequence || kp.size() < 4) return {1, 1, 1};
    size_t cnt = kp.size() / 4;
    auto at = [&](size_t k) { return Col3{kp[k * 4 + 1], kp[k * 4 + 2], kp[k * 4 + 3]}; };
    if (t <= kp[0]) return at(0);
    for (size_t k = 1; k < cnt; k++) {
        if (t <= kp[k * 4]) {
            float t0 = kp[(k - 1) * 4], t1 = kp[k * 4], f = t1 > t0 ? (t - t0) / (t1 - t0) : 0;
            Col3 a = at(k - 1), b = at(k);
            return {a.r + (b.r - a.r) * f, a.g + (b.g - a.g) * f, a.b + (b.b - a.b) * f};
        }
    }
    return at(cnt - 1);
}

const char* Value::typeName(Type t) {
    switch (t) {
    case Nil: return "nil";
    case Bool: return "boolean";
    case Number: return "number";
    case String: return "string";
    case Vector3: return "Vector3";
    case Vector2: return "Vector2";
    case UDim: return "UDim";
    case UDim2: return "UDim2";
    case Color3: return "Color3";
    case Ref: return "Instance";
    case Enum: return "EnumItem";
    case Font: return "Font";
    case NumberRange: return "NumberRange";
    case PhysProps: return "PhysicalProperties";
    case NumberSequence: return "NumberSequence";
    case ColorSequence: return "ColorSequence";
    case Content: return "Content";
    }
    return "?";
}

// ---- enums -------------------------------------------------------------------------
const EnumItem* EnumDef::find(const std::string& itemName) const {
    for (auto& i : items) if (itemName == i.name) return &i;
    return nullptr;
}
const EnumItem* EnumDef::findValue(int v) const {
    for (auto& i : items) if (i.value == v) return &i;
    return nullptr;
}

// Values are Roblox's own, so a serialized place round-trips.
static std::vector<EnumDef> g_enums = {
    {"PartType", {{"Ball", 0}, {"Block", 1}, {"Cylinder", 2}, {"Wedge", 3}, {"CornerWedge", 4}}},
    {"MeshType", {{"Head", 0}, {"Torso", 1}, {"Wedge", 2}, {"Sphere", 3}, {"Cylinder", 4}, {"FileMesh", 5}, {"Brick", 6}, {"Prism", 7},
                  {"Pyramid", 8}, {"ParallelRamp", 9}, {"RightAngleRamp", 10}, {"CornerWedge", 11}}},
    {"CollisionFidelity", {{"Default", 0}, {"Hull", 1}, {"Box", 2}, {"PreciseConvexDecomposition", 3}, {"Tunable", 4}}},
    {"FluidFidelity", {{"Automatic", 0}, {"UseCollisionGeometry", 1}, {"UsePreciseGeometry", 2}}},
    {"WeldConstraintPreserve", {{"All", 0}, {"None", 1}, {"Touching", 2}}},
    {"RenderFidelity", {{"Automatic", 0}, {"Precise", 1}, {"Performance", 2}}},
    {"Material", {{"Plastic", 256}, {"SmoothPlastic", 272}, {"Neon", 288}, {"Wood", 512}, {"WoodPlanks", 528},
                  {"Marble", 784}, {"Slate", 800}, {"Concrete", 816}, {"Granite", 832}, {"Brick", 848},
                  {"Pebble", 864}, {"Cobblestone", 880}, {"CorrodedMetal", 1040}, {"DiamondPlate", 1056},
                  {"Foil", 1072}, {"Metal", 1088}, {"Grass", 1280}, {"Sand", 1296}, {"Fabric", 1312},
                  {"Ice", 1536}, {"Glass", 1568}, {"ForceField", 1584},
                  // the terrain materials
                  {"Basalt", 788}, {"CrackedLava", 804}, {"Limestone", 820}, {"Pavement", 836},
                  {"Rock", 896}, {"Sandstone", 912}, {"LeafyGrass", 1284}, {"Snow", 1328}, {"Mud", 1344},
                  {"Ground", 1360}, {"Asphalt", 1376}, {"Salt", 1392}, {"Glacier", 1552},
                  {"Cardboard", 2304}, {"Carpet", 2305}, {"CeramicTiles", 2306}, {"ClayRoofTiles", 2307},
                  {"RoofShingles", 2308}, {"Leather", 2309}, {"Plaster", 2310}, {"Rubber", 2311},
                  {"Air", 1792}, {"Water", 2048}}},
    {"AnimationPriority", {{"Idle", 0}, {"Movement", 1}, {"Action", 2}, {"Action2", 3}, {"Action3", 4}, {"Action4", 5}, {"Core", 1000}}},
    {"PoseEasingStyle", {{"Linear", 0}, {"Constant", 1}, {"Elastic", 2}, {"Cubic", 3}, {"Bounce", 4}, {"CubicV2", 5}}},
    {"PoseEasingDirection", {{"Out", 0}, {"In", 1}, {"InOut", 2}}},
    {"AccessoryType", {{"Unknown", 0}, {"Hat", 1}, {"Hair", 2}, {"Face", 3}, {"Neck", 4}, {"Shoulder", 5},
                       {"Front", 6}, {"Back", 7}, {"Waist", 8}, {"TShirt", 9}, {"Shirt", 10}, {"Pants", 11},
                       {"Jacket", 12}, {"Sweater", 13}, {"Shorts", 14}, {"LeftShoe", 15}, {"RightShoe", 16},
                       {"DressSkirt", 17}, {"Eyebrow", 18}, {"Eyelash", 19}}},
    {"HumanoidRigType", {{"R6", 0}, {"R15", 1}}},
    {"NormalId", {{"Right", 0}, {"Top", 1}, {"Back", 2}, {"Left", 3}, {"Bottom", 4}, {"Front", 5}}},
    {"ParticleEmitterShape", {{"Box", 0}, {"Sphere", 1}, {"Cylinder", 2}, {"Disc", 3}}},
    {"ExplosionType", {{"NoCraters", 0}, {"Craters", 1}, {"CratersAndDebris", 2}}},
    {"ActuatorType", {{"None", 0}, {"Motor", 1}, {"Servo", 2}}},
    {"ActuatorRelativeTo", {{"Attachment0", 0}, {"Attachment1", 1}, {"World", 2}}},
    {"PositionAlignmentMode", {{"OneAttachment", 0}, {"TwoAttachment", 1}}},
    {"OrientationAlignmentMode", {{"OneAttachment", 0}, {"TwoAttachment", 1}}},
    {"VelocityConstraintMode", {{"Vector", 0}, {"Line", 1}, {"Plane", 2}}},
    {"ForceLimitMode", {{"Magnitude", 0}, {"PerAxis", 1}}},
    {"HighlightDepthMode", {{"AlwaysOnTop", 0}, {"Occluded", 1}}},
    {"ParticleEmitterShapeStyle", {{"Volume", 0}, {"Surface", 1}}},
    {"ParticleEmitterShapeInOut", {{"Outward", 0}, {"Inward", 1}, {"InAndOut", 2}}},
    {"ParticleOrientation", {{"FacingCamera", 0}, {"FacingCameraWorldUp", 1}, {"VelocityParallel", 2}, {"VelocityPerpendicular", 3}}},
    {"SurfaceGuiSizingMode", {{"FixedSize", 0}, {"PixelsPerStud", 1}}},
    {"PathStatus", {{"Success", 0}, {"ClosestNoPath", 1}, {"ClosestOutOfRange", 2}, {"FailStartNotEmpty", 3}, {"FailFinishNotEmpty", 4}, {"NoPath", 5}}},
    {"PathWaypointAction", {{"Walk", 0}, {"Jump", 1}, {"Custom", 2}}},
    {"HumanoidDisplayDistanceType", {{"Viewer", 0}, {"Subject", 1}, {"None", 2}}},
    {"HumanoidHealthDisplayType", {{"DisplayWhenDamaged", 0}, {"AlwaysOn", 1}, {"AlwaysOff", 2}}},
    {"NameOcclusion", {{"NoOcclusion", 0}, {"EnemyOcclusion", 1}, {"OccludeAll", 2}}},
    {"Axis", {{"X", 0}, {"Y", 1}, {"Z", 2}}},
    {"RunContext", {{"Legacy", 0}, {"Server", 1}, {"Client", 2}, {"Plugin", 3}}},
    {"EasingStyle", {{"Linear", 0}, {"Sine", 1}, {"Back", 2}, {"Quad", 3}, {"Quart", 4}, {"Quint", 5},
                     {"Bounce", 6}, {"Elastic", 7}, {"Exponential", 8}, {"Circular", 9}, {"Cubic", 10}}},
    {"EasingDirection", {{"In", 0}, {"Out", 1}, {"InOut", 2}}},
    {"PlaybackState", {{"Begin", 0}, {"Delayed", 1}, {"Playing", 2}, {"Paused", 3}, {"Completed", 4}, {"Cancelled", 5}}},
    {"HumanoidStateType", {{"FallingDown", 1}, {"Ragdoll", 9}, {"GettingUp", 2}, {"Jumping", 3}, {"Swimming", 4}, {"Freefall", 5}, {"Flying", 6}, {"Landed", 7},
                           {"Running", 8}, {"RunningNoPhysics", 10}, {"StrafingNoPhysics", 11}, {"Climbing", 12}, {"Seated", 13}, {"PlatformStanding", 14}, {"Dead", 15}, {"Physics", 16}, {"None", 18}}},
    {"CameraType", {{"Fixed", 0}, {"Attach", 1}, {"Watch", 2}, {"Track", 3}, {"Follow", 4}, {"Custom", 5}, {"Scriptable", 6}, {"Orbital", 7}}},
    {"ProximityPromptExclusivity", {{"OnePerButton", 0}, {"OneGlobally", 1}, {"AlwaysShow", 2}}},
    {"ProximityPromptStyle", {{"Default", 0}, {"Custom", 1}}},
    {"ProximityPromptInputType", {{"Keyboard", 0}, {"Gamepad", 1}, {"Touch", 2}}},
    {"RollOffMode", {{"Inverse", 0}, {"Linear", 1}, {"LinearSquare", 2}, {"InverseTapered", 3}}},
    // the Audio API's enums
    {"AudioFilterType", {{"Peak", 0}, {"LowShelf", 1}, {"HighShelf", 2}, {"Lowpass12dB", 3}, {"Lowpass24dB", 4}, {"Lowpass48dB", 5},
                         {"Highpass12dB", 6}, {"Highpass24dB", 7}, {"Highpass48dB", 8}, {"Bandpass", 9}, {"Notch", 10}, {"Lowpass6dB", 11}}},
    {"DistanceAttenuationMode", {{"Custom", 0}, {"InverseTapered", 1}, {"Linear", 2}, {"LinearSquared", 3}, {"Inverse", 4}}},
    {"EmitterPositionType", {{"Parent", 0}, {"Instance", 1}}},
    {"ListenerPositionType", {{"Parent", 0}, {"Instance", 1}}},
    {"ListenerLocation", {{"Default", 0}, {"None", 1}, {"Character", 2}, {"Camera", 3}}},
    {"AudioChannelLayout", {{"Mono", 0}, {"Stereo", 1}, {"Quad", 2}, {"Surround_5", 3}, {"Surround_5_1", 4}, {"Surround_7_1", 5}, {"Surround_7_1_4", 6}}},
    {"AudioWindowSize", {{"Small", 0}, {"Medium", 1}, {"Large", 2}}},
    {"AccessModifierType", {{"Allow", 0}, {"Deny", 1}}},
    {"AudioSubType", {{"Music", 1}, {"SoundEffect", 2}}},
    {"RotationType", {{"MovementRelative", 0}, {"CameraRelative", 1}}},
    {"SavedQualitySetting", {{"Automatic", 0}, {"QualityLevel1", 1}, {"QualityLevel2", 2}, {"QualityLevel3", 3}, {"QualityLevel4", 4}, {"QualityLevel5", 5},
                             {"QualityLevel6", 6}, {"QualityLevel7", 7}, {"QualityLevel8", 8}, {"QualityLevel9", 9}, {"QualityLevel10", 10}}},
    {"ChatColor", {{"Blue", 0}, {"Green", 1}, {"Red", 2}, {"White", 3}}},
    {"ChatVersion", {{"LegacyChatService", 0}, {"TextChatService", 1}}},
    {"TextChatMessageStatus", {{"Unknown", 1}, {"Success", 2}, {"Sending", 3}, {"TextFilterFailed", 4}, {"Floodchecked", 5},
                               {"InvalidPrivacySettings", 6}, {"InvalidTextChannelPermissions", 7}, {"MessageTooLong", 8}}},
    {"CoreGuiType", {{"PlayerList", 0}, {"Health", 1}, {"Backpack", 2}, {"Chat", 3}, {"All", 4}, {"EmotesMenu", 5}, {"SelfView", 6}, {"Captures", 7}}},
    {"KeyCode", {{"Unknown", 0}, {"Backspace", 8}, {"Tab", 9}, {"Return", 13}, {"Escape", 27}, {"Space", 32}, {"Quote", 39},
                 {"Comma", 44}, {"Minus", 45}, {"Period", 46}, {"Slash", 47},
                 {"Zero", 48}, {"One", 49}, {"Two", 50}, {"Three", 51}, {"Four", 52}, {"Five", 53}, {"Six", 54}, {"Seven", 55}, {"Eight", 56}, {"Nine", 57},
                 {"Semicolon", 59}, {"Equals", 61}, {"LeftBracket", 91}, {"BackSlash", 92}, {"RightBracket", 93}, {"Backquote", 96},
                 {"A", 97}, {"B", 98}, {"C", 99}, {"D", 100}, {"E", 101}, {"F", 102}, {"G", 103}, {"H", 104}, {"I", 105}, {"J", 106},
                 {"K", 107}, {"L", 108}, {"M", 109}, {"N", 110}, {"O", 111}, {"P", 112}, {"Q", 113}, {"R", 114}, {"S", 115}, {"T", 116},
                 {"U", 117}, {"V", 118}, {"W", 119}, {"X", 120}, {"Y", 121}, {"Z", 122}, {"Delete", 127}, {"KeypadEnter", 271},
                 {"Up", 273}, {"Down", 274}, {"Right", 275}, {"Left", 276}, {"Insert", 277}, {"Home", 278}, {"End", 279}, {"PageUp", 280}, {"PageDown", 281},
                 {"F1", 282}, {"F2", 283}, {"F3", 284}, {"F4", 285}, {"F5", 286}, {"F6", 287}, {"F7", 288}, {"F8", 289}, {"F9", 290}, {"F10", 291}, {"F11", 292}, {"F12", 293},
                 {"CapsLock", 301}, {"RightShift", 303}, {"LeftShift", 304}, {"RightControl", 305}, {"LeftControl", 306},
                 {"RightAlt", 307}, {"LeftAlt", 308}, {"LeftSuper", 311}, {"RightSuper", 312},
                 {"ButtonX", 1000}, {"ButtonY", 1001}, {"ButtonB", 1002}, {"ButtonA", 1003}, {"ButtonR1", 1004}, {"ButtonL1", 1005},
                 {"ButtonR2", 1006}, {"ButtonL2", 1007}, {"ButtonR3", 1008}, {"ButtonL3", 1009}, {"ButtonStart", 1010}, {"ButtonSelect", 1011},
                 {"DPadLeft", 1012}, {"DPadRight", 1013}, {"DPadUp", 1014}, {"DPadDown", 1015}, {"Thumbstick1", 1016}, {"Thumbstick2", 1017}}},
    {"UserInputType", {{"MouseButton1", 0}, {"MouseButton2", 1}, {"MouseButton3", 2}, {"MouseWheel", 3}, {"MouseMovement", 4}, {"Touch", 7},
                       {"Keyboard", 8}, {"Focus", 9}, {"Accelerometer", 10}, {"Gyro", 11}, {"Gamepad1", 12}, {"TextInput", 21}, {"InputMethod", 22}, {"None", 23}}},
    {"ContextActionResult", {{"Sink", 0}, {"Pass", 1}}},
    {"RaycastFilterType", {{"Exclude", 0}, {"Include", 1}, {"Blacklist", 0}, {"Whitelist", 1}}},
    {"ContextActionPriority", {{"Low", 1000}, {"Medium", 2000}, {"Default", 2000}, {"High", 3000}}},
    {"MouseBehavior", {{"Default", 0}, {"LockCenter", 1}, {"LockCurrentPosition", 2}}},
    {"Font", {{"Legacy", 0}, {"Arial", 1}, {"ArialBold", 2}, {"SourceSans", 3}, {"SourceSansBold", 4}, {"SourceSansLight", 5},
              {"SourceSansItalic", 6}, {"Bodoni", 7}, {"Garamond", 8}, {"Cartoon", 9}, {"Code", 10}, {"Highway", 11}, {"SciFi", 12},
              {"Arcade", 13}, {"Fantasy", 14}, {"Antique", 15}, {"SourceSansSemibold", 16}, {"Gotham", 17}, {"GothamMedium", 18},
              {"GothamBold", 19}, {"GothamBlack", 20}, {"AmaticSC", 21}, {"Bangers", 22}, {"Creepster", 23}, {"DenkOne", 24},
              {"Fondamento", 25}, {"FredokaOne", 26}, {"GrenzeGotisch", 27}, {"IndieFlower", 28}, {"JosefinSans", 29}, {"Jura", 30},
              {"Kalam", 31}, {"LuckiestGuy", 32}, {"Merriweather", 33}, {"Michroma", 34}, {"Nunito", 35}, {"Oswald", 36},
              {"PatrickHand", 37}, {"PermanentMarker", 38}, {"Roboto", 39}, {"RobotoCondensed", 40}, {"RobotoMono", 41},
              {"Sarpanch", 42}, {"SpecialElite", 43}, {"TitilliumWeb", 44}, {"Ubuntu", 45}, {"BuilderSans", 46},
              {"BuilderSansMedium", 47}, {"BuilderSansBold", 48}, {"BuilderSansExtraBold", 49}, {"Unknown", 100}}},
    {"FontWeight", {{"Thin", 100}, {"ExtraLight", 200}, {"Light", 300}, {"Regular", 400}, {"Medium", 500}, {"SemiBold", 600},
                    {"Bold", 700}, {"ExtraBold", 800}, {"Heavy", 900}}},
    {"FontStyle", {{"Normal", 0}, {"Italic", 1}}},
    {"TextXAlignment", {{"Left", 0}, {"Right", 1}, {"Center", 2}}},
    {"TextYAlignment", {{"Top", 0}, {"Center", 1}, {"Bottom", 2}}},
    {"TextTruncate", {{"None", 0}, {"AtEnd", 1}, {"SplitWord", 2}}},
    {"AutomaticSize", {{"None", 0}, {"X", 1}, {"Y", 2}, {"XY", 3}}},
    {"SizeConstraint", {{"RelativeXY", 0}, {"RelativeXX", 1}, {"RelativeYY", 2}}},
    {"ZIndexBehavior", {{"Global", 0}, {"Sibling", 1}}},
    {"ScaleType", {{"Stretch", 0}, {"Slice", 1}, {"Tile", 2}, {"Fit", 3}, {"Crop", 4}}},
    {"ResamplerMode", {{"Default", 0}, {"Pixelated", 1}}},
    {"FillDirection", {{"Horizontal", 0}, {"Vertical", 1}}},
    {"HorizontalAlignment", {{"Center", 0}, {"Left", 1}, {"Right", 2}}},
    {"VerticalAlignment", {{"Center", 0}, {"Top", 1}, {"Bottom", 2}}},
    {"AlphaMode", {{"Overlay", 0}, {"Transparency", 1}, {"TintMask", 2}}},
    {"MaterialPattern", {{"Regular", 0}, {"Organic", 1}}},
    {"TextureMode", {{"Stretch", 0}, {"Wrap", 1}, {"Static", 2}}},
    {"ParticleFlipbookLayout", {{"None", 0}, {"Grid2x2", 1}, {"Grid4x4", 2}, {"Grid8x8", 3}, {"Custom", 4}}},
    {"ParticleFlipbookMode", {{"Loop", 0}, {"OneShot", 1}, {"PingPong", 2}, {"Random", 3}}},
    {"TonemapperPreset", {{"Default", 0}, {"Retro", 1}}},
    {"LightingStyle", {{"Realistic", 0}, {"Soft", 1}}},
    {"RolloutState", {{"Default", 0}, {"Disabled", 1}, {"Enabled", 2}}},
    // Declared but not acted on: Enum.X.Y resolves and a place file's tokens round-trip.
    {"InputType", {{"NoInput", 0}, {"Constant", 1}, {"Sin", 2}}},
    {"InitialDockState", {{"Top", 0}, {"Bottom", 1}, {"Left", 2}, {"Right", 3}, {"Float", 4}}},
    {"ImageCombineType", {{"BlendSourceOver", 1}, {"Overwrite", 2}, {"Add", 3}, {"Multiply", 4}, {"AlphaBlend", 5}, {"NormalMapBlend", 6}, {"Subtract", 7}}},
    {"AntiAliasing", {{"Disabled", 0}, {"Enabled", 1}}},
    {"MeshAttribute", {{"Vertex", 0}, {"Normal", 1}, {"Color", 2}, {"UV", 3}, {"Face", 4}}},
    {"FacsActionUnit", {{"ChinRaiserUpperLip", 0}, {"ChinRaiser", 1}, {"FlatPucker", 2}, {"Funneler", 3}, {"LowerLipSuck", 4}, {"LipPresser", 5},
                        {"LipsTogether", 6}, {"MouthLeft", 7}, {"MouthRight", 8}, {"Pucker", 9}, {"UpperLipSuck", 10}, {"LeftCheekPuff", 11},
                        {"LeftDimpler", 12}, {"LeftLipCornerDown", 13}, {"LeftLowerLipDepressor", 14}, {"LeftLipCornerPuller", 15},
                        {"LeftLipStretcher", 16}, {"LeftUpperLipRaiser", 17}, {"RightCheekPuff", 18}, {"RightDimpler", 19},
                        {"RightLipCornerDown", 20}, {"RightLowerLipDepressor", 21}, {"RightLipCornerPuller", 22}, {"RightLipStretcher", 23},
                        {"RightUpperLipRaiser", 24}, {"JawDrop", 25}, {"JawLeft", 26}, {"JawRight", 27}, {"Corrugator", 28},
                        {"LeftBrowLowerer", 29}, {"LeftOuterBrowRaiser", 30}, {"LeftNoseWrinkler", 31}, {"LeftInnerBrowRaiser", 32},
                        {"RightBrowLowerer", 33}, {"RightOuterBrowRaiser", 34}, {"RightInnerBrowRaiser", 35}, {"RightNoseWrinkler", 36},
                        {"EyesLookDown", 37}, {"EyesLookLeft", 38}, {"EyesLookUp", 39}, {"EyesLookRight", 40}, {"LeftCheekRaiser", 41},
                        {"LeftEyeUpperLidRaiser", 42}, {"LeftEyeClosed", 43}, {"RightCheekRaiser", 44}, {"RightEyeUpperLidRaiser", 45},
                        {"RightEyeClosed", 46}, {"TongueDown", 47}, {"TongueOut", 48}, {"TongueUp", 49}}},
    {"ImageAlphaType", {{"Default", 1}, {"LockCanvasAlpha", 2}, {"LockCanvasColor", 3}}},
    {"CreateContentResult", {{"Success", 1}, {"PermissionDenied", 2}, {"UploadFailed", 3}, {"StorageLimitExceeded", 4}, {"Unknown", 5}}},
    {"ContentSourceType", {{"None", 0}, {"Uri", 1}, {"Object", 2}, {"Opaque", 3}}},
    {"SurfaceType", {{"Smooth", 0}, {"Glue", 1}, {"Weld", 2}, {"Studs", 3}, {"Inlet", 4}, {"Universal", 5}, {"Hinge", 6}, {"Motor", 7}, {"SteppingMotor", 8}, {"SmoothNoOutlines", 10}}},
    {"SurfaceConstraint", {{"None", 0}, {"Hinge", 1}, {"SteppingMotor", 2}, {"Motor", 3}}},
    {"FormFactor", {{"Symmetric", 0}, {"Brick", 1}, {"Plate", 2}, {"Custom", 3}}},
    {"Limb", {{"Head", 0}, {"Torso", 1}, {"LeftArm", 2}, {"RightArm", 3}, {"LeftLeg", 4}, {"RightLeg", 5}, {"Unknown", 6}}},
    {"BodyPart", {{"Head", 0}, {"Torso", 1}, {"LeftArm", 2}, {"RightArm", 3}, {"LeftLeg", 4}, {"RightLeg", 5}}},
    {"Technology", {{"Legacy", 0}, {"Voxel", 1}, {"Compatibility", 2}, {"ShadowMap", 3}, {"Future", 4}}},
    {"TweenStatus", {{"Canceled", 0}, {"Completed", 1}}},
    {"HttpContentType", {{"ApplicationJson", 0}, {"ApplicationXml", 1}, {"ApplicationUrlEncoded", 2}, {"TextPlain", 3}, {"TextXml", 4}}},
    {"AssetFetchStatus", {{"Success", 0}, {"Failure", 1}, {"None", 2}, {"Loading", 3}, {"TimedOut", 4}}},
    {"BorderMode", {{"Outline", 0}, {"Middle", 1}, {"Inset", 2}}},
    {"ButtonStyle", {{"Custom", 0}, {"RobloxButtonDefault", 1}, {"RobloxButton", 2}, {"RobloxRoundButton", 3}, {"RobloxRoundDefaultButton", 4}, {"RobloxRoundDropdownButton", 5}}},
    {"FrameStyle", {{"Custom", 0}, {"ChatBlue", 1}, {"RobloxSquare", 2}, {"RobloxRound", 3}, {"ChatGreen", 4}, {"ChatRed", 5}, {"DropShadow", 6}}},
    {"LineJoinMode", {{"Round", 0}, {"Bevel", 1}, {"Miter", 2}}},
    {"ApplyStrokeMode", {{"Contextual", 0}, {"Border", 1}}},
    {"TextDirection", {{"Auto", 0}, {"LeftToRight", 1}, {"RightToLeft", 2}}},
    {"SelectionBehavior", {{"Escape", 0}, {"Stop", 1}}},
    {"DevCameraOcclusionMode", {{"Zoom", 0}, {"Invisicam", 1}}},
    {"DevComputerCameraMovementMode", {{"UserChoice", 0}, {"Classic", 1}, {"Follow", 2}, {"Orbital", 3}, {"CameraToggle", 4}}},
    {"DevComputerMovementMode", {{"UserChoice", 0}, {"KeyboardMouse", 1}, {"ClickToMove", 2}, {"Scriptable", 3}}},
    {"DevTouchMovementMode", {{"UserChoice", 0}, {"Thumbstick", 1}, {"DPad", 2}, {"Thumbpad", 3}, {"ClickToMove", 4}, {"Scriptable", 5}, {"DynamicThumbstick", 6}}},
    {"DevTouchCameraMovementMode", {{"UserChoice", 0}, {"Classic", 1}, {"Follow", 2}, {"Orbital", 3}}},
    {"CameraMode", {{"Classic", 0}, {"LockFirstPerson", 1}}},
    {"ThumbnailType", {{"HeadShot", 0}, {"AvatarBust", 1}, {"AvatarThumbnail", 2}}},
    {"ThumbnailSize", {{"Size48x48", 0}, {"Size180x180", 1}, {"Size420x420", 2}, {"Size60x60", 3}, {"Size100x100", 4}, {"Size150x150", 5}, {"Size352x352", 6}}},
    {"MembershipType", {{"None", 0}, {"BuildersClub", 1}, {"TurboBuildersClub", 2}, {"OutrageousBuildersClub", 3}, {"Premium", 4}}},
    {"ScreenOrientation", {{"LandscapeLeft", 0}, {"LandscapeRight", 1}, {"LandscapeSensor", 2}, {"Portrait", 3}, {"Sensor", 4}}},
    {"TextFilterContext", {{"PublicChat", 1}, {"PrivateChat", 2}}},
    {"ReverbType", {{"NoReverb", 0}, {"GenericReverb", 1}, {"PaddedCell", 2}, {"Room", 3}, {"Bathroom", 4}, {"LivingRoom", 5}, {"StoneRoom", 6}, {"Auditorium", 7},
                    {"ConcertHall", 8}, {"Cave", 9}, {"Arena", 10}, {"Hangar", 11}, {"CarpettedHallway", 12}, {"Hallway", 13}, {"StoneCorridor", 14}, {"Alley", 15},
                    {"Forest", 16}, {"City", 17}, {"Mountains", 18}, {"Quarry", 19}, {"Plain", 20}, {"ParkingLot", 21}, {"SewerPipe", 22}, {"UnderWater", 23}}},
    {"ListenerType", {{"Camera", 0}, {"CFrame", 1}, {"ObjectPosition", 2}, {"ObjectCFrame", 3}}},
    {"MessageType", {{"MessageOutput", 0}, {"MessageInfo", 1}, {"MessageWarning", 2}, {"MessageError", 3}}},
    {"KeyInterpolationMode", {{"Constant", 0}, {"Linear", 1}, {"Cubic", 2}}},
    {"ModelLevelOfDetail", {{"Automatic", 0}, {"StreamingMesh", 1}, {"Disabled", 2}}},
    {"ModelStreamingMode", {{"Default", 0}, {"Atomic", 1}, {"Persistent", 2}, {"PersistentPerPlayer", 3}, {"Nonatomic", 4}}},
    {"AdornCullingMode", {{"Automatic", 0}, {"Never", 1}}},
    {"PlayerActions", {{"CharacterForward", 0}, {"CharacterBackward", 1}, {"CharacterLeft", 2}, {"CharacterRight", 3}, {"CharacterJump", 4}}},
    {"SwipeDirection", {{"Right", 0}, {"Left", 1}, {"Up", 2}, {"Down", 3}, {"None", 4}}},
    {"FontSize", {{"Size8", 0}, {"Size9", 1}, {"Size10", 2}, {"Size11", 3}, {"Size12", 4}, {"Size14", 5}, {"Size18", 6}, {"Size24", 7}, {"Size36", 8}, {"Size48", 9},
                  {"Size28", 10}, {"Size32", 11}, {"Size42", 12}, {"Size60", 13}, {"Size96", 14}}},
    {"DialogPurpose", {{"Quest", 0}, {"Help", 1}, {"Shop", 2}}},
    {"DialogTone", {{"Neutral", 0}, {"Friendly", 1}, {"Enemy", 2}}},
    {"DialogBehaviorType", {{"SinglePlayer", 0}, {"MultiplePlayers", 1}}},
    {"CurrencyType", {{"Default", 0}, {"Robux", 1}, {"Tix", 2}}},
    {"InfoType", {{"Asset", 0}, {"Product", 1}, {"GamePass", 2}, {"Subscription", 3}, {"Bundle", 4}}},
    {"ProductPurchaseDecision", {{"NotProcessedYet", 0}, {"PurchaseGranted", 1}}},
    {"Platform", {{"Windows", 0}, {"OSX", 1}, {"IOS", 2}, {"Android", 3}, {"XBoxOne", 4}, {"PS4", 5}, {"PS3", 6}, {"XBox360", 7}, {"WiiU", 8}, {"NX", 9}, {"Ouya", 10},
                  {"AndroidTV", 11}, {"Chromecast", 12}, {"Linux", 13}, {"SteamOS", 14}, {"WebOS", 15}, {"DOS", 16}, {"BeOS", 17}, {"UWP", 18}, {"PS5", 19}, {"None", 20}}},
    {"InOut", {{"Edge", 0}, {"Inset", 1}, {"Center", 2}}},
    {"LeftRight", {{"Left", 0}, {"Center", 1}, {"Right", 2}}},
    {"TopBottom", {{"Top", 0}, {"Center", 1}, {"Bottom", 2}}},
    {"BulkMoveMode", {{"FireAllEvents", 0}, {"FireCFrameChanged", 1}}},
    // the data family
    {"CreatorType", {{"User", 0}, {"Group", 1}}},
    {"JointCreationMode", {{"All", 0}, {"Surface", 1}, {"None", 2}}},
    {"RibbonTool", {{"Select", 0}, {"Scale", 1}, {"Rotate", 2}, {"Move", 3}, {"Transform", 4}, {"ColorPicker", 5}, {"MaterialPicker", 6}, {"Group", 7},
                    {"Ungroup", 8}, {"None", 9}, {"PivotEditor", 10}}},
    {"CustomCameraMode", {{"Default", 0}, {"Classic", 1}, {"Follow", 2}}},
    {"ComputerCameraMovementMode", {{"Default", 0}, {"Classic", 1}, {"Follow", 2}, {"Orbital", 3}, {"CameraToggle", 4}}},
    {"ComputerMovementMode", {{"Default", 0}, {"KeyboardMouse", 1}, {"ClickToMove", 2}}},
    {"TouchCameraMovementMode", {{"Default", 0}, {"Classic", 1}, {"Follow", 2}, {"Orbital", 3}}},
    {"TouchMovementMode", {{"Default", 0}, {"Thumbstick", 1}, {"DPad", 2}, {"Thumbpad", 3}, {"ClickToMove", 4}, {"DynamicThumbstick", 5}}},
    {"SortOrder", {{"Name", 0}, {"Custom", 1}, {"LayoutOrder", 2}}},
    {"StartCorner", {{"TopLeft", 0}, {"TopRight", 1}, {"BottomLeft", 2}, {"BottomRight", 3}}},
    {"AspectType", {{"FitWithinMaxSize", 0}, {"ScaleWithParentSize", 1}}},
    {"DominantAxis", {{"Width", 0}, {"Height", 1}}},
    {"ScrollingDirection", {{"X", 1}, {"Y", 2}, {"XY", 4}}},
    {"ElasticBehavior", {{"WhenScrollable", 0}, {"Always", 1}, {"Never", 2}}},
    {"ScrollBarInset", {{"None", 0}, {"ScrollBar", 1}, {"Always", 2}}},
    {"VerticalScrollBarPosition", {{"Right", 0}, {"Left", 1}}},
    {"UserInputState", {{"Begin", 0}, {"Change", 1}, {"End", 2}, {"Cancel", 3}}},
    {"BodyPartR15", {{"Head", 0}, {"UpperTorso", 1}, {"LowerTorso", 2}, {"LeftFoot", 3}, {"LeftLowerLeg", 4}, {"LeftUpperLeg", 5}, {"RightFoot", 6},
                     {"RightLowerLeg", 7}, {"RightUpperLeg", 8}, {"LeftHand", 9}, {"LeftLowerArm", 10}, {"LeftUpperArm", 11}, {"RightHand", 12},
                     {"RightLowerArm", 13}, {"RightUpperArm", 14}, {"RootPart", 15}, {"Unknown", 17}}},
    {"ChatStyle", {{"Classic", 0}, {"Bubble", 1}, {"ClassicAndBubble", 2}}},
    {"LoadDynamicHeads", {{"Default", 0}, {"Disabled", 1}, {"Enabled", 2}}},
    {"LoadCharacterLayeredClothing", {{"Default", 0}, {"Disabled", 1}, {"Enabled", 2}}},
    // the parts family
    {"AlignType", {{"Parallel", 0}, {"Perpendicular", 1}, {"AllAxes", 2}, {"PrimaryAxisParallel", 3}, {"PrimaryAxisPerpendicular", 4}, {"PrimaryAxisLookAt", 5}}},
    {"FieldOfViewMode", {{"Vertical", 0}, {"Diagonal", 1}, {"MaxAxis", 2}}},
    {"Style", {{"AlternatingSupports", 0}, {"BridgeStyleSupports", 1}, {"NoSupports", 2}}},
    {"DragDetectorDragStyle", {{"TranslateLine", 0}, {"TranslatePlane", 1}, {"TranslatePlaneOrLine", 2}, {"TranslateLineOrPlane", 3}, {"TranslateViewPlane", 4},
                               {"RotateAxis", 5}, {"RotateTrackball", 6}, {"Scriptable", 7}, {"BestForDevice", 8}}},
    {"DragDetectorResponseStyle", {{"Geometric", 0}, {"Physical", 1}, {"Custom", 2}}},
    {"DragDetectorPermissionPolicy", {{"Nobody", 0}, {"Everybody", 1}, {"Scriptable", 2}}},
    {"WrapLayerAutoSkin", {{"Disabled", 0}, {"EnabledPreserve", 1}, {"EnabledOverride", 2}}},
    // Workspace's engine switches: Roblox's three-way rollout flags
    {"FluidForces", {{"Default", 0}, {"Experimental", 1}}},
    {"PhysicsSteppingMethod", {{"Default", 0}, {"Fixed", 1}, {"Adaptive", 2}}},
    {"SignalBehavior", {{"Default", 0}, {"Immediate", 1}, {"Deferred", 2}, {"AncestryDeferred", 3}}},
    {"StreamingIntegrityMode", {{"Default", 0}, {"Disabled", 1}, {"MinimumRadiusPause", 2}, {"PauseOutsideLoadedArea", 3}}},
    {"StreamOutBehavior", {{"Default", 0}, {"LowMemory", 1}, {"Opportunistic", 2}}},
    {"ModelStreamingBehavior", {{"Default", 0}, {"Legacy", 1}, {"Improved", 2}}},
    {"ClientAnimatorThrottlingMode", {{"Default", 0}, {"Disabled", 1}, {"Enabled", 2}}},
    {"AvatarUnificationMode", {{"Default", 0}, {"Disabled", 1}, {"Enabled", 2}}},
    {"MeshPartHeadsAndAccessories", {{"Default", 0}, {"Disabled", 1}, {"Enabled", 2}}},
    {"RejectCharacterDeletions", {{"Default", 0}, {"Disabled", 1}, {"Enabled", 2}}},
    {"ReplicateInstanceDestroySetting", {{"Default", 0}, {"Disabled", 1}, {"Enabled", 2}}},
    {"SandboxedInstanceMode", {{"Default", 0}, {"Experimental", 1}}},
    {"PlayerCharacterDestroyBehavior", {{"Default", 0}, {"Disabled", 1}, {"Enabled", 2}}},
    {"IKControlConstraintSupport", {{"Default", 0}, {"Disabled", 1}, {"Enabled", 2}}},
    {"PrimalPhysicsSolver", {{"Default", 0}, {"Disabled", 1}, {"Experimental", 2}}},
    {"RenderingCacheOptimizationMode", {{"Default", 0}, {"Disabled", 1}, {"Enabled", 2}}},
    {"PathfindingUseImprovedSearch", {{"Default", 0}, {"Disabled", 1}, {"Enabled", 2}}},
    {"AnimatorRetargetingMode", {{"Default", 0}, {"Disabled", 1}, {"Enabled", 2}}},
    // the gui family
    {"GuiState", {{"Idle", 0}, {"Hover", 1}, {"Press", 2}, {"NonInteractable", 3}}},
    {"ModifierKey", {{"Alt", 0}, {"Ctrl", 1}, {"Meta", 2}, {"Shift", 3}}},
    {"PreferredInput", {{"KeyboardAndMouse", 0}, {"Gamepad", 1}, {"Touch", 2}}},
    {"PreferredTextSize", {{"Medium", 0}, {"Large", 1}, {"Larger", 2}, {"Largest", 3}}},
    {"SafeAreaCompatibility", {{"None", 0}, {"FullscreenExtension", 1}}},
    {"ScreenInsets", {{"None", 0}, {"DeviceSafeInsets", 1}, {"CoreUISafeInsets", 2}, {"TopbarSafeInsets", 3}}},
    {"VirtualCursorMode", {{"Default", 0}, {"Disabled", 1}, {"Enabled", 2}}},
    {"RtlTextSupport", {{"Default", 0}, {"Disabled", 1}, {"Enabled", 2}}},
    {"HandlesStyle", {{"Resize", 0}, {"Movement", 1}}},
    {"UIFlexMode", {{"None", 0}, {"Fill", 1}, {"Grow", 2}, {"Shrink", 3}, {"Custom", 4}}},
    {"UIFlexAlignment", {{"None", 0}, {"Fill", 1}, {"SpaceAround", 2}, {"SpaceBetween", 3}, {"SpaceEvenly", 4}}},
    {"ItemLineAlignment", {{"Automatic", 0}, {"Start", 1}, {"Center", 2}, {"End", 3}, {"Stretch", 4}}},
    {"TableMajorAxis", {{"RowMajor", 0}, {"ColumnMajor", 1}}},
    {"UIDragDetectorDragStyle", {{"TranslateLine", 0}, {"TranslatePlane", 1}, {"Rotate", 2}, {"Scriptable", 3}}},
    {"UIDragDetectorDragRelativity", {{"Absolute", 0}, {"Relative", 1}}},
    {"UIDragDetectorDragSpace", {{"Parent", 0}, {"LayerCollector", 1}, {"Screen", 2}}},
    {"UIDragDetectorResponseStyle", {{"Offset", 0}, {"Scale", 1}, {"CustomOffset", 2}, {"CustomScale", 3}}},
    {"UIDragDetectorBoundingBehavior", {{"Automatic", 0}, {"EntireObject", 1}, {"HitPoint", 2}}},
    {"UIDragSpeedAxisMapping", {{"XY", 0}, {"XX", 1}, {"YY", 2}}},
    // the services family
    {"SortDirection", {{"Ascending", 0}, {"Descending", 1}}},
    {"TeleportResult", {{"Success", 0}, {"Failure", 1}, {"GameNotFound", 2}, {"GameEnded", 3}, {"GameFull", 4}, {"Unauthorized", 5}, {"Flooded", 6}, {"IsTeleporting", 7}}},
    {"UserCFrame", {{"Head", 0}, {"LeftHand", 1}, {"RightHand", 2}, {"Floor", 3}}},
    {"VRTouchpad", {{"Left", 0}, {"Right", 1}}},
    {"VRTouchpadMode", {{"Touch", 0}, {"VirtualThumbstick", 1}, {"ABXY", 2}}},
    {"VRScaling", {{"World", 0}, {"Off", 1}}},
    {"VRControllerModelMode", {{"Disabled", 0}, {"Transparent", 1}}},
    {"VRLaserPointerMode", {{"Disabled", 0}, {"Pointer", 1}, {"DualPointer", 2}, {"Hidden", 3}}},
    {"VolumetricAudio", {{"Disabled", 0}, {"Automatic", 1}, {"Enabled", 2}}},
    {"AudioApiRollout", {{"Default", 0}, {"Disabled", 1}, {"Enabled", 2}}},
    {"DeveloperMemoryTag", {{"Internal", 0}, {"HttpCache", 1}, {"Instances", 2}, {"Signals", 3}, {"LuaHeap", 4}, {"Script", 5}, {"PhysicsCollision", 6},
                            {"PhysicsParts", 7}, {"GraphicsSolidModels", 8}, {"GraphicsMeshParts", 9}, {"GraphicsParticles", 10}, {"GraphicsParts", 11},
                            {"GraphicsSpatialHash", 12}, {"GraphicsTerrain", 13}, {"GraphicsTexture", 14}, {"GraphicsTextureCharacter", 15}, {"Sounds", 16},
                            {"StreamingSounds", 17}, {"TerrainVoxels", 18}, {"Gui", 19}, {"Animation", 20}, {"Navigation", 21}, {"GeometryCSG", 22}}},
    // The catalog's asset kinds AvatarEditorService:GetAccessoryType maps onto Enum.AccessoryType.
    {"AvatarAssetType", {{"Image", 1}, {"TShirt", 2}, {"Audio", 3}, {"Mesh", 4}, {"Lua", 5}, {"Hat", 8}, {"Place", 9}, {"Model", 10}, {"Shirt", 11}, {"Pants", 12},
                         {"Decal", 13}, {"Head", 17}, {"Face", 18}, {"Gear", 19}, {"Badge", 21}, {"Animation", 24}, {"Torso", 27}, {"RightArm", 28},
                         {"LeftArm", 29}, {"LeftLeg", 30}, {"RightLeg", 31}, {"Package", 32}, {"GamePass", 34}, {"Plugin", 38}, {"MeshPart", 40},
                         {"HairAccessory", 41}, {"FaceAccessory", 42}, {"NeckAccessory", 43}, {"ShoulderAccessory", 44}, {"FrontAccessory", 45},
                         {"BackAccessory", 46}, {"WaistAccessory", 47}, {"ClimbAnimation", 48}, {"DeathAnimation", 49}, {"FallAnimation", 50},
                         {"IdleAnimation", 51}, {"JumpAnimation", 52}, {"RunAnimation", 53}, {"SwimAnimation", 54}, {"WalkAnimation", 55},
                         {"PoseAnimation", 56}, {"EarAccessory", 57}, {"EyeAccessory", 58}, {"EmoteAnimation", 61}, {"Video", 62},
                         {"TShirtAccessory", 64}, {"ShirtAccessory", 65}, {"PantsAccessory", 66}, {"JacketAccessory", 67}, {"SweaterAccessory", 68},
                         {"ShortsAccessory", 69}, {"LeftShoeAccessory", 70}, {"RightShoeAccessory", 71}, {"DressSkirtAccessory", 72},
                         {"EyebrowAccessory", 76}, {"EyelashAccessory", 77}, {"MoodAnimation", 78}, {"DynamicHead", 79}}},
    // the rest family
    {"SensorMode", {{"Floor", 0}, {"Ladder", 1}, {"ClassicFloor", 2}, {"ClassicLadder", 3}}},
    {"SensorUpdateType", {{"OnRead", 0}, {"Manual", 1}}},
    {"IKControlType", {{"Transform", 0}, {"Position", 1}, {"Rotation", 2}, {"LookAt", 3}}},
    {"RotationOrder", {{"XYZ", 0}, {"XZY", 1}, {"YZX", 2}, {"YXZ", 3}, {"ZXY", 4}, {"ZYX", 5}}},
    {"InputActionType", {{"Bool", 0}, {"Direction1D", 1}, {"Direction2D", 2}, {"Direction3D", 3}, {"ViewportPosition", 4}}},
    {"InputBindingType", {{"Automatic", 0}, {"Scriptable", 1}}},
    {"HapticEffectType", {{"Custom", 0}, {"UIHover", 1}, {"UIClick", 2}, {"UINotification", 3}, {"GameplayExplosion", 4}, {"GameplayCollision", 5}}},
    {"AdUnitStatus", {{"Inactive", 0}, {"Active", 1}}},
    {"CaptureType", {{"Screenshot", 1}, {"Video", 2}}},
    {"ConfigSnapshotErrorState", {{"None", 0}, {"LoadFailed", 1}}},
    {"WebStreamClientState", {{"Connecting", 0}, {"Open", 1}, {"Error", 2}, {"Closed", 3}}},
    {"VideoSampleSize", {{"Small", 0}, {"Medium", 1}, {"Large", 2}, {"Full", 3}}},
    {"PackagePermission", {{"None", 0}, {"NoAccess", 1}, {"Revoked", 2}, {"UseView", 3}, {"Edit", 4}, {"Own", 5}}},
    {"MakeupType", {{"Face", 0}, {"Lip", 1}, {"Eye", 2}}},
    {"DigitsRigDescriptionSide", {{"None", 0}, {"Left", 1}, {"Right", 2}}},
    {"AnimationNodeType", {{"InvalidNode", 0}, {"AddNode", 1}, {"OverNode", 2}, {"Blend1DNode", 3}, {"Blend2DNode", 4}, {"ClipNode", 5}, {"GraphOutput", 6},
                           {"MaskNode", 7}, {"PrioritySelectNode", 8}, {"RandomSequenceNode", 9}, {"SelectNode", 10}, {"SequenceNode", 11}, {"SpeedNode", 12},
                           {"SubtractNode", 13}, {"OneShotNode", 14}, {"StateMachineNode", 16}}},
    {"CompositeValueCurveType", {{"ColorRGB", 0}, {"ColorHSV", 1}, {"NumberRange", 2}, {"Rect", 3}, {"UDim", 4}, {"UDim2", 5}, {"Vector2", 6}, {"Vector3", 7}}},
    {"QualityLevel", {{"Automatic", 0}, {"Level01", 1}, {"Level02", 2}, {"Level03", 3}, {"Level04", 4}, {"Level05", 5}, {"Level06", 6}, {"Level07", 7},
                      {"Level08", 8}, {"Level09", 9}, {"Level10", 10}, {"Level11", 11}, {"Level12", 12}, {"Level13", 13}, {"Level14", 14}, {"Level15", 15},
                      {"Level16", 16}, {"Level17", 17}, {"Level18", 18}, {"Level19", 19}, {"Level20", 20}, {"Level21", 21}}},
    {"FramerateManagerMode", {{"Automatic", 0}, {"On", 1}, {"Off", 2}}},
    {"GraphicsMode", {{"Automatic", 1}, {"Direct3D11", 2}, {"OpenGL", 4}, {"Metal", 5}, {"Vulkan", 6}, {"NoGraphics", 9}}},
    {"MeshPartDetailLevel", {{"DistanceBased", 0}, {"Level00", 1}, {"Level01", 2}, {"Level02", 3}, {"Level03", 4}, {"Level04", 5}, {"Level05", 6},
                             {"Level06", 7}, {"Level07", 8}, {"Level08", 9}, {"Level09", 10}}},
    {"ViewMode", {{"None", 0}, {"GeometryComplexity", 1}, {"Transparent", 2}, {"Decal", 3}}},
    {"TickCountSampleMethod", {{"Fast", 0}, {"Benchmark", 1}, {"Precise", 2}}},
    {"ThreadPoolConfig", {{"Auto", 0}, {"Threads1", 1}, {"Threads2", 2}, {"Threads3", 3}, {"Threads4", 4}, {"Threads8", 8}, {"Threads16", 16},
                          {"PerCore1", 101}, {"PerCore2", 102}, {"PerCore3", 103}, {"PerCore4", 104}}},
    {"EnviromentalPhysicsThrottle", {{"DefaultAuto", 0}, {"Disabled", 1}, {"Always", 2}, {"Skip2", 3}, {"Skip4", 4}, {"Skip8", 5}, {"Skip16", 6}}},
    {"StudioCaptureScreenshotFormat", {{"RGBA8", 0}, {"PNG", 1}}},
    {"StudioCaptureBufferStatus", {{"NotStarted", 0}, {"Pending", 1}, {"Ready", 2}, {"Error", 3}}},
    {"UICaptureMode", {{"All", 0}, {"None", 1}}},
    {"DraggerCoordinateSpace", {{"Object", 0}, {"World", 1}}},
    {"DraggerMovementMode", {{"Geometric", 0}, {"Physical", 1}}},
    {"SolverConvergenceMetricType", {{"IterationBased", 0}, {"AlgorithmAgnostic", 1}}},
    {"SolverConvergenceVisualizationMode", {{"Disabled", 0}, {"PerIsland", 1}, {"PerEdge", 2}}},
    {"Button", {{"Jump", 32}, {"Dismount", 8}}},
    {"HashAlgorithm", {{"Blake2b", 0}, {"Blake3", 1}, {"Md5", 2}, {"Sha1", 3}, {"Sha256", 4}}},
    {"CompressionAlgorithm", {{"Zstd", 0}}},
};

const EnumDef* findEnum(const std::string& name) {
    for (auto& e : g_enums) if (name == e.name) return &e;
    return nullptr;
}
const std::vector<EnumDef>& allEnums() { return g_enums; }

// ---- classes -----------------------------------------------------------------------
const PropDef* ClassDef::findProp(const std::string& n) const {
    for (const ClassDef* c = this; c; c = c->base)
        for (auto& p : c->props) if (p.name == n) return &p;
    return nullptr;
}
bool ClassDef::hasEvent(const std::string& n) const {
    for (const ClassDef* c = this; c; c = c->base)
        for (auto& e : c->events) if (e == n) return true;
    return false;
}
bool ClassDef::hasCallback(const std::string& n) const {
    for (const ClassDef* c = this; c; c = c->base)
        for (auto& e : c->callbacks) if (e == n) return true;
    return false;
}
bool ClassDef::isA(const std::string& className) const {
    for (const ClassDef* c = this; c; c = c->base) if (c->name == className) return true;
    return false;
}
void ClassDef::collectProps(std::vector<const PropDef*>& out) const {
    if (base) base->collectProps(out);
    for (auto& p : props) out.push_back(&p);
}

namespace {

struct Registry {
    std::vector<std::unique_ptr<ClassDef>> owned;
    std::vector<const ClassDef*> all;

    std::unordered_map<std::string, const ClassDef*> byName;
    const ClassDef* find(const std::string& name) const { auto it = byName.find(name); return it == byName.end() ? nullptr : it->second; }
    ClassDef& add(const char* name, const char* base, bool creatable = true, bool service = false) {
        auto c = std::make_unique<ClassDef>();
        c->name = name;
        c->base = base ? find(base) : nullptr;   // not findClass(): registry() is still being initialised
        c->creatable = creatable;
        c->service = service;
        ClassDef* raw = c.get();
        owned.push_back(std::move(c));
        all.push_back(raw);
        byName[raw->name] = raw;
        return *raw;
    }
    ClassDef& service(const char* name, const char* base = "Instance") { return add(name, base, false, true); }
};

Registry& registry();

PropDef P(const char* name, Value def, unsigned flags = 0) { return {name, def.type, def, flags, nullptr}; }
PropDef PEnum(const char* name, const char* enumName, const char* item, unsigned flags = 0) {
    const EnumDef* e = findEnum(enumName);
    const EnumItem* i = e ? e->find(item) : nullptr;
    return {name, Value::Enum, Value::enumItem(item, i ? i->value : 0), flags, e};
}
PropDef PRef(const char* name, unsigned flags = 0) { return {name, Value::Ref, Value::instance(0), flags, nullptr}; }
// nil default: the material's own properties.
PropDef PPhys(const char* name, unsigned flags = 0) { return {name, Value::PhysProps, Value::nil(), flags, nullptr}; }

void build(Registry& r) {
    // Property defaults are Roblox's. Object is the root: EditableImage and EditableMesh are
    // Objects but not Instances -- no Name, no Parent, no place in the tree.
    auto& object = r.add("Object", nullptr, false);
    object.props = {P("ClassName", Value::string(""), ReadOnly)};
    object.events = {"Changed"};
    auto& inst = r.add("Instance", "Object", false);
    inst.props = {P("Name", Value::string("Instance")), P("Archivable", Value::boolean(true))};
    inst.events = {"ChildAdded", "ChildRemoved", "DescendantAdded", "DescendantRemoving", "AncestryChanged",
                   "Destroying", "AttributeChanged"};
    // Sandboxed is recorded, not acted on: nothing here sandboxes a subtree. IsInSandbox reads it up the
    // ancestors (rbx_api.cpp). UniqueId is Roblox's 16-byte id kept as hex, the engine's own to read and write.
    // Capabilities (a SecurityCapabilities) and PredictionMode are not declared: no such datatype and no
    // verified enum here. StyledPropertiesChanged never fires: no StyleSheet applies here.
    inst.props.push_back(P("Sandboxed", Value::boolean(false)));
    inst.props.push_back(P("UniqueId", Value::string(""), NoScriptRead | NoScriptWrite | Hidden));
    inst.events.push_back("StyledPropertiesChanged");
    // Roblox's base of the DataModel and the settings: GetService / FindService (rbx_api.cpp). ServiceAdded
    // and ServiceRemoving fire as a service enters or leaves the DataModel (rbx_runtime.cpp).
    auto& serviceProvider = r.add("ServiceProvider", "Instance", false);
    serviceProvider.events = {"Close", "ServiceAdded", "ServiceRemoving"};
    r.add("GenericSettings", "ServiceProvider", false);

    r.add("Folder", "Instance");
    r.add("Configuration", "Instance");
    // Roblox's base of Model and BasePart: GetPivot / PivotTo (rbx_api.cpp). Its Origin and
    // "Pivot Offset" are NotScriptable on Roblox and are not declared.
    auto& pv = r.add("PVInstance", "Instance", false);
    auto& model = r.add("Model", "Instance");
    model.base = &pv;
    // ScaleFactor is what Model:GetScale answers and Model:ScaleTo changes.
    model.props = {PRef("PrimaryPart"), P("ScaleFactor", Value::number(1), Hidden),
                   // Model.WorldPivot with no PrimaryPart: held here once a script writes it (WorldPivotSet),
                   // else the pivot is the first part's, as GetPivot has always answered.
                   P("WorldPivotPosition", Value::vector3(0, 0, 0), Hidden), P("WorldPivotOrientation", Value::vector3(0, 0, 0), Hidden),
                   P("WorldPivotSet", Value::boolean(false), Hidden),
                   PEnum("LevelOfDetail", "ModelLevelOfDetail", "Automatic", PluginWrite),   // recorded, not acted on: nothing here streams meshes
                   PEnum("ModelStreamingMode", "ModelStreamingMode", "Default"),              // recorded, not acted on: nothing here streams
                   // AddPersistentPlayer / GetPersistentPlayers / RemovePersistentPlayer keep UserIds here; nothing streams
                   P("PersistentPlayers", Value::string(""), Hidden | NoReplicate)};

    auto& bp = r.add("BasePart", "Instance", false);
    bp.base = &pv;
    bp.props = {P("Position", Value::vector3(0, 0, 0)),
                P("Orientation", Value::vector3(0, 0, 0)),          // degrees, applied Y-X-Z like Roblox
                P("Size", Value::vector3(4, 1.2f, 2)),
                P("Color", Value::color3(163 / 255.f, 162 / 255.f, 165 / 255.f)),
                P("Transparency", Value::number(0)),
                // legacy surface motors: a Motor face with SurfaceInput Constant spins at ParamB rad/s
                P("RightParamA", Value::number(-0.5)), P("RightParamB", Value::number(0.5)), PEnum("RightSurfaceInput", "InputType", "NoInput"),
                P("LeftParamA", Value::number(-0.5)), P("LeftParamB", Value::number(0.5)), PEnum("LeftSurfaceInput", "InputType", "NoInput"),
                P("TopParamA", Value::number(-0.5)), P("TopParamB", Value::number(0.5)), PEnum("TopSurfaceInput", "InputType", "NoInput"),
                P("BottomParamA", Value::number(-0.5)), P("BottomParamB", Value::number(0.5)), PEnum("BottomSurfaceInput", "InputType", "NoInput"),
                P("BackParamA", Value::number(-0.5)), P("BackParamB", Value::number(0.5)), PEnum("BackSurfaceInput", "InputType", "NoInput"),
                P("FrontParamA", Value::number(-0.5)), P("FrontParamB", Value::number(0.5)), PEnum("FrontSurfaceInput", "InputType", "NoInput"),
                P("Reflectance", Value::number(0)),
                P("Anchored", Value::boolean(false)),
                P("CanCollide", Value::boolean(true)),
                P("CanTouch", Value::boolean(true)),
                P("CanQuery", Value::boolean(true)),
                P("CastShadow", Value::boolean(true)),
                // sound occlusion; stored, not simulated -- the audio API owes the behaviour
                // (OPEN.md item 8). Solid modelling copies it onto a result part (kSolidLook in rbx_api.cpp).
                P("AudioCanCollide", Value::boolean(true)),
                P("Locked", Value::boolean(false)),
                P("Massless", Value::boolean(false)),
                PEnum("Material", "Material", "Plastic"),
                // names a MaterialVariant under MaterialService
                P("MaterialVariant", Value::string("")),
                // stored and serialized; every face draws smooth
                PEnum("TopSurface", "SurfaceType", "Smooth"), PEnum("BottomSurface", "SurfaceType", "Smooth"),
                PEnum("LeftSurface", "SurfaceType", "Smooth"), PEnum("RightSurface", "SurfaceType", "Smooth"),
                PEnum("FrontSurface", "SurfaceType", "Smooth"), PEnum("BackSurface", "SurfaceType", "Smooth"),
                PPhys("CustomPhysicalProperties"),
                P("AssemblyLinearVelocity", Value::vector3(0, 0, 0)),
                P("AssemblyAngularVelocity", Value::vector3(0, 0, 0)),
                // Which part of a welded assembly is its root: highest wins, before anchoring
                // and before mass (rbx_api.cpp assemblyRoot).
                P("RootPriority", Value::number(0)),
                P("CollisionGroup", Value::string("Default")),
                // PivotOffset (a CFrame, see cframeProp in rbx_api.cpp): GetPivot is CFrame * PivotOffset and PivotTo lands the pivot
                P("PivotOffsetPosition", Value::vector3(0, 0, 0), Hidden), P("PivotOffsetOrientation", Value::vector3(0, 0, 0), Hidden),
                // Recorded, not acted on: the host's physics has no aerodynamics or buoyancy.
                P("EnableFluidForces", Value::boolean(true)),
                // Rendering: the part draws at 1 - (1 - Transparency) * (1 - LocalTransparencyModifier) (pulseblockz_world.cpp)
                P("LocalTransparencyModifier", Value::number(0), NoReplicate),
                // Seconds since the last physics update arrived over the wire: nothing here is owned remotely, so 0.
                P("ReceiveAge", Value::number(0), ReadOnly | NoReplicate),
                // The Studio resize handles' step, Roblox's for a Part. Roblox's per-class values are not verified here.
                P("ResizeIncrement", Value::number(1), ReadOnly)};
    bp.events = {"Touched", "TouchEnded"};

    auto& part = r.add("Part", "BasePart");
    part.props = {PEnum("Shape", "PartType", "Block")};
    // Seating welds the HumanoidRootPart to the seat (SeatWeld) and sets Humanoid.Sit and
    // SeatPart until the character jumps; a VehicleSeat reads its controls into Throttle / Steer.
    auto& seat = r.add("Seat", "Part");
    seat.props = {P("Disabled", Value::boolean(false)), PRef("Occupant", ReadOnly)};
    auto& vseat = r.add("VehicleSeat", "Part");
    vseat.props = {P("Disabled", Value::boolean(false)), PRef("Occupant", ReadOnly), P("MaxSpeed", Value::number(25)), P("Torque", Value::number(100)),
                   P("TurnSpeed", Value::number(1)), P("Throttle", Value::number(0)), P("Steer", Value::number(0)),
                   P("ThrottleFloat", Value::number(0)), P("SteerFloat", Value::number(0)), P("HeadsUpDisplay", Value::boolean(true)),
                   // Recorded, not acted on: no legacy Hinge surfaces are detected here, so it stays 0.
                   P("AreHingesDetected", Value::number(0), ReadOnly)};
    r.add("WedgePart", "BasePart");
    r.add("CornerWedgePart", "BasePart");
    // Drawn as a block by the host; Style is recorded, not acted on: no lattice is drawn.
    r.add("TrussPart", "BasePart").props = {PEnum("Style", "Style", "AlternatingSupports")};
    // Roblox's abstract base for PartOperation and MeshPart; its geometry properties are
    // PluginSecurity to write, so Studio decides them.
    auto& tri = r.add("TriangleMeshPart", "BasePart", false);
    tri.props = {PEnum("CollisionFidelity", "CollisionFidelity", "Default", PluginWrite),
                 PEnum("FluidFidelity", "FluidFidelity", "Automatic", PluginWrite),
                 P("MeshSize", Value::vector3(0, 0, 0), ReadOnly | NoReplicate)};
    auto& op = r.add("PartOperation", "TriangleMeshPart", false);
    // MeshData is base64 vertices, normals and indices in the part's own space, scaled to Size; it
    // replicates because a client without it draws nothing. UsePartColor false keeps face colours.
    op.props = {P("MeshData", Value::string(""), Hidden),
                P("Operands", Value::string(""), Hidden | NoReplicate),   // what made it, so Separate can unmake it
                PEnum("RenderFidelity", "RenderFidelity", "Automatic", PluginWrite),
                P("SmoothingAngle", Value::number(0), PluginWrite),
                P("TriangleCount", Value::number(0), ReadOnly),
                P("UsePartColor", Value::boolean(false))};
    r.add("UnionOperation", "PartOperation", false);
    // A part marked to be subtracted: on its own it draws as a ghost and collides with nothing.
    r.add("NegateOperation", "PartOperation", false);
    r.add("IntersectOperation", "PartOperation", false);                   // BasePart:IntersectAsync's result
    // MeshId is a path under the engine's asset root (.obj, .glb, .gltf), fitted to Size with its
    // own bounds in MeshSize. A GeometryService MeshPart has no file: MeshData carries the shape.
    auto& mesh = r.add("MeshPart", "TriangleMeshPart");
    mesh.props = {P("MeshId", Value::string("")), P("TextureID", Value::string("")),
                  PEnum("RenderFidelity", "RenderFidelity", "Automatic"),
                  P("DoubleSided", Value::boolean(false)), P("MeshData", Value::string(""), Hidden),
                  // Recorded, not acted on: no mesh here carries skinning data, so it stays false.
                  P("HasSkinnedMesh", Value::boolean(false), ReadOnly | NoScriptWrite)};
    // Draws instead of its parent part's Shape: a primitive of the part's size times Scale, or a
    // MeshId with MeshType FileMesh. Collision stays the part's.
    auto& dmm = r.add("DataModelMesh", "Instance", false);
    dmm.props = {P("Scale", Value::vector3(1, 1, 1)), P("Offset", Value::vector3(0, 0, 0)), P("VertexColor", Value::vector3(1, 1, 1))};
    auto& fileMesh = r.add("FileMesh", "DataModelMesh", false);
    fileMesh.props = {P("MeshId", Value::string("")), P("TextureId", Value::string(""))};
    r.add("SpecialMesh", "FileMesh").props = {PEnum("MeshType", "MeshType", "Head")};
    auto& bevel = r.add("BevelMesh", "DataModelMesh", false);
    bevel.props = {P("Bevel", Value::number(0)), P("Bulge", Value::number(0)), P("Roundness", Value::number(0))};
    r.add("BlockMesh", "BevelMesh");
    r.add("CylinderMesh", "BevelMesh");
    // In a character, Humanoid:TakeDamage does nothing; SpawnLocation.Duration puts one there.
    r.add("ForceField", "Instance").props = {P("Visible", Value::boolean(true))};
    auto& spawn = r.add("SpawnLocation", "BasePart");
    spawn.props = {P("Enabled", Value::boolean(true)), P("Neutral", Value::boolean(true)), P("Duration", Value::number(10)),
                   P("TeamColor", Value::color3(0xA3 / 255.f, 0xA2 / 255.f, 0xA5 / 255.f), Brick),   // Medium stone grey
                   P("AllowTeamChangeOnTouch", Value::boolean(false))};
    // One per Workspace, made by the runtime.
    auto& terrain = r.add("Terrain", "BasePart", false);
    // Heights is this engine's voxel blob (PBVX, base64), Hidden only so that no panel tries to
    // render tens of kilobytes: with no NoReplicate it still crosses the wire and still round-trips
    // through the place file. SmoothGrid is the place file's Roblox blob, cleared once read into Heights.
    terrain.props = {P("Heights", Value::string(""), Hidden), P("SmoothGrid", Value::string(""), Hidden),
                     P("MaterialColors", Value::string(""), Hidden),   // Roblox's 21 material colours, base64 of its 69-byte blob
                     // the wave properties are stored, not simulated
                     P("WaterColor", Value::color3(12 / 255.f, 84 / 255.f, 91 / 255.f)), P("WaterTransparency", Value::number(0.3)),
                     P("WaterReflectance", Value::number(1)), P("WaterWaveSize", Value::number(0.15)), P("WaterWaveSpeed", Value::number(10)),
                     // Recorded, not acted on: no grass is drawn on the ground here.
                     P("Decoration", Value::boolean(false)), P("GrassLength", Value::number(1))};

    auto& lbs = r.add("LuaSourceContainer", "Instance", false);
    // PluginSecurity to write, as on Roblox: a script that could set a ModuleScript's Source and
    // require() it would run code the published hash does not cover. Host loaders use Instance::set.
    lbs.props = {P("Source", Value::string(""), NoReplicate | Hidden | PluginWrite)};
    auto& bs = r.add("BaseScript", "LuaSourceContainer", false);
    bs.props = {P("Enabled", Value::boolean(true)), P("Disabled", Value::boolean(false))};
    // On BaseScript with PluginSecurity to write, as Roblox's API declares it: a game script
    // may read where it runs, not move itself. shouldRun (rbx_runtime.cpp) reads it on a Script.
    bs.props.push_back(PEnum("RunContext", "RunContext", "Legacy", PluginWrite));
    r.add("Script", "BaseScript");
    r.add("LocalScript", "BaseScript");
    r.add("ModuleScript", "LuaSourceContainer");

    auto& re = r.add("RemoteEvent", "Instance");
    re.events = {"OnServerEvent", "OnClientEvent"};
    // On Roblox these may drop, reorder and cap at 900 bytes; here they travel as a RemoteEvent's.
    r.add("UnreliableRemoteEvent", "RemoteEvent");
    r.add("RemoteFunction", "Instance").callbacks = {"OnServerInvoke", "OnClientInvoke"};
    auto& be = r.add("BindableEvent", "Instance");
    be.events = {"Event"};
    r.add("BindableFunction", "Instance").callbacks = {"OnInvoke"};

    auto& sv = r.add("StringValue", "Instance"); sv.props = {P("Value", Value::string(""))};
    auto& iv = r.add("IntValue", "Instance"); iv.props = {P("Value", Value::number(0))};
    auto& nv = r.add("NumberValue", "Instance"); nv.props = {P("Value", Value::number(0))};
    auto& bv = r.add("BoolValue", "Instance"); bv.props = {P("Value", Value::boolean(false))};
    auto& ov = r.add("ObjectValue", "Instance"); ov.props = {PRef("Value")};
    auto& vv = r.add("Vector3Value", "Instance"); vv.props = {P("Value", Value::vector3(0, 0, 0))};
    auto& cv = r.add("Color3Value", "Instance"); cv.props = {P("Value", Value::color3(0, 0, 0))};
    // CFrameValue.Value is a CFrame (see cframeProp) over this pair.
    r.add("CFrameValue", "Instance").props = {P("ValuePosition", Value::vector3(0, 0, 0)), P("ValueOrientation", Value::vector3(0, 0, 0))};
    // Roblox's base of every value object; the Changed of each carries the new value (rbx_runtime.cpp).
    auto& valueBase = r.add("ValueBase", "Instance", false);
    for (auto& o : r.owned) if (o->name.size() > 5 && o->name.compare(o->name.size() - 5, 5, "Value") == 0 && o->base == &inst) o->base = &valueBase;
    r.add("BinaryStringValue", "ValueBase");   // its Value is NotScriptable on Roblox, so none is declared
    r.add("BrickColorValue", "ValueBase");     // Value: kDataShared, a BrickColor over a Color3
    // RayValue.Value is a Ray over this pair (rbx_api.cpp); Changed fires once with the whole Ray.
    r.add("RayValue", "ValueBase").props = {P("ValueOrigin", Value::vector3(0, 0, 0)), P("ValueDirection", Value::vector3(0, 0, 0))};
    // An Actor's messages: BindToMessage / SendMessage (rbx_api.cpp). One VM here: BindToMessageParallel
    // binds on the same thread, and a bound callback runs on the next deferred pass.
    r.add("Actor", "Model");

    auto& players = r.service("Players");
    players.props = {PRef("LocalPlayer", ReadOnly), P("MaxPlayers", Value::number(50)), P("RespawnTime", Value::number(5)),
                     P("CharacterAutoLoads", Value::boolean(true))};
    players.events = {"PlayerAdded", "PlayerRemoving"};
    // BubbleChat and ClassicChat are what SetChatStyle sets; the chat drawn here follows
    // TextChatService, so they are recorded and not acted on. BanningEnabled and
    // UseStrafingAnimations are recorded and not acted on: nothing here bans or strafes.
    // PreferredPlayers' default is unverified; the type's zero.
    players.props.insert(players.props.end(), {P("BanningEnabled", Value::boolean(true)), P("BubbleChat", Value::boolean(false), ReadOnly),
                                               P("ClassicChat", Value::boolean(true), ReadOnly), P("PreferredPlayers", Value::number(0)),
                                               P("UseStrafingAnimations", Value::boolean(false))});
    players.events.insert(players.events.end(), {"PlayerMembershipChanged", "UserSubscriptionStatusChanged"});   // never fired: no memberships here
    auto& player = r.add("Player", "Instance", false);
    player.props = {P("UserId", Value::number(0), ReadOnly), P("DisplayName", Value::string("")),
                    PRef("Character"), PRef("Team"), P("TeamColor", Value::color3(0xF2 / 255.f, 0xF3 / 255.f, 0xF3 / 255.f), Brick),   // White
                    P("Neutral", Value::boolean(true)), PRef("RespawnLocation"), P("CharacterAppearanceId", Value::number(0)),
                    // Whether this player's Roblox avatar is worn. Nothing here fetches one, so
                    // the engine's behaviour is already the false one; the value is kept so a
                    // place that turns it off reads back what it set.
                    P("CanLoadCharacterAppearance", Value::boolean(true)),
                    // seeded from StarterPlayer's; Scriptable turns the default controls off for
                    // the place's own control script
                    PEnum("DevComputerMovementMode", "DevComputerMovementMode", "UserChoice", ServerWrite)};
    player.events = {"CharacterAdded", "CharacterRemoving", "Chatted"};
    // Seeded from StarterPlayer's at join (rbx_runtime.cpp addPlayer), as on Roblox. The
    // camera reads CameraMinZoomDistance and CameraMaxZoomDistance (pulseblockz_world.cpp
    // camera_input; the engine's own 128-stud ceiling stays under the 400 default). The rest
    // are recorded and not acted on: the camera and control modes here are the engine's own,
    // AutoJumpEnabled is for touch controls there are none of, and ReplicationFocus is for
    // streaming there is none of.
    player.props.insert(player.props.end(), {
        P("AutoJumpEnabled", Value::boolean(true)), P("CameraMaxZoomDistance", Value::number(400)), P("CameraMinZoomDistance", Value::number(0.5)),
        PEnum("CameraMode", "CameraMode", "Classic"), PEnum("DevCameraOcclusionMode", "DevCameraOcclusionMode", "Zoom"),
        PEnum("DevComputerCameraMode", "DevComputerCameraMovementMode", "UserChoice"), P("DevEnableMouseLock", Value::boolean(true)),
        PEnum("DevTouchCameraMode", "DevTouchCameraMovementMode", "UserChoice"), PEnum("DevTouchMovementMode", "DevTouchMovementMode", "UserChoice"),
        PRef("ReplicationFocus"),
        // Account facts. No account here has an age, a badge or a follow; SetAccountAge writes AccountAge.
        P("AccountAge", Value::number(0), ReadOnly), P("HasVerifiedBadge", Value::boolean(false), ReadOnly), P("FollowUserId", Value::number(0), ReadOnly),
        P("LocaleId", Value::string("en-us"), ReadOnly),
        // Roblox's own: an age check, a client's input lag and the engine's pause; no script reads or sets them.
        P("AgeChecked", Value::boolean(false), NoScriptRead | NoScriptWrite), P("InputLatency", Value::number(0), NoScriptRead | NoScriptWrite),
        P("GameplayPaused", Value::boolean(false), NoScriptWrite)});
    // CharacterAppearanceLoaded fires right after CharacterAdded: there is no avatar to fetch.
    // Idled and OnTeleport are declared and never fired: nothing here counts idle time or teleports.
    player.events.insert(player.events.end(), {"CharacterAppearanceLoaded", "Idled", "OnTeleport"});
    // UserSettings() sits outside the game tree; UserGameSettings is the one service it gives,
    // SavedQualityLevel is the menu's Graphics Quality, and RotationType turns the client's
    // character to face the way it walks (MovementRelative) or the way the camera looks
    // (CameraRelative, what shift lock sets). The classic Mouse's Hit, Target, UnitRay
    // and Origin are computed from the camera on read, so they are not properties here.
    r.add("UserSettings", "GenericSettings", false);
    auto& ugs = r.add("UserGameSettings", "Instance", false);
    ugs.props = {PEnum("RotationType", "RotationType", "MovementRelative"), PEnum("SavedQualityLevel", "SavedQualitySetting", "Automatic", ReadOnly)};
    // The client's own settings. Recorded, not acted on: nothing here reads a sensitivity or a movement
    // mode from them; the character and camera follow the Player's Dev* modes. The RobloxScriptSecurity
    // ones are the engine's own, unreadable to a script. The volumes' and GraphicsQualityLevel's Roblox
    // defaults are not verified here (their zero); the rest are Roblox's.
    ugs.props.insert(ugs.props.end(), {
        P("MouseSensitivity", Value::number(1)), P("GamepadCameraSensitivity", Value::number(1)),
        PEnum("ComputerCameraMovementMode", "ComputerCameraMovementMode", "Default"), PEnum("ComputerMovementMode", "ComputerMovementMode", "Default"),
        PEnum("TouchCameraMovementMode", "TouchCameraMovementMode", "Default"), PEnum("TouchMovementMode", "TouchMovementMode", "Default"),
        P("RCCProfilerRecordFrameRate", Value::number(0)), P("RCCProfilerRecordTimeFrame", Value::number(0)),
        P("AllTutorialsDisabled", Value::boolean(false), NoScriptRead | NoScriptWrite), P("BadgeVisible", Value::boolean(true), NoScriptRead | NoScriptWrite),
        PEnum("CameraMode", "CustomCameraMode", "Default", NoScriptRead | NoScriptWrite), P("ChatVisible", Value::boolean(true), NoScriptRead | NoScriptWrite),
        P("Fullscreen", Value::boolean(false), NoScriptRead | NoScriptWrite), P("GraphicsQualityLevel", Value::number(0), NoScriptRead | NoScriptWrite),
        P("HasEverUsedVR", Value::boolean(false), NoScriptRead | NoScriptWrite), P("MasterVolume", Value::number(0), NoScriptRead | NoScriptWrite),
        P("MasterVolumeStudio", Value::number(0), NoScriptRead | NoScriptWrite), P("MaxQualityEnabled", Value::boolean(false), NoScriptRead | NoScriptWrite),
        P("OnboardingsCompleted", Value::string(""), NoScriptRead | NoScriptWrite), P("PartyVoiceVolume", Value::number(0), NoScriptRead | NoScriptWrite),
        P("PlayerListVisible", Value::boolean(true), NoScriptRead | NoScriptWrite), P("PlayerNamesEnabled", Value::boolean(true), NoScriptRead | NoScriptWrite),
        P("StartMaximized", Value::boolean(false), NoScriptRead | NoScriptWrite), P("StartScreenPosition", Value::vector2(0, 0), NoScriptRead | NoScriptWrite),
        P("StartScreenSize", Value::vector2(0, 0), NoScriptRead | NoScriptWrite), P("UsedCoreGuiIsVisibleToggle", Value::boolean(false), NoScriptRead | NoScriptWrite),
        P("UsedCustomGuiIsVisibleToggle", Value::boolean(false), NoScriptRead | NoScriptWrite), P("UsedHideHudShortcut", Value::boolean(false), NoScriptRead | NoScriptWrite),
        P("VignetteEnabled", Value::boolean(false), NoScriptRead | NoScriptWrite), P("VoiceChatVolume", Value::number(0), NoScriptRead | NoScriptWrite),
        P("VREnabled", Value::boolean(false), NoScriptRead | NoScriptWrite), P("VRRotationIntensity", Value::number(0), NoScriptRead | NoScriptWrite),
        P("VRSmoothRotationEnabled", Value::boolean(false), NoScriptRead | NoScriptWrite)});
    // GraphicsOptimizationMode and PeoplePageLayout are not declared: their enums are not verified here.
    // Neither event fires: nothing here changes the window or leaves Studio mode while running.
    ugs.events = {"FullscreenChanged", "StudioModeChanged"};
    auto& mouse = r.add("Mouse", "Instance", false);
    mouse.props = {P("X", Value::number(0), ReadOnly), P("Y", Value::number(0), ReadOnly),
                   P("ViewSizeX", Value::number(0), ReadOnly), P("ViewSizeY", Value::number(0), ReadOnly),
                   P("Icon", Value::string("")), PRef("TargetFilter")};
    mouse.events = {"Button1Down", "Button1Up", "Button2Down", "Button2Up", "Move", "WheelForward", "WheelBackward", "Idle"};
    // MaxActivationDistance is to the character; MouseClick(player) fires on the clicking client and the server.
    auto& cd = r.add("ClickDetector", "Instance");
    cd.props = {P("MaxActivationDistance", Value::number(32)), P("CursorIcon", Value::string(""))};
    cd.events = {"MouseClick", "RightMouseClick", "MouseHoverEnter", "MouseHoverLeave"};
    // Recorded, not acted on: nothing here drags a part with the pointer, so DragStart, DragContinue
    // and DragEnd never fire and the drag-style, constraint and permission functions are not answered.
    // Enabled and Orientation are not declared. DragFrame is a CFrame (cframeProp in rbx_api.cpp).
    auto& dd = r.add("DragDetector", "ClickDetector");
    dd.props = {P("ActivatedCursorIcon", Value::string("")), P("ApplyAtCenterOfMass", Value::boolean(false)),
                P("Axis", Value::vector3(1, 0, 0)), P("DragFramePosition", Value::vector3(0, 0, 0), Hidden), P("DragFrameOrientation", Value::vector3(0, 0, 0), Hidden),
                PEnum("DragStyle", "DragDetectorDragStyle", "TranslateViewPlane"),
                PEnum("GamepadModeSwitchKeyCode", "KeyCode", "ButtonR3"), PEnum("KeyboardModeSwitchKeyCode", "KeyCode", "LeftControl"),
                PEnum("VRSwitchKeyCode", "KeyCode", "ButtonR3"),
                P("MaxDragAngle", Value::number(0)), P("MinDragAngle", Value::number(0)), P("MaxDragTranslation", Value::vector3(0, 0, 0)), P("MinDragTranslation", Value::vector3(0, 0, 0)),
                P("MaxForce", Value::number(1e7)), P("MaxTorque", Value::number(1e7)),
                PEnum("PermissionPolicy", "DragDetectorPermissionPolicy", "Everybody"), PRef("ReferenceInstance"),
                PEnum("ResponseStyle", "DragDetectorResponseStyle", "Geometric"), P("Responsiveness", Value::number(10)),
                P("RunLocally", Value::boolean(false)), P("SecondaryAxis", Value::vector3(0, 1, 0)),
                P("TrackballRadialPullFactor", Value::number(1)), P("TrackballRollFactor", Value::number(1))};
    dd.events = {"DragContinue", "DragEnd", "DragStart"};
    // The engine draws the prompt and writes Shown; a press fires Triggered(player) on the client
    // and, re-checked, on the server (Runtime::Impl::updatePrompts / promptPress).
    auto& pp = r.add("ProximityPrompt", "Instance");
    pp.props = {P("ActionText", Value::string("")), P("ObjectText", Value::string("")),
                PEnum("KeyboardKeyCode", "KeyCode", "E"), P("MaxActivationDistance", Value::number(10)),
                P("HoldDuration", Value::number(0)), P("Enabled", Value::boolean(true)),
                P("RequiresLineOfSight", Value::boolean(true)), P("ClickablePrompt", Value::boolean(true)),
                PEnum("Exclusivity", "ProximityPromptExclusivity", "OnePerButton"),
                PEnum("Style", "ProximityPromptStyle", "Default"), P("UIOffset", Value::vector2(0, 0)),
                P("Shown", Value::boolean(false), ReadOnly | NoReplicate | Hidden)};
    pp.events = {"Triggered", "TriggerEnded", "PromptShown", "PromptHidden", "PromptButtonHoldBegan", "PromptButtonHoldEnded"};
    // Recorded, not acted on: the prompt's text is not localized, no gamepad presses it, and nothing
    // here draws the far-off indicator, so IndicatorShown / IndicatorHidden never fire.
    pp.props.push_back(P("AutoLocalize", Value::boolean(true)));
    pp.props.push_back(PEnum("GamepadKeyCode", "KeyCode", "ButtonX"));
    pp.props.push_back(PRef("RootLocalizationTable"));
    pp.events.push_back("IndicatorShown");
    pp.events.push_back("IndicatorHidden");
    auto& pps = r.service("ProximityPromptService");
    pps.props = {P("Enabled", Value::boolean(true)), P("MaxPromptsVisible", Value::number(1))};
    pps.events = {"PromptTriggered", "PromptTriggerEnded", "PromptShown", "PromptHidden", "PromptButtonHoldBegan", "PromptButtonHoldEnded"};
    // Recorded, not acted on: nothing here draws an off-screen prompt's indicator, so the two never
    // fire. MaxIndicatorsVisible's Roblox default is not verified here: the type's zero.
    pps.props.push_back(P("MaxIndicatorsVisible", Value::number(0)));
    pps.events.push_back("IndicatorShown");
    pps.events.push_back("IndicatorHidden");
    r.add("PlayerScripts", "Instance", false);
    // ScreenOrientation is recorded and not acted on (no device turns here); the current one
    // is a desktop's. SelectionImageObject is recorded and not acted on: no gamepad selection here.
    // Roblox's base of PlayerGui, StarterGui and CoreGui: GetGuiObjectsAtPosition (rbx_api.cpp).
    r.add("BasePlayerGui", "Instance", false);
    r.add("PlayerGui", "BasePlayerGui", false).props = {PEnum("ScreenOrientation", "ScreenOrientation", "LandscapeSensor"),
                                                   PEnum("CurrentScreenOrientation", "ScreenOrientation", "LandscapeLeft", ReadOnly),
                                                   PRef("SelectionImageObject")};
    // Shines from the part it is in; Face is the side it points out of.
    auto& lightBase = r.add("Light", "Instance", false);
    lightBase.props = {P("Brightness", Value::number(1)), P("Color", Value::color3(1, 1, 1)), P("Enabled", Value::boolean(true)), P("Shadows", Value::boolean(false))};
    auto& pointLight = r.add("PointLight", "Light");
    pointLight.props = {P("Range", Value::number(8))};
    auto& spotLight = r.add("SpotLight", "Light");
    spotLight.props = {P("Range", Value::number(16)), P("Angle", Value::number(90)), PEnum("Face", "NormalId", "Front")};
    auto& surfaceLight = r.add("SurfaceLight", "Light");
    surfaceLight.props = {P("Range", Value::number(16)), P("Angle", Value::number(90)), PEnum("Face", "NormalId", "Front")};
    // A Decal fits one Face of a part; a Texture tiles it every StudsPerTileU x StudsPerTileV studs.
    auto& faceInstance = r.add("FaceInstance", "Instance", false);
    faceInstance.props = {PEnum("Face", "NormalId", "Front")};
    auto& decal = r.add("Decal", "FaceInstance");
    decal.props = {P("Texture", Value::string("")), P("Transparency", Value::number(0)), P("Color3", Value::color3(1, 1, 1)), P("ZIndex", Value::number(1)),
                   // Folded into the image's alpha with Transparency, as a part's is (pulseblockz_world.cpp, style_decal).
                   P("LocalTransparencyModifier", Value::number(0), NoReplicate),
                   // The image's uv is uv * UVScale + UVOffset on a Decal's quad; a Texture's tiling is its own StudsPerTile mapping.
                   P("UVOffset", Value::vector2(0, 0)), P("UVScale", Value::vector2(1, 1)),
                   // Recorded, not acted on: no localization table swaps an image here. Roblox's default is
                   // unverified (a GuiBase2d's is true); this is the type's zero. Rotation is not declared.
                   P("AutoLocalize", Value::boolean(false))};
    auto& texture = r.add("Texture", "Decal");
    texture.props = {P("StudsPerTileU", Value::number(2)), P("StudsPerTileV", Value::number(2)), P("OffsetStudsU", Value::number(0)), P("OffsetStudsV", Value::number(0))};
    // EmitBurst is how Emit(n) and Clear() reach the engine: n, then back to 0; -1 for a clear.
    auto& emitter = r.add("ParticleEmitter", "Instance");
    emitter.props = {P("Enabled", Value::boolean(true)), P("Rate", Value::number(20)), P("Lifetime", Value::numberRange(5, 10)),
                     P("Speed", Value::numberRange(5, 5)), P("Color", Value::colorSequence(Col3{1, 1, 1})), P("Size", Value::numberSequence(1, 1)),
                     P("Transparency", Value::numberSequence(0, 0)), P("Squash", Value::numberSequence(0, 0)),
                     P("Texture", Value::string("rbxasset://textures/particles/sparkles_main.dds")),
                     P("LightEmission", Value::number(0)), P("LightInfluence", Value::number(1)), P("Brightness", Value::number(1)),
                     P("Acceleration", Value::vector3(0, 0, 0)), P("Drag", Value::number(0)), P("Rotation", Value::numberRange(0, 0)),
                     P("RotSpeed", Value::numberRange(0, 0)), P("SpreadAngle", Value::vector2(0, 0)), PEnum("EmissionDirection", "NormalId", "Top"),
                     P("LockedToPart", Value::boolean(false)), P("ZOffset", Value::number(0)), P("TimeScale", Value::number(1)),
                     P("VelocityInheritance", Value::number(0)), PEnum("Shape", "ParticleEmitterShape", "Box"),
                     PEnum("ShapeStyle", "ParticleEmitterShapeStyle", "Volume"), PEnum("ShapeInOut", "ParticleEmitterShapeInOut", "Outward"),
                     PEnum("Orientation", "ParticleOrientation", "FacingCamera"), P("EmitBurst", Value::number(0), ReadOnly | Hidden),
                     // A flipbook cuts Texture into a grid and plays it over each particle: the layout is the
                     // grid, Loop and OneShot are the two Godot has (PingPong plays as Loop, Random as Loop from a
                     // random frame), Framerate is frames per second against the lifetime's top, StartRandom a
                     // random first frame. Custom is drawn as no flipbook: what it cuts by is not known here.
                     PEnum("FlipbookLayout", "ParticleFlipbookLayout", "None"), PEnum("FlipbookMode", "ParticleFlipbookMode", "Loop"),
                     P("FlipbookFramerate", Value::numberRange(1, 1)), P("FlipbookStartRandom", Value::boolean(false)),
                     // Recorded, not acted on: frames are not blended, nothing sizes a Custom grid, and no reason is
                     // ever written. Roblox's defaults for the three are unverified; these are the types' zeros.
                     P("FlipbookBlendFrames", Value::boolean(false)), P("FlipbookSizeX", Value::number(0)), P("FlipbookSizeY", Value::number(0)),
                     P("FlipbookIncompatible", Value::string(""), ReadOnly),
                     // Folded into the particles' alpha with Transparency (style_emitter).
                     P("LocalTransparencyModifier", Value::number(0), NoReplicate),
                     // Recorded, not acted on: the emission shape is always the whole shape, and there is no wind.
                     P("ShapePartial", Value::number(1)), P("WindAffectsDrag", Value::boolean(false))};
    // Detonates when parented into the Workspace and removes itself: Hit(part, distance) for each
    // part within BlastRadius, and joints within DestroyJointRadiusPercent of it break --
    // BlastPressure is the impulse thrown at the parts that come loose, and a character whose
    // joints break dies.
    auto& explosion = r.add("Explosion", "Instance");
    explosion.props = {P("BlastPressure", Value::number(500000)), P("BlastRadius", Value::number(4)), P("DestroyJointRadiusPercent", Value::number(1)),
                       PEnum("ExplosionType", "ExplosionType", "Craters"), P("Position", Value::vector3(0, 0, 0)), P("TimeScale", Value::number(1)),
                       P("Visible", Value::boolean(true)),
                       // Recorded, not acted on: the explosion's flash draws at full strength.
                       P("LocalTransparencyModifier", Value::number(0), NoReplicate)};
    explosion.events = {"Hit"};
    // The ribbon the two Attachments have swept over the last Lifetime seconds; the sequences run
    // along its length, time 0 at the attachments.
    r.add("Trail", "Instance").props = {PRef("Attachment0"), PRef("Attachment1"), P("Enabled", Value::boolean(true)),
                                        P("Lifetime", Value::number(2)), P("MinLength", Value::number(0.1)), P("MaxLength", Value::number(0)),
                                        P("Color", Value::colorSequence(Col3{1, 1, 1})), P("Transparency", Value::numberSequence(0.5, 0.5)),
                                        P("WidthScale", Value::numberSequence(1, 1)), P("Texture", Value::string("")), P("TextureLength", Value::number(1)),
                                        PEnum("TextureMode", "TextureMode", "Stretch"), P("LightEmission", Value::number(0)), P("LightInfluence", Value::number(0)),
                                        P("Brightness", Value::number(1)), P("FaceCamera", Value::boolean(false)),
                                        // Folded into the ribbon's alpha with Transparency (step_trails).
                                        P("LocalTransparencyModifier", Value::number(0), NoReplicate),
                                        // Clear() pulses this to 1 and back; the engine drops the ribbon's samples on the 1.
                                        P("ClearTick", Value::number(0), ReadOnly | Hidden)};
    // A ribbon of Segments quads on a cubic Bezier from Attachment0 to Attachment1, bowed
    // CurveSize0 / CurveSize1 along the attachments' X axes and Width0 to Width1 across.
    r.add("Beam", "Instance").props = {PRef("Attachment0"), PRef("Attachment1"), P("Enabled", Value::boolean(true)),
                                       P("Color", Value::colorSequence(Col3{1, 1, 1})), P("Transparency", Value::numberSequence(0.5, 0.5)),
                                       P("Width0", Value::number(1)), P("Width1", Value::number(1)),
                                       P("CurveSize0", Value::number(0)), P("CurveSize1", Value::number(0)), P("Segments", Value::number(10)),
                                       P("FaceCamera", Value::boolean(false)), P("LightEmission", Value::number(0)), P("LightInfluence", Value::number(0)),
                                       P("Texture", Value::string("")), P("TextureLength", Value::number(1)), P("TextureSpeed", Value::number(1)),
                                       PEnum("TextureMode", "TextureMode", "Stretch"), P("ZOffset", Value::number(0)), P("Brightness", Value::number(1)),
                                       // Folded into the ribbon's alpha with Transparency (step_beams).
                                       P("LocalTransparencyModifier", Value::number(0), NoReplicate),
                                       // SetTextureOffset's value, in texture cycles: the scroll TextureSpeed runs restarts from it.
                                       P("TextureOffset", Value::number(0), Hidden | NoReplicate)};
    // A bitmap a script draws into, shown through an ImageLabel's ImageContent; a frame's writes
    // reach the engine as one texture update.
    r.add("EditableImage", "Object", false).props = {P("Size", Value::vector2(512, 512), ReadOnly)};
    // Built and edited in rbx_editable_mesh.cpp, shown through AssetService:CreateMeshPartAsync.
    r.add("EditableMesh", "Object", false).props = {P("FixedSize", Value::boolean(false), ReadOnly)};
    // What AssetService:CreateDataModelContentAsync bakes an editable into, named by an Opaque
    // Content. It does not replicate, and Replicator::sendContentHolder is empty for that reason,
    // not by oversight: a picture a client is to see goes on chain, named by hash. The object
    // lives as long as a Content names it, which is what holdContent in setImpl maintains.
    r.add("DataModelContent", "Object", false).props = {P("Kind", Value::string("Image"), ReadOnly | Hidden),
                                                         P("Size", Value::vector2(0, 0), ReadOnly | Hidden),      // pixels for an image
                                                         P("MeshBounds", Value::vector3(0, 0, 0), ReadOnly | Hidden),   // studs for a mesh
                                                         P("Data", Value::string(""), ReadOnly | Hidden)};
    // Outlines and tints its Adornee (its parent when nil), through walls unless DepthMode is Occluded.
    auto& highlight = r.add("Highlight", "Instance");
    highlight.props = {PRef("Adornee"), PEnum("DepthMode", "HighlightDepthMode", "AlwaysOnTop"), P("Enabled", Value::boolean(true)),
                       P("FillColor", Value::color3(1, 0, 0)), P("FillTransparency", Value::number(0.5)),
                       P("OutlineColor", Value::color3(1, 1, 1)), P("OutlineTransparency", Value::number(0))};
    auto& fire = r.add("Fire", "Instance");
    fire.props = {P("Enabled", Value::boolean(true)), P("Color", Value::color3(236 / 255.f, 139 / 255.f, 70 / 255.f)),
                  P("SecondaryColor", Value::color3(139 / 255.f, 80 / 255.f, 15 / 255.f)), P("Heat", Value::number(9)), P("Size", Value::number(5)),
                  P("TimeScale", Value::number(1)),
                  // Folded into the flames' alpha (style_emitter).
                  P("LocalTransparencyModifier", Value::number(0), NoReplicate)};
    auto& smoke = r.add("Smoke", "Instance");
    smoke.props = {P("Enabled", Value::boolean(true)), P("Color", Value::color3(178 / 255.f, 178 / 255.f, 178 / 255.f)), P("Opacity", Value::number(0.5)),
                   P("RiseVelocity", Value::number(1)), P("Size", Value::number(1)), P("TimeScale", Value::number(1))};
    auto& sparkles = r.add("Sparkles", "Instance");
    sparkles.props = {P("Enabled", Value::boolean(true)), P("SparkleColor", Value::color3(144 / 255.f, 144 / 255.f, 1)), P("TimeScale", Value::number(1))};

    // 2D GUI: StarterGui is copied into each PlayerGui. Sizes and positions are UDim2, a scale of
    // the parent plus pixels; AnchorPoint is the fraction of the object Position lands on.
    auto& guiBase = r.add("GuiBase", "Instance", false);   // Roblox's root of GuiBase2d, GuiBase3d and Path2D
    auto& gb = r.add("GuiBase2d", "Instance", false);
    gb.base = &guiBase;
    gb.props = {P("AbsolutePosition", Value::vector2(0, 0), ReadOnly), P("AbsoluteSize", Value::vector2(0, 0), ReadOnly),
                P("AbsoluteRotation", Value::number(0), ReadOnly)};
    // Recorded, not acted on: nothing here localises text or moves a gamepad selection by these.
    // SelectionChanged fires when GuiService.SelectedObject moves into, out of or within it.
    gb.props.insert(gb.props.end(), {P("AutoLocalize", Value::boolean(true)), PRef("RootLocalizationTable"),
                                     PEnum("SelectionBehaviorDown", "SelectionBehavior", "Escape"), PEnum("SelectionBehaviorLeft", "SelectionBehavior", "Escape"),
                                     PEnum("SelectionBehaviorRight", "SelectionBehavior", "Escape"), PEnum("SelectionBehaviorUp", "SelectionBehavior", "Escape"),
                                     P("SelectionGroup", Value::boolean(false))});
    gb.events = {"SelectionChanged"};
    auto& lc = r.add("LayerCollector", "GuiBase2d", false);
    lc.props = {P("Enabled", Value::boolean(true)), P("ResetOnSpawn", Value::boolean(true)),
                PEnum("ZIndexBehavior", "ZIndexBehavior", "Sibling")};
    auto& sg = r.add("ScreenGui", "LayerCollector");
    sg.props = {P("DisplayOrder", Value::number(0)), P("IgnoreGuiInset", Value::boolean(false))};
    // ScreenInsets reads None while IgnoreGuiInset is true, and writing it sets IgnoreGuiInset
    // (rbx_api.cpp); the topbar gap is the only inset here. The other two are recorded, not acted on.
    sg.props.insert(sg.props.end(), {PEnum("ScreenInsets", "ScreenInsets", "CoreUISafeInsets"), P("ClipToDeviceSafeArea", Value::boolean(true)),
                                     PEnum("SafeAreaCompatibility", "SafeAreaCompatibility", "FullscreenExtension")});
    // Faces the camera over its Adornee (its parent when nil). Size's scale is studs and its offset
    // pixels, StudsOffset is in camera axes and the WorldSpace pair in world axes, ExtentsOffset in
    // halves of the part's size. In a PlayerGui with an Adornee, only that player sees it.
    auto& bb = r.add("BillboardGui", "LayerCollector");
    bb.props = {PRef("Adornee"), P("Size", Value::udim2(1, 0, 1, 0)), P("SizeOffset", Value::vector2(0, 0)),
                P("StudsOffset", Value::vector3(0, 0, 0)), P("StudsOffsetWorldSpace", Value::vector3(0, 0, 0)),
                P("ExtentsOffset", Value::vector3(0, 0, 0)), P("ExtentsOffsetWorldSpace", Value::vector3(0, 0, 0)),
                P("AlwaysOnTop", Value::boolean(false)), P("MaxDistance", Value::number(std::numeric_limits<double>::infinity())),
                P("Active", Value::boolean(false)), P("ClipsDescendants", Value::boolean(false)),
                P("Brightness", Value::number(1)), P("LightInfluence", Value::number(1)),
                P("DistanceLowerLimit", Value::number(0)), P("DistanceUpperLimit", Value::number(-1)),
                P("DistanceStep", Value::number(0)), PRef("PlayerToHideFrom")};
    bb.props.push_back(P("CurrentDistance", Value::number(0), ReadOnly));   // CurrentCamera to the Adornee's position (inst_index)
    // Painted onto one Face of its Adornee (its parent when nil): a CanvasSize-pixel canvas over
    // the face, or PixelsPerStud pixels a stud. The engine routes the pointer into it.
    auto& sgb = r.add("SurfaceGuiBase", "LayerCollector", false);
    sgb.props = {PRef("Adornee"), PEnum("Face", "NormalId", "Front"), P("Active", Value::boolean(false))};
    auto& sfg = r.add("SurfaceGui", "SurfaceGuiBase");
    sfg.props = {P("CanvasSize", Value::vector2(800, 600)), PEnum("SizingMode", "SurfaceGuiSizingMode", "FixedSize"),
                 P("PixelsPerStud", Value::number(50)), P("AlwaysOnTop", Value::boolean(false)),
                 P("Brightness", Value::number(1)), P("LightInfluence", Value::number(0)),
                 P("ClipsDescendants", Value::boolean(false)), P("ZOffset", Value::number(0)),
                 P("MaxDistance", Value::number(0)), P("ToolPunchThroughDistance", Value::number(0))};   // 0: seen from any distance, Roblox's default
    auto& go = r.add("GuiObject", "GuiBase2d", false);
    go.props = {P("Active", Value::boolean(false)), P("AnchorPoint", Value::vector2(0, 0)),
                PEnum("AutomaticSize", "AutomaticSize", "None"),
                P("BackgroundColor3", Value::color3(163 / 255.f, 162 / 255.f, 165 / 255.f)),
                P("BackgroundTransparency", Value::number(0)),
                P("BorderColor3", Value::color3(27 / 255.f, 42 / 255.f, 53 / 255.f)), P("BorderSizePixel", Value::number(1)),
                P("ClipsDescendants", Value::boolean(false)), P("LayoutOrder", Value::number(0)),
                P("Position", Value::udim2(0, 0, 0, 0)), P("Size", Value::udim2(0, 100, 0, 100)), P("Rotation", Value::number(0)),
                P("Selectable", Value::boolean(false)), PEnum("SizeConstraint", "SizeConstraint", "RelativeXY"),
                P("Visible", Value::boolean(true)), P("ZIndex", Value::number(1)), P("Interactable", Value::boolean(true))};
    go.events = {"MouseEnter", "MouseLeave", "MouseMoved", "InputBegan", "InputChanged", "InputEnded"};
    // GuiState follows the pointer (Runtime::guiInput), written silently: no Changed for it.
    // SelectionOrder orders GuiService:Select's pick; the NextSelection refs, SelectionImageObject
    // and BorderMode are recorded, not acted on (no gamepad here; the border is drawn Outline).
    go.props.insert(go.props.end(), {PEnum("GuiState", "GuiState", "Idle", ReadOnly), P("SelectionOrder", Value::number(0)),
                                     PRef("NextSelectionDown"), PRef("NextSelectionLeft"), PRef("NextSelectionRight"), PRef("NextSelectionUp"),
                                     PRef("SelectionImageObject"), PEnum("BorderMode", "BorderMode", "Outline")});
    // The wheel events fire from the engine's pointer; the selection pair from GuiService.SelectedObject;
    // the touch events never fire (no touch screen here).
    go.events.insert(go.events.end(), {"MouseWheelForward", "MouseWheelBackward", "SelectionGained", "SelectionLost",
                                       "TouchTap", "TouchLongPress", "TouchPan", "TouchPinch", "TouchRotate", "TouchSwipe"});
    // Style is recorded, not acted on: every Frame is drawn Custom.
    r.add("Frame", "GuiObject").props = {PEnum("Style", "FrameStyle", "Custom")};
    // GroupTransparency and GroupColor3 modulate the group and everything under it (the engine's node tint).
    r.add("CanvasGroup", "GuiObject").props = {P("GroupColor3", Value::color3(1, 1, 1)), P("GroupTransparency", Value::number(0))};
    auto& sf = r.add("ScrollingFrame", "GuiObject");
    sf.props = {P("CanvasSize", Value::udim2(0, 0, 0, 0)), P("CanvasPosition", Value::vector2(0, 0)),
                PEnum("AutomaticCanvasSize", "AutomaticSize", "None"), P("ScrollingEnabled", Value::boolean(true)),
                PEnum("ScrollingDirection", "ScrollingDirection", "XY"), P("ScrollBarThickness", Value::number(12)),
                P("ScrollBarImageColor3", Value::color3(0, 0, 0)), P("ScrollBarImageTransparency", Value::number(0)),
                PEnum("ElasticBehavior", "ElasticBehavior", "WhenScrollable"),
                PEnum("VerticalScrollBarPosition", "VerticalScrollBarPosition", "Right"),
                PEnum("VerticalScrollBarInset", "ScrollBarInset", "None"), PEnum("HorizontalScrollBarInset", "ScrollBarInset", "None"),
                P("TopImage", Value::string("")), P("MidImage", Value::string("")), P("BottomImage", Value::string("")),
                P("AbsoluteCanvasSize", Value::vector2(0, 0), ReadOnly), P("AbsoluteWindowSize", Value::vector2(0, 0), ReadOnly)};
    std::vector<PropDef> text = {P("Text", Value::string("")), P("TextColor3", Value::color3(0, 0, 0)), P("TextSize", Value::number(14)),
                                 PEnum("Font", "Font", "SourceSans"), P("FontFace", Value::font("rbxasset://fonts/families/SourceSansPro.json", 400, false)),
                                 P("TextScaled", Value::boolean(false)),
                                 P("TextWrapped", Value::boolean(false)), PEnum("TextXAlignment", "TextXAlignment", "Center"),
                                 PEnum("TextYAlignment", "TextYAlignment", "Center"), P("TextTransparency", Value::number(0)),
                                 P("TextStrokeTransparency", Value::number(1)), P("TextStrokeColor3", Value::color3(0, 0, 0)),
                                 P("RichText", Value::boolean(false)), P("LineHeight", Value::number(1)),
                                 PEnum("TextTruncate", "TextTruncate", "None"), P("TextBounds", Value::vector2(0, 0), ReadOnly),
                                 P("TextFits", Value::boolean(true), ReadOnly), P("ContentText", Value::string(""), ReadOnly)};
    // MaxVisibleGraphemes caps the characters drawn (-1: all). LocalizedText reads as Text: nothing
    // here localises (inst_index; read only, which is not verified against Roblox). OpenTypeFeatures
    // and TextDirection are recorded, not acted on: the engine's font shaping takes neither.
    text.insert(text.end(), {P("MaxVisibleGraphemes", Value::number(-1)), P("LocalizedText", Value::string(""), ReadOnly),
                             P("OpenTypeFeatures", Value::string("")), P("OpenTypeFeaturesError", Value::string(""), ReadOnly),
                             PEnum("TextDirection", "TextDirection", "Auto")});
    auto& tl = r.add("TextLabel", "GuiObject");
    tl.props = text;
    auto& gbtn = r.add("GuiButton", "GuiObject", false);
    // Selectable is redeclared because Roblox defaults it true on a button and false on a GuiObject:
    // without the override, setting it false matches the inherited default and never reaches the host.
    gbtn.props = {P("AutoButtonColor", Value::boolean(true)), P("Modal", Value::boolean(false)),
                  P("Selected", Value::boolean(false)), P("Selectable", Value::boolean(true))};
    gbtn.events = {"Activated", "MouseButton1Click", "MouseButton1Down", "MouseButton1Up", "MouseButton2Click", "MouseButton2Down", "MouseButton2Up"};
    // Style and the two HapticEffect refs are recorded, not acted on: every button is drawn Custom and nothing here buzzes.
    gbtn.props.insert(gbtn.props.end(), {PEnum("Style", "ButtonStyle", "Custom"), PRef("HoverHapticEffect"), PRef("PressHapticEffect")});
    gbtn.events.push_back("SecondaryActivated");   // a right-button click (Runtime::guiInput)
    auto& tb = r.add("TextButton", "GuiButton");
    tb.props = text;
    // Image is a path under the engine's asset root; the engine fills IsLoaded and ContentImageSize.
    std::vector<PropDef> image = {P("Image", Value::string("")), P("ImageColor3", Value::color3(1, 1, 1)), P("ImageTransparency", Value::number(0)),
                                  PEnum("ScaleType", "ScaleType", "Stretch"), PEnum("ResampleMode", "ResamplerMode", "Default"),
                                  P("ImageRectOffset", Value::vector2(0, 0)), P("ImageRectSize", Value::vector2(0, 0)),
                                  P("TileSize", Value::udim2(1, 0, 1, 0)),
                                  P("IsLoaded", Value::boolean(false), ReadOnly), P("ContentImageSize", Value::vector2(0, 0), ReadOnly),
                                  P("SliceScale", Value::number(1))};   // recorded, not acted on: a Slice is drawn at its pixel size
    r.add("ImageLabel", "GuiObject").props = image;
    auto& ib = r.add("ImageButton", "GuiButton");
    ib.props = image;
    ib.props.push_back(P("HoverImage", Value::string("")));
    ib.props.push_back(P("PressedImage", Value::string("")));
    // Parts parented into one are drawn inside it and nowhere else, through CurrentCamera.
    auto& vpf = r.add("ViewportFrame", "GuiObject");
    vpf.props = {PRef("CurrentCamera"),
                 P("Ambient", Value::color3(200 / 255.f, 200 / 255.f, 200 / 255.f)),
                 P("LightColor", Value::color3(140 / 255.f, 140 / 255.f, 140 / 255.f)),
                 P("LightDirection", Value::vector3(-1, -1, -1)),
                 P("ImageColor3", Value::color3(1, 1, 1)),
                 P("ImageTransparency", Value::number(0))};

    auto& tbox = r.add("TextBox", "GuiObject");
    tbox.props = text;
    tbox.props.push_back(P("PlaceholderText", Value::string("")));
    tbox.props.push_back(P("PlaceholderColor3", Value::color3(178 / 255.f, 178 / 255.f, 178 / 255.f)));
    tbox.props.push_back(P("ClearTextOnFocus", Value::boolean(true)));
    tbox.props.push_back(P("MultiLine", Value::boolean(false)));
    tbox.props.push_back(P("TextEditable", Value::boolean(true)));
    tbox.props.push_back(P("CursorPosition", Value::number(-1)));   // -1: not focused; otherwise the caret, 1 before the first character
    tbox.props.push_back(P("SelectionStart", Value::number(-1)));
    tbox.props.push_back(P("ShowNativeInput", Value::boolean(true)));   // recorded, not acted on: no on-screen keyboard here
    tbox.events = {"Focused", "FocusLost", "ReturnPressedFromOnScreenKeyboard"};
    // Play and Pause set Playing and fire Played / Paused (rbx_api.cpp); nothing here decodes a
    // video, so IsLoaded, Resolution and TimeLength stay at zero and Loaded, Ended and DidLoop never
    // fire. The roll-off figures' Roblox defaults are not verified here (Sound's are used).
    auto& vf = r.add("VideoFrame", "GuiObject");
    vf.props = {P("Playing", Value::boolean(false)), P("IsLoaded", Value::boolean(false), ReadOnly),
                P("Resolution", Value::vector2(0, 0), ReadOnly), P("TimeLength", Value::number(0), ReadOnly)};
    vf.events = {"Played", "Paused", "Loaded", "Ended", "DidLoop"};
    auto& uic = r.add("UIComponent", "Instance", false);
    (void)uic;
    r.add("UICorner", "UIComponent").props = {P("CornerRadius", Value::udim(0, 8))};
    r.add("UIPadding", "UIComponent").props = {P("PaddingTop", Value::udim(0, 0)), P("PaddingBottom", Value::udim(0, 0)),
                                               P("PaddingLeft", Value::udim(0, 0)), P("PaddingRight", Value::udim(0, 0))};
    auto& uistroke = r.add("UIStroke", "UIComponent");
    uistroke.props = {P("Color", Value::color3(0, 0, 0)), P("Thickness", Value::number(1)),
                      P("Transparency", Value::number(0)), P("Enabled", Value::boolean(true))};
    // Recorded, not acted on: the stroke is drawn as the parent's border whichever mode or join is set.
    uistroke.props.insert(uistroke.props.end(), {PEnum("ApplyStrokeMode", "ApplyStrokeMode", "Contextual"), PEnum("LineJoinMode", "LineJoinMode", "Round")});
    // The ramps are stored; the GUI here draws the parent flat.
    r.add("UIGradient", "UIComponent").props = {P("Color", Value::colorSequence(Col3{1, 1, 1})), P("Transparency", Value::numberSequence(0, 0)),
                                                P("Rotation", Value::number(0)), P("Offset", Value::vector2(0, 0)), P("Enabled", Value::boolean(true))};
    // Roblox's bases of the layouts: UILayout, then UIGridStyleLayout. Each layout declares the
    // grid-style members itself, their defaults differing by class.
    r.add("UILayout", "UIComponent", false);
    auto& gridStyle = r.add("UIGridStyleLayout", "UILayout", false);
    auto& uil = r.add("UIListLayout", "UIComponent");
    uil.base = &gridStyle;
    uil.props = {PEnum("FillDirection", "FillDirection", "Vertical"), P("Padding", Value::udim(0, 0)),
                 PEnum("HorizontalAlignment", "HorizontalAlignment", "Left"), PEnum("VerticalAlignment", "VerticalAlignment", "Top"),
                 PEnum("SortOrder", "SortOrder", "Name"), P("AbsoluteContentSize", Value::vector2(0, 0), ReadOnly)};
    // Recorded, not acted on: the engine's list neither flexes nor wraps.
    uil.props.insert(uil.props.end(), {PEnum("HorizontalFlex", "UIFlexAlignment", "None"), PEnum("VerticalFlex", "UIFlexAlignment", "None"),
                                       PEnum("ItemLineAlignment", "ItemLineAlignment", "Automatic"), P("Wraps", Value::boolean(false))});
    auto& uig = r.add("UIGridLayout", "UIComponent");
    uig.base = &gridStyle;
    // JumpTo / JumpToIndex / Next / Previous move CurrentPage among the parent's GuiObject children
    // in SortOrder and fire PageLeave, PageEnter and Stopped at once (rbx_api.cpp); nothing here
    // slides the pages, so the tween settings are recorded, not acted on.
    auto& upl = r.add("UIPageLayout", "UIGridStyleLayout");
    upl.props = {PEnum("FillDirection", "FillDirection", "Horizontal"), PEnum("HorizontalAlignment", "HorizontalAlignment", "Left"),
                 PEnum("VerticalAlignment", "VerticalAlignment", "Top"), P("AbsoluteContentSize", Value::vector2(0, 0), ReadOnly),
                 P("Animated", Value::boolean(true)), P("Circular", Value::boolean(false)), PRef("CurrentPage", ReadOnly),
                 P("GamepadInputEnabled", Value::boolean(true)), P("ScrollWheelInputEnabled", Value::boolean(true)),
                 P("TouchInputEnabled", Value::boolean(true)), P("TweenTime", Value::number(1))};
    upl.events = {"PageEnter", "PageLeave", "Stopped"};
    // Recorded, not acted on: nothing here lays a table out.
    r.add("UITableLayout", "UIGridStyleLayout").props = {PEnum("FillDirection", "FillDirection", "Horizontal"), PEnum("HorizontalAlignment", "HorizontalAlignment", "Left"),
                                                        PEnum("VerticalAlignment", "VerticalAlignment", "Top"), P("AbsoluteContentSize", Value::vector2(0, 0), ReadOnly),
                                                        P("FillEmptySpaceColumns", Value::boolean(false)), P("FillEmptySpaceRows", Value::boolean(false)),
                                                        PEnum("MajorAxis", "TableMajorAxis", "RowMajor")};
    // Recorded, not acted on: the engine's list ignores a flex item.
    r.add("UIFlexItem", "UIComponent").props = {PEnum("FlexMode", "UIFlexMode", "None"), P("GrowRatio", Value::number(0)), P("ShrinkRatio", Value::number(0)),
                                                PEnum("ItemLineAlignment", "ItemLineAlignment", "Automatic")};
    // Recorded, not acted on: nothing here drags a GuiObject with the pointer, so the three drag
    // events never fire and the constraint and drag-style functions are not answered.
    // SelectionModeDragSpeed and SelectionModeRotateSpeed are not declared (their types are not verified here).
    auto& udd = r.add("UIDragDetector", "UIComponent");
    udd.props = {PEnum("DragStyle", "UIDragDetectorDragStyle", "TranslatePlane"), PEnum("DragRelativity", "UIDragDetectorDragRelativity", "Relative"),
                 PEnum("DragSpace", "UIDragDetectorDragSpace", "Parent"), PEnum("ResponseStyle", "UIDragDetectorResponseStyle", "Offset"),
                 PEnum("BoundingBehavior", "UIDragDetectorBoundingBehavior", "Automatic"), PRef("BoundingUI"), PRef("ReferenceUIInstance"),
                 P("DragAxis", Value::vector2(1, 0)), P("DragRotation", Value::number(0)), P("DragUDim2", Value::udim2(0, 0, 0, 0)),
                 P("MaxDragAngle", Value::number(0)), P("MinDragAngle", Value::number(0)),
                 P("MaxDragTranslation", Value::udim2(0, 0, 0, 0)), P("MinDragTranslation", Value::udim2(0, 0, 0, 0)),
                 PEnum("UIDragSpeedAxisMapping", "UIDragSpeedAxisMapping", "XY"),
                 P("CursorIcon", Value::string("")), P("ActivatedCursorIcon", Value::string(""))};
    udd.events = {"DragStart", "DragContinue", "DragEnd"};
    // Recorded, not acted on: nothing here draws a shadow. Roblox's defaults for BlurRadius and
    // Spread are not verified here (the type's zero); Offset is not declared (its type is not verified).
    r.add("UIShadow", "UIComponent").props = {P("BlurRadius", Value::number(0)), P("Spread", Value::number(0))};

    // 3D GUI: the adornments. Recorded, not acted on: nothing here draws one, picks one with the
    // pointer or drags a Handles, so their mouse events never fire. Transparency, Visible, Adornee,
    // AlwaysOnTop, ZIndex, Size, Thickness, Scale and Image are declared by table below (kGuiShared);
    // CFrame is a CFrame over the pair here (cframeProp in rbx_api.cpp). Faces and Axes are not
    // declared (no Faces or Axes datatype here), nor is Shading (its type is not verified).
    r.add("GuiBase3d", "GuiBase", false).props = {P("Color3", Value::color3(13 / 255.f, 105 / 255.f, 172 / 255.f))};
    r.add("PVAdornment", "GuiBase3d", false);
    r.add("PartAdornment", "GuiBase3d", false);
    r.add("InstanceAdornment", "GuiBase3d", false);
    auto& ha = r.add("HandleAdornment", "PVAdornment", false);
    ha.props = {PEnum("AdornCullingMode", "AdornCullingMode", "Automatic"),
                P("CFramePosition", Value::vector3(0, 0, 0), Hidden), P("CFrameOrientation", Value::vector3(0, 0, 0), Hidden),
                P("SizeRelativeOffset", Value::vector3(0, 0, 0))};
    ha.events.insert(ha.events.end(), {"MouseButton1Down", "MouseButton1Up", "MouseEnter", "MouseLeave"});
    r.add("BoxHandleAdornment", "HandleAdornment");
    r.add("ConeHandleAdornment", "HandleAdornment").props = {P("Height", Value::number(2)), P("Radius", Value::number(0.5))};   // Hollow is not declared (its type is not verified here)
    r.add("CylinderHandleAdornment", "HandleAdornment").props = {P("Angle", Value::number(360)), P("Height", Value::number(1)),
                                                                 P("InnerRadius", Value::number(0)), P("Radius", Value::number(1))};
    r.add("SphereHandleAdornment", "HandleAdornment").props = {P("Radius", Value::number(1))};
    r.add("LineHandleAdornment", "HandleAdornment").props = {P("Length", Value::number(5))};
    r.add("ImageHandleAdornment", "HandleAdornment");
    // Height, Sides and Size are not declared: their types and defaults are not verified here.
    r.add("PyramidHandleAdornment", "HandleAdornment");
    // AddLine, AddLines, AddPath and Clear keep the segments (rbx_api.cpp); nothing here draws them.
    // AddText is not declared (its signature is not verified here).
    r.add("WireframeHandleAdornment", "HandleAdornment");
    auto& hb = r.add("HandlesBase", "PartAdornment", false);
    hb.events.insert(hb.events.end(), {"MouseButton1Down", "MouseButton1Up", "MouseDrag", "MouseEnter", "MouseLeave"});
    r.add("Handles", "HandlesBase").props = {PEnum("Style", "HandlesStyle", "Resize")};
    r.add("ArcHandles", "HandlesBase");
    r.add("SelectionBox", "InstanceAdornment").props = {P("LineThickness", Value::number(0.15)),
                                                        P("SurfaceColor3", Value::color3(13 / 255.f, 105 / 255.f, 172 / 255.f)), P("SurfaceTransparency", Value::number(1))};
    r.add("SelectionSphere", "PVAdornment").props = {P("SurfaceColor3", Value::color3(13 / 255.f, 105 / 255.f, 172 / 255.f)), P("SurfaceTransparency", Value::number(1))};
    // A 2D curve. Recorded, not acted on: nothing here draws it, and the control-point methods are
    // not declared (no Path2DControlPoint datatype here). Its Roblox defaults are not verified here.
    r.add("Path2D", "GuiBase").props = {P("Closed", Value::boolean(false)), P("Color3", Value::color3(1, 1, 1))};
    uig.props = {P("CellSize", Value::udim2(0, 100, 0, 100)), P("CellPadding", Value::udim2(0, 5, 0, 5)),
                 PEnum("FillDirection", "FillDirection", "Horizontal"), P("FillDirectionMaxCells", Value::number(0)),
                 PEnum("StartCorner", "StartCorner", "TopLeft"),
                 PEnum("HorizontalAlignment", "HorizontalAlignment", "Left"), PEnum("VerticalAlignment", "VerticalAlignment", "Top"),
                 PEnum("SortOrder", "SortOrder", "Name"), P("AbsoluteContentSize", Value::vector2(0, 0), ReadOnly),
                 P("AbsoluteCellCount", Value::vector2(0, 0), ReadOnly), P("AbsoluteCellSize", Value::vector2(0, 0), ReadOnly)};
    r.add("UIScale", "UIComponent").props = {P("Scale", Value::number(1))};
    r.add("UIAspectRatioConstraint", "UIComponent").props = {P("AspectRatio", Value::number(1)),
                                                             PEnum("AspectType", "AspectType", "FitWithinMaxSize"),
                                                             PEnum("DominantAxis", "DominantAxis", "Width")};
    r.add("UISizeConstraint", "UIComponent").props = {P("MinSize", Value::vector2(0, 0)),
                                                      P("MaxSize", Value::vector2(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()))};
    r.add("UITextSizeConstraint", "UIComponent").props = {P("MinTextSize", Value::number(1)), P("MaxTextSize", Value::number(100))};
    r.add("Backpack", "Instance", false);
    r.add("StarterGear", "Instance", false);
    // A Tool is a Model with a Handle: on the hotbar in a Backpack, Equipped in a character.
    auto& bpi = r.add("BackpackItem", "Model", false);
    bpi.props = {P("TextureId", Value::string(""))};
    // Holds Part1 at Part0 * C0 * C1:Inverse(); a Motor6D turns C0 about its Z by CurrentAngle,
    // chasing DesiredAngle at MaxVelocity a step. CFrame is not a Value type, so C0 and C1 are a
    // position and an orientation each.
    auto& joint = r.add("JointInstance", "Instance", false);
    joint.props = {PRef("Part0"), PRef("Part1"), P("Enabled", Value::boolean(true)),
                   P("C0Position", Value::vector3(0, 0, 0)), P("C0Orientation", Value::vector3(0, 0, 0)),
                   P("C1Position", Value::vector3(0, 0, 0)), P("C1Orientation", Value::vector3(0, 0, 0))};
    r.add("Weld", "JointInstance");
    r.add("Snap", "JointInstance");
    r.add("ManualWeld", "JointInstance");
    // The legacy Motor: CurrentAngle chases DesiredAngle at MaxVelocity a step (Runtime::Impl::stepMotors),
    // and the host turns C0 about its Z by it, as for a Motor6D. SetDesiredAngle writes DesiredAngle.
    r.add("Motor", "JointInstance").props = {P("CurrentAngle", Value::number(0)), P("DesiredAngle", Value::number(0)), P("MaxVelocity", Value::number(0.1))};
    // Recorded, not acted on: nothing here steps a VelocityMotor.
    r.add("VelocityMotor", "JointInstance").props = {P("CurrentAngle", Value::number(0)), P("DesiredAngle", Value::number(0)),
                                                     P("MaxVelocity", Value::number(0)), PRef("Hole")};
    // Transform (Part1 = Part0 * C0 * Transform * C1^-1) is written each frame by the Animator.
    r.add("Motor6D", "JointInstance").props = {P("CurrentAngle", Value::number(0)), P("DesiredAngle", Value::number(0)), P("MaxVelocity", Value::number(0.1)),
                                               P("TransformPosition", Value::vector3(0, 0, 0), Hidden), P("TransformOrientation", Value::vector3(0, 0, 0), Hidden)};
    // Keeps the offset the two parts had when it was enabled; nothing to store.
    r.add("WeldConstraint", "Instance").props = {PRef("Part0"), PRef("Part1"), P("Enabled", Value::boolean(true))};
    // Its two parts pass through each other and touch everything else as before.
    r.add("NoCollisionConstraint", "Instance").props = {PRef("Part0"), PRef("Part1"), P("Enabled", Value::boolean(true))};
    // A free hinge about C0's Z axis; RotateP / RotateV keep their settings but are not driven.
    r.add("Rotate", "JointInstance");
    auto& dynRot = r.add("DynamicRotate", "JointInstance", false); dynRot.props = {P("BaseAngle", Value::number(0))};
    r.add("RotateP", "DynamicRotate").props = {P("DesiredAngle", Value::number(0)), P("MaxVelocity", Value::number(0)), P("P", Value::number(0))};
    r.add("RotateV", "DynamicRotate");
    // Legacy BodyMovers, stepped by step_movers in the host: a part's own, with no Attachment.
    auto& mover = r.add("BodyMover", "Instance", false); (void)mover;
    r.add("BodyGyro", "BodyMover").props = {P("CFramePosition", Value::vector3(0, 0, 0)), P("CFrameOrientation", Value::vector3(0, 0, 0)),
                                            P("D", Value::number(500)), P("P", Value::number(3000)), P("MaxTorque", Value::vector3(400000, 0, 400000))};
    r.add("BodyVelocity", "BodyMover").props = {P("Velocity", Value::vector3(0, 2, 0)), P("MaxForce", Value::vector3(4000, 4000, 4000)), P("P", Value::number(1250))};
    r.add("BodyPosition", "BodyMover").props = {P("Position", Value::vector3(0, 50, 0)), P("MaxForce", Value::vector3(4000, 4000, 4000)),
                                                P("P", Value::number(10000)), P("D", Value::number(1250))};
    r.add("BodyAngularVelocity", "BodyMover").props = {P("AngularVelocity", Value::vector3(0, 2, 0)), P("MaxTorque", Value::vector3(4000, 4000, 4000)), P("P", Value::number(1250))};
    r.add("BodyForce", "BodyMover").props = {P("Force", Value::vector3(0, 1, 0))};
    r.add("BodyThrust", "BodyMover").props = {P("Force", Value::vector3(0, 1, 0)), P("Location", Value::vector3(0, 0, 0))};
    // Constraints join the two Attachments' parts through the engine's physics. A hinge turns about
    // Attachment0's axis: Motor spins it at AngularVelocity, Servo holds TargetAngle at AngularSpeed.
    auto& constraint = r.add("Constraint", "Instance", false);
    constraint.props = {PRef("Attachment0"), PRef("Attachment1"), P("Enabled", Value::boolean(true)), P("Visible", Value::boolean(false))};
    r.add("HingeConstraint", "Constraint").props = {PEnum("ActuatorType", "ActuatorType", "None"), P("AngularVelocity", Value::number(0)),
        P("MotorMaxAcceleration", Value::number(1e6)), P("MotorMaxTorque", Value::number(0)), P("AngularSpeed", Value::number(0)),
        P("ServoMaxTorque", Value::number(0)), P("TargetAngle", Value::number(0)), P("LimitsEnabled", Value::boolean(false)),
        P("LowerAngle", Value::number(-45)), P("UpperAngle", Value::number(45)), P("Restitution", Value::number(0)),
        P("CurrentAngle", Value::number(0), ReadOnly), P("Radius", Value::number(0.15)),
        // Recorded, not acted on: the host's hinge servo has one fixed response.
        P("AngularResponsiveness", Value::number(45))};
    // Rope: within Length, a winch reeling it. Rod: at Length. Spring: toward FreeLength.
    r.add("RopeConstraint", "Constraint").props = {P("Length", Value::number(5)), P("Restitution", Value::number(0)), P("Thickness", Value::number(0.1)),
        P("WinchEnabled", Value::boolean(false)), P("WinchTarget", Value::number(0)), P("WinchSpeed", Value::number(0)),
        P("WinchForce", Value::number(0)), P("WinchResponsiveness", Value::number(10)), P("CurrentDistance", Value::number(0), ReadOnly)};
    r.add("RodConstraint", "Constraint").props = {P("Length", Value::number(5)), P("Thickness", Value::number(0.1)), P("LimitsEnabled", Value::boolean(false)),
        P("LimitAngle0", Value::number(45)), P("LimitAngle1", Value::number(45)), P("CurrentDistance", Value::number(0), ReadOnly)};
    r.add("SpringConstraint", "Constraint").props = {P("Stiffness", Value::number(100)), P("Damping", Value::number(0)), P("FreeLength", Value::number(5)),
        P("MinLength", Value::number(0)), P("MaxLength", Value::number(1e6)), P("MaxForce", Value::number(1e6)), P("LimitsEnabled", Value::boolean(false)),
        P("Coils", Value::number(3)), P("Radius", Value::number(0.5)), P("Thickness", Value::number(0.1)), P("CurrentLength", Value::number(0), ReadOnly)};
    // These push Attachment0's assembly; AlignPosition / AlignOrientation aim at Attachment1, or at Position / CFrame in OneAttachment mode.
    r.add("VectorForce", "Constraint").props = {P("Force", Value::vector3(0, 0, 0)), PEnum("RelativeTo", "ActuatorRelativeTo", "Attachment0"), P("ApplyAtCenterOfMass", Value::boolean(false))};
    r.add("Torque", "Constraint").props = {P("Torque", Value::vector3(0, 0, 0)), PEnum("RelativeTo", "ActuatorRelativeTo", "Attachment0")};
    r.add("LinearVelocity", "Constraint").props = {P("VectorVelocity", Value::vector3(0, 0, 0)), P("MaxForce", Value::number(0)), PEnum("RelativeTo", "ActuatorRelativeTo", "World"),
        PEnum("VelocityConstraintMode", "VelocityConstraintMode", "Vector"), P("LineDirection", Value::vector3(1, 0, 0)), P("LineVelocity", Value::number(0)),
        P("PlaneVelocity", Value::vector2(0, 0)), P("PrimaryTangentAxis", Value::vector3(1, 0, 0)), P("SecondaryTangentAxis", Value::vector3(0, 1, 0)),
        PEnum("ForceLimitMode", "ForceLimitMode", "Magnitude"), P("MaxAxesForce", Value::vector3(0, 0, 0)),
        // Recorded, not acted on: the host caps the force by MaxForce whatever ForceLimitsEnabled says, and
        // MaxPlanarAxesForce is not read. ReactionForceEnabled reaches the host (it pushes Attachment1's assembly back).
        P("ForceLimitsEnabled", Value::boolean(false)), P("MaxPlanarAxesForce", Value::vector2(0, 0)), P("ReactionForceEnabled", Value::boolean(false))};
    r.add("AngularVelocity", "Constraint").props = {P("AngularVelocity", Value::vector3(0, 0, 0)), P("MaxTorque", Value::number(0)), PEnum("RelativeTo", "ActuatorRelativeTo", "World"), P("ReactionTorqueEnabled", Value::boolean(false))};
    r.add("AlignPosition", "Constraint").props = {PEnum("Mode", "PositionAlignmentMode", "TwoAttachment"), P("Position", Value::vector3(0, 0, 0)), P("MaxForce", Value::number(10000)),
        P("MaxVelocity", Value::number(1e6)), P("Responsiveness", Value::number(10)), P("RigidityEnabled", Value::boolean(false)), P("ApplyAtCenterOfMass", Value::boolean(false)),
        P("ReactionForceEnabled", Value::boolean(false)), PEnum("ForceLimitMode", "ForceLimitMode", "Magnitude"),
        P("MaxAxesForce", Value::vector3(0, 0, 0)),
        // the frame MaxAxesForce's three axes are measured in
        PEnum("ForceRelativeTo", "ActuatorRelativeTo", "World")};
    r.add("AlignOrientation", "Constraint").props = {PEnum("Mode", "OrientationAlignmentMode", "TwoAttachment"), P("CFramePosition", Value::vector3(0, 0, 0)), P("CFrameOrientation", Value::vector3(0, 0, 0)),
        P("MaxTorque", Value::number(10000)), P("MaxAngularVelocity", Value::number(1e6)), P("Responsiveness", Value::number(10)), P("RigidityEnabled", Value::boolean(false)),
        P("PrimaryAxisOnly", Value::boolean(false)), P("ReactionTorqueEnabled", Value::boolean(false)),
        // Recorded, not acted on: the host aligns all axes (or the primary one under PrimaryAxisOnly) and
        // never looks at a point. Roblox's AlignType default is not verified here: AllAxes is assumed.
        PEnum("AlignType", "AlignType", "AllAxes"), P("LookAtPosition", Value::vector3(0, 0, 0)),
        P("PrimaryAxis", Value::vector3(1, 0, 0)), P("SecondaryAxis", Value::vector3(0, 1, 0))};
    r.add("BallSocketConstraint", "Constraint").props = {P("LimitsEnabled", Value::boolean(false)), P("UpperAngle", Value::number(45)),
        P("TwistLimitsEnabled", Value::boolean(false)), P("TwistLowerAngle", Value::number(-45)), P("TwistUpperAngle", Value::number(45)),
        P("Restitution", Value::number(0)), P("MaxFrictionTorque", Value::number(0)), P("Radius", Value::number(0.15))};
    // Slides along Attachment0's X axis: ActuatorType Motor drives it at Velocity, Servo to
    // TargetPosition at Speed. A CylindricalConstraint also turns about that axis.
    auto& sliding = r.add("SlidingBallConstraint", "Constraint", false);
    sliding.props = {PEnum("ActuatorType", "ActuatorType", "None"), P("Velocity", Value::number(0)),
        P("MotorMaxAcceleration", Value::number(1e6)), P("MotorMaxForce", Value::number(0)), P("Speed", Value::number(0)),
        P("ServoMaxForce", Value::number(0)), P("TargetPosition", Value::number(0)), P("LimitsEnabled", Value::boolean(false)),
        P("LowerLimit", Value::number(-5)), P("UpperLimit", Value::number(5)), P("Restitution", Value::number(0)),
        P("CurrentPosition", Value::number(0), ReadOnly), P("Size", Value::number(0.15)),
        // Recorded, not acted on: the host's slider servo has one fixed response.
        P("LinearResponsiveness", Value::number(45))};
    r.add("PrismaticConstraint", "SlidingBallConstraint");
    r.add("CylindricalConstraint", "SlidingBallConstraint").props = {PEnum("AngularActuatorType", "ActuatorType", "None"),
        P("AngularVelocity", Value::number(0)), P("MotorMaxAngularAcceleration", Value::number(1e6)), P("MotorMaxTorque", Value::number(0)),
        P("AngularSpeed", Value::number(0)), P("ServoMaxTorque", Value::number(0)), P("TargetAngle", Value::number(0)),
        P("AngularLimitsEnabled", Value::boolean(false)), P("LowerAngle", Value::number(-45)), P("UpperAngle", Value::number(45)),
        P("AngularRestitution", Value::number(0)), P("InclinationAngle", Value::number(0)),
        P("CurrentAngle", Value::number(0), ReadOnly), P("RotationAxisVisible", Value::boolean(false)),
        // Recorded, not acted on: the host's angular servo has one fixed response. WorldRotationAxis
        // (Attachment0's WorldAxis) is computed in rbx_api.cpp.
        P("AngularResponsiveness", Value::number(45))};
    // Recorded, not acted on: the host simulates neither. Roblox's defaults for Damping, MaxTorque and
    // Radius on the torsion spring are not verified here: the type's zero.
    r.add("TorsionSpringConstraint", "Constraint").props = {P("Coils", Value::number(3)), P("CurrentAngle", Value::number(0), ReadOnly),
        P("Damping", Value::number(0)), P("LimitsEnabled", Value::boolean(false)), P("MaxAngle", Value::number(45)), P("MaxTorque", Value::number(0)),
        P("Radius", Value::number(0)), P("Restitution", Value::number(0)), P("Stiffness", Value::number(100))};
    r.add("UniversalConstraint", "Constraint").props = {P("LimitsEnabled", Value::boolean(false)), P("MaxAngle", Value::number(45)),
        P("Radius", Value::number(0.15)), P("Restitution", Value::number(0))};
    // Recorded, not acted on: nothing here animates through a constraint. Transform is a CFrame (cframeProp
    // in rbx_api.cpp). Roblox's defaults for the strengths, dampings and limits are not verified here: the type's zero.
    r.add("AnimationConstraint", "Constraint").props = {P("AngularDamping", Value::number(0)), P("AngularStrength", Value::number(0)),
        P("IsKinematic", Value::boolean(false)), P("LinearDamping", Value::number(0)), P("LinearStrength", Value::number(0)),
        P("MaxForce", Value::number(0)), P("MaxTorque", Value::number(0)),
        P("TransformPosition", Value::vector3(0, 0, 0), Hidden), P("TransformOrientation", Value::vector3(0, 0, 0), Hidden)};

    // Worn by matching the Handle's Attachment to the limb Attachment of the same name (HatAttachment, RightGripAttachment).
    auto& accoutrement = r.add("Accoutrement", "Instance", false);
    accoutrement.props = {P("AttachmentPointPosition", Value::vector3(0, 0, 0), Hidden), P("AttachmentPointOrientation", Value::vector3(0, 0, 0), Hidden),
                          P("AttachmentPos", Value::vector3(0, 0, 0)), P("AttachmentForward", Value::vector3(0, 0, -1)),
                          P("AttachmentRight", Value::vector3(1, 0, 0)), P("AttachmentUp", Value::vector3(0, 1, 0))};
    r.add("Accessory", "Accoutrement").props = {PEnum("AccessoryType", "AccessoryType", "Unknown")};
    r.add("Hat", "Accoutrement");

    r.add("Attachment", "Instance").props = {P("Position", Value::vector3(0, 0, 0)), P("Orientation", Value::vector3(0, 0, 0)),
                                             P("Visible", Value::boolean(false))};
    // An Attachment with a Transform (a CFrame, cframeProp in rbx_api.cpp): TransformedCFrame is CFrame * Transform
    // and TransformedWorldCFrame the same in the world, through a parent Bone's own. Nothing here skins a mesh by it.
    r.add("Bone", "Attachment").props = {P("TransformPosition", Value::vector3(0, 0, 0), Hidden), P("TransformOrientation", Value::vector3(0, 0, 0), Hidden)};
    auto& tool = r.add("Tool", "BackpackItem");
    tool.props = {P("RequiresHandle", Value::boolean(true)), P("CanBeDropped", Value::boolean(true)), P("Enabled", Value::boolean(true)),
                  P("ManualActivationOnly", Value::boolean(false)), P("ToolTip", Value::string("")),
                  // Tool.Grip (a CFrame, see inst_index) is these four
                  P("GripPos", Value::vector3(0, 0, 0)), P("GripForward", Value::vector3(0, 0, -1)),
                  P("GripRight", Value::vector3(1, 0, 0)), P("GripUp", Value::vector3(0, 1, 0)),
                  // 1-9 while the tool is in the local player's Backpack or hand, 0 elsewhere
                  P("HotbarSlot", Value::number(0), ReadOnly | NoReplicate | Hidden)};
    tool.events = {"Activated", "Deactivated", "Equipped", "Unequipped"};

    auto& hum = r.add("Humanoid", "Instance");
    hum.props = {P("Health", Value::number(100)), P("MaxHealth", Value::number(100)), P("WalkSpeed", Value::number(16)),
                 P("JumpPower", Value::number(50)), P("JumpHeight", Value::number(7.2)),
                 // which of the two above is read; false, Roblox's default, is JumpHeight
                 P("UseJumpPower", Value::boolean(false)), P("DisplayName", Value::string("")),
                 P("Jump", Value::boolean(false)), P("Sit", Value::boolean(false)), PRef("SeatPart", ReadOnly), P("PlatformStand", Value::boolean(false)),
                 P("AutoRotate", Value::boolean(true)), P("HipHeight", Value::number(0)), P("MoveDirection", Value::vector3(0, 0, 0), ReadOnly),
                P("WalkToPoint", Value::vector3(0, 0, 0)), PRef("WalkToPart"),
                // the name and health bar the engine draws over the head
                P("NameDisplayDistance", Value::number(100)), P("HealthDisplayDistance", Value::number(100)),
                PEnum("DisplayDistanceType", "HumanoidDisplayDistanceType", "Viewer"),
                PEnum("HealthDisplayType", "HumanoidHealthDisplayType", "DisplayWhenDamaged"),
                PEnum("NameOcclusion", "NameOcclusion", "OccludeAll"),
                P("StateName", Value::string("Running"), Hidden),   // what Humanoid:GetState() answers: Running, Freefall, Swimming, Seated
                // Writable, as on Roblox. Setting it does not re-rig a character that is already
                // built -- the rig is its parts -- but a Humanoid in a StarterCharacter is read
                // when that model is cloned to spawn with (rbx_runtime.cpp spawnCharacter).
                PEnum("RigType", "HumanoidRigType", "R6"),
                // Roblox lets a dead character's joints go, so it falls apart where it stands.
                // Recorded, not acted on: nothing here takes a character apart when it dies.
                P("BreakJointsOnDeath", Value::boolean(true)),
                // Roblox scales an R15 rig to the avatar's body-scale values. Nothing here
                // scales a character, so this records what a place asked for and the engine's
                // behaviour is already the false one.
                P("AutomaticScalingEnabled", Value::boolean(true)),
                // Roblox kills a Humanoid whose Neck joint is destroyed. Recorded, not acted
                // on: no character here dies for a missing joint.
                P("RequiresNeck", Value::boolean(true))};
    hum.events = {"Died", "HealthChanged", "Running", "Jumping", "StateChanged", "Seated", "Touched", "MoveToFinished"};
    hum.props.insert(hum.props.end(), {
        // The camera's focus, offset in the root part's own frame (pulseblockz_world.cpp update_camera).
        P("CameraOffset", Value::vector3(0, 0, 0)),
        // The material under the feet, cast for on read (rbx_api.cpp inst_index): Air off the
        // ground, Water while swimming.
        PEnum("FloorMaterial", "Material", "Air", ReadOnly),
        // Where the last tool click pointed (rbx_api.cpp ToolActivate, on the clicking client).
        P("TargetPoint", Value::vector3(0, 0, 0)),
        // Recorded, not acted on: the slope the body climbs is Godot's own floor angle, not this.
        P("MaxSlopeAngle", Value::number(89)),
        // Recorded, not acted on: AutoJumpEnabled is for touch controls there are none of, and
        // EvaluateStateMachine turns off a state machine the engine runs regardless.
        P("AutoJumpEnabled", Value::boolean(true)), P("EvaluateStateMachine", Value::boolean(true)),
        // SetStateEnabled's record, a bit per HumanoidStateType value. Recorded, not acted on:
        // the engine enters every state whether or not a place disabled it.
        P("StatesDisabled", Value::number(0), Hidden | NoReplicate)});
    // FreeFalling(active) and Swimming(speed) fire on the StateName edges (rbx_runtime.cpp);
    // StateEnabledChanged from SetStateEnabled; ApplyDescriptionFinished from
    // ApplyDescriptionAsync. Climbing, FallingDown, GettingUp, PlatformStanding, Ragdoll and
    // Strafing are declared and never fired: the engine has none of those states.
    hum.events.insert(hum.events.end(), {"ApplyDescriptionFinished", "Climbing", "FallingDown", "FreeFalling", "GettingUp", "PlatformStanding",
                                         "Ragdoll", "StateEnabledChanged", "Strafing", "Swimming"});

    // What Humanoid:ApplyDescriptionAsync takes and Player:LoadCharacterWithHumanoidDescriptionAsync
    // spawns with. Applying one paints the six body colours onto the limbs; every asset id (body
    // parts, clothing, the face, accessories, animations), the scales and the emotes are recorded
    // and not worn: nothing here fetches an avatar asset or scales a rig. Asset ids are 0, scales
    // Roblox's (BodyTypeScale and ProportionScale 0, the rest 1), colours black, AccessoryBlob
    // an empty JSON list. EmotesBlob and EquippedEmotesBlob are the emote tables as JSON.
    // UseAvatarSettings' and StaticFacialAnimation's defaults are unverified: the type's zero.
    auto& hd = r.add("HumanoidDescription", "Instance");
    for (const char* n : {"Face", "Head", "Torso", "LeftArm", "LeftLeg", "RightArm", "RightLeg", "Shirt", "Pants", "GraphicTShirt",
                          "ClimbAnimation", "FallAnimation", "IdleAnimation", "JumpAnimation", "MoodAnimation", "RunAnimation",
                          "SwimAnimation", "WalkAnimation", "StaticFacialAnimation"})
        hd.props.push_back(P(n, Value::number(0)));
    for (const char* n : {"BackAccessory", "FaceAccessory", "FrontAccessory", "HairAccessory", "HatAccessory", "NeckAccessory",
                          "ShouldersAccessory", "WaistAccessory"})
        hd.props.push_back(P(n, Value::string("")));
    for (const char* n : {"HeadColor", "TorsoColor", "LeftArmColor", "LeftLegColor", "RightArmColor", "RightLegColor"})
        hd.props.push_back(P(n, Value::color3(0, 0, 0)));
    for (const char* n : {"DepthScale", "HeadScale", "HeightScale", "WidthScale"}) hd.props.push_back(P(n, Value::number(1)));
    hd.props.insert(hd.props.end(), {P("BodyTypeScale", Value::number(0)), P("ProportionScale", Value::number(0)),
                                     P("AccessoryBlob", Value::string("[]")), P("UseAvatarSettings", Value::boolean(false)),
                                     P("EmotesBlob", Value::string("{}"), Hidden | NotBrowsable), P("EquippedEmotesBlob", Value::string("[]"), Hidden | NotBrowsable)});
    hd.events = {"EmotesChanged", "EquippedEmotesChanged"};

    // A character's dressing. A BodyColors under a character paints its limbs (rbx_runtime.cpp
    // bodyColorsChanged): an R6 limb by name, an R15 limb by the group it belongs to. Clothing,
    // a ShirtGraphic and a CharacterMesh are recorded and not worn: nothing here textures a rig
    // or swaps a limb for a mesh. Their Content twins are in kPairs below.
    r.add("CharacterAppearance", "Instance", false);
    auto& bc = r.add("BodyColors", "CharacterAppearance");
    // Roblox's new-BodyColors palette: Bright yellow head and arms, Bright blue torso, Br.
    // yellowish green legs. Each BrickColor-typed name is its Color3 read through the palette.
    {
        const Col3 yellow{245 / 255.f, 205 / 255.f, 48 / 255.f}, blue{13 / 255.f, 105 / 255.f, 172 / 255.f}, green{164 / 255.f, 189 / 255.f, 71 / 255.f};
        struct LimbColor { const char* limb; Col3 c; };
        for (LimbColor lc : {LimbColor{"Head", yellow}, LimbColor{"LeftArm", yellow}, LimbColor{"RightArm", yellow}, LimbColor{"Torso", blue},
                             LimbColor{"LeftLeg", green}, LimbColor{"RightLeg", green}}) {
            std::string c3 = std::string(lc.limb) + "Color3", brick = std::string(lc.limb) + "Color";
            bc.props.push_back(P(c3.c_str(), Value::color3(lc.c.r, lc.c.g, lc.c.b)));
            PropDef b = P(brick.c_str(), Value::color3(lc.c.r, lc.c.g, lc.c.b), Brick);
            b.aliasOf = c3;
            bc.props.push_back(std::move(b));
        }
    }
    r.add("CharacterMesh", "CharacterAppearance").props = {PEnum("BodyPart", "BodyPart", "Head")};
    r.add("Clothing", "CharacterAppearance", false).props = {P("Color3", Value::color3(1, 1, 1))};
    r.add("Shirt", "Clothing");
    r.add("Pants", "Clothing");
    r.add("ShirtGraphic", "CharacterAppearance").props = {P("Color3", Value::color3(1, 1, 1))};

    // AnimationId is a path to a KeyframeSequence in the tree, not an asset id. A KeyframeSequence
    // holds Keyframes at a Time, each a tree of Poses named for the limbs they move.
    r.add("Animation", "Instance").props = {P("AnimationId", Value::string(""))};
    auto& animator = r.add("Animator", "Instance");
    animator.events = {"AnimationPlayed"};
    // Recorded, not acted on: nothing here throttles animation evaluation or picks a level of detail.
    animator.props = {P("EvaluationThrottled", Value::boolean(false), NotBrowsable | NoReplicate), P("PreferLodEnabled", Value::boolean(true))};
    auto& track = r.add("AnimationTrack", "Instance", false);
    track.props = {PRef("Animation", ReadOnly), P("IsPlaying", Value::boolean(false), ReadOnly), P("Length", Value::number(0), ReadOnly),
                   P("Looped", Value::boolean(false)), PEnum("Priority", "AnimationPriority", "Action"),
                   P("Speed", Value::number(1), ReadOnly), P("TimePosition", Value::number(0)),
                   P("WeightCurrent", Value::number(1), ReadOnly), P("WeightTarget", Value::number(1), ReadOnly),
                   // the weight fade the engine runs, and whether the track stops at its end
                   P("FadeTime", Value::number(0), Hidden), P("Stopping", Value::boolean(false), Hidden)};
    track.events = {"DidLoop", "Ended", "KeyframeReached", "Stopped"};
    // The clip a KeyframeSequence is one of. Length is recorded and not acted on (a track's
    // Length is read off the keyframes) and its default is unverified: the type's zero. Loop
    // and Priority stay declared on KeyframeSequence, the one concrete clip here.
    auto& clip = r.add("AnimationClip", "Instance", false);
    clip.props = {P("Length", Value::number(0))};
    r.add("KeyframeSequence", "Instance").props = {P("Loop", Value::boolean(true)), PEnum("Priority", "AnimationPriority", "Action"),
                                                   P("AuthoredHipHeight", Value::number(2))};
    for (auto& owned : r.owned) if (owned->name == "KeyframeSequence") owned->base = &clip;
    r.add("Keyframe", "Instance").props = {P("Time", Value::number(0))};
    // A named point in a Keyframe: AnimationTrack:GetMarkerReachedSignal(Name) fires with Value
    // when the track passes the keyframe. Its Value is a StringValue's.
    r.add("KeyframeMarker", "Instance").props = sv.props;
    r.add("NumberPose", "Instance").props = {P("Value", Value::number(0)), P("Weight", Value::number(1)),
                                             PEnum("EasingStyle", "PoseEasingStyle", "Linear"), PEnum("EasingDirection", "PoseEasingDirection", "Out")};
    r.add("AnimationController", "Instance");   // an Animator's host on a rig with no Humanoid
    // ColorMap, MetalnessMap, NormalMap and RoughnessMap are worn: they reach the material
    // through pulseblockz_world.cpp's material key. TexturePack, Roblox's newer bundled form
    // of the same four, is not read -- a place carrying only that gets an undressed mesh.
    // The avatar's cages below are stored and nothing draws from them.
    r.add("SurfaceAppearance", "Instance").props = {P("ColorMap", Value::string("")), P("MetalnessMap", Value::string("")), P("NormalMap", Value::string("")),
                                                    P("RoughnessMap", Value::string("")), PEnum("AlphaMode", "AlphaMode", "Overlay")};
    // CageOrigin and ImportOrigin are CFrames (cframeProp in rbx_api.cpp), PluginSecurity to write; their
    // World twins are the parent part's CFrame times them. Color and DebugMode are not declared (the
    // debug-mode enums are unverified here).
    auto& wrap = r.add("BaseWrap", "Instance", false); wrap.props = {P("CageMeshId", Value::string("")),
        P("CageOriginPosition", Value::vector3(0, 0, 0), Hidden | PluginWrite), P("CageOriginOrientation", Value::vector3(0, 0, 0), Hidden | PluginWrite),
        P("ImportOriginPosition", Value::vector3(0, 0, 0), Hidden | PluginWrite), P("ImportOriginOrientation", Value::vector3(0, 0, 0), Hidden | PluginWrite)};
    r.add("WrapTarget", "BaseWrap").props = {P("Stiffness", Value::number(0))};
    r.add("WrapLayer", "BaseWrap").props = {P("ReferenceMeshId", Value::string("")), P("Order", Value::number(1)), P("Puffiness", Value::number(1)),
                                            P("Enabled", Value::boolean(true)),
                                            // Recorded, not acted on: nothing here fits clothing to a body.
                                            PEnum("AutoSkin", "WrapLayerAutoSkin", "Disabled"),
                                            P("BindOffsetPosition", Value::vector3(0, 0, 0), Hidden | PluginWrite), P("BindOffsetOrientation", Value::vector3(0, 0, 0), Hidden | PluginWrite),
                                            P("ReferenceOriginPosition", Value::vector3(0, 0, 0), Hidden | PluginWrite), P("ReferenceOriginOrientation", Value::vector3(0, 0, 0), Hidden | PluginWrite)};
    // Declared so a place carrying one loads; nothing here deforms a mesh, so its methods are not answered.
    r.add("WrapDeformer", "BaseWrap");
    r.add("Pose", "Instance").props = {P("CFramePosition", Value::vector3(0, 0, 0), Hidden), P("CFrameOrientation", Value::vector3(0, 0, 0), Hidden),
                                       P("Weight", Value::number(1)), P("MaskWeight", Value::number(0)),
                                       PEnum("EasingStyle", "PoseEasingStyle", "Linear"), PEnum("EasingDirection", "PoseEasingDirection", "Out")};

    // Services / containers
    // Roblox's base of Workspace: the spatial queries and casts (registered on Workspace in rbx_api.cpp,
    // where BulkMoveTo and ArePartsTouchingOthers join them).
    auto& worldRoot = r.add("WorldRoot", "Model", false);
    auto& ws = r.service("Workspace", "Model");
    ws.base = &worldRoot;
    ws.props = {P("Gravity", Value::number(196.2)), PRef("CurrentCamera"), P("FallenPartsDestroyHeight", Value::number(-500)),
                // every side's own clock, as on Roblox; sending it would cost a packet per step per client
                P("DistributedGameTime", Value::number(0), ReadOnly | NoReplicate), P("StreamingEnabled", Value::boolean(false)),
                // Recorded, not acted on: the host's physics has no air, no wind and no fluid. AirDensity's
                // default is Roblox's documented 1.2; AirTurbulenceIntensity's is not verified here.
                P("AirDensity", Value::number(1.2)), P("AirTurbulenceIntensity", Value::number(0)),
                PEnum("FluidForces", "FluidForces", "Default"), P("GlobalWind", Value::vector3(0, 0, 0)),
                // Recorded, not acted on: no sales, no streaming, no fall-height gate (the default of
                // FallHeightEnabled is not verified here), and Studio's insert point is nothing to the runtime.
                P("AllowThirdPartySales", Value::boolean(false)), P("FallHeightEnabled", Value::boolean(false), PluginWrite),
                P("InsertPoint", Value::vector3(0, 0, 0)),
                P("StreamingMinRadius", Value::number(64)), P("StreamingTargetRadius", Value::number(1024)),
                PEnum("StreamingIntegrityMode", "StreamingIntegrityMode", "Default"), PEnum("StreamOutBehavior", "StreamOutBehavior", "Default"),
                PEnum("ModelStreamingBehavior", "ModelStreamingBehavior", "Default"),
                // Recorded, not acted on: Roblox's engine switches. Every one here runs its Default path;
                // signals fire immediately, physics steps with the host, touches ignore collision groups.
                PEnum("PhysicsSteppingMethod", "PhysicsSteppingMethod", "Default"), PEnum("SignalBehavior", "SignalBehavior", "Default"),
                P("TouchesUseCollisionGroups", Value::boolean(false)),
                PEnum("AvatarUnificationMode", "AvatarUnificationMode", "Default"), PEnum("ClientAnimatorThrottling", "ClientAnimatorThrottlingMode", "Default"),
                PEnum("MeshPartHeadsAndAccessories", "MeshPartHeadsAndAccessories", "Default"), PEnum("RejectCharacterDeletions", "RejectCharacterDeletions", "Default"),
                PEnum("ReplicateInstanceDestroySetting", "ReplicateInstanceDestroySetting", "Default"), PEnum("SandboxedInstanceMode", "SandboxedInstanceMode", "Default"),
                PEnum("PlayerCharacterDestroyBehavior", "PlayerCharacterDestroyBehavior", "Default"), PEnum("IKControlConstraintSupport", "IKControlConstraintSupport", "Default"),
                PEnum("PrimalPhysicsSolver", "PrimalPhysicsSolver", "Default"), PEnum("RenderingCacheOptimizations", "RenderingCacheOptimizationMode", "Default"),
                PEnum("PathfindingUseImprovedSearch", "PathfindingUseImprovedSearch", "Default"), PEnum("Retargeting", "AnimatorRetargetingMode", "Default")};
    // Recorded, not acted on: nothing here streams, so PersistentLoaded never fires.
    ws.events = {"PersistentLoaded"};
    auto& cam = r.add("Camera", "Instance");
    cam.props = {P("FieldOfView", Value::number(70)), PRef("CameraSubject"), P("Position", Value::vector3(0, 0, 0)),
                 P("Orientation", Value::vector3(0, 0, 0)), PEnum("CameraType", "CameraType", "Custom"),
                 P("ViewportSize", Value::vector2(1024, 768), ReadOnly),
                 // DiagonalFieldOfView and MaxAxisFieldOfView are FieldOfView through the viewport's aspect (rbx_api.cpp).
                 // FieldOfViewMode is recorded, not acted on: the host projects by the vertical FieldOfView whatever it says.
                 PEnum("FieldOfViewMode", "FieldOfViewMode", "Vertical"),
                 // Focus is a CFrame (cframeProp in rbx_api.cpp), recorded, not acted on: nothing here lights or streams around it.
                 P("FocusPosition", Value::vector3(0, 0, 0), Hidden), P("FocusOrientation", Value::vector3(0, 0, 0), Hidden),
                 // Recorded, not acted on: no VR headset here.
                 P("HeadLocked", Value::boolean(true)), P("HeadScale", Value::number(1)), P("VRTiltAndRollEnabled", Value::boolean(false)),
                 // Roblox's near clip, read only; the host's own near plane is not this figure.
                 P("NearPlaneZ", Value::number(-0.1), ReadOnly)};
    auto& light = r.service("Lighting");
    light.props = {P("Ambient", Value::color3(0, 0, 0)), P("OutdoorAmbient", Value::color3(0.5f, 0.5f, 0.5f)),
                   P("Brightness", Value::number(3)), P("ClockTime", Value::number(14)), P("TimeOfDay", Value::string("14:00:00")),
                   P("GlobalShadows", Value::boolean(true)), P("FogEnd", Value::number(100000)), P("FogStart", Value::number(0)),
                   P("FogColor", Value::color3(0.75f, 0.75f, 0.75f)), P("ExposureCompensation", Value::number(0)),
                   P("ColorShift_Top", Value::color3(0, 0, 0)), P("ColorShift_Bottom", Value::color3(0, 0, 0)),
                   // How much of the sky's own light lands on the world. Roblox adds this on top
                   // of OutdoorAmbient, which is why a place under a bright skybox is lit in the
                   // shade and one under a dark sky is not.
                   P("EnvironmentDiffuseScale", Value::number(1)), P("EnvironmentSpecularScale", Value::number(1)),
                   // Recorded, not acted on: the sun's arc here is over the equator whatever the latitude
                   // (GetSunDirection follows the arc drawn), shadows are as sharp as the host draws them,
                   // a light's Range is not capped at 60 or 120, and there is one lighting style.
                   P("GeographicLatitude", Value::number(41.733)), P("ShadowSoftness", Value::number(0.2)),
                   PEnum("ExtendLightRangeTo120", "RolloutState", "Default", NoScriptRead | NoScriptWrite),
                   PEnum("LightingStyle", "LightingStyle", "Realistic", NoScriptRead | NoScriptWrite),
                   P("PrioritizeLightingQuality", Value::boolean(false), NoScriptRead | NoScriptWrite)};
    // LightingChanged(skyChanged): on any property of Lighting, and with true on a Sky's under it (rbx_runtime.cpp).
    light.events = {"LightingChanged"};
    // Lighting's children; post-processing also works under the Camera.
    auto& fx = r.add("PostEffect", "Instance", false);
    fx.props = {P("Enabled", Value::boolean(true))};
    r.add("BloomEffect", "PostEffect").props = {P("Intensity", Value::number(0.4)), P("Size", Value::number(24)), P("Threshold", Value::number(0.95))};
    r.add("BlurEffect", "PostEffect").props = {P("Size", Value::number(24))};
    r.add("ColorCorrectionEffect", "PostEffect").props = {P("Brightness", Value::number(0)), P("Contrast", Value::number(0)),
                                                          P("Saturation", Value::number(0)), P("TintColor", Value::color3(1, 1, 1))};
    r.add("DepthOfFieldEffect", "PostEffect").props = {P("FarIntensity", Value::number(0.75)), P("FocusDistance", Value::number(0.05)),
                                                       P("InFocusRadius", Value::number(10)), P("NearIntensity", Value::number(0.75))};
    r.add("SunRaysEffect", "PostEffect").props = {P("Intensity", Value::number(0.25)), P("Spread", Value::number(1))};
    // Recorded, not acted on: the host's tonemapper is filmic whichever preset is named.
    r.add("ColorGradingEffect", "PostEffect").props = {PEnum("TonemapperPreset", "TonemapperPreset", "Default")};
    // Recorded, not acted on: nothing here draws a cloud layer. Color and Enabled are not declared.
    // Roblox's Cover and Density defaults are as best known here.
    r.add("Clouds", "Instance").props = {P("Cover", Value::number(0.5)), P("Density", Value::number(0.7))};
    r.add("Atmosphere", "Instance").props = {P("Density", Value::number(0.395)), P("Offset", Value::number(0)),
                                             P("Color", Value::color3(199 / 255.f, 170 / 255.f, 107 / 255.f)), P("Decay", Value::color3(106 / 255.f, 112 / 255.f, 125 / 255.f)),
                                             P("Glare", Value::number(0)), P("Haze", Value::number(0))};
    r.add("Sky", "Instance").props = {P("CelestialBodiesShown", Value::boolean(true)), P("MoonAngularSize", Value::number(11)), P("MoonTextureId", Value::string("")),
                                      P("StarCount", Value::number(3000)), P("SunAngularSize", Value::number(21)), P("SunTextureId", Value::string("")),
                                      P("SkyboxBk", Value::string("")), P("SkyboxDn", Value::string("")), P("SkyboxFt", Value::string("")),
                                      P("SkyboxLf", Value::string("")), P("SkyboxRt", Value::string("")), P("SkyboxUp", Value::string("")),
                                      // Turns the six faces, degrees about Y, X then Z as a part's Orientation; the sun and moon stay put.
                                      P("SkyboxOrientation", Value::vector3(0, 0, 0))};
    r.service("ReplicatedStorage");
    r.service("ReplicatedFirst");   // RemoveDefaultLoadingScreen: no loading screen is drawn here, so there is nothing to remove (rbx_api.cpp)
    r.service("ServerStorage");
    // LoadStringEnabled is recorded, not acted on: there is no loadstring here.
    r.service("ServerScriptService").props = {P("LoadStringEnabled", Value::boolean(false))};
    auto& sp = r.service("StarterPlayer");
    sp.props = {P("CameraMaxZoomDistance", Value::number(400)), P("CameraMinZoomDistance", Value::number(0.5)),
                P("CharacterWalkSpeed", Value::number(16)), P("CharacterJumpPower", Value::number(50)),
                P("CharacterJumpHeight", Value::number(7.2)), P("CharacterUseJumpPower", Value::boolean(false)),
                P("CharacterMaxSlopeAngle", Value::number(89)), P("HealthDisplayDistance", Value::number(100)),
                P("NameDisplayDistance", Value::number(100)), P("EnableMouseLockOption", Value::boolean(true)),
                PEnum("CharacterRigType", "HumanoidRigType", "R6"),
                P("LoadCharacterAppearance", Value::boolean(true)),   // as Player.CanLoadCharacterAppearance, for everyone
                PEnum("DevComputerMovementMode", "DevComputerMovementMode", "UserChoice")};
    // Seeds for each Player at join (rbx_runtime.cpp addPlayer); see Player for what each does
    // there. AllowCustomAnimations is Roblox's own. CharacterBreakJointsOnDeath, ClassicDeath,
    // CreateDefaultPlayerModule, EnableDynamicHeads, LoadCharacterLayeredClothing and
    // UserEmotesEnabled are recorded and not acted on: nothing here breaks a dead rig, plays
    // a death, loads a player module, a dynamic head, layered clothing or an emote menu.
    // ClassicDeath's and CreateDefaultPlayerModule's defaults are unverified: the type's zero.
    sp.props.insert(sp.props.end(), {
        P("AutoJumpEnabled", Value::boolean(true)), PEnum("CameraMode", "CameraMode", "Classic"),
        PEnum("DevCameraOcclusionMode", "DevCameraOcclusionMode", "Zoom"),
        PEnum("DevComputerCameraMovementMode", "DevComputerCameraMovementMode", "UserChoice"),
        PEnum("DevTouchCameraMovementMode", "DevTouchCameraMovementMode", "UserChoice"),
        PEnum("DevTouchMovementMode", "DevTouchMovementMode", "UserChoice"),
        P("AllowCustomAnimations", Value::boolean(true), NoScriptRead | NoScriptWrite),
        P("CharacterBreakJointsOnDeath", Value::boolean(true)), P("ClassicDeath", Value::boolean(false)),
        P("CreateDefaultPlayerModule", Value::boolean(false)), PEnum("EnableDynamicHeads", "LoadDynamicHeads", "Default"),
        PEnum("LoadCharacterLayeredClothing", "LoadCharacterLayeredClothing", "Default"), P("UserEmotesEnabled", Value::boolean(true))});
    // A Player's own copies of the two display distances, seeded from these at join.
    for (const char* n : {"NameDisplayDistance", "HealthDisplayDistance"}) player.props.push_back(*sp.findProp(n));
    r.add("StarterPlayerScripts", "Instance", false);
    r.add("StarterCharacterScripts", "Instance", false);
    // CoreGuiHidden is the set of CoreGuiTypes SetCoreGuiEnabled has turned off, as bits.
    auto& sgui = r.service("StarterGui");
    sgui.props = {P("ShowDevelopmentGui", Value::boolean(true)), P("CoreGuiHidden", Value::number(0), ReadOnly | NoReplicate | Hidden),
                  // where SetCore("TopbarEnabled") and SetCore("ChatActive") land
                  P("TopbarEnabled", Value::boolean(true), ReadOnly | NoReplicate | Hidden), P("ChatActive", Value::boolean(true), ReadOnly | NoReplicate | Hidden)};
    sgui.base = r.find("BasePlayerGui");
    // Recorded, not acted on: no device turns, no virtual cursor and no right-to-left shaping here,
    // and ProcessUserInput changes nothing about the edit world's GUIs.
    sgui.props.insert(sgui.props.end(), {PEnum("ScreenOrientation", "ScreenOrientation", "LandscapeSensor"), PEnum("VirtualCursorMode", "VirtualCursorMode", "Default"),
                                         PEnum("RtlTextSupport", "RtlTextSupport", "Default"), P("ProcessUserInput", Value::boolean(false), PluginWrite)});
    r.service("StarterPack");
    r.service("Teams");
    auto& team = r.add("Team", "Instance");
    team.props = {P("TeamColor", Value::color3(0xF2 / 255.f, 0xF3 / 255.f, 0xF3 / 255.f), Brick), P("AutoAssignable", Value::boolean(true))};
    team.events = {"PlayerAdded", "PlayerRemoved"};
    team.events = {"PlayerAdded", "PlayerRemoved"};
    auto& rs = r.service("RunService");
    rs.events = {"Heartbeat", "Stepped", "RenderStepped", "PreRender", "PreSimulation", "PostSimulation", "PreAnimation"};
    // Roblox's client-side prediction: nothing here predicts or rolls back, so neither fires.
    rs.events.push_back("Misprediction");
    rs.events.push_back("Rollback");
    // HttpEnabled is the host's option, read and (with the Plugin capability) written through rbx_api.cpp.
    r.service("HttpService");
    r.service("TweenService");
    // The base of Tween, its only subclass here: PlaybackState, Completed, Play, Pause and Cancel are
    // declared on Tween. A Tween's Instance and TweenInfo are read from the running tween (rbx_api.cpp).
    r.add("TweenBase", "Instance", false);
    auto& tween = r.add("Tween", "TweenBase", false);
    tween.props = {PEnum("PlaybackState", "PlaybackState", "Begin", ReadOnly)};
    tween.events = {"Completed"};
    r.service("Debris");
    // ---- the Studio's plugins: scripts under PluginDebugService, each with a `plugin` ----
    r.service("PluginDebugService");
    auto& selection = r.service("Selection");
    selection.events = {"SelectionChanged"};
    selection.props = {P("SelectionThickness", Value::number(0))};   // recorded, not acted on; its Roblox default is not verified here
    r.service("ChangeHistoryService").events = {"OnUndo", "OnRedo", "OnRecordingStarted", "OnRecordingFinished"};
    auto& coreGui = r.service("CoreGui");                     // where a plugin's own GUIs go
    coreGui.base = r.find("BasePlayerGui");
    // Recorded, not acted on: no gamepad selection here. Version's Roblox default is not verified here.
    coreGui.props = {PRef("SelectionImageObject", NoScriptRead | NoScriptWrite), P("Version", Value::number(0))};
    auto& plugin = r.add("Plugin", "Instance", false);
    plugin.props = {P("CollisionEnabled", Value::boolean(false)), P("GridSize", Value::number(1))};
    // The engine's own, unreadable to a plugin. IsDebuggable's Roblox default is not verified here.
    plugin.props.push_back(P("DisableUIDragDetectorDrags", Value::boolean(false), NoScriptRead | NoScriptWrite));
    plugin.props.push_back(P("IsDebuggable", Value::boolean(false), NoScriptRead | NoScriptWrite));
    plugin.events = {"Deactivation", "Unloading"};
    r.add("PluginToolbar", "Instance", false);
    auto& pluginButton = r.add("PluginToolbarButton", "Instance", false);
    pluginButton.props = {P("Icon", Value::string("")), P("Enabled", Value::boolean(true)), P("ClickableWhenViewportHidden", Value::boolean(false))};
    pluginButton.events = {"Click"};
    r.add("PluginMouse", "Mouse", false).events = {"DragEnter"};   // never fires: Plugin:StartDrag drags nothing here
    // CreatePluginAction fills the three from its arguments; Text: kDataShared.
    auto& pluginAction = r.add("PluginAction", "Instance", false);
    pluginAction.events = {"Triggered"};
    pluginAction.props = {P("ActionId", Value::string(""), ReadOnly), P("AllowBinding", Value::boolean(true), ReadOnly), P("StatusTip", Value::string(""), ReadOnly)};
    // Plugin:CreatePluginMenu makes one; its actions are its children. Nothing here draws a menu:
    // ShowAsync answers nil at once. Icon, Title and Visible: kDataShared.
    r.add("PluginMenu", "Instance", false);
    // What a PluginDragDropped / PluginDragEntered handler would receive; nothing here starts a drag.
    r.add("PluginDragEvent", "Instance", false).props = {P("Data", Value::string("")), P("MimeType", Value::string("")), P("Sender", Value::string(""))};
    auto& pluginGui = r.add("PluginGui", "LayerCollector", false);
    pluginGui.props = {P("Title", Value::string(""))};
    pluginGui.events = {"WindowFocused", "WindowFocusReleased", "PluginDragDropped"};
    pluginGui.events.insert(pluginGui.events.end(), {"PluginDragEntered", "PluginDragLeft", "PluginDragMoved"});   // never fire: no drag here
    auto& dock = r.add("DockWidgetPluginGui", "PluginGui", false);
    dock.props = {P("FloatingXSize", Value::number(300), Hidden), P("FloatingYSize", Value::number(200), Hidden),   // its frame, from the DockWidgetPluginGuiInfo
                  P("HostWidgetWasRestored", Value::boolean(false), ReadOnly)};   // false: the Studio here restores no widget layout
    // TagAdded fires when a tag reaches its first instance in the tree, TagRemoved when it leaves its last (rbx_runtime.cpp).
    r.service("CollectionService").events = {"TagAdded", "TagRemoved"};
    // CreatePath(agentParameters) -> a Path: ComputeAsync(start, goal) sets Status and the waypoints
    // ({Position, Action, Label}) GetWaypoints returns; Blocked / Unblocked carry a waypoint index.
    r.service("PathfindingService");
    // UnionAsync / SubtractAsync / IntersectAsync; the host does them (rbx_api.cpp).
    r.service("GeometryService");
    auto& path = r.add("Path", "Instance", false);
    path.props = {PEnum("Status", "PathStatus", "NoPath", ReadOnly)};
    path.events = {"Blocked", "Unblocked"};
    // In a part, Label is the key looked up in the agent's Costs table; PassThrough makes the part
    // air to the search rather than merely cheap to cross.
    auto& pmod = r.add("PathfindingModifier", "Instance");
    pmod.props = {P("Label", Value::string("")), P("PassThrough", Value::boolean(false)), P("ModifierEnabled", Value::boolean(true))};
    auto& plink = r.add("PathfindingLink", "Instance");
    plink.props = {PRef("Attachment0"), PRef("Attachment1"), P("Label", Value::string("")), P("IsBidirectional", Value::boolean(true))};
    // SoundId is a file in the project. In a part the sound is 3D (RollOff*), anywhere else it is
    // everywhere. Both sides run the clock, so Ended fires on the server too.
    auto& snd = r.add("Sound", "Instance");
    snd.props = {P("SoundId", Value::string("")), P("Volume", Value::number(0.5)), P("PlaybackSpeed", Value::number(1)),
                 P("Looped", Value::boolean(false)), P("Playing", Value::boolean(false)), P("TimePosition", Value::number(0)),
                 P("IsPlaying", Value::boolean(false), ReadOnly | NoReplicate), P("TimeLength", Value::number(0), ReadOnly | NoReplicate),
                 // IsPlaying's opposite, written beside it
                 P("IsPaused", Value::boolean(true), ReadOnly | NoReplicate),
                 P("IsLoaded", Value::boolean(false), ReadOnly | NoReplicate), P("PlayOnRemove", Value::boolean(false)),
                 P("RollOffMaxDistance", Value::number(10000)), P("RollOffMinDistance", Value::number(10)),
                 PEnum("RollOffMode", "RollOffMode", "Inverse"), P("PlaybackLoudness", Value::number(0), ReadOnly | NoReplicate),
                 PRef("SoundGroup")};
    snd.events = {"Played", "Stopped", "Paused", "Resumed", "Ended", "Loaded", "DidLoop"};
    // With PlaybackRegionsEnabled the clock runs inside PlaybackRegion and, when Looped, wraps
    // inside LoopRegion (both in seconds, clamped to the clip); off, the whole clip plays.
    snd.props.insert(snd.props.end(), {P("PlaybackRegionsEnabled", Value::boolean(false)),
                                       P("PlaybackRegion", Value::numberRange(0, 60000)), P("LoopRegion", Value::numberRange(0, 60000))});
    // Scales every Sound in it; groups nest.
    r.add("SoundGroup", "Instance").props = {P("Volume", Value::number(1)), PRef("SoundGroup")};
    // Stored with their settings; the mixer here does not apply them.
    auto& sfx = r.add("SoundEffect", "Instance", false); sfx.props = {P("Enabled", Value::boolean(true)), P("Priority", Value::number(0))};
    r.add("EqualizerSoundEffect", "SoundEffect").props = {P("HighGain", Value::number(0)), P("MidGain", Value::number(0)), P("LowGain", Value::number(0))};
    r.add("DistortionSoundEffect", "SoundEffect").props = {P("Level", Value::number(0.75))};
    r.add("ReverbSoundEffect", "SoundEffect").props = {P("DecayTime", Value::number(1.5)), P("Density", Value::number(1)), P("Diffusion", Value::number(1)),
                                                        P("DryLevel", Value::number(0)), P("WetLevel", Value::number(0))};
    r.add("EchoSoundEffect", "SoundEffect").props = {P("Delay", Value::number(1)), P("DryLevel", Value::number(0)), P("Feedback", Value::number(0.5)), P("WetLevel", Value::number(0))};
    r.add("PitchShiftSoundEffect", "SoundEffect").props = {P("Octave", Value::number(1.25))};
    r.add("CompressorSoundEffect", "SoundEffect").props = {P("Attack", Value::number(0.1)), P("GainMakeup", Value::number(0)), P("Ratio", Value::number(40)),
                                                            P("Release", Value::number(0.1)), P("Threshold", Value::number(-40)), PRef("SideChain")};
    r.add("ChorusSoundEffect", "SoundEffect").props = {P("Depth", Value::number(0.15)), P("Mix", Value::number(0.5)), P("Rate", Value::number(0.5))};
    r.add("FlangeSoundEffect", "SoundEffect").props = {P("Depth", Value::number(0.45)), P("Mix", Value::number(0.85)), P("Rate", Value::number(5))};
    r.add("TremoloSoundEffect", "SoundEffect").props = {P("Depth", Value::number(1)), P("Duty", Value::number(0.5)), P("Frequency", Value::number(5))};
    auto& ss = r.service("SoundService");
    ss.props = {P("RespectFilteringEnabled", Value::boolean(true)),
                // where the Audio API is heard from; Default and Camera both mean the camera
                PEnum("DefaultListenerLocation", "ListenerLocation", "Default"),
                // SoundService:SetListener: Camera is the engine's own ear, the others place one each frame
                PEnum("ListenerType", "ListenerType", "Camera", Hidden),
                P("ListenerObject", Value::instance(0), Hidden),
                P("ListenerPosition", Value::vector3(0, 0, 0), Hidden),
                P("ListenerOrientation", Value::vector3(0, 0, 0), Hidden)};
    // Recorded, not acted on: nothing here reverberates, scales distance or doppler, or shapes a
    // sound to its part's volume, and the character sounds are the engine's own.
    ss.props.insert(ss.props.end(), {PEnum("AmbientReverb", "ReverbType", "NoReverb"), P("DistanceFactor", Value::number(3.33)),
                                     P("DopplerScale", Value::number(1)), P("RolloffScale", Value::number(1)),
                                     PEnum("VolumetricAudio", "VolumetricAudio", "Automatic"),
                                     P("CharacterSoundsUseNewApi", Value::boolean(false), PluginWrite)});

    // ---- the Audio API ----------------------------------------------------------------------
    // Instances joined by Wires, one's pin to another's: an AudioPlayer's Output, an effect's Input
    // and Output, an AudioEmitter broadcasting from its parent, an AudioDeviceOutput to the
    // speakers. sync_audio in the host walks each emitter's wires back to its players. The defaults
    // below are Studio's, taken from rbx-dom's dump of it; the docs state few of them.
    auto& aplayer = r.add("AudioPlayer", "Instance");
    aplayer.props = {P("Asset", Value::string("")), P("AutoLoad", Value::boolean(true)), P("AutoPlay", Value::boolean(false)),
                     // read-only to scripts but replicated; Play() and Stop() write it
                     P("IsPlaying", Value::boolean(false), ReadOnly),
                     P("IsReady", Value::boolean(false), ReadOnly | NoReplicate),
                     P("Looping", Value::boolean(false)),
                     P("LoopRegion", Value::numberRange(0, 60000)), P("PlaybackRegion", Value::numberRange(0, 60000)),
                     P("PlaybackSpeed", Value::number(1)), P("TimeLength", Value::number(0), ReadOnly | NoReplicate),
                     P("TimePosition", Value::number(0)), P("Volume", Value::number(1))};
    aplayer.events = {"Ended", "Looped", "WiringChanged"};
    auto& afilter = r.add("AudioFilter", "Instance");
    afilter.props = {P("Bypass", Value::boolean(false)), PEnum("FilterType", "AudioFilterType", "Peak"),
                     P("Frequency", Value::number(2000)), P("Gain", Value::number(0)), P("Q", Value::number(0.707))};
    afilter.events = {"WiringChanged"};
    auto& areverb = r.add("AudioReverb", "Instance");
    areverb.props = {P("Bypass", Value::boolean(false)), P("DecayRatio", Value::number(0.5)), P("DecayTime", Value::number(1.5)),
                     P("Density", Value::number(1)), P("Diffusion", Value::number(1)), P("DryLevel", Value::number(0)),
                     P("EarlyDelayTime", Value::number(0.02)), P("HighCutFrequency", Value::number(20000)),
                     P("LateDelayTime", Value::number(0.04)), P("LowShelfFrequency", Value::number(250)),
                     P("LowShelfGain", Value::number(0)), P("ReferenceFrequency", Value::number(5000)), P("WetLevel", Value::number(-6))};
    areverb.events = {"WiringChanged"};
    // The two curves are a hidden string of distance:volume pairs; Set/GetDistanceAttenuation reach
    // them. On Roblox they are Flyweights, there to replicate and not for scripts, so nothing is
    // gained by standing instances up for them here.
    auto& aemitter = r.add("AudioEmitter", "Instance");
    aemitter.props = {P("AcousticSimulationEnabled", Value::boolean(true)), P("AudioInteractionGroup", Value::string("")),
                      P("DistanceAttenuationBounds", Value::numberRange(4, 10000)),
                      PEnum("DistanceAttenuationMode", "DistanceAttenuationMode", "Custom"),
                      PEnum("PositionType", "EmitterPositionType", "Parent"), PRef("PositionInstance"),
                      P("DistanceAttenuation", Value::string(""), Hidden | NoScriptRead | NoScriptWrite),
                      P("AngleAttenuation", Value::string(""), Hidden | NoScriptRead | NoScriptWrite)};
    aemitter.events = {"WiringChanged"};
    auto& alistener = r.add("AudioListener", "Instance");
    alistener.props = {P("AcousticSimulationEnabled", Value::boolean(true)), P("AudioInteractionGroup", Value::string("")),
                       PEnum("PositionType", "ListenerPositionType", "Parent"), PRef("PositionInstance"),
                       P("DistanceAttenuation", Value::string(""), Hidden | NoScriptRead | NoScriptWrite),
                       P("AngleAttenuation", Value::string(""), Hidden | NoScriptRead | NoScriptWrite)};
    alistener.events = {"WiringChanged"};
    auto& adevice = r.add("AudioDeviceOutput", "Instance");
    adevice.props = {PRef("Player")};
    adevice.events = {"WiringChanged"};
    // Carries a stream from SourceInstance's SourceName pin to TargetInstance's TargetName pin.
    // Connected is the engine's verdict: both ends set, both pins real, no loop made.
    auto& wire = r.add("Wire", "Instance");
    wire.props = {PRef("SourceInstance"), P("SourceName", Value::string("Output")),
                  PRef("TargetInstance"), P("TargetName", Value::string("Input")),
                  P("Connected", Value::boolean(false), ReadOnly | NoReplicate)};
    // The rest of the graph. Every effect has an Input and an Output pin and is heard through the
    // host mixer as the Godot effect nearest it (audio_effects_for); a member the mixer cannot
    // shape says so below. Defaults are Studio's, from rbx-dom's dump.
    auto& afader = r.add("AudioFader", "Instance");   // Volume: the gain, 0 to 3, in kAudioShared
    afader.props = {P("Bypass", Value::boolean(false))};
    afader.events = {"WiringChanged"};
    // DryLevel / WetLevel (dB) are in kAudioShared. RampTime is recorded, not acted on: a DelayTime
    // change lands at once here rather than sliding over RampTime seconds.
    auto& aecho = r.add("AudioEcho", "Instance");
    aecho.props = {P("Bypass", Value::boolean(false)), P("DelayTime", Value::number(1)), P("Feedback", Value::number(0.5)), P("RampTime", Value::number(0))};
    aecho.events = {"WiringChanged"};
    // A Sidechain pin is accepted by Wires; recorded, not acted on: the mixer compresses on the
    // Input alone.
    auto& acomp = r.add("AudioCompressor", "Instance");
    acomp.props = {P("Attack", Value::number(0.1)), P("Bypass", Value::boolean(false)), P("MakeupGain", Value::number(0)),
                   P("Ratio", Value::number(40)), P("Release", Value::number(0.1)), P("Threshold", Value::number(-40))};
    acomp.events = {"WiringChanged"};
    auto& alimiter = r.add("AudioLimiter", "Instance");
    alimiter.props = {P("Bypass", Value::boolean(false)), P("MaxLevel", Value::number(0)), P("Release", Value::number(0.01))};
    alimiter.events = {"WiringChanged"};
    auto& adistort = r.add("AudioDistortion", "Instance");
    adistort.props = {P("Bypass", Value::boolean(false)), P("Level", Value::number(0.5))};
    adistort.events = {"WiringChanged"};
    // MidRange is recorded, not acted on: the mixer's three bands sit where Godot's six do.
    auto& aeq = r.add("AudioEqualizer", "Instance");
    aeq.props = {P("Bypass", Value::boolean(false)), P("HighGain", Value::number(0)), P("LowGain", Value::number(0)),
                 P("MidGain", Value::number(0)), P("MidRange", Value::numberRange(400, 4000))};
    aeq.events = {"WiringChanged"};
    auto& achorus = r.add("AudioChorus", "Instance");   // Rate (Hz) in kAudioShared
    achorus.props = {P("Bypass", Value::boolean(false)), P("Depth", Value::number(0.45)), P("Mix", Value::number(0.85))};
    achorus.events = {"WiringChanged"};
    auto& aflanger = r.add("AudioFlanger", "Instance");
    aflanger.props = achorus.props;
    aflanger.events = {"WiringChanged"};
    // Recorded, not acted on: Godot has no noise gate, so the stream passes through unchanged.
    auto& agate = r.add("AudioGate", "Instance");
    agate.props = {P("Attack", Value::number(0.01)), P("Bypass", Value::boolean(false)), P("Release", Value::number(0.1)),
                   P("Threshold", Value::numberRange(-36, -24))};
    agate.events = {"WiringChanged"};
    // WindowSize is recorded, not acted on: the mixer's shifter picks its own FFT size.
    auto& apitch = r.add("AudioPitchShifter", "Instance");
    apitch.props = {P("Bypass", Value::boolean(false)), P("Pitch", Value::number(1.25)), PEnum("WindowSize", "AudioWindowSize", "Medium")};
    apitch.events = {"WiringChanged"};
    // Frequency / Shape in kAudioShared. Shape, Skew and Square are recorded, not acted on: the
    // mixer's tremolo is the SoundEffect's, a sine of Depth, Duty and Frequency.
    auto& atremolo = r.add("AudioTremolo", "Instance");
    atremolo.props = {P("Bypass", Value::boolean(false)), P("Depth", Value::number(1)), P("Duty", Value::number(0.5)),
                      P("Skew", Value::number(0)), P("Square", Value::number(0))};
    atremolo.events = {"WiringChanged"};
    // A sink with an Input pin. Recorded, not acted on: nothing here measures a stream, so
    // PeakLevel and RmsLevel stay 0 and GetSpectrum is empty.
    auto& aanalyzer = r.add("AudioAnalyzer", "Instance");
    aanalyzer.props = {P("PeakLevel", Value::number(0), ReadOnly | NoReplicate), P("RmsLevel", Value::number(0), ReadOnly | NoReplicate),
                       P("SpectrumEnabled", Value::boolean(true)), PEnum("WindowSize", "AudioWindowSize", "Medium")};
    aanalyzer.events = {"WiringChanged"};
    // Layout names the pins (audioChannelPins): a mixer's inputs, a splitter's outputs. Recorded,
    // not acted on: the mixer here carries every voice whole, so a Wire into one channel is a
    // Wire into all of it and a Wire out of one channel is the whole stream.
    auto& amixer = r.add("AudioChannelMixer", "Instance");
    amixer.props = {PEnum("Layout", "AudioChannelLayout", "Stereo")};
    amixer.events = {"WiringChanged"};
    auto& asplitter = r.add("AudioChannelSplitter", "Instance");
    asplitter.props = amixer.props;
    asplitter.events = {"WiringChanged"};
    // A microphone with an Output pin. Recorded, not acted on: nothing here captures a
    // microphone, so IsReady stays false and Active (in kAudioShared) says only what was stored.
    // AccessList is Get/SetUserIdAccessList's ids, comma-joined.
    auto& ainput = r.add("AudioDeviceInput", "Instance");
    ainput.props = {PEnum("AccessType", "AccessModifierType", "Deny"), P("IsReady", Value::boolean(false), ReadOnly | NoReplicate),
                    P("Muted", Value::boolean(false)), P("AccessList", Value::string(""), Hidden | NoScriptRead | NoScriptWrite)};
    ainput.events = {"WiringChanged"};
    // Recorded, not acted on: nothing here records, so CanRecordAsync is false and RecordAsync
    // refuses as Roblox does when recording is not permitted.
    auto& arecorder = r.add("AudioRecorder", "Instance");
    arecorder.props = {P("IsRecording", Value::boolean(false), ReadOnly), P("TimeLength", Value::number(0), ReadOnly | NoReplicate)};
    arecorder.events = {"WiringChanged"};
    // Studio's audio search (AssetService:SearchAudio) has no counterpart here; the fields are
    // stored. AudioSubType's Roblox default is not documented: Music is picked.
    auto& asearch = r.add("AudioSearchParams", "Instance");
    asearch.props = {P("Album", Value::string("")), P("Artist", Value::string("")), PEnum("AudioSubType", "AudioSubType", "Music"),
                     P("MaxDuration", Value::number(0)), P("MinDuration", Value::number(0)), P("SearchKeyword", Value::string("")), P("Tag", Value::string(""))};
    // Recorded, not acted on: nothing here transcribes, so Text (in kAudioShared) is only what was
    // stored and VoiceDetected stays false.
    auto& astt = r.add("AudioSpeechToText", "Instance");
    astt.props = {P("VoiceDetected", Value::boolean(false), ReadOnly | NoReplicate)};
    astt.events = {"WiringChanged"};
    // Recorded, not acted on: nothing here synthesizes speech. LoadAsync leaves IsLoaded false and
    // TimeLength 0; Play and Pause only set IsPlaying, and no Ended or Looped fires.
    auto& atts = r.add("AudioTextToSpeech", "Instance");
    atts.props = {P("IsLoaded", Value::boolean(false), ReadOnly | NoReplicate), P("IsPlaying", Value::boolean(false), ReadOnly),
                  P("Looping", Value::boolean(false)), P("Pitch", Value::number(0)), P("TimeLength", Value::number(0), ReadOnly | NoReplicate),
                  P("VoiceId", Value::string(""))};
    atts.events = aplayer.events;
    auto& uis = r.service("UserInputService");
    uis.props = {PEnum("MouseBehavior", "MouseBehavior", "Default"),
                 P("MouseIconEnabled", Value::boolean(true)),
                 P("MouseDeltaSensitivity", Value::number(1)),
                 P("KeyboardEnabled", Value::boolean(true), ReadOnly),
                 P("MouseEnabled", Value::boolean(true), ReadOnly),
                 P("TouchEnabled", Value::boolean(false), ReadOnly),
                 P("GamepadEnabled", Value::boolean(false), ReadOnly),
                 P("AccelerometerEnabled", Value::boolean(false), ReadOnly),
                 P("GyroscopeEnabled", Value::boolean(false), ReadOnly)};
    uis.events = {"InputBegan", "InputEnded", "InputChanged", "JumpRequest", "TouchTap", "WindowFocused", "WindowFocusReleased", "TextBoxFocused", "TextBoxFocusReleased"};
    // No on-screen keyboard, touch screen, gamepad or motion sensor here: these read their idle
    // values and the events below never fire, except LastInputTypeChanged (Runtime::input).
    uis.props.insert(uis.props.end(), {P("OnScreenKeyboardVisible", Value::boolean(false), ReadOnly), P("OnScreenKeyboardPosition", Value::vector2(0, 0), ReadOnly),
                                       P("OnScreenKeyboardSize", Value::vector2(0, 0), ReadOnly), PEnum("PreferredInput", "PreferredInput", "KeyboardAndMouse", ReadOnly),
                                       P("TouchScreenEnabled", Value::boolean(false), NoScriptRead | NoScriptWrite)});
    uis.events.insert(uis.events.end(), {"LastInputTypeChanged", "GamepadConnected", "GamepadDisconnected", "DeviceAccelerationChanged", "DeviceGravityChanged",
                                         "DeviceRotationChanged", "PointerAction", "TouchStarted", "TouchMoved", "TouchEnded", "TouchDrag", "TouchLongPress",
                                         "TouchPan", "TouchPinch", "TouchRotate", "TouchSwipe", "TouchTapInWorld"});
    auto& io = r.add("InputObject", "Instance", false);
    io.props = {PEnum("KeyCode", "KeyCode", "Unknown", ReadOnly),
                PEnum("UserInputType", "UserInputType", "None", ReadOnly),
                PEnum("UserInputState", "UserInputState", "Begin", ReadOnly),
                P("Position", Value::vector3(0, 0, 0), ReadOnly),
                P("Delta", Value::vector3(0, 0, 0), ReadOnly)};
    // LocalToolEquipped / LocalToolUnequipped follow the local character's Tool (Runtime::Impl::toolMoved).
    r.service("ContextActionService").events = {"LocalToolEquipped", "LocalToolUnequipped"};
    // No purchase can be made here: every prompt is declined at once, firing its Finished event
    // with false on the side that asked (rbx_api.cpp), so ProcessReceipt is never called.
    auto& mps = r.service("MarketplaceService");
    mps.events = {"PromptBulkPurchaseFinished", "PromptBundlePurchaseFinished", "PromptGamePassPurchaseFinished", "PromptPremiumPurchaseFinished",
                  "PromptProductPurchaseFinished", "PromptPurchaseFinished", "PromptRobloxSubscriptionPurchaseFinished", "PromptSubscriptionPurchaseFinished"};
    mps.callbacks = {"ProcessReceipt"};
    r.service("DataStoreService");
    r.add("GlobalDataStore", "Instance", false);
    r.add("OrderedDataStore", "GlobalDataStore", false);
    // What GetDataStore hands back; GetGlobalDataStore's is a plain GlobalDataStore.
    r.add("DataStore", "GlobalDataStore", false);
    // The options objects the store methods take. AllScopes is recorded, not acted on: keys here are
    // not prefixed by scope. UseCache is recorded, not acted on: there is no cache in front of the
    // stores here. SetExperimentalFeatures records its table (rbx_api.cpp) and turns nothing on.
    r.add("DataStoreOptions", "Instance").props = {P("AllScopes", Value::boolean(false)), P("ExperimentalFeatures", Value::string("{}"), Hidden)};
    r.add("DataStoreGetOptions", "Instance").props = {P("UseCache", Value::boolean(true))};
    // GetMetadata / SetMetadata keep the table as JSON in Metadata, declared by table below (kServicesShared);
    // SetAsync stores it with the key's version (rbx_api.cpp).
    r.add("DataStoreSetOptions", "Instance");
    r.add("DataStoreIncrementOptions", "Instance");
    r.add("Pages", "Instance", false).props = {P("IsFinished", Value::boolean(false), ReadOnly)};
    r.add("StandardPages", "Pages", false);
    r.add("DataStorePages", "Pages", false);
    // Cursor is where the current page starts, "" on the first; ListKeysAsync and ListDataStoresAsync take one back to resume there.
    r.add("DataStoreKeyPages", "Pages", false).props = {P("Cursor", Value::string(""), ReadOnly)};
    r.add("DataStoreListingPages", "Pages", false).props = {P("Cursor", Value::string(""), ReadOnly)};
    r.add("DataStoreVersionPages", "Pages", false);
    r.add("DataStoreKey", "Instance", false).props = {P("KeyName", Value::string(""), ReadOnly)};
    r.add("DataStoreInfo", "Instance", false).props = {P("DataStoreName", Value::string(""), ReadOnly),
                                                       P("CreatedTime", Value::number(0), ReadOnly), P("UpdatedTime", Value::number(0), ReadOnly)};
    r.add("DataStoreObjectVersionInfo", "Instance", false).props = {P("Version", Value::string(""), ReadOnly),
                                                                    P("CreatedTime", Value::number(0), ReadOnly), P("IsDeleted", Value::boolean(false), ReadOnly)};
    // UserIds, and the Metadata declared by table below (kServicesShared), are the JSON SetAsync /
    // UpdateAsync / IncrementAsync stored with the version; GetUserIds / GetMetadata decode them.
    r.add("DataStoreKeyInfo", "Instance", false).props = {P("Version", Value::string(""), ReadOnly),
                                                          P("CreatedTime", Value::number(0), ReadOnly), P("UpdatedTime", Value::number(0), ReadOnly),
                                                          P("UserIds", Value::string("[]"), ReadOnly | Hidden)};
    // rbx_chat.cpp makes TextChannels/RBXGeneral and RBXSystem, with a TextSource per player.
    auto& tcs = r.service("TextChatService");
    tcs.props = {PEnum("ChatVersion", "ChatVersion", "TextChatService"), P("CreateDefaultTextChannels", Value::boolean(true)),
                 P("CreateDefaultCommands", Value::boolean(true))};
    tcs.events = {"MessageReceived", "SendingMessage"};
    tcs.callbacks = {"OnIncomingMessage", "OnBubbleAdded", "OnChatWindowAdded"};
    // The engine's own switches (RobloxScriptSecurity): nothing here translates chat or joins a
    // platform's. Their Roblox defaults are not verified here: the type's zero.
    tcs.props.push_back(P("ChatTranslationEnabled", Value::boolean(false), NoScriptRead | NoScriptWrite));
    tcs.props.push_back(P("PlatformIntegratedChat", Value::boolean(false), NoScriptRead | NoScriptWrite));
    // BubbleDisplayed carries a bubble GuiObject Roblox's chat builds; the bubbles here are the engine's own, so it never fires.
    tcs.events.push_back("BubbleDisplayed");
    auto& tch = r.add("TextChannel", "Instance");
    tch.events = {"MessageReceived"};
    tch.callbacks = {"ShouldDeliverCallback", "OnIncomingMessage"};
    tch.props.push_back(PRef("DirectChatRequester", ReadOnly));   // SetDirectChatRequester's TextSource
    r.add("TextSource", "Instance", false).props = {P("UserId", Value::number(0), ReadOnly), P("CanSend", Value::boolean(true))};
    r.add("TextChatMessage", "Instance", false).props = {
        P("Text", Value::string(""), ReadOnly | NoReplicate), P("PrefixText", Value::string(""), ReadOnly | NoReplicate),
        P("MessageId", Value::string(""), ReadOnly | NoReplicate), P("Metadata", Value::string(""), ReadOnly | NoReplicate),
        P("Translation", Value::string(""), ReadOnly | NoReplicate), P("Timestamp", Value::number(0), ReadOnly | NoReplicate),
        PEnum("Status", "TextChatMessageStatus", "Unknown", ReadOnly | NoReplicate),
        PRef("TextSource", ReadOnly | NoReplicate), PRef("TextChannel", ReadOnly | NoReplicate),
        PRef("BubbleChatMessageProperties", NoReplicate), PRef("ChatWindowMessageProperties", NoReplicate)};
    r.add("TextChatMessageProperties", "Instance").props = {P("Text", Value::string("")), P("PrefixText", Value::string("")), P("Translation", Value::string(""))};
    auto& cmd = r.add("TextChatCommand", "Instance");
    cmd.props = {P("PrimaryAlias", Value::string("")), P("SecondaryAlias", Value::string("")), P("Enabled", Value::boolean(true)),
                 P("AutocompleteVisible", Value::boolean(true))};
    cmd.events = {"Triggered"};
    // Chat appearance objects: stored, but the window and bubbles here draw their own.
    auto chatLook = [&](const char* name) -> ClassDef& {
        auto& c = r.add(name, "Instance", false);
        c.props = {P("Enabled", Value::boolean(true)), P("BackgroundColor3", Value::color3(25 / 255.0, 27 / 255.0, 29 / 255.0)),
                   P("BackgroundTransparency", Value::number(0.3)), P("FontFace", Value::font("rbxasset://fonts/families/GothamSSm.json", 500, false)),
                   P("TextColor3", Value::color3(1, 1, 1)), P("TextSize", Value::number(14)),
                   P("TextStrokeColor3", Value::color3(0, 0, 0)), P("TextStrokeTransparency", Value::number(0.5))};
        return c;
    };
    auto& chatWin = chatLook("ChatWindowConfiguration");
    chatWin.props.insert(chatWin.props.end(), {P("HeightScale", Value::number(1)), P("WidthScale", Value::number(1)),
                                               PEnum("HorizontalAlignment", "HorizontalAlignment", "Left"), PEnum("VerticalAlignment", "VerticalAlignment", "Top")});
    auto& chatBar = chatLook("ChatInputBarConfiguration");
    chatBar.props.insert(chatBar.props.end(), {P("PlaceholderColor3", Value::color3(178 / 255.0, 178 / 255.0, 178 / 255.0)), PEnum("KeyboardKeyCode", "KeyCode", "Slash"),
                                               P("AutocompleteEnabled", Value::boolean(true)), PRef("TargetTextChannel")});
    auto& bubble = chatLook("BubbleChatConfiguration");
    bubble.props.insert(bubble.props.end(), {P("AdorneeName", Value::string("HumanoidRootPart")), P("BubbleDuration", Value::number(15)), P("BubblesSpacing", Value::number(6)),
                                             P("LocalPlayerStudsOffset", Value::vector3(0, 0, 0)), P("MaxDistance", Value::number(100)), P("MaxBubbles", Value::number(3)),
                                             P("MinimizeDistance", Value::number(40)), P("VerticalStudsOffset", Value::number(0)), P("TailVisible", Value::boolean(true))});
    auto& tabs = chatLook("ChannelTabsConfiguration");
    tabs.props.insert(tabs.props.end(), {P("HoverBackgroundColor3", Value::color3(125 / 255.0, 125 / 255.0, 125 / 255.0)), P("SelectedTabTextColor3", Value::color3(1, 1, 1))});
    // One choice in two properties, as a TextLabel's: Font follows FontFace (rbx_runtime.cpp); the
    // default is the Enum.Font of the FontFace above. The three configurations' AbsolutePosition and
    // AbsoluteSize, the input bar's IsFocused and TextBox: the engine's chat draws its own window, so
    // they are declared by table below (kServicesShared) and read their zeros.
    bubble.props.push_back(PEnum("Font", "Font", "GothamMedium"));
    // The look of one bubble, from OnBubbleAdded; the bubbles here are the engine's own, so it is recorded, not
    // acted on. Its defaults are BubbleChatConfiguration's here; Roblox's exact ones are not verified.
    r.add("BubbleChatMessageProperties", "TextChatMessageProperties").props = {P("TailVisible", Value::boolean(true))};
    // The look of one window line, from ChatWindowConfiguration:DeriveNewMessageProperties; the
    // window here is the engine's own, so it is recorded, not acted on. Its look properties are
    // declared by table below (kServicesShared), the defaults ChatWindowConfiguration's here.
    r.add("ChatWindowMessageProperties", "TextChatMessageProperties");
    // The legacy chat: the engine's window and bubbles, Player.Chatted on the server.
    r.service("Chat").props = {P("BubbleChatEnabled", Value::boolean(true)), P("LoadDefaultChat", Value::boolean(true))};
    r.service("PhysicsService");
    // The engine's escape menu writes these, on the client only.
    auto& guiSvc = r.service("GuiService");
    guiSvc.props = {P("MenuIsOpen", Value::boolean(false), ReadOnly)};
    guiSvc.events = {"MenuOpened", "MenuClosed"};
    // SelectedObject moves the selection: SelectionLost / SelectionGained on the objects and
    // SelectionChanged up their GuiBase2d chains (rbx_api.cpp); nothing here draws or steers it,
    // so the three navigation flags are recorded, not acted on. The accessibility readings are a
    // desktop's. The hidden three hold what the Get / Set pairs answer; no menu opens here.
    guiSvc.props.insert(guiSvc.props.end(), {PRef("SelectedObject"), P("AutoSelectGuiEnabled", Value::boolean(true)), P("CoreGuiNavigationEnabled", Value::boolean(true)),
                                             P("GuiNavigationEnabled", Value::boolean(true)), P("TouchControlsEnabled", Value::boolean(true)),
                                             PEnum("PreferredTextSize", "PreferredTextSize", "Medium", ReadOnly), P("PreferredTransparency", Value::number(1), ReadOnly),
                                             P("ReducedMotionEnabled", Value::boolean(false), ReadOnly),
                                             P("InspectMenuEnabled", Value::boolean(true), NoReplicate | Hidden), P("EmotesMenuOpen", Value::boolean(false), NoReplicate | Hidden),
                                             P("GameplayPausedNotificationEnabled", Value::boolean(true), NoReplicate | Hidden)});
    // No teleport leaves here: TeleportAsync fires TeleportInitFailed and errors, nobody arrives
    // (rbx_api.cpp). The teleport settings are kept for the session.
    r.service("TeleportService").events = {"LocalPlayerArrivedFromTeleport", "TeleportInitFailed"};
    // TeleportAsync's options: the data table is kept as JSON in TeleportData (Get / SetTeleportData).
    // ReservedServerAccessCode, ServerInstanceId and ShouldReserveServer are recorded, not acted on:
    // no server is reserved or picked here.
    r.add("TeleportOptions", "Instance").props = {P("ReservedServerAccessCode", Value::string("")), P("ServerInstanceId", Value::string("")),
                                                  P("ShouldReserveServer", Value::boolean(false)), P("TeleportData", Value::string("null"), Hidden)};
    // What a teleport would answer with; none is made here. PrivateServerId is declared by table below (kServicesShared).
    r.add("TeleportAsyncResult", "Instance", false).props = {P("ReservedServerAccessCode", Value::string(""), ReadOnly)};
    // Awards are kept for the session (rbx_api.cpp): no badge backend here.
    r.service("BadgeService");
    // Error(message, stackTrace, script) fires for every uncaught script error (rbx_runtime.cpp report).
    r.service("ScriptContext").events = {"Error"};
    // MessageOut(message, messageType) fires for every print, warn and uncaught error; GetLogHistory holds the last of them.
    r.service("LogService").events = {"MessageOut"};
    // Registered so a place file's children under them (a MaterialVariant, a LocalizationTable) survive a load.
    // SetBaseMaterialOverride(material, name) keeps the name in <Material>Name, as Roblox's hidden
    // storage does; a part of that Material wearing no MaterialVariant of its own then wears the
    // MaterialVariant so named (pulseblockz_world.cpp, surface_for).
    auto& materials = r.service("MaterialService");
    materials.props = {P("Use2022Materials", Value::boolean(true))};
    for (const char* m : {"Asphalt", "Basalt", "Brick", "Cardboard", "Carpet", "CeramicTiles", "ClayRoofTiles", "Cobblestone", "Concrete",
                          "CorrodedMetal", "CrackedLava", "DiamondPlate", "Fabric", "Foil", "Glacier", "Granite", "Grass", "Ground", "Ice",
                          "LeafyGrass", "Leather", "Limestone", "Marble", "Metal", "Mud", "Pavement", "Pebble", "Plaster", "Plastic", "Rock",
                          "RoofShingles", "Rubber", "Salt", "Sand", "Sandstone", "Slate", "SmoothPlastic", "Snow", "Wood", "WoodPlanks"})
        materials.props.push_back(P((std::string(m) + "Name").c_str(), Value::string(""), NoScriptRead | NoScriptWrite));
    r.add("MaterialVariant", "Instance").props = {PEnum("BaseMaterial", "Material", "Plastic"), P("ColorMap", Value::string("")), P("MetalnessMap", Value::string("")),
                                                  P("NormalMap", Value::string("")), P("RoughnessMap", Value::string("")), P("StudsPerTile", Value::number(10)),
                                                  PEnum("MaterialPattern", "MaterialPattern", "Regular"),
                                                  // Recorded, not acted on: a variant's ColorMap draws opaque, and a part wearing
                                                  // one keeps the physics of its own Material.
                                                  PEnum("AlphaMode", "AlphaMode", "Overlay"), PPhys("CustomPhysicalProperties")};
    // The locale here is en-us: nothing else is configured.
    r.service("LocalizationService").props = {P("RobloxLocaleId", Value::string("en-us"), ReadOnly), P("SystemLocaleId", Value::string("en-us"), ReadOnly)};
    // Contents is the entries as Roblox's JSON: [{key, source, context, example, values: {locale: text}}];
    // the entry methods and a Translator read and write it (rbx_api.cpp).
    r.add("LocalizationTable", "Instance").props = {P("SourceLocaleId", Value::string("en-us")), P("Contents", Value::string("[]"), Hidden | NoScriptWrite)};
    // From GetTranslator / GetTranslatorForPlayerAsync / GetTranslatorForLocaleAsync. Table is the
    // LocalizationTable it reads; nil reads every table under LocalizationService.
    r.add("Translator", "Instance", false).props = {P("LocaleId", Value::string("en-us"), ReadOnly), PRef("Table", ReadOnly | Hidden)};
    // The checks print through the Output in Roblox's wording and count into ErrorCount and
    // WarnCount (rbx_api.cpp). The rest are recorded, not acted on: nothing here runs a test session,
    // lags, throttles or sleeps physics on their say.
    auto& ts = r.service("TestService");
    ts.props = {P("AutoRuns", Value::boolean(true)), P("Description", Value::string("")), P("ErrorCount", Value::number(0), ReadOnly),
                P("ExecuteWithStudioRun", Value::boolean(false)), P("IsPhysicsEnvironmentalThrottled", Value::boolean(false)),
                P("IsSleepAllowed", Value::boolean(true)), P("NumberOfPlayers", Value::number(0)), P("SimulateSecondsLag", Value::number(0)),
                P("TestCount", Value::number(0), ReadOnly), P("ThrottlePhysicsToRealtime", Value::boolean(true)), P("Timeout", Value::number(10)),
                P("WarnCount", Value::number(0), ReadOnly)};
    ts.events = {"ServerCollectConditionalResult", "ServerCollectResult"};   // a Studio test session's; none runs here
    r.service("InsertService");
    // AllowInsertFreeAssets is the engine's own (RobloxScriptSecurity); its Roblox default is not verified here.
    r.service("AssetService").props = {P("AllowInsertFreeAssets", Value::boolean(false), NoScriptRead | NoScriptWrite)};
    auto& cp = r.service("ContentProvider");
    cp.props = {P("RequestQueueSize", Value::number(0), ReadOnly)};
    cp.props.push_back(P("BaseUrl", Value::string(""), ReadOnly));   // no web site fronts the assets here
    cp.events.push_back("AssetFetchFailed");                          // (contentId): a PreloadAsync content the host could not load
    // No headset here: VREnabled is false, every UserCFrame reads as the identity and none is
    // enabled. The touchpad modes are kept (Get / SetTouchpadMode) and RequestNavigation fires
    // NavigationRequested (rbx_api.cpp). The rest are recorded, not acted on: nothing here scales
    // a world to a headset, gestures, draws a controller, fades on collision or points a laser.
    // LaserPointer's Roblox default is not verified here: the type's zero.
    auto& vr = r.service("VRService");
    vr.props = {P("VREnabled", Value::boolean(false), ReadOnly), PEnum("AutomaticScaling", "VRScaling", "World"), P("AvatarGestures", Value::boolean(false)),
                PEnum("ControllerModels", "VRControllerModelMode", "Disabled"), P("FadeOutViewOnCollision", Value::boolean(true)),
                PEnum("GuiInputUserCFrame", "UserCFrame", "Head"), PEnum("LaserPointer", "VRLaserPointerMode", "Disabled"),
                P("ThirdPersonFollowCamEnabled", Value::boolean(false))};
    vr.events = {"NavigationRequested", "TouchpadModeChanged", "UserCFrameChanged", "UserCFrameEnabled"};
    // No voice here: IsVoiceEnabledForUserIdAsync answers false. The three switches are recorded, not
    // acted on. EnableVoiceVolumeControls' Roblox default is not verified here: the type's zero.
    r.service("VoiceChatService").props = {P("EnableDefaultVoice", Value::boolean(true), PluginWrite), PEnum("UseAudioApi", "AudioApiRollout", "Default", PluginWrite),
                                           P("EnableVoiceVolumeControls", Value::boolean(false), NoScriptRead | NoScriptWrite)};
    r.service("GamePassService");
    r.service("TouchInputService");
    r.service("PermissionsService");
    r.service("PolicyService");
    // No invite, call, share sheet or phone book opens here: PromptGameInvite closes its prompt at
    // once (rbx_api.cpp); the other three events never fire and OnCallInviteInvoked is never called.
    auto& social = r.service("SocialService");
    social.events = {"CallInviteStateChanged", "GameInvitePromptClosed", "PhoneBookPromptClosed", "ShareSheetClosed"};
    social.callbacks = {"OnCallInviteInvoked"};
    r.service("GroupService");
    // ---- the services family's newcomers, after the services above so their ids keep ----
    // Memory stores live in this process for the session: a hash map, a sorted map and a queue per
    // name, with expiry, and a queue read's invisibility window (rbx_api.cpp). Server only.
    r.service("MemoryStoreService");
    r.add("MemoryStoreHashMap", "Instance", false);
    r.add("MemoryStoreSortedMap", "Instance", false);
    r.add("MemoryStoreQueue", "Instance", false);
    r.add("MemoryStoreHashMapPages", "Pages", false);
    // PublishAsync reaches this server's own subscribers a step later; there is no other server here.
    r.service("MessagingService");
    // InstanceCount and PrimitivesCount are counted from the tree; nothing here measures the rest, so
    // they read zero (rbx_api.cpp). MemoryTrackingEnabled is recorded, not acted on.
    auto& stats = r.service("Stats");
    stats.props = {P("ContactsCount", Value::number(0), ReadOnly), P("DataReceiveKbps", Value::number(0), ReadOnly), P("DataSendKbps", Value::number(0), ReadOnly),
                   P("FrameTime", Value::number(0), ReadOnly), P("HeartbeatTime", Value::number(0), ReadOnly), P("MovingPrimitivesCount", Value::number(0), ReadOnly),
                   P("PhysicsReceiveKbps", Value::number(0), ReadOnly), P("PhysicsSendKbps", Value::number(0), ReadOnly), P("PhysicsStepTime", Value::number(0), ReadOnly),
                   P("RenderCPUFrameTime", Value::number(0), ReadOnly), P("RenderGPUFrameTime", Value::number(0), ReadOnly),
                   P("SceneDrawcallCount", Value::number(0), ReadOnly), P("SceneTriangleCount", Value::number(0), ReadOnly),
                   P("ShadowsDrawcallCount", Value::number(0), ReadOnly), P("ShadowsTriangleCount", Value::number(0), ReadOnly),
                   P("UI2DDrawcallCount", Value::number(0), ReadOnly), P("UI2DTriangleCount", Value::number(0), ReadOnly),
                   P("UI3DDrawcallCount", Value::number(0), ReadOnly), P("UI3DTriangleCount", Value::number(0), ReadOnly),
                   P("MemoryTrackingEnabled", Value::boolean(false))};
    r.add("StatsItem", "Instance", false);   // Stats' counters on Roblox; none is made here, so GetValue reads 0
    // The users known here are the players in the game (GetUserInfosByUserIdsAsync).
    r.service("UserService");
    // Registered so GetService answers. Their members are Roblox's backends and prompts: none is
    // declared but AvatarEditorService:GetAccessoryType, a pure mapping, and AnalyticsService's
    // Log* methods, which take their arguments and drop them (rbx_api.cpp).
    r.service("AdService");
    r.service("AnalyticsService");
    r.service("AvatarCreationService").events = {"AvatarAssetModerationCompleted", "AvatarOutfitModerationCompleted"};
    r.service("AvatarEditorService").events = {"PromptAllowInventoryReadAccessCompleted", "PromptCreateOutfitCompleted", "PromptDeleteOutfitCompleted",
                                              "PromptRenameOutfitCompleted", "PromptSaveAvatarCompleted", "PromptSetFavoriteCompleted", "PromptUpdateOutfitCompleted"};
    // No notification opt-in exists here: CanPromptOptInAsync is false and PromptOptIn closes at once.
    r.service("ExperienceNotificationService").events = {"OptInPromptClosed"};
    // No SharedTable datatype here, so neither method is declared.
    r.service("SharedTableRegistry");
    // Registered so GetService answers; last, since a service's id is its place in this order.
    // Its methods are not declared: GetTextSize and GetTextBoundsAsync need font metrics the
    // runtime does not have (the engine measures TextBounds on the drawn label), and
    // FilterStringAsync, GetFamilyInfoAsync and GetTextSizeOffsetAsync are Roblox's backends.
    r.service("TextService");

    auto& dm = r.add("DataModel", "ServiceProvider", false);
    dm.props = {P("PlaceId", Value::number(0), ReadOnly), P("GameId", Value::number(0), ReadOnly),
                P("JobId", Value::string(""), ReadOnly), P("PlaceVersion", Value::number(0), ReadOnly),
                P("PrivateServerId", Value::string(""), ReadOnly), P("PrivateServerOwnerId", Value::number(0), ReadOnly)};
    // No creator here: 0 and User. Workspace and RunService read as the services (rbx_api.cpp).
    // MatchmakingType is not declared: its enum is not verified here.
    dm.props.push_back(P("CreatorId", Value::number(0), ReadOnly));
    dm.props.push_back(PEnum("CreatorType", "CreatorType", "User", ReadOnly));
    // Close: ServiceProvider's. None of these three fires: IsLoaded is true from the start, so Loaded
    // never comes (as on Roblox once loaded); no quality key and no restart schedule here.
    dm.events = {"Loaded", "GraphicsQualityChangeRequest", "ServerRestartScheduled"};

    // The classic NPC dialog. Recorded, not acted on: nothing here draws a speech bubble, so InUse
    // stays false, GetCurrentPlayers is empty and DialogChoiceSelected never fires. Defaults Roblox's.
    auto& dialog = r.add("Dialog", "Instance");
    dialog.props = {PEnum("BehaviorType", "DialogBehaviorType", "SinglePlayer"), P("ConversationDistance", Value::number(25)),
                    P("GoodbyeChoiceActive", Value::boolean(true)), P("GoodbyeDialog", Value::string("")), P("InitialPrompt", Value::string("")),
                    P("InUse", Value::boolean(false)), PEnum("Purpose", "DialogPurpose", "Help"), PEnum("Tone", "DialogTone", "Neutral"),
                    P("TriggerDistance", Value::number(0)), P("TriggerOffset", Value::vector3(0, 0, 0))};
    dialog.events = {"DialogChoiceSelected"};
    r.add("DialogChoice", "Instance").props = {P("GoodbyeChoiceActive", Value::boolean(true)), P("GoodbyeDialog", Value::string("")),
                                                 P("ResponseDialog", Value::string("")), P("UserDialog", Value::string(""))};
    // The deprecated surface features' base (Hole, MotorFeature). Recorded, not acted on: nothing here
    // cuts a hole. FaceId's Roblox default is not verified here (NormalId's first).
    r.add("Feature", "Instance", false).props = {PEnum("FaceId", "NormalId", "Right"), PEnum("InOut", "InOut", "Center"),
                                                   PEnum("LeftRight", "LeftRight", "Center"), PEnum("TopBottom", "TopBottom", "Center")};

    // ---- the rest family ------------------------------------------------------------------
    // Defaults are Roblox's: rbx-dom's reflection database for what serializes, the type's zero for
    // a read-only or unserialized member whose Roblox reading is not verified here. Members whose
    // names demo2's classes carry too are in kRestShared at the end of build().

    // The avatar controller stack. Recorded, not acted on: the character here walks on its Humanoid;
    // nothing drives a ControllerManager, so no controller is ever Active and no sensor senses.
    auto& sensorBase = r.add("SensorBase", "Instance", false);
    sensorBase.props = {PEnum("UpdateType", "SensorUpdateType", "OnRead")};
    sensorBase.events = {"OnSensorOutputChanged"};   // never fires
    r.add("ControllerSensor", "SensorBase", false);
    // HitFrame is a CFrame over this pair (cframeProp in rbx_api.cpp).
    r.add("ControllerPartSensor", "ControllerSensor").props = {P("HitFramePosition", Value::vector3(0, 0, 0), Hidden), P("HitFrameOrientation", Value::vector3(0, 0, 0), Hidden),
        P("HitNormal", Value::vector3(0, 1, 0)), P("LadderSearchHeight", Value::number(6)), P("LadderSearchOffset", Value::number(5)), P("SearchDistance", Value::number(0)),
        PEnum("SensedMaterial", "Material", "Air"), PRef("SensedPart"), PEnum("SensorMode", "SensorMode", "Floor")};
    r.add("BuoyancySensor", "SensorBase").props = {P("FullySubmerged", Value::boolean(false)), P("TouchingSurface", Value::boolean(false))};
    r.add("AtmosphereSensor", "SensorBase").props = {P("AirDensity", Value::number(0), ReadOnly), P("RelativeWindVelocity", Value::vector3(0, 0, 0), ReadOnly)};
    // EvaluateAsync answers the three readings (rbx_api.cpp): zero, as nothing here has fluid.
    r.add("FluidForceSensor", "SensorBase").props = {P("CenterOfPressure", Value::vector3(0, 0, 0), ReadOnly), P("Force", Value::vector3(0, 0, 0), ReadOnly),
                                                     P("Torque", Value::vector3(0, 0, 0), ReadOnly)};
    // Active: kRestShared. MoveSpeedFactor's 1 is Roblox's documented default.
    r.add("ControllerBase", "Instance", false).props = {P("BalanceRigidityEnabled", Value::boolean(false)), P("MoveSpeedFactor", Value::number(1))};
    r.add("GroundController", "ControllerBase").props = {P("AccelerationTime", Value::number(0)), P("BalanceMaxTorque", Value::number(10000)), P("BalanceSpeed", Value::number(100)),
        P("DecelerationTime", Value::number(0)), P("Friction", Value::number(2)), P("FrictionWeight", Value::number(1)), P("GroundOffset", Value::number(1)),
        P("TurnSpeedFactor", Value::number(1))};
    r.add("AirController", "ControllerBase").props = {P("BalanceMaxTorque", Value::number(10000)), P("BalanceSpeed", Value::number(100)), P("MaintainAngularMomentum", Value::boolean(true)),
        P("MaintainLinearMomentum", Value::boolean(true)), P("MoveMaxForce", Value::number(1000)), P("TurnMaxTorque", Value::number(10000)), P("TurnSpeedFactor", Value::number(1))};
    r.add("ClimbController", "ControllerBase").props = {P("AccelerationTime", Value::number(0)), P("BalanceMaxTorque", Value::number(10000)), P("BalanceSpeed", Value::number(100)),
        P("MoveMaxForce", Value::number(10000))};
    r.add("SwimController", "ControllerBase").props = {P("PitchMaxTorque", Value::number(10000)), P("PitchSpeedFactor", Value::number(1)), P("RollMaxTorque", Value::number(10000)),
        P("RollSpeedFactor", Value::number(1))};
    r.add("ControllerManager", "Instance").props = {PRef("ActiveController"), P("BaseMoveSpeed", Value::number(16)), P("BaseTurnSpeed", Value::number(8)), PRef("ClimbSensor"),
        P("FacingDirection", Value::vector3(0, 0, 1)), PRef("GroundSensor"), P("MovingDirection", Value::vector3(0, 0, 0)), PRef("RootPart"), P("UpDirection", Value::vector3(0, 1, 0))};

    // IKControl: recorded, not acted on: nothing here solves a chain, so the limbs stay where the
    // animation puts them. The chain is still readable: GetChainCount / GetChainLength walk the
    // Motor6Ds (or Bones) from EndEffector up to ChainRoot, GetNodeWorldCFrame / GetNodeLocalCFrame
    // read the nodes as they are, and GetRawFinalTarget / GetSmoothedFinalTarget give Target's frame
    // times Offset (no smoothing) (rbx_api.cpp). Offset and EndEffectorOffset are CFrames over their
    // pairs; Enabled, Priority, Target and Type: kRestShared.
    r.add("IKControl", "Instance").props = {PRef("ChainRoot"), PRef("EndEffector"),
        P("EndEffectorOffsetPosition", Value::vector3(0, 0, 0), Hidden), P("EndEffectorOffsetOrientation", Value::vector3(0, 0, 0), Hidden),
        P("OffsetPosition", Value::vector3(0, 0, 0), Hidden), P("OffsetOrientation", Value::vector3(0, 0, 0), Hidden),
        PRef("Pole"), P("SmoothTime", Value::number(0.05)), P("Weight", Value::number(1))};

    // LineForce pushes Attachment0's assembly toward Attachment1 with Magnitude (over the squared
    // distance when InverseSquareLaw), capped by MaxForce, at the attachment or the centre of mass,
    // and back on Attachment1's when ReactionForceEnabled (pulseblockz_world.cpp step_movers).
    // Magnitude: kRestShared.
    r.add("LineForce", "Constraint").props = {P("ApplyAtCenterOfMass", Value::boolean(false)), P("InverseSquareLaw", Value::boolean(false)),
        P("MaxForce", Value::number(std::numeric_limits<double>::infinity())), P("ReactionForceEnabled", Value::boolean(false))};

    // The animation curves. A FloatCurve's or RotationCurve's keys live in the hidden KeysBlob, read
    // and written as FloatCurveKeys / RotationCurveKeys and sampled by GetValueAtTime (rbx_api.cpp);
    // a MarkerCurve's markers likewise. Length follows the keys. An EulerRotationCurve's or
    // Vector3Curve's X / Y / Z are child FloatCurves of those names. Nothing here plays a curve.
    r.add("FloatCurve", "Instance").props = {P("KeysBlob", Value::string(""), Hidden), P("Length", Value::number(0), ReadOnly)};
    r.add("RotationCurve", "Instance").props = {P("KeysBlob", Value::string(""), Hidden), P("Length", Value::number(0), ReadOnly)};
    r.add("MarkerCurve", "Instance").props = {P("KeysBlob", Value::string(""), Hidden), P("Length", Value::number(0), ReadOnly)};
    r.add("EulerRotationCurve", "Instance").props = {PEnum("RotationOrder", "RotationOrder", "XYZ")};
    r.add("Vector3Curve", "Instance");
    // Their keys are ValueCurveKeys, no such datatype here, so their methods are not answered.
    r.add("ValueCurve", "Instance").props = {P("Length", Value::number(0), ReadOnly), P("ValueType", Value::string(""), ReadOnly)};
    r.add("CompositeValueCurve", "Instance").props = {PEnum("CurveType", "CompositeValueCurveType", "NumberRange")};

    // The Input Action System. An InputBinding under an InputAction under an InputContext takes the
    // keyboard (rbx_runtime.cpp inputActions): KeyCode for a Bool or Direction1D action, Up / Down /
    // Left / Right (and Forward / Backward) summed for a Direction2D / Direction3D one, times Scale
    // and the vector scales, clamped when ClampMagnitudeToOne; Fire sets the state by hand. The
    // action's state is in the *State property its Type names, read by GetState; StateChanged fires
    // on a change, Pressed / Released on a Bool's. A Sink context stops lower priorities. A gamepad,
    // touch or UIButton binding is recorded, not acted on. PreferredBinding is the first binding
    // child. Enabled, Priority, Sink, Type and the key names: kRestShared.
    r.add("InputContext", "Instance");
    auto& inputAction = r.add("InputAction", "Instance");
    inputAction.props = {P("BoolState", Value::boolean(false), NoScriptRead | NoScriptWrite | Hidden), P("Direction1DState", Value::number(0), NoScriptRead | NoScriptWrite | Hidden),
                         P("Direction2DState", Value::vector2(0, 0), NoScriptRead | NoScriptWrite | Hidden), P("Direction3DState", Value::vector3(0, 0, 0), NoScriptRead | NoScriptWrite | Hidden),
                         P("ViewportPositionState", Value::vector2(0, 0), NoScriptRead | NoScriptWrite | Hidden)};
    inputAction.events = {"Pressed", "Released", "StateChanged"};
    r.add("InputBinding", "Instance").props = {PEnum("Backward", "KeyCode", "Unknown"), P("ClampMagnitudeToOne", Value::boolean(true)), PEnum("Forward", "KeyCode", "Unknown"),
        P("PointerIndex", Value::number(0)), P("PressedThreshold", Value::number(0.5)), PEnum("PrimaryModifier", "KeyCode", "Unknown"), P("ReleasedThreshold", Value::number(0.2)),
        P("ResponseCurve", Value::number(1)), PEnum("SecondaryModifier", "KeyCode", "Unknown"), PRef("UIButton"), PRef("UIModifier"),
        P("Vector2Scale", Value::vector2(1, 1)), P("Vector3Scale", Value::vector3(1, 1, 1))};
    // A label of an action's binding. Recorded, not acted on: nothing here draws it or resolves a
    // key's glyph, so ResolvedText and ResolvedImageContent stay empty. Its defaults are not verified
    // here (the type's zero; ImageColor3 white as every image's). The text members: kRestShared.
    r.add("InputActionLabel", "GuiObject").props = {PRef("InputAction"), P("ResolvedText", Value::string(""), ReadOnly)};

    // HumanoidDescription's newer parts. Recorded, not acted on: nothing here wears them, so
    // GetAppliedInstance answers the Instance property (rbx_api.cpp). Instance, Position, Rotation,
    // Scale and Color: kRestShared.
    r.add("AccessoryDescription", "Instance").props = {PEnum("AccessoryType", "AccessoryType", "Unknown"), P("AssetId", Value::number(0)), P("IsLayered", Value::boolean(false)),
                                                       P("Order", Value::number(0))};
    r.add("BodyPartDescription", "Instance").props = {P("AssetId", Value::number(0)), PEnum("BodyPart", "BodyPart", "Head"), P("HeadShape", Value::string(""))};
    r.add("MakeupDescription", "Instance").props = {P("AssetId", Value::number(0)), PEnum("MakeupType", "MakeupType", "Face"), P("Order", Value::number(0))};

    // Roblox's base of Pose and NumberPose; its two easings are kRestShared, the same as theirs.
    auto& poseBase = r.add("PoseBase", "Instance", false);
    for (auto& o : r.owned) if (o->name == "Pose" || o->name == "NumberPose") o->base = &poseBase;

    // The rig descriptions an avatar importer fills. Recorded, not acted on: nothing here retargets
    // a rig. Each joint is a Ref with a RangeMax / RangeMin, a Size and a TposeAdjustment CFrame
    // (over its pair, cframeProp). GetJointNames lists the joints set, GetR15JointNames / GetR6JointNames
    // the rigs' names, GetJointFromName the joint (rbx_api.cpp).
    auto& rig = r.add("HumanoidRigDescription", "Instance");
    for (const char* j : {"Chest", "HeadBase", "LeftAnkle", "LeftClavicle", "LeftElbow", "LeftHip", "LeftKnee", "LeftShoulder", "LeftToeBase", "LeftWrist", "Neck",
                          "RightAnkle", "RightClavicle", "RightElbow", "RightHip", "RightKnee", "RightShoulder", "RightToeBase", "RightWrist", "Root", "Spine", "Waist"}) {
        std::string n = j;
        rig.props.push_back(PRef(n.c_str()));
        rig.props.push_back(P((n + "RangeMax").c_str(), Value::vector3(0, 0, 0)));
        rig.props.push_back(P((n + "RangeMin").c_str(), Value::vector3(0, 0, 0)));
        rig.props.push_back(P((n + "Size").c_str(), Value::number(0)));
        rig.props.push_back(P((n + "TposeAdjustmentPosition").c_str(), Value::vector3(0, 0, 0), Hidden));
        rig.props.push_back(P((n + "TposeAdjustmentOrientation").c_str(), Value::vector3(0, 0, 0), Hidden));
    }
    rig.props.push_back(P("OriginOffsetPosition", Value::vector3(0, 0, 0), Hidden | NoScriptRead | NoScriptWrite));
    rig.props.push_back(P("OriginOffsetOrientation", Value::vector3(0, 0, 0), Hidden | NoScriptRead | NoScriptWrite));
    // A hand: five fingers of three joints. GetFingerTip / SetFingerTip and GetFingerControl /
    // SetFingerControl keep a point and a control vector per finger index in the hidden blobs.
    auto& digits = r.add("DigitsRigDescription", "Instance");
    for (const char* f : {"Index", "Middle", "Pinky", "Ring", "Thumb"}) {
        std::string n = f;
        for (const char* k : {"1", "2", "3"}) {
            digits.props.push_back(PRef((n + k).c_str()));
            digits.props.push_back(P((n + k + "TposeAdjustmentPosition").c_str(), Value::vector3(0, 0, 0), Hidden));
            digits.props.push_back(P((n + k + "TposeAdjustmentOrientation").c_str(), Value::vector3(0, 0, 0), Hidden));
        }
        digits.props.push_back(P((n + "Range").c_str(), Value::vector3(0, 0, 0)));
        digits.props.push_back(P((n + "Size").c_str(), Value::number(0)));
    }
    digits.props.push_back(PEnum("Side", "DigitsRigDescriptionSide", "None"));
    digits.props.push_back(P("FingerTips", Value::string(""), Hidden));
    digits.props.push_back(P("FingerControls", Value::string(""), Hidden));
    // The FACS action units a dynamic head animates, 0..1 each. Recorded, not acted on: no head
    // here has a face rig. Their Roblox defaults are 0.
    auto& face = r.add("FaceControls", "Instance");
    for (auto& e : g_enums) if (!std::strcmp(e.name, "FacsActionUnit")) for (auto& it : e.items) face.props.push_back(P(it.name, Value::number(0)));

    // Haptics: Play / Stop keep nothing and Ended never fires, as no controller here rumbles;
    // SetWaveformKeys takes its FloatCurveKeys and keeps them in the hidden blob (rbx_api.cpp).
    // Looped, Position and Type: kRestShared.
    auto& haptic = r.add("HapticEffect", "Instance");
    haptic.props = {P("Radius", Value::number(3)), P("KeysBlob", Value::string(""), Hidden)};
    haptic.events = {"Ended"};

    // A TextService:FilterStringAsync result: no filter here, so every reading is the text as it
    // came (kept in FilteredText). A translated result's GetTranslations is empty.
    r.add("TextFilterResult", "Instance", false).props = {P("FilteredText", Value::string(""), Hidden)};
    r.add("TextFilterTranslatedResult", "Instance", false).props = {P("SourceLanguage", Value::string(""), ReadOnly), PRef("SourceText", ReadOnly)};
    // What TextService:GetTextBoundsAsync would measure; nothing here has font metrics, so the
    // method stays undeclared. Font's default is Roblox's; Size and Text: kRestShared.
    r.add("GetTextBoundsParams", "Instance").props = {P("Font", Value::font("rbxasset://fonts/families/SourceSansPro.json", 400, false)), P("RichText", Value::boolean(false)),
                                                      P("Width", Value::number(0))};

    // GUI styling. A StyleSheet's rules are its StyleRule children (GetStyleRules / InsertStyleRule /
    // SetStyleRules), its derives the StyleDerive children; a StyleRule keeps its properties in the
    // runtime (SetProperty / GetProperties) and its transitions as JSON in the hidden blobs, a
    // StyleQuery its conditions likewise (rbx_api.cpp). Recorded, not acted on: nothing here applies
    // a rule to a GuiObject, so a StyleLink links nothing, SelectorError stays "" and a StyleQuery's
    // IsActive false.
    auto& styleBase = r.add("StyleBase", "Instance", false);
    styleBase.events = {"StyleRulesChanged"};
    r.add("StyleSheet", "StyleBase");
    r.add("StyleRule", "StyleBase").props = {P("Selector", Value::string("")), P("SelectorError", Value::string(""), ReadOnly),
                                             P("TransitionsBlob", Value::string("{}"), Hidden), P("DefaultTransitionBlob", Value::string(""), Hidden)};
    r.add("StyleDerive", "Instance").props = {PRef("StyleSheet")};
    r.add("StyleLink", "Instance").props = {PRef("StyleSheet")};
    r.add("StyleQuery", "Instance").props = {P("IsActive", Value::boolean(false), ReadOnly), P("ConditionsBlob", Value::string("{}"), Hidden)};

    // Video. Recorded, not acted on: nothing here decodes a video, so a VideoPlayer never loads
    // (IsLoaded false, LoadAsync answers Failure and PlayFailed fires with it), IsPlaying stays
    // false through Play / Pause, and a VideoDisplay draws nothing. Their wiring methods answer
    // as the audio graph's do (Wire); MaximumResolution is Hidden as on Roblox. PlaybackSpeed,
    // TimePosition, Volume, ResampleMode and ScaleType: kRestShared.
    auto& videoPlayer = r.add("VideoPlayer", "Instance");
    videoPlayer.props = {P("AutoLoadInStudio", Value::boolean(false), NoScriptRead | NoScriptWrite), P("AutoPlayInStudio", Value::boolean(false), NoScriptRead | NoScriptWrite),
                         P("IsLoaded", Value::boolean(false), ReadOnly), P("IsPlaying", Value::boolean(false), ReadOnly), P("Looping", Value::boolean(false)),
                         PEnum("MaximumResolution", "VideoSampleSize", "Full", Hidden), P("Resolution", Value::vector2(0, 0), ReadOnly), P("TimeLength", Value::number(0), ReadOnly)};
    videoPlayer.events = {"DidEnd", "DidLoop", "PlayFailed", "WiringChanged"};
    auto& videoDisplay = r.add("VideoDisplay", "GuiObject");
    videoDisplay.props = {P("TileSize", Value::udim2(1, 0, 1, 0)), P("VideoColor3", Value::color3(1, 1, 1)), P("VideoRectOffset", Value::vector2(0, 0)),
                          P("VideoRectSize", Value::vector2(0, 0)), P("VideoTransparency", Value::number(0))};
    videoDisplay.events = {"WiringChanged"};
    r.service("VideoService");   // CreateVideoSamplerAsync is not answered: no decoder here
    r.add("VideoSampler", "Object", false).props = {P("TimeLength", Value::number(0), ReadOnly)};

    // Captures. Recorded, not acted on: nothing here takes a screenshot, so CaptureScreenshot
    // never calls back, the prompts decline and the gallery reads empty (rbx_api.cpp); no
    // Capture is ever made. A ScreenshotHud draws nothing. CaptureTime is a DateTime, no such
    // datatype here, so it is not declared. Visible, Position: kRestShared.
    auto& captureService = r.service("CaptureService");
    captureService.events = {"CaptureBegan", "CaptureEnded", "UserCaptureSaved"};
    r.add("Capture", "Object", false).props = {PEnum("CaptureType", "CaptureType", "Screenshot", ReadOnly), P("FilePathString", Value::string(""), ReadOnly | NoScriptRead | NoScriptWrite),
        P("LocalId", Value::string(""), ReadOnly), P("SourcePlaceId", Value::number(0), ReadOnly), P("SourceUniverseId", Value::number(0), ReadOnly)};
    r.add("VideoCapture", "Capture", false).props = {P("FilePath", Value::string(""), ReadOnly | NoScriptRead | NoScriptWrite), P("TimeLength", Value::number(0), ReadOnly)};
    r.add("ScreenshotHud", "Instance", false).props = {P("CameraButtonIcon", Value::string("")), P("CameraButtonPosition", Value::udim2(0, 0, 0, 0)),
        P("CloseButtonPosition", Value::udim2(0, 0, 0, 0)), P("CloseWhenScreenshotTaken", Value::boolean(false)), P("HideCoreGuiForCaptures", Value::boolean(false)),
        P("HidePlayerGuiForCaptures", Value::boolean(false))};
    r.service("StudioCaptureService");   // CanCaptureScreenshot is false here; the rest is not answered
    r.add("StudioScreenshotCapture", "Instance", false).props = {PEnum("BufferFormat", "StudioCaptureScreenshotFormat", "RGBA8", ReadOnly | PluginWrite),
        PEnum("BufferStatus", "StudioCaptureBufferStatus", "NotStarted", ReadOnly | PluginWrite), P("OriginalSize", Value::vector2(0, 0), ReadOnly | PluginWrite),
        P("Resolution", Value::vector2(0, 0), ReadOnly | PluginWrite), PEnum("UICaptureMode", "UICaptureMode", "All", ReadOnly | PluginWrite)};

    // The CoreGui's panels as a script configures them. Recorded, not acted on: the engine's own
    // player list and capture panels do not read these. Their defaults are not verified here.
    r.add("BaseCoreGuiConfiguration", "Instance", false);
    r.add("CapturesViewConfiguration", "BaseCoreGuiConfiguration", false).props = {P("Open", Value::boolean(false))};
    r.add("PlayerListConfiguration", "BaseCoreGuiConfiguration", false).props = {P("Open", Value::boolean(false))};
    r.add("SelfViewConfiguration", "BaseCoreGuiConfiguration", false).props = {P("Open", Value::boolean(false))};
    r.service("CoreGuiConfiguration").props = {PRef("CapturesViewConfiguration"), PRef("PlayerListConfiguration"), PRef("SelfViewConfiguration")};

    // The engine settings under Roblox's settings(). Recorded, not acted on: nothing here reads a
    // quality level, a throttle or a debug flag. DebugSettings reads the live instance and player
    // counts and this engine's version; TaskScheduler's readings are this thread's (rbx_api.cpp).
    // Unserialized defaults are not verified here (the type's zero).
    auto& debugSettings = r.service("DebugSettings");
    debugSettings.props = {P("DataModel", Value::number(0), ReadOnly | PluginWrite), P("InstanceCount", Value::number(0), ReadOnly | PluginWrite),
        P("IsScriptStackTracingEnabled", Value::boolean(false), PluginWrite), P("JobCount", Value::number(0), ReadOnly | PluginWrite), P("PlayerCount", Value::number(0), ReadOnly | PluginWrite),
        P("ReportSoundWarnings", Value::boolean(false), PluginWrite), P("RobloxVersion", Value::string(""), ReadOnly | PluginWrite),
        PEnum("TickCountPreciseOverride", "TickCountSampleMethod", "Fast", PluginWrite)};
    auto& networkSettings = r.service("NetworkSettings");
    networkSettings.props = {P("EmulatedTotalMemoryInMB", Value::number(0), Hidden | NoReplicate | PluginWrite), P("FreeMemoryMBytes", Value::number(0), ReadOnly | Hidden | NoReplicate | PluginWrite),
        P("HttpProxyEnabled", Value::boolean(false), NoScriptRead | NoScriptWrite), P("HttpProxyURL", Value::string(""), NoScriptRead | NoScriptWrite),
        P("InboundNetworkJitterMs", Value::number(0), NoReplicate | PluginWrite), P("InboundNetworkLossPercent", Value::number(0), NoReplicate | PluginWrite),
        P("InboundNetworkMinDelayMs", Value::number(0), NoReplicate | PluginWrite), P("IncomingReplicationLag", Value::number(0)),
        P("OutboundNetworkJitterMs", Value::number(0), NoReplicate | PluginWrite), P("OutboundNetworkLossPercent", Value::number(0), NoReplicate | PluginWrite),
        P("OutboundNetworkMinDelayMs", Value::number(0), NoReplicate | PluginWrite), P("PrintJoinSizeBreakdown", Value::boolean(false)), P("PrintPhysicsErrors", Value::boolean(false)),
        P("PrintStreamInstanceQuota", Value::boolean(false)), P("RandomizeJoinInstanceOrder", Value::boolean(false)), P("RenderStreamedRegions", Value::boolean(false)),
        P("ShowActiveAnimationAsset", Value::boolean(false))};
    auto& physicsSettings = r.service("PhysicsSettings");
    for (const char* n : {"AllowSleep", "AreAnchorsShown", "AreAssembliesShown", "AreAwakePartsHighlighted", "AreBodyTypesShown", "AreContactIslandsShown", "AreContactPointsShown",
                          "AreJointCoordinatesShown", "AreMechanismsShown", "AreModelCoordsShown", "AreNonAnchorsShown", "AreOwnersShown", "ArePartCoordsShown", "AreRegionsShown",
                          "AreTerrainReplicationRegionsShown", "AreUnalignedPartsShown", "AreWorldCoordsShown", "DisableCSGv2", "DisableCSGv3ForPlugins", "ForceCSGv2",
                          "IsInterpolationThrottleShown", "IsReceiveAgeShown", "IsTreeShown", "ShowDecompositionGeometry", "UseCSGv2"})
        physicsSettings.props.push_back(P(n, Value::boolean(false), PluginWrite));
    for (const char* n : {"AreAssemblyCentersOfMassShown", "AreCollisionCostsShown", "AreConstraintForcesShownForSelectedOrHoveredInstances",
                          "AreConstraintTorquesShownForSelectedOrHoveredInstances", "AreContactForcesShownForSelectedOrHoveredAssemblies",
                          "AreGravityForcesShownForSelectedOrHoveredAssemblies", "AreMagnitudesShownForDrawnForcesAndTorques", "AreSolverIslandsShown", "AreTimestepsShown",
                          "DrawConstraintsNetForce", "DrawContactsNetForce", "DrawTotalNetForce", "EnableForceVisualizationSmoothing",
                          "ShowFluidForcesForSelectedOrHoveredMechanisms", "ShowInstanceNamesForDrawnForcesAndTorques"})
        physicsSettings.props.push_back(P(n, Value::boolean(false), NoScriptRead | NoScriptWrite));
    physicsSettings.props.insert(physicsSettings.props.end(), {P("FluidForceDrawScale", Value::number(0), NoScriptRead | NoScriptWrite), P("ForceDrawScale", Value::number(0), NoScriptRead | NoScriptWrite),
        P("ForceVisualizationSmoothingSteps", Value::number(0), NoScriptRead | NoScriptWrite), P("TorqueDrawScale", Value::number(0), NoScriptRead | NoScriptWrite),
        PEnum("PhysicsEnvironmentalThrottle", "EnviromentalPhysicsThrottle", "DefaultAuto", PluginWrite), P("ThrottleAdjustTime", Value::number(0), PluginWrite),
        PEnum("SolverConvergenceMetricType", "SolverConvergenceMetricType", "IterationBased", NoScriptRead | NoScriptWrite),
        PEnum("SolverConvergenceVisualizationMode", "SolverConvergenceVisualizationMode", "Disabled", NoScriptRead | NoScriptWrite)});
    // GetMaxQualityLevel answers 21, QualityLevel's last (rbx_api.cpp).
    r.service("RenderSettings").props = {P("AutoFRMLevel", Value::number(0), PluginWrite), P("EagerBulkExecution", Value::boolean(false), PluginWrite),
        PEnum("EditQualityLevel", "QualityLevel", "Automatic", PluginWrite), P("Enable VR Mode", Value::boolean(false), PluginWrite), P("EnableFRM", Value::boolean(false), Hidden | NoReplicate | PluginWrite),
        P("ExportMergeByMaterial", Value::boolean(false), PluginWrite), PEnum("FrameRateManager", "FramerateManagerMode", "Automatic", PluginWrite),
        PEnum("GraphicsMode", "GraphicsMode", "Automatic", PluginWrite), P("MeshCacheSize", Value::number(0), PluginWrite),
        PEnum("MeshPartDetailLevel", "MeshPartDetailLevel", "DistanceBased", PluginWrite), PEnum("QualityLevel", "QualityLevel", "Automatic", PluginWrite),
        P("ReloadAssets", Value::boolean(false), PluginWrite), P("RenderCSGTrianglesDebug", Value::boolean(false), PluginWrite), P("ShowBoundingBoxes", Value::boolean(false), PluginWrite),
        PEnum("ViewMode", "ViewMode", "None", PluginWrite)};
    r.service("TaskScheduler").props = {P("SchedulerDutyCycle", Value::number(0), ReadOnly), P("SchedulerRate", Value::number(0), ReadOnly),
                                        PEnum("ThreadPoolConfig", "ThreadPoolConfig", "Auto"), P("ThreadPoolSize", Value::number(0), ReadOnly)};

    // The Studio's dragger settings, a plugin's to read and write. Recorded, not acted on: the
    // Studio here drags on its own settings. Their defaults are not verified here.
    r.service("DraggerService").props = {P("AlignDraggedObjects", Value::boolean(false), NoReplicate), P("AngleSnapEnabled", Value::boolean(false), NoReplicate),
        P("AngleSnapIncrement", Value::number(0), NoReplicate), P("AnimateHover", Value::boolean(false), NoReplicate), P("CollisionsEnabled", Value::boolean(false), NoReplicate),
        PEnum("DraggerCoordinateSpace", "DraggerCoordinateSpace", "Object", NoReplicate), PEnum("DraggerMovementMode", "DraggerMovementMode", "Geometric", NoReplicate),
        P("GeometrySnapColor", Value::color3(0, 0, 0), NoReplicate), P("HoverAnimateFrequency", Value::number(0), NoReplicate), P("HoverThickness", Value::number(0), NoReplicate),
        P("JointsEnabled", Value::boolean(false), NoReplicate), P("LinearSnapEnabled", Value::boolean(false), NoReplicate), P("LinearSnapIncrement", Value::number(0), NoReplicate),
        P("PartSnapEnabled", Value::boolean(false), NoReplicate | NoScriptRead | NoScriptWrite), P("PivotSnapToGeometry", Value::boolean(false), NoReplicate | NoScriptRead | NoScriptWrite),
        P("ShowHover", Value::boolean(false), NoReplicate), P("ShowPivotIndicator", Value::boolean(false), NoReplicate),
        P("UseBoundingBoxes", Value::boolean(false), NoReplicate | NoScriptRead | NoScriptWrite)};

    // A Studio package's link. Recorded, not acted on: nothing here publishes or updates a package.
    // PackageContent is PackageId's Content twin (kPairs). Status: kRestShared.
    r.add("PackageLink", "Instance", false).props = {P("AutoUpdate", Value::boolean(false), NoScriptRead | NoScriptWrite), P("Creator", Value::string(""), ReadOnly | NoReplicate),
        P("DefaultName", Value::string(""), NoScriptWrite), P("PackageAssetName", Value::string(""), ReadOnly | NoReplicate),
        PEnum("PermissionLevel", "PackagePermission", "None", ReadOnly | NoReplicate), P("SerializedDefaultAttributes", Value::string(""), NoScriptWrite | Hidden),
        P("VersionNumber", Value::number(0), NoReplicate | NoScriptWrite)};
    // The Studio's reflection metadata (ReflectionMetadata.xml). Recorded, not acted on: the
    // Explorer here orders by its own table. Unserialized defaults are not verified here.
    r.add("ReflectionMetadataItem", "Instance", false).props = {P("Browsable", Value::boolean(false)), P("ClassCategory", Value::string("")), P("ClientOnly", Value::boolean(false)),
        P("Constraint", Value::string("")), P("Deprecated", Value::boolean(false)), P("EditingDisabled", Value::boolean(false)), P("EditorType", Value::string("")),
        P("FFlag", Value::string("")), P("IsBackend", Value::boolean(false)), P("PropertyOrder", Value::number(0)), P("ScriptContext", Value::string("")),
        P("ServerOnly", Value::boolean(false)), P("SliderScaling", Value::string("")), P("UIMaximum", Value::number(0)), P("UIMinimum", Value::number(0)),
        P("UINumTicks", Value::number(0))};
    r.add("ReflectionMetadataClass", "ReflectionMetadataItem").props = {P("ExplorerImageIndex", Value::number(0)), P("ExplorerOrder", Value::number(2147483647)),
        P("Insertable", Value::boolean(true)), P("PreferredParent", Value::string(""))};
    // Reads this registry: GetClasses / GetClass / GetPropertiesOfClass / GetMethodsOfClass /
    // GetEventsOfClass (rbx_api.cpp).
    r.service("ReflectionService");
    // Base64Encode / Base64Decode (rbx_api.cpp). The hashes and zstd are not answered: no
    // Blake, MD5, SHA or zstd here.
    r.service("EncodingService");

    // Studio's, the network's and Roblox's own services, declared so GetService answers. Their
    // members are the backends' (rbx_api.cpp says which decline here); the rest are not answered.
    r.service("AnimationClipProvider");
    r.add("AnimationNodeDefinition", "Instance").props = {P("NodeId", Value::string(""), NoScriptRead | NoScriptWrite), PEnum("NodeType", "AnimationNodeType", "InvalidNode"),
                                                          P("InputPins", Value::string(""), Hidden)};   // the ordered pins, comma-joined (rbx_api.cpp)
    for (auto& o : r.owned) if (o->name == "AnimationNodeDefinition") o->events = {"InputPinsChanged"};
    r.service("AssetDeliveryProxy").props = {P("Interface", Value::string("")), P("Port", Value::number(0)), P("StartServer", Value::boolean(false))};
    r.add("AssetPatchSettings", "Instance", false).props = {P("ContentId", Value::string("")), P("OutputPath", Value::string("")), P("PatchId", Value::string(""))};
    r.service("IncrementalPatchBuilder").props = {P("AddPathsToBundle", Value::boolean(false)), P("BuildDebouncePeriod", Value::number(0)), P("HighCompression", Value::boolean(false)),
        P("SerializePatch", Value::boolean(false)), P("UseFileLevelCompressionInsteadOfChunk", Value::boolean(false)), P("ZstdCompression", Value::boolean(false))};
    // An ad unit's state: never Active here, as no ad is served. Status: kRestShared.
    r.add("AdPortal", "Instance").props = {P("PortalInvalidReason", Value::string(""), ReadOnly | NoReplicate | NoScriptWrite)};
    auto& commerce = r.service("CommerceService");
    commerce.events = {"PromptCommerceProductPurchaseFinished"};   // never fires: the prompts decline
    auto& configService = r.service("ConfigService");
    // GetConfigAsync gives a snapshot with no keys: GetValue answers nil, Outdated stays false,
    // UpdateAvailable never fires. SetTestingValue / ClearTestingValue keep test keys in the
    // service's hidden blob, which a snapshot then answers (rbx_api.cpp). Error: kRestShared.
    configService.props = {P("TestingValues", Value::string("{}"), Hidden)};
    r.add("ConfigSnapshot", "Object", false).props = {P("Outdated", Value::boolean(false), ReadOnly), P("ValuesBlob", Value::string("{}"), Hidden)};
    for (auto& o : r.owned) if (o->name == "ConfigSnapshot") o->events = {"UpdateAvailable"};
    // The legacy Controller: BindButton / UnbindButton keep the bound buttons, GetButton answers
    // false and ButtonChanged / AxisChanged never fire, as no Enum.Button input reaches one here.
    auto& controller = r.add("Controller", "Instance", false);
    controller.events = {"ButtonChanged"};
    controller.props = {P("BoundButtons", Value::string(""), Hidden)};
    auto& skate = r.add("SkateboardController", "Controller");
    skate.props = {P("Steer", Value::number(0), ReadOnly), P("Throttle", Value::number(0), ReadOnly)};
    skate.events = {"AxisChanged"};
    r.add("CustomLog", "Instance");   // Open / Close / WriteAppend write nothing; GetLogPath is "" (rbx_api.cpp)
    r.add("ExperienceInviteOptions", "Instance").props = {P("InviteMessageId", Value::string("")), P("InviteUser", Value::number(0)), P("LaunchData", Value::string("")),
                                                          P("PromptMessage", Value::string(""))};
    // A file a plugin was handed. Nothing here hands one out: GetBinaryContents is "" and
    // GetTemporaryId "". Size: kRestShared.
    r.add("File", "Instance", false);
    r.service("GamepadService").props = {P("GamepadCursorEnabled", Value::boolean(false), NoScriptRead | NoScriptWrite)};   // Enable/DisableGamepadCursor record it
    r.add("GeneratedFolder", "Folder");   // SetPrimaryPart keeps the part in the hidden ref (rbx_api.cpp)
    for (auto& o : r.owned) if (o->name == "GeneratedFolder") o->props = {PRef("GeneratedPrimaryPart", Hidden)};
    r.service("GenerationService");
    r.service("HeapProfilerService").events = {"OnNewData"};
    r.service("ScriptProfilerService").events = {"OnNewData"};
    auto& debugger = r.service("ScriptDebuggerService");
    debugger.events = {"Resumed"};
    debugger.callbacks = {"OnStopped"};
    r.service("InstanceFileSyncService").events = {"StatusChanged"};
    r.service("MLService");
    r.add("MLSession", "Object", false);
    r.service("MatchmakingService");   // GetServerAttribute / SetServerAttribute keep this server's attributes in the hidden blob (rbx_api.cpp)
    for (auto& o : r.owned) if (o->name == "MatchmakingService") o->props = {P("ServerAttributes", Value::string("{}"), Hidden)};
    r.add("MemStorageConnection", "Instance", false);   // Disconnect: nothing here to disconnect
    r.service("ModerationService");
    // The network objects. Nothing here exposes its peers: none is ever made and no event fires.
    r.add("NetworkPeer", "Instance", false);
    r.service("NetworkClient", "NetworkPeer").events = {"ConnectionAccepted", "ConnectionFailed"};
    r.service("NetworkServer", "NetworkPeer");
    r.add("NetworkReplicator", "Instance", false);
    r.add("NetworkMarker", "Instance", false).events = {"Received"};
    r.service("PlayerViewService");   // GetDeviceCameraCFrame is the current camera's (rbx_api.cpp)
    // A generated model. Recorded, not acted on: no generator here, so ForceGeneration and
    // WaitForGenerationAsync answer false and GenerationError stays "". Size: kRestShared.
    r.add("ProceduralModel", "Model").props = {P("GenerationError", Value::string(""), ReadOnly), PRef("Generator")};
    r.service("RecommendationService");
    r.service("RemoteCommandService");
    r.service("SceneAnalysisService");
    r.add("SelectionLasso", "GuiBase3d", false);   // Humanoid: kRestShared
    r.service("SerializationService");
    r.service("StudioDeviceSimulatorService").events = {"ConfigurationChanged"};
    r.service("StudioTestService").props = {P("EditModeActive", Value::boolean(false), PluginWrite)};
    r.add("SurfaceSelection", "PartAdornment").props = {PEnum("TargetSurface", "NormalId", "Right")};
    r.add("TextChannelWindow", "GuiObject");   // Target: kRestShared; nothing here draws a channel's window
    // A text generator: no model here, so GenerateTextAsync is not answered.
    r.add("TextGenerator", "Instance").props = {P("Seed", Value::number(0)), P("SystemPrompt", Value::string("")), P("Temperature", Value::number(0.7)), P("TopP", Value::number(0.9))};
    r.service("UniqueIdLookupService");
    // Registered last on purpose: service ids are positional, and a service added ahead of
    // one that already exists moves every id after it. A client and server from different
    // builds would then disagree on which instance a service is.
    // The Studio's edit state, as a plugin reads it. Recorded, not acted on: the Studio here keeps its own
    // grid and rotation steps; GetUserId is 0 (no account), GetClassIcon an empty image, the import
    // prompts decline (rbx_api.cpp). RotateIncrement's Roblox default is not verified here; Secrets is not
    // declared (no SecretsService here).
    auto& studioService = r.service("StudioService");
    studioService.props = {PRef("ActiveScript", ReadOnly), P("DraggerSolveConstraints", Value::boolean(false)), P("GridSize", Value::number(1)),
                           P("RotateIncrement", Value::number(0)), P("ShowConstraintDetails", Value::boolean(false)),
                           P("ShowWeldDetails", Value::boolean(false), NoScriptRead | NoScriptWrite), P("StudioLocaleId", Value::string("en-us")),
                           P("UseLocalSpace", Value::boolean(false))};
    r.add("VirtualInput", "Object", false);   // its Send* methods are not answered: nothing here hands out one
    // An HttpService:CreateWebStreamClient result; none is ever made here, so the state is Closed.
    auto& webStream = r.add("WebStreamClient", "Object", false);
    webStream.props = {PEnum("ConnectionState", "WebStreamClientState", "Closed", ReadOnly)};
    webStream.events = {"Closed", "Opened"};
    webStream.events.push_back("Error");
    webStream.events.push_back("MessageReceived");
    // A WorldRoot of its own, for a ViewportFrame: its parts are not simulated here, and its spatial
    // queries are WorldRoot's. UseWorkspaceCollisionGroups is recorded, not acted on.
    r.add("WorldModel", "WorldRoot").props = {P("UseWorkspaceCollisionGroups", Value::boolean(false))};
    r.add("WrapTextureTransfer", "Instance").props = {P("UVMaxBound", Value::vector2(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity())),
                                                      P("UVMinBound", Value::vector2(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()))};
    for (auto& o : r.owned) if (o->name == "ChatWindowMessageProperties") o->props.push_back(PRef("PrefixTextProperties"));   // a ChatWindowMessageProperties for the prefix

    // Every Content property Roblox pairs with an older ContentId property, each with its own
    // security. The pairings come from rbx-dom's serializer patches, which migrate them, and each
    // side's flags from the class reference. The Content is added beside its twin, defaulting to
    // the twin's default; objects is what Content.fromObject may hold there -- an EditableImage, an
    // EditableMesh, or nothing. Where the reference names no object, an image property still gets
    // EditableImage, since Roblox announced one for every image Content.
    struct ContentPair { const char* cls; const char* legacy; const char* content; unsigned legacyFlags, contentFlags; const char* objects; };
    static const ContentPair kPairs[] = {
        {"Animation", "AnimationId", "AnimationContent", 0, 0, nullptr},
        {"CharacterMesh", "BaseTextureId", "BaseTextureContent", 0, 0, nullptr},
        {"CharacterMesh", "MeshId", "MeshContent", 0, 0, nullptr},
        {"CharacterMesh", "OverlayTextureId", "OverlayTextureContent", 0, 0, nullptr},
        {"Pants", "PantsTemplate", "PantsTemplateContent", 0, 0, nullptr},
        {"Shirt", "ShirtTemplate", "ShirtTemplateContent", 0, 0, nullptr},
        {"ShirtGraphic", "Graphic", "TextureContent", 0, 0, nullptr},
        {"BackpackItem", "TextureId", "TextureContent", 0, 0, nullptr},
        {"Beam", "Texture", "TextureContent", 0, 0, "EditableImage"},
        {"ClickDetector", "CursorIcon", "CursorIconContent", 0, 0, nullptr},
        {"PluginToolbarButton", "Icon", "IconContent", 0, PluginWrite, nullptr},
        {"Decal", "Texture", "TextureContent", 0, 0, "EditableImage"},
        // A normal, roughness or metalness map makes the Decal a PBR surface.
        {"Decal", "MetalnessMap", "MetalnessMapContent", PluginRead | PluginWrite | NotBrowsable, PluginWrite, "EditableImage"},
        {"Decal", "NormalMap", "NormalMapContent", PluginRead | PluginWrite | NotBrowsable, PluginWrite, "EditableImage"},
        {"Decal", "RoughnessMap", "RoughnessMapContent", PluginRead | PluginWrite | NotBrowsable, PluginWrite, "EditableImage"},
        {"FileMesh", "MeshId", "MeshContent", 0, 0, "EditableMesh"},
        {"FileMesh", "TextureId", "TextureContent", 0, 0, "EditableImage"},
        {"ImageButton", "Image", "ImageContent", 0, 0, "EditableImage"},
        {"ImageButton", "HoverImage", "HoverImageContent", 0, 0, nullptr},
        {"ImageButton", "PressedImage", "PressedImageContent", 0, 0, nullptr},
        {"ImageLabel", "Image", "ImageContent", 0, 0, "EditableImage"},
        {"MaterialVariant", "ColorMap", "ColorMapContent", PluginRead | PluginWrite, PluginRead | PluginWrite, nullptr},
        {"MaterialVariant", "MetalnessMap", "MetalnessMapContent", PluginRead | PluginWrite, PluginRead | PluginWrite, nullptr},
        {"MaterialVariant", "NormalMap", "NormalMapContent", PluginRead | PluginWrite, PluginRead | PluginWrite, nullptr},
        {"MaterialVariant", "RoughnessMap", "RoughnessMapContent", PluginRead | PluginWrite, PluginRead | PluginWrite, nullptr},
        {"MeshPart", "MeshId", "MeshContent", NoScriptWrite, NoScriptWrite, "EditableMesh"},
        {"MeshPart", "TextureID", "TextureContent", 0, 0, "EditableImage"},
        {"Mouse", "Icon", "IconContent", 0, 0, nullptr},
        {"ParticleEmitter", "Texture", "TextureContent", 0, 0, "EditableImage"},
        {"ScrollingFrame", "BottomImage", "BottomImageContent", 0, 0, nullptr},
        {"ScrollingFrame", "MidImage", "MidImageContent", 0, 0, nullptr},
        {"ScrollingFrame", "TopImage", "TopImageContent", 0, 0, nullptr},
        {"Sky", "MoonTextureId", "MoonTextureContent", 0, 0, "EditableImage"},
        {"Sky", "SkyboxBk", "SkyboxBackContent", 0, 0, "EditableImage"},
        {"Sky", "SkyboxDn", "SkyboxDownContent", 0, 0, "EditableImage"},
        {"Sky", "SkyboxFt", "SkyboxFrontContent", 0, 0, "EditableImage"},
        {"Sky", "SkyboxLf", "SkyboxLeftContent", 0, 0, "EditableImage"},
        {"Sky", "SkyboxRt", "SkyboxRightContent", 0, 0, "EditableImage"},
        {"Sky", "SkyboxUp", "SkyboxUpContent", 0, 0, "EditableImage"},
        {"Sky", "SunTextureId", "SunTextureContent", 0, 0, "EditableImage"},
        {"Sound", "SoundId", "AudioContent", 0, NotBrowsable, nullptr},
        {"AudioPlayer", "Asset", "AudioContent", 0, NotBrowsable, nullptr},
        {"SurfaceAppearance", "ColorMap", "ColorMapContent", PluginRead | PluginWrite, PluginWrite | NotBrowsable, "EditableImage"},
        {"SurfaceAppearance", "MetalnessMap", "MetalnessMapContent", PluginRead | PluginWrite, PluginWrite | NotBrowsable, "EditableImage"},
        {"SurfaceAppearance", "NormalMap", "NormalMapContent", PluginRead | PluginWrite, PluginWrite | NotBrowsable, "EditableImage"},
        {"SurfaceAppearance", "RoughnessMap", "RoughnessMapContent", PluginRead | PluginWrite, PluginWrite | NotBrowsable, "EditableImage"},
        {"Trail", "Texture", "TextureContent", 0, 0, "EditableImage"},
        {"UserInputService", "MouseIcon", "MouseIconContent", 0, 0, nullptr},
        {"ImageHandleAdornment", "Image", "ImageContent", 0, 0, "EditableImage"},
        {"VideoFrame", "Video", "VideoContent", 0, 0, nullptr},
        {"UIDragDetector", "CursorIcon", "CursorIconContent", 0, 0, nullptr},
        {"UIDragDetector", "ActivatedCursorIcon", "ActivatedCursorIconContent", 0, 0, nullptr},
        {"BaseWrap", "CageMeshId", "CageMeshContent", PluginWrite, PluginWrite, "EditableMesh"},
        {"WrapLayer", "ReferenceMeshId", "ReferenceMeshContent", PluginWrite, PluginWrite, "EditableMesh"},
        // Hidden surface removal for layered clothing, generated when a BaseWrap is published.
        {"BaseWrap", "HSRAssetId", "HSRContent", NoScriptRead | NoScriptWrite, NoScriptRead | NoScriptWrite, nullptr},
        // the rest family
        {"ScreenshotHud", "CameraButtonIcon", "CameraButtonIconContent", 0, 0, nullptr},
        {"PackageLink", "PackageId", "PackageContent", ReadOnly | NoReplicate, ReadOnly | NoReplicate, nullptr},
    };
    for (const ContentPair& pair : kPairs) {
        ClassDef* cls = nullptr;
        for (auto& owned : r.owned) if (owned->name == pair.cls) cls = owned.get();
        if (!cls) continue;
        PropDef* legacy = nullptr;
        for (PropDef& p : cls->props) if (p.name == pair.legacy) legacy = &p;
        if (!legacy) { cls->props.push_back(P(pair.legacy, Value::string(""))); legacy = &cls->props.back(); }
        legacy->flags |= pair.legacyFlags;
        legacy->twin = pair.content;
        Value def = Value::contentUri(legacy->def.s);
        PropDef content{pair.content, Value::Content, def, pair.contentFlags, nullptr, pair.legacy};
        content.contentObjects = pair.objects;
        cls->props.push_back(std::move(content));
    }

    struct LoneContent { const char* cls; const char* name; unsigned flags; const char* objects; };
    static const LoneContent kLone[] = {
        // An emissive mask is greyscale: black is no emissivity, white is full.
        {"SurfaceAppearance", "EmissiveMaskContent", PluginWrite, "EditableImage"},
        {"MaterialVariant", "EmissiveMaskContent", PluginRead | PluginWrite, nullptr},
        // Roblox packs a PBR surface's maps into one asset on publish; nothing here publishes packs.
        {"Decal", "TexturePackContent", NoScriptRead | NoScriptWrite, nullptr},
        {"SurfaceAppearance", "TexturePackContent", NoScriptRead | NoScriptWrite, nullptr},
        // the rest family
        {"InputBinding", "DisplayImage", 0, nullptr},
        {"InputActionLabel", "ResolvedImageContent", ReadOnly, nullptr},
        {"VideoPlayer", "VideoContent", 0, nullptr},
        {"VideoSampler", "VideoContent", ReadOnly, nullptr},
        {"WrapTextureTransfer", "ReferenceCageMeshContent", 0, nullptr},
    };
    for (const LoneContent& lone : kLone) {
        ClassDef* cls = nullptr;
        for (auto& owned : r.owned) if (owned->name == lone.cls) cls = owned.get();
        if (!cls) continue;
        PropDef content{lone.name, Value::Content, Value::content(Value::ContentNone), lone.flags, nullptr};
        content.contentObjects = lone.objects;
        cls->props.push_back(std::move(content));
    }

    // The gui family's members whose names other classes carry too (a part's Size, a Sound's
    // Volume): one row a class and member, the defaults Roblox's. UIPageLayout.SortOrder orders its
    // pages (rbx_api.cpp); the rest are recorded, not acted on: nothing here draws an adornment, a
    // shadow or a table, slides a page, plays a video or drags a GuiObject. VideoFrame's roll-off
    // defaults are Sound's; a Path2D's, a UIShadow's and UIStroke.ZIndex's Roblox defaults are not
    // verified here.
    struct GuiShared { const char* cls; const char* name; Value def; const char* enumName = nullptr; };
    const GuiShared kGuiShared[] = {
        {"GuiBase3d", "Transparency", Value::number(0)},
        {"GuiBase3d", "Visible", Value::boolean(true)},
        {"PVAdornment", "Adornee", Value::instance(0)},
        {"PartAdornment", "Adornee", Value::instance(0)},
        {"InstanceAdornment", "Adornee", Value::instance(0)},
        {"HandleAdornment", "AlwaysOnTop", Value::boolean(false)},
        {"HandleAdornment", "ZIndex", Value::number(-1)},
        {"BoxHandleAdornment", "Size", Value::vector3(1, 1, 1)},
        {"ImageHandleAdornment", "Size", Value::vector2(1, 1)},
        {"LineHandleAdornment", "Thickness", Value::number(1)},
        {"WireframeHandleAdornment", "Scale", Value::vector3(1, 1, 1)},
        {"WireframeHandleAdornment", "Thickness", Value::number(1)},
        {"Path2D", "Thickness", Value::number(1)},
        {"Path2D", "Visible", Value::boolean(true)},
        {"Path2D", "ZIndex", Value::number(1)},
        {"VideoFrame", "Looped", Value::boolean(false)},
        {"VideoFrame", "TimePosition", Value::number(0)},
        {"VideoFrame", "Volume", Value::number(1)},
        {"VideoFrame", "RollOffMaxDistance", Value::number(10000)},
        {"VideoFrame", "RollOffMinDistance", Value::number(10)},
        {"VideoFrame", "RollOffMode", Value::enumItem("Inverse", 0), "RollOffMode"},
        {"UIPageLayout", "Padding", Value::udim(0, 0)},
        {"UIPageLayout", "SortOrder", Value::enumItem("Name", 0), "SortOrder"},
        {"UIPageLayout", "EasingStyle", Value::enumItem("Back", 2), "EasingStyle"},
        {"UIPageLayout", "EasingDirection", Value::enumItem("Out", 1), "EasingDirection"},
        {"UITableLayout", "Padding", Value::udim2(0, 0, 0, 0)},
        {"UITableLayout", "SortOrder", Value::enumItem("Name", 0), "SortOrder"},
        {"UIDragDetector", "Enabled", Value::boolean(true)},
        {"UIShadow", "Enabled", Value::boolean(true)},
        {"UIShadow", "Color", Value::color3(0, 0, 0)},
        {"UIShadow", "Transparency", Value::number(0)},
        {"UIShadow", "ZIndex", Value::number(0)},
        {"UIStroke", "ZIndex", Value::number(0)},
    };
    for (const GuiShared& row : kGuiShared) {
        ClassDef* cls = nullptr;
        for (auto& owned : r.owned) if (owned->name == row.cls) cls = owned.get();
        if (!cls) continue;
        cls->props.push_back({row.name, row.def.type, row.def, 0, row.enumName ? findEnum(row.enumName) : nullptr});
    }
    // The audio family's members whose names other classes carry too. Active is Roblox's
    // read-anyone, write-RobloxScript, as Sound.IsPlaying is here.
    struct AudioShared { const char* cls; const char* name; Value def; unsigned flags = 0; };
    const AudioShared kAudioShared[] = {
        {"AudioFader", "Volume", Value::number(1)},
        {"AudioEcho", "DryLevel", Value::number(0)},
        {"AudioEcho", "WetLevel", Value::number(0)},
        {"AudioChorus", "Rate", Value::number(5)},
        {"AudioFlanger", "Rate", Value::number(5)},
        {"AudioTremolo", "Frequency", Value::number(5)},
        {"AudioTremolo", "Shape", Value::number(0)},
        {"AudioDeviceInput", "Active", Value::boolean(true), ReadOnly},
        {"AudioDeviceInput", "Player", Value::instance(0)},
        {"AudioDeviceInput", "Volume", Value::number(1)},
        {"AudioSearchParams", "Title", Value::string("")},
        {"AudioSpeechToText", "Enabled", Value::boolean(false)},
        {"AudioSpeechToText", "Text", Value::string("")},
        {"AudioTextToSpeech", "PlaybackSpeed", Value::number(1)},
        {"AudioTextToSpeech", "Speed", Value::number(1)},
        {"AudioTextToSpeech", "Text", Value::string("")},
        {"AudioTextToSpeech", "TimePosition", Value::number(0)},
        {"AudioTextToSpeech", "Volume", Value::number(1)},
    };
    for (const AudioShared& row : kAudioShared) {
        ClassDef* cls = nullptr;
        for (auto& owned : r.owned) if (owned->name == row.cls) cls = owned.get();
        if (!cls) continue;
        cls->props.push_back({row.name, row.def.type, row.def, row.flags, nullptr});
    }
    // The services family's members whose names the GUI classes carry too. The chat configurations'
    // readings are zero: the engine's chat draws its own window, so no rect, focus or box is theirs.
    // A BubbleChatMessageProperties' look is BubbleChatConfiguration's here. A TeleportAsyncResult is
    // never made here.
    struct ServicesShared { const char* cls; const char* name; Value def; unsigned flags; };
    const ServicesShared kServicesShared[] = {
        {"ChatWindowConfiguration", "AbsolutePosition", Value::vector2(0, 0), ReadOnly},
        {"ChatWindowConfiguration", "AbsoluteSize", Value::vector2(0, 0), ReadOnly},
        {"ChatInputBarConfiguration", "AbsolutePosition", Value::vector2(0, 0), ReadOnly},
        {"ChatInputBarConfiguration", "AbsoluteSize", Value::vector2(0, 0), ReadOnly},
        {"ChatInputBarConfiguration", "IsFocused", Value::boolean(false), ReadOnly},
        {"ChatInputBarConfiguration", "TextBox", Value::instance(0), ReadOnly},
        {"ChannelTabsConfiguration", "AbsolutePosition", Value::vector2(0, 0), ReadOnly},
        {"ChannelTabsConfiguration", "AbsoluteSize", Value::vector2(0, 0), ReadOnly},
        {"BubbleChatMessageProperties", "BackgroundColor3", Value::color3(25 / 255.0, 27 / 255.0, 29 / 255.0), 0},
        {"BubbleChatMessageProperties", "BackgroundTransparency", Value::number(0.3), 0},
        {"BubbleChatMessageProperties", "FontFace", Value::font("rbxasset://fonts/families/GothamSSm.json", 500, false), 0},
        {"BubbleChatMessageProperties", "TextColor3", Value::color3(1, 1, 1), 0},
        {"BubbleChatMessageProperties", "TextSize", Value::number(14), 0},
        {"ChatWindowMessageProperties", "BackgroundColor3", Value::color3(25 / 255.0, 27 / 255.0, 29 / 255.0), 0},
        {"ChatWindowMessageProperties", "BackgroundTransparency", Value::number(0.3), 0},
        {"ChatWindowMessageProperties", "FontFace", Value::font("rbxasset://fonts/families/GothamSSm.json", 500, false), 0},
        {"ChatWindowMessageProperties", "TextColor3", Value::color3(1, 1, 1), 0},
        {"ChatWindowMessageProperties", "TextSize", Value::number(14), 0},
        {"ChatWindowMessageProperties", "TextStrokeColor3", Value::color3(0, 0, 0), 0},
        {"ChatWindowMessageProperties", "TextStrokeTransparency", Value::number(0.5), 0},
        {"TeleportAsyncResult", "PrivateServerId", Value::string(""), ReadOnly},
        {"DataStoreSetOptions", "Metadata", Value::string("{}"), Hidden},
        {"DataStoreIncrementOptions", "Metadata", Value::string("{}"), Hidden},
        {"DataStoreKeyInfo", "Metadata", Value::string("{}"), ReadOnly | Hidden},
    };
    for (const ServicesShared& row : kServicesShared) {
        ClassDef* cls = nullptr;
        for (auto& owned : r.owned) if (owned->name == row.cls) cls = owned.get();
        if (!cls) continue;
        cls->props.push_back({row.name, row.def.type, row.def, row.flags, nullptr});
    }
    // The data family's members whose names other classes carry too. A BrickColorValue's Value is
    // Medium stone grey, read and written as a BrickColor. A PluginAction's Text is the engine's to
    // write (CreatePluginAction). A PluginMenu's three and a PluginDragEvent's Position are recorded, not
    // acted on: nothing here draws a menu or drags.
    struct DataShared { const char* cls; const char* name; Value def; unsigned flags; };
    const DataShared kDataShared[] = {
        {"BrickColorValue", "Value", Value::color3(163 / 255.f, 162 / 255.f, 165 / 255.f), Brick},
        {"PluginAction", "Text", Value::string(""), NoScriptWrite},
        {"PluginMenu", "Icon", Value::string(""), 0},
        {"PluginMenu", "Title", Value::string(""), 0},
        {"PluginMenu", "Visible", Value::boolean(false), NoScriptRead | NoScriptWrite},
        {"PluginDragEvent", "Position", Value::vector2(0, 0), 0},
    };
    for (const DataShared& row : kDataShared) {
        ClassDef* cls = nullptr;
        for (auto& owned : r.owned) if (owned->name == row.cls) cls = owned.get();
        if (!cls) continue;
        cls->props.push_back({row.name, row.def.type, row.def, row.flags, nullptr});
    }
    // The rest family's members whose names other classes carry too, the defaults Roblox's where
    // it serializes them. An InputActionLabel's text members and GetTextBoundsParams.Size are not
    // verified here (the type's zero). PoseBase's easings are Pose's own.
    struct RestShared { const char* cls; const char* name; Value def; unsigned flags = 0; const char* enumName = nullptr; };
    const RestShared kRestShared[] = {
        {"ControllerBase", "Active", Value::boolean(false), ReadOnly},
        {"IKControl", "Enabled", Value::boolean(true)},
        {"IKControl", "Priority", Value::number(0)},
        {"IKControl", "Target", Value::instance(0)},
        {"IKControl", "Type", Value::enumItem("Transform", 0), 0, "IKControlType"},
        {"LineForce", "Magnitude", Value::number(1000)},
        {"InputContext", "Enabled", Value::boolean(true)},
        {"InputContext", "Priority", Value::number(1000)},
        {"InputContext", "Sink", Value::boolean(false)},
        {"InputAction", "Enabled", Value::boolean(true)},
        {"InputAction", "Type", Value::enumItem("Bool", 0), 0, "InputActionType"},
        {"InputBinding", "DisplayName", Value::string("")},
        {"InputBinding", "Down", Value::enumItem("Unknown", 0), 0, "KeyCode"},
        {"InputBinding", "Up", Value::enumItem("Unknown", 0), 0, "KeyCode"},
        {"InputBinding", "Left", Value::enumItem("Unknown", 0), 0, "KeyCode"},
        {"InputBinding", "Right", Value::enumItem("Unknown", 0), 0, "KeyCode"},
        {"InputBinding", "KeyCode", Value::enumItem("Unknown", 0), 0, "KeyCode"},
        {"InputBinding", "Scale", Value::number(1)},
        {"InputBinding", "Type", Value::enumItem("Automatic", 0), 0, "InputBindingType"},
        {"InputActionLabel", "FontFace", Value::font("rbxasset://fonts/families/SourceSansPro.json", 400, false)},
        {"InputActionLabel", "ImageColor3", Value::color3(1, 1, 1)},
        {"InputActionLabel", "ImageTransparency", Value::number(0)},
        {"InputActionLabel", "TextColor3", Value::color3(0, 0, 0)},
        {"InputActionLabel", "TextSize", Value::number(0)},
        {"InputActionLabel", "TextTransparency", Value::number(0)},
        {"InputActionLabel", "TextWrapped", Value::boolean(false)},
        {"InputActionLabel", "TextXAlignment", Value::enumItem("Left", 0), 0, "TextXAlignment"},
        {"InputActionLabel", "TextYAlignment", Value::enumItem("Top", 0), 0, "TextYAlignment"},
        {"AccessoryDescription", "Instance", Value::instance(0)},
        {"AccessoryDescription", "Position", Value::vector3(0, 0, 0)},
        {"AccessoryDescription", "Rotation", Value::vector3(0, 0, 0)},
        {"AccessoryDescription", "Scale", Value::vector3(1, 1, 1)},
        {"BodyPartDescription", "Color", Value::color3(0, 0, 0)},
        {"BodyPartDescription", "Instance", Value::instance(0)},
        {"MakeupDescription", "Instance", Value::instance(0)},
        {"PoseBase", "EasingDirection", Value::enumItem("Out", 1), 0, "PoseEasingDirection"},
        {"PoseBase", "EasingStyle", Value::enumItem("Linear", 0), 0, "PoseEasingStyle"},
        {"HapticEffect", "Looped", Value::boolean(false)},
        {"HapticEffect", "Position", Value::vector3(0, 0, 0)},
        {"HapticEffect", "Type", Value::enumItem("UIClick", 2), 0, "HapticEffectType"},
        {"GetTextBoundsParams", "Size", Value::number(0)},
        {"GetTextBoundsParams", "Text", Value::string("")},
        {"StyleDerive", "Priority", Value::number(0)},
        {"StyleRule", "Priority", Value::number(0)},
        {"VideoPlayer", "PlaybackSpeed", Value::number(1)},
        {"VideoPlayer", "TimePosition", Value::number(0)},
        {"VideoPlayer", "Volume", Value::number(1)},
        {"VideoDisplay", "ResampleMode", Value::enumItem("Default", 0), 0, "ResamplerMode"},
        {"VideoDisplay", "ScaleType", Value::enumItem("Stretch", 0), 0, "ScaleType"},
        {"ScreenshotHud", "Visible", Value::boolean(false)},
        {"StudioScreenshotCapture", "Position", Value::vector2(0, 0), ReadOnly | PluginWrite},
        {"AdPortal", "Status", Value::enumItem("Inactive", 0), ReadOnly | NoReplicate, "AdUnitStatus"},
        {"PackageLink", "Status", Value::string(""), ReadOnly | NoReplicate | NoScriptRead | NoScriptWrite},
        {"ConfigSnapshot", "Error", Value::enumItem("None", 0), ReadOnly, "ConfigSnapshotErrorState"},
        {"File", "Size", Value::number(0), PluginWrite},
        {"ProceduralModel", "Size", Value::vector3(12, 12, 12)},
        {"SelectionLasso", "Humanoid", Value::instance(0)},
        {"TextChannelWindow", "Target", Value::instance(0)},
    };
    for (const RestShared& row : kRestShared) {
        ClassDef* cls = nullptr;
        for (auto& owned : r.owned) if (owned->name == row.cls) cls = owned.get();
        if (!cls) continue;
        cls->props.push_back({row.name, row.def.type, row.def, row.flags, row.enumName ? findEnum(row.enumName) : nullptr});
    }
    for (const char* surface : {"SurfaceAppearance", "MaterialVariant"}) {
        for (auto& owned : r.owned) if (owned->name == surface) {
            owned->props.push_back(P("EmissiveStrength", Value::number(1)));
            owned->props.push_back(P("EmissiveTint", Value::color3(1, 1, 1)));
        }
    }
    // ColorMap / ColorMapContent supersede Texture / TextureContent. Aliases, not storage:
    // neither replicates or serializes, and both read and write the value under the older name.
    for (auto& owned : r.owned) if (owned->name == "Decal") {
        PropDef colorMap = P("ColorMap", Value::string(""));
        colorMap.aliasOf = "Texture";
        owned->props.push_back(std::move(colorMap));
        PropDef colorMapContent{"ColorMapContent", Value::Content, Value::content(Value::ContentNone), 0, nullptr};
        colorMapContent.aliasOf = "TextureContent";
        colorMapContent.contentObjects = "EditableImage";
        owned->props.push_back(std::move(colorMapContent));
    }
}

Registry& registry() {
    static Registry* r = [] {
        auto* reg = new Registry();
        build(*reg);
        return reg;
    }();
    return *r;
}

} // namespace

const ClassDef* findClass(const std::string& name) { return registry().find(name); }
const std::vector<const ClassDef*>& allClasses() { return registry().all; }

// ---- Instance ----------------------------------------------------------------------
Instance::Instance(DataModel* dm, int64_t id, const ClassDef* cls) : dm_(dm), id_(id), cls_(cls), name_(cls->name) {}

Instance::~Instance() {
    if (binding && bindingFree) bindingFree(binding);
    if (dm_ && !dm_->dying_ && dm_->onReleased) dm_->onReleased(id_);
    // An instance nobody parented and nobody references leaves the id table, so the mirror hears of it.
    if (!destroyed_ && dm_ && !dm_->dying_) {
        Change c; c.kind = Change::Destroy; c.id = id_;
        dm_->record(std::move(c));
        dm_->forget(id_);
    }
}

void Instance::holdContent(const std::string& prop, const Value& v) {
    Instance* o = v.type == Value::Content && (v.n == Value::ContentObject || v.n == Value::ContentOpaque) ? dm_->find(v.ref) : nullptr;
    if (o) heldObjects_[prop] = o->shared_from_this(); else heldObjects_.erase(prop);
}

Value Instance::get(const std::string& prop) const {
    if (prop == "Name") return Value::string(name_);
    if (prop == "ClassName") return Value::string(cls_->name);
    if (prop == "Parent") return Value::instance(parent_ ? parent_->id() : 0);
    auto it = props_.find(prop);
    if (it != props_.end()) return it->second;
    const PropDef* d = cls_->findProp(prop);
    if (d && !d->aliasOf.empty()) return get(d->aliasOf);
    return d ? d->def : Value::nil();
}

bool Instance::set(const std::string& prop, const Value& in, std::string* err) { return setImpl(prop, in, err, false); }

bool Instance::setImpl(const std::string& prop, const Value& in, std::string* err, bool host) {
    if (prop == "Parent") {
        Instance* p = nullptr;
        if (in.type == Value::Ref && in.ref != 0) {
            p = dm_->find(in.ref);
            if (!p) { if (err) *err = "Parent: unknown instance"; return false; }
        } else if (in.type != Value::Nil && in.type != Value::Ref) {
            if (err) *err = "invalid argument #3 to 'Parent' (Instance expected, got " + std::string(Value::typeName(in.type)) + ")";
            return false;
        }
        return setParent(p, err);
    }
    if (prop == "ClassName") { if (err) *err = "ClassName cannot be assigned to"; return false; }
    const PropDef* d = cls_->findProp(prop);
    if (!d) {
        if (err) *err = prop + " is not a valid member of " + cls_->name + " \"" + fullName() + "\"";
        return false;
    }
    if ((d->flags & ReadOnly) && !host) { if (err) *err = "Unable to assign property " + prop + ". Property is read only"; return false; }
    if (!d->aliasOf.empty()) return setImpl(d->aliasOf, in, err, host);
    Value v = in;
    // Coercions Roblox allows: an enum item's name or value, never number<->bool.
    if (d->type == Value::Enum) {
        if (v.type == Value::String) {
            const EnumItem* i = d->enumType ? d->enumType->find(v.s) : nullptr;
            if (!i) { if (err) *err = "Unable to assign property " + prop + ". Invalid value for enum " + (d->enumType ? d->enumType->name : "?"); return false; }
            v = Value::enumItem(i->name, i->value);
        } else if (v.type == Value::Number) {
            const EnumItem* i = d->enumType ? d->enumType->findValue((int)v.n) : nullptr;
            if (!i) { if (err) *err = "Unable to assign property " + prop + ". Invalid value for enum " + (d->enumType ? d->enumType->name : "?"); return false; }
            v = Value::enumItem(i->name, i->value);
        }
    } else if (d->type == Value::Ref && v.type == Value::Nil) {
        v = Value::instance(0);
    } else if (d->type == Value::Number && v.type == Value::Bool) {
        // not coerced; Roblox rejects it too
    }
    if (v.type != d->type && !(d->type == Value::PhysProps && v.type == Value::Nil)) {   // CustomPhysicalProperties: nil is the material's own
        if (err) *err = "invalid argument #3 to '" + prop + "' (" + Value::typeName(d->type) + " expected, got " + Value::typeName(v.type) + ")";
        return false;
    }
    if (d->type == Value::Content && v.n == Value::ContentOpaque) {
        const Instance* holder = dm_->find(v.ref);
        const std::string kind = holder ? holder->get("Kind").s : "";
        const bool fits = d->contentObjects && ((kind == "Image" && !std::strcmp(d->contentObjects, "EditableImage")) || (kind == "Mesh" && !std::strcmp(d->contentObjects, "EditableMesh")));
        if (!fits) { if (err) *err = "Unable to assign property " + prop + ". This content cannot be used here"; return false; }
    }
    if (d->type == Value::Content && v.n == Value::ContentObject) {
        const Instance* obj = dm_->find(v.ref);
        if (!d->contentObjects) { if (err) *err = "Unable to assign property " + prop + ". Only asset URIs are supported"; return false; }
        if (obj && !obj->isA(d->contentObjects)) { if (err) *err = "Unable to assign property " + prop + ". Expected an " + d->contentObjects + ", got a " + obj->className(); return false; }
    }
    if (!d->twin.empty()) {
        // A Content property and its string twin are one value: writing the string means
        // Content.fromUri(it), writing a Content leaves its uri in the string, "" for an object.
        const PropDef* td = cls_->findProp(d->twin);
        if (!td) { if (err) *err = prop + ": its twin " + d->twin + " is missing"; return false; }
        Value tv = d->type == Value::Content ? Value::string(v.n == Value::ContentUri ? v.s : "") : Value::contentUri(v.s);
        bool mine = get(prop) != v, theirs = get(d->twin) != tv;
        if (!mine && !theirs) return true;
        if (v == d->def) props_.erase(prop); else props_[prop] = v;
        if (tv == td->def) props_.erase(d->twin); else props_[d->twin] = tv;
        holdContent(d->type == Value::Content ? prop : d->twin, d->type == Value::Content ? v : tv);
        // The string first, then the Content: whoever applies the two in order ends on the Content.
        const std::string str = d->type == Value::Content ? d->twin : prop;
        const std::string con = d->type == Value::Content ? prop : d->twin;
        for (const std::string* n : {&str, &con}) {
            Change c; c.kind = Change::Property; c.id = id_; c.name = *n; c.value = get(*n);
            dm_->record(std::move(c));
        }
        if (dm_->onPropertyChanged) {
            dm_->onPropertyChanged(*this, str);
            dm_->onPropertyChanged(*this, con);
            // and the other names the value goes by (Decal's ColorMap and ColorMapContent)
            std::vector<const PropDef*> all;
            cls_->collectProps(all);
            for (const PropDef* a : all)
                if (!a->aliasOf.empty() && (a->aliasOf == str || a->aliasOf == con)) dm_->onPropertyChanged(*this, a->name);
        }
        return true;
    }
    if (prop == "Name") {
        if (name_ == v.s) return true;
        name_ = v.s;
    } else {
        Value old = get(prop);
        if (old == v) return true;
        if (v == d->def) props_.erase(prop); else props_[prop] = v;
        if (d->type == Value::Content) holdContent(prop, v);
    }
    Change c; c.kind = Change::Property; c.id = id_; c.name = prop; c.value = v;
    dm_->record(std::move(c));
    if (dm_->onPropertyChanged) dm_->onPropertyChanged(*this, prop);
    return true;
}

void Instance::touch(const std::string& prop) {
    Change c; c.kind = Change::Property; c.id = id_; c.name = prop; c.value = get(prop);
    dm_->record(std::move(c));
    if (dm_->onPropertyChanged) dm_->onPropertyChanged(*this, prop);
}

void Instance::detachFromParent() {
    if (!parent_) return;
    auto& sib = parent_->children_;
    for (size_t i = 0; i < sib.size(); ++i) {
        if (sib[i].get() == this) { sib.erase(sib.begin() + i); break; }
    }
    parent_ = nullptr;
}

bool Instance::setParent(Instance* p, std::string* err) {
    if (p == parent_) return true;
    if (parentLocked_) {
        if (err) *err = "The Parent property of " + name_ + " is locked, current parent: NULL, new parent " + (p ? p->name() : "NULL");
        return false;
    }
    if (p) {
        if (p->destroyed_) { if (err) *err = "Cannot parent to a destroyed instance"; return false; }
        if (p == this || p->isDescendantOf(this)) {
            if (err) *err = "Attempt to set parent of " + name_ + " to " + p->name() + " would result in circular reference";
            return false;
        }
        if (&p->dataModel() != dm_) { if (err) *err = "Instance belongs to another DataModel"; return false; }
    }
    Ptr keepAlive = shared_from_this();
    Instance* old = parent_;
    detachFromParent();
    parent_ = p;
    if (p) p->children_.push_back(keepAlive);
    Change c; c.kind = Change::Parent; c.id = id_; c.parent = p ? p->id() : kNoParent;
    dm_->record(std::move(c));
    if (dm_->onParentChanged) dm_->onParentChanged(*this, old, p);
    return true;
}

Instance* Instance::findFirstChild(const std::string& n, bool recursive) const {
    for (auto& c : children_) if (c->name_ == n) return c.get();
    if (recursive)
        for (auto& c : children_) if (Instance* r = c->findFirstChild(n, true)) return r;
    return nullptr;
}
Instance* Instance::findFirstChildOfClass(const std::string& className) const {
    for (auto& c : children_) if (c->className() == className) return c.get();
    return nullptr;
}
Instance* Instance::findFirstChildWhichIsA(const std::string& className) const {
    for (auto& c : children_) if (c->isA(className)) return c.get();
    return nullptr;
}
Instance* Instance::findFirstAncestorOfClass(const std::string& className) const {
    for (Instance* p = parent_; p; p = p->parent_) if (p->className() == className) return p;
    return nullptr;
}
std::vector<Instance*> Instance::getDescendants() const {
    std::vector<Instance*> out;
    std::vector<const Instance*> stack{this};
    // Pre-order like Roblox.
    std::function<void(const Instance*)> rec = [&](const Instance* i) {
        for (auto& c : i->children_) { out.push_back(c.get()); rec(c.get()); }
    };
    rec(this);
    return out;
}
bool Instance::isDescendantOf(const Instance* a) const {
    for (Instance* p = parent_; p; p = p->parent_) if (p == a) return true;
    return false;
}
std::string Instance::fullName() const {
    // Roblox omits the DataModel: "Workspace.Model.Part".
    if (!parent_ || parent_ == dm_->root()) return name_;
    return parent_->fullName() + "." + name_;
}

void Instance::destroy() {
    if (destroyed_) return;
    Ptr keepAlive = shared_from_this();
    if (dm_->onDestroying) dm_->onDestroying(*this);
    setParent(nullptr);
    destroyed_ = true;
    parentLocked_ = true;
    // Copied: destroy() detaches each child from children_ as it goes.
    std::vector<Ptr> kids = children_;
    for (auto& k : kids) k->destroy();
    Change c; c.kind = Change::Destroy; c.id = id_;
    dm_->record(std::move(c));
    dm_->forget(id_);
}

Instance::Ptr Instance::cloneTree(std::unordered_map<int64_t, int64_t>& made) const {
    if (!get("Archivable").b) return nullptr;
    // A PartOperation cannot be Instance.new'd, but Roblox clones an existing one, so
    // createInternal here. Player, Terrain, Tween and the rest keep their rule.
    Ptr copy = cls_->isA("PartOperation") ? dm_->createInternal(cls_->name)
                                          : dm_->create(cls_->name == "DataModel" ? "Folder" : cls_->name);
    if (!copy) return nullptr;
    made[id_] = copy->id();
    copy->setName(name_);
    for (auto& [k, v] : props_) copy->set(k, v);
    copy->attrs_ = attrs_;
    copy->tags_ = tags_;
    for (auto& k : children_) {
        Ptr kc = k->cloneTree(made);
        if (kc) kc->setParent(copy.get());
    }
    return copy;
}

// Roblox's clone rule: a Ref pointing inside the copied tree is repointed at the copy, a Ref
// leaving it left alone. Without this pass a clone's PrimaryPart and joints name the original's parts.
Instance::Ptr Instance::clone() const {
    std::unordered_map<int64_t, int64_t> made;
    Ptr copy = cloneTree(made);
    if (!copy) return nullptr;
    std::vector<Instance*> all = copy->getDescendants();
    all.push_back(copy.get());
    for (Instance* i : all) {
        for (auto& [k, v] : i->props_) {
            if (v.type != Value::Ref || v.ref == 0) continue;
            auto it = made.find(v.ref);
            if (it != made.end() && it->second != v.ref) i->set(k, Value::instance(it->second));
        }
    }
    return copy;
}

Value Instance::getAttribute(const std::string& n) const {
    auto it = attrs_.find(n);
    return it == attrs_.end() ? Value::nil() : it->second;
}
bool Instance::setAttribute(const std::string& n, const Value& v) {
    if (v.type == Value::Nil) { if (!attrs_.erase(n)) return true; }
    else {
        auto it = attrs_.find(n);
        if (it != attrs_.end() && it->second == v) return true;
        attrs_[n] = v;
    }
    Change c; c.kind = Change::Property; c.id = id_; c.name = "@" + n; c.value = v;   // attributes ride the property channel
    dm_->record(std::move(c));
    if (dm_->onPropertyChanged) dm_->onPropertyChanged(*this, "@" + n);
    return true;
}

bool Instance::hasTag(const std::string& t) const { return std::find(tags_.begin(), tags_.end(), t) != tags_.end(); }
void Instance::addTag(const std::string& t) {
    if (hasTag(t)) return;
    tags_.push_back(t);
    Change c; c.kind = Change::Property; c.id = id_; c.name = "#" + t; c.value = Value::boolean(true);
    dm_->record(std::move(c));
    if (dm_->onPropertyChanged) dm_->onPropertyChanged(*this, "#" + t);
}
void Instance::removeTag(const std::string& t) {
    auto it = std::find(tags_.begin(), tags_.end(), t);
    if (it == tags_.end()) return;
    tags_.erase(it);
    Change c; c.kind = Change::Property; c.id = id_; c.name = "#" + t; c.value = Value::boolean(false);
    dm_->record(std::move(c));
    if (dm_->onPropertyChanged) dm_->onPropertyChanged(*this, "#" + t);
}

// ---- DataModel ---------------------------------------------------------------------
int64_t DataModel::serviceId(const std::string& className) {
    int64_t n = 1;
    for (const ClassDef* c : allClasses()) {
        if (!c->service) continue;
        if (c->name == className) return n;
        n++;
    }
    return 0;
}

// A GUID for one running server, in Roblox's shape: 8-4-4-4-12 hex, version 4. Nothing about the
// port, address or place goes into it: two servers of one place on a machine are two jobs.
static std::string makeJobId() {
    std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count()
                        ^ std::random_device{}());
    static const char* hex = "0123456789abcdef";
    std::string out;
    for (int i = 0; i < 32; i++) {
        if (i == 8 || i == 12 || i == 16 || i == 20) out += '-';
        int nib = (int)(rng() & 15);
        if (i == 12) nib = 4;                       // the version
        if (i == 16) nib = 8 + (nib & 3);           // the variant
        out += hex[nib];
    }
    return out;
}

DataModel::DataModel(bool isServer) : isServer_(isServer), nextId_(isServer ? kFirstServerId : -1) {
    root_ = Instance::Ptr(new Instance(this, 0, findClass("DataModel")));
    root_->name_ = "Game";
    byId_[0] = root_.get();
    // The server mints it; a client gets it by replication, or every client would hold a different id.
    if (isServer) root_->setInternal("JobId", Value::string(makeJobId()));
}

DataModel::~DataModel() {
    dying_ = true;
    // Break parent pointers bottom-up so no destructor sees a dangling one.
    std::vector<Instance*> all = root_->getDescendants();
    for (auto it = all.rbegin(); it != all.rend(); ++it) { (*it)->parent_ = nullptr; (*it)->children_.clear(); }
    root_->children_.clear();
}

Instance* DataModel::getService(const std::string& className) {
    const ClassDef* c = findClass(className);
    if (!c || !c->service) return nullptr;
    if (Instance* s = root_->findFirstChildOfClass(className)) return s;
    Instance::Ptr s = createWithId(serviceId(className), className);
    s->setParent(root_.get());
    return s.get();
}

Instance::Ptr DataModel::create(const std::string& className, Instance* parent, std::string* err) {
    const ClassDef* c = findClass(className);
    if (!c) { if (err) *err = "Unable to create an Instance of type \"" + className + "\""; return nullptr; }
    if (!c->creatable && !(c->service && parent == nullptr)) {
        if (err) *err = "Unable to create an Instance of type \"" + className + "\"";
        return nullptr;
    }
    int64_t id = nextId_;
    nextId_ += isServer_ ? 1 : -1;
    Instance::Ptr i = createWithId(id, className);
    if (parent) i->setParent(parent, err);
    return i;
}

Instance::Ptr DataModel::createInternal(const std::string& className, Instance* parent) {
    if (!findClass(className)) return nullptr;
    int64_t id = nextId_;
    nextId_ += isServer_ ? 1 : -1;
    Instance::Ptr i = createWithId(id, className);
    if (parent) i->setParent(parent);
    return i;
}

Instance::Ptr DataModel::createWithId(int64_t id, const std::string& className) {
    const ClassDef* c = findClass(className);
    if (!c) return nullptr;
    Instance::Ptr i(new Instance(this, id, c));
    byId_[id] = i.get();
    Change ch; ch.kind = Change::Create; ch.id = id; ch.className = className; ch.name = i->name_;
    record(std::move(ch));
    return i;
}

static void snapshotInto(const Instance& i, std::vector<Change>& out) {
    Change c; c.kind = Change::Create; c.id = i.id(); c.className = i.className(); c.name = i.name();
    c.parent = i.parent() ? i.parent()->id() : kNoParent;
    out.push_back(c);
    if (i.parent()) { Change p; p.kind = Change::Parent; p.id = i.id(); p.parent = i.parent()->id(); out.push_back(p); }
    std::vector<const PropDef*> props;
    i.cls().collectProps(props);
    for (const PropDef* d : props) {
        if (d->name == "Name" || d->name == "Parent" || d->name == "ClassName") continue;
        Value v = i.get(d->name);
        if (v == d->def) continue;
        Change pc; pc.kind = Change::Property; pc.id = i.id(); pc.name = d->name; pc.value = v;
        out.push_back(std::move(pc));
    }
    for (auto& [n, v] : i.attributes()) { Change a; a.kind = Change::Property; a.id = i.id(); a.name = "@" + n; a.value = v; out.push_back(std::move(a)); }
    for (auto& t : i.tags()) { Change a; a.kind = Change::Property; a.id = i.id(); a.name = "#" + t; a.value = Value::boolean(true); out.push_back(std::move(a)); }
    for (auto& k : i.children()) snapshotInto(*k, out);
}

std::vector<Change> DataModel::snapshot(const Instance* from) const {
    std::vector<Change> out;
    if (!from) { for (auto& k : root_->children()) snapshotInto(*k, out); }
    else snapshotInto(*from, out);
    return out;
}

Instance* DataModel::find(int64_t id) const {
    auto it = byId_.find(id);
    return it == byId_.end() ? nullptr : it->second;
}

void DataModel::record(Change c) { if (quiet_) return; c.fromHost = hostWriting_ || applyingRemote_; c.remote = applyingRemote_; c.script = currentScript_; changes_.push_back(std::move(c)); }

bool DataModel::apply(const Change& c, std::string* err) {
    setApplyingRemote(true);
    struct Reset { DataModel* d; ~Reset() { d->setApplyingRemote(false); } } reset{this};
    switch (c.kind) {
    case Change::Create: {
        if (Instance* have = find(c.id)) {
            // A service both sides make, or a subtree resent whole: keep it, take the name.
            if (have->className() != c.className) { if (err) *err = "id " + std::to_string(c.id) + " is a " + have->className() + ", not a " + c.className; return false; }
            if (have->name() != c.name) have->setName(c.name);
            return true;
        }
        Instance::Ptr i = createWithId(c.id, c.className);
        if (!i) { if (err) *err = "unknown class " + c.className; return false; }
        i->setName(c.name);
        // The Create carries no parent; a Parent change follows it, and nothing else holds it until then.
        orphans_[c.id] = i;
        return true;
    }
    case Change::Destroy: {
        Instance* i = find(c.id);
        if (!i) return true;
        i->destroy();
        orphans_.erase(c.id);
        return true;
    }
    case Change::Parent: {
        Instance* i = find(c.id);
        if (!i) { if (err) *err = "unknown instance"; return false; }
        Instance* p = c.parent == kNoParent ? nullptr : find(c.parent);
        if (c.parent != kNoParent && !p) { if (err) *err = "unknown parent"; return false; }
        if (!i->setParent(p, err)) return false;
        if (p) orphans_.erase(c.id); else orphans_[c.id] = i->shared_from_this();
        return true;
    }
    case Change::Property: {
        Instance* i = find(c.id);
        if (!i) { if (err) *err = "unknown instance"; return false; }
        if (!c.name.empty() && c.name[0] == '@') return i->setAttribute(c.name.substr(1), c.value);
        if (!c.name.empty() && c.name[0] == '#') { if (c.value.b) i->addTag(c.name.substr(1)); else i->removeTag(c.name.substr(1)); return true; }
        return i->setInternal(c.name, c.value, err);   // the other side is authoritative, ReadOnly included
    }
    }
    return false;
}

// ---- placement rules ---------------------------------------------------------------
static const Instance* topService(const Instance& i) {
    const Instance* top = &i;
    const Instance* root = i.dataModel().root();
    while (top->parent() && top->parent() != root) top = top->parent();
    return top->parent() == root ? top : nullptr;
}

bool serverScriptRunsUnder(const Instance& container) {
    const Instance* top = topService(container);
    if (!top) return false;
    const std::string& c = top->className();
    return c == "ServerScriptService" || c == "Workspace" || c == "Players";   // Players: a Script in a Backpack / Tool
}

bool clientScriptRunsUnder(const Instance& container, const Instance* localPlayer) {
    const Instance* top = topService(container);
    if (!top) return false;
    if (top->className() == "ReplicatedFirst") return true;
    if (localPlayer) {
        for (const Instance* p = &container; p; p = p->parent()) {
            if (p->parent() == localPlayer) {
                const std::string& c = p->className();
                if (c == "PlayerScripts" || c == "PlayerGui" || c == "Backpack") return true;
            }
        }
        Value ch = localPlayer->get("Character");
        if (ch.ref != 0) {
            const Instance* character = localPlayer->dataModel().find(ch.ref);
            if (character && (&container == character || container.isDescendantOf(character))) return true;
        }
    }
    return false;
}

bool replicates(const Instance& svc) {
    const std::string& c = svc.className();
    // ServerStorage and ServerScriptService are the server's. UserInputService is the client's:
    // replicated, the server's copy would overwrite a LocalScript's MouseBehavior every frame.
    return !(c == "ServerStorage" || c == "ServerScriptService" || c == "UserInputService");
}

const MaterialPhysics& materialPhysics(const std::string& material) {
    static const std::unordered_map<std::string, MaterialPhysics> kTable = {
        {"Plastic", {0.7, 0.3, 0.5}},        {"SmoothPlastic", {0.7, 0.2, 0.5}},
        {"Neon", {0.7, 0.3, 0.2}},           {"Wood", {0.35, 0.48, 0.2}},
        {"WoodPlanks", {0.35, 0.48, 0.2}},   {"Marble", {2.56, 0.2, 0.17}},
        {"Slate", {2.691, 0.4, 0.2}},        {"Concrete", {2.403, 0.7, 0.2}},
        {"Granite", {2.691, 0.4, 0.2}},      {"Brick", {1.922, 0.8, 0.15}},
        {"Pebble", {2.403, 0.4, 0.17}},      {"Cobblestone", {2.691, 0.5, 0.17}},
        {"CorrodedMetal", {7.85, 0.7, 0.2}}, {"DiamondPlate", {7.85, 0.35, 0.25}},
        {"Foil", {2.7, 0.4, 0.25}},          {"Metal", {7.85, 0.4, 0.25}},
        {"Grass", {0.9, 0.4, 0.1}},          {"Sand", {1.6, 0.5, 0.05}},
        {"Fabric", {0.7, 0.35, 0.05}},       {"Ice", {0.92, 0.02, 0.15}},
        {"Glass", {2.4, 0.25, 0.2}},         {"ForceField", {0.7, 0.25, 0.5}},
    };
    size_t dot = material.rfind('.');   // a Value's enum name may be "Material.Ice"
    auto it = kTable.find(dot == std::string::npos ? material : material.substr(dot + 1));
    return it != kTable.end() ? it->second : kTable.find("Plastic")->second;
}

MaterialPhysics physicsOf(const std::string& material, const Value& custom) {
    if (custom.type != Value::PhysProps) return materialPhysics(material);
    return {custom.u[0], custom.u[1], custom.u[2]};
}

} // namespace pulseblockz::rbx
