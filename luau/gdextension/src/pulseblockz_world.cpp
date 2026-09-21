#include "pulseblockz_world.h"
#include "pulse_gradient.gen.h"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/http_client.hpp>
#include <godot_cpp/classes/http_request.hpp>
#include <godot_cpp/classes/ip.hpp>
#include <godot_cpp/classes/physics_body3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/marshalls.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/system_font.hpp>
#include <godot_cpp/classes/font_file.hpp>
#include <cctype>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/kinematic_collision3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/torus_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/prism_mesh.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/importer_mesh_instance3d.hpp>
#include <godot_cpp/classes/importer_mesh.hpp>
#include <godot_cpp/classes/gltf_document.hpp>
#include <godot_cpp/classes/gltf_state.hpp>
#include <sstream>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include <godot_cpp/classes/convex_polygon_shape3d.hpp>
#include <godot_cpp/classes/mesh_convex_decomposition_settings.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/spot_light3d.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/procedural_sky_material.hpp>
#include <godot_cpp/classes/cubemap.hpp>
#include <godot_cpp/classes/camera_attributes_practical.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/gpu_particles3d.hpp>
#include <godot_cpp/classes/hinge_joint3d.hpp>
#include <godot_cpp/classes/pin_joint3d.hpp>
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/physics_material.hpp>
#include <godot_cpp/classes/physics_direct_body_state3d.hpp>
#include <godot_cpp/classes/cone_twist_joint3d.hpp>
#include <godot_cpp/classes/generic6_dof_joint3d.hpp>
#include <godot_cpp/classes/particle_process_material.hpp>
#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/classes/curve_texture.hpp>
#include <godot_cpp/classes/curve_xyz_texture.hpp>
#include <tuple>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/style_box_texture.hpp>
#include <godot_cpp/classes/style_box_empty.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/audio_effect.hpp>
#include <godot_cpp/classes/audio_effect_reverb.hpp>
#include <godot_cpp/classes/audio_effect_filter.hpp>
#include <godot_cpp/classes/audio_effect_low_pass_filter.hpp>
#include <godot_cpp/classes/audio_effect_high_pass_filter.hpp>
#include <godot_cpp/classes/audio_effect_band_pass_filter.hpp>
#include <godot_cpp/classes/audio_effect_notch_filter.hpp>
#include <godot_cpp/classes/audio_effect_low_shelf_filter.hpp>
#include <godot_cpp/classes/audio_effect_high_shelf_filter.hpp>
#include <godot_cpp/classes/audio_effect_eq21.hpp>
#include <godot_cpp/classes/audio_effect_eq6.hpp>
#include <godot_cpp/classes/audio_effect_delay.hpp>
#include <godot_cpp/classes/audio_effect_distortion.hpp>
#include <godot_cpp/classes/audio_effect_chorus.hpp>
#include <godot_cpp/classes/audio_effect_compressor.hpp>
#include <godot_cpp/classes/audio_effect_pitch_shift.hpp>
#include <godot_cpp/classes/audio_effect_amplify.hpp>
#include <godot_cpp/classes/audio_effect_hard_limiter.hpp>
#include "pulseblockz_tremolo.h"
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream_ogg_vorbis.hpp>
#include <godot_cpp/classes/audio_stream_mp3.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <cstring>
#include <cctype>
#include <cmath>
#include <limits>
#include <functional>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/csg_combiner3d.hpp>
#include <godot_cpp/classes/csg_mesh3d.hpp>
#include <godot_cpp/classes/csg_shape3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "rbx_internal.h"
#include "rbx_audio.h"
#include "eip712.h"
#include "meshoptimizer.h"
#include "draco/compression/decode.h"
#include "draco/mesh/mesh.h"
#include "version.gen.h"
#include <random>

using namespace godot;

using namespace pulseblockz::rbx;

// ---- conversions -------------------------------------------------------------------
static Vector3 toGd(Vec3 v) { return Vector3(v.x, v.y, v.z); }
static Vec3 fromGd(Vector3 v) { return {(float)v.x, (float)v.y, (float)v.z}; }

// Roblox and Godot share handedness and axes (Y up, -Z forward): a CFrame's Right/Up/Back
// columns are a Basis's columns.
static Transform3D toTransform(Vec3 pos, Vec3 orientDeg) {
    CFrameV c = cframeFromPosOrient(pos, orientDeg);
    Basis b(toGd(c.col(0)), toGd(c.col(1)), toGd(c.col(2)));
    return Transform3D(b, toGd(pos));
}
static void fromTransform(const Transform3D& t, Vec3& pos, Vec3& orientDeg) {
    CFrameV c;
    c.p = fromGd(t.origin);
    for (int col = 0; col < 3; col++) {
        Vector3 a = t.basis.get_column(col);
        c.m[col] = (float)a.x; c.m[3 + col] = (float)a.y; c.m[6 + col] = (float)a.z;
    }
    cframeToPosOrient(c, pos, orientDeg);
}
static bool near(Vec3 a, Vec3 b, float eps) {
    return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.z - b.z) < eps;
}
static std::string toStd(const String& s) { return std::string(s.utf8().get_data()); }
static double lerpAngle(double from, double to, double t) {
    double d = std::fmod(to - from, 2 * Math_PI);
    if (d > Math_PI) d -= 2 * Math_PI;
    if (d < -Math_PI) d += 2 * Math_PI;
    return from + d * t;
}
static bool audio_class(const std::string& c);   // the Audio API's classes (sync_audio)
static std::string enumName(const Value& v) {
    size_t dot = v.s.rfind('.');
    return dot == std::string::npos ? v.s : v.s.substr(dot + 1);
}
// ---- the mirrored tree, as Variants ------------------------------------------------
// A property value as an Explorer / Properties panel wants it. An Enum becomes its item's
// name, a Ref the instance id, a UDim a Vector2 of (scale, offset), a UDim2 a Vector4 of both
// pairs, PhysicalProperties its five numbers, a sequence its raw keypoints.
static Variant toVariant(const Value& v) {
    switch (v.type) {
    case Value::Bool: return v.b;
    case Value::Number: return v.n;
    case Value::String: return String::utf8(v.s.c_str());
    case Value::Vector3: return toGd(v.v);
    case Value::Vector2: return Vector2(v.v.x, v.v.y);
    case Value::Color3: return Color(v.c.r, v.c.g, v.c.b);
    case Value::Ref: return v.ref;
    case Value::Enum: return String::utf8(enumName(v).c_str());
    case Value::UDim: return Vector2(v.u[0], v.u[1]);
    case Value::UDim2: return Vector4(v.u[0], v.u[1], v.u[2], v.u[3]);
    case Value::NumberRange: return Vector2(v.u[0], v.u[1]);
    case Value::PhysProps: {
        PackedFloat32Array a;
        a.push_back(v.u[0]); a.push_back(v.u[1]); a.push_back(v.u[2]); a.push_back(v.u[3]); a.push_back((float)v.n);
        return a;
    }
    case Value::NumberSequence:
    case Value::ColorSequence: {
        PackedFloat32Array a;
        for (float f : v.kp) a.push_back(f);
        return a;
    }
    case Value::Font: {
        Dictionary d;
        d["family"] = String::utf8(v.s.c_str());
        d["weight"] = v.n;
        d["italic"] = v.b;
        return d;
    }
    case Value::Content: {
        Dictionary d;
        d["source"] = (int)v.n;
        d["uri"] = String::utf8(v.s.c_str());
        d["object"] = v.ref;
        return d;
    }
    case Value::Nil: break;
    }
    return Variant();
}
// The same shapes back, rejected unless they match the property's declared type. A number may
// arrive as either INT or FLOAT.
static bool fromVariant(const PropDef& d, const Variant& in, Value& out) {
    Variant::Type t = in.get_type();
    auto floats = [&](int want, float* f) {
        if (t != Variant::PACKED_FLOAT32_ARRAY) return false;
        PackedFloat32Array a = in;
        if (a.size() != want) return false;
        for (int i = 0; i < want; i++) f[i] = a[i];
        return true;
    };
    switch (d.type) {
    case Value::Bool:
        if (t != Variant::BOOL) return false;
        out = Value::boolean((bool)in);
        return true;
    case Value::Number:
        if (t != Variant::INT && t != Variant::FLOAT) return false;
        out = Value::number((double)in);
        return true;
    case Value::String:
        if (t != Variant::STRING && t != Variant::STRING_NAME) return false;
        out = Value::string(toStd(in));
        return true;
    case Value::Vector3:
        if (t != Variant::VECTOR3) return false;
        out = Value::vector3(fromGd(Vector3(in)));
        return true;
    case Value::Vector2: {
        if (t != Variant::VECTOR2) return false;
        Vector2 g = in;
        out = Value::vector2((float)g.x, (float)g.y);
        return true;
    }
    case Value::Color3: {
        if (t != Variant::COLOR) return false;
        Color c = in;
        out = Value::color3(c.r, c.g, c.b);
        return true;
    }
    case Value::Ref:
        if (t != Variant::INT) return false;
        out = Value::instance((int64_t)in);
        return true;
    case Value::Enum: {
        if (t != Variant::STRING && t != Variant::STRING_NAME) return false;
        std::string item = toStd(in);
        size_t dot = item.rfind('.');
        if (dot != std::string::npos) item = item.substr(dot + 1);
        const EnumItem* e = d.enumType ? d.enumType->find(item) : nullptr;
        if (!e) return false;
        out = Value::enumItem(e->name, e->value);
        return true;
    }
    case Value::UDim: {
        if (t != Variant::VECTOR2) return false;
        Vector2 g = in;
        out = Value::udim((float)g.x, (float)g.y);
        return true;
    }
    case Value::UDim2: {
        if (t != Variant::VECTOR4) return false;
        Vector4 g = in;
        out = Value::udim2((float)g.x, (float)g.y, (float)g.z, (float)g.w);
        return true;
    }
    case Value::NumberRange: {
        if (t != Variant::VECTOR2) return false;
        Vector2 g = in;
        out = Value::numberRange((float)g.x, (float)g.y);
        return true;
    }
    case Value::PhysProps: {
        float f[5];
        if (!floats(5, f)) return false;
        out = Value::physProps(f[0], f[1], f[2], f[3], f[4]);
        return true;
    }
    case Value::NumberSequence:
    case Value::ColorSequence: {
        if (t != Variant::PACKED_FLOAT32_ARRAY) return false;
        PackedFloat32Array a = in;
        int stride = d.type == Value::NumberSequence ? 3 : 4;
        if (a.size() < stride || a.size() % stride) return false;
        std::vector<float> kp;
        for (int i = 0; i < a.size(); i++) kp.push_back(a[i]);
        out = d.type == Value::NumberSequence ? Value::numberSequence(std::move(kp)) : Value::colorSequence(std::move(kp));
        return true;
    }
    case Value::Font: {
        if (t != Variant::DICTIONARY) return false;
        Dictionary g = in;
        if (!g.has("family")) return false;
        out = Value::font(toStd(g["family"]), (int)(double)g.get("weight", 400), (bool)g.get("italic", false));
        return true;
    }
    case Value::Content: {
        // a uri string (the panel's edit), or the dictionary above
        if (t == Variant::STRING || t == Variant::STRING_NAME) { out = Value::contentUri(toStd(in)); return true; }
        if (t != Variant::DICTIONARY) return false;
        Dictionary g = in;
        int source = (int)(double)g.get("source", 0);
        int64_t object = (int64_t)g.get("object", 0);
        if (source == Value::ContentObject && object) { out = Value::content(Value::ContentObject, "", object); return true; }
        out = Value::contentUri(toStd(g.get("uri", "")));
        return true;
    }
    case Value::Nil: break;
    }
    return false;
}

// A part's pose. Sent as a stream: the next value supersedes a lost one.
static bool poseProp(const std::string& n) {
    return n == "Position" || n == "Orientation" || n == "AssemblyLinearVelocity" || n == "AssemblyAngularVelocity";
}
// What a client asserts about its OWN character and must never hear back. A jump is a velocity
// SET, so one echoed part-way up relaunches from the current height; a StateName echoed a round
// trip late overwrites the state the body has already moved on to.
static bool ownInputProp(const std::string& n) {
    return n == "Jump" || n == "MoveDirection" || n == "StateName";
}
// What a client may change about a server-owned part: how it looks, never where it is.
static bool visualProp(const std::string& n) {
    return n == "Color" || n == "Transparency" || n == "Reflectance" || n == "Material" || n == "CastShadow";
}

// ---- node --------------------------------------------------------------------------
// Godot carries zstd; the runtime does not.
static bool godotZstd(const std::string& in, size_t outLen, std::string& out) {
    PackedByteArray raw;
    raw.resize((int64_t)in.size());
    std::memcpy(raw.ptrw(), in.data(), in.size());
    PackedByteArray got = raw.decompress((int64_t)outLen, FileAccess::COMPRESSION_ZSTD);
    if ((size_t)got.size() != outLen) return false;
    out.assign((const char*)got.ptr(), (size_t)got.size());
    return true;
}

PulseBlockzWorld::PulseBlockzWorld() {
    setZstdDecoder(godotZstd);
    // Roblox's limits: a script runs until it yields, and only ten seconds without yielding
    // stops it (Studio's "Script timeout"). No step, memory or frame cap.
    opts_.budget.maxMillis = 10000.0;
    opts_.budget.maxFrameMillis = 0.0;
    opts_.budget.maxSteps = 0;
    opts_.budget.maxMemory = 0;
    opts_.isServer = true;
}

PulseBlockzWorld::~PulseBlockzWorld() {
    watchdogStop_.store(true);
    if (watchdog_.joinable()) watchdog_.join();
    for (auto& [key, job] : hullJobs_) if (job->thread.joinable()) job->thread.join();   // a running decomposition writes into this world
    // Workers must be gone before the scene nodes they may still be describing.
    if (client_) { client_->waitIdle(); client_.reset(); }
    if (server_) { server_->waitIdle(); server_.reset(); }
}

void PulseBlockzWorld::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_mode", "v"), &PulseBlockzWorld::set_mode);
    ClassDB::bind_method(D_METHOD("get_mode"), &PulseBlockzWorld::get_mode);
    ClassDB::bind_method(D_METHOD("set_player_name", "v"), &PulseBlockzWorld::set_player_name);
    ClassDB::bind_method(D_METHOD("get_player_name"), &PulseBlockzWorld::get_player_name);
    ClassDB::bind_method(D_METHOD("set_user_id", "v"), &PulseBlockzWorld::set_user_id);
    ClassDB::bind_method(D_METHOD("get_user_id"), &PulseBlockzWorld::get_user_id);
    ClassDB::bind_method(D_METHOD("set_auto_join", "v"), &PulseBlockzWorld::set_auto_join);
    ClassDB::bind_method(D_METHOD("get_auto_join"), &PulseBlockzWorld::get_auto_join);
    ClassDB::bind_method(D_METHOD("set_default_controls", "v"), &PulseBlockzWorld::set_default_controls);
    ClassDB::bind_method(D_METHOD("get_default_controls"), &PulseBlockzWorld::get_default_controls);
    ClassDB::bind_method(D_METHOD("set_default_camera", "v"), &PulseBlockzWorld::set_default_camera);
    ClassDB::bind_method(D_METHOD("get_default_camera"), &PulseBlockzWorld::get_default_camera);
    ClassDB::bind_method(D_METHOD("set_default_lighting", "v"), &PulseBlockzWorld::set_default_lighting);
    ClassDB::bind_method(D_METHOD("get_default_lighting"), &PulseBlockzWorld::get_default_lighting);
    ClassDB::bind_method(D_METHOD("set_default_animations", "v"), &PulseBlockzWorld::set_default_animations);
    ClassDB::bind_method(D_METHOD("get_default_animations"), &PulseBlockzWorld::get_default_animations);
    ClassDB::bind_method(D_METHOD("set_edit_mode", "v"), &PulseBlockzWorld::set_edit_mode);
    ClassDB::bind_method(D_METHOD("get_edit_mode"), &PulseBlockzWorld::get_edit_mode);
    ClassDB::bind_method(D_METHOD("set_gui_preview", "v"), &PulseBlockzWorld::set_gui_preview);
    ClassDB::bind_method(D_METHOD("get_gui_preview"), &PulseBlockzWorld::get_gui_preview);
    ClassDB::bind_method(D_METHOD("set_gui_preview_rect", "r"), &PulseBlockzWorld::set_gui_preview_rect);
    ClassDB::bind_method(D_METHOD("get_gui_preview_rect"), &PulseBlockzWorld::get_gui_preview_rect);
    ClassDB::bind_method(D_METHOD("gui_at", "point"), &PulseBlockzWorld::gui_at);
    ClassDB::bind_method(D_METHOD("gui_rect", "id"), &PulseBlockzWorld::gui_rect);
    ClassDB::bind_method(D_METHOD("get_class_members", "class_name"), &PulseBlockzWorld::get_class_members);
    ClassDB::bind_method(D_METHOD("import_place", "bytes"), &PulseBlockzWorld::import_place);
    ClassDB::bind_method(D_METHOD("set_track_properties", "v"), &PulseBlockzWorld::set_track_properties);
    ClassDB::bind_method(D_METHOD("get_track_properties"), &PulseBlockzWorld::get_track_properties);
    ClassDB::bind_method(D_METHOD("set_threaded", "v"), &PulseBlockzWorld::set_threaded);
    ClassDB::bind_method(D_METHOD("get_threaded"), &PulseBlockzWorld::get_threaded);
    ClassDB::bind_method(D_METHOD("set_auto_step", "v"), &PulseBlockzWorld::set_auto_step);
    ClassDB::bind_method(D_METHOD("get_auto_step"), &PulseBlockzWorld::get_auto_step);
    ClassDB::bind_method(D_METHOD("set_max_millis", "v"), &PulseBlockzWorld::set_max_millis);
    ClassDB::bind_method(D_METHOD("get_max_millis"), &PulseBlockzWorld::get_max_millis);
    ClassDB::bind_method(D_METHOD("set_max_frame_millis", "v"), &PulseBlockzWorld::set_max_frame_millis);
    ClassDB::bind_method(D_METHOD("get_max_frame_millis"), &PulseBlockzWorld::get_max_frame_millis);
    ClassDB::bind_method(D_METHOD("set_max_steps", "v"), &PulseBlockzWorld::set_max_steps);
    ClassDB::bind_method(D_METHOD("get_max_steps"), &PulseBlockzWorld::get_max_steps);
    ClassDB::bind_method(D_METHOD("set_max_memory_mb", "v"), &PulseBlockzWorld::set_max_memory_mb);
    ClassDB::bind_method(D_METHOD("get_max_memory_mb"), &PulseBlockzWorld::get_max_memory_mb);
    ClassDB::bind_method(D_METHOD("set_listen_port", "v"), &PulseBlockzWorld::set_listen_port);
    ClassDB::bind_method(D_METHOD("get_listen_port"), &PulseBlockzWorld::get_listen_port);
    ClassDB::bind_method(D_METHOD("set_data_store_path", "v"), &PulseBlockzWorld::set_data_store_path);
    ClassDB::bind_method(D_METHOD("get_data_store_path"), &PulseBlockzWorld::get_data_store_path);
    ClassDB::bind_method(D_METHOD("set_asset_root", "v"), &PulseBlockzWorld::set_asset_root);
    ClassDB::bind_method(D_METHOD("get_asset_root"), &PulseBlockzWorld::get_asset_root);
    ClassDB::bind_method(D_METHOD("set_bind_address", "v"), &PulseBlockzWorld::set_bind_address);
    ClassDB::bind_method(D_METHOD("get_bind_address"), &PulseBlockzWorld::get_bind_address);
    ClassDB::bind_method(D_METHOD("set_server_address", "v"), &PulseBlockzWorld::set_server_address);
    ClassDB::bind_method(D_METHOD("get_server_address"), &PulseBlockzWorld::get_server_address);
    ClassDB::bind_method(D_METHOD("set_server_port", "v"), &PulseBlockzWorld::set_server_port);
    ClassDB::bind_method(D_METHOD("get_server_port"), &PulseBlockzWorld::get_server_port);
    ClassDB::bind_method(D_METHOD("set_physics_layer", "v"), &PulseBlockzWorld::set_physics_layer);
    ClassDB::bind_method(D_METHOD("get_physics_layer"), &PulseBlockzWorld::get_physics_layer);
    ClassDB::bind_method(D_METHOD("listen", "port", "max_clients"), &PulseBlockzWorld::listen, DEFVAL(32));
    ClassDB::bind_method(D_METHOD("connect_to_server", "host", "port"), &PulseBlockzWorld::connect_to_server);
    ClassDB::bind_method(D_METHOD("disconnect_from_server"), &PulseBlockzWorld::disconnect_from_server);
    ClassDB::bind_method(D_METHOD("is_server_connected"), &PulseBlockzWorld::is_server_connected);
    ClassDB::bind_method(D_METHOD("get_client_count"), &PulseBlockzWorld::get_client_count);
    ClassDB::bind_method(D_METHOD("_on_peer_connected", "peer"), &PulseBlockzWorld::_on_peer_connected);
    ClassDB::bind_method(D_METHOD("_on_peer_disconnected", "peer"), &PulseBlockzWorld::_on_peer_disconnected);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Play Solo,Server,Client"), "set_mode", "get_mode");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "player_name"), "set_player_name", "get_player_name");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "user_id"), "set_user_id", "get_user_id");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_join"), "set_auto_join", "get_auto_join");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_controls"), "set_default_controls", "get_default_controls");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_camera"), "set_default_camera", "get_default_camera");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_lighting"), "set_default_lighting", "get_default_lighting");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "default_animations"), "set_default_animations", "get_default_animations");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "edit_mode"), "set_edit_mode", "get_edit_mode");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "track_properties"), "set_track_properties", "get_track_properties");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gui_preview"), "set_gui_preview", "get_gui_preview");
    ADD_PROPERTY(PropertyInfo(Variant::RECT2, "gui_preview_rect"), "set_gui_preview_rect", "get_gui_preview_rect");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "threaded"), "set_threaded", "get_threaded");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_step"), "set_auto_step", "get_auto_step");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_millis_per_call"), "set_max_millis", "get_max_millis");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_frame_millis"), "set_max_frame_millis", "get_max_frame_millis");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_steps"), "set_max_steps", "get_max_steps");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_memory_mb"), "set_max_memory_mb", "get_max_memory_mb");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "listen_port"), "set_listen_port", "get_listen_port");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "bind_address"), "set_bind_address", "get_bind_address");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "data_store_path", PROPERTY_HINT_FILE, "*.json"), "set_data_store_path", "get_data_store_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "asset_root", PROPERTY_HINT_DIR), "set_asset_root", "get_asset_root");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "server_address"), "set_server_address", "get_server_address");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "server_port"), "set_server_port", "get_server_port");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "physics_layer", PROPERTY_HINT_RANGE, "1,32"), "set_physics_layer", "get_physics_layer");
    BIND_ENUM_CONSTANT(MODE_PLAY_SOLO);
    BIND_ENUM_CONSTANT(MODE_SERVER);
    BIND_ENUM_CONSTANT(MODE_CLIENT);

    ClassDB::bind_method(D_METHOD("load_file", "rel_path", "source"), &PulseBlockzWorld::load_file);
    ClassDB::bind_method(D_METHOD("unload_file", "rel_path"), &PulseBlockzWorld::unload_file);
    ClassDB::bind_method(D_METHOD("run_chunk", "name", "source"), &PulseBlockzWorld::run_chunk);
    ClassDB::bind_method(D_METHOD("run_client_chunk", "name", "source"), &PulseBlockzWorld::run_client_chunk);
    ClassDB::bind_method(D_METHOD("answer_sign_in", "address", "signature", "server"), &PulseBlockzWorld::answer_sign_in, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("sign_in_message", "nonce"), &PulseBlockzWorld::sign_in_message);
    ClassDB::bind_method(D_METHOD("set_max_clients", "v"), &PulseBlockzWorld::set_max_clients);
    ClassDB::bind_method(D_METHOD("get_max_clients"), &PulseBlockzWorld::get_max_clients);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_clients"), "set_max_clients", "get_max_clients");   // how many clients the socket takes
    ClassDB::bind_method(D_METHOD("set_place_uri", "uri"), &PulseBlockzWorld::set_place_uri);
    ClassDB::bind_method(D_METHOD("get_place_uri"), &PulseBlockzWorld::get_place_uri);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "place_uri"), "set_place_uri", "get_place_uri");   // the published place this server runs
    ClassDB::bind_method(D_METHOD("server_place"), &PulseBlockzWorld::server_place);
    ClassDB::bind_method(D_METHOD("set_hold_for_place", "v"), &PulseBlockzWorld::set_hold_for_place);
    ClassDB::bind_method(D_METHOD("get_hold_for_place"), &PulseBlockzWorld::get_hold_for_place);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hold_for_place"), "set_hold_for_place", "get_hold_for_place");   // wait until the place is built before taking the tree
    ClassDB::bind_method(D_METHOD("ready_for_place"), &PulseBlockzWorld::ready_for_place);
    ClassDB::bind_method(D_METHOD("set_public_names", "names"), &PulseBlockzWorld::set_public_names);
    ClassDB::bind_method(D_METHOD("get_public_names"), &PulseBlockzWorld::get_public_names);
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "public_names"), "set_public_names", "get_public_names");   // the names players dial this server by
    ClassDB::bind_method(D_METHOD("plugin_add", "name", "source"), &PulseBlockzWorld::plugin_add);
    ClassDB::bind_method(D_METHOD("plugin_add_file", "file", "source"), &PulseBlockzWorld::plugin_add_file);
    ClassDB::bind_method(D_METHOD("plugin_unload_all"), &PulseBlockzWorld::plugin_unload_all);
    ClassDB::bind_method(D_METHOD("plugin_click", "button"), &PulseBlockzWorld::plugin_click);
    ClassDB::bind_method(D_METHOD("plugin_selection", "ids"), &PulseBlockzWorld::plugin_selection);
    ClassDB::bind_method(D_METHOD("plugin_setting", "plugin", "key", "json"), &PulseBlockzWorld::plugin_setting);
    ClassDB::bind_method(D_METHOD("plugin_input", "event", "ray_origin", "ray_direction"), &PulseBlockzWorld::plugin_input);
    ClassDB::bind_method(D_METHOD("is_hosting"), &PulseBlockzWorld::is_hosting);
    ClassDB::bind_method(D_METHOD("_on_cloud_done", "result", "code", "headers", "body", "num"), &PulseBlockzWorld::_on_cloud_done);
    ClassDB::bind_method(D_METHOD("set_http_enabled", "v"), &PulseBlockzWorld::set_http_enabled);
    ClassDB::bind_method(D_METHOD("get_http_enabled"), &PulseBlockzWorld::get_http_enabled);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "http_enabled"), "set_http_enabled", "get_http_enabled");   // HttpService, server-side, off until the operator says otherwise
    ClassDB::bind_method(D_METHOD("_on_http_done", "result", "code", "headers", "body", "id"), &PulseBlockzWorld::_on_http_done);
    ClassDB::bind_method(D_METHOD("asset_arrived", "uri", "path"), &PulseBlockzWorld::asset_arrived);
    ClassDB::bind_method(D_METHOD("net_stats"), &PulseBlockzWorld::net_stats);
    ClassDB::bind_method(D_METHOD("texture_tally"), &PulseBlockzWorld::texture_tally);
    ClassDB::bind_method(D_METHOD("set_cloud_fetch_base", "v"), &PulseBlockzWorld::set_cloud_fetch_base);
    ClassDB::bind_method(D_METHOD("get_cloud_fetch_base"), &PulseBlockzWorld::get_cloud_fetch_base);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "cloud_fetch_base"), "set_cloud_fetch_base", "get_cloud_fetch_base");   // where an rbxassetid is fetched from (a test points it at its own server)
    // No ADD_PROPERTY for the cookie: a property is saved into the .tscn, and this one is full
    // account access. No getter for the value either.
    ClassDB::bind_method(D_METHOD("set_cloud_cookie", "v"), &PulseBlockzWorld::set_cloud_cookie);
    ClassDB::bind_method(D_METHOD("has_cloud_cookie"), &PulseBlockzWorld::has_cloud_cookie);
    ClassDB::bind_method(D_METHOD("cloud_cookie_len"), &PulseBlockzWorld::cloud_cookie_len);
    ClassDB::bind_method(D_METHOD("collaborating"), &PulseBlockzWorld::collaborating);
    ClassDB::bind_method(D_METHOD("set_breakpoint", "script", "line", "on"), &PulseBlockzWorld::set_breakpoint);
    ClassDB::bind_method(D_METHOD("debug_continue"), &PulseBlockzWorld::debug_continue);
    ClassDB::bind_method(D_METHOD("debug_step", "kind"), &PulseBlockzWorld::debug_step);
    ClassDB::bind_method(D_METHOD("add_player", "name", "user_id"), &PulseBlockzWorld::add_player);
    ClassDB::bind_method(D_METHOD("remove_player", "name"), &PulseBlockzWorld::remove_player);
    ClassDB::bind_method(D_METHOD("flush"), &PulseBlockzWorld::flush);
    ClassDB::bind_method(D_METHOD("is_idle"), &PulseBlockzWorld::is_idle);
    ClassDB::bind_method(D_METHOD("get_stats"), &PulseBlockzWorld::get_stats);
    ClassDB::bind_method(D_METHOD("get_part_node", "id"), &PulseBlockzWorld::get_part_node);
    ClassDB::bind_method(D_METHOD("get_part_mesh", "id"), &PulseBlockzWorld::get_part_mesh);
    ClassDB::bind_method(D_METHOD("get_part_id", "node"), &PulseBlockzWorld::get_part_id);
    ClassDB::bind_method(D_METHOD("get_camera"), &PulseBlockzWorld::get_camera);
    ClassDB::bind_method(D_METHOD("get_local_player_id"), &PulseBlockzWorld::get_local_player_id);
    ClassDB::bind_method(D_METHOD("get_local_character_id"), &PulseBlockzWorld::get_local_character_id);
    ClassDB::bind_method(D_METHOD("get_local_character_node"), &PulseBlockzWorld::get_local_character_node);
    ClassDB::bind_method(D_METHOD("get_child_ids", "id", "client"), &PulseBlockzWorld::get_child_ids, DEFVAL(false));
    ClassDB::bind_method(D_METHOD("get_instance", "id"), &PulseBlockzWorld::get_instance);
    ClassDB::bind_method(D_METHOD("get_properties", "id", "hidden"), &PulseBlockzWorld::get_properties, DEFVAL(false));
    ClassDB::bind_method(D_METHOD("set_property", "id", "name", "value"), &PulseBlockzWorld::set_property);
    ClassDB::bind_method(D_METHOD("part_surface_pieces", "id", "which"), &PulseBlockzWorld::part_surface_pieces);
    ClassDB::bind_method(D_METHOD("gui_control", "id"), &PulseBlockzWorld::gui_control);
    ClassDB::bind_method(D_METHOD("part_offset", "id"), &PulseBlockzWorld::part_offset);
    ClassDB::bind_method(D_METHOD("wanted_mouse_mode"), &PulseBlockzWorld::wanted_mouse_mode);
    ClassDB::bind_method(D_METHOD("get_attributes", "id"), &PulseBlockzWorld::get_attributes);
    ClassDB::bind_method(D_METHOD("get_tags", "id"), &PulseBlockzWorld::get_tags);
    ClassDB::bind_method(D_METHOD("set_attribute", "id", "name", "value"), &PulseBlockzWorld::set_attribute);
    ClassDB::bind_method(D_METHOD("set_tag", "id", "tag", "on"), &PulseBlockzWorld::set_tag);
    ClassDB::bind_method(D_METHOD("get_creatable_classes"), &PulseBlockzWorld::get_creatable_classes);
    ClassDB::bind_method(D_METHOD("to_rbxmx", "model_json", "name"), &PulseBlockzWorld::to_rbxmx);
    ClassDB::bind_method(D_METHOD("to_rbxlx", "place_json"), &PulseBlockzWorld::to_rbxlx);
    ClassDB::bind_method(D_METHOD("get_terrain_id"), &PulseBlockzWorld::get_terrain_id);
    ClassDB::bind_method(D_METHOD("get_terrain_info"), &PulseBlockzWorld::get_terrain_info);
    ClassDB::bind_method(D_METHOD("terrain_flatten", "resolution", "cell_size", "y"), &PulseBlockzWorld::terrain_flatten);
    ClassDB::bind_method(D_METHOD("terrain_raycast", "from", "dir"), &PulseBlockzWorld::terrain_raycast);
    ClassDB::bind_method(D_METHOD("terrain_sculpt", "at", "radius", "amount", "mode", "level", "material"), &PulseBlockzWorld::terrain_sculpt, DEFVAL(-1));
    ClassDB::bind_method(D_METHOD("terrain_material_at", "point"), &PulseBlockzWorld::terrain_material_at);
    ClassDB::bind_method(D_METHOD("get_terrain_stats"), &PulseBlockzWorld::get_terrain_stats);
    ClassDB::bind_method(D_METHOD("terrain_commit"), &PulseBlockzWorld::terrain_commit);
    ClassDB::bind_method(D_METHOD("terrain_smoothgrid"), &PulseBlockzWorld::terrain_smoothgrid);
    ClassDB::bind_method(D_METHOD("terrain_material_colors"), &PulseBlockzWorld::terrain_material_colors);
    ClassDB::bind_method(D_METHOD("terrain_height_at", "x", "z"), &PulseBlockzWorld::terrain_height_at);
    ClassDB::bind_method(D_METHOD("terrain_voxel_at", "point"), &PulseBlockzWorld::terrain_voxel_at);
    ClassDB::bind_method(D_METHOD("get_tree_version"), &PulseBlockzWorld::get_tree_version);
    ClassDB::bind_method(D_METHOD("create_instance", "class_name", "parent"), &PulseBlockzWorld::create_instance);
    ClassDB::bind_method(D_METHOD("destroy_instance", "id"), &PulseBlockzWorld::destroy_instance);
    ClassDB::bind_method(D_METHOD("set_parent", "id", "parent"), &PulseBlockzWorld::set_parent);
    ClassDB::bind_method(D_METHOD("add_model", "parent_path", "name", "model_json"), &PulseBlockzWorld::add_model);
    ClassDB::bind_method(D_METHOD("_on_body_shape_entered", "rid", "body", "body_shape", "local_shape", "id"), &PulseBlockzWorld::_on_body_shape_entered);
    ClassDB::bind_method(D_METHOD("_on_gui_input", "event", "id"), &PulseBlockzWorld::_on_gui_input);
    ClassDB::bind_method(D_METHOD("_on_edit_focus", "in", "id"), &PulseBlockzWorld::_on_edit_focus);
    ClassDB::bind_method(D_METHOD("_on_edit_changed", "text", "id"), &PulseBlockzWorld::_on_edit_changed);
    ClassDB::bind_method(D_METHOD("_on_multi_changed", "id"), &PulseBlockzWorld::_on_multi_changed);
    ClassDB::bind_method(D_METHOD("_on_edit_submitted", "text", "id"), &PulseBlockzWorld::_on_edit_submitted);
    ClassDB::bind_method(D_METHOD("_on_edit_input", "event", "id"), &PulseBlockzWorld::_on_edit_input);
    ClassDB::bind_method(D_METHOD("_on_scroll_bar", "value", "id"), &PulseBlockzWorld::_on_scroll_bar);
    ClassDB::bind_method(D_METHOD("_on_chat_submitted", "text"), &PulseBlockzWorld::_on_chat_submitted);
    ClassDB::bind_method(D_METHOD("_on_chat_focus", "in"), &PulseBlockzWorld::_on_chat_focus);
    ClassDB::bind_method(D_METHOD("chat", "text"), &PulseBlockzWorld::chat);
    ClassDB::bind_method(D_METHOD("_on_hotbar_pressed", "slot"), &PulseBlockzWorld::_on_hotbar_pressed);
    ClassDB::bind_method(D_METHOD("_on_menu_pressed"), &PulseBlockzWorld::_on_menu_pressed);
    ClassDB::bind_method(D_METHOD("_on_chat_toggle_pressed"), &PulseBlockzWorld::_on_chat_toggle_pressed);
    ClassDB::bind_method(D_METHOD("_on_menu_button", "which"), &PulseBlockzWorld::_on_menu_button);
    ClassDB::bind_method(D_METHOD("_on_quality_auto", "on"), &PulseBlockzWorld::_on_quality_auto);
    ClassDB::bind_method(D_METHOD("_on_quality_slid", "value"), &PulseBlockzWorld::_on_quality_slid);
    ClassDB::bind_method(D_METHOD("set_quality", "saved", "save"), &PulseBlockzWorld::set_quality);   // 0 Automatic, 1..10
    ClassDB::bind_method(D_METHOD("get_quality"), &PulseBlockzWorld::get_quality);
    ClassDB::bind_method(D_METHOD("get_quality_level"), &PulseBlockzWorld::get_quality_level);         // the level in effect
    ClassDB::bind_method(D_METHOD("_on_notification_button", "serial", "callback", "text"), &PulseBlockzWorld::_on_notification_button);
    ClassDB::bind_method(D_METHOD("_on_confirm", "yes"), &PulseBlockzWorld::_on_confirm);
    ClassDB::bind_method(D_METHOD("set_menu_open", "open"), &PulseBlockzWorld::set_menu_open);
    ClassDB::bind_method(D_METHOD("is_menu_open"), &PulseBlockzWorld::is_menu_open);
    ClassDB::bind_method(D_METHOD("_on_gui_mouse", "entered", "id"), &PulseBlockzWorld::_on_gui_mouse);
    ClassDB::bind_method(D_METHOD("_on_body_shape_exited", "rid", "body", "body_shape", "local_shape", "id"), &PulseBlockzWorld::_on_body_shape_exited);

    ADD_SIGNAL(MethodInfo("script_print", PropertyInfo(Variant::STRING, "script"), PropertyInfo(Variant::STRING, "text")));
    ADD_SIGNAL(MethodInfo("script_warn", PropertyInfo(Variant::STRING, "script"), PropertyInfo(Variant::STRING, "text")));
    ADD_SIGNAL(MethodInfo("script_error", PropertyInfo(Variant::STRING, "script"), PropertyInfo(Variant::STRING, "error")));
    ADD_SIGNAL(MethodInfo("script_killed", PropertyInfo(Variant::STRING, "script"), PropertyInfo(Variant::STRING, "reason")));
    ADD_SIGNAL(MethodInfo("script_paused", PropertyInfo(Variant::STRING, "script"), PropertyInfo(Variant::INT, "line"), PropertyInfo(Variant::ARRAY, "frames")));   // stopped in the debugger: frames are {function, script, line, locals}
    ADD_SIGNAL(MethodInfo("plugin_buttons_changed", PropertyInfo(Variant::ARRAY, "buttons")));   // every plugin toolbar button: {id, plugin, toolbar, button, tooltip, icon, text, active, enabled}
    ADD_SIGNAL(MethodInfo("plugin_selection_requested", PropertyInfo(Variant::PACKED_INT64_ARRAY, "ids")));   // Selection:Set from a plugin
    ADD_SIGNAL(MethodInfo("plugin_setting_changed", PropertyInfo(Variant::STRING, "plugin"), PropertyInfo(Variant::STRING, "key"), PropertyInfo(Variant::STRING, "json")));
    ADD_SIGNAL(MethodInfo("plugin_activated", PropertyInfo(Variant::STRING, "plugin"), PropertyInfo(Variant::BOOL, "active"), PropertyInfo(Variant::BOOL, "exclusive")));
    ADD_SIGNAL(MethodInfo("plugin_open_script", PropertyInfo(Variant::INT, "script"), PropertyInfo(Variant::INT, "line")));
    ADD_SIGNAL(MethodInfo("plugin_edit", PropertyInfo(Variant::STRING, "kind"), PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "name"),
                          PropertyInfo(Variant::NIL, "before", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT),
                          PropertyInfo(Variant::NIL, "after", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));   // a plugin changed the place: property / create / parent
    ADD_SIGNAL(MethodInfo("plugin_waypoint", PropertyInfo(Variant::STRING, "name")));   // ChangeHistoryService:SetWaypoint: close the undo entry
    ADD_SIGNAL(MethodInfo("script_resumed"));
    ADD_SIGNAL(MethodInfo("frame_finished", PropertyInfo(Variant::DICTIONARY, "stats")));
    ADD_SIGNAL(MethodInfo("player_joined", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::INT, "player_id")));
    ADD_SIGNAL(MethodInfo("client_joined", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::INT, "player_id"), PropertyInfo(Variant::INT, "peer")));
    ADD_SIGNAL(MethodInfo("client_left", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::INT, "player_id"), PropertyInfo(Variant::INT, "peer")));
    ADD_SIGNAL(MethodInfo("server_connected", PropertyInfo(Variant::INT, "player_id")));
    // Sign `nonce` as an EIP-191 message and call answer_sign_in.
    ADD_SIGNAL(MethodInfo("sign_in_requested", PropertyInfo(Variant::STRING, "nonce")));
    ADD_SIGNAL(MethodInfo("server_place_named", PropertyInfo(Variant::STRING, "uri")));   // a joined server said which published place it runs
    ADD_SIGNAL(MethodInfo("asset_wanted", PropertyInfo(Variant::STRING, "uri"), PropertyInfo(Variant::STRING, "kind")));   // a pblockz:// something here needs: the host fetches it and calls asset_arrived
    ADD_SIGNAL(MethodInfo("server_disconnected"));
    ADD_SIGNAL(MethodInfo("leave_game"));                        // the escape menu's Leave, confirmed: the scene decides where to go
}

void PulseBlockzWorld::set_mode(int v) { mode_ = v == MODE_SERVER ? MODE_SERVER : v == MODE_CLIENT ? MODE_CLIENT : MODE_PLAY_SOLO; }
int PulseBlockzWorld::get_mode() const { return mode_; }
void PulseBlockzWorld::set_player_name(const String& v) { playerName_ = toStd(v); }
String PulseBlockzWorld::get_player_name() const { return String::utf8(playerName_.c_str()); }
void PulseBlockzWorld::set_user_id(int64_t v) { userId_ = v; }
int64_t PulseBlockzWorld::get_user_id() const { return userId_; }
void PulseBlockzWorld::set_auto_join(bool v) { autoJoin_ = v; }
bool PulseBlockzWorld::get_auto_join() const { return autoJoin_; }
void PulseBlockzWorld::set_default_controls(bool v) { defaultControls_ = v; }
bool PulseBlockzWorld::get_default_controls() const { return defaultControls_; }
void PulseBlockzWorld::set_default_camera(bool v) { defaultCamera_ = v; }
bool PulseBlockzWorld::get_default_camera() const { return defaultCamera_; }
void PulseBlockzWorld::set_default_lighting(bool v) { defaultLighting_ = v; }
bool PulseBlockzWorld::get_default_lighting() const { return defaultLighting_; }
void PulseBlockzWorld::set_default_animations(bool v) { defaultAnimations_ = v; }
bool PulseBlockzWorld::get_default_animations() const { return defaultAnimations_; }
void PulseBlockzWorld::set_threaded(bool v) { threaded_ = v; }
bool PulseBlockzWorld::get_threaded() const { return threaded_; }
void PulseBlockzWorld::set_auto_step(bool v) { autoStep_ = v; }
bool PulseBlockzWorld::get_auto_step() const { return autoStep_; }
// A world already running takes the new budget from its next call on.
void PulseBlockzWorld::push_budget() {
    pulseblockz::Budget b = opts_.budget;
    if (server_) queue_job([b](Runtime& rt) { rt.sandbox().setBudget(b); });
    if (client_) clientJobs_.push_back([b](Runtime& rt) { rt.sandbox().setBudget(b); });
}
void PulseBlockzWorld::set_max_millis(double v) { opts_.budget.maxMillis = v; push_budget(); }
double PulseBlockzWorld::get_max_millis() const { return opts_.budget.maxMillis; }
void PulseBlockzWorld::set_max_frame_millis(double v) { opts_.budget.maxFrameMillis = v; push_budget(); }
double PulseBlockzWorld::get_max_frame_millis() const { return opts_.budget.maxFrameMillis; }
void PulseBlockzWorld::set_max_steps(int64_t v) { opts_.budget.maxSteps = (uint64_t)v; push_budget(); }
int64_t PulseBlockzWorld::get_max_steps() const { return (int64_t)opts_.budget.maxSteps; }
void PulseBlockzWorld::set_max_memory_mb(int v) { opts_.budget.maxMemory = (size_t)v << 20; push_budget(); }
void PulseBlockzWorld::set_listen_port(int v) { listenPort_ = v; }
int PulseBlockzWorld::get_listen_port() const { return listenPort_; }
void PulseBlockzWorld::set_bind_address(const String& v) { bindAddress_ = toStd(v); }
String PulseBlockzWorld::get_bind_address() const { return String::utf8(bindAddress_.c_str()); }
void PulseBlockzWorld::set_data_store_path(const String& v) { dataStorePath_ = toStd(v); }
String PulseBlockzWorld::get_data_store_path() const { return String::utf8(dataStorePath_.c_str()); }
void PulseBlockzWorld::set_asset_root(const String& v) { assetRoot_ = toStd(v); if (!assetRoot_.empty() && assetRoot_.back() != '/') assetRoot_ += '/'; }
String PulseBlockzWorld::get_asset_root() const { return String::utf8(assetRoot_.c_str()); }
void PulseBlockzWorld::set_server_address(const String& v) { serverAddress_ = toStd(v); }
String PulseBlockzWorld::get_server_address() const { return String::utf8(serverAddress_.c_str()); }
void PulseBlockzWorld::set_server_port(int v) { serverPort_ = v; }
int PulseBlockzWorld::get_server_port() const { return serverPort_; }
void PulseBlockzWorld::set_physics_layer(int v) { physicsLayer_ = std::clamp(v, 1, 16); }
int PulseBlockzWorld::get_physics_layer() const { return physicsLayer_; }
int PulseBlockzWorld::get_max_memory_mb() const { return (int)(opts_.budget.maxMemory >> 20); }

void PulseBlockzWorld::ensure_runtime() {
    if (server_ || client_) return;
    Runtime::Options copts = opts_;
    copts.isServer = false;
    if (mode_ == MODE_CLIENT) {
        client_ = std::make_unique<RuntimeThread>(copts, threaded_);
        netClient_ = true;
    } else {
        opts_.isServer = true;
        opts_.dataStorePath = dataStorePath_.empty() ? "" : toStd(ProjectSettings::get_singleton()->globalize_path(String::utf8(dataStorePath_.c_str())));
        server_ = std::make_unique<RuntimeThread>(opts_, threaded_);
        server_->runtime().setReplicateAll(editMode_);   // a Team Create host sends the whole place
        // Play Solo is one process with no socket and no second party, so client code may be
        // handed straight over. Over a socket a client fetches the place from the chain instead.
        if (mode_ == MODE_PLAY_SOLO) server_->runtime().setInProcess(true);
        server_->runtime().setReplicating(true);
        if (mode_ == MODE_PLAY_SOLO) client_ = std::make_unique<RuntimeThread>(copts, threaded_);
    }
    // The initial skeleton (services) predates the change log; the client's is the same tree
    // with the same fixed ids.
    for (const Change& c : (server_ ? *server_ : *client_).runtime().dataModel().snapshot()) apply_change(c);
    refresh_all_visibility();
    Variant g = ProjectSettings::get_singleton()->get_setting("physics/3d/default_gravity");
    if (g.get_type() == Variant::FLOAT || g.get_type() == Variant::INT) engineGravity_ = (double)g;
    if (engineGravity_ <= 0) engineGravity_ = 9.8;
}

// The build stamp is printed once per process: neither this library nor a script on disk picks
// up an edit without a restart.
void PulseBlockzWorld::_ready() {
    start_watchdog();
    if (Engine::get_singleton()->is_editor_hint()) return;
    static bool said = false;
    if (!said) {
        said = true;
        UtilityFunctions::print("pulseblockz " PULSEBLOCKZ_VERSION ", built " PULSEBLOCKZ_BUILT);
    }
    ensure_runtime();
    if (client_ && defaultCamera_ && !camera_) {
        camera_ = memnew(Camera3D);
        camera_->set_name("Camera");
        add_child(camera_);
        camera_->look_at_from_position(Vector3(0, 22, 34), Vector3(0, 0, 0), Vector3(0, 1, 0));
        camera_->set_fov(70);   // Camera.FieldOfView's default; both are vertical
        camera_->make_current();
    }
    if (client_ && defaultLighting_ && !sun_) {
        // The Lighting service, rendered: a sun following ClockTime, a procedural sky, ambient
        // and fog.
        sun_ = memnew(DirectionalLight3D);
        sun_->set_name("Sun");
        add_child(sun_);
        env_ = memnew(WorldEnvironment);
        env_->set_name("Lighting");
        Ref<Environment> e; e.instantiate();
        Ref<Sky> sky; sky.instantiate();
        Ref<ProceduralSkyMaterial> mat; mat.instantiate();
        // Below the horizon Roblox's sky stays sky -- pale blue-grey, not Godot's earth-brown.
        mat->set_ground_horizon_color(Color(0.6463f, 0.6558f, 0.6708f));
        mat->set_ground_bottom_color(Color(0.50f, 0.56f, 0.64f));
        sky->set_material(mat);
        e->set_sky(sky);
        e->set_background(Environment::BG_SKY);
        e->set_tonemapper(Environment::TONE_MAPPER_FILMIC);
        // White at 1.6, where Neon is drawn (kNeonHdr). At the filmic default of 1 anything
        // brighter clips to white and a red Neon comes out pink.
        e->set_tonemap_white(1.6f);
        e->set_ambient_source(Environment::AMBIENT_SOURCE_COLOR);
        e->set_fog_mode(Environment::FOG_MODE_DEPTH);
        env_->set_environment(e);
        add_child(env_);
        update_lighting();
        effectsDirty_ = true;   // the effects the place already carries, before any is added
    }
    set_process(true);
    set_physics_process(true);
    set_process_unhandled_input(true);
    set_process_input(true);      // motion, which a control the pointer is over would otherwise eat
    if (server_ && listenPort_ > 0) listen(listenPort_, maxClients_);
    if (netClient_ && autoJoin_) connect_to_server(String::utf8(serverAddress_.c_str()), serverPort_);
}

// ---- jobs --------------------------------------------------------------------------
void PulseBlockzWorld::queue_job(std::function<void(Runtime&)> job) {
    ensure_runtime();
    (netClient_ ? clientJobs_ : jobs_).push_back(std::move(job));   // a Client world only has the one runtime
}

// Job errors go through the runtime's log, so they arrive with the frame, in order with script
// output.
void PulseBlockzWorld::import_place(const PackedByteArray& bytes) {
    std::string b((const char*)bytes.ptr(), (size_t)bytes.size());
    queue_job([b](Runtime& rt) {
        std::string err, note;
        int n = importPlace(rt, b, &err, &note);
        if (n < 0) { rt.reportError("import", err); return; }
        rt.reportWarn("import", "imported " + std::to_string(n) + " instances" + (note.empty() ? "" : "; " + note));
    });
}

void PulseBlockzWorld::load_file(const String& rel_path, const String& source) {
    std::string p = toStd(rel_path), s = toStd(source);
    if (collaborating()) { NetEdit e; e.op = "file"; e.name = p; e.text = s; forward_edit(std::move(e)); return; }
    queue_job([p, s](Runtime& rt) {
        std::string err;
        if (!loadSourceFile(rt, p, s, &err)) rt.reportError(p, err);
    });
}
void PulseBlockzWorld::unload_file(const String& rel_path) {
    std::string p = toStd(rel_path);
    if (collaborating()) { NetEdit e; e.op = "unfile"; e.name = p; forward_edit(std::move(e)); return; }
    queue_job([p](Runtime& rt) { unloadSourceFile(rt, p); });
}
void PulseBlockzWorld::run_chunk(const String& name, const String& source) {
    std::string n = toStd(name), s = toStd(source);
    if (collaborating()) { NetEdit e; e.op = "chunk"; e.name = n; e.text = s; forward_edit(std::move(e)); return; }   // the command bar runs on the host
    queue_job([n, s](Runtime& rt) { rt.runChunk(n, s); });
}
void PulseBlockzWorld::run_client_chunk(const String& name, const String& source) {
    std::string n = toStd(name), s = toStd(source);
    ensure_runtime();
    if (!client_) return;
    clientJobs_.push_back([n, s](Runtime& rt) { rt.runChunk(n, s); });
}
// ---- sign-in ------------------------------------------------------------------------
// A player is their wallet. Each joiner gets a random nonce, its wallet signs it and names its
// address, the server recovers the signer and on a match sets the Player's WalletAddress
// attribute. The server holds no key; a joiner whose wallet cannot sign stays a guest.

static std::string new_sign_in_nonce() {
    std::random_device rd;
    std::string out = "0x";
    static const char* hex = "0123456789abcdef";
    for (int i = 0; i < 32; i++) { unsigned v = rd() & 0xff; out += hex[v >> 4]; out += hex[v & 15]; }
    return out;
}

// An EIP-191 personal message, so it can never be mistaken for a transaction. It names the
// server as well as the nonce: with the nonce alone, a server you joined could relay another
// server's challenge and sign in there as you. The client writes the host:port it dialled.
static const char* kSoloServer = "this machine";
static std::string sign_in_text(const std::string& server, const std::string& nonce) {
    return "PulseBlockz sign-in\nServer: " + server + "\nNonce: " + nonce;
}
static pulseblockz::crypto::Hash sign_in_digest(const std::string& server, const std::string& nonce) {
    const std::string message = sign_in_text(server, nonce);
    const std::string prefixed = "\x19" "Ethereum Signed Message:\n" + std::to_string(message.size()) + message;
    return pulseblockz::crypto::keccak256(prefixed);
}

// The address that proved itself, checksummed; "" if the proof proves nothing.
static std::string verify_sign_in(const std::string& server, const std::string& nonce, const std::string& address, const std::string& signature) {
    if (server.empty() || nonce.empty() || address.empty() || signature.empty()) return "";
    std::vector<uint8_t> sig;
    if (!pulseblockz::crypto::fromHex(signature, sig) || sig.size() != 65) return "";
    const std::string signer = pulseblockz::crypto::recoverAddress(sign_in_digest(server, nonce), sig);
    auto lower = [](std::string s) { for (auto& ch : s) ch = (char)std::tolower((unsigned char)ch); return s; };
    if (signer.empty() || lower(signer) != lower(address)) return "";
    return signer;
}

// A job: the Player lives in the server runtime's tree.
void PulseBlockzWorld::sign_in_player(int64_t playerId, const std::string& address) {
    if (!playerId || address.empty()) return;
    jobs_.push_back([playerId, address](Runtime& rt) {
        if (Instance* p = rt.dataModel().find(playerId)) if (!p->destroyed()) p->setAttribute("WalletAddress", Value::string(address));
    });
}

String PulseBlockzWorld::sign_in_message(const String& nonce) const {
    const std::string server = netClient_ ? serverAddress_ + ":" + std::to_string(serverPort_) : std::string(kSoloServer);
    return String::utf8(sign_in_text(server, toStd(nonce)).c_str());
}

void PulseBlockzWorld::set_max_clients(int v) { maxClients_ = v > 0 ? v : 1; }
int PulseBlockzWorld::get_max_clients() const { return maxClients_; }

void PulseBlockzWorld::set_place_uri(const String& v) { placeUri_ = toStd(v); }
String PulseBlockzWorld::get_place_uri() const { return String::utf8(placeUri_.c_str()); }
String PulseBlockzWorld::server_place() const { return String::utf8(placeUri_.c_str()); }

void PulseBlockzWorld::set_hold_for_place(bool v) { holdForPlace_ = v; }
bool PulseBlockzWorld::get_hold_for_place() const { return holdForPlace_; }

// Adding the local player is what copies StarterPlayerScripts into PlayerScripts, so it waits
// until the checked place has been loaded over the sourceless instances the server sent.
void PulseBlockzWorld::ready_for_place() {
    if (!awaitingTree_ || !pendingJoin_) return;
    awaitingTree_ = false;
    auto j = pendingJoin_;
    const bool edit = editMode_;   // a joined Studio flies its own camera and plays nothing
    clientJobs_.push_back([j, edit](Runtime& rt) {
        DataModel& dm = rt.dataModel();
        if (!edit) {
            Instance::Ptr cam = dm.create("Camera", dm.workspace());
            cam->setName("Camera");
            dm.workspace()->set("CurrentCamera", Value::instance(cam->id()));
            j->cameraId = cam->id();
        }
        rt.addPlayer(j->name, j->userId, j->playerId);
    });
}

void PulseBlockzWorld::set_public_names(const PackedStringArray& v) {
    publicNames_.clear();
    for (int64_t i = 0; i < v.size(); i++) { std::string n = toStd(v[i]); if (!n.empty()) publicNames_.push_back(n); }
}
PackedStringArray PulseBlockzWorld::get_public_names() const {
    PackedStringArray out;
    for (const auto& n : publicNames_) out.push_back(String::utf8(n.c_str()));
    return out;
}

// The other half of the anti-relay check: a proof counts only for a name this server answers to.
bool PulseBlockzWorld::accepts_server_name(const std::string& server) const {
    const size_t colon = server.rfind(':');
    if (colon == std::string::npos || colon == 0) return false;
    std::string host = server.substr(0, colon);
    const std::string port = server.substr(colon + 1);
    if (listeningPort_ <= 0 || port != std::to_string(listeningPort_)) return false;
    if (host.size() > 2 && host.front() == '[' && host.back() == ']') host = host.substr(1, host.size() - 2);
    auto lower = [](std::string s) { for (auto& ch : s) ch = (char)std::tolower((unsigned char)ch); return s; };
    host = lower(host);
    if (host == "localhost") return true;
    for (const auto& n : publicNames_) if (lower(n) == host) return true;
    PackedStringArray mine = IP::get_singleton()->get_local_addresses();
    for (int64_t i = 0; i < mine.size(); i++) if (lower(toStd(mine[i])) == host) return true;
    return false;
}

void PulseBlockzWorld::answer_sign_in(const String& address, const String& signature, const String& server) {
    const std::string addr = toStd(address), sig = toStd(signature);
    if (netClient_) {
        // The server does the checking; the name defaults to the one this client dialled.
        NetPacket pk; pk.type = NetPacket::Proof;
        pk.address = addr; pk.signature = sig;
        pk.server = server.is_empty() ? serverAddress_ + ":" + std::to_string(serverPort_) : toStd(server);
        net_send(1, pk);
        return;
    }
    // Play Solo: the same check against the local player's own nonce, in this process.
    const std::string proven = verify_sign_in(kSoloServer, localNonce_, addr, sig);
    if (proven.empty()) return;
    localNonce_.clear();
    sign_in_player(localPlayerId_, proven);
}

// ---- the debugger -------------------------------------------------------------------
void PulseBlockzWorld::set_breakpoint(const String& script, int line, bool on) {
    std::string n = toStd(script);
    ensure_runtime();
    auto job = [n, line, on](Runtime& rt) { rt.setBreakpoint(n, line, on); };
    if (server_) jobs_.push_back(job);
    if (client_) clientJobs_.push_back(job);
}
void PulseBlockzWorld::debug_continue() {
    ensure_runtime();
    auto job = [](Runtime& rt) { rt.debugContinue(); };
    if (server_) jobs_.push_back(job);
    if (client_) clientJobs_.push_back(job);
}
void PulseBlockzWorld::debug_step(int kind) {
    ensure_runtime();
    auto job = [kind](Runtime& rt) { rt.debugStep(kind); };
    if (server_) jobs_.push_back(job);
    if (client_) clientJobs_.push_back(job);
}
// Physics is held for as long as either runtime stands still in the debugger.
void PulseBlockzWorld::debug_report(const FrameOut& out, bool server) {
    if (out.debugFresh) {
        Array frames;
        for (auto& f : out.debug.frames) {
            Dictionary d;
            d["function"] = String::utf8(f.function.c_str());
            d["script"] = String::utf8(f.script.c_str());
            d["line"] = f.line;
            Dictionary locals;
            for (auto& [k, v] : f.locals) locals[String::utf8(k.c_str())] = String::utf8(v.c_str());
            d["locals"] = locals;
            frames.push_back(d);
        }
        emit_signal("script_paused", String::utf8(out.debug.script.c_str()), out.debug.line, frames);
    }
    bool before = debugPausedServer_ || debugPausedClient_;
    (server ? debugPausedServer_ : debugPausedClient_) = out.debugPaused;
    bool after = debugPausedServer_ || debugPausedClient_;
    if (before && !after) emit_signal("script_resumed");
    if (after != physicsHeld_) { physicsHeld_ = after; PhysicsServer3D::get_singleton()->set_active(!after); }
}

void PulseBlockzWorld::add_player(const String& name, int64_t user_id) {
    std::string n = toStd(name);
    ensure_runtime();
    if (netClient_) return;                                    // a Client joins through the server
    bool localPending = (join_ && join_->peer == 0);
    for (auto& j : joinQueue_) if (j->peer == 0) localPending = true;
    if (!client_ || localPlayerId_ || localPending) {
        // Headless: a Player on the server with nobody behind it.
        queue_job([n, user_id](Runtime& rt) { rt.addPlayer(n, user_id); });
        return;
    }
    // The local player: joins like a network client, minus the wire.
    auto j = std::make_shared<Join>();
    j->name = n; j->userId = user_id; j->peer = 0;
    joinQueue_.push_back(j);
}

// One join per server frame: add the Player, then snapshot everything the joiner must know,
// its own Player included. What the frame replicated before the snapshot still goes to the
// clients already there.
void PulseBlockzWorld::dispatch_join() {
    if (join_ || joinQueue_.empty() || !server_) return;
    join_ = joinQueue_.front();
    joinQueue_.pop_front();
    auto j = join_;
    jobs_.push_back([j](Runtime& rt) {
        Instance* p = rt.addPlayer(j->name, j->userId);
        j->playerId = p ? p->id() : 0;
        j->before = rt.takeReplication();
        j->snapshot = rt.replicationJoin();
        j->done = true;
    });
}
void PulseBlockzWorld::remove_player(const String& name) {
    std::string n = toStd(name);
    queue_job([n](Runtime& rt) {
        Instance* players = rt.dataModel().getService("Players");
        if (Instance* p = players->findFirstChild(n)) rt.removePlayer(p);
    });
}

bool PulseBlockzWorld::is_idle() const { return (!server_ || server_->idle()) && (!client_ || client_->idle()); }
Dictionary PulseBlockzWorld::get_stats() const { return stats_; }
Camera3D* PulseBlockzWorld::get_camera() const { return camera_; }
int64_t PulseBlockzWorld::get_local_player_id() const { return localPlayerId_; }
int64_t PulseBlockzWorld::get_local_character_id() const { return localChar_; }
Node3D* PulseBlockzWorld::get_local_character_node() const {
    auto cit = chars_.find(localChar_);
    if (cit == chars_.end() || !cit->second.root) return nullptr;
    auto pit = parts_.find(cit->second.root);
    return pit == parts_.end() ? nullptr : pit->second.body;
}

// ---- the mirrored tree ------------------------------------------------------------
// entries_ is what the runtimes have reported so far, readable on any frame without a lock.
// Ids keep the sign the DataModel gave them -- a server's own count up, a client's own count
// down, a replicated instance keeps the server's id on both sides -- so one map holds a Play
// Solo world's two trees without collisions.
void PulseBlockzWorld::set_edit_mode(bool v) {
    editMode_ = v;
    opts_.runScripts = !v;
}
bool PulseBlockzWorld::get_edit_mode() const { return editMode_; }

void PulseBlockzWorld::set_gui_preview(bool v) {
    if (guiPreview_ == v) return;
    guiPreview_ = v;
    guiDirty_ = guiRestyleAll_ = true;   // built or freed on the next sync
}
bool PulseBlockzWorld::get_gui_preview() const { return guiPreview_; }

void PulseBlockzWorld::set_gui_preview_rect(const Rect2& r) {
    if (guiPreviewRect_ == r) return;
    guiPreviewRect_ = r;
    guiDirty_ = guiRestyleAll_ = true;
}
Rect2 PulseBlockzWorld::get_gui_preview_rect() const { return guiPreviewRect_; }

Dictionary PulseBlockzWorld::get_class_members(const String& class_name) const {
    Dictionary out;
    const ClassDef* cls = findClass(toStd(class_name));
    if (!cls) return out;
    std::vector<std::string> props, events, callbacks;
    for (const ClassDef* c = cls; c; c = c->base) {
        for (const PropDef& p : c->props) if (!(p.flags & Hidden)) props.push_back(p.name);
        for (const std::string& e : c->events) events.push_back(e);
        for (const std::string& k : c->callbacks) callbacks.push_back(k);
    }
    std::sort(props.begin(), props.end()); props.erase(std::unique(props.begin(), props.end()), props.end());
    std::sort(events.begin(), events.end()); events.erase(std::unique(events.begin(), events.end()), events.end());
    auto pack = [](const std::vector<std::string>& v) { PackedStringArray a; for (auto& s : v) a.push_back(String::utf8(s.c_str())); return a; };
    out["properties"] = pack(props);
    out["events"] = pack(events);
    out["methods"] = pack(methodNamesFor(*cls));
    std::sort(callbacks.begin(), callbacks.end()); callbacks.erase(std::unique(callbacks.begin(), callbacks.end()), callbacks.end());
    out["callbacks"] = pack(callbacks);
    return out;
}

Rect2 PulseBlockzWorld::gui_rect(int64_t id) const {
    auto it = guis_.find(id);
    if (it == guis_.end() || !it->second.node || !it->second.node->is_visible_in_tree()) return Rect2();
    return Rect2(it->second.node->get_global_position(), it->second.node->get_size());
}

// The deepest GuiObject whose drawn rect holds the point: a button inside a frame wins.
int64_t PulseBlockzWorld::gui_at(const Vector2& point) const {
    int64_t best = 0;
    int bestDepth = -1;
    for (const auto& [id, g] : guis_) {
        if (g.kind != GUI_OBJECT || !g.node || !g.node->is_visible_in_tree()) continue;
        if (!Rect2(g.node->get_global_position(), g.node->get_size()).has_point(point)) continue;
        // Only what the place authored: the built-in hotbar and player list cover most of the
        // screen.
        int depth = 0;
        bool mine = false;
        for (int64_t a = id, hops = 0; a != kNoParent && hops < 64; hops++) {
            auto ait = entries_.find(a);
            if (ait == entries_.end()) break;
            a = ait->second.parent;
            depth++;
            if (a == starterGuiId_ && starterGuiId_) { mine = true; break; }
        }
        if (!mine) continue;
        if (depth > bestDepth) { bestDepth = depth; best = id; }
    }
    return best;
}

void PulseBlockzWorld::set_track_properties(bool v) {
    if (trackProps_ == v) return;
    trackProps_ = v;
    if (!v) for (auto& [id, e] : entries_) e.props.clear();
}
bool PulseBlockzWorld::get_track_properties() const { return trackProps_; }
int64_t PulseBlockzWorld::get_tree_version() const { return treeVersion_; }

PackedInt64Array PulseBlockzWorld::get_child_ids(int64_t id, bool client) const {
    // Creation order: what Roblox's Explorer shows, and the order a place file was written in.
    // The mirror is a hash map, so each entry carries the count it arrived on.
    std::vector<std::pair<int64_t, int64_t>> kids;
    for (const auto& [cid, e] : entries_)
        if (e.parent == id && (id != 0 || (client ? cid < 0 : cid > 0))) kids.push_back({e.made, cid});
    std::sort(kids.begin(), kids.end());
    PackedInt64Array out;
    for (const auto& [made, cid] : kids) out.push_back(cid);
    return out;
}

// An attribute rides the change log as a property named `@Thing`, a tag as `#Thing` holding a
// boolean, so the mirror already holds both.
Dictionary PulseBlockzWorld::get_attributes(int64_t id) const {
    Dictionary out;
    auto it = entries_.find(id);
    if (it == entries_.end()) return out;
    for (const auto& [name, v] : it->second.props)
        if (name.size() > 1 && name[0] == '@' && v.type != Value::Nil)
            out[String::utf8(name.c_str() + 1)] = toVariant(v);
    return out;
}

PackedStringArray PulseBlockzWorld::get_tags(int64_t id) const {
    PackedStringArray out;
    auto it = entries_.find(id);
    if (it == entries_.end()) return out;
    for (const auto& [name, v] : it->second.props)
        if (name.size() > 1 && name[0] == '#' && v.type == Value::Bool && v.b)
            out.push_back(String::utf8(name.c_str() + 1));
    return out;
}

bool PulseBlockzWorld::set_attribute(int64_t id, const String& name, const Variant& value) {
    if (!entries_.count(id)) return false;
    std::string what = toStd(name);
    if (what.empty()) return false;
    Value v;
    switch (value.get_type()) {
    case Variant::NIL: break;                                    // removes it
    case Variant::BOOL: v = Value::boolean((bool)value); break;
    case Variant::INT: case Variant::FLOAT: v = Value::number((double)value); break;
    case Variant::STRING: v = Value::string(toStd(value)); break;
    case Variant::VECTOR3: v = Value::vector3(fromGd(Vector3(value))); break;
    case Variant::COLOR: { Color c = value; v = Value::color3(c.r, c.g, c.b); break; }
    default: return false;
    }
    if (collaborating()) { NetEdit e; e.op = "attr"; e.id = id; e.name = what; e.value = v; forward_edit(std::move(e)); return true; }
    queue_job([id, what, v](Runtime& rt) {
        if (Instance* i = rt.dataModel().find(id)) i->setAttribute(what, v);
    });
    return true;
}

bool PulseBlockzWorld::set_tag(int64_t id, const String& tag, bool on) {
    if (!entries_.count(id)) return false;
    std::string what = toStd(tag);
    if (what.empty()) return false;
    if (collaborating()) { NetEdit e; e.op = "tag"; e.id = id; e.name = what; e.flag = on; forward_edit(std::move(e)); return true; }
    queue_job([id, what, on](Runtime& rt) {
        Instance* i = rt.dataModel().find(id);
        if (!i) return;
        if (on) i->addTag(what); else i->removeTag(what);
    });
    return true;
}

Dictionary PulseBlockzWorld::get_instance(int64_t id) const {
    Dictionary d;
    if (id == 0) {   // the DataModel itself: no Create ever announced it
        d["id"] = (int64_t)0;
        d["name"] = "game";
        d["class_name"] = "DataModel";
        d["parent"] = (int64_t)kNoParent;
        d["path"] = "game";
        return d;
    }
    auto it = entries_.find(id);
    if (it == entries_.end()) return d;
    d["id"] = id;
    d["name"] = String::utf8(it->second.name.c_str());
    d["class_name"] = String::utf8(it->second.className.c_str());
    d["parent"] = it->second.parent;
    String path = String::utf8(it->second.name.c_str());
    int64_t up = it->second.parent;
    for (int hops = 0; up != 0 && up != kNoParent && hops < 64; hops++) {
        auto pit = entries_.find(up);
        if (pit == entries_.end()) break;
        path = String::utf8(pit->second.name.c_str()) + "." + path;
        up = pit->second.parent;
    }
    d["path"] = up == 0 ? "game." + path : path;
    return d;
}

Array PulseBlockzWorld::get_properties(int64_t id, bool hidden) const {
    Array out;
    auto it = entries_.find(id);
    const ClassDef* cls = it == entries_.end() ? nullptr : findClass(it->second.className);
    if (!cls) return out;
    std::vector<const PropDef*> defs;
    cls->collectProps(defs);
    for (const PropDef* def : defs) {
        if ((def->flags & (Hidden | NotBrowsable)) && !hidden) continue;
        auto pit = it->second.props.find(def->aliasOf.empty() ? def->name : def->aliasOf);
        bool own = pit != it->second.props.end();
        Dictionary e;
        e["name"] = String::utf8(def->name.c_str());
        e["type"] = String(Value::typeName(def->type));
        e["value"] = toVariant(own ? pit->second : def->def);
        e["read_only"] = (def->flags & ReadOnly) != 0;
        // The Content twin this string mirrors ("Image" -> "ImageContent"): a saved file keeps
        // the Content, as Roblox's own files do.
        e["twin"] = def->type == Value::String && !def->twin.empty() ? String::utf8(def->twin.c_str()) : String();
        e["alias"] = String::utf8(def->aliasOf.c_str());   // another name for a stored property: never saved under this one
        e["is_default"] = !own;   // nothing has written it: this is the class default
        e["enum_type"] = def->enumType ? String(def->enumType->name) : String();
        PackedStringArray items;
        if (def->enumType) for (const EnumItem& i : def->enumType->items) items.push_back(String(i.name));
        e["enum_items"] = items;
        out.push_back(e);
    }
    return out;
}

bool PulseBlockzWorld::set_property(int64_t id, const String& name, const Variant& value) {
    auto it = entries_.find(id);
    // id 0 is the DataModel, which is not in entries_ (that mirror holds what is under the
    // services), so JobId, PlaceVersion and the private-server pair need this branch.
    const ClassDef* cls = id == 0 ? findClass("DataModel")
                        : it == entries_.end() ? nullptr : findClass(it->second.className);
    std::string prop = toStd(name);
    const PropDef* def = cls ? cls->findProp(prop) : nullptr;
    if (!def) return false;
    Value v;
    if (!fromVariant(*def, value, v)) return false;
    // ReadOnly binds scripts, not Studio -- Roblox's own panel sets Humanoid.RigType -- so a
    // ReadOnly write goes in through setInternal.
    if (collaborating()) { NetEdit e; e.op = "prop"; e.id = id; e.name = prop; e.value = v; forward_edit(std::move(e)); return true; }
    bool host = (def->flags & ReadOnly) != 0;
    queue_job([id, prop, v, host](Runtime& rt) {
        Instance* i = rt.dataModel().find(id);
        if (!i) return;
        std::string err;
        if (!(host ? i->setInternal(prop, v, &err) : i->set(prop, v, &err))) rt.reportError("studio", err);
    });
    return true;
}

bool PulseBlockzWorld::create_instance(const String& class_name, int64_t parent) {
    std::string cls = toStd(class_name);
    const ClassDef* def = findClass(cls);
    if (!def || !def->creatable) return false;
    if (parent != 0 && !entries_.count(parent)) return false;
    if (collaborating()) { NetEdit e; e.op = "create"; e.name = cls; e.parent = parent; forward_edit(std::move(e)); return true; }
    queue_job([cls, parent](Runtime& rt) {
        DataModel& dm = rt.dataModel();
        Instance* p = parent ? dm.find(parent) : dm.root();
        if (!p) return;
        std::string err;
        if (!dm.create(cls, p, &err)) rt.reportError("studio", err);
    });
    return true;
}

bool PulseBlockzWorld::destroy_instance(int64_t id) {
    if (!id || !entries_.count(id)) return false;
    if (collaborating()) { NetEdit e; e.op = "destroy"; e.id = id; forward_edit(std::move(e)); return true; }
    queue_job([id](Runtime& rt) {
        if (Instance* i = rt.dataModel().find(id)) i->destroy();
    });
    return true;
}

PackedStringArray PulseBlockzWorld::get_creatable_classes() const {
    std::vector<std::string> names;
    for (const ClassDef* c : allClasses())
        if (c->creatable && !c->service) names.push_back(c->name);
    std::sort(names.begin(), names.end());
    PackedStringArray out;
    for (const std::string& n : names) out.push_back(String::utf8(n.c_str()));
    return out;
}

String PulseBlockzWorld::to_rbxlx(const String& place_json) const {
    std::string out, err;
    if (!placeJsonToRbxlx(toStd(place_json), out, &err)) {
        UtilityFunctions::push_warning(String("to_rbxlx: ") + String::utf8(err.c_str()));
        return String();
    }
    return String::utf8(out.c_str());
}

String PulseBlockzWorld::to_rbxmx(const String& model_json, const String& name) const {
    std::string out, err;
    if (!modelJsonToRbxmx(toStd(model_json), toStd(name), out, &err)) {
        UtilityFunctions::push_warning(String("to_rbxmx: ") + String::utf8(err.c_str()));
        return String();
    }
    return String::utf8(out.c_str());
}

bool PulseBlockzWorld::set_parent(int64_t id, int64_t parent) {
    if (!id || !entries_.count(id)) return false;
    if (parent && !entries_.count(parent)) return false;
    if (collaborating()) { NetEdit e; e.op = "parent"; e.id = id; e.parent = parent; forward_edit(std::move(e)); return true; }
    queue_job([id, parent](Runtime& rt) {
        DataModel& dm = rt.dataModel();
        Instance* i = dm.find(id);
        Instance* p = parent ? dm.find(parent) : dm.root();
        if (!i || !p) return;
        std::string err;
        if (!i->set("Parent", Value::instance(p->id()), &err)) rt.reportError("studio", err);
    });
    return true;
}

void PulseBlockzWorld::add_model(const String& parent_path, const String& name, const String& model_json) {
    std::vector<std::string> containers;
    for (const String& part : parent_path.split("/", false)) containers.push_back(toStd(part));
    std::string what = toStd(name), json = toStd(model_json);
    if (collaborating()) { NetEdit e; e.op = "model"; e.path = toStd(parent_path); e.name = what; e.text = json; forward_edit(std::move(e)); return; }
    queue_job([containers, what, json](Runtime& rt) {
        std::string err;
        if (!addModelJson(rt, containers, what, json, &err)) rt.reportError(what, err);
    });
}

// ---- Team Create ---------------------------------------------------------------------
bool PulseBlockzWorld::is_hosting() const { return server_ && peer_.is_valid(); }
void PulseBlockzWorld::forward_edit(NetEdit e) { ensure_runtime(); pendingEdits_.push_back(std::move(e)); }
// An edit from a joined Studio, applied through the same calls the local panels use. The host
// is not collaborating(), so nothing forwards back out.
void PulseBlockzWorld::apply_edit(const NetEdit& e) {
    if (e.op == "prop") set_property(e.id, String::utf8(e.name.c_str()), toVariant(e.value));
    else if (e.op == "attr") set_attribute(e.id, String::utf8(e.name.c_str()), toVariant(e.value));
    else if (e.op == "tag") set_tag(e.id, String::utf8(e.name.c_str()), e.flag);
    else if (e.op == "parent") set_parent(e.id, e.parent);
    else if (e.op == "create") create_instance(String::utf8(e.name.c_str()), e.parent);
    else if (e.op == "destroy") destroy_instance(e.id);
    else if (e.op == "model") add_model(String::utf8(e.path.c_str()), String::utf8(e.name.c_str()), String::utf8(e.text.c_str()));
    else if (e.op == "chunk") run_chunk(String::utf8(e.name.c_str()), String::utf8(e.text.c_str()));
    else if (e.op == "file") load_file(String::utf8(e.name.c_str()), String::utf8(e.text.c_str()));
    else if (e.op == "unfile") unload_file(String::utf8(e.name.c_str()));
}

Node3D* PulseBlockzWorld::get_part_node(int64_t id) const {
    auto it = parts_.find(id);
    return it == parts_.end() ? nullptr : it->second.body;
}
Node3D* PulseBlockzWorld::get_part_mesh(int64_t id) const {
    auto it = parts_.find(id);
    return it == parts_.end() ? nullptr : it->second.mesh;
}
int64_t PulseBlockzWorld::get_part_id(Node* node) const {
    if (!node) return 0;
    auto it = bodyToId_.find(node->get_instance_id());
    return it == bodyToId_.end() ? 0 : it->second;
}

void PulseBlockzWorld::flush() {
    ensure_runtime();
    auto settle = [&] {
        if (server_) server_->waitIdle();
        if (client_) client_->waitIdle();
        if (!inFlight_) return;
        if (server_) { FrameOut so = server_->take(); apply_server(so); }
        if (client_) { FrameOut co = client_->take(); apply_client(co); }
        inFlight_ = false; framesDone_++;
        emit_signal("frame_finished", stats_);
        flush_cloud_notes();
    };
    settle();
    net_poll();
    dispatch_join();
    submit_frames();
    settle();
}

// ---- the frame loop ----------------------------------------------------------------
void PulseBlockzWorld::_process(double delta) {
    if (!server_ && !client_) return;
    lastTickUsec_.store((int64_t)Time::get_singleton()->get_ticks_usec());
    phase_.store("net poll");
    net_poll();                               // every render frame, in flight or not: packets queue up for the next one
    pendingDt_ += delta;
    if (!is_idle()) return;                   // last frame still running: try again next render frame
    if (inFlight_) {
        // Host time, apart from the runtime thread: applying the frame's changes here, and
        // snapshot plus submit below.
        const auto t0 = std::chrono::steady_clock::now();
        phase_.store("applying the server frame");
        if (server_) { FrameOut so = server_->take(); apply_server(so); }
        phase_.store("applying the client frame");
        if (client_) { FrameOut co = client_->take(); apply_client(co); }
        phase_.store("after the frame");
        stats_["host_apply_ms"] = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        stats_["host_submit_ms"] = hostSubmitMs_;
        inFlight_ = false; framesDone_++;
        emit_signal("frame_finished", stats_);
        flush_cloud_notes();                  // one line for the frame's cloud loads
    }
    if (client_) { if (!qualityLoaded_) load_quality(); step_auto_quality(delta); }
    // Join after the first frame, so scripts loaded at startup (StarterPlayerScripts included)
    // exist before the player does, as on a live server.
    if (framesDone_ == 1 && autoJoin_ && client_ && !netClient_ && !localPlayerId_ && !join_) add_player(String::utf8(playerName_.c_str()), userId_);
    dispatch_join();
    step_trails(delta);                       // per render frame: the ribbons follow the drawn bodies
    step_beams();
    update_camera();
    if (!autoStep_ && jobs_.empty() && clientJobs_.empty()) return;
    const auto t1 = std::chrono::steady_clock::now();
    phase_.store("submitting");
    submit_frames();
    phase_.store("idle");
    hostSubmitMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count();
}

void PulseBlockzWorld::submit_frames() {
    pump_solids();
    pump_meshes();
    pump_images();
    pump_preloads();
    FrameIn sin, cin, up;                     // up: a Client world's report to its server
    sin.dt = cin.dt = (debugPausedServer_ || debugPausedClient_) ? 0.0 : pendingDt_;   // stopped in the debugger: no time passes for either side
    pendingDt_ = 0;
    FrameIn& toServer = server_ ? sin : up;
    snapshot_physics(toServer, client_ ? &cin : nullptr);
    // Play Solo: the local character's touches and its MoveToFinished fire on the client too,
    // as on Roblox -- the client simulates its own character.
    int64_t myRoot = 0, myHum = 0;
    if (server_ && client_ && localChar_) { auto cit = chars_.find(localChar_); if (cit != chars_.end()) { myRoot = cit->second.root; myHum = cit->second.humanoid; } }
    auto mine = [&](const HostEvent& e) {
        if (!myRoot) return false;
        if (e.id == myRoot || e.id == myHum) return true;
        for (auto& a : e.args) if (a.type == Value::Ref && (a.ref == myRoot || a.ref == myHum)) return true;
        return false;
    };
    for (auto& e : events_) {
        if (client_ && e.id < 0) cin.events.push_back(std::move(e));
        else if (server_) { if (mine(e)) cin.events.push_back(e); sin.events.push_back(std::move(e)); }
        else { cin.events.push_back(e); up.events.push_back(std::move(e)); }   // the local character's touches fire on both sides
    }
    events_.clear();
    touchedThisFrame_.clear();
    sin.jobs = std::move(jobs_);
    jobs_.clear();
    sin.httpAnswers = std::move(httpAnswers_);
    httpAnswers_.clear();
    sin.solidAnswers = std::move(solidAnswersServer_);
    solidAnswersServer_.clear();
    sin.meshAnswers = std::move(meshAnswersServer_);
    meshAnswersServer_.clear();
    sin.imageAnswers = std::move(imageAnswersServer_);
    imageAnswersServer_.clear();
    sin.preloadAnswers = std::move(preloadAnswersServer_);
    preloadAnswersServer_.clear();
    for (auto& w : netWrites_) sin.writes.push_back(std::move(w));
    netWrites_.clear();
    for (auto& w : assetWrites_) {              // both sides keep the clock behind Playing, and MeshSize
        if (server_ && w.id > 0) sin.writes.push_back(w);
        if (client_) cin.writes.push_back(std::move(w));
    }
    assetWrites_.clear();
    (server_ ? sin : up).remotes = std::move(toServerRemotes_);
    toServerRemotes_.clear();
    if (netClient_ && serverConnected_ && localPlayerId_) {
        // Poses mid-motion go unreliable-ordered; the first pose of a motion, the rest pose and
        // everything else go reliably. net_flush_clients makes the same split downstream.
        NetPacket rel; rel.type = NetPacket::ClientFrame;
        NetPacket fast; fast.type = NetPacket::ClientFrame;
        std::set<int64_t> movingNow;
        for (auto& w : up.writes) {
            if (poseProp(w.prop)) { movingNow.insert(w.id); (movingUp_.count(w.id) ? fast : rel).writes.push_back(std::move(w)); }
            else rel.writes.push_back(std::move(w));
        }
        for (int64_t id : movingUp_) if (!movingNow.count(id)) rest_pose(id, &rel.writes, nullptr);
        movingUp_ = std::move(movingNow);
        rel.events = std::move(up.events); rel.remotes = std::move(up.remotes);
        rel.edits = std::move(pendingEdits_); pendingEdits_.clear();
        if (!rel.writes.empty() || !rel.events.empty() || !rel.remotes.empty() || !rel.edits.empty()) net_send(1, rel, true);
        if (!fast.writes.empty()) net_send(1, fast, false);
    }
    if (client_) {
        cin.replication = std::move(toClientRep_);
        toClientRep_.clear();
        cin.remotes = std::move(toClientRemotes_);
        toClientRemotes_.clear();
        cin.jobs = std::move(clientJobs_);
        clientJobs_.clear();
        cin.solidAnswers = std::move(solidAnswersClient_);
        solidAnswersClient_.clear();
        cin.meshAnswers = std::move(meshAnswersClient_);
        meshAnswersClient_.clear();
        cin.imageAnswers = std::move(imageAnswersClient_);
        imageAnswersClient_.clear();
        cin.preloadAnswers = std::move(preloadAnswersClient_);
        preloadAnswersClient_.clear();
        const auto lt0 = std::chrono::steady_clock::now();
        layout_gui(cin.writes);
        const double layoutMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - lt0).count();
        if (layoutMs >= 50.0) UtilityFunctions::print(String("[World] a slow layout: ") + String::num_int64((int64_t)layoutMs) + " ms laying out " + String::num_int64((int64_t)guis_.size()) + " gui object(s)" + String::utf8(layoutNote_.c_str()));
        place_name_tags();
        if (camera_ && cameraId_) {
            Vec3 pos, orient;
            fromTransform(camera_->get_transform(), pos, orient);
            if (!near(pos, camSentPos, 1e-4f)) { cin.writes.push_back({cameraId_, "Position", Value::vector3(pos)}); camSentPos = pos; }
            if (!near(orient, camSentOrient, 1e-3f)) { cin.writes.push_back({cameraId_, "Orientation", Value::vector3(orient)}); camSentOrient = orient; }
            // Camera.ViewportSize: what Camera:ViewportPointToRay and the Mouse see as the screen.
            if (Viewport* vp = get_viewport()) {
                Vector2 size = vp->get_visible_rect().size;
                if (size != camSentViewport) { cin.writes.push_back({cameraId_, "ViewportSize", Value::vector2(size.x, size.y)}); camSentViewport = size; }
            }
        }
    }
    if (server_) server_->submit(std::move(sin));
    if (client_) client_->submit(std::move(cin));
    inFlight_ = true;
}

// What this world simulates. A part simulated elsewhere -- another machine's character -- is
// skipped: its pose comes from the tree instead.
void PulseBlockzWorld::snapshot_physics(FrameIn& server, FrameIn* client) {
    // A Client world reports its character to the server and writes it into its own tree, so
    // LocalScripts read the live pose rather than the echo. A quiet write stays on this machine:
    // never up the wire, and in Play Solo to the client runtime too, since replication skips it.
    auto put = [&](FrameIn& in, HostWrite w) {
        if (w.quiet) {
            // The mirror learns of everything else from the change log; this it is told directly.
            auto eit = entries_.find(w.id);
            if (eit != entries_.end()) eit->second.props[w.prop] = w.value;
        }
        if (client && &in != client && (netClient_ || w.quiet)) client->writes.push_back(w);
        if (w.quiet && netClient_) return;
        in.writes.push_back(std::move(w));
    };
    auto parentOf = [&](int64_t id) { auto it = entries_.find(id); return it == entries_.end() ? (int64_t)0 : it->second.parent; };
    for (auto& [id, p] : parts_) {
        if (!p.visible) continue;
        FrameIn& in = (client && id < 0) ? *client : server;
        Vec3 pos, orient, vel = p.vel, angVel = p.angVel;
        bool quiet = false;   // derived from what is already known: written for scripts, not logged, not sent
        if (p.role == ROLE_CHAR_ROOT) {
            if (!p.body || remote_char(parentOf(id))) continue;
            fromTransform(p.body->get_transform(), pos, orient);
            if (auto* cb = Object::cast_to<CharacterBody3D>(p.body)) vel = fromGd(cb->get_velocity());
            if (pos.y < fallenHeight_ && server_) {
                // Fell out of the world: the humanoid dies, the server respawns it.
                auto cit = chars_.find(parentOf(id));
                int64_t hid = cit == chars_.end() ? 0 : cit->second.humanoid;
                in.jobs.push_back([hid](Runtime& rt) { if (Instance* h = rt.dataModel().find(hid)) h->set("Health", Value::number(0)); });
            }
        } else if (p.role == ROLE_LIMB) {
            auto rit = parts_.find(p.attachedTo);
            if (!p.mesh || rit == parts_.end() || !rit->second.body) continue;
            // Limbs are derived even for a body somebody else simulates: the mesh follows the
            // root and the collision is one capsule, but Head.Position must still answer right
            // for a script or GetPartBoundsInRadius. p.offset is the limb's frame in the root,
            // kept current by the joints. Quiet, because a logged, replicated write for each of
            // a dressed character's sixty parts costs hundreds of packets a second per player.
            fromTransform(rit->second.body->get_transform() * p.offset, pos, orient);
            quiet = true;
        } else if (!p.rigid) {
            if (!p.body || netClient_ || weld_root(id) != id) continue;   // an anchored assembly's root: moved through any of its parts
            fromTransform(p.body->get_transform(), pos, orient);
        } else {
            auto* rb = Object::cast_to<RigidBody3D>(p.body);
            if (!rb) continue;
            fromTransform(rb->get_transform(), pos, orient);
            vel = fromGd(rb->get_linear_velocity());
            angVel = fromGd(rb->get_angular_velocity());
            if (pos.y < fallenHeight_) {
                // Workspace.FallenPartsDestroyHeight: the part is destroyed, as on Roblox.
                in.jobs.push_back([id](Runtime& rt) { if (Instance* i = rt.dataModel().find(id)) i->destroy(); });
                continue;
            }
        }
        if (!near(pos, p.sentPos, 1e-4f)) { put(in, {id, "Position", Value::vector3(pos), quiet}); p.sentPos = p.pos = pos; }
        if (!near(orient, p.sentOrient, 1e-3f)) { put(in, {id, "Orientation", Value::vector3(orient), quiet}); p.sentOrient = p.orient = orient; }
        if (!near(vel, p.sentVel, 1e-3f)) {
            put(in, {id, "AssemblyLinearVelocity", Value::vector3(vel)});
            // The last three sent, so an echo is recognised however late it arrives. See the
            // fromHost branch in apply_change.
            p.sentVelWasWas = p.sentVelWas; p.sentVelWas = p.sentVel;
            p.sentVel = p.vel = vel;
        }
        if (!near(angVel, p.sentAngVel, 1e-3f)) { put(in, {id, "AssemblyAngularVelocity", Value::vector3(angVel)}); p.sentAngVel = p.angVel = angVel; }
    }
    for (HostWrite& w : pendingWrites_) put(server, std::move(w));
    pendingWrites_.clear();
    for (auto& [model, ch] : chars_) {
        if (!ch.humanoid || remote_char(model)) continue;
        FrameIn& in = (client && ch.humanoid < 0) ? *client : server;
        if (!near(ch.moveDir, ch.sentMoveDir, 1e-3f)) { put(in, {ch.humanoid, "MoveDirection", Value::vector3(ch.moveDir)}); ch.sentMoveDir = ch.moveDir; }
        // Only ever written false. A write of true onto a true records no change, and a jump is
        // learned only from a change, so a true/false pair landing out of order leaves Jump
        // stuck true and the character can never jump again.
        if (ch.jumpConsumed) { put(in, {ch.humanoid, "Jump", Value::boolean(false)}); ch.jumpConsumed = false; }
    }
}

// A welded assembly is one body wearing every part's collision shape, so the contact's shape
// names the part actually hit: Touched reaches it, as on Roblox, not the assembly's root.
int64_t PulseBlockzWorld::part_of_shape(CollisionObject3D* co, int64_t shapeIndex, int64_t fallback) const {
    if (!co || shapeIndex < 0) return fallback;
    Object* owner = co->shape_owner_get_owner(co->shape_find_owner((int32_t)shapeIndex));
    if (!owner) return fallback;
    auto it = shapeToId_.find(owner->get_instance_id());
    return it == shapeToId_.end() ? fallback : it->second;
}

// How fast a thrown character may turn the direction it is travelling, in unit vector per
// second: about a quarter turn a second, enough to pick a landing but not to steer out of one.
static constexpr double AIR_TURN = 1.6;

// Roblox offers the same jump two ways and UseJumpPower says which: JumpPower is the launch
// speed, JumpHeight the height reached. v = sqrt(2 g h), so JumpHeight holds at any gravity.
double PulseBlockzWorld::jump_speed(const Character& ch) const {
    if (ch.useJumpPower) return ch.jumpPower;
    return std::sqrt(2.0 * gravity_ * std::max(0.0, ch.jumpHeight));
}

void PulseBlockzWorld::_on_body_shape_entered(const RID& rid, Node* body, int64_t bodyShape, int64_t localShape, int64_t id) {
    (void)rid;
    int64_t other = get_part_id(body);
    if (!other || !parts_.count(id)) return;
    auto* co = Object::cast_to<CollisionObject3D>(body);
    other = part_of_shape(co, bodyShape, other);
    id = part_of_shape(Object::cast_to<CollisionObject3D>(parts_[id].body), localShape, id);
    if (!parts_.count(id) || !parts_.count(other) || id == other) return;
    std::pair<int64_t, int64_t> key = std::minmax(id, other);
    if (!touchedThisFrame_.insert(key).second) return;   // both bodies monitoring: report the pair once
    // CanTouch false silences that part only; the other still hears the collision.
    if (parts_[id].canTouch) events_.push_back({id, "Touched", {Value::instance(other)}});
    if (parts_[other].canTouch) events_.push_back({other, "Touched", {Value::instance(id)}});
}
void PulseBlockzWorld::_on_body_shape_exited(const RID& rid, Node* body, int64_t bodyShape, int64_t localShape, int64_t id) {
    (void)rid;
    int64_t other = get_part_id(body);
    if (!other || !parts_.count(id)) return;
    other = part_of_shape(Object::cast_to<CollisionObject3D>(body), bodyShape, other);
    id = part_of_shape(Object::cast_to<CollisionObject3D>(parts_[id].body), localShape, id);
    if (!parts_.count(id) || !parts_.count(other) || id == other) return;
    events_.push_back({id, "TouchEnded", {Value::instance(other)}});
    events_.push_back({other, "TouchEnded", {Value::instance(id)}});
}

// ---- applying a frame's changes ----------------------------------------------------
void PulseBlockzWorld::finish_frame(FrameOut& out, bool server) {
    // Each phase timed; a frame over 100 ms prints its costliest phases once.
    std::vector<std::pair<const char*, double>> phases;
    const auto frameT0 = std::chrono::steady_clock::now();
    auto timed = [&](const char* name, auto&& fn) {
        const auto t0 = std::chrono::steady_clock::now();
        phase_.store(name);
        fn();
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if (ms >= 1.0) phases.push_back({name, ms});
    };
    timed("hull jobs", [&] { if (!hullJobs_.empty()) poll_hull_jobs(); });
    // Parts still waiting on a texture: asked again, so one arriving a frame late still lands.
    timed("late textures", [&] {
        if (awaitingTexture_.empty()) return;
        std::vector<int64_t> waiting(awaitingTexture_.begin(), awaitingTexture_.end());
        for (int64_t pid : waiting) {
            auto it = parts_.find(pid);
            if (it == parts_.end()) { awaitingTexture_.erase(pid); continue; }
            update_material(it->second);
        }
    });
    timed("animate_rigs", [&] { animate_rigs(); });
    timed("rebuild_assemblies", [&] { if (jointsDirty_) rebuild_assemblies(); });
    timed("refresh_all_visibility", [&] { if (visibilityDirty_) refresh_all_visibility(); });
    timed("rebuild_terrain", [&] { if (terrain_.dirty) rebuild_terrain(); });
    // A SurfaceAppearance or MaterialVariant changed: every part is re-dressed, because which
    // parts wear it is the question the changed thing answers.
    timed("update_material", [&] { if (materialsDirty_) { materialsDirty_ = false; for (auto& [id, part] : parts_) update_material(part); } });
    timed("update_lods", [&] { update_lods(); });
    timed("sync_gui", [&] { if (guiDirty_) sync_gui(); });
    // After the GUI: whether a Modal button is on screen is what sync_gui has just decided.
    timed("update_mouse_mode", [&] { if (mouseModeDirty_) { mouseModeDirty_ = false; update_mouse_mode(); } });
    timed("sync_shields", [&] { if (shieldsDirty_) sync_shields(); });
    timed("limb offsets", [&] { for (auto& [id, p] : parts_) if (p.offsetDirty && !p.tool && !p.worn) update_limb_offset(id, p); });
    timed("limb offsets", [&] { for (auto& [id, p] : parts_) if (p.offsetDirty) update_limb_offset(id, p); });
    timed("refresh_masses", [&] { if (massDirty_) refresh_masses(); });
    timed("sync_lights", [&] { if (lightsDirty_) sync_lights(); });
    const bool decalsAll = decalsDirty_, emittersAll = emittersDirty_;
    timed("sync_decals", [&] { if (decalsDirty_) sync_decals(); });
    timed("sync_emitters", [&] { if (emittersDirty_) sync_emitters(); });
    timed("resized parts", [&] {
        if (resizedParts_.empty()) return;
        if (!decalsAll) for (auto& [id, d] : decals_) if (d.node && resizedParts_.count(d.part)) style_decal(d);
        if (!emittersAll) for (auto& [id, e] : emitters_) if (e.node && resizedParts_.count(e.part)) style_emitter(e);
        resizedParts_.clear();
    });
    timed("sync_explosions", [&] { if (explosionsDirty_) sync_explosions(); });
    if (!constraints_.empty()) sync_constraints();
    timed("sync_highlights", [&] { if (highlightsDirty_) sync_highlights(); });
    timed("sync_effects", [&] { if (effectsDirty_) sync_effects(); });
    timed("sync_prompts", [&] { if (promptsDirty_) sync_prompts(); });
    timed("sync_sounds", [&] { if (soundsDirty_) sync_sounds(); });
    timed("sync_audio", [&] { if (audioDirty_) sync_audio(); else attenuate_voices(); });
    timed("report_loudness", [&] { report_loudness(assetWrites_); });   // a meter, so it goes out with the other engine-side reports
    timed("sync_hotbar", [&] { if (hotbarDirty_) sync_hotbar(); });
    timed("update_vehicle_hud", [&] { update_vehicle_hud(); });   // the number changes every frame, so it is not dirty-gated
    timed("sync_player_list", [&] { if (playerListDirty_) sync_player_list(); });
    timed("update_listener", [&] { if (client_) update_listener(); });
    timed("sync_health", [&] { if (client_) sync_health(); });
    timed("chat and notifications", [&] { if (client_ && !server) { if (!chatRoot_) sync_chat(); if (!topbarRoot_) sync_topbar(); for (auto& l : out.chat) chat_line(l); place_bubbles(); for (auto& n : out.notifications) notify(n); expire_notifications(); } });
    {
        const double total = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frameT0).count();
        if (total >= 100.0 && !server) {
            std::sort(phases.begin(), phases.end(), [](auto& a, auto& b) { return a.second > b.second; });
            std::string line = "[World] a slow frame: " + std::to_string((int)total) + " ms finishing " + std::to_string(out.changes.size()) + " change(s), the costly phases:";
            for (size_t r = 0; r < phases.size() && r < 6; r++) line += std::string(" ") + phases[r].first + " " + std::to_string((int)phases[r].second) + " ms;";
            line += " sync_gui parts: ensure " + std::to_string((int)guiEnsureMs_) + " ms, style " + std::to_string((int)guiStyleMs_) + " ms x" + std::to_string(guiStyled_) + ", frames " + std::to_string((int)guiFramesMs_) + " ms, order " + std::to_string((int)guiOrderMs_) + " ms, " + std::to_string(guis_.size()) + " objects;";
            auto lines = [](const char* what, std::map<int, int>& by) { std::string o; if (by.empty()) return o; o = std::string(" ") + what + " dirtied at line:"; for (auto& [ln, n] : by) o += " " + std::to_string(ln) + " x" + std::to_string(n) + ";"; return o; };
            line += lines("emitters", emitterDirtBy_) + lines("sounds", soundDirtBy_);
            if (!guiDirtBy_.empty()) {
                std::vector<std::pair<std::string, int>> rows(guiDirtBy_.begin(), guiDirtBy_.end());
                std::sort(rows.begin(), rows.end(), [](auto& a, auto& b) { return a.second > b.second; });
                line += " gui dirtied by:";
                for (size_t r = 0; r < rows.size() && r < 4; r++) line += " " + rows[r].first + " x" + std::to_string(rows[r].second) + ";";
            }
            UtilityFunctions::print(String::utf8(line.c_str()));
        }
        guiDirtBy_.clear(); emitterDirtBy_.clear(); soundDirtBy_.clear();
    }

    for (size_t k = 0; k < orphanSounds_.size();) {
        bool on = false;
        if (auto* a = Object::cast_to<AudioStreamPlayer>(orphanSounds_[k])) on = a->is_playing();
        else if (auto* a = Object::cast_to<AudioStreamPlayer3D>(orphanSounds_[k])) on = a->is_playing();
        if (on) { k++; continue; }
        orphanSounds_[k]->queue_free();
        orphanSounds_.erase(orphanSounds_.begin() + k);
    }
    for (const LogLine& l : out.logs) {
        const char* sig = l.level == LogLine::Print ? "script_print" : l.level == LogLine::Warn ? "script_warn"
                        : l.level == LogLine::Error ? "script_error" : "script_killed";
        emit_signal(sig, String::utf8(l.script.c_str()), String::utf8(l.text.c_str()));
    }
    if (server || netClient_) {   // the one runtime this world owns: its frame is the frame
        stats_ = Dictionary();
        stats_["millis"] = out.millis;
        stats_["step_millis"] = out.stats.lastStepMillis;
        stats_["now"] = out.now;
        stats_["threads_live"] = out.stats.threadsLive;
        stats_["scripts_started"] = out.stats.scriptsStarted;
        stats_["resumes"] = out.stats.resumes;
        stats_["errors"] = out.stats.errors;
        stats_["kills"] = out.stats.kills;
        stats_["skipped"] = out.stats.skipped;
        stats_["memory_now"] = (int64_t)out.stats.memoryNow;
        stats_["memory_peak"] = (int64_t)out.stats.memoryPeak;
        stats_["instances"] = (int64_t)entries_.size();
        stats_["parts"] = (int64_t)parts_.size();
        stats_["client_millis"] = server ? 0.0 : out.millis;
        stats_["changes"] = (int64_t)out.changes.size();
        stats_["client_threads_live"] = server ? 0 : out.stats.threadsLive;
    } else {
        stats_["client_millis"] = out.millis;
        stats_["client_threads_live"] = out.stats.threadsLive;
        stats_["errors"] = (int64_t)stats_["errors"] + (int64_t)out.stats.errors;
    }
}

// What the plugins built this frame, handed to the Studio as signals.
void PulseBlockzWorld::plugin_report(const FrameOut& out) {
    if (out.pluginButtonsChanged) {
        Array list;
        for (auto& b : out.pluginButtons) {
            Dictionary d;
            d["id"] = (int64_t)b.id;
            d["plugin"] = String::utf8(b.plugin.c_str());
            d["toolbar"] = String::utf8(b.toolbar.c_str());
            d["button"] = String::utf8(b.buttonId.c_str());
            d["tooltip"] = String::utf8(b.tooltip.c_str());
            d["icon"] = String::utf8(b.icon.c_str());
            d["text"] = String::utf8(b.text.c_str());
            d["active"] = b.active;
            d["enabled"] = b.enabled;
            list.push_back(d);
        }
        emit_signal("plugin_buttons_changed", list);
    }
    if (out.selectionRequested) {
        PackedInt64Array ids;
        for (int64_t id : out.selection) ids.push_back(id);
        emit_signal("plugin_selection_requested", ids);
    }
    for (auto& s : out.pluginSettings) emit_signal("plugin_setting_changed", String::utf8(s.plugin.c_str()), String::utf8(s.key.c_str()), String::utf8(s.json.c_str()));
    if (out.pluginActiveChanged) emit_signal("plugin_activated", String::utf8(out.activePlugin.c_str()), out.pluginActive, out.pluginExclusive);
    for (auto& [id, line] : out.openScripts) emit_signal("plugin_open_script", (int64_t)id, line);
}

// A plugin's script lives under PluginDebugService; its edits to the place -- not to CoreGui or
// to that service's own tree -- are the Studio's to undo.
bool PulseBlockzWorld::plugin_script(int64_t script) const {
    if (!script || !editMode_) return false;
    for (int64_t a = script, hops = 0; a != kNoParent && hops < 64; hops++) {
        auto e = entries_.find(a);
        if (e == entries_.end()) return false;
        if (e->second.className == "PluginDebugService") return true;
        a = e->second.parent;
    }
    return false;
}
bool PulseBlockzWorld::in_place(int64_t id) const {
    for (int64_t a = id, hops = 0; hops < 64; hops++) {
        if (a == 0) return true;                       // reached the DataModel
        if (a == kNoParent) return false;
        auto e = entries_.find(a);
        if (e == entries_.end()) return false;
        if (e->second.className == "PluginDebugService" || e->second.className == "CoreGui") return false;
        a = e->second.parent;
    }
    return false;
}

void PulseBlockzWorld::plugin_add(const String& name, const String& source) {
    std::string n = toStd(name), s = toStd(source);
    queue_job([n, s](Runtime& rt) { rt.addPlugin(n, s); });
}
void PulseBlockzWorld::plugin_add_file(const String& file, const String& source) {
    std::string f = toStd(file), s = toStd(source);
    queue_job([f, s](Runtime& rt) { rt.addPluginFile(f, s); });
}
void PulseBlockzWorld::plugin_unload_all() { queue_job([](Runtime& rt) { rt.unloadPlugins(); }); }
void PulseBlockzWorld::plugin_click(int64_t button) { queue_job([button](Runtime& rt) { rt.pluginButtonClick(button); }); }
void PulseBlockzWorld::plugin_selection(const PackedInt64Array& ids) {
    std::vector<int64_t> v(ids.ptr(), ids.ptr() + ids.size());
    queue_job([v](Runtime& rt) { rt.setSelection(v); });
}
void PulseBlockzWorld::plugin_setting(const String& plugin, const String& key, const String& json) {
    std::string p = toStd(plugin), k = toStd(key), j = toStd(json);
    queue_job([p, k, j](Runtime& rt) { rt.setPluginSetting(p, k, j); });
}
// The pointer over the Studio view, as PluginMouse's Move / Button1Down / ...; Hit and Target
// are cast along the ray passed in.
void PulseBlockzWorld::plugin_input(const Ref<InputEvent>& event, Vector3 origin, Vector3 direction) {
    Runtime::UserInput in;
    if (auto* mb = Object::cast_to<InputEventMouseButton>(event.ptr())) {
        Vector2 p = mb->get_position();
        in.position = {(float)p.x, (float)p.y, 0};
        switch (mb->get_button_index()) {
        case MOUSE_BUTTON_LEFT: in.type = "MouseButton1"; break;
        case MOUSE_BUTTON_RIGHT: in.type = "MouseButton2"; break;
        case MOUSE_BUTTON_WHEEL_UP: in.type = "MouseWheel"; in.position.z = 1; break;
        case MOUSE_BUTTON_WHEEL_DOWN: in.type = "MouseWheel"; in.position.z = -1; break;
        default: return;
        }
        if (in.type == "MouseWheel") { if (!mb->is_pressed()) return; in.state = "Change"; }
        else in.state = mb->is_pressed() ? "Begin" : "End";
    } else if (auto* mm = Object::cast_to<InputEventMouseMotion>(event.ptr())) {
        Vector2 p = mm->get_position();
        in.type = "MouseMovement";
        in.state = "Change";
        in.position = {(float)p.x, (float)p.y, 0};
    } else return;
    Vec3 o{(float)origin.x, (float)origin.y, (float)origin.z}, d{(float)direction.x, (float)direction.y, (float)direction.z};
    queue_job([in, o, d](Runtime& rt) { rt.pluginInput(in, o, d); });
}

// EditableMeshes changed this frame: every MeshPart whose MeshId is rbxobject://<id> is rebuilt
// from the new geometry.
void PulseBlockzWorld::editable_mesh_report(const FrameOut& out, bool mirrored) {
    for (const auto& f : out.editableMeshes) {
        if (!mirrored && f.id > 0) continue;   // Play Solo: the server's, not the client's placeholder for it
        editableMeshes_[f.id] = editable_to_mesh(f.pos, f.nrm, f.uv, f.rgba);
        const std::string key = "rbxobject://" + std::to_string((long long)f.id);
        for (auto& [pid, p] : parts_) if (p.meshId == key) { update_shape(p); update_material(p); }
    }
}

// An EditableImage drawn into this frame: its texture is updated in place, so every ImageLabel
// showing it (ImageContent) follows without a restyle.
void PulseBlockzWorld::editable_report(const FrameOut& out, bool mirrored) {
    for (const auto& f : out.editableImages) {
        // Play Solo draws the server's tree, so only client-made (negative) ids come from the
        // client: its placeholder carries the server image's id and would paint over it.
        if (!mirrored && f.id > 0) continue;
        if (f.w <= 0 || f.h <= 0 || f.rgba.size() < (size_t)f.w * f.h * 4) continue;
        PackedByteArray bytes;
        bytes.resize((int64_t)f.rgba.size());
        std::memcpy(bytes.ptrw(), f.rgba.data(), f.rgba.size());
        EditableTex& t = editableTex_[f.id];
        Ref<Image> img = Image::create_from_data(f.w, f.h, false, Image::FORMAT_RGBA8, bytes);
        if (img.is_null()) continue;
        const std::string key = "rbxobject://" + std::to_string((long long)f.id);
        if (t.texture.is_null()) { t.image = img; t.texture = ImageTexture::create_from_image(img); note_texture("editable image", f.w, f.h); touch_asset(key); }
        else if (t.image.is_null()) { t.image = img; t.texture->set_image(img); touch_asset(key); }
        else if (t.image.is_valid() && t.image->get_width() == f.w && t.image->get_height() == f.h) { t.image = img; t.texture->update(img); }
        else { t.image = img; t.texture->set_image(img); touch_asset(key); }
    }
}

void PulseBlockzWorld::apply_server(FrameOut& out) {
    // Server only. A client that asked is already refused in the runtime; this is the second
    // lock on the same door.
    for (const HttpAsk& ask : out.httpAsks) start_http(ask);
    for (const SolidAsk& ask : out.solidAsks) start_solid(ask, true);
    for (const MeshAsk& ask : out.meshAsks) meshJobs_.push_back({ask.id, ask.uri, true, ask.geometry});
    for (const ImageAsk& ask : out.imageAsks) imageJobs_.push_back({ask.id, ask.uri, true});
    for (const PreloadAsk& ask : out.preloadAsks) preloadJobs_.push_back({ask.id, ask.uri, true});
    debug_report(out, true);
    plugin_report(out);
    editable_report(out, true);
    editable_mesh_report(out, true);
    bool joining = join_ && join_->done && !join_->sent;   // its Character is among this frame's changes
    if (joining) {
        if (join_->peer == 0) localPlayerId_ = join_->playerId;
        else if (auto it = clients_.find(join_->peer); it != clients_.end()) it->second.playerId = join_->playerId;
    }
    for (const Change& c : out.changes) {
        apply_change(c);
        if (c.id == localPlayerId_ && localPlayerId_) {
            if (c.kind == Change::Property && c.name == "Character") localChar_ = c.value.ref;
            else if (c.kind == Change::Destroy) { localPlayerId_ = 0; localChar_ = 0; }
        }
        if (c.kind == Change::Property && c.name == "Character" && !clients_.empty())
            for (auto& [peer, nc] : clients_)
                if (nc.playerId == c.id && nc.playerId) {
                    remoteChars_.erase(nc.charId);
                    nc.charId = c.value.ref;
                    if (nc.charId) remoteChars_.insert(nc.charId);
                }
    }
    // a waypoint closes the undo entry over the edits applied above
    for (auto& w : out.pluginWaypoints) emit_signal("plugin_waypoint", String::utf8(w.c_str()));
    if (joining) {
        // The joiner gets the world as of the join; everyone else gets what the frame
        // replicated before the snapshot.
        if (client_ && localPlayerId_ && join_->peer != 0) for (Change& c : join_->before) toClientRep_.push_back(c);
        for (auto& [peer, nc] : clients_)
            if (nc.joined && peer != join_->peer) for (Change& c : join_->before) nc.rep.push_back(c);
        if (join_->peer == 0) {
            toClientRep_ = std::move(join_->snapshot);
            auto j = join_;
            clientJobs_.push_back([j](Runtime& rt) {
                DataModel& dm = rt.dataModel();
                Instance::Ptr cam = dm.create("Camera", dm.workspace());   // before the starter scripts run
                cam->setName("Camera");
                dm.workspace()->set("CurrentCamera", Value::instance(cam->id()));
                j->cameraId = cam->id();
                rt.addPlayer(j->name, j->userId, j->playerId);
            });
            join_->sent = true;
            emit_signal("player_joined", String::utf8(join_->name.c_str()), localPlayerId_);
            localNonce_ = new_sign_in_nonce();
            call_deferred("emit_signal", "sign_in_requested", String::utf8(localNonce_.c_str()));
        } else {
            auto it = clients_.find(join_->peer);
            if (it != clients_.end() && join_->playerId) {
                NetPacket pk; pk.type = NetPacket::Welcome;
                pk.playerId = join_->playerId;
                pk.changes = std::move(join_->snapshot);
                it->second.nonce = pk.nonce = new_sign_in_nonce();
                // Which place this is: the client fetches its own checked copy, since the code
                // itself never goes on the wire.
                pk.place = placeUri_;
                net_send(join_->peer, pk);
                it->second.joined = true;
                emit_signal("client_joined", String::utf8(it->second.name.c_str()), join_->playerId, (int64_t)join_->peer);
            } else if (it != clients_.end()) {
                peer_->disconnect_peer(join_->peer);   // the server refused the Player (name in use)
            } else if (join_->playerId) {
                // Gone before the welcome: remove the Player its join made, as for a leaver.
                int64_t pid = join_->playerId;
                queue_job([pid](Runtime& rt) { if (Instance* p = rt.dataModel().find(pid)) if (p->isA("Player")) rt.removePlayer(p); });
            }
            join_.reset();
        }
    }
    if (client_ && !netClient_) {
        for (const Change& c : out.replication) toClientRep_.push_back(c);
        for (const RemoteMsg& m : out.remotes)
            if (m.player == 0 || m.player == localPlayerId_) toClientRemotes_.push_back(m);
    }
    for (auto& [peer, nc] : clients_) {
        if (!nc.joined) continue;
        // Never a client's own reported pose back to it: the echo re-applies its motion and
        // re-flags it airborne. fromHost is the test -- a pose a server script WROTE is not
        // fromHost and must arrive, or knockback would never reach anybody.
        int64_t ownRoot = 0, ownHumanoid = 0;
        if (auto cit = chars_.find(nc.charId); cit != chars_.end()) {
            ownRoot = cit->second.root;
            ownHumanoid = cit->second.humanoid;
        }
        for (const Change& c : out.replication) {
            if (ownRoot && c.id == ownRoot && c.kind == Change::Property && c.fromHost
                && poseProp(c.name)) continue;
            // Its own inputs, by the same test: a server script setting Humanoid.Jump is not
            // fromHost and still arrives.
            if (ownHumanoid && c.id == ownHumanoid && c.kind == Change::Property && c.fromHost
                && ownInputProp(c.name)) continue;
            nc.rep.push_back(c);
        }
        for (const RemoteMsg& m : out.remotes)
            if (m.player == 0 || m.player == nc.playerId) nc.remotes.push_back(m);
    }
    net_flush_clients();
    apply_terrain_ops(out.terrain);
    apply_impulses(out.impulses);
    finish_frame(out, true);
}

void PulseBlockzWorld::apply_client(FrameOut& out) {
    // A LocalScript's boolean operations are done here too, answered back to the client.
    for (const SolidAsk& ask : out.solidAsks) start_solid(ask, false);
    for (const MeshAsk& ask : out.meshAsks) meshJobs_.push_back({ask.id, ask.uri, false, ask.geometry});
    for (const ImageAsk& ask : out.imageAsks) imageJobs_.push_back({ask.id, ask.uri, false});
    for (const PreloadAsk& ask : out.preloadAsks) preloadJobs_.push_back({ask.id, ask.uri, false});
    debug_report(out, false);
    editable_report(out, netClient_);
    editable_mesh_report(out, netClient_);
    // Timed per class and property, printed once when the frame runs over 100 ms.
    std::map<std::string, std::pair<double, int>> slow;
    const auto frameStart = std::chrono::steady_clock::now();
    for (const Change& c : out.changes) {
        if (netClient_) {
            const auto t0 = std::chrono::steady_clock::now();
            phase_.store(c.kind == Change::Create ? "applying a create" : c.kind == Change::Destroy ? "applying a destroy" : "applying a property");
            phaseId_.store(c.id);
            apply_change(c);                                                       // the whole scene comes from here
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            // Only a long change or an already busy second builds strings.
            const bool busy = c.remote && ++wireCount_ >= 1500;   // the wire's changes; this side's own writes come back through here too
            if (ms >= 0.5 || busy) {
                std::string what;
                auto eit = entries_.find(c.id);
                std::string cls = eit != entries_.end() ? eit->second.className : std::string("?");
                if (c.kind == Change::Create) what = "create " + c.className;
                else if (c.kind == Change::Destroy) what = "destroy " + cls;
                else if (c.kind == Change::Property) what = cls + "." + c.name;
                else what = cls + " (other)";
                if (ms >= 0.5) { auto& row = slow[what]; row.first += ms; row.second++; }
                if (busy) wireTally_[what]++;
            }
            if (c.id == localPlayerId_ && localPlayerId_) {
                if (c.kind == Change::Property && c.name == "Character") localChar_ = c.value.ref;
                else if (c.kind == Change::Destroy) { localPlayerId_ = 0; localChar_ = 0; }
            }
        }
        // UserInputService does not replicate and the branch below takes Creates only for
        // client-made (negative) ids, so without this uisId_ stays zero and every MouseBehavior
        // a LocalScript writes lands on an unwatched id.
        else if (c.kind == Change::Create && c.className == "UserInputService") uisId_ = c.id;
        else if (c.id < 0) apply_change(c);                                        // client-made: the client's own
        // A place's client half asking the host for something, over attributes named "@Ask...".
        // The host mirrors the SERVER's tree, and in Play Solo the server's own `Request`
        // channel is read off this same mirror, so client writes cannot be mirrored wholesale.
        else if (c.kind == Change::Property && !c.fromHost && c.name.rfind("@Ask", 0) == 0) apply_change(c);
        // A Humanoid's StateName, whoever owns it and whoever wrote it. The clause below takes
        // humanoid properties only for the LOCAL character, so a remote body's state would never
        // arrive, and guessing it from vertical speed reads "falling" for anyone standing still.
        else if (c.kind == Change::Property && c.name == "StateName") apply_change(c);
        else if (c.kind == Change::Property && !c.fromHost && (visualProp(c.name) || local_humanoid(c.id) || (poseProp(c.name) && local_root(c.id)) || c.id == lightingId_ || c.id == soundServiceId_ || c.id == materialServiceId_ || c.id == uisId_ || c.id == starterGuiId_ || c.name == "HotbarSlot" || guis_.count(c.id) || lights_.count(c.id) || prompts_.count(c.id) || sounds_.count(c.id) || dataMeshes_.count(c.id) || effects_.count(c.id) || joints_.count(c.id) || decals_.count(c.id) || emitters_.count(c.id) || attachments_.count(c.id) || explosions_.count(c.id) || highlights_.count(c.id) || constraints_.count(c.id))) apply_change(c);
        if (cameraId_ && c.id == cameraId_ && c.kind == Change::Property && !c.fromHost) {
            if (c.name == "CameraType") cameraType_ = enumName(c.value);
            else if (c.name == "CameraSubject") cameraSubject_ = c.value.type == Value::Ref ? c.value.ref : 0;
            else if (c.name == "FieldOfView") { if (camera_) camera_->set_fov((float)c.value.n); }
            else if (c.name == "Position" || c.name == "Orientation") {
                (c.name == "Position" ? camSentPos : camSentOrient) = c.value.v;
                if (camera_ && cameraType_ == "Scriptable") camera_->set_transform(toTransform(camSentPos, camSentOrient));
            }
        }
    }
    {
        const double total = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frameStart).count();
        if (total >= 100.0 && !slow.empty()) {
            std::vector<std::pair<std::string, std::pair<double, int>>> rows(slow.begin(), slow.end());
            std::sort(rows.begin(), rows.end(), [](auto& a, auto& b) { return a.second.first > b.second.first; });
            std::string line = "[World] a slow frame: " + std::to_string((int)total) + " ms applying " + std::to_string(out.changes.size()) + " change(s), the costly ones:";
            for (size_t r = 0; r < rows.size() && r < 6; r++)
                line += " " + rows[r].first + " " + std::to_string((int)rows[r].second.first) + " ms x" + std::to_string(rows[r].second.second) + ";";
            UtilityFunctions::print(String::utf8(line.c_str()));
        }
        const auto now = std::chrono::steady_clock::now();
        if (now - wireSince_ >= std::chrono::seconds(1)) {
            if (wireCount_ >= 1500) {
                std::vector<std::pair<std::string, int>> rows(wireTally_.begin(), wireTally_.end());
                std::sort(rows.begin(), rows.end(), [](auto& a, auto& b) { return a.second > b.second; });
                std::string line = "[World] busy wire: " + std::to_string(wireCount_) + " change(s) over the wire this second; past the first 1500, most were:";
                for (size_t r = 0; r < rows.size() && r < 5; r++) line += " " + rows[r].first + " x" + std::to_string(rows[r].second) + ";";
                UtilityFunctions::print(String::utf8(line.c_str()));
            }
            wireTally_.clear(); wireCount_ = 0; wireSince_ = now;
        }
    }
    if (join_ && join_->sent && join_->cameraId) { cameraId_ = join_->cameraId; join_.reset(); }
    for (RemoteMsg& m : out.remotes) { m.player = localPlayerId_; toServerRemotes_.push_back(std::move(m)); }
    finish_frame(out, false);
}

void PulseBlockzWorld::apply_change(const Change& c) {
    switch (c.kind) {
    case Change::Create: {
        Entry& e = entries_[c.id];
        e.className = c.className; e.name = c.name; e.parent = c.parent;
        kids_add(c.id, c.parent);
        e.props.clear();
        // (not a service the plugin's GetService brought into being: a child of the DataModel itself)
        if (c.parent != kNoParent && c.parent != 0 && plugin_script(c.script) && in_place(c.parent))
            emit_signal("plugin_edit", "create", (int64_t)c.id, String::utf8(c.name.c_str()), Variant(), (int64_t)c.parent);
        e.made = ++madeCount_;
        treeVersion_++;
        if (!highlights_.empty()) highlightsDirty_ = true;          // it may land under an Adornee
        if (c.className == "Highlight") { highlights_[c.id]; highlightsDirty_ = true; }
        if (c.parent != kNoParent) visibilityDirty_ = true;
        if (c.className == "Workspace") workspaceId_ = c.id;
        if (c.className == "Lighting") lightingId_ = c.id;
        if (c.className == "MaterialService") materialServiceId_ = c.id;
        // Both runtimes make one in Play Solo. A negative id is client-made and wins; the
        // server's stands in only until a client one turns up.
        if (c.className == "UserInputService" && (c.id < 0 || !uisId_)) uisId_ = c.id;
        if (c.className == "Mouse" && c.id < 0) mouseId_ = c.id;   // the client's own (the server's is nobody's pointer)
        if (c.className == "SoundService" && (c.id < 0 || !soundServiceId_)) soundServiceId_ = c.id;   // the client's ear
        if (c.className == "UserGameSettings" && c.id < 0) userGameSettingsId_ = c.id;   // the person's, so the client's
        if (c.className == "StarterGui") starterGuiId_ = c.id;
        if (c.className == "CoreGui") coreGuiId_ = c.id;
        if (c.className == "Chat") chatServiceId_ = c.id;
        if (c.className == "Humanoid" && c.parent != kNoParent) { chars_[c.parent].humanoid = c.id; replay_humanoid(c.id); }
        if (c.className == "Player") { players_[c.id]; playerListDirty_ = true; }
        if (c.className == "Team") { teams_[c.id] = {0xF2 / 255.f, 0xF3 / 255.f, 0xF3 / 255.f}; playerListDirty_ = true; }
        if (leaderstat(c.id)) playerListDirty_ = true;
        const ClassDef* cls = findClass(c.className);
        if (cls && (cls->isA("GuiBase2d") || cls->isA("UIComponent"))) {
            Gui& g = guis_[c.id];
            g.kind = cls->isA("ScreenGui") || cls->isA("PluginGui") ? GUI_SCREEN : cls->isA("BillboardGui") ? GUI_BILLBOARD : cls->isA("SurfaceGui") ? GUI_SURFACE : cls->isA("GuiObject") ? GUI_OBJECT : cls->isA("UIComponent") ? GUI_MODIFIER : GUI_OTHER;
            g.button = cls->isA("GuiButton");
            g.text = cls->isA("TextLabel") || cls->isA("TextButton") || cls->isA("TextBox");
            g.box = cls->isA("TextBox");
            g.scrolling = cls->isA("ScrollingFrame");
            g.image = cls->isA("ImageLabel") || cls->isA("ImageButton");
            guiDirty_ = true; guiOrderDirty_ = true; guiDirtBy_["create " + c.className]++;
        }
        if (cls && cls->isA("Terrain")) {
            // Terrain is a BasePart by class but has no Size or Position and a shape of its
            // own, so it gets no Part struct and nothing draws a brick or a box for it.
            terrain_.id = c.id;
            terrain_.dirty = true;
        } else if (cls && cls->isA("BasePart")) {
            Part& p = parts_[c.id];
            p.className = c.className;
            p.id = c.id;
            if (c.className == "SpawnLocation") { p.anchored = true; p.size = {12, 1, 12}; }
        }
        if (cls && cls->isA("Light")) {
            Light& l = lights_[c.id];
            l.className = c.className;
            if (c.className != "PointLight") l.range = 16;
            lightsDirty_ = true;
        }
        if (cls && cls->isA("ProximityPrompt")) { prompts_[c.id]; promptsDirty_ = true; }
        if (cls && cls->isA("Decal")) { decals_[c.id].tiled = c.className == "Texture"; decalsDirty_ = true; }
        if (cls && (cls->isA("ParticleEmitter") || cls->isA("Fire") || cls->isA("Smoke") || cls->isA("Sparkles"))) {
            emitters_[c.id].className = c.className;
            dirty_emitters(__LINE__);
        }
        if (c.className == "Explosion") { explosions_[c.id]; explosionsDirty_ = true; }
        if (c.className == "Trail") trails_[c.id];
        if (c.className == "Beam") beams_[c.id];
        if (c.className == "NoCollisionConstraint") noCollide_[c.id];
        if (cls && (cls->isA("Constraint") || cls->isA("BodyMover") || cls->isA("Rotate") || cls->isA("DynamicRotate"))) {
            Constraint& k = constraints_[c.id];
            k.className = c.className;
            k.rotateJoint = cls->isA("Rotate") || cls->isA("DynamicRotate");   // the old free hinge: a door on its post
            if (c.className == "LineForce") k.maxForce = std::numeric_limits<double>::infinity();   // Roblox's default, uncapped
            constraintsDirty_ = true;
            if (cls->isA("BodyMover")) {   // Roblox's defaults, for what a file or script leaves unsaid
                if (c.className == "BodyGyro") { k.maxTorqueV = {400000, 0, 400000}; k.pGain = 3000; k.dGain = 500; }
                else if (c.className == "BodyVelocity") { k.vel = {0, 2, 0}; k.pGain = 1250; }
                else if (c.className == "BodyPosition") { k.position = {0, 50, 0}; k.pGain = 10000; k.dGain = 1250; }
                else if (c.className == "BodyAngularVelocity") { k.angVel = {0, 2, 0}; k.pGain = 1250; }
                else k.force = {0, 1, 0};
            }
        }
        if (cls && cls->isA("Attachment")) attachments_[c.id] = Transform3D();
        if (c.className == "AnimationTrack") tracks_[c.id];
        if (c.className == "Animation") animationIds_[c.id];
        if (c.className == "Keyframe") keyframes_[c.id] = 0;
        if (c.className == "Pose") poses_[c.id];
        if (cls && (cls->isA("PostEffect") || cls->isA("Atmosphere") || cls->isA("Sky"))) {
            Effect& fx = effects_[c.id];
            fx.className = c.className;
            std::vector<const PropDef*> defs; cls->collectProps(defs);
            for (const PropDef* p : defs) fx.props[p->name] = p->def;
            effectsDirty_ = true;
        }
        // A SurfaceAppearance hangs under the MeshPart it dresses; a MaterialVariant sits under
        // MaterialService and is worn by name.
        if (c.className == "SurfaceAppearance" || c.className == "MaterialVariant") {
            Effect& fx = effects_[c.id];
            fx.className = c.className;
            std::vector<const PropDef*> defs; cls->collectProps(defs);
            for (const PropDef* p : defs) fx.props[p->name] = p->def;
            materialsDirty_ = true;
        }
        if (c.className == "ForceField") shieldsDirty_ = true;
        if (c.className == "Accessory" || c.className == "Accoutrement" || c.className == "Hat") mounts_[c.id];
        if (cls && cls->isA("Sound")) { sounds_[c.id]; dirty_sounds(__LINE__); }
        if (audio_class(c.className)) {
            AudioNode& a = audio_[c.id];
            a.className = c.className;
            if (cls) { std::vector<const PropDef*> defs; cls->collectProps(defs); for (const PropDef* p : defs) a.props[p->name] = p->def; }
            audioDirty_ = true;
        }
        // A SoundEffect's properties are kept here and the bus is rebuilt when any of them
        // move; a Sound with no effect keeps the default bus.
        if (cls && cls->isA("SoundEffect")) {
            Effect& fx = effects_[c.id];
            fx.className = c.className;
            std::vector<const PropDef*> defs; cls->collectProps(defs);
            for (const PropDef* p : defs) fx.props[p->name] = p->def;
            dirty_sounds(__LINE__);
        }
        if (c.className == "SoundGroup") { soundGroups_[c.id] = {1.0, 0}; dirty_sounds(__LINE__); }
        if (cls && (cls->isA("JointInstance") || cls->isA("WeldConstraint")) && !cls->isA("Rotate") && !cls->isA("DynamicRotate")) { joints_[c.id].className = c.className; jointsDirty_ = true; }
        if (cls && cls->isA("DataModelMesh")) {
            DataMesh& m = dataMeshes_[c.id];
            m.type = c.className == "BlockMesh" ? "Brick" : c.className == "CylinderMesh" ? "Cylinder" : "Head";
            attach_data_mesh(c.id, m, c.parent);
        }
        break;
    }
    case Change::Parent: {
        auto it = entries_.find(c.id);
        if (it == entries_.end()) break;
        if (!highlights_.empty()) highlightsDirty_ = true;
        if (leaderstat(c.id)) playerListDirty_ = true;
        const int64_t wasUnder = it->second.parent;
        it->second.parent = c.parent;
        kids_move(c.id, wasUnder, c.parent);
        // (a service GetService brought into being arrives as a move onto the DataModel: not an edit)
        if (plugin_script(c.script) && c.parent != 0 && wasUnder != 0 && (in_place(wasUnder) || in_place(c.parent)))
            emit_signal("plugin_edit", "parent", (int64_t)c.id, String::utf8(it->second.name.c_str()), (int64_t)wasUnder, (int64_t)c.parent);
        if (leaderstat(c.id)) playerListDirty_ = true;
        visibilityDirty_ = true;
        treeVersion_++;
        if (auto git = guis_.find(c.id); git != guis_.end()) { guiDirty_ = true; git->second.dirty = true; guiOrderDirty_ = true; guiDirtBy_["reparent"]++; }
        if (lights_.count(c.id) || attachments_.count(c.id)) lightsDirty_ = true;
        if (decals_.count(c.id)) decalsDirty_ = true;
        if (emitters_.count(c.id) || attachments_.count(c.id)) dirty_emitters(__LINE__);
        if (explosions_.count(c.id)) explosionsDirty_ = true;
        if (!constraints_.empty()) constraintsDirty_ = true;
        if (auto fit = effects_.find(c.id); fit != effects_.end()) {
            effectsDirty_ = true;
            if (fit->second.className.find("SoundEffect") != std::string::npos) dirty_sounds(__LINE__);
        }
        if (!joints_.empty() && (joints_.count(c.id) || parts_.count(c.id))) jointsDirty_ = true;
        if (prompts_.count(c.id)) promptsDirty_ = true;
        if (hotbarSlots_.count(c.id)) hotbarDirty_ = true;       // in hand or back in the Backpack
        if (players_.count(c.id) || teams_.count(c.id)) playerListDirty_ = true;
        if (auto sit = sounds_.find(c.id); sit != sounds_.end()) { dirty_sounds(__LINE__); if (c.parent == kNoParent) sound_removed(c.id, sit->second); }
        if (!audio_.empty() && (audio_.count(c.id) || attachments_.count(c.id) || parts_.count(c.id))) audioDirty_ = true;
        if (auto mit = dataMeshes_.find(c.id); mit != dataMeshes_.end()) attach_data_mesh(c.id, mit->second, c.parent);
        if (it->second.className == "Humanoid") {
            for (auto ch = chars_.begin(); ch != chars_.end();) ch = ch->second.humanoid == c.id ? chars_.erase(ch) : std::next(ch);
            // Parented at last: replay the properties written while it had no parent.
            if (c.parent != kNoParent) { chars_[c.parent].humanoid = c.id; replay_humanoid(c.id); }
        }
        break;
    }
    case Change::Destroy: {
        frameCams_.erase(c.id);
        editableTex_.erase(c.id);
        if (opaqueImages_.erase(c.id)) { textures_.erase("rbxopaque://" + std::to_string((long long)c.id)); forget_mesh("rbxopaque://" + std::to_string((long long)c.id)); }
        editableMeshes_.erase(c.id);
        if (!highlights_.empty()) highlightsDirty_ = true;
        if (auto hit = highlights_.find(c.id); hit != highlights_.end()) { for (auto* n : hit->second.nodes) if (UtilityFunctions::is_instance_valid(n)) { if (n->get_parent()) n->get_parent()->remove_child(n); n->queue_free(); } highlights_.erase(hit); }
        auto pit = parts_.find(c.id);
        if (pit != parts_.end()) {
            if (weldRoot_.count(c.id)) jointsDirty_ = true;
            free_part(c.id, pit->second); parts_.erase(pit);
            // A touch the physics saw this frame on a part just destroyed would reach
            // Touched as nil, so it is dropped.
            events_.erase(std::remove_if(events_.begin(), events_.end(), [&](const HostEvent& e) {
                if (e.id == c.id) return true;
                for (const Value& a : e.args) if (a.type == Value::Ref && a.ref == c.id) return true;
                return false;
            }), events_.end());
        }
        chars_.erase(c.id);
        for (auto ch = chars_.begin(); ch != chars_.end();) ch = ch->second.humanoid == c.id ? chars_.erase(ch) : std::next(ch);
        if (auto git = guis_.find(c.id); git != guis_.end()) { if (git->second.button) mouseModeDirty_ = true; free_gui(git->second); guis_.erase(git); guiDirty_ = true; guiOrderDirty_ = true; if (typingBox_ == c.id) typingBox_ = 0; }
        if (auto lit = lights_.find(c.id); lit != lights_.end()) { if (lit->second.node) lit->second.node->queue_free(); lights_.erase(lit); }
        if (auto dit = decals_.find(c.id); dit != decals_.end()) { if (dit->second.node) dit->second.node->queue_free(); decals_.erase(dit); }
        if (auto eit2 = emitters_.find(c.id); eit2 != emitters_.end()) {
            if (eit2->second.node) eit2->second.node->queue_free();
            if (eit2->second.burst) eit2->second.burst->queue_free();
            emitters_.erase(eit2);
        }
        if (attachments_.erase(c.id)) emittersDirty_ = lightsDirty_ = true;
        tracks_.erase(c.id); animationIds_.erase(c.id); keyframes_.erase(c.id); poses_.erase(c.id);
        if (soundGroups_.erase(c.id)) dirty_sounds(__LINE__);
        if (auto xit = explosions_.find(c.id); xit != explosions_.end()) { if (xit->second.node) xit->second.node->queue_free(); explosions_.erase(xit); }
        if (auto tit = trails_.find(c.id); tit != trails_.end()) { if (tit->second.node) tit->second.node->queue_free(); trails_.erase(tit); }
        if (auto bit = beams_.find(c.id); bit != beams_.end()) { if (bit->second.node) bit->second.node->queue_free(); beams_.erase(bit); }
        if (auto kit = constraints_.find(c.id); kit != constraints_.end()) { if (kit->second.node) { remove_child(kit->second.node); kit->second.node->queue_free(); } constraints_.erase(kit); }
        if (auto nit = noCollide_.find(c.id); nit != noCollide_.end()) { no_collide_clear(nit->second); noCollide_.erase(nit); }
        if (!constraints_.empty() && (attachments_.count(c.id) || parts_.count(c.id))) constraintsDirty_ = true;
        if (auto fit = effects_.find(c.id); fit != effects_.end()) {
            // Read before erasing: afterwards there is nothing to ask.
            const bool wasSound = fit->second.className.find("SoundEffect") != std::string::npos;
            const bool wasSurface = fit->second.className == "SurfaceAppearance" || fit->second.className == "MaterialVariant";
            effects_.erase(fit);
            effectsDirty_ = true;
            if (wasSound) dirty_sounds(__LINE__);
            // Taking a SurfaceAppearance off puts the plain surface back; otherwise its maps
            // stay on the part.
            if (wasSurface) materialsDirty_ = true;
        }
        mounts_.erase(c.id);
        if (shields_.count(c.id)) shieldsDirty_ = true;
        if (joints_.erase(c.id)) jointsDirty_ = true;
        if (auto mit = dataMeshes_.find(c.id); mit != dataMeshes_.end()) { attach_data_mesh(c.id, mit->second, kNoParent); dataMeshes_.erase(mit); }
        if (auto pit = prompts_.find(c.id); pit != prompts_.end()) { if (pit->second.node) pit->second.node->queue_free(); prompts_.erase(pit); }
        if (auto sit = sounds_.find(c.id); sit != sounds_.end()) { sound_removed(c.id, sit->second); if (sit->second.node) sit->second.node->queue_free(); sounds_.erase(sit); }
        if (audio_.erase(c.id)) audioDirty_ = true;
        if (!audio_.empty() && parts_.count(c.id)) {
            // a voice under this part's mesh goes with it; the next sync re-places it
            for (auto& [key, v] : voices_) if (v.part == c.id) { v.node = nullptr; v.playing = false; }
            audioDirty_ = true;
        }
        if (hotbarSlots_.erase(c.id)) hotbarDirty_ = true;
        if (players_.erase(c.id) | teams_.erase(c.id) | (leaderstat(c.id) ? 1 : 0)) playerListDirty_ = true;
        statValues_.erase(c.id);
        grips_.erase(c.id);
        if (auto de = entries_.find(c.id); de != entries_.end()) kids_drop(c.id, de->second.parent);
        kids_.erase(c.id);
        entries_.erase(c.id);
        humanoidProps_.erase(c.id);
        treeVersion_++;
        if (c.id == localChar_) localChar_ = 0;
        break;
    }
    case Change::Property: {
        // The string half of a Content pair (Image beside ImageContent) is the Content's echo;
        // replayed after the Content it would blank an object back to "".
        const PropDef* twinDef = nullptr;
        if (!forwardingContent_ && !c.name.empty() && c.name[0] != '@' && c.name[0] != '#')
            if (auto tit = entries_.find(c.id); tit != entries_.end())
                if (const ClassDef* tcls = findClass(tit->second.className))
                    if (const PropDef* td = tcls->findProp(c.name); td && !td->twin.empty()) twinDef = td;
        if (plugin_script(c.script) && in_place(c.id) && !(twinDef && c.value.type != Value::Content)) {
            Variant before;   // the tracked value, else the class default (never written since it was made)
            if (auto tit = entries_.find(c.id); tit != entries_.end()) {
                if (auto pit = tit->second.props.find(c.name); pit != tit->second.props.end()) before = toVariant(pit->second);
                else if (const ClassDef* cls = findClass(tit->second.className)) if (const PropDef* pd = cls->findProp(c.name)) before = toVariant(pd->def);
            }
            emit_signal("plugin_edit", "property", (int64_t)c.id, String::utf8(c.name.c_str()), before, toVariant(c.value));
        }
        if (trackProps_) if (auto tit = entries_.find(c.id); tit != entries_.end()) tit->second.props[c.name] = c.value;
        // A DataModelContent's pixels, kept for the Opaque Contents that name it; whatever was
        // waiting on them is drawn again.
        if (auto tit = entries_.find(c.id); tit != entries_.end() && tit->second.className == "DataModelContent") {
            OpaqueImage& o = opaqueImages_[c.id];
            if (c.name == "Size" && c.value.type == Value::Vector2) { o.w = (int)c.value.v.x; o.h = (int)c.value.v.y; }
            else if (c.name == "Kind" && c.value.type == Value::String) o.kind = c.value.s;
            else if (c.name == "Data" && c.value.type == Value::String) o.rgba = c.value.s;
            CloudAsset arrived;
            arrived.ids.insert("rbxopaque://" + std::to_string((long long)c.id));
            cloud_arrived(arrived);
            break;
        }
        // Everything below was written against the older string twin (Image, Texture,
        // TextureID...), so a Content is re-applied under the twin's name as a key load_texture
        // understands: its uri, or rbxobject://<id> for an EditableImage. The string half itself
        // reads "" for an object and is skipped, or it would blank what the Content shows.
        if (twinDef) {
            {
                if (c.value.type != Value::Content) break;
                Change asString = c;
                asString.name = twinDef->twin;
                asString.script = 0;
                asString.value = Value::string(content_key(c.value));
                bool track = trackProps_;
                trackProps_ = false;
                forwardingContent_ = true;
                apply_change(asString);
                forwardingContent_ = false;
                trackProps_ = track;
                break;
            }
        }
        // The branches below each claim one kind of instance and break, so a rename is answered
        // ahead of them or it would never reach the mirror.
        if (c.id == terrain_.id && terrain_.id) {
            // The blob in the tree is what the place keeps; decoded unless it is the one just
            // handed over from here.
            if (c.name == "Heights" && c.value.type == Value::String && c.value.s != terrain_.sent) terrain_decode(c.value.s);
            else if (c.name == "SmoothGrid" && c.value.type == Value::String && !c.value.s.empty()) terrain_decode_smoothgrid(c.value.s);
            else if (c.name == "MaterialColors" && c.value.type == Value::String) terrain_apply_material_colors(c.value.s);
            else if (c.name == "WaterColor" && c.value.type == Value::Color3) { terrain_.waterColor = Color(c.value.c.r, c.value.c.g, c.value.c.b); terrain_style_water(); }
            else if (c.name == "WaterTransparency" && c.value.type == Value::Number) { terrain_.waterTransparency = (float)c.value.n; terrain_style_water(); }
            else if (c.name == "WaterReflectance" && c.value.type == Value::Number) { terrain_.waterReflectance = (float)c.value.n; terrain_style_water(); }
            else if (c.name == "WaterWaveSize" && c.value.type == Value::Number) { terrain_.waveSize = (float)c.value.n; terrain_style_water(); }
            else if (c.name == "WaterWaveSpeed" && c.value.type == Value::Number) { terrain_.waveSpeed = (float)c.value.n; terrain_style_water(); }
            else if (c.name == "Color" && c.value.type == Value::Color3) {
                terrain_.color = Color(c.value.c.r, c.value.c.g, c.value.c.b);
                terrain_.dirty = true;
            } else if (c.name == "Material" || c.name == "Transparency") terrain_.dirty = true;
        }
        if (c.name == "Name") {
            treeVersion_++;
            if (auto nit = entries_.find(c.id); nit != entries_.end()) { nit->second.name = c.value.s; visibilityDirty_ = true; }   // a rename can make/unmake a HumanoidRootPart
            if (hotbarSlots_.count(c.id)) hotbarDirty_ = true;
            if (players_.count(c.id) || teams_.count(c.id) || leaderstat(c.id)) playerListDirty_ = true;
            break;
        }
        if (c.id == workspaceId_) {
            if (c.name == "Gravity") {
                gravity_ = c.value.n;
                for (auto& [id, p] : parts_)
                    if (auto* rb = Object::cast_to<RigidBody3D>(p.body)) rb->set_gravity_scale(gravity_ / engineGravity_);
            } else if (c.name == "FallenPartsDestroyHeight") fallenHeight_ = c.value.n;
            break;
        }
        if (c.id == lightingId_) { apply_lighting(c); break; }
        if (c.id == soundServiceId_) { apply_sound_service(c); break; }
        if (c.id == materialServiceId_) {
            // <Material>Name: the base material override SetBaseMaterialOverride keeps there.
            if (c.name.size() > 4 && c.name.compare(c.name.size() - 4, 4, "Name") == 0 && c.value.type == Value::String) {
                materialOverrides_[c.name.substr(0, c.name.size() - 4)] = c.value.s;
                materialsDirty_ = true;
            }
            break;
        }
        // A Camera a ViewportFrame may look through: any Camera but the client's own, which
        // drives the screen and is handled in apply_client.
        if ((c.name == "Position" || c.name == "Orientation" || c.name == "FieldOfView") && c.id != cameraId_) {
            if (auto eit = entries_.find(c.id); eit != entries_.end() && eit->second.className == "Camera") {
                FrameCam& fc = frameCams_[c.id];
                if (c.name == "Position" && c.value.type == Value::Vector3) fc.pos = c.value.v;
                else if (c.name == "Orientation" && c.value.type == Value::Vector3) fc.orient = c.value.v;
                else if (c.name == "FieldOfView" && c.value.type == Value::Number) fc.fov = c.value.n;
                break;
            }
        }
        if (auto fit = effects_.find(c.id); fit != effects_.end()) {
            fit->second.props[c.name] = c.value;
            effectsDirty_ = true;
            // A SoundEffect shares this map but is rebuilt by sync_sounds, on its own flag.
            if (fit->second.className.find("SoundEffect") != std::string::npos) dirty_sounds(__LINE__);
            // SurfaceAppearance and MaterialVariant too: they dress parts, not the environment.
            if (fit->second.className == "SurfaceAppearance" || fit->second.className == "MaterialVariant") materialsDirty_ = true;
            break;
        }
        if (auto jit = joints_.find(c.id); jit != joints_.end()) {
            Joint& j = jit->second;
            const std::string& n = c.name;
            auto cf = [&](Transform3D& t, bool pos) { Vec3 p, o; fromTransform(t, p, o); if (pos) p = c.value.v; else o = c.value.v; t = toTransform(p, o); };
            if (n == "Part0") { j.part0 = c.value.type == Value::Ref ? c.value.ref : 0; j.captured = false; }
            else if (n == "Part1") { j.part1 = c.value.type == Value::Ref ? c.value.ref : 0; j.captured = false; }
            else if (n == "Enabled") { j.enabled = c.value.b; if (!j.enabled) j.captured = false; }
            else if (n == "C0Position") cf(j.c0, true);
            else if (n == "C0Orientation") cf(j.c0, false);
            else if (n == "C1Position") cf(j.c1, true);
            else if (n == "C1Orientation") cf(j.c1, false);
            else if (n == "CurrentAngle") j.angle = c.value.n;
            else if (n == "TransformPosition") { cf(j.transform, true); j.animatedByTrack = false; }
            else if (n == "TransformOrientation") { cf(j.transform, false); j.animatedByTrack = false; }
            else break;
            jointsDirty_ = true;
            break;
        }
        if (c.name == "Visible") if (auto e = entries_.find(c.id); e != entries_.end() && e->second.className == "ForceField") {
            e->second.props["Visible"] = c.value;
            shieldsDirty_ = true;
            break;
        }
        if (auto mit = mounts_.find(c.id); mit != mounts_.end() && c.value.type == Value::Vector3) {
            WornMount& m = mit->second;
            if (c.name == "AttachmentPos") { m.pos = toGd(c.value.v); m.stated = true; }
            else if (c.name == "AttachmentForward") { m.forward = toGd(c.value.v); m.stated = true; }
            else if (c.name == "AttachmentRight") { m.right = toGd(c.value.v); m.stated = true; }
            else if (c.name == "AttachmentUp") { m.up = toGd(c.value.v); m.stated = true; }
            else break;
            // Every part of it, because the Handle carries the rest.
            for (int64_t pid : parts_under(c.id)) if (Part& part = parts_[pid]; part.worn) part.offsetDirty = true;
            break;
        }
        if (c.id == uisId_) {
            if (c.name == "MouseIcon" && c.value.type == Value::String) { uisMouseIcon_ = c.value.s; apply_mouse_icon(uisMouseIcon_.empty() ? mouseIcon_s_ : uisMouseIcon_); }
            else apply_input_settings(c);
            break;
        }
        if (c.id == userGameSettingsId_ && userGameSettingsId_) {
            if (c.name == "RotationType") cameraRelative_ = c.value.s == "CameraRelative";
            break;
        }
        if (c.id == mouseId_ && mouseId_) {
            if (c.name == "Icon" && c.value.type == Value::String) { mouseIcon_s_ = c.value.s; if (uisMouseIcon_.empty()) apply_mouse_icon(mouseIcon_s_); }
            break;
        }
        if (c.id == starterGuiId_) {
            if (c.name == "CoreGuiHidden") { coreGuiHidden_ = (int)c.value.n; hotbarDirty_ = playerListDirty_ = true; sync_chat(); sync_topbar(); }
            else if (c.name == "TopbarEnabled") { topbarEnabled_ = c.value.b; sync_topbar(); }
            else if (c.name == "ChatActive") { chatActive_ = c.value.b; sync_chat(); }
            break;
        }
        if (c.id == chatServiceId_) { if (c.name == "BubbleChatEnabled") bubbleChat_ = c.value.b; break; }
        if (auto plit = players_.find(c.id); plit != players_.end()) {
            PlayerInfo& pi = plit->second;
            if (c.name == "DisplayName") pi.displayName = c.value.s;
            else if (c.name == "Team") pi.team = c.value.ref;
            else if (c.name == "TeamColor") pi.teamColor = c.value.c;
            else if (c.name == "Neutral") pi.neutral = c.value.b;
            else if (c.name == "Character") pi.character = c.value.ref;
            else if (c.name == "DevComputerMovementMode") pi.movementMode = c.value.s;
            else if (c.name == "CameraMinZoomDistance") pi.minZoom = c.value.n;
            else if (c.name == "CameraMaxZoomDistance") pi.maxZoom = c.value.n;
            playerListDirty_ = true;
        }
        if (auto tit = teams_.find(c.id); tit != teams_.end()) { if (c.name == "TeamColor") tit->second = c.value.c; playerListDirty_ = true; }
        if (c.name == "HotbarSlot") {
            int slot = (int)c.value.n;
            if (slot > 0) hotbarSlots_[c.id] = slot; else hotbarSlots_.erase(c.id);
            hotbarDirty_ = true;
            break;
        }
        auto eit = entries_.find(c.id);
        // A GUI property from anywhere, except the measurements layout_gui writes back: they
        // change nothing about the drawing, and a name tag over a walking player would report
        // new TextBounds every frame, each costing a full GUI sync. The test is the name, not
        // fromHost, which covers the whole of DataModel::apply and so stamps a LocalScript's
        // own redraw inside a replicated change too.
        bool ownWriteback = c.fromHost
            && (c.name == "AbsolutePosition" || c.name == "AbsoluteSize" || c.name == "AbsoluteRotation"
                || c.name == "TextBounds" || c.name == "TextFits");
        if (auto git = guis_.find(c.id); git != guis_.end() && !ownWriteback) {
            Gui& g = git->second;
            g.props[c.name] = c.value; guiDirty_ = true; g.dirty = true;
            // Inherited by everything under it, so everything is styled again.
            if (c.name == "@PixelFont") for (auto& [gid, other] : guis_) other.dirty = true;
            { auto eit = entries_.find(c.id); guiDirtBy_[(eit != entries_.end() ? eit->second.className + " " + eit->second.name : std::string("?")) + "." + c.name]++; }
            if (c.name == "ZIndex") guiOrderDirty_ = true;
            // A modifier is read by its parent, so the parent is what needs laying out again.
            if (auto pe = entries_.find(c.id); pe != entries_.end())
                if (auto pg = guis_.find(pe->second.parent); pg != guis_.end()) pg->second.dirty = true;
            // A script writing Text / CursorPosition; the engine's own writes come back
            // fromHost and are not echoed into the box.
            if (!c.fromHost && box_node(g) && c.name == "Text" && c.value.type == Value::String) {
                if (box_text(g) != c.value.s) set_box_text(g, c.value.s, (int)c.value.s.size() + 1);
            } else if (!c.fromHost && box_node(g) && c.name == "CursorPosition" && c.value.type == Value::Number) {
                Control* box = box_node(g);
                if (c.value.n < 1) { if (box->has_focus()) box->release_focus(); }
                else { if (!box->has_focus()) box->grab_focus(); set_box_text(g, box_text(g), (int)c.value.n); }
            } else if (!c.fromHost && g.scrolling && c.name == "CanvasPosition" && c.value.type == Value::Vector2) {
                scroll_to(g, Vector2(c.value.v.x, c.value.v.y));   // a script scrolling it
            }
        }
        if (c.name.rfind("Grip", 0) == 0 && eit != entries_.end() && eit->second.className == "Tool") {
            Grip& g = grips_[c.id];
            if (c.name == "GripPos") g.pos = toGd(c.value.v); else if (c.name == "GripForward") g.forward = toGd(c.value.v);
            else if (c.name == "GripRight") g.right = toGd(c.value.v); else if (c.name == "GripUp") g.up = toGd(c.value.v);
            for (int64_t pid : parts_under(c.id)) if (Part& p = parts_[pid]; p.tool) p.offsetDirty = true;
            break;
        }
        if (c.name == "Value" && eit != entries_.end() && statClass(eit->second.className)) {
            statValues_[c.id] = c.value;
            if (leaderstat(c.id)) playerListDirty_ = true;
            break;
        }
        if (eit != entries_.end() && eit->second.className == "Humanoid") { apply_humanoid(c); break; }
        if (auto lit = lights_.find(c.id); lit != lights_.end()) { apply_light(lit->second, c); break; }
        if (auto hit = highlights_.find(c.id); hit != highlights_.end()) {
            Highlight& h = hit->second;
            if (c.name == "Adornee") h.adornee = c.value.ref;
            else if (c.name == "Enabled") h.enabled = c.value.b;
            else if (c.name == "DepthMode") h.onTop = enumName(c.value) != "Occluded";
            else if (c.name == "FillColor") h.fill = c.value.c;
            else if (c.name == "OutlineColor") h.outline = c.value.c;
            else if (c.name == "FillTransparency") h.fillT = (float)c.value.n;
            else if (c.name == "OutlineTransparency") h.outlineT = (float)c.value.n;
            highlightsDirty_ = true;
            break;
        }
        if (auto nit = noCollide_.find(c.id); nit != noCollide_.end()) {
            NoCollide& n = nit->second;
            if (c.name == "Part0") n.p0 = c.value.type == Value::Ref ? c.value.ref : 0;
            else if (c.name == "Part1") n.p1 = c.value.type == Value::Ref ? c.value.ref : 0;
            else if (c.name == "Enabled") n.enabled = c.value.b;
        }
        if (auto kit = constraints_.find(c.id); kit != constraints_.end()) {
            Constraint& k = kit->second;
            const std::string& n = c.name;
            auto ref = [&]() { return c.value.type == Value::Ref ? c.value.ref : 0; };
            if (n == "Attachment0") k.att0 = ref();
            else if (n == "Attachment1") k.att1 = ref();
            else if (n == "Part0") k.part0 = ref();
            else if (n == "Part1") k.part1 = ref();
            else if (n == "C0Position") k.c0Pos = c.value.v;
            else if (n == "C0Orientation") k.c0Ori = c.value.v;
            else if (n == "C1Position") k.c1Pos = c.value.v;
            else if (n == "C1Orientation") k.c1Ori = c.value.v;
            else if (n == "Enabled") k.enabled = c.value.b;
            else if (n == "LimitsEnabled") k.limits = c.value.b;
            else if (n == "TwistLimitsEnabled") k.twistLimits = c.value.b;
            else if (n == "ActuatorType") k.actuator = enumName(c.value);
            else if (n == "AngularActuatorType") k.angularActuator = enumName(c.value);
            else if (n == "AngularLimitsEnabled") k.angularLimits = c.value.b;
            else if (n == "Velocity" && c.value.type == Value::Vector3) k.vel = c.value.v;   // a BodyVelocity's
            else if (n == "Velocity") k.velocity = c.value.n;
            else if (n == "MotorMaxForce") k.motorMaxForce = c.value.n;
            else if (n == "Speed") k.speed = c.value.n;
            else if (n == "ServoMaxForce") k.servoMaxForce = c.value.n;
            else if (n == "TargetPosition") k.targetPosition = c.value.n;
            else if (n == "LowerLimit") k.lowerLimit = c.value.n;
            else if (n == "UpperLimit") k.upperLimit = c.value.n;
            else if (n == "AngularVelocity" && c.value.type == Value::Number) k.angularVelocity = c.value.n;
            else if (n == "MotorMaxTorque") k.motorMaxTorque = c.value.n;
            else if (n == "AngularSpeed") k.angularSpeed = c.value.n;
            else if (n == "ServoMaxTorque") k.servoMaxTorque = c.value.n;
            else if (n == "TargetAngle") k.targetAngle = c.value.n;
            else if (n == "LowerAngle") k.lower = c.value.n;
            else if (n == "UpperAngle") k.upper = c.value.n;
            else if (n == "TwistLowerAngle") k.twistLower = c.value.n;
            else if (n == "TwistUpperAngle") k.twistUpper = c.value.n;
            else if (n == "Length") k.length = c.value.n;
            else if (n == "Restitution") k.restitution = c.value.n;
            else if (n == "Stiffness") k.stiffness = c.value.n;
            else if (n == "Damping") k.damping = c.value.n;
            else if (n == "FreeLength") k.freeLength = c.value.n;
            else if (n == "MinLength") k.minLength = c.value.n;
            else if (n == "MaxLength") k.maxLength = c.value.n;
            else if (n == "MaxForce" && c.value.type == Value::Vector3) k.maxForceV = c.value.v;   // a BodyMover's, per axis
            else if (n == "MaxForce") k.maxForce = c.value.n;
            else if (n == "WinchEnabled") k.winch = c.value.b;
            else if (n == "WinchTarget") k.winchTarget = c.value.n;
            else if (n == "WinchSpeed") k.winchSpeed = c.value.n;
            else if (n == "Force") k.force = c.value.v;
            else if (n == "Torque") k.torque = c.value.v;
            else if (n == "VectorVelocity") k.vel = c.value.v;
            else if (n == "AngularVelocity") k.angVel = c.value.v;
            else if (n == "Position") k.position = c.value.v;
            else if (n == "LineDirection") k.lineDir = c.value.v;
            else if (n == "LineVelocity") k.lineVelocity = c.value.n;
            else if (n == "PlaneVelocity") { k.planeVel[0] = c.value.v.x; k.planeVel[1] = c.value.v.y; }
            else if (n == "PrimaryTangentAxis") k.tangent0 = c.value.v;
            else if (n == "SecondaryTangentAxis") k.tangent1 = c.value.v;
            else if (n == "CFramePosition") k.goalPos = c.value.v;
            else if (n == "CFrameOrientation") k.goalOrient = c.value.v;
            else if (n == "RelativeTo") k.relativeTo = enumName(c.value);
            else if (n == "VelocityConstraintMode") k.velocityMode = enumName(c.value);
            else if (n == "Mode") k.twoAttachment = enumName(c.value) == "TwoAttachment";
            else if (n == "RigidityEnabled") k.rigidity = c.value.b;
            else if (n == "ApplyAtCenterOfMass") k.atCenterOfMass = c.value.b;
            else if (n == "PrimaryAxisOnly") k.primaryAxisOnly = c.value.b;
            else if (n == "MaxTorque" && c.value.type == Value::Vector3) k.maxTorqueV = c.value.v;
            else if (n == "MaxTorque") k.maxTorque = c.value.n;
            else if (n == "P") k.pGain = c.value.n;
            else if (n == "D") k.dGain = c.value.n;
            else if (n == "Location") k.location = c.value.v;
            else if (n == "MaxVelocity") k.maxVelocity = c.value.n;
            else if (n == "MaxAxesForce" && c.value.type == Value::Vector3) k.maxAxesForce = c.value.v;
            else if (n == "ForceLimitMode") k.forceLimitMode = enumName(c.value);
            else if (n == "ForceRelativeTo") k.relativeTo = enumName(c.value);
            else if (n == "ReactionForceEnabled" || n == "ReactionTorqueEnabled") k.reaction = c.value.b;
            else if (n == "MotorMaxAcceleration" || n == "MotorMaxAngularAcceleration") k.motorMaxAccel = c.value.n;
            else if (n == "AngularRestitution") k.angularRestitution = c.value.n;
            else if (n == "Magnitude") k.magnitude = c.value.n;
            else if (n == "InverseSquareLaw") k.inverseSquare = c.value.b;
            else if (n == "InclinationAngle") k.inclination = c.value.n;
            else if (n == "MaxFrictionTorque") k.maxTorque = c.value.n;
            else if (n == "WinchForce") k.winchForce = c.value.n;
            else if (n == "WinchResponsiveness") k.winchResponsiveness = c.value.n;
            else if (n == "LimitAngle0") k.limitAngle0 = c.value.n;
            else if (n == "LimitAngle1") k.limitAngle1 = c.value.n;
            else if (n == "MaxAngularVelocity") k.maxAngularVelocity = c.value.n;
            else if (n == "Responsiveness") k.responsiveness = c.value.n;
            else if (n == "DesiredAngle") k.desiredAngle = c.value.n;
            else if (n == "BaseAngle") k.baseAngle = c.value.n;
            else break;
            constraintsDirty_ = true;
            break;
        }
        if (auto tit = trails_.find(c.id); tit != trails_.end()) {
            Trail& t = tit->second;
            if (c.name == "Attachment0") t.att0 = c.value.type == Value::Ref ? c.value.ref : 0;
            else if (c.name == "Attachment1") t.att1 = c.value.type == Value::Ref ? c.value.ref : 0;
            else if (c.name == "Enabled") t.enabled = c.value.b;
            else if (c.name == "Lifetime") t.lifetime = std::max(0.01, c.value.n);
            else if (c.name == "MinLength") t.minLength = std::max(0.0, c.value.n);
            else if (c.name == "MaxLength") t.maxLength = std::max(0.0, c.value.n);
            else if (c.name == "Brightness") t.brightness = c.value.n;
            else if (c.name == "Color" && c.value.type == Value::ColorSequence) t.colorSeq = c.value;
            else if (c.name == "Transparency" && c.value.type == Value::NumberSequence) t.transSeq = c.value;
            else if (c.name == "WidthScale" && c.value.type == Value::NumberSequence) t.widthScale = c.value;
            else if (c.name == "Texture") t.texture = c.value.s;
            else if (c.name == "TextureLength") t.textureLength = std::max(0.01, c.value.n);
            else if (c.name == "TextureMode") t.textureMode = enumName(c.value) == "Wrap" ? 1 : enumName(c.value) == "Static" ? 2 : 0;
            else if (c.name == "FaceCamera") t.faceCamera = c.value.b;
            else if (c.name == "LocalTransparencyModifier") t.localT = c.value.n;
            else if (c.name == "ClearTick" && c.value.n > 0) { t.pts.clear(); if (t.node) t.node->set_visible(false); }   // Clear()
        }
        if (auto bit = beams_.find(c.id); bit != beams_.end()) {
            Beam& b = bit->second;
            if (c.name == "Attachment0") b.att0 = c.value.type == Value::Ref ? c.value.ref : 0;
            else if (c.name == "Attachment1") b.att1 = c.value.type == Value::Ref ? c.value.ref : 0;
            else if (c.name == "Enabled") b.enabled = c.value.b;
            else if (c.name == "FaceCamera") b.faceCamera = c.value.b;
            else if (c.name == "Width0") b.width0 = std::max(0.0, c.value.n);
            else if (c.name == "Width1") b.width1 = std::max(0.0, c.value.n);
            else if (c.name == "CurveSize0") b.curve0 = c.value.n;
            else if (c.name == "CurveSize1") b.curve1 = c.value.n;
            else if (c.name == "Segments") b.segments = (int)std::clamp(c.value.n, 1.0, 200.0);
            else if (c.name == "Brightness") b.brightness = c.value.n;
            else if (c.name == "Color" && c.value.type == Value::ColorSequence) b.colorSeq = c.value;
            else if (c.name == "Transparency" && c.value.type == Value::NumberSequence) b.transSeq = c.value;
            else if (c.name == "Texture") b.texture = c.value.s;
            else if (c.name == "TextureLength") b.textureLength = std::max(0.01, c.value.n);
            else if (c.name == "TextureSpeed") b.textureSpeed = c.value.n;
            else if (c.name == "TextureMode") b.textureMode = enumName(c.value) == "Wrap" ? 1 : enumName(c.value) == "Static" ? 2 : 0;
            else if (c.name == "LocalTransparencyModifier") b.localT = c.value.n;
            else if (c.name == "TextureOffset") { b.textureOffset = c.value.n; b.offsetAt = (double)Time::get_singleton()->get_ticks_msec() / 1000.0; }   // SetTextureOffset
        }
        if (auto xit = explosions_.find(c.id); xit != explosions_.end()) {
            Explosion& x = xit->second;
            if (c.name == "Position") x.pos = c.value.v;
            else if (c.name == "BlastRadius") x.radius = (float)c.value.n;
            else if (c.name == "BlastPressure") x.pressure = (float)c.value.n;
            else if (c.name == "Visible") x.visible = c.value.b;
            explosionsDirty_ = true;
            break;
        }
        if (auto emit = emitters_.find(c.id); emit != emitters_.end()) {
            Emitter& e = emit->second;
            if (c.name == "EmitBurst") { if (c.value.n > 0) burst_emitter(e, (int)c.value.n); else if (c.value.n < 0 && e.node) { e.node->restart(); style_emitter(e); if (e.burst) e.burst->set_emitting(false); } break; }
            e.props[c.name] = c.value;
            style_emitter(e);
            break;
        }
        if (auto git2 = soundGroups_.find(c.id); git2 != soundGroups_.end()) {
            if (c.name == "Volume") git2->second.first = c.value.n;
            else if (c.name == "SoundGroup") git2->second.second = c.value.type == Value::Ref ? c.value.ref : 0;
            else break;
            for (auto& [sid, snd] : sounds_) style_sound(snd);   // everything in it follows
            break;
        }
        if (auto tit = tracks_.find(c.id); tit != tracks_.end()) {
            Track& t = tit->second;
            if (c.name == "Animation") { t.animation = c.value.ref; t.resolved = false; }
            else if (c.name == "IsPlaying") { t.playing = c.value.b; if (t.playing) t.lastKeyframe.clear(); }
            else if (c.name == "TimePosition") { if (c.fromHost) t.sentTime = c.value.n; else t.time = c.value.n; }
            else if (c.name == "Speed") t.speed = c.value.n;
            else if (c.name == "WeightCurrent") { if (!c.fromHost) t.weight = c.value.n; }
            else if (c.name == "WeightTarget") t.weightTarget = c.value.n;
            else if (c.name == "FadeTime") t.fade = c.value.n;
            else if (c.name == "Stopping") t.stopping = c.value.b;
            else if (c.name == "Priority") {
                const std::string n = enumName(c.value);
                t.priority = n == "Idle" ? 0 : n == "Movement" ? 1 : n == "Action" ? 2 : n == "Action2" ? 3
                           : n == "Action3" ? 4 : n == "Action4" ? 5 : n == "Core" ? 1000 : 2;
            }
            else if (c.name == "Looped") t.looped = c.value.b;
            else if (c.name == "Length") t.length = c.value.n;
            break;
        }
        if (auto ait2 = animationIds_.find(c.id); ait2 != animationIds_.end()) {
            if (c.name == "AnimationId") { ait2->second = c.value.s; for (auto& [tid, t] : tracks_) t.resolved = false; }
            break;
        }
        if (auto kit = keyframes_.find(c.id); kit != keyframes_.end()) {
            if (c.name == "Time") kit->second = c.value.n;
            break;
        }
        if (auto pit2 = poses_.find(c.id); pit2 != poses_.end()) {
            Vec3 pos, ori;
            fromTransform(pit2->second.cframe, pos, ori);
            if (c.name == "CFramePosition") pit2->second.cframe = toTransform(c.value.v, ori);
            else if (c.name == "CFrameOrientation") pit2->second.cframe = toTransform(pos, c.value.v);
            else if (c.name == "Weight") pit2->second.weight = c.value.n;
            break;
        }
        if (auto ait = attachments_.find(c.id); ait != attachments_.end()) {
            if (c.name == "Position" || c.name == "Orientation") {
                Vec3 pos, ori;
                fromTransform(ait->second, pos, ori);
                if (c.name == "Position") pos = c.value.v; else ori = c.value.v;
                ait->second = toTransform(pos, ori);
                dirty_emitters(__LINE__);
                lightsDirty_ = true;
                if (!constraints_.empty()) constraintsDirty_ = true;
            }
            break;
        }
        if (auto dit = decals_.find(c.id); dit != decals_.end()) {
            Decal& d = dit->second;
            if (c.name == "Face") d.face = enumName(c.value);
            else if (c.name == "Texture") d.texture = c.value.s;
            else if (c.name == "NormalMap") d.normalMap = c.value.s;
            else if (c.name == "MetalnessMap") d.metalnessMap = c.value.s;
            else if (c.name == "RoughnessMap") d.roughnessMap = c.value.s;
            else if (c.name == "Transparency") d.transparency = (float)c.value.n;
            else if (c.name == "LocalTransparencyModifier") d.localT = (float)c.value.n;
            else if (c.name == "UVOffset" && c.value.type == Value::Vector2) d.uvOffset = c.value.v2();
            else if (c.name == "UVScale" && c.value.type == Value::Vector2) d.uvScale = c.value.v2();
            else if (c.name == "Color3") d.color = c.value.c;
            else if (c.name == "StudsPerTileU") d.studsU = (float)c.value.n;
            else if (c.name == "StudsPerTileV") d.studsV = (float)c.value.n;
            else if (c.name == "OffsetStudsU") d.offsetU = (float)c.value.n;
            else if (c.name == "OffsetStudsV") d.offsetV = (float)c.value.n;
            else break;
            style_decal(d);
            break;
        }
        if (auto mit = dataMeshes_.find(c.id); mit != dataMeshes_.end()) {
            DataMesh& m = mit->second;
            const std::string& n = c.name;
            if (n == "MeshType") m.type = enumName(c.value);
            else if (n == "MeshId") m.meshId = c.value.s;
            else if (n == "TextureId") m.textureId = c.value.s;
            else if (n == "Scale") m.scale = c.value.v;
            else if (n == "Offset") m.offset = c.value.v;
            else if (n == "VertexColor") m.vertexColor = c.value.v;
            else break;
            if (auto pp = parts_.find(m.part); pp != parts_.end()) {
                update_shape(pp->second);
                if (n == "TextureId" || n == "VertexColor") update_material(pp->second);
            }
            break;
        }
        if (auto pit = prompts_.find(c.id); pit != prompts_.end()) { apply_prompt(pit->second, c); break; }
        // Everything arriving over the wire is stamped fromHost by DataModel::apply, so a
        // blanket !fromHost test would drop every server-made Sound on a client. Only the host's
        // own writebacks are named. TimePosition is one: the host writes it every frame a sound
        // plays, and re-applying it seeks the stream.
        bool ownSoundWriteback = c.fromHost
            && (c.name == "TimeLength" || c.name == "IsLoaded" || c.name == "PlaybackLoudness"
                || c.name == "IsPlaying" || c.name == "TimePosition");
        if (auto sit = sounds_.find(c.id); sit != sounds_.end()) { if (!ownSoundWriteback) apply_sound(c.id, sit->second, c); break; }
        // The Audio API's. Only the engine's own reports are filtered: IsPlaying here is real
        // state, written by Play and Stop, and has to arrive however it travelled.
        if (auto ait = audio_.find(c.id); ait != audio_.end()) {
            if (!(c.fromHost && (c.name == "TimeLength" || c.name == "IsReady"))) apply_audio(c.id, ait->second, c);
            break;
        }
        auto pit = parts_.find(c.id);
        if (pit == parts_.end()) break;
        Part& p = pit->second;
        const std::string& n = c.name;
        bool follow = c.fromHost && takes_pose_from_tree(c.id, p);
        // A remote character's root eases toward reported poses instead of jumping.
        auto place = [&]() {
            if (follow && p.role == ROLE_CHAR_ROOT) {
                auto cit = chars_.find(eit != entries_.end() ? eit->second.parent : 0);
                if (cit != chars_.end() && cit->second.root == c.id) { remote_pose(cit->second, p); return; }
            }
            update_transform(p);
        };
        // A LocalScript moving the character this machine simulates, as Roblox's character
        // ownership allows. Not marked as already sent: the pose reported next is what tells
        // the server.
        const bool ownPoseWrite = !c.fromHost && p.role == ROLE_CHAR_ROOT && local_root(c.id);
        // A script runs a frame behind the physics, so `root.CFrame = root.CFrame * turn` hands
        // back a stale position: an unchanged Position moves nothing, and an Orientation turns
        // the body about wherever it has got to since.
        if (n == "Position" && ownPoseWrite && near(c.value.v, p.sentPos, 1e-3f)) break;
        if (n == "Orientation" && ownPoseWrite && p.body) {
            p.orient = c.value.v;
            Transform3D now = p.body->get_transform();
            Transform3D turned = toTransform(p.pos, p.orient);
            turned.origin = now.origin;
            p.body->set_transform(turned);
            break;
        }
        if (n == "Position") {
            p.pos = c.value.v;
            if (!c.fromHost || follow) { if (!ownPoseWrite) p.sentPos = p.pos; place(); }
            if (follow && server_ && p.role == ROLE_CHAR_ROOT && p.pos.y < fallenHeight_) {
                // A client's character fell out of the world: the humanoid dies, the server respawns it.
                auto cit = chars_.find(eit != entries_.end() ? eit->second.parent : 0);
                int64_t hid = cit == chars_.end() ? 0 : cit->second.humanoid;
                if (hid) jobs_.push_back([hid](Runtime& rt) { if (Instance* h = rt.dataModel().find(hid)) h->set("Health", Value::number(0)); });
            }
        }
        else if (n == "Orientation") { p.orient = c.value.v; if (!c.fromHost || follow) { if (!ownPoseWrite) p.sentOrient = p.orient; place(); } }
        else if (n == "AssemblyLinearVelocity") {
            p.vel = c.value.v;
            // A body somebody else simulates reports what it has; without this the rest pose
            // would resend a thrown root's launch velocity after it had stopped.
            if (follow && c.fromHost) p.sentVel = p.vel;
            if (!c.fromHost) {
                p.sentVel = p.vel;
                if (auto* rb = Object::cast_to<RigidBody3D>(p.body)) rb->set_linear_velocity(toGd(p.vel));
                // A character too: Roblox's jump pad is Touched plus a write to the root's
                // Velocity, and a character is a CharacterBody3D, which the line above misses.
                else if (auto* cb = Object::cast_to<CharacterBody3D>(p.body)) {
                    cb->set_velocity(toGd(p.vel));
                    // Airborne for the step that follows: a velocity given while standing gets
                    // swept along that surface -- a 35-degree pad throws a quarter as high as
                    // asked -- and the walk's ground grip erases the sideways part.
                    auto eit2 = entries_.find(c.id);
                    if (eit2 != entries_.end())
                        if (auto cit = chars_.find(eit2->second.parent); cit != chars_.end())
                            cit->second.launched = true;
                }
            }
            // From the SERVER, on a client: taken even for the body this client owns -- that is
            // how a shove or a jump pad reaches a player, as on Roblox. Not this client's own
            // report coming back: re-applying a stale velocity re-flags the character airborne
            // mid-jump.
            else if (follow || (netClient_ && p.role == ROLE_CHAR_ROOT)) {
                // Own body: only what came over the wire (c.remote) is taken. Matching by value
                // cannot tell an old echo from the server's word once the runtime falls behind.
                const bool echo = netClient_ && p.role == ROLE_CHAR_ROOT && !follow && !c.remote;
                if (echo) {}
                else if (auto* cb = Object::cast_to<CharacterBody3D>(p.body)) {
                    cb->set_velocity(toGd(p.vel));
                    // Airborne for the next step, or the sweep lays the throw along the ground.
                    if (auto eit3 = entries_.find(c.id); eit3 != entries_.end())
                        if (auto cit = chars_.find(eit3->second.parent); cit != chars_.end())
                            cit->second.launched = true;
                }
            }
        }
        else if (n == "AssemblyAngularVelocity") {
            p.angVel = c.value.v;
            if (!c.fromHost) { p.sentAngVel = p.angVel; if (auto* rb = Object::cast_to<RigidBody3D>(p.body)) rb->set_angular_velocity(toGd(p.angVel)); }
        }
        else if (n == "Size") { p.size = c.value.v; update_shape(p); update_skin(p); resizedParts_.insert(c.id); massDirty_ = true; }
        else if (n == "Throttle") p.throttle = (float)c.value.n;
        else if (n == "Steer") p.steer = (float)c.value.n;
        else if (n == "MaxSpeed") p.maxSpeed = (float)c.value.n;
        else if (n == "Torque" && p.className == "VehicleSeat") p.driveTorque = (float)c.value.n;
        else if (n == "TurnSpeed") p.turnSpeed = (float)c.value.n;
        else if (n == "Shape") { p.shape = enumName(c.value); update_shape(p); }
        else if (n == "MeshData") { p.meshData = c.value.s; p.opMesh.unref(); p.opFailed = false; update_shape(p); }
        else if (n == "MeshId") { p.meshId = c.value.s; p.collisionMesh = Ref<Mesh>(); update_shape(p); update_material(p); }
        else if (n == "TextureID") { p.textureId = c.value.s; update_material(p); }
        else if (n == "CollisionFidelity") { p.collisionFidelity = enumName(c.value); update_shape(p); }
        else if (n == "Color") { p.color = c.value.c; update_material(p); }
        else if (n == "Transparency" || n == "LocalTransparencyModifier") {
            // Roblox folds the two: 1 - (1 - Transparency) * (1 - LocalTransparencyModifier)
            (n == "Transparency" ? p.baseTransparency : p.localTransparency) = (float)c.value.n;
            p.transparency = 1.0f - (1.0f - p.baseTransparency) * (1.0f - p.localTransparency);
            update_material(p);
        }
        else if (n == "Reflectance") { p.reflectance = (float)c.value.n; update_material(p); }
        else if (n.size() > 6 && (n.compare(n.size() - 6, 6, "ParamB") == 0 || (n.size() > 12 && n.compare(n.size() - 12, 12, "SurfaceInput") == 0))) {
            // a legacy Motor face's speed and switch, for the RotateV it turns
            static const char* faces[6] = {"Right", "Left", "Top", "Bottom", "Back", "Front"};
            for (int f = 0; f < 6; f++) if (n.rfind(faces[f], 0) == 0) {
                if (n.compare(n.size() - 6, 6, "ParamB") == 0) p.paramB[f] = (float)c.value.n;
                else p.surfaceInput[f] = (uint8_t)(c.value.type == Value::Enum ? (enumName(c.value) == "Constant" ? 1 : enumName(c.value) == "Sin" ? 2 : 0) : 0);
                constraintsDirty_ = true;
            }
        }
        // What a face looks like, not what it drives.
        else if (n.size() > 7 && n.compare(n.size() - 7, 7, "Surface") == 0) {
            static const char* faces[6] = {"Right", "Left", "Top", "Bottom", "Back", "Front"};
            const std::string kind = enumName(c.value);
            // Studs and Inlet are the only two Roblox still draws; Universal, Glue, Weld,
            // Hinge, Motor and SteppingMotor all render smooth.
            const uint8_t wears = kind == "Studs" ? 1 : kind == "Inlet" ? 2 : 0;
            for (int f = 0; f < 6; f++) if (n.rfind(faces[f], 0) == 0 && p.surface[f] != wears) {
                p.surface[f] = wears;
                update_studs(p);
            }
        }
        else if (n == "HeadsUpDisplay") p.headsUp = c.value.b;
        else if (n == "MaterialVariant") { p.materialVariant = c.value.s; update_material(p); }
        else if (n == "Material") { p.material = enumName(c.value); update_material(p); update_surface(p); massDirty_ = true; }
        else if (n == "CustomPhysicalProperties") { p.custom = c.value; update_surface(p); massDirty_ = true; }
        else if (n == "Massless") { p.massless = c.value.b; massDirty_ = true; }
        else if (n == "Anchored") {
            p.anchored = c.value.b;
            if (weldRoot_.count(c.id)) {
                jointsDirty_ = true;                                          // the assembly's root may change hands
                // Rebuild the root's body even when the root does not change hands: a welded
                // assembly is one body, static or rigid by the root part's own Anchored, and
                // neither rebuild_assemblies (which rebuilds on a root changing hands) nor
                // refresh_visibility looks at Anchored, so the body would stay a StaticBody.
                int64_t root = weld_root(c.id);
                if (auto rit = parts_.find(root); root && rit != parts_.end() && rit->second.visible) {
                    free_part(root, rit->second);
                    build_part(root, rit->second);
                }
            }
            else if (p.visible && p.role == ROLE_FREE) { free_part(c.id, p); build_part(c.id, p); }
        }
        else if (n == "CanCollide") { p.canCollide = c.value.b; if (p.col && (p.role == ROLE_FREE || p.welded)) { p.col->set_disabled(!p.canCollide); for (auto* x : p.extraCols) x->set_disabled(!p.canCollide); } }
        else if (n == "RenderFidelity") { p.renderFidelity = enumName(c.value); update_shape(p); }
        else if (n == "SmoothingAngle") { p.smoothingAngle = (float)c.value.n; update_shape(p); }
        // Separate from CanCollide, as on Roblox: a part can be walked through and still touch.
        else if (n == "CanTouch") p.canTouch = c.value.b;
        else if (n == "DoubleSided") { p.doubleSided = c.value.b; update_material(p); }
        else if (n == "UsePartColor") { p.usePartColor = c.value.b; update_material(p); }
        else if (n == "CastShadow") {
            p.castShadow = c.value.b;
            if (p.mesh) p.mesh->set_cast_shadows_setting(p.castShadow ? GeometryInstance3D::SHADOW_CASTING_SETTING_ON : GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
        }
        break;
    }
    }
}

// ---- lights ------------------------------------------------------------------------
void PulseBlockzWorld::apply_light(Light& l, const Change& c) {
    const std::string& n = c.name;
    if (n == "Brightness") l.brightness = (float)c.value.n;
    else if (n == "Range") l.range = (float)c.value.n;
    else if (n == "Angle") l.angle = (float)c.value.n;
    else if (n == "Color") l.color = c.value.c;
    else if (n == "Enabled") l.enabled = c.value.b;
    else if (n == "Shadows") l.shadows = c.value.b;
    else if (n == "Face") l.face = enumName(c.value);
    else return;
    style_light(l);
}

// A part's mesh went away (and the light node with it).
void PulseBlockzWorld::detach_lights(int64_t partId) {
    for (int64_t id : children_of(partId)) if (auto it = lights_.find(id); it != lights_.end() && it->second.part == partId && it->second.node) { it->second.node = nullptr; lightsDirty_ = true; }
    detach_decals(partId);
}

// ---- decals ----------------------------------------------------------------------
void PulseBlockzWorld::detach_decals(int64_t partId) {
    for (int64_t id : children_of(partId)) if (auto it = decals_.find(id); it != decals_.end() && it->second.part == partId && it->second.node) { it->second.node = nullptr; decalsDirty_ = true; }
    detach_emitters(partId);
}

// ---- constraints -----------------------------------------------------------------
// The part an Attachment sits in, and its frame there; 0 when it is in none.
int64_t PulseBlockzWorld::attachment_part(int64_t att, Transform3D& local) const {
    auto ait = attachments_.find(att);
    auto eit = entries_.find(att);
    if (ait == attachments_.end() || eit == entries_.end() || !parts_.count(eit->second.parent)) return 0;
    local = ait->second;
    return eit->second.parent;
}

// Where a built part is now, root or limb alike (the mesh carries the pose).
Transform3D PulseBlockzWorld::part_frame(const Part& p) const {
    if (p.mesh) return p.mesh->get_global_transform() * p.shapeLocal.affine_inverse();
    if (p.body) return p.body->get_global_transform();
    return toTransform(p.pos, p.orient);
}

// How far Attachment1 sits along Attachment0's axis: a slider's CurrentPosition.
static double slider_position(const Transform3D& w0, const Transform3D& w1) {
    return (w1.origin - w0.origin).dot(w0.basis.get_column(0).normalized());
}

// The turn of w1 against w0 about w0's X (a HingeConstraint's axis) or its Z (a legacy
// Rotate's), right-handed.
static double hinge_angle(const Transform3D& w0, const Transform3D& w1, bool aboutZ = false) {
    Vector3 axis = w0.basis.get_column(aboutZ ? 2 : 0).normalized();
    Vector3 y0 = w0.basis.get_column(aboutZ ? 0 : 1), y1 = w1.basis.get_column(aboutZ ? 0 : 1);
    y0 = (y0 - axis * y0.dot(axis)).normalized();
    y1 = (y1 - axis * y1.dot(axis)).normalized();
    return std::atan2(y0.cross(y1).dot(axis), y0.dot(y1));
}

// A hinged body's principal inertia is floored to an eighth of its largest component: Godot's
// hinge solver tears apart a long thin body (a fan blade, a 6-stud rod 0.4 across) whose inertia
// about the axis is a hundredth of that across it.
static void settle_hinged_inertia(Node3D* n) {
    auto* rb = Object::cast_to<RigidBody3D>(n);
    if (!rb || rb->get_inertia() != Vector3()) return;   // already settled (refresh_masses clears it)
    auto* st = PhysicsServer3D::get_singleton()->body_get_direct_state(rb->get_rid());
    if (!st) return;
    Vector3 inv = st->get_inverse_inertia();
    if (inv.x <= 0 || inv.y <= 0 || inv.z <= 0) return;   // not worked out yet: next frame
    Vector3 I(1 / inv.x, 1 / inv.y, 1 / inv.z);
    real_t floor = std::max(I.x, std::max(I.y, I.z)) / 8;
    if (I.x >= floor && I.y >= floor && I.z >= floor) return;   // stocky enough: Godot's own tensor stays
    rb->set_inertia(Vector3(std::max(I.x, floor), std::max(I.y, floor), std::max(I.z, floor)));
}

void PulseBlockzWorld::sync_constraints() {
    constraintsDirty_ = false;
    for (auto& [id, k] : constraints_) {
        Transform3D l0, l1;
        int64_t p0, p1;
        if (k.rotateJoint) {   // Part0 / Part1 with C0 / C1 in place of attachments
            p0 = parts_.count(k.part0) ? k.part0 : 0; l0 = toTransform(k.c0Pos, k.c0Ori);
            p1 = parts_.count(k.part1) ? k.part1 : 0; l1 = toTransform(k.c1Pos, k.c1Ori);
        } else { p0 = attachment_part(k.att0, l0); p1 = attachment_part(k.att1, l1); }
        int64_t r0 = p0 ? (weld_root(p0) ? weld_root(p0) : p0) : 0, r1 = p1 ? (weld_root(p1) ? weld_root(p1) : p1) : 0;
        Node3D* b0 = nullptr; Node3D* b1 = nullptr;
        if (r0 && r1 && r0 != r1 && k.enabled && !netClient_ && in_workspace(id) && in_workspace(p0) && in_workspace(p1)) {
            auto a = parts_.find(r0), b = parts_.find(r1);
            if (a != parts_.end() && b != parts_.end()) { b0 = a->second.body; b1 = b->second.body; }
        }
        // a joint between two anchored parts (or a character's) holds nothing
        bool want = b0 && b1 && (Object::cast_to<RigidBody3D>(b0) || Object::cast_to<RigidBody3D>(b1));
        bool slider = k.className == "PrismaticConstraint" || k.className == "CylindricalConstraint";
        int kind = k.className == "HingeConstraint" || k.rotateJoint ? 1 : k.className == "BallSocketConstraint" ? (k.limits ? 3 : 2) : slider ? 4 : 0;
        if (kind == 0) want = false;         // a rope / rod / spring: step_constraints keeps it
        if (k.node && (!want || k.bodyA != b0 || k.bodyB != b1 || k.kind != kind || !UtilityFunctions::is_instance_valid(k.bodyA) || !UtilityFunctions::is_instance_valid(k.bodyB))) {
            remove_child(k.node); k.node->queue_free(); k.node = nullptr;
        }
        if (!want) continue;
        const Part& part0 = parts_[p0];
        const Part& part1 = parts_[p1];
        Transform3D w0 = part_frame(part0) * l0, w1 = part_frame(part1) * l1;
        if (!k.node) {
            Transform3D frame = w0;
            if (kind == 1) {
                k.node = memnew(HingeJoint3D);
                // Godot hinges about the joint's Z, turning the other way round: Z is
                // -Attachment0.X. A Rotate turns about C0's own Z.
                if (!k.rotateJoint) frame.basis = frame.basis * Basis(Vector3(0, 1, 0), (real_t)(-Math_PI / 2));
                k.angle0 = hinge_angle(w0, w1, k.rotateJoint);
            } else if (kind == 4) {
                k.node = memnew(Generic6DOFJoint3D);   // it slides along the joint's X (and a cylinder turns about it), as Roblox's does along the attachment's
                k.pos0 = slider_position(w0, w1);
                k.angle0 = hinge_angle(w0, w1);
            } else if (kind == 3) k.node = memnew(ConeTwistJoint3D);   // its twist axis the joint's X, as Roblox's
            else k.node = memnew(PinJoint3D);
            auto eit = entries_.find(id);
            k.node->set_name(String::utf8(eit == entries_.end() ? "Constraint" : eit->second.name.c_str()));
            add_child(k.node);
            k.node->set_global_transform(frame);
            k.node->set_node_a(k.node->get_path_to(b0));
            k.node->set_node_b(k.node->get_path_to(b1));
            k.bodyA = b0; k.bodyB = b1; k.kind = kind;
        }
        if (auto* h = Object::cast_to<HingeJoint3D>(k.node)) {
            settle_hinged_inertia(b0); settle_hinged_inertia(b1);
            double angle = hinge_angle(w0, w1, k.rotateJoint);
            // Roblox's angle is between the attachments; Godot's zero is the pose the joint was
            // made in, so its limits sit angle0 away.
            h->set_flag(HingeJoint3D::FLAG_USE_LIMIT, k.limits);
            if (k.limits) {
                double lo = Math::deg_to_rad(k.lower) - k.angle0, hi = Math::deg_to_rad(k.upper) - k.angle0;
                h->set_param(HingeJoint3D::PARAM_LIMIT_LOWER, (real_t)std::max(-Math_PI, std::min(lo, hi)));
                h->set_param(HingeJoint3D::PARAM_LIMIT_UPPER, (real_t)std::min(Math_PI, std::max(lo, hi)));
            }
            bool motor = k.actuator == "Motor" && k.motorMaxTorque > 0;
            bool servo = k.actuator == "Servo" && k.servoMaxTorque > 0 && k.angularSpeed > 0;
            double torque = servo ? k.servoMaxTorque : k.motorMaxTorque;
            double v = k.angularVelocity;
            if (k.rotateJoint) {
                // A RotateV turns at the speed of the Motor face it was made from: Part0's face
                // along C0's Z, its ParamB in radians a second while that face's SurfaceInput is
                // Constant. A RotateP servos to DesiredAngle at MaxVelocity radians a step.
                Vector3 axis = toTransform(k.c0Pos, k.c0Ori).basis.get_column(2);
                int face = std::fabs(axis.x) >= std::fabs(axis.y) && std::fabs(axis.x) >= std::fabs(axis.z) ? (axis.x >= 0 ? 0 : 1)
                         : std::fabs(axis.y) >= std::fabs(axis.z) ? (axis.y >= 0 ? 2 : 3) : (axis.z >= 0 ? 4 : 5);
                motor = servo = false;
                // scaled to what it turns: a limitless impulse on a light blade shakes the hinge apart
                double m = 0;
                if (auto* r = Object::cast_to<RigidBody3D>(b0)) m += r->get_mass();
                if (auto* r = Object::cast_to<RigidBody3D>(b1)) m += r->get_mass();
                torque = 100.0 * std::max(m, 0.1);
                if (k.className == "RotateV") { motor = part0.surfaceInput[face] == 1; v = part0.paramB[face]; }
                else if (k.className == "RotateP") {
                    servo = true;
                    double speed = std::max(0.01, k.maxVelocity) * 60.0;
                    double err = (k.baseAngle + k.desiredAngle) - angle;
                    v = std::max(-speed, std::min(speed, err * 6.0));
                }
                v = -v;   // Godot's positive turn is left-handed about the joint's Z
            } else if (servo) {
                // a Servo: toward TargetAngle at up to AngularSpeed, easing in over the last radian
                double err = Math::deg_to_rad(k.targetAngle) - angle;
                v = std::max(-k.angularSpeed, std::min(k.angularSpeed, err * 6.0));
            }
            // AngularRestitution is bounce off the angular limit; Godot's relaxation is the same
            // idea from the other end, so it takes 1 - restitution.
            if (k.angularRestitution > 0)
                h->set_param(HingeJoint3D::PARAM_LIMIT_RELAXATION,
                             (real_t)std::clamp(1.0 - k.angularRestitution, 0.0, 1.0));
            h->set_flag(HingeJoint3D::FLAG_ENABLE_MOTOR, motor || servo);
            if (motor || servo) {
                // at a limit the motor stalls (Godot's limit is soft; a motor leaning on it creeps past)
                if (k.limits && ((v > 0 && angle >= Math::deg_to_rad(k.upper) - 0.01) || (v < 0 && angle <= Math::deg_to_rad(k.lower) + 0.01))) v = 0;
                // MotorMaxAcceleration caps how much the speed may change per step; Roblox's
                // default is no limit. Stepped from the motor's current target velocity.
                if (k.motorMaxAccel > 0) {
                    const double step = k.motorMaxAccel / Engine::get_singleton()->get_physics_ticks_per_second();
                    const double now = h->get_param(HingeJoint3D::PARAM_MOTOR_TARGET_VELOCITY);
                    v = std::max(now - step, std::min(now + step, v));
                }
                h->set_param(HingeJoint3D::PARAM_MOTOR_TARGET_VELOCITY, (real_t)v);
                h->set_param(HingeJoint3D::PARAM_MOTOR_MAX_IMPULSE, (real_t)(torque / Engine::get_singleton()->get_physics_ticks_per_second()));
            }
            double deg = Math::rad_to_deg(angle);
            if (std::fabs(deg - k.sentAngle) > 0.05) { pendingWrites_.push_back({id, "CurrentAngle", Value::number(deg)}); k.sentAngle = deg; }
        } else if (auto* sl = Object::cast_to<Generic6DOFJoint3D>(k.node)) {
            // Everything held but the slide along X (and, for a cylinder, the turn about it).
            // Godot measures from the pose the joint was made in: limits sit pos0/angle0 away.
            using G6 = Generic6DOFJoint3D;
            double pos = slider_position(w0, w1), lo = k.lowerLimit - k.pos0, hi = k.upperLimit - k.pos0;
            bool turns = k.className == "CylindricalConstraint";
            sl->set_flag_x(G6::FLAG_ENABLE_LINEAR_LIMIT, k.limits);
            sl->set_param_x(G6::PARAM_LINEAR_LOWER_LIMIT, (real_t)std::min(lo, hi));
            sl->set_param_x(G6::PARAM_LINEAR_UPPER_LIMIT, (real_t)std::max(lo, hi));
            sl->set_flag_x(G6::FLAG_ENABLE_ANGULAR_LIMIT, !turns || k.angularLimits);
            double alo = turns ? Math::deg_to_rad(k.lower) - k.angle0 : 0, ahi = turns ? Math::deg_to_rad(k.upper) - k.angle0 : 0;
            sl->set_param_x(G6::PARAM_ANGULAR_LOWER_LIMIT, (real_t)std::min(alo, ahi));
            sl->set_param_x(G6::PARAM_ANGULAR_UPPER_LIMIT, (real_t)std::max(alo, ahi));
            for (int ax = 0; ax < 2; ax++) {   // Y and Z hold, both ways
                auto flag = [&](G6::Flag f, bool on) { ax ? sl->set_flag_z(f, on) : sl->set_flag_y(f, on); };
                auto param = [&](G6::Param p, double v) { ax ? sl->set_param_z(p, (real_t)v) : sl->set_param_y(p, (real_t)v); };
                flag(G6::FLAG_ENABLE_LINEAR_LIMIT, true); flag(G6::FLAG_ENABLE_ANGULAR_LIMIT, true);
                param(G6::PARAM_LINEAR_LOWER_LIMIT, 0); param(G6::PARAM_LINEAR_UPPER_LIMIT, 0);
                param(G6::PARAM_ANGULAR_LOWER_LIMIT, 0); param(G6::PARAM_ANGULAR_UPPER_LIMIT, 0);
            }
            if (std::fabs(pos - k.sentPos) > 0.01) { pendingWrites_.push_back({id, "CurrentPosition", Value::number(pos)}); k.sentPos = pos; }
            if (turns) {
                double deg = Math::rad_to_deg(hinge_angle(w0, w1));
                if (std::fabs(deg - k.sentAngle) > 0.05) { pendingWrites_.push_back({id, "CurrentAngle", Value::number(deg)}); k.sentAngle = deg; }
            }
        } else if (auto* ct = Object::cast_to<ConeTwistJoint3D>(k.node)) {
            ct->set_param(ConeTwistJoint3D::PARAM_SWING_SPAN, (real_t)Math::deg_to_rad(std::max(0.0, k.upper)));
            ct->set_param(ConeTwistJoint3D::PARAM_TWIST_SPAN, (real_t)(k.twistLimits ? Math::deg_to_rad(std::max(0.0, (k.twistUpper - k.twistLower) / 2)) : Math_PI));
        }
    }
}

// ---- particles -------------------------------------------------------------------
void PulseBlockzWorld::detach_emitters(int64_t partId) {
    for (int64_t id : children_of(partId)) if (auto it = emitters_.find(id); it != emitters_.end() && it->second.part == partId && it->second.node) { it->second.node = nullptr; it->second.burst = nullptr; dirty_emitters(__LINE__); }
}

// An emitter sits in a part, or in an Attachment in a part (then at its frame); 0 otherwise.
int64_t PulseBlockzWorld::emitter_host(int64_t id, Transform3D& offset) const {
    offset = Transform3D();
    auto eit = entries_.find(id);
    if (eit == entries_.end()) return 0;
    int64_t parent = eit->second.parent;
    if (parts_.count(parent)) return parent;
    if (auto ait = attachments_.find(parent); ait != attachments_.end()) {
        auto pit = entries_.find(parent);
        if (pit != entries_.end() && parts_.count(pit->second.parent)) { offset = ait->second; return pit->second.parent; }
    }
    return 0;
}

Ref<Texture2D> PulseBlockzWorld::soft_dot() {
    if (softDot_.is_null()) {
        Ref<Image> im = Image::create(64, 64, false, Image::FORMAT_RGBA8);
        for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
            float d = std::sqrt((x - 31.5f) * (x - 31.5f) + (y - 31.5f) * (y - 31.5f)) / 31.5f;
            float a = std::clamp(1 - d, 0.f, 1.f); a = a * a * (3 - 2 * a);
            im->set_pixel(x, y, Color(1, 1, 1, a));
        }
        softDot_ = ImageTexture::create_from_image(im);
    }
    return softDot_;
}

void PulseBlockzWorld::sync_highlights() {
    highlightsDirty_ = false;
    for (auto& [id, h] : highlights_) {
        // freed now, not at the frame's end: a fresh one would be renamed by the name clash
        for (auto* n : h.nodes) if (UtilityFunctions::is_instance_valid(n)) { if (n->get_parent()) n->get_parent()->remove_child(n); n->queue_free(); }
        h.nodes.clear();
        auto eit = entries_.find(id);
        int64_t target = h.adornee ? h.adornee : (eit == entries_.end() ? 0 : eit->second.parent);
        if (!h.enabled || !target || !in_workspace(target)) continue;
        // its parts: the Adornee itself, or every part under it
        std::vector<Part*> parts;
        if (auto pit = parts_.find(target); pit != parts_.end()) parts.push_back(&pit->second);
        else for (auto& [pid, p] : parts_) {
            int64_t a = pid;
            for (int guard = 0; guard < 256 && a && a != target; guard++) { auto ait = entries_.find(a); a = ait == entries_.end() ? 0 : ait->second.parent; }
            if (a == target) parts.push_back(&p);
        }
        for (Part* p : parts) {
            if (!p->mesh || !p->visible || p->mesh->get_mesh().is_null()) continue;
            auto add = [&](const char* name, Col3 col, float alpha, bool outline, int priority) {
                auto* n = memnew(MeshInstance3D);
                n->set_name(String(name));
                n->set_mesh(p->mesh->get_mesh());
                n->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
                Ref<StandardMaterial3D> m; m.instantiate();
                m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
                m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
                m->set_albedo(Color(col.r, col.g, col.b, alpha));
                m->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, h.onTop);
                m->set_render_priority(priority);
                if (outline) { m->set_grow_enabled(true); m->set_grow(0.08f); m->set_cull_mode(BaseMaterial3D::CULL_FRONT); }
                n->set_material_override(m);
                p->mesh->add_child(n);
                h.nodes.push_back(n);
            };
            if (h.fillT < 1) add("Highlight Fill", h.fill, 1 - std::clamp(h.fillT, 0.f, 1.f), false, 1);
            if (h.outlineT < 1) add("Highlight Outline", h.outline, 1 - std::clamp(h.outlineT, 0.f, 1.f), true, 2);
        }
    }
}

void PulseBlockzWorld::step_trails(double dt) {
    for (auto& [id, t] : trails_) {
        t.clock += dt;
        bool on = t.enabled && in_workspace(id);
        Transform3D l0, l1;
        int64_t p0 = on ? attachment_part(t.att0, l0) : 0;
        int64_t p1 = on ? attachment_part(t.att1, l1) : 0;
        if (p0 && p1) {
            Vector3 a = (part_frame(parts_[p0]) * l0).origin, b = (part_frame(parts_[p1]) * l1).origin;
            Vector3 mid = (a + b) * 0.5f;
            if (t.pts.empty() || mid.distance_to(t.lastMid) >= (real_t)t.minLength) {
                if (!t.pts.empty()) t.travelled += mid.distance_to(t.lastMid);
                t.pts.push_back({a, b, t.clock, t.travelled}); t.lastMid = mid;
            }
        }
        while (!t.pts.empty() && t.clock - t.pts.front().t > t.lifetime) t.pts.pop_front();
        if (t.maxLength > 0 && t.pts.size() > 1) {
            double len = 0;
            for (size_t k = t.pts.size() - 1; k > 0; k--) {
                len += ((t.pts[k].a + t.pts[k].b) * 0.5f).distance_to((t.pts[k - 1].a + t.pts[k - 1].b) * 0.5f);
                if (len > t.maxLength) { t.pts.erase(t.pts.begin(), t.pts.begin() + (std::ptrdiff_t)k); break; }
            }
        }
        if (t.pts.size() < 2) { if (t.node) t.node->set_visible(false); continue; }
        if (!t.node) {
            t.node = memnew(MeshInstance3D);
            auto eit = entries_.find(id);
            t.node->set_name(eit != entries_.end() ? String::utf8(eit->second.name.c_str()) : String("Trail"));
            t.mesh.instantiate();
            t.node->set_mesh(t.mesh);
            t.node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
            t.mat.instantiate();
            t.mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
            t.mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
            t.mat->set_flag(BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
            t.mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
            t.mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
            t.node->set_material_override(t.mat);
            add_child(t.node, true);
        }
        if (t.textureLoaded != t.texture) {   // Texture: the image along the ribbon, times the colour sequence
            t.textureLoaded = t.texture;
            t.mat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, t.texture.empty() ? Ref<Texture2D>() : load_texture(t.texture));
        }
        t.node->set_visible(true);
        t.mesh->clear_surfaces();
        t.mesh->surface_begin(Mesh::PRIMITIVE_TRIANGLE_STRIP);
        // u along the ribbon: Stretch fits the texture once from attachments to tail, Wrap
        // repeats it every TextureLength studs from the attachments, Static every TextureLength
        // studs of the path itself, so it stays where it was laid.
        const double sNew = t.pts.back().s, sOld = t.pts.front().s, span = std::max(1e-6, sNew - sOld);
        const Vector3 eye = camera_ ? camera_->get_global_position() : Vector3(0, 0, 10);
        for (size_t i = 0; i < t.pts.size(); i++) {
            const Trail::Sample& s = t.pts[i];
            float age = (float)std::clamp((t.clock - s.t) / t.lifetime, 0.0, 1.0);   // 0 at the attachments, 1 about to go
            Col3 c = t.colorSeq.sampleColor(age);
            float alpha = std::clamp(1.0f - t.transSeq.sampleNumber(age), 0.0f, 1.0f) * (float)std::clamp(1.0 - t.localT, 0.0, 1.0);
            Color col((float)(c.r * t.brightness), (float)(c.g * t.brightness), (float)(c.b * t.brightness), alpha);
            float u = t.textureMode == 1 ? (float)((sNew - s.s) / t.textureLength)
                    : t.textureMode == 2 ? (float)(s.s / t.textureLength)
                    : (float)((sNew - s.s) / span);
            // WidthScale narrows the ribbon about its middle; FaceCamera turns its width to the eye
            Vector3 mid = (s.a + s.b) * 0.5f, a = s.a, b = s.b;
            float w = std::max(0.0f, t.widthScale.sampleNumber(age));
            if (t.faceCamera) {
                const Trail::Sample& n = i + 1 < t.pts.size() ? t.pts[i + 1] : t.pts[i > 0 ? i - 1 : i];
                Vector3 tangent = (n.a + n.b) * 0.5f - mid;
                Vector3 across = tangent.cross(eye - mid);
                if (across.length_squared() > 1e-8f) {
                    across = across.normalized() * (s.a.distance_to(s.b) * 0.5f);
                    a = mid + across; b = mid - across;
                }
            }
            a = mid + (a - mid) * w; b = mid + (b - mid) * w;
            t.mesh->surface_set_color(col); t.mesh->surface_set_uv(Vector2(u, 0)); t.mesh->surface_add_vertex(a);
            t.mesh->surface_set_color(col); t.mesh->surface_set_uv(Vector2(u, 1)); t.mesh->surface_add_vertex(b);
        }
        t.mesh->surface_end();
    }
}

// Every render frame: each Beam redrawn between where its attachments are now.
void PulseBlockzWorld::step_beams() {
    for (auto& [id, b] : beams_) {
        bool on = b.enabled && in_workspace(id);
        Transform3D l0, l1;
        int64_t p0 = on ? attachment_part(b.att0, l0) : 0;
        int64_t p1 = on ? attachment_part(b.att1, l1) : 0;
        if (!p0 || !p1 || !parts_.count(p0) || !parts_.count(p1)) { if (b.node) b.node->set_visible(false); continue; }
        Transform3D w0 = part_frame(parts_[p0]) * l0, w1 = part_frame(parts_[p1]) * l1;
        const Vector3 P0 = w0.origin, P3 = w1.origin;
        const Vector3 P1 = P0 + w0.basis.get_column(0) * (real_t)b.curve0;
        const Vector3 P2 = P3 - w1.basis.get_column(0) * (real_t)b.curve1;
        const Vector3 up0 = w0.basis.get_column(1), up1 = w1.basis.get_column(1);
        const Vector3 eye = camera_ ? camera_->get_global_position() : P0 + Vector3(0, 0, 10);
        if (!b.node) {
            b.node = memnew(MeshInstance3D);
            auto eit = entries_.find(id);
            b.node->set_name(eit != entries_.end() ? String::utf8(eit->second.name.c_str()) : String("Beam"));
            b.mesh.instantiate();
            b.node->set_mesh(b.mesh);
            b.node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
            b.mat.instantiate();
            b.mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
            b.mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
            b.mat->set_flag(BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
            b.mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
            b.mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
            b.node->set_material_override(b.mat);
            add_child(b.node, true);
        }
        if (b.textureLoaded != b.texture) {   // Texture: the image along the ribbon, times the colour sequence
            b.textureLoaded = b.texture;
            b.mat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, b.texture.empty() ? Ref<Texture2D>() : load_texture(b.texture));
        }
        b.node->set_visible(true);
        b.mesh->clear_surfaces();
        b.mesh->surface_begin(Mesh::PRIMITIVE_TRIANGLE_STRIP);
        const int n = std::clamp(b.segments, 1, 200);
        // u along the curve: Stretch fits the texture TextureLength times over the whole beam,
        // Wrap and Static repeat it every TextureLength studs; TextureSpeed slides it, studs/s.
        std::vector<float> along(n + 1, 0.f);
        {
            Vector3 prev = P0;
            for (int i = 1; i <= n; i++) {
                const float t = (float)i / (float)n, u = 1.0f - t;
                Vector3 p = P0 * (u * u * u) + P1 * (3 * u * u * t) + P2 * (3 * u * t * t) + P3 * (t * t * t);
                along[i] = along[i - 1] + prev.distance_to(p);
                prev = p;
            }
        }
        const double now = (double)Time::get_singleton()->get_ticks_msec() / 1000.0;
        const float slide = (float)(b.textureOffset + (now - b.offsetAt) * b.textureSpeed);   // SetTextureOffset restarts the scroll from its offset
        for (int i = 0; i <= n; i++) {
            const float t = (float)i / (float)n, u = 1.0f - t;
            const float tu = (b.textureMode == 0 ? t * (float)b.textureLength : along[i] / (float)b.textureLength) - slide;
            Vector3 p = P0 * (u * u * u) + P1 * (3 * u * u * t) + P2 * (3 * u * t * t) + P3 * (t * t * t);
            Vector3 tangent = (P1 - P0) * (3 * u * u) + (P2 - P1) * (6 * u * t) + (P3 - P2) * (3 * t * t);
            if (tangent.length_squared() < 1e-8f) tangent = P3 - P0;
            Vector3 across = b.faceCamera ? tangent.cross(eye - p) : up0.lerp(up1, t);
            if (across.length_squared() < 1e-8f) across = Vector3(0, 1, 0);
            across.normalize();
            const float half = (float)(b.width0 * u + b.width1 * t) * 0.5f;
            Col3 c = b.colorSeq.sampleColor(t);
            const float alpha = std::clamp(1.0f - b.transSeq.sampleNumber(t), 0.0f, 1.0f) * (float)std::clamp(1.0 - b.localT, 0.0, 1.0);
            Color col((float)(c.r * b.brightness), (float)(c.g * b.brightness), (float)(c.b * b.brightness), alpha);
            b.mesh->surface_set_color(col); b.mesh->surface_set_uv(Vector2(tu, 0)); b.mesh->surface_add_vertex(p + across * half);
            b.mesh->surface_set_color(col); b.mesh->surface_set_uv(Vector2(tu, 1)); b.mesh->surface_add_vertex(p - across * half);
        }
        b.mesh->surface_end();
    }
}

void PulseBlockzWorld::sync_explosions() {
    explosionsDirty_ = false;
    for (auto& [id, x] : explosions_) {
        if (x.fired || !in_workspace(id)) continue;
        x.fired = true;
        Vector3 at = toGd(x.pos);
        // BlastPressure flings the loose bodies within the radius, harder up close (Studio's
        // 500000 sends a part off at a few dozen studs a second)
        for (auto& [pid, p] : parts_) {
            auto* rb = Object::cast_to<RigidBody3D>(p.body);
            if (!rb || !p.visible) continue;
            Vector3 d = rb->get_global_position() - at;
            float dist = d.length();
            if (dist > x.radius) continue;
            Vector3 dir = dist < 0.01f ? Vector3(0, 1, 0) : d / dist;
            float strength = x.pressure * 5e-5f * (1.0f - 0.5f * dist / std::max(x.radius, 0.01f));
            rb->apply_central_impulse((dir + Vector3(0, 0.5f, 0)).normalized() * strength * (float)rb->get_mass());
        }
        if (!x.visible) continue;
        // the look: a one-shot burst of hot, fading sparks from the centre, sized to the radius
        auto* n = memnew(GPUParticles3D);
        auto eit = entries_.find(id);
        n->set_name(eit != entries_.end() ? String(eit->second.name.c_str()) : String("Explosion"));
        n->set_one_shot(true);
        n->set_explosiveness_ratio(1);
        n->set_amount(std::clamp((int)(x.radius * 12), 24, 400));
        n->set_lifetime(1.0);
        n->set_use_local_coordinates(false);
        n->set_position(at);
        Ref<ParticleProcessMaterial> pm; pm.instantiate();
        pm->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_SPHERE);
        pm->set_emission_sphere_radius(x.radius * 0.25f);
        pm->set_direction(Vector3(0, 1, 0));
        pm->set_spread(180);
        pm->set_param(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, Vector2(x.radius * 1.5f, x.radius * 4));
        pm->set_gravity(Vector3(0, -x.radius * 2, 0));
        pm->set_param(ParticleProcessMaterial::PARAM_DAMPING, Vector2(x.radius * 2, x.radius * 3));
        pm->set_param(ParticleProcessMaterial::PARAM_SCALE, Vector2(x.radius * 0.3f, x.radius * 0.6f));
        Ref<Gradient> g; g.instantiate();
        g->set_offsets(PackedFloat32Array{0.0f, 0.3f, 1.0f});
        g->set_colors(PackedColorArray{Color(1, 0.95f, 0.6f, 1), Color(1, 0.45f, 0.1f, 0.8f), Color(0.2f, 0.2f, 0.2f, 0)});
        Ref<GradientTexture1D> gt; gt.instantiate(); gt->set_gradient(g);
        pm->set_color_ramp(gt);
        n->set_process_material(pm);
        Ref<QuadMesh> qm; qm.instantiate(); qm->set_size(Vector2(1, 1));
        Ref<StandardMaterial3D> mat; mat.instantiate();
        mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
        mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
        mat->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
        mat->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
        mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
        mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
        mat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, soft_dot());
        qm->set_material(mat);
        n->set_draw_pass_mesh(0, qm);
        add_child(n);
        n->set_emitting(true);
        x.node = n;
    }
}

void PulseBlockzWorld::sync_emitters() {
    emittersDirty_ = false;
    for (auto& [id, e] : emitters_) {
        Transform3D offset;
        int64_t partId = emitter_host(id, offset);
        auto pit = parts_.find(partId);
        MeshInstance3D* mesh = pit == parts_.end() ? nullptr : pit->second.mesh;
        if (e.node && (e.part != partId || !mesh)) { e.node->queue_free(); e.node = nullptr; if (e.burst) e.burst->queue_free(); e.burst = nullptr; }
        e.offset = offset;
        if (!e.node && mesh) {
            e.node = memnew(GPUParticles3D);
            e.node->set_name(String::utf8(entries_[id].name.c_str()));
            e.part = partId;
            mesh->add_child(e.node);
        }
        style_emitter(e);
        if (e.pendingBurst > 0) { int n = e.pendingBurst; e.pendingBurst = 0; burst_emitter(e, n); }
    }
}

void PulseBlockzWorld::style_emitter(Emitter& e) {
    if (!e.node) return;
    auto pit = parts_.find(e.part);
    if (pit == parts_.end()) return;
    const Part& p = pit->second;
    auto prop = [&](const char* name) -> const Value* { auto it = e.props.find(name); return it == e.props.end() ? nullptr : &it->second; };
    auto num = [&](const char* name, float def) { const Value* v = prop(name); return v ? (float)v->n : def; };   // the runtime's value, or the class default
    auto boolean = [&](const char* name, bool def) { const Value* v = prop(name); return v ? v->b : def; };
    auto color = [&](const char* name, Col3 def) { const Value* v = prop(name); return v && v->type == Value::Color3 ? v->c : def; };
    auto range = [&](const char* name, float lo, float hi) { const Value* v = prop(name); if (v && v->type == Value::NumberRange) { lo = v->u[0]; hi = v->u[1]; } else if (v && v->type == Value::Number) lo = hi = (float)v->n; return std::make_pair(lo, hi); };
    // Fire, Smoke and Sparkles are mapped onto a ParticleEmitter's properties.
    bool enabled = boolean("Enabled", true);
    float rate = 20, lifeLo = 5, lifeHi = 10, speedLo = 5, speedHi = 5, spread = 0, drag = 0, lightEmission = 0, brightness = num("Brightness", 1), timeScale = num("TimeScale", 1);
    std::pair<float, float> rot{0, 0}, rotSpeed{0, 0};
    Vector3 accel, dir(0, 1, 0);
    Value colorSeq = Value::colorSequence(Col3{1, 1, 1}), sizeSeq = Value::numberSequence(1, 1), transSeq = Value::numberSequence(0, 0);
    Value squashSeq = Value::numberSequence(0, 0);
    std::string texture;
    bool locked = false, sphere = false;
    // Shape, ShapeStyle (throughout the volume or only off its skin) and ShapeInOut.
    std::string shapeName = "Box", inOut = "Outward";
    bool surfaceOnly = false;
    float inherit = 0;
    if (e.className == "Fire") {
        float size = num("Size", 5), heat = num("Heat", 9);
        Col3 a = color("Color", {236 / 255.f, 139 / 255.f, 70 / 255.f}), b = color("SecondaryColor", {139 / 255.f, 80 / 255.f, 15 / 255.f});
        rate = 40; lifeLo = 0.5f; lifeHi = 0.9f; speedLo = heat * 0.5f; speedHi = heat; spread = 25; lightEmission = 1;
        colorSeq = Value::colorSequence({0, a.r, a.g, a.b, 0.6f, b.r, b.g, b.b, 1, b.r * 0.3f, b.g * 0.3f, b.b * 0.3f});
        sizeSeq = Value::numberSequence({0, size * 0.5f, 0, 0.4f, size * 0.35f, 0, 1, 0, 0});
        transSeq = Value::numberSequence({0, 0.2f, 0, 0.7f, 0.5f, 0, 1, 1, 0});
        dir = Vector3(0, 1, 0); sphere = true;
    } else if (e.className == "Smoke") {
        float size = num("Size", 1), opacity = num("Opacity", 0.5f), rise = num("RiseVelocity", 1);
        Col3 c = color("Color", {178 / 255.f, 178 / 255.f, 178 / 255.f});
        rate = 8; lifeLo = 2.5f; lifeHi = 4; speedLo = std::max(rise, 0.f) * 0.7f; speedHi = std::max(rise, 0.f) + 0.5f; spread = 15;
        colorSeq = Value::colorSequence(c);
        sizeSeq = Value::numberSequence({0, size * 1.5f, 0, 1, size * 5, 0});
        transSeq = Value::numberSequence({0, 1 - opacity, 0, 0.3f, 1 - opacity * 0.8f, 0, 1, 1, 0});
        rotSpeed = {-40, 40}; rot = {0, 360};
        dir = Vector3(0, 1, 0);
    } else if (e.className == "Sparkles") {
        Col3 c = color("SparkleColor", {144 / 255.f, 144 / 255.f, 1});
        rate = 30; lifeLo = 0.5f; lifeHi = 1.2f; speedLo = 3; speedHi = 7; spread = 180; lightEmission = 1;
        colorSeq = Value::colorSequence(c, Col3{1, 1, 1});
        sizeSeq = Value::numberSequence({0, 0.6f, 0, 1, 0, 0});
        transSeq = Value::numberSequence(0, 1);
        sphere = true;
    } else {
        rate = num("Rate", 20);
        std::tie(lifeLo, lifeHi) = range("Lifetime", 5, 10);
        std::tie(speedLo, speedHi) = range("Speed", 5, 5);
        rot = range("Rotation", 0, 0); rotSpeed = range("RotSpeed", 0, 0);
        if (const Value* v = prop("SpreadAngle")) spread = std::max(std::fabs(v->v.x), std::fabs(v->v.y));
        drag = num("Drag", 0); lightEmission = num("LightEmission", 0);
        if (const Value* v = prop("Acceleration")) accel = toGd(v->v);
        if (const Value* v = prop("Color")) colorSeq = *v;
        if (const Value* v = prop("Size")) sizeSeq = *v;
        if (const Value* v = prop("Transparency")) transSeq = *v;
        if (const Value* v = prop("Squash")) squashSeq = *v;
        if (const Value* v = prop("Texture")) texture = v->s;
        locked = boolean("LockedToPart", false);
        if (const Value* v = prop("Shape")) shapeName = enumName(*v);
        sphere = shapeName == "Sphere";
        if (const Value* v = prop("ShapeStyle")) surfaceOnly = enumName(*v) == "Surface";
        if (const Value* v = prop("ShapeInOut")) inOut = enumName(*v);
        inherit = num("VelocityInheritance", 0);
        std::string face = prop("EmissionDirection") ? enumName(*prop("EmissionDirection")) : "Top";
        if (face == "Bottom") dir = Vector3(0, -1, 0); else if (face == "Front") dir = Vector3(0, 0, -1); else if (face == "Back") dir = Vector3(0, 0, 1);
        else if (face == "Right") dir = Vector3(1, 0, 0); else if (face == "Left") dir = Vector3(-1, 0, 0);
    }
    lifeHi = std::max(lifeHi, 0.01f); lifeLo = std::clamp(lifeLo, 0.f, lifeHi);
    // Roblox's default sparkle sheet is not shipped: a soft spot stands in, as for any image that fails to load.
    Ref<Texture2D> tex;
    if (!texture.empty() && texture != "rbxasset://textures/particles/sparkles_main.dds") tex = load_texture(texture);
    if (tex.is_null()) tex = soft_dot();
    // As many particles as a lifetime's worth of the rate, in the part's frame.
    GPUParticles3D* n = e.node;
    n->set_transform(p.shapeLocal.affine_inverse() * e.offset);
    int amount = std::clamp((int)std::ceil(rate * lifeHi), 1, 4000);
    if (n->get_amount() != amount) n->set_amount(amount);
    n->set_lifetime(lifeHi);
    n->set_speed_scale(std::max(timeScale, 0.f));
    n->set_use_local_coordinates(locked);
    n->set_emitting(enabled && rate > 0);
    n->set_visible(true);
    bool squashed = false;
    for (size_t k = 0; k + 2 < squashSeq.kp.size(); k += 3)
        if (std::fabs(squashSeq.kp[k + 1]) > 1e-4f) squashed = true;
    // A stretched particle stretches along its travel, not up the screen: the node aligns Z at
    // the camera and Y down the velocity, which the material's own billboard cannot.
    n->set_transform_align(squashed ? GPUParticles3D::TRANSFORM_ALIGN_Z_BILLBOARD_Y_TO_VELOCITY
                                    : GPUParticles3D::TRANSFORM_ALIGN_DISABLED);
    Ref<QuadMesh> qm = n->get_draw_pass_mesh(0);
    if (qm.is_null()) { qm.instantiate(); qm->set_size(Vector2(1, 1)); n->set_draw_pass_mesh(0, qm); }
    Ref<StandardMaterial3D> m = qm->get_material();
    if (m.is_null()) {
        m.instantiate();
        m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
        m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
        m->set_billboard_mode(BaseMaterial3D::BILLBOARD_PARTICLES);
        // Without this a billboard rebuilds the quad's basis with unit axes and throws the
        // scale curve away: every particle draws one stud across whatever Size asks for.
        m->set_flag(BaseMaterial3D::FLAG_BILLBOARD_KEEP_SCALE, true);
        m->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
        m->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
        qm->set_material(m);
    }
    // When the node aligns, the material must leave the basis alone or it overwrites it.
    m->set_billboard_mode(squashed ? BaseMaterial3D::BILLBOARD_DISABLED
                                   : BaseMaterial3D::BILLBOARD_PARTICLES);
    m->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, tex);
    m->set_blend_mode(lightEmission >= 0.5f ? BaseMaterial3D::BLEND_MODE_ADD : BaseMaterial3D::BLEND_MODE_MIX);
    Ref<ParticleProcessMaterial> pm = n->get_process_material();
    if (pm.is_null()) { pm.instantiate(); n->set_process_material(pm); }
    {
        // The flipbook: Texture cut into a grid and played over each particle. Loop and OneShot are
        // Godot's two (OneShot once over the lifetime, Framerate ignored, as on Roblox); PingPong
        // plays as Loop and Random as Loop from a random frame. Godot's speed is animations per
        // lifetime, so Framerate runs against the lifetime's top. Custom cuts by nothing known here
        // and draws whole, and a squashed emitter (no billboard) does not animate.
        const std::string layout = prop("FlipbookLayout") ? enumName(*prop("FlipbookLayout")) : "None";
        const std::string mode = prop("FlipbookMode") ? enumName(*prop("FlipbookMode")) : "Loop";
        const int grid = layout == "Grid2x2" ? 2 : layout == "Grid4x4" ? 4 : layout == "Grid8x8" ? 8 : 1;
        const auto fps = range("FlipbookFramerate", 1, 1);
        const float frames = (float)(grid * grid);
        m->set_particles_anim_h_frames(grid);
        m->set_particles_anim_v_frames(grid);
        m->set_particles_anim_loop(grid > 1 && mode != "OneShot");
        pm->set_param_min(ParticleProcessMaterial::PARAM_ANIM_SPEED, grid == 1 ? 0.f : mode == "OneShot" ? 1.f : fps.first * lifeHi / frames);
        pm->set_param_max(ParticleProcessMaterial::PARAM_ANIM_SPEED, grid == 1 ? 0.f : mode == "OneShot" ? 1.f : fps.second * lifeHi / frames);
        pm->set_param_min(ParticleProcessMaterial::PARAM_ANIM_OFFSET, 0);
        pm->set_param_max(ParticleProcessMaterial::PARAM_ANIM_OFFSET, grid > 1 && (mode == "Random" || boolean("FlipbookStartRandom", false)) ? 1.f : 0.f);
    }
    Vector3 half = toGd(p.size) * 0.5f;
    // A box emitting outward is the only case Godot does with a shape plus a direction. A
    // cylinder, a disc, a skin or inward particles need an origin AND a direction per particle,
    // which means a point cloud with normals.
    const bool simple = shapeName == "Box" && !surfaceOnly && inOut == "Outward";
    if (simple) {
        if (sphere) { pm->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_SPHERE_SURFACE); pm->set_emission_sphere_radius(std::max({half.x, half.y, half.z, 0.05f}) * 0.5f); }
        else { pm->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_BOX); pm->set_emission_box_extents(half); }
        pm->set_direction(dir);
    } else {
        set_emitter_cloud(pm, shapeName, surfaceOnly, inOut, half);
        // The cloud carries the direction per particle; a direction here would fight it.
        pm->set_direction(Vector3(0, 1, 0));
    }
    // VelocityInheritance: 0 leaves the sparks where they were lit, 1 drags them along.
    pm->set_inherit_velocity_ratio(std::clamp((double)inherit, -10.0, 10.0));
    pm->set_spread(std::clamp(spread, 0.f, 180.f));
    pm->set_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, speedLo);
    pm->set_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, speedHi);
    pm->set_gravity(accel);
    pm->set_param_min(ParticleProcessMaterial::PARAM_DAMPING, drag * speedLo);
    pm->set_param_max(ParticleProcessMaterial::PARAM_DAMPING, drag * speedHi);
    pm->set_param_min(ParticleProcessMaterial::PARAM_ANGLE, rot.first);
    pm->set_param_max(ParticleProcessMaterial::PARAM_ANGLE, rot.second);
    pm->set_param_min(ParticleProcessMaterial::PARAM_ANGULAR_VELOCITY, rotSpeed.first);
    pm->set_param_max(ParticleProcessMaterial::PARAM_ANGULAR_VELOCITY, rotSpeed.second);
    pm->set_lifetime_randomness(lifeHi > 0 ? 1 - lifeLo / lifeHi : 0);
    // Size over life as a curve; colour and transparency as one ramp over both sets of keypoints.
    Ref<Curve> curve; curve.instantiate();
    float sizeMax = 0.01f;
    for (size_t k = 0; k + 2 < sizeSeq.kp.size(); k += 3) sizeMax = std::max(sizeMax, sizeSeq.kp[k + 1]);
    curve->set_min_value(0); curve->set_max_value(sizeMax);
    for (size_t k = 0; k + 2 < sizeSeq.kp.size(); k += 3) curve->add_point(Vector2(sizeSeq.kp[k], sizeSeq.kp[k + 1]));
    pm->set_param_min(ParticleProcessMaterial::PARAM_SCALE, 1);
    pm->set_param_max(ParticleProcessMaterial::PARAM_SCALE, 1);
    // Squash stretches a particle along its travel over its life, and the quad's Y runs along
    // that travel, so it is a scale curve on Y alone. Always the three-channel texture, even
    // with nothing to squash: Godot picks a process material's shader from the kind of curve on
    // scale_curve, and swapping kinds on a live material leaves the old shader running.
    Ref<Curve> along; along.instantiate();
    std::vector<float> ts;
    for (size_t k = 0; k + 2 < sizeSeq.kp.size(); k += 3) ts.push_back(sizeSeq.kp[k]);
    for (size_t k = 0; k + 2 < squashSeq.kp.size(); k += 3) ts.push_back(squashSeq.kp[k]);
    std::sort(ts.begin(), ts.end());
    ts.erase(std::unique(ts.begin(), ts.end()), ts.end());
    if (ts.empty()) ts = {0, 1};
    float longMax = 0.01f;
    for (float t : ts) longMax = std::max(longMax, sizeSeq.sampleNumber(t) * std::max(1 + squashSeq.sampleNumber(t), 0.f));
    // One ceiling for both curves: each bakes over its own min and max.
    curve->set_max_value(longMax);
    along->set_min_value(0); along->set_max_value(longMax);
    for (float t : ts)
        along->add_point(Vector2(t, sizeSeq.sampleNumber(t) * std::max(1 + squashSeq.sampleNumber(t), 0.f)));
    Ref<CurveXYZTexture> xyz; xyz.instantiate();
    xyz->set_curve_x(curve); xyz->set_curve_y(along); xyz->set_curve_z(curve);
    // The property, not set_param_texture: the typed setter takes a Texture2D and keeps only
    // the curve's X.
    pm->set("scale_curve", xyz);
    std::vector<float> times;
    for (size_t k = 0; k + 3 < colorSeq.kp.size(); k += 4) times.push_back(colorSeq.kp[k]);
    for (size_t k = 0; k + 2 < transSeq.kp.size(); k += 3) times.push_back(transSeq.kp[k]);
    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end()), times.end());
    if (times.empty()) times = {0, 1};
    PackedFloat32Array offsets; PackedColorArray colors;
    const float shown = std::clamp(1 - num("LocalTransparencyModifier", 0), 0.f, 1.f);   // folded in with Transparency, as on a part
    for (float t : times) {
        Col3 c = colorSeq.sampleColor(t);
        offsets.push_back(t);
        colors.push_back(Color(c.r * brightness, c.g * brightness, c.b * brightness, std::clamp(1 - transSeq.sampleNumber(t), 0.f, 1.f) * shown));
    }
    Ref<Gradient> g; g.instantiate();
    g->set_offsets(offsets); g->set_colors(colors);
    Ref<GradientTexture1D> gt; gt.instantiate(); gt->set_gradient(g);
    pm->set_color_ramp(gt);
    if (e.burst) { e.burst->set_transform(n->get_transform()); e.burst->set_process_material(pm); e.burst->set_draw_pass_mesh(0, qm); e.burst->set_lifetime(lifeHi); e.burst->set_use_local_coordinates(locked); }
}

// Emit(n): a one-shot node beside the emitter's, all n at once.
void PulseBlockzWorld::burst_emitter(Emitter& e, int count) {
    if (count <= 0) return;
    if (!e.node) { e.pendingBurst += count; dirty_emitters(__LINE__); return; }
    if (!e.burst) {
        e.burst = memnew(GPUParticles3D);
        e.burst->set_name(e.node->get_name() + String(" Burst"));
        e.burst->set_one_shot(true);
        e.burst->set_explosiveness_ratio(1);
        e.burst->set_emitting(false);
        e.node->get_parent()->add_child(e.burst);
        style_emitter(e);
    }
    e.burst->set_amount(std::clamp(count, 1, 4000));
    e.burst->restart();
}

// The layer the host draws on: the core GUI and anything the embedder puts above the game.
// Script-made ScreenGuis are clamped below it.
static constexpr int kHostGuiLayer = 100;

// A Decal hangs under the mesh of the part it is in: a quad on the Face, sized to it, a hair
// out so it draws over the part.
void PulseBlockzWorld::sync_decals() {
    decalsDirty_ = false;
    for (auto& [id, d] : decals_) {
        auto eit = entries_.find(id);
        int64_t partId = eit == entries_.end() ? 0 : eit->second.parent;
        auto pit = parts_.find(partId);
        MeshInstance3D* mesh = pit == parts_.end() ? nullptr : pit->second.mesh;
        if (d.node && (d.part != partId || !mesh)) { d.node->queue_free(); d.node = nullptr; }
        if (!d.node && mesh) {
            d.node = memnew(MeshInstance3D);
            d.node->set_name(String::utf8(eit->second.name.c_str()));
            Ref<QuadMesh> qm; qm.instantiate();
            d.node->set_mesh(qm);
            d.node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
            d.part = partId;
            mesh->add_child(d.node);
        }
        style_decal(d);
    }
}

static Ref<ArrayMesh> unionDecalMesh(const Ref<ArrayMesh>& drawn, int face);   // below, with the union drawing

void PulseBlockzWorld::style_decal(Decal& d) {
    if (!d.node) return;
    auto pit = parts_.find(d.part);
    if (pit == parts_.end()) return;
    const Part& p = pit->second;
    // d.texture is TextureContent's key: a uri, or an EditableImage a script drew into.
    Ref<Texture2D> tex = load_texture(d.texture);
    // Roblox folds the two: 1 - (1 - Transparency) * (1 - LocalTransparencyModifier)
    const float shown = std::clamp(1 - d.transparency, 0.0f, 1.0f) * std::clamp(1 - d.localT, 0.0f, 1.0f);
    d.node->set_visible(tex.is_valid() && shown > 0);
    if (tex.is_null()) return;
    const ClassDef* pcls = findClass(p.className);
    if (pcls && pcls->isA("PartOperation") && p.opDrawn.is_valid()) {
        static const char* faces[6] = {"Right", "Left", "Top", "Bottom", "Back", "Front"};
        int face = 5;
        for (int k = 0; k < 6; k++) if (d.face == faces[k]) face = k;
        Ref<ArrayMesh> projected = unionDecalMesh(p.opDrawn, face);
        d.node->set_mesh(projected);
        d.node->set_visible(projected.is_valid() && d.transparency < 1);
        d.node->set_transform(p.shapeLocal.affine_inverse());
        Ref<StandardMaterial3D> um = d.node->get_material_override();
        if (um.is_null()) { um.instantiate(); um->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA); um->set_cull_mode(BaseMaterial3D::CULL_BACK); um->set_roughness(0.9f); d.node->set_material_override(um); }
        um->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, tex);
        um->set_albedo(Color(d.color.r, d.color.g, d.color.b, shown));
        um->set_flag(BaseMaterial3D::FLAG_USE_TEXTURE_REPEAT, d.tiled);
        const Vector3 dims = toGd(p.size);
        const float w = face <= 1 ? dims.z : dims.x, h = (face == 2 || face == 3) ? dims.z : dims.y;
        if (d.tiled) {
            float su = std::max(d.studsU, 0.01f), sv = std::max(d.studsV, 0.01f);
            um->set_uv1_scale(Vector3(w / su, h / sv, 1));
            um->set_uv1_offset(Vector3(-d.offsetU / su, d.offsetV / sv, 0));
        } else { um->set_uv1_scale(Vector3(d.uvScale.x, d.uvScale.y, 1)); um->set_uv1_offset(Vector3(d.uvOffset.x, d.uvOffset.y, 0)); }
        return;
    }
    if (d.node->get_mesh().is_null() || !Object::cast_to<QuadMesh>(d.node->get_mesh().ptr())) { Ref<QuadMesh> qm; qm.instantiate(); d.node->set_mesh(qm); }
    // The face: its normal, the image's right and up as seen from outside.
    Vector3 n(0, 0, -1), right(-1, 0, 0), up(0, 1, 0);
    float w = p.size.x, h = p.size.y, out = p.size.z * 0.5f;
    if (d.face == "Back") { n = Vector3(0, 0, 1); right = Vector3(1, 0, 0); }
    else if (d.face == "Right") { n = Vector3(1, 0, 0); right = Vector3(0, 0, -1); w = p.size.z; out = p.size.x * 0.5f; }
    else if (d.face == "Left") { n = Vector3(-1, 0, 0); right = Vector3(0, 0, 1); w = p.size.z; out = p.size.x * 0.5f; }
    else if (d.face == "Top") { n = Vector3(0, 1, 0); right = Vector3(1, 0, 0); up = Vector3(0, 0, -1); h = p.size.z; out = p.size.y * 0.5f; }
    else if (d.face == "Bottom") { n = Vector3(0, -1, 0); right = Vector3(-1, 0, 0); up = Vector3(0, 0, -1); h = p.size.z; out = p.size.y * 0.5f; }
    Ref<QuadMesh> qm = d.node->get_mesh();
    if (qm.is_valid()) qm->set_size(Vector2(w, h));
    d.node->set_transform(p.shapeLocal.affine_inverse() * Transform3D(Basis(right, up, n), n * (out + 0.01f)));
    Ref<StandardMaterial3D> m = d.node->get_material_override();
    if (m.is_null()) {
        m.instantiate();
        m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
        m->set_shading_mode(BaseMaterial3D::SHADING_MODE_PER_PIXEL);
        m->set_cull_mode(BaseMaterial3D::CULL_BACK);
        m->set_roughness(0.9f);
        d.node->set_material_override(m);
    }
    m->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, tex);
    m->set_albedo(Color(d.color.r, d.color.g, d.color.b, shown));
    // Each map replaces the flat value it modulates, as a SurfaceAppearance's does, and tiles
    // with the image.
    Ref<Texture2D> normal = load_texture(d.normalMap), metal = load_texture(d.metalnessMap), rough = load_texture(d.roughnessMap);
    m->set_feature(BaseMaterial3D::FEATURE_NORMAL_MAPPING, normal.is_valid());
    m->set_texture(BaseMaterial3D::TEXTURE_NORMAL, normal);
    m->set_texture(BaseMaterial3D::TEXTURE_METALLIC, metal);
    m->set_metallic(metal.is_valid() ? 1.0f : 0.0f);
    m->set_texture(BaseMaterial3D::TEXTURE_ROUGHNESS, rough);
    m->set_roughness(rough.is_valid() ? 1.0f : 0.9f);
    m->set_flag(BaseMaterial3D::FLAG_USE_TEXTURE_REPEAT, d.tiled);
    if (d.tiled) {
        float su = std::max(d.studsU, 0.01f), sv = std::max(d.studsV, 0.01f);
        m->set_uv1_scale(Vector3(w / su, h / sv, 1));
        m->set_uv1_offset(Vector3(-d.offsetU / su, d.offsetV / sv, 0));
    } else { m->set_uv1_scale(Vector3(d.uvScale.x, d.uvScale.y, 1)); m->set_uv1_offset(Vector3(d.uvOffset.x, d.uvOffset.y, 0)); }
}

// Every light hangs under the mesh of the part it is in, so it rides along. In an Attachment it
// hangs at the attachment on that attachment's part, which is how Roblox puts several lights at
// different points on one part. Anywhere else it has no node.
void PulseBlockzWorld::sync_lights() {
    lightsDirty_ = false;
    for (auto& [id, l] : lights_) {
        auto eit = entries_.find(id);
        int64_t partId = emitter_host(id, l.offset);
        auto pit = parts_.find(partId);
        MeshInstance3D* mesh = pit == parts_.end() ? nullptr : pit->second.mesh;
        if (l.node && (l.part != partId || !mesh)) { l.node->queue_free(); l.node = nullptr; }
        if (!l.node && mesh) {
            l.node = l.className == "PointLight" ? static_cast<Light3D*>(memnew(OmniLight3D)) : static_cast<Light3D*>(memnew(SpotLight3D));
            l.node->set_name(String::utf8(eit->second.name.c_str()));
            l.part = partId;
            mesh->add_child(l.node);
        }
        style_light(l);
    }
}

void PulseBlockzWorld::style_light(Light& l) {
    if (!l.node) return;
    l.node->set_visible(l.enabled);
    l.node->set_color(Color(l.color.r, l.color.g, l.color.b));
    l.node->set_param(Light3D::PARAM_ENERGY, l.brightness);
    l.node->set_param(Light3D::PARAM_RANGE, std::max(l.range, 0.01f));
    l.node->set_shadow(l.shadows && qualityLevel_ >= 7);
    // Back to the part's own frame (the mesh may lie rotated), then the attachment, then Face.
    Transform3D t;
    if (auto pit = parts_.find(l.part); pit != parts_.end()) t = pit->second.shapeLocal.affine_inverse();
    t = t * l.offset;
    if (l.className != "PointLight") {
        l.node->set_param(Light3D::PARAM_SPOT_ANGLE, std::clamp(l.angle * 0.5f, 0.5f, 89.f));   // Roblox: the whole cone
        Basis b;                                                          // Front: a spot shines down -Z already
        if (l.face == "Back") b = Basis(Vector3(0, 1, 0), Math_PI);
        else if (l.face == "Left") b = Basis(Vector3(0, 1, 0), Math_PI / 2);
        else if (l.face == "Right") b = Basis(Vector3(0, 1, 0), -Math_PI / 2);
        else if (l.face == "Top") b = Basis(Vector3(1, 0, 0), Math_PI / 2);
        else if (l.face == "Bottom") b = Basis(Vector3(1, 0, 0), -Math_PI / 2);
        t = t * Transform3D(b, Vector3());
    }
    l.node->set_transform(t);
}

// ---- proximity prompts -------------------------------------------------------------
void PulseBlockzWorld::apply_prompt(Prompt& pr, const Change& c) {
    const std::string& n = c.name;
    if (n == "ActionText") pr.actionText = c.value.s;
    else if (n == "ObjectText") pr.objectText = c.value.s;
    else if (n == "KeyboardKeyCode") pr.key = enumName(c.value);
    else if (n == "Style") pr.style = enumName(c.value);
    else if (n == "Shown") pr.shown = c.value.b;
    else if (n == "UIOffset") { pr.offsetX = c.value.v.x; pr.offsetY = c.value.v.y; }
    else return;
    style_prompt(pr);
}

void PulseBlockzWorld::detach_prompts(int64_t partId) {
    for (int64_t id : children_of(partId)) if (auto it = prompts_.find(id); it != prompts_.end() && it->second.part == partId && it->second.node) { it->second.node = nullptr; promptsDirty_ = true; }
}

// A prompt's label hangs under its part's mesh -- in a Model, under the first part in it.
void PulseBlockzWorld::sync_prompts() {
    promptsDirty_ = false;
    for (auto& [id, pr] : prompts_) {
        auto eit = entries_.find(id);
        int64_t holder = eit == entries_.end() ? kNoParent : eit->second.parent, partId = 0;
        if (parts_.count(holder)) partId = holder;
        else if (holder != kNoParent)
            for (int64_t pid : parts_under(holder))
                if (auto pp = parts_.find(pid); pp != parts_.end() && pp->second.mesh) { partId = pid; break; }
        auto pit = parts_.find(partId);
        MeshInstance3D* mesh = pit == parts_.end() ? nullptr : pit->second.mesh;
        if (pr.node && (pr.part != partId || !mesh)) { pr.node->queue_free(); pr.node = nullptr; }
        if (!pr.node && mesh) {
            pr.node = memnew(Label3D);
            pr.node->set_name(String::utf8(eit->second.name.c_str()));
            pr.node->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
            pr.node->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true);
            pr.node->set_render_priority(10);
            pr.node->set_pixel_size(0.012f);
            pr.node->set_font_size(40);
            pr.node->set_outline_size(12);
            pr.node->set_modulate(Color(1, 1, 1));
            pr.node->set_outline_modulate(Color(0, 0, 0, 0.85f));
            pr.part = partId;
            mesh->add_child(pr.node);
        }
        style_prompt(pr);
    }
}

// Roblox's default prompt, near enough: the object's name over the key and the action.
void PulseBlockzWorld::style_prompt(Prompt& pr) {
    if (!pr.node) return;
    // UIOffset is in screen pixels: divided by the same pixel_size the text draws at, so it
    // holds its place on screen at any distance. Y up, as Roblox measures it.
    pr.node->set_position(Vector3(pr.offsetX * 0.012f, -pr.offsetY * 0.012f, 0));
    pr.node->set_visible(pr.shown && pr.style == "Default");
    static const char* const digits[] = {"Zero", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine"};
    std::string key = pr.key;
    for (int k = 0; k < 10; k++) if (key == digits[k]) key = std::string(1, (char)('0' + k));
    std::string text = pr.objectText.empty() ? "" : pr.objectText + "\n";
    text += "[" + key + "]";
    if (!pr.actionText.empty()) text += "  " + pr.actionText;
    pr.node->set_text(String::utf8(text.c_str()));
    Transform3D t;
    if (auto pit = parts_.find(pr.part); pit != parts_.end()) t = pit->second.shapeLocal.affine_inverse();
    pr.node->set_transform(t);
}

// ---- sounds ------------------------------------------------------------------------
void PulseBlockzWorld::apply_sound(int64_t id, Sound& s, const Change& c) {
    const std::string& n = c.name;
    if (n == "SoundId") { s.soundId = c.value.s; load_sound(id, s); }
    else if (n == "Volume") { s.volume = (float)c.value.n; style_sound(s); }
    else if (n == "PlaybackSpeed") { s.speed = (float)c.value.n; style_sound(s); }
    else if (n == "RollOffMinDistance") { s.minDist = (float)c.value.n; style_sound(s); }
    else if (n == "RollOffMaxDistance") { s.maxDist = (float)c.value.n; style_sound(s); }
    else if (n == "RollOffMode") { s.rollOff = enumName(c.value); style_sound(s); }
    else if (n == "PlayOnRemove") s.playOnRemove = c.value.b;
    else if (n == "PlaybackRegionsEnabled") s.regions = c.value.b;
    else if (n == "PlaybackRegion" && c.value.type == Value::NumberRange) { s.region[0] = c.value.u[0]; s.region[1] = c.value.u[1]; }
    else if (n == "LoopRegion" && c.value.type == Value::NumberRange) { s.loopRegion[0] = c.value.u[0]; s.loopRegion[1] = c.value.u[1]; }
    else if (n == "SoundGroup") { s.group = c.value.type == Value::Ref ? c.value.ref : 0; style_sound(s); }
    else if (n == "Looped") {
        s.looped = c.value.b;
        if (auto* o = Object::cast_to<AudioStreamOggVorbis>(s.stream.ptr())) o->set_loop(s.looped);
        else if (auto* m = Object::cast_to<AudioStreamMP3>(s.stream.ptr())) m->set_loop(s.looped);
        else if (auto* w = Object::cast_to<AudioStreamWAV>(s.stream.ptr())) w->set_loop_mode(s.looped ? AudioStreamWAV::LOOP_FORWARD : AudioStreamWAV::LOOP_DISABLED);
    }
    else if (n == "TimePosition") {
        s.timePosition = c.value.n;
        if (s.playing) { if (auto* a = Object::cast_to<AudioStreamPlayer>(s.node)) a->seek((float)s.timePosition); else if (auto* a = Object::cast_to<AudioStreamPlayer3D>(s.node)) a->seek((float)s.timePosition); }
    }
    else if (n == "Playing") {
        // Play() while playing records it again and restarts. Stop() wrote TimePosition 0
        // first; Pause() did not, so the position stands.
        s.playing = c.value.b;
        if (s.playing) start_sound(s); else stop_sound(s, true);
    }
}

// SoundService:SetListener. Camera, the default, is Godot's own ear (it listens from the current
// camera when no listener is current); the rest place an AudioListener3D each frame.
void PulseBlockzWorld::apply_sound_service(const Change& c) {
    if (c.name == "ListenerType") listenerType_ = (int)c.value.n;
    else if (c.name == "ListenerObject") listenerObject_ = c.value.type == Value::Ref ? c.value.ref : 0;
    else if (c.name == "ListenerPosition") listenerPos_ = c.value.v;
    else if (c.name == "ListenerOrientation") listenerOrient_ = c.value.v;
}
void PulseBlockzWorld::update_listener() {
    auto camera_ear = [&]() { if (listener_ && listener_->is_current()) listener_->clear_current(); };
    if (listenerType_ == 0) { camera_ear(); return; }
    Transform3D xf;
    if (listenerType_ == 1) xf = toTransform(listenerPos_, listenerOrient_);
    else {
        auto pit = parts_.find(listenerObject_);
        if (pit == parts_.end() || !pit->second.mesh) { camera_ear(); return; }   // gone, or not built yet: the camera hears
        Transform3D part = pit->second.mesh->get_global_transform() * pit->second.shapeLocal.affine_inverse();
        if (listenerType_ == 2) {   // ObjectPosition: where the part is, facing as the camera faces
            xf = camera_ ? camera_->get_global_transform() : Transform3D();
            xf.origin = part.origin;
        } else xf = part;           // ObjectCFrame
    }
    if (!listener_) { listener_ = memnew(AudioListener3D); listener_->set_name("Listener"); add_child(listener_); }
    listener_->set_global_transform(xf);
    if (!listener_->is_current()) listener_->make_current();
}

// Where this client hears from. Godot's own attenuation follows the current listener; the
// roll-off curves for Sounds and Audio API voices are measured from here, so all three agree.
Transform3D PulseBlockzWorld::ear() const {
    if (listener_ && listener_->is_current()) return listener_->get_global_transform();
    if (camera_) return camera_->get_global_transform();
    return Transform3D();
}

// PlaybackRegionsEnabled: [lo, hi) of the clip, and the loop's [ls, le) inside it, the runtime's
// rule (rbx_runtime.cpp playbackRegion / loopRegion). False when regions are off.
bool PulseBlockzWorld::sound_regions(const Sound& s, double& lo, double& hi, double& ls, double& le) const {
    if (!s.regions || s.stream.is_null()) return false;
    const double len = s.stream->get_length();
    lo = std::max(0.0, s.region[0]); hi = std::min(len, s.region[1]);
    if (s.region[0] == s.region[1] || hi <= lo) { lo = 0; hi = len; }
    ls = std::max(s.loopRegion[0], lo); le = std::min(s.loopRegion[1], hi);
    if (s.loopRegion[0] == s.loopRegion[1] || le <= ls) { ls = lo; le = hi; }
    return true;
}

void PulseBlockzWorld::start_sound(Sound& s) {
    if (!s.node || s.stream.is_null()) return;
    double lo, hi, ls, le;
    const double from = sound_regions(s, lo, hi, ls, le) && s.timePosition < lo ? lo : s.timePosition;
    if (auto* a = Object::cast_to<AudioStreamPlayer>(s.node)) a->play((float)from);
    else if (auto* a = Object::cast_to<AudioStreamPlayer3D>(s.node)) a->play((float)from);
}

// A playing Sound with regions on, at the end of its region: Looped seeks to the loop's start,
// else it stops where the runtime's clock will (the runtime writes Playing false).
void PulseBlockzWorld::clock_sound_regions() {
    for (auto& [id, s] : sounds_) {
        double lo, hi, ls, le;
        if (!s.node || !sound_regions(s, lo, hi, ls, le)) continue;
        auto* a = Object::cast_to<AudioStreamPlayer>(s.node);
        auto* a3 = Object::cast_to<AudioStreamPlayer3D>(s.node);
        const bool playing = a ? a->is_playing() : a3 && a3->is_playing();
        if (!playing) continue;
        const double pos = a ? a->get_playback_position() : a3->get_playback_position();
        if (s.looped) { if (pos >= le) { if (a) a->seek((float)ls); else a3->seek((float)ls); } }
        else if (pos >= hi) { if (a) a->stop(); else a3->stop(); }
    }
}

void PulseBlockzWorld::stop_sound(Sound& s, bool remember) {
    if (!s.node) return;
    if (auto* a = Object::cast_to<AudioStreamPlayer>(s.node)) { if (remember && a->is_playing()) s.timePosition = a->get_playback_position(); a->stop(); }
    else if (auto* a = Object::cast_to<AudioStreamPlayer3D>(s.node)) { if (remember && a->is_playing()) s.timePosition = a->get_playback_position(); a->stop(); }
}

// A SoundGroup's volume multiplies its Sounds', and a group inside a group multiplies again.
float PulseBlockzWorld::group_volume(int64_t group) const {
    float v = 1;
    for (int hop = 0; hop < 8 && group; hop++) {
        auto it = soundGroups_.find(group);
        if (it == soundGroups_.end()) break;
        v *= (float)it->second.first;
        group = it->second.second;
    }
    return v;
}

// ---- the Audio API ------------------------------------------------------------------
// Roblox's newer audio is a graph: an AudioPlayer's Output wired through effects into an
// AudioEmitter (heard from where it is) or an AudioDeviceOutput (heard straight). Each path from
// a player to a place it is heard becomes a voice: a player node at the emitter, on a bus of its
// own carrying the effects in wire order. The listener is the camera, in the empty interaction
// group, so an emitter in any other group is not heard.

static bool audio_class(const std::string& c) {
    return c == "AudioPlayer" || c == "AudioEmitter" || c == "AudioListener" || c == "AudioDeviceOutput" || c == "Wire"
        || pulseblockz::rbx::audio::effectClass(c) || c == "AudioChannelMixer" || c == "AudioChannelSplitter";
}
static bool audio_effect_class(const std::string& c) { return pulseblockz::rbx::audio::effectClass(c); }
// What a stream passes through on its way from a player to where it is heard: an effect, or a
// splitter or mixer, which carry it whole here (one voice a path, every channel in it).
static bool audio_through_class(const std::string& c) { return audio_effect_class(c) || c == "AudioChannelMixer" || c == "AudioChannelSplitter"; }
// Whether `pin` is one a Wire can leave (`output`) or enter a node of class `c` by: the runtime's
// audioHasPin, for the graph the host builds to match Wire.Connected.
static bool audio_pin(const std::string& c, const std::map<std::string, Value>& props, const std::string& pin, bool output) {
    using namespace pulseblockz::rbx::audio;
    auto layout = [&] { auto it = props.find("Layout"); return it == props.end() ? std::string("Stereo") : (it->second.type == Value::Enum ? enumName(it->second) : it->second.s); };
    auto channel = [&] { for (const std::string& ch : channelPins(layout())) if (ch == pin) return true; return false; };
    if (c == "AudioChannelSplitter") return output ? channel() : pin == "Input";
    if (c == "AudioChannelMixer") return output ? pin == "Output" : channel();
    if (output) return pin == "Output" && (c == "AudioPlayer" || effectClass(c));
    return (pin == "Input" && (c == "AudioEmitter" || c == "AudioDeviceOutput" || effectClass(c))) || (pin == "Sidechain" && c == "AudioCompressor");
}
static Ref<AudioEffect> sound_effect_for(const std::string& cls, const std::map<std::string, Value>& p, const Ref<AudioEffect>& reuse = Ref<AudioEffect>());

void PulseBlockzWorld::apply_audio(int64_t id, AudioNode& a, const Change& c) {
    a.props[c.name] = c.value;
    if (a.className != "AudioPlayer") { audioDirty_ = true; return; }
    const std::string& n = c.name;
    if (n == "IsPlaying") {
        for (auto& [key, v] : voices_) if (v.player == id) { if (c.value.b) play_voice(v); else if (v.node) {
            if (auto* p = Object::cast_to<AudioStreamPlayer>(v.node)) p->stop(); else if (auto* p3 = Object::cast_to<AudioStreamPlayer3D>(v.node)) p3->stop();
            v.playing = false; } }
        audioDirty_ = true;
    } else if (n == "TimePosition") {
        for (auto& [key, v] : voices_) if (v.player == id && v.playing) {
            if (auto* p = Object::cast_to<AudioStreamPlayer>(v.node)) p->seek((float)c.value.n);
            else if (auto* p3 = Object::cast_to<AudioStreamPlayer3D>(v.node)) p3->seek((float)c.value.n);
        }
    } else audioDirty_ = true;
}

static double audio_num(const std::map<std::string, Value>& p, const char* k, double d) {
    auto it = p.find(k);
    return it != p.end() && it->second.type == Value::Number ? it->second.n : d;
}
static std::string audio_str(const std::map<std::string, Value>& p, const char* k) {
    auto it = p.find(k);
    if (it == p.end()) return "";
    return it->second.type == Value::Enum ? enumName(it->second) : it->second.s;
}
static bool audio_bool(const std::map<std::string, Value>& p, const char* k, bool d) {
    auto it = p.find(k);
    return it != p.end() && it->second.type == Value::Bool ? it->second.b : d;
}
static double db_to_gain(double db) { return std::pow(10.0, db / 20.0); }

// One Audio API effect as the Godot effects that do the same thing. An effect in `have` (the
// bus's, from `at` on) that is already the right kind is retuned in place: rebuilding throws
// the filter's state away, which a swept Frequency would do every step.
void PulseBlockzWorld::audio_effects_for(const AudioNode& a, const std::vector<Ref<AudioEffect>>& have, size_t& at,
                                       std::vector<Ref<AudioEffect>>& want) {
    const auto& p = a.props;
    if (audio_bool(p, "Bypass", false)) return;
    auto reuse = [&](auto* hint) {
        using T = std::remove_pointer_t<decltype(hint)>;
        Ref<T> r;
        if (at < have.size()) r = Ref<T>(Object::cast_to<T>(have[at].ptr()));
        if (r.is_null()) r.instantiate();
        at++;
        return r;
    };
    if (a.className == "AudioReverb") {
        Ref<AudioEffectReverb> fx = reuse((AudioEffectReverb*)nullptr);
        fx->set_room_size((float)std::clamp(audio_num(p, "DecayTime", 1.5) / 6.0, 0.0, 1.0));
        fx->set_predelay_msec((float)std::clamp(audio_num(p, "EarlyDelayTime", 0.02) * 1000.0 + audio_num(p, "LateDelayTime", 0.04) * 1000.0, 20.0, 500.0));
        // High frequencies dying sooner than the whole tail is what damping is.
        fx->set_damping((float)std::clamp(1.0 - audio_num(p, "DecayRatio", 0.5), 0.0, 1.0));
        fx->set_spread((float)std::clamp(audio_num(p, "Diffusion", 1.0), 0.0, 1.0));
        fx->set_wet((float)std::clamp(db_to_gain(audio_num(p, "WetLevel", -6.0)), 0.0, 1.0));
        fx->set_dry((float)std::clamp(db_to_gain(audio_num(p, "DryLevel", 0.0)), 0.0, 1.0));
        want.push_back(fx);
        return;
    }
    // The effects the SoundEffect classes already map: the same Godot effect, the members renamed.
    auto viaSoundEffect = [&](const char* cls, std::map<std::string, Value> q) {
        Ref<AudioEffect> reuse;
        if (at < have.size()) reuse = have[at];
        Ref<AudioEffect> fx = sound_effect_for(cls, q, reuse);
        if (fx.is_null()) return;
        at++;
        want.push_back(fx);
    };
    auto num = [&](const char* k, double d) { return Value::number(audio_num(p, k, d)); };
    if (a.className == "AudioFader") {
        Ref<AudioEffectAmplify> fx = reuse((AudioEffectAmplify*)nullptr);
        const double volume = std::clamp(audio_num(p, "Volume", 1.0), 0.0, 3.0);
        fx->set_volume_db(volume <= 0 ? -80.f : (float)(20.0 * std::log10(volume)));
        want.push_back(fx);
        return;
    }
    if (a.className == "AudioLimiter") {
        Ref<AudioEffectHardLimiter> fx = reuse((AudioEffectHardLimiter*)nullptr);
        fx->set_ceiling_db((float)std::clamp(audio_num(p, "MaxLevel", 0.0), -24.0, 0.0));
        fx->set_release((float)std::clamp(audio_num(p, "Release", 0.01), 0.01, 3.0));
        want.push_back(fx);
        return;
    }
    if (a.className == "AudioEcho") { viaSoundEffect("EchoSoundEffect", {{"Delay", num("DelayTime", 1)}, {"Feedback", num("Feedback", 0.5)}, {"DryLevel", num("DryLevel", 0)}, {"WetLevel", num("WetLevel", 0)}}); return; }
    if (a.className == "AudioCompressor") {
        viaSoundEffect("CompressorSoundEffect", {{"Attack", num("Attack", 0.1)}, {"Release", num("Release", 0.1)}, {"Ratio", num("Ratio", 40)},
                                                 {"Threshold", num("Threshold", -40)}, {"GainMakeup", num("MakeupGain", 0)}});
        return;
    }
    if (a.className == "AudioDistortion") { viaSoundEffect("DistortionSoundEffect", {{"Level", num("Level", 0.5)}}); return; }
    if (a.className == "AudioEqualizer") { viaSoundEffect("EqualizerSoundEffect", {{"LowGain", num("LowGain", 0)}, {"MidGain", num("MidGain", 0)}, {"HighGain", num("HighGain", 0)}}); return; }
    if (a.className == "AudioChorus" || a.className == "AudioFlanger") {
        viaSoundEffect(a.className == "AudioChorus" ? "ChorusSoundEffect" : "FlangeSoundEffect", {{"Depth", num("Depth", 0.45)}, {"Mix", num("Mix", 0.85)}, {"Rate", num("Rate", 5)}});
        return;
    }
    if (a.className == "AudioPitchShifter") { viaSoundEffect("PitchShiftSoundEffect", {{"Octave", num("Pitch", 1.25)}}); return; }
    if (a.className == "AudioTremolo") { viaSoundEffect("TremoloSoundEffect", {{"Depth", num("Depth", 1)}, {"Duty", num("Duty", 0.5)}, {"Frequency", num("Frequency", 5)}}); return; }
    // AudioGate, AudioChannelMixer and AudioChannelSplitter pass the stream through unchanged.
    if (a.className != "AudioFilter") return;
    const std::string type = audio_str(p, "FilterType");
    const float freq = (float)std::clamp(audio_num(p, "Frequency", 2000.0), 20.0, 22000.0);
    // Godot's resonance is the biquad's Q itself (AudioFilterSW: alpha = sin w / 2Q).
    const float q = (float)std::clamp(audio_num(p, "Q", 0.707), 0.1, 10.0);
    const double gainDb = std::clamp(audio_num(p, "Gain", 0.0), -30.0, 30.0);
    // Godot counts slope in biquads: its "6dB" is one biquad, which is Roblox's 12dB per octave;
    // "12dB" is Roblox's 24, "24dB" is Roblox's 48. Roblox's 6dB is one pole, and the gentlest
    // Godot has is the single biquad with its resonance flattened.
    auto slope = [&](AudioEffectFilter* f) {
        if (type.find("48dB") != std::string::npos) f->set_db(AudioEffectFilter::FILTER_24DB);
        else if (type.find("24dB") != std::string::npos) f->set_db(AudioEffectFilter::FILTER_12DB);
        else f->set_db(AudioEffectFilter::FILTER_6DB);
        f->set_cutoff(freq);
        f->set_resonance(type == "Lowpass6dB" ? 0.5f : q);
    };
    if (type.rfind("Lowpass", 0) == 0) { Ref<AudioEffectLowPassFilter> f = reuse((AudioEffectLowPassFilter*)nullptr); slope(f.ptr()); want.push_back(f); }
    else if (type.rfind("Highpass", 0) == 0) { Ref<AudioEffectHighPassFilter> f = reuse((AudioEffectHighPassFilter*)nullptr); slope(f.ptr()); want.push_back(f); }
    else if (type == "Bandpass") { Ref<AudioEffectBandPassFilter> f = reuse((AudioEffectBandPassFilter*)nullptr); f->set_cutoff(freq); f->set_resonance(q); want.push_back(f); }
    else if (type == "Notch") { Ref<AudioEffectNotchFilter> f = reuse((AudioEffectNotchFilter*)nullptr); f->set_cutoff(freq); f->set_resonance(q); want.push_back(f); }
    else if (type == "LowShelf" || type == "HighShelf") {
        Ref<AudioEffectFilter> f = type == "LowShelf" ? Ref<AudioEffectFilter>(reuse((AudioEffectLowShelfFilter*)nullptr))
                                                      : Ref<AudioEffectFilter>(reuse((AudioEffectHighShelfFilter*)nullptr));
        f->set_cutoff(freq); f->set_resonance(q);
        f->set_gain((float)std::clamp(db_to_gain(gainDb), 0.0, 4.0));
        want.push_back(f);
    } else {
        // Peak: Godot has no peaking filter, so the band of a 21-band EQ nearest Frequency takes Gain.
        Ref<AudioEffectEQ21> eq = reuse((AudioEffectEQ21*)nullptr);
        int nearest = 0;
        double best = 1e18;
        for (int b = 0; b < eq->get_band_count(); b++) {
            eq->set_band_gain_db(b, 0);
            const double hz = 31.25 * std::pow(2.0, b / 2.0);   // 21 bands, half an octave apart from 31 Hz
            const double off = std::fabs(std::log2(hz / freq));
            if (off < best) { best = off; nearest = b; }
        }
        eq->set_band_gain_db(nearest, (float)gainDb);
        want.push_back(eq);
    }
}

void PulseBlockzWorld::sync_voice_bus(AudioVoice& v) {
    AudioServer* audio = AudioServer::get_singleton();
    if (!audio || !v.node) return;
    const String want = String("pblockz_audio_") + String::num_int64(v.player) + "_" + String::num_int64(v.sink);
    int at = -1;
    for (int i = 0; i < audio->get_bus_count(); i++) if (audio->get_bus_name(i) == want) { at = i; break; }
    if (at < 0) {
        at = audio->get_bus_count();
        audio->add_bus(at);
        audio->set_bus_name(at, want);
        audio->set_bus_send(at, "Master");
    }
    std::vector<Ref<AudioEffect>> have, wanted;
    for (int k = 0; k < audio->get_bus_effect_count(at); k++) have.push_back(audio->get_bus_effect(at, k));
    size_t cursor = 0;
    for (int64_t fx : v.chain) if (auto it = audio_.find(fx); it != audio_.end()) audio_effects_for(it->second, have, cursor, wanted);
    bool same = have.size() == wanted.size();
    for (size_t k = 0; same && k < have.size(); k++) same = have[k].ptr() == wanted[k].ptr();
    if (!same) {
        for (int k = audio->get_bus_effect_count(at) - 1; k >= 0; k--) audio->remove_bus_effect(at, k);
        for (auto& e : wanted) audio->add_bus_effect(at, e);
    }
    if (auto* p = Object::cast_to<AudioStreamPlayer>(v.node)) p->set_bus(want);
    else if (auto* p3 = Object::cast_to<AudioStreamPlayer3D>(v.node)) p3->set_bus(want);
}

void PulseBlockzWorld::play_voice(AudioVoice& v) {
    if (!v.node || v.stream.is_null()) return;
    auto it = audio_.find(v.player);
    if (it == audio_.end()) return;
    const float from = (float)std::max(0.0, audio_num(it->second.props, "TimePosition", 0));
    if (auto* p = Object::cast_to<AudioStreamPlayer>(v.node)) p->play(from);
    else if (auto* p3 = Object::cast_to<AudioStreamPlayer3D>(v.node)) p3->play(from);
    v.playing = true;
}

static bool isCloudAsset(const std::string& id);
static std::string cloudNumber(const std::string& id);

// TimeLength / IsReady back to the runtime once an AudioPlayer's asset is settled. A cloud
// asset still on its way is not reported; cloud_arrived marks the graph dirty when it lands.
void PulseBlockzWorld::report_audio_player(int64_t player, const std::string& asset, bool ok) {
    if (asset.empty()) return;
    if (!ok && isCloudAsset(asset)) {
        auto it = cloudAssets_.find(cloudNumber(asset));
        if (it == cloudAssets_.end() || (!it->second.failed && it->second.file.empty())) return;   // on its way
    }
    if (audioReported_[player] == asset) return;
    audioReported_[player] = asset;
    Ref<AudioStream> s = ok ? load_stream(asset) : Ref<AudioStream>();
    assetWrites_.push_back({player, "TimeLength", Value::number(s.is_valid() ? s->get_length() : 0)});
    assetWrites_.push_back({player, "IsReady", Value::boolean(s.is_valid())});
}

void PulseBlockzWorld::sync_audio() {
    audioDirty_ = false;
    // Every AudioPlayer's length, voice or no voice: a dedicated server has no output and makes
    // no voices, and a script waiting on IsReady there would wait forever.
    for (auto& [id, a] : audio_) {
        if (a.className != "AudioPlayer" || !entries_.count(id)) continue;
        const std::string asset = audio_str(a.props, "Asset");
        if (asset.empty()) continue;
        if (audioReported_[id] == asset) continue;
        report_audio_player(id, asset, load_stream(asset).is_valid());
    }
    // The wires that could carry anything: both ends here, and the pins they name real.
    struct W { int64_t src, dst; };
    std::vector<W> live;
    // Creation order, refusing a wire that closes a loop: the runtime's rule for Wire.Connected,
    // so what is heard is what a script reads.
    std::vector<int64_t> wireOrder;
    for (auto& [id, a] : audio_) if (a.className == "Wire") wireOrder.push_back(id);
    std::sort(wireOrder.begin(), wireOrder.end(), [](int64_t x, int64_t y) { return std::llabs(x) < std::llabs(y); });
    auto reaches = [&](int64_t from, int64_t to) {
        std::vector<int64_t> stack{from};
        std::unordered_set<int64_t> seen;
        while (!stack.empty()) {
            int64_t at = stack.back(); stack.pop_back();
            if (at == to) return true;
            if (!seen.insert(at).second) continue;
            for (const W& w : live) if (w.src == at) stack.push_back(w.dst);
        }
        return false;
    };
    for (int64_t id : wireOrder) {
        const AudioNode& a = audio_[id];
        if (!entries_.count(id)) continue;
        auto s = a.props.find("SourceInstance"), t = a.props.find("TargetInstance");
        if (s == a.props.end() || t == a.props.end() || s->second.type != Value::Ref || t->second.type != Value::Ref) continue;
        auto sa = audio_.find(s->second.ref), ta = audio_.find(t->second.ref);
        if (sa == audio_.end() || ta == audio_.end() || !entries_.count(s->second.ref) || !entries_.count(t->second.ref)) continue;
        // A player or something a stream passes through at the source end; an emitter, an output
        // or a through node at the target. What ends elsewhere (an analyzer, a recorder) makes no
        // voice, and a Sidechain carries nothing here.
        const bool srcOut = (sa->second.className == "AudioPlayer" || audio_through_class(sa->second.className))
            && audio_pin(sa->second.className, sa->second.props, audio_str(a.props, "SourceName"), true);
        const bool dstIn = (ta->second.className == "AudioEmitter" || ta->second.className == "AudioDeviceOutput" || audio_through_class(ta->second.className))
            && audio_pin(ta->second.className, ta->second.props, audio_str(a.props, "TargetName"), false)
            && audio_str(a.props, "TargetName") != "Sidechain";
        if (srcOut && dstIn && !reaches(t->second.ref, s->second.ref)) live.push_back({s->second.ref, t->second.ref});
    }
    // Every player-to-sink path, walked back from each sink through the effects.
    std::map<std::pair<int64_t, int64_t>, std::vector<int64_t>> paths;
    for (auto& [sinkId, sink] : audio_) {
        if (sink.className != "AudioEmitter" && sink.className != "AudioDeviceOutput") continue;
        if (sink.className == "AudioEmitter" && !audio_str(sink.props, "AudioInteractionGroup").empty()) continue;
        if (sink.className == "AudioDeviceOutput") {
            auto pl = sink.props.find("Player");
            if (pl != sink.props.end() && pl->second.type == Value::Ref && pl->second.ref && pl->second.ref != localPlayerId_) continue;
        }
        std::function<void(int64_t, std::vector<int64_t>, int)> back = [&](int64_t into, std::vector<int64_t> chain, int depth) {
            if (depth > 32) return;
            for (const W& w : live) {
                if (w.dst != into) continue;
                auto src = audio_.find(w.src);
                if (src == audio_.end()) continue;
                if (src->second.className == "AudioPlayer") paths[{w.src, sinkId}] = chain;
                else if (audio_through_class(src->second.className)) {
                    std::vector<int64_t> more = chain;
                    more.insert(more.begin(), w.src);
                    back(w.src, more, depth + 1);
                }
            }
        };
        back(sinkId, {}, 0);
    }
    // A voice whose path is gone.
    for (auto it = voices_.begin(); it != voices_.end();) {
        if (paths.count(it->first)) { ++it; continue; }
        if (it->second.node) it->second.node->queue_free();
        if (AudioServer* audio = AudioServer::get_singleton()) {
            const String name = String("pblockz_audio_") + String::num_int64(it->second.player) + "_" + String::num_int64(it->second.sink);
            for (int i = 0; i < audio->get_bus_count(); i++) if (audio->get_bus_name(i) == name) { audio->remove_bus(i); break; }
        }
        it = voices_.erase(it);
    }
    for (auto& [key, chain] : paths) {
        AudioVoice& v = voices_[key];
        v.player = key.first; v.sink = key.second; v.chain = chain;
        const AudioNode& player = audio_[v.player];
        const AudioNode& sink = audio_[v.sink];
        // Where it is heard from: an emitter's part (at its attachment), else everywhere.
        int64_t partId = 0;
        Transform3D at;
        if (sink.className == "AudioEmitter") {
            int64_t holder = entries_.count(v.sink) ? entries_[v.sink].parent : kNoParent;
            if (audio_str(sink.props, "PositionType") == "Instance") {
                auto pi = sink.props.find("PositionInstance");
                holder = pi != sink.props.end() && pi->second.type == Value::Ref ? pi->second.ref : kNoParent;
            }
            if (auto ait = attachments_.find(holder); ait != attachments_.end()) {
                at = ait->second;
                auto ae = entries_.find(holder);
                holder = ae == entries_.end() ? holder : ae->second.parent;
            }
            if (auto pit = parts_.find(holder); pit != parts_.end() && pit->second.mesh) partId = holder;
            else if (holder != cameraId_ || !cameraId_) continue;   // nowhere: an emitter with no place is silent
        }
        if (v.node && v.part != partId) { v.node->queue_free(); v.node = nullptr; v.playing = false; }
        if (!v.node && client_) {
            if (partId) {
                auto* p3 = memnew(AudioStreamPlayer3D);
                p3->set_attenuation_model(AudioStreamPlayer3D::ATTENUATION_DISABLED);   // attenuate_voices does it, Roblox's way
                p3->set_max_distance(0);
                parts_[partId].mesh->add_child(p3);
                p3->set_transform(parts_[partId].shapeLocal.affine_inverse() * at);
                v.node = p3;
            } else {
                auto* p = memnew(AudioStreamPlayer);
                add_child(p);
                v.node = p;
            }
            v.node->set_name("AudioVoice");
            v.part = partId;
        }
        if (!v.node) continue;
        // The asset, once per player, and its length back to the runtime so the clock can run.
        const std::string asset = audio_str(player.props, "Asset");
        if (v.asset != asset || (v.stream.is_null() && !asset.empty())) {
            v.asset = asset;
            Ref<AudioStream> base = load_stream(asset);
            v.stream = base.is_valid() ? Ref<AudioStream>(base->duplicate()) : Ref<AudioStream>();
            if (auto* p = Object::cast_to<AudioStreamPlayer>(v.node)) p->set_stream(v.stream);
            else if (auto* p3 = Object::cast_to<AudioStreamPlayer3D>(v.node)) p3->set_stream(v.stream);
            v.playing = false;
            report_audio_player(v.player, asset, v.stream.is_valid());
        }
        const bool looping = audio_bool(player.props, "Looping", false);
        if (auto* w = Object::cast_to<AudioStreamWAV>(v.stream.ptr())) w->set_loop_mode(looping ? AudioStreamWAV::LOOP_FORWARD : AudioStreamWAV::LOOP_DISABLED);
        else if (auto* o = Object::cast_to<AudioStreamOggVorbis>(v.stream.ptr())) o->set_loop(looping);
        else if (auto* m = Object::cast_to<AudioStreamMP3>(v.stream.ptr())) m->set_loop(looping);
        const float speed = (float)std::clamp(audio_num(player.props, "PlaybackSpeed", 1.0), 0.01, 20.0);
        if (auto* p = Object::cast_to<AudioStreamPlayer>(v.node)) p->set_pitch_scale(speed);
        else if (auto* p3 = Object::cast_to<AudioStreamPlayer3D>(v.node)) p3->set_pitch_scale(speed);
        sync_voice_bus(v);
        const bool shouldPlay = audio_bool(player.props, "IsPlaying", false);
        if (shouldPlay && !v.playing) play_voice(v);
        else if (!shouldPlay && v.playing) {
            if (auto* p = Object::cast_to<AudioStreamPlayer>(v.node)) p->stop(); else if (auto* p3 = Object::cast_to<AudioStreamPlayer3D>(v.node)) p3->stop();
            v.playing = false;
        }
    }
    attenuate_voices();
}

// Volume, and at an emitter the distance from the listener, every frame.
void PulseBlockzWorld::attenuate_voices() {
    if (voices_.empty()) return;
    const Vector3 ear = this->ear().origin;
    for (auto& [key, v] : voices_) {
        if (!v.node) continue;
        auto pit = audio_.find(v.player);
        if (pit == audio_.end()) continue;
        double volume = std::clamp(audio_num(pit->second.props, "Volume", 1.0), 0.0, 10.0);
        if (auto* p3 = Object::cast_to<AudioStreamPlayer3D>(v.node)) {
            auto sit = audio_.find(v.sink);
            if (sit != audio_.end()) {
                const auto& sp = sit->second.props;
                auto b = sp.find("DistanceAttenuationBounds");
                const double lo = b != sp.end() && b->second.type == Value::NumberRange ? b->second.u[0] : 4;
                const double hi = b != sp.end() && b->second.type == Value::NumberRange ? b->second.u[1] : 10000;
                const double d = (p3->get_global_position() - ear).length();
                volume *= pulseblockz::rbx::audio::emitterDistanceGain(audio_str(sp, "DistanceAttenuationMode"), lo, hi,
                                                                    pulseblockz::rbx::audio::parseCurve(audio_str(sp, "DistanceAttenuation")), d);
            }
            p3->set_volume_db(volume <= 0 ? -80.f : (float)(20.0 * std::log10(volume)));
        } else if (auto* p = Object::cast_to<AudioStreamPlayer>(v.node)) {
            p->set_volume_db(volume <= 0 ? -80.f : (float)(20.0 * std::log10(volume)));
        }
    }
}

// One SoundEffect as a Godot effect. Roblox parents SoundEffects to a Sound and mixes them per
// voice; Godot attaches effects to buses, so a Sound with effects gets a bus of its own and one
// with none keeps the default. Eight of the nine classes map onto a Godot effect; tremolo is in
// pulseblockz_tremolo.cpp. A `reuse` of the right kind is retuned rather than replaced.
static Ref<AudioEffect> sound_effect_for(const std::string& cls,
                                         const std::map<std::string, Value>& p,
                                         const Ref<AudioEffect>& reuse) {
    auto num = [&](const char* n, double d) {
        auto it = p.find(n);
        return it != p.end() && it->second.type == Value::Number ? it->second.n : d;
    };
    if (cls == "ReverbSoundEffect") {
        Ref<AudioEffectReverb> fx = Object::cast_to<AudioEffectReverb>(reuse.ptr()); if (fx.is_null()) fx.instantiate();
        // DecayTime is seconds of tail; Godot's room_size is 0..1 and behaves like one.
        fx->set_room_size((float)std::clamp(num("DecayTime", 1.5) / 6.0, 0.0, 1.0));
        fx->set_damping((float)std::clamp(1.0 - num("Diffusion", 1.0), 0.0, 1.0));
        // Roblox's levels are decibels, -80 to 10, and Godot wants 0..1.
        fx->set_wet((float)std::clamp(std::pow(10.0, num("WetLevel", 0.0) / 20.0), 0.0, 1.0));
        fx->set_dry((float)std::clamp(std::pow(10.0, num("DryLevel", 0.0) / 20.0), 0.0, 1.0));
        return fx;
    }
    if (cls == "EqualizerSoundEffect") {
        Ref<AudioEffectEQ6> fx = Object::cast_to<AudioEffectEQ6>(reuse.ptr()); if (fx.is_null()) fx.instantiate();
        // Six bands over three controls: two bands each for LowGain, MidGain and HighGain.
        const double lo = num("LowGain", 0), mid = num("MidGain", 0), hi = num("HighGain", 0);
        const double gains[6] = {lo, lo, mid, mid, hi, hi};
        for (int i = 0; i < 6; i++) fx->set_band_gain_db(i, (float)gains[i]);
        return fx;
    }
    if (cls == "EchoSoundEffect") {
        Ref<AudioEffectDelay> fx = Object::cast_to<AudioEffectDelay>(reuse.ptr()); if (fx.is_null()) fx.instantiate();
        fx->set_tap1_active(true);
        fx->set_tap1_delay_ms((float)(num("Delay", 1.0) * 1000.0));
        fx->set_tap1_level_db((float)num("WetLevel", 0.0));
        fx->set_tap2_active(false);
        fx->set_feedback_active(true);
        fx->set_feedback_delay_ms((float)(num("Delay", 1.0) * 1000.0));
        fx->set_feedback_level_db((float)(-6.0 * (1.0 - std::clamp(num("Feedback", 0.5), 0.0, 1.0)) - 6.0));
        fx->set_dry((float)std::clamp(std::pow(10.0, num("DryLevel", 0.0) / 20.0), 0.0, 1.0));
        return fx;
    }
    if (cls == "DistortionSoundEffect") {
        Ref<AudioEffectDistortion> fx = Object::cast_to<AudioEffectDistortion>(reuse.ptr()); if (fx.is_null()) fx.instantiate();
        fx->set_mode(AudioEffectDistortion::MODE_OVERDRIVE);
        fx->set_drive((float)std::clamp(num("Level", 0.75), 0.0, 1.0));
        return fx;
    }
    if (cls == "ChorusSoundEffect" || cls == "FlangeSoundEffect") {
        Ref<AudioEffectChorus> fx = Object::cast_to<AudioEffectChorus>(reuse.ptr()); if (fx.is_null()) fx.instantiate();
        fx->set_voice_count(2);
        // A flange is a chorus with a much shorter delay, so one class serves both.
        const float base = cls == "FlangeSoundEffect" ? 3.0f : 15.0f;
        for (int v = 0; v < 2; v++) {
            fx->set_voice_delay_ms(v, base + v * 2.0f);
            fx->set_voice_depth_ms(v, (float)(num("Depth", 0.5) * (cls == "FlangeSoundEffect" ? 2.0 : 6.0)));
            fx->set_voice_rate_hz(v, (float)num("Rate", 1.0));
            fx->set_voice_level_db(v, 0.0f);
        }
        fx->set_wet((float)std::clamp(num("Mix", 0.5), 0.0, 1.0));
        fx->set_dry((float)std::clamp(1.0 - num("Mix", 0.5), 0.0, 1.0));
        return fx;
    }
    if (cls == "CompressorSoundEffect") {
        Ref<AudioEffectCompressor> fx = Object::cast_to<AudioEffectCompressor>(reuse.ptr()); if (fx.is_null()) fx.instantiate();
        fx->set_threshold((float)num("Threshold", -20.0));
        fx->set_ratio((float)std::max(1.0, num("Ratio", 4.0)));
        fx->set_attack_us((float)std::max(1.0, num("Attack", 0.1) * 1000000.0));
        fx->set_release_ms((float)std::max(1.0, num("Release", 0.1) * 1000.0));
        fx->set_gain((float)num("GainMakeup", 0.0));
        return fx;
    }
    if (cls == "PitchShiftSoundEffect") {
        Ref<AudioEffectPitchShift> fx = Object::cast_to<AudioEffectPitchShift>(reuse.ptr()); if (fx.is_null()) fx.instantiate();
        // Octave is a multiplier on Roblox, not a count of octaves.
        fx->set_pitch_scale((float)std::clamp(num("Octave", 1.25), 0.25, 4.0));
        return fx;
    }
    if (cls == "TremoloSoundEffect") {
        // Godot ships no tremolo: amplitude modulation, in pulseblockz_tremolo.cpp.
        Ref<PulseBlockzTremolo> fx = Object::cast_to<PulseBlockzTremolo>(reuse.ptr()); if (fx.is_null()) fx.instantiate();
        fx->set_depth((float)std::clamp(num("Depth", 1.0), 0.0, 1.0));
        fx->set_frequency((float)std::max(0.0, num("Frequency", 5.0)));
        fx->set_duty((float)std::clamp(num("Duty", 0.5), 0.0, 1.0));
        return fx;
    }
    return Ref<AudioEffect>();
}

// The bus a Sound's effects live on, made when it has any and dropped when it does not. Godot's
// buses are a flat numbered list and removing one renumbers the rest, so a bus is found by name
// each time: a remembered index would point at another sound's bus after a removal.
void PulseBlockzWorld::sync_sound_bus(int64_t id, Sound& s) {
    AudioServer* audio = AudioServer::get_singleton();
    if (!audio) return;

    // Which effects belong to this Sound, in the order they were parented.
    std::vector<int64_t> mine;
    for (auto& [eid, fx] : effects_) {
        auto ee = entries_.find(eid);
        if (ee == entries_.end() || ee->second.parent != id) continue;
        auto on = fx.props.find("Enabled");
        if (on != fx.props.end() && on->second.type == Value::Bool && !on->second.b) continue;
        if (fx.className.find("SoundEffect") == std::string::npos) continue;
        mine.push_back(eid);
    }

    const String want = String("pblockz_sfx_") + String::num_int64(id);
    int at = -1;
    for (int i = 0; i < audio->get_bus_count(); i++) if (audio->get_bus_name(i) == want) { at = i; break; }

    if (mine.empty()) {
        // Back to the default bus.
        if (at >= 0) audio->remove_bus(at);
        if (auto* a = Object::cast_to<AudioStreamPlayer>(s.node)) a->set_bus("Master");
        else if (auto* a = Object::cast_to<AudioStreamPlayer3D>(s.node)) a->set_bus("Master");
        return;
    }

    if (at < 0) {
        at = audio->get_bus_count();
        audio->add_bus(at);
        audio->set_bus_name(at, want);
        audio->set_bus_send(at, "Master");
    }
    // Retuned in place when the chain is the same effects in the same order. Rebuilding throws
    // away the effect's state -- a reverb's tail, a delay's echoes -- on every property change.
    bool same = audio->get_bus_effect_count(at) == (int)mine.size();
    for (int k = 0; same && k < (int)mine.size(); k++) {
        Ref<AudioEffect> have = audio->get_bus_effect(at, k);
        same = sound_effect_for(effects_[mine[k]].className, effects_[mine[k]].props, have).ptr() == have.ptr();
    }
    if (same) {
        if (auto* a = Object::cast_to<AudioStreamPlayer>(s.node)) a->set_bus(want);
        else if (auto* a = Object::cast_to<AudioStreamPlayer3D>(s.node)) a->set_bus(want);
        return;
    }
    // Rebuilt otherwise: one can go from the middle, and the order matters to how it sounds.
    for (int k = audio->get_bus_effect_count(at) - 1; k >= 0; k--) audio->remove_bus_effect(at, k);
    for (int64_t eid : mine) {
        Ref<AudioEffect> made = sound_effect_for(effects_[eid].className, effects_[eid].props);
        if (made.is_valid()) audio->add_bus_effect(at, made);
    }
    if (auto* a = Object::cast_to<AudioStreamPlayer>(s.node)) a->set_bus(want);
    else if (auto* a = Object::cast_to<AudioStreamPlayer3D>(s.node)) a->set_bus(want);
}

void PulseBlockzWorld::style_sound(Sound& s) {
    if (!s.node) return;
    float volume = s.volume * group_volume(s.group);
    float db = volume <= 0 ? -80.f : 20.f * std::log10(volume);
    if (auto* a = Object::cast_to<AudioStreamPlayer>(s.node)) { a->set_volume_db(db); a->set_pitch_scale(std::max(s.speed, 0.01f)); }
    else if (auto* a = Object::cast_to<AudioStreamPlayer3D>(s.node)) {
        a->set_volume_db(db);
        a->set_pitch_scale(std::max(s.speed, 0.01f));
        a->set_unit_size(std::max(s.minDist, 0.01f));
        a->set_max_distance(std::max(s.maxDist, 0.f));
        a->set_max_db(0);                                   // flat inside RollOffMinDistance
        // Godot's inverse-distance with unit_size at RollOffMinDistance is exactly Roblox's
        // Inverse (minDist / d). The other three have no Godot model and are worked out per
        // frame against the listener in attenuate_sounds().
        a->set_attenuation_model(s.rollOff == "Inverse" ? AudioStreamPlayer3D::ATTENUATION_INVERSE_DISTANCE
                                                        : AudioStreamPlayer3D::ATTENUATION_DISABLED);
        Transform3D t;
        if (auto pit = parts_.find(s.part); pit != parts_.end()) t = pit->second.shapeLocal.affine_inverse() * s.at;
        a->set_transform(t);
    }
}

// The three rolloffs Godot has no model for; Inverse (minDist / d) is Godot's own and is set in
// style_sound.
//
//   Linear         (maxDist - d) / (maxDist - minDist)
//   LinearSquare   that, squared
//   InverseTapered the smaller of inverse and linear-square at every distance
//
// Flat inside RollOffMinDistance and silent past RollOffMaxDistance, as all four are.
void PulseBlockzWorld::attenuate_sounds() {
    if (!camera_ && !(listener_ && listener_->is_current())) return;
    const Vector3 ear = this->ear().origin;
    for (auto& [id, s] : sounds_) {
        if (s.rollOff == "Inverse") continue;
        auto* a = Object::cast_to<AudioStreamPlayer3D>(s.node);
        if (!a) continue;
        float lo = std::max(s.minDist, 0.01f), hi = std::max(s.maxDist, lo + 0.01f);
        double d = (a->get_global_position() - ear).length();
        double gain;
        if (d <= lo) gain = 1;
        else if (d >= hi) gain = 0;
        else {
            double linear = (hi - d) / (hi - lo);
            if (s.rollOff == "Linear") gain = linear;
            else if (s.rollOff == "LinearSquare") gain = linear * linear;
            else gain = std::min(lo / d, linear * linear);     // InverseTapered
        }
        float volume = s.volume * group_volume(s.group) * (float)gain;
        a->set_volume_db(volume <= 0 ? -80.f : 20.f * std::log10(volume));
    }
}

// PlaybackLoudness, 0..1000 as Roblox reports it: RMS over the window now playing, scaled by
// Volume. WAVs only, since an Ogg would have to be decoded to be measured.
void PulseBlockzWorld::report_loudness(std::vector<HostWrite>& writes) {
    attenuate_sounds();
    clock_sound_regions();
    for (auto& [id, s] : sounds_) {
        double pos = -1;
        bool playing = false;
        if (auto* a = Object::cast_to<AudioStreamPlayer>(s.node)) { playing = a->is_playing(); if (playing) pos = a->get_playback_position(); }
        else if (auto* a = Object::cast_to<AudioStreamPlayer3D>(s.node)) { playing = a->is_playing(); if (playing) pos = a->get_playback_position(); }

        double loud = 0;
        auto* wav = playing ? Object::cast_to<AudioStreamWAV>(s.stream.ptr()) : nullptr;
        if (wav) {
            const PackedByteArray data = wav->get_data();
            const int rate = wav->get_mix_rate();
            const bool stereo = wav->is_stereo();
            const bool is16 = wav->get_format() == AudioStreamWAV::FORMAT_16_BITS;
            const int chans = stereo ? 2 : 1;
            const int bytesPerFrame = (is16 ? 2 : 1) * chans;
            if (rate > 0 && bytesPerFrame > 0 && data.size() >= bytesPerFrame) {
                const int64_t frames = data.size() / bytesPerFrame;
                // 50 ms: long enough not to twitch on one sample, short enough to follow a drum.
                const int64_t want = std::min<int64_t>(frames, rate / 20);
                int64_t at = (int64_t)(pos * rate);
                if (at < 0) at = 0;
                if (at + want > frames) at = std::max<int64_t>(0, frames - want);
                double sum = 0;
                const uint8_t* raw = data.ptr();
                for (int64_t f = 0; f < want; f++) {
                    const int64_t base = (at + f) * bytesPerFrame;
                    for (int c = 0; c < chans; c++) {
                        double v;
                        if (is16) {
                            const int64_t o = base + c * 2;
                            v = (double)(int16_t)((uint16_t)raw[o] | ((uint16_t)raw[o + 1] << 8)) / 32768.0;
                        } else {
                            v = (double)(int8_t)raw[base + c] / 128.0;
                        }
                        sum += v * v;
                    }
                }
                const double rms = want > 0 ? std::sqrt(sum / (double)(want * chans)) : 0.0;
                loud = std::clamp(rms * s.volume * 1000.0, 0.0, 1000.0);
            }
        }
        // Only when it moves by a whole unit: a meter is not worth a write per sound per frame.
        if (std::fabs(loud - s.reportedLoudness) >= 1.0 || (loud == 0 && s.reportedLoudness != 0)) {
            s.reportedLoudness = loud;
            writes.push_back({id, "PlaybackLoudness", Value::number(loud)});
        }
    }
}

// The SoundId became a stream, or failed to: TimeLength / IsLoaded go back next frame, and a
// Sound already Playing starts.
void PulseBlockzWorld::load_sound(int64_t id, Sound& s) {
    if (s.loadedId == s.soundId && (s.stream.is_valid() || s.soundId.empty())) return;
    Ref<AudioStream> base = load_stream(s.soundId);
    s.loadedId = s.soundId;
    s.stream = base.is_valid() ? Ref<AudioStream>(base->duplicate()) : Ref<AudioStream>();
    if (auto* o = Object::cast_to<AudioStreamOggVorbis>(s.stream.ptr())) o->set_loop(s.looped);
    else if (auto* m = Object::cast_to<AudioStreamMP3>(s.stream.ptr())) m->set_loop(s.looped);
    else if (auto* w = Object::cast_to<AudioStreamWAV>(s.stream.ptr())) w->set_loop_mode(s.looped ? AudioStreamWAV::LOOP_FORWARD : AudioStreamWAV::LOOP_DISABLED);
    if (auto* a = Object::cast_to<AudioStreamPlayer>(s.node)) a->set_stream(s.stream);
    else if (auto* a = Object::cast_to<AudioStreamPlayer3D>(s.node)) a->set_stream(s.stream);
    bool ok = s.stream.is_valid();
    assetWrites_.push_back({id, "TimeLength", Value::number(ok ? s.stream->get_length() : 0)});
    assetWrites_.push_back({id, "IsLoaded", Value::boolean(ok)});
    if (ok && s.playing) start_sound(s);
}

// Only a file inside the project has a .godot/imported entry; asking the ResourceLoader for one
// anywhere else prints its way to a null, so those are read straight off disk.
// Whether Godot's ResourceLoader could know this file. Only what the project imported, which
// lives under res://. A user:// path is bytes this program wrote at run time -- a mesh fetched
// from Roblox, an asset off the chain -- and asking the loader for one prints three errors
// before falling through to the readers that can actually read it: three a mesh, and a place
// brings hundreds.
static bool importedResource(const String& path) {
    return path.begins_with("res://");
}

// A stand-in for one of Roblox's built-in images, drawn here rather than shipped. Every place
// asks for these three -- a spawn pad has one, and a Sky names a sun and a moon -- and without
// them a pad draws untextured and the sky's bodies lose their glow. They are Roblox's art, so
// what is drawn is an equivalent of the right shape and weight, not a copy: a soft white disc
// for the sun, a dimmer grey one for the moon, and a bordered tile for the pad.
static Ref<Texture2D> builtin_texture(const std::string& name) {
    const bool sun = name == "sky/sun.jpg", moon = name == "sky/moon.jpg";
    const bool pad = name == "textures/SpawnLocation.png";
    if (!sun && !moon && !pad) return Ref<Texture2D>();
    const int n = 128;
    Ref<Image> img = Image::create_empty(n, n, false, Image::FORMAT_RGBA8);
    if (sun || moon) {
        const float peak = sun ? 1.0f : 0.78f;
        for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) {
            const float dx = (x + 0.5f) / n * 2.0f - 1.0f, dy = (y + 0.5f) / n * 2.0f - 1.0f;
            const float d = std::sqrt(dx * dx + dy * dy);
            // Solid to two thirds of the radius, then off to nothing by the edge.
            const float a = d >= 1.0f ? 0.0f : (d <= 0.66f ? 1.0f : 1.0f - (d - 0.66f) / 0.34f);
            img->set_pixel(x, y, Color(peak, peak, moon ? peak : peak * 0.97f, a * a));
        }
    } else {
        const Color face(0.66f, 0.67f, 0.69f), edge(0.40f, 0.41f, 0.43f);
        for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) {
            const int m = std::min(std::min(x, y), std::min(n - 1 - x, n - 1 - y));
            img->set_pixel(x, y, m < n / 12 ? edge : face);
        }
    }
    return ImageTexture::create_from_image(img);
}

// Roblox's spellings of a cloud asset (an id, a thumbnail, the web URLs older places carry) and
// pblockz://, content on the chain. Each is fetched on the machine that draws it: a resolved
// file path is local, so a server must never replicate one.
static bool isCloudAsset(const std::string& id) {
    static const char* const heads[] = {"pblockz://", "rbxassetid://", "rbxthumb://", "rbxgameasset://", "rbxhttp://",
                                        "http://www.roblox.com/asset", "https://www.roblox.com/asset", "http://roblox.com/asset", "https://roblox.com/asset",
                                        "http://assetdelivery.roblox.com/", "https://assetdelivery.roblox.com/", "https://assetgame.roblox.com/"};
    for (const char* h : heads) if (id.rfind(h, 0) == 0) return true;
    return false;
}
// What to say about a missing file: Roblox's own built-ins are not shipped here.
static std::string missingNote(const std::string& id, const std::string& path, const char* kinds) {
    if (id.rfind("rbxasset://", 0) == 0) return id + " is one of Roblox's own built-in files, not shipped here; " + kinds + " at " + path + " would stand in for it";
    return "could not load " + id + " (" + path + "): " + kinds + " in the project";
}

// ---- HttpService -------------------------------------------------------------------
// Off by default and server-only: the client half is refused inside the runtime and only
// apply_server drains the asks, so a place cannot reach the network from a player's computer.
void PulseBlockzWorld::set_http_enabled(bool v) { opts_.httpEnabled = v; }
bool PulseBlockzWorld::get_http_enabled() const { return opts_.httpEnabled; }

void PulseBlockzWorld::start_http(const HttpAsk& ask) {
    HTTPRequest* req = memnew(HTTPRequest);
    req->set_name(String("Http") + String::num_int64((int64_t)ask.id));
    req->set_use_threads(true);
    req->set_timeout(30);
    add_child(req);
    Array bound; bound.push_back((int64_t)ask.id);
    req->connect("request_completed", Callable(this, "_on_http_done").bindv(bound));
    PackedStringArray headers;
    if (!ask.contentType.empty())
        headers.push_back(String("Content-Type: ") + String::utf8(ask.contentType.c_str()));
    for (const auto& h : ask.headers)
        headers.push_back(String::utf8(h.first.c_str()) + String(": ") + String::utf8(h.second.c_str()));
    HTTPClient::Method method = HTTPClient::METHOD_GET;
    if (ask.method == "POST") method = HTTPClient::METHOD_POST;
    else if (ask.method == "PUT") method = HTTPClient::METHOD_PUT;
    else if (ask.method == "DELETE") method = HTTPClient::METHOD_DELETE;
    else if (ask.method == "HEAD") method = HTTPClient::METHOD_HEAD;
    else if (ask.method == "PATCH") method = HTTPClient::METHOD_PATCH;
    httpJobs_[ask.id] = req;
    if (req->request(String::utf8(ask.url.c_str()), headers, method, String::utf8(ask.body.c_str())) != OK) {
        // Answered straight away: a script waiting on a reply that cannot come parks forever.
        httpJobs_.erase(ask.id);
        req->queue_free();
        HttpAnswer a; a.id = ask.id; a.ok = false;
        a.statusText = "HttpService: could not start the request (is the URL well formed?)";
        httpAnswers_.push_back(std::move(a));
    }
}

void PulseBlockzWorld::_on_http_done(int result, int code, const PackedStringArray& headers,
                                   const PackedByteArray& body, int64_t id) {
    auto it = httpJobs_.find((uint64_t)id);
    if (it == httpJobs_.end()) return;
    it->second->queue_free();
    httpJobs_.erase(it);
    HttpAnswer a;
    a.id = (uint64_t)id;
    a.ok = (result == HTTPRequest::RESULT_SUCCESS);
    a.status = code;
    if (!a.ok) a.statusText = "HttpService: the request failed (result " + std::to_string(result) + ")";
    else a.statusText = code == 200 ? "OK" : "";
    if (body.size() > 0) a.body.assign((const char*)body.ptr(), (size_t)body.size());
    for (int i = 0; i < headers.size(); i++) {
        const String& line = headers[i];
        const int at = line.find(":");
        if (at <= 0) continue;
        a.headers.emplace_back(toStd(line.substr(0, at).strip_edges()),
                               toStd(line.substr(at + 1, line.length()).strip_edges()));
    }
    httpAnswers_.push_back(std::move(a));
}

void PulseBlockzWorld::set_cloud_fetch_base(const String& v) { cloudFetchBase_ = toStd(v); }
String PulseBlockzWorld::get_cloud_fetch_base() const { return String::utf8(cloudFetchBase_.c_str()); }

// The raw cookie value, with or without the ".ROBLOSECURITY=" Roblox writes in front of it. No
// getter: nothing reads it back out, so nothing can print it by accident.
void PulseBlockzWorld::set_cloud_cookie(const String& v) {
    std::string s = toStd(v.strip_edges());
    // A UTF-8 BOM is not whitespace, so strip_edges() leaves it on the front.
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB
        && (unsigned char)s[2] == 0xBF) s = s.substr(3);
    const std::string tag = ".ROBLOSECURITY=";
    if (s.rfind(tag, 0) == 0) s = s.substr(tag.size());
    if (!s.empty() && s.back() == ';') s.pop_back();
    // A header value must be ASCII. Anything else is a mis-encoded file, not a cookie.
    for (unsigned char ch : s) if (ch < 0x20 || ch > 0x7E) { cloudCookie_.clear(); return; }
    cloudCookie_ = s;
}
bool PulseBlockzWorld::has_cloud_cookie() const { return !cloudCookie_.empty(); }
int PulseBlockzWorld::cloud_cookie_len() const { return (int)cloudCookie_.size(); }

// The number in any spelling of a cloud asset: rbxassetid://123, rbxassetid://123&x,
// http://www.roblox.com/asset/?id=123, https://assetdelivery.roblox.com/v1/asset/?id=123.
static std::string cloudNumber(const std::string& id) {
    if (!isCloudAsset(id)) return "";
    // The content hash out of a pblockz:// uri: lower case, no 0x, 64 hex digits.
    for (const std::string& scheme : {std::string("pblockz://")}) {
        const size_t head = scheme.size();
        if (id.rfind(scheme, 0) != 0) continue;
        size_t end = id.find('?');
        std::string hash = id.substr(head, end == std::string::npos ? std::string::npos : end - head);
        for (char& c : hash) c = (char)std::tolower((unsigned char)c);
        if (hash.rfind("0x", 0) == 0) hash = hash.substr(2);
        return hash.size() == 64 ? "pblockz:" + hash : "";
    }
    size_t at = id.find("id=");
    if (at != std::string::npos) at += 3;
    else { at = id.find("://"); if (at == std::string::npos) return ""; at += 3; }
    std::string num;
    while (at < id.size() && std::isdigit((unsigned char)id[at])) num += id[at++];
    return num;
}

// The cached file for a cloud asset; "" while it is on its way (the fetch starts here) or when
// it could not be had. Nothing is cached against a miss, so the caller asks again.
std::string PulseBlockzWorld::cloud_local(const std::string& id, const char* kind) {
    std::string num = cloudNumber(id);
    if (num.empty()) { cloud_note(kind, id, "a file in the project"); return ""; }
    CloudAsset& a = cloudAssets_[num];
    a.num = num; a.ids.insert(id);
    if (a.kind.empty()) a.kind = kind;
    if (!a.file.empty()) return a.file;
    if (num.rfind("pblockz:", 0) == 0) {
        // The host's media cache: the first 32 hex of the hash, with the extension its mime gave
        // it. Looked at first, so a second run never asks the chain for what it already has.
        const std::string hash32 = num.substr(8, 32);
        std::vector<std::string> exts;
        if (size_t m = id.find("mime="); m != std::string::npos) {
            std::string mime = id.substr(m + 5, id.find('&', m) == std::string::npos ? std::string::npos : id.find('&', m) - m - 5);
            for (size_t p; (p = mime.find("%2F")) != std::string::npos;) mime.replace(p, 3, "/");
            static const std::pair<const char*, const char*> kExt[] = {
                {"image/png", "png"}, {"image/jpeg", "jpg"}, {"image/webp", "webp"}, {"image/bmp", "bmp"}, {"image/svg+xml", "svg"},
                {"model/gltf-binary", "glb"}, {"model/gltf+json", "gltf"}, {"model/obj", "obj"}, {"text/plain", "obj"},
                {"font/ttf", "ttf"}, {"font/otf", "otf"}, {"font/woff", "woff"}, {"font/woff2", "woff2"}, {"application/x-font-ttf", "ttf"}, {"application/font-woff", "woff"},
                {"audio/wav", "wav"}, {"audio/x-wav", "wav"}, {"audio/ogg", "ogg"}, {"audio/mpeg", "mp3"}};
            for (auto& [mm, ext] : kExt) if (mime == mm) exts.push_back(ext);
        }
        for (const char* ext : {"png", "jpg", "webp", "bmp", "svg", "obj", "glb", "gltf", "ttf", "otf", "woff", "woff2", "wav", "ogg", "mp3", "json"}) exts.push_back(ext);
        for (const std::string& ext : exts) {
            String p = String("user://media/") + String::utf8(hash32.c_str()) + String(".") + String::utf8(ext.c_str());
            if (FileAccess::file_exists(p)) { a.file = toStd(p); return a.file; }
        }
        // Not here: the host fetches it (asset_wanted -> asset_arrived). A failed fetch is tried
        // again after 60 s, so a chain node down for a minute does not lose the texture for good.
        const double now = (double)Time::get_singleton()->get_ticks_msec() / 1000.0;
        if (a.failed) { if (now - a.failedAt < 60.0) return ""; a.failed = false; }
        // Asked again if nothing answers. A host can start listening a frame after the first
        // part wants a texture -- the Studio staging a place's files, a wallet still starting
        // up -- and an ask sent into an empty room would otherwise never be answered.
        const double ASK_AGAIN = 3.0;
        if (!a.asked || now - a.askedAt > ASK_AGAIN) {
            a.asked = true;
            a.askedAt = now;
            emit_signal("asset_wanted", String::utf8(id.c_str()), String(kind));
        }
        return "";
    }
    if (a.failed || a.req) return "";
    const String dir = "user://roblox_cache";
    DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(dir));
    for (const char* ext : {"png", "jpg", "webp", "gif", "mesh", "ogg", "mp3", "wav", "rbxm", "rbxmx", "bin"}) {   // fetched before
        String p = dir + String("/") + String::utf8(num.c_str()) + String(".") + String(ext);
        if (FileAccess::file_exists(p)) { a.file = toStd(p); return a.file; }
    }
    a.req = memnew(HTTPRequest);
    a.req->set_name(String("Fetch") + String::utf8(num.c_str()));
    a.req->set_use_threads(true);
    a.req->set_timeout(30);
    add_child(a.req);
    Array bound; bound.push_back(String::utf8(num.c_str()));
    a.req->connect("request_completed", Callable(this, "_on_cloud_done").bindv(bound));
    PackedStringArray headers;
    headers.push_back("User-Agent: Roblox/WinInet");
    headers.push_back("Accept: */*");
    // Only to Roblox's own https host: cloudFetchBase_ is settable, and an unguarded cookie
    // would follow it anywhere.
    if (!cloudCookie_.empty() && cloudFetchBase_.rfind(kCloudAuthHost, 0) == 0)
        headers.push_back(String::utf8(("Cookie: .ROBLOSECURITY=" + cloudCookie_).c_str()));
    if (a.req->request(String::utf8((cloudFetchBase_ + num).c_str()), headers) != OK) {
        a.failed = true;
        a.req->queue_free(); a.req = nullptr;
        emit_signal("script_warn", String(kind), String::utf8(("could not start fetching rbxassetid://" + num).c_str()));
    }
    return "";
}

void PulseBlockzWorld::note_texture(const char* site, int w, int h, int bytesPerPixel) {
    if (w <= 0 || h <= 0) return;
    auto& t = texTally_[site];
    t.first += (int64_t)w * h * bytesPerPixel;
    t.second += 1;
}

Dictionary PulseBlockzWorld::texture_tally() const {
    Dictionary out;
    for (const auto& [site, t] : texTally_) {
        Dictionary row;
        row["mb"] = t.first / 1048576.0;
        row["count"] = t.second;
        out[String::utf8(site.c_str())] = row;
    }
    return out;
}

Dictionary PulseBlockzWorld::net_stats() const {
    Dictionary out;
    if (peer_.is_null()) return out;
    Ref<ENetConnection> host = peer_->get_host();
    if (host.is_valid()) {   // since the last call: pop_statistic reads and resets
        out["sent_bytes"] = host->pop_statistic(ENetConnection::HOST_TOTAL_SENT_DATA);
        out["sent_packets"] = host->pop_statistic(ENetConnection::HOST_TOTAL_SENT_PACKETS);
        out["recv_bytes"] = host->pop_statistic(ENetConnection::HOST_TOTAL_RECEIVED_DATA);
        out["recv_packets"] = host->pop_statistic(ENetConnection::HOST_TOTAL_RECEIVED_PACKETS);
    }
    // On a client the one peer is the server (id 1); on a server, the worst of the clients.
    double rtt = 0, rttLast = 0, loss = 0;
    if (netClient_) {
        Ref<ENetPacketPeer> p = peer_->get_peer(1);
        if (p.is_valid()) { rtt = p->get_statistic(ENetPacketPeer::PEER_ROUND_TRIP_TIME); rttLast = p->get_statistic(ENetPacketPeer::PEER_LAST_ROUND_TRIP_TIME); loss = p->get_statistic(ENetPacketPeer::PEER_PACKET_LOSS); }
    } else {
        for (auto& [id, nc] : clients_) {
            Ref<ENetPacketPeer> p = peer_->get_peer(id);
            if (p.is_valid()) { rtt = std::max(rtt, p->get_statistic(ENetPacketPeer::PEER_ROUND_TRIP_TIME)); loss = std::max(loss, p->get_statistic(ENetPacketPeer::PEER_PACKET_LOSS)); }
        }
    }
    out["rtt_ms"] = rtt;
    out["rtt_last_ms"] = rttLast;
    out["loss"] = loss / 65536.0;   // ENET_PEER_PACKET_LOSS_SCALE
    return out;
}

// The host fetched a pblockz:// into a file, or could not: an empty path.
void PulseBlockzWorld::asset_arrived(const String& uri, const String& path) {
    std::string num = cloudNumber(toStd(uri));
    auto it = cloudAssets_.find(num);
    if (it == cloudAssets_.end()) return;
    CloudAsset& a = it->second;
    a.asked = false;
    if (path.is_empty()) {
        a.failed = true;
        a.failedAt = (double)Time::get_singleton()->get_ticks_msec() / 1000.0;
        emit_signal("script_warn", String::utf8(a.kind.c_str()), String("could not fetch ") + uri + String("; it will be asked for again"));
        return;
    }
    a.file = toStd(path);
    cloud_arrived(a);
}

// The fetch came back: the first bytes say what it is, and the file is kept for next time.
void PulseBlockzWorld::_on_cloud_done(int result, int code, const PackedStringArray& headers, const PackedByteArray& body, const String& num) {
    auto it = cloudAssets_.find(toStd(num));
    if (it == cloudAssets_.end()) return;
    CloudAsset& a = it->second;
    if (a.req) { a.req->queue_free(); a.req = nullptr; }
    if (result != HTTPRequest::RESULT_SUCCESS || code != 200 || body.size() == 0) {
        a.failed = true;
        emit_signal("script_warn", String::utf8(a.kind.c_str()),
                    String::utf8(("could not fetch rbxassetid://" + a.num + (code ? " (HTTP " + std::to_string(code) + ")" : " (no answer)") + "; a file in the project would stand in for it").c_str()));
        return;
    }
    PackedByteArray raw = body;
    if (raw.size() > 2 && (uint8_t)raw[0] == 0x1F && (uint8_t)raw[1] == 0x8B) {
        PackedByteArray un = raw.decompress_dynamic(-1, FileAccess::COMPRESSION_GZIP);
        if (un.size()) raw = un;
    }
    const uint8_t* d = raw.ptr(); const size_t n = (size_t)raw.size();
    auto starts = [&](const char* s) { size_t l = std::strlen(s); return n >= l && std::memcmp(d, s, l) == 0; };
    const char* ext = "bin";
    if (starts("\x89PNG")) ext = "png";
    else if (n > 2 && d[0] == 0xFF && d[1] == 0xD8) ext = "jpg";
    else if (starts("GIF8")) ext = "gif";
    else if (starts("RIFF") && n > 12 && std::memcmp(d + 8, "WEBP", 4) == 0) ext = "webp";
    else if (starts("RIFF") && n > 12 && std::memcmp(d + 8, "WAVE", 4) == 0) ext = "wav";
    else if (starts("OggS")) ext = "ogg";
    else if (starts("ID3") || (n > 2 && d[0] == 0xFF && (d[1] & 0xE0) == 0xE0)) ext = "mp3";
    else if (starts("version ")) ext = "mesh";
    else if (starts("<roblox!")) ext = "rbxm";
    else if (starts("<roblox") || starts("<?xml")) ext = "rbxmx";
    String path = String("user://roblox_cache/") + num + String(".") + String(ext);
    if (Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE); f.is_valid()) f->store_buffer(raw);
    a.file = toStd(path);
    emit_signal("script_print", String("Roblox"), String::utf8(("fetched rbxassetid://" + a.num + ": " + ext + ", " + std::to_string((n + 512) / 1024) + " KB").c_str()));
    cloud_arrived(a);
}

// Restyle whatever names this asset, now that it can be loaded.
void PulseBlockzWorld::touch_asset(const std::string& key) {
    for (auto& [gid, g] : guis_) for (auto& [k, v] : g.props) if (v.type == Value::String && v.s == key) { g.dirty = true; guiDirty_ = true; break; }
    for (auto& [did, d] : decals_) if (d.texture == key) { if (d.node) style_decal(d); else decalsDirty_ = true; }
    for (auto& [eid, e] : emitters_) {
        auto tit = e.props.find("Texture");
        if (tit != e.props.end() && tit->second.type == Value::String && tit->second.s == key) { if (e.node) style_emitter(e); else emittersDirty_ = true; }
    }
}

void PulseBlockzWorld::cloud_arrived(CloudAsset& a) {
    for (const std::string& id : a.ids) { textures_.erase(id); forget_mesh(id); streams_.erase(id); textureWarned_.erase(id); meshWarned_.erase(id); soundWarned_.erase(id); }
    for (auto fit = fonts_.begin(); fit != fonts_.end();) {   // a face keyed "<file>|<weight>[|i]"
        bool stale = false;
        for (const std::string& id : a.ids) if (fit->first.rfind(id + "|", 0) == 0) stale = true;
        fit = stale ? fonts_.erase(fit) : std::next(fit);
    }
    effectsDirty_ = true;   // a Sky whose face this is tries its cubemap again
    for (auto mit = materials_.begin(); mit != materials_.end();) {   // a material made with the texture missing
        // Anywhere in the key: a material is keyed
        // colour|transparency|reflectance|material|texture|maps..., so the texture sits mid-key.
        bool stale = false;
        for (const std::string& id : a.ids) if (!id.empty() && mit->first.find(id) != std::string::npos) stale = true;
        mit = stale ? materials_.erase(mit) : std::next(mit);
    }
    std::set<int64_t> dataParts;
    for (auto& [did, m] : dataMeshes_) if (a.ids.count(m.meshId) || a.ids.count(m.textureId)) for (auto& [pid, p] : parts_) if (p.dataMesh == did) dataParts.insert(pid);
    for (auto& [pid, p] : parts_)
        if (a.ids.count(p.meshId) || a.ids.count(p.textureId) || dataParts.count(pid)) { update_shape(p); update_material(p); }
    for (auto& [tid, t] : trails_) if (a.ids.count(t.texture)) t.textureLoaded.clear();
    for (auto& [bid, b] : beams_) if (a.ids.count(b.texture)) b.textureLoaded.clear();
    for (auto& [sid, snd] : sounds_) if (a.ids.count(snd.soundId)) { snd.loadedId.clear(); soundsDirty_ = true; }
    for (auto& [key, v] : voices_) if (a.ids.count(v.asset)) { v.asset.clear(); v.stream.unref(); audioDirty_ = true; }   // an AudioPlayer's asset: loaded again, its length reported
    // A face reaches every label, so a font restyles them all; anything else, only what names it.
    if (a.kind == "Font") { guiDirty_ = guiRestyleAll_ = true; }
    for (const std::string& id : a.ids) touch_asset(id);
    if (a.kind == "Animation") { cloud_into_tree(a); for (auto& [tid, t] : tracks_) t.resolved = false; }
}

// A model asset (a KeyframeSequence, a model) goes into the tree under
// ReplicatedStorage.RobloxAssets by its number, the way a file in the project would.
void PulseBlockzWorld::cloud_into_tree(CloudAsset& a) {
    if (a.inTree || a.file.empty()) return;
    a.inTree = true;
    String gp = String::utf8(a.file.c_str());
    String ext = gp.get_extension().to_lower();
    if (ext == "rbxm") load_file(String("ReplicatedStorage/RobloxAssets/") + String::utf8(a.num.c_str()) + ".rbxm", Marshalls::get_singleton()->raw_to_base64(FileAccess::get_file_as_bytes(gp)));
    else if (ext == "rbxmx") load_file(String("ReplicatedStorage/RobloxAssets/") + String::utf8(a.num.c_str()) + ".rbxmx", FileAccess::get_file_as_string(gp));
}

// Roblox's mesh file as asset delivery hands it out. "version 1.00"/"1.01" is text: a face per
// three [x,y,z][nx,ny,nz][u,v,w] triples, 1.00's positions at twice scale. "version 2.00".."5.00"
// is binary: a header, vertices of position/normal/uv/tangent/colour, faces of three indices,
// LODs from 3.00 of which the first is the mesh, then bones and subsets, skipped. One surface,
// the first LOD, faces in the file's order.
// A Draco blob straight to a mesh. Draco keeps one index per attribute, so the corners are
// written out flat rather than indexed: a position shared by two faces with different normals
// is two corners, which is what SurfaceTool wants anyway.
static Ref<ArrayMesh> draco_to_mesh(const uint8_t* data, size_t size) {
    draco::DecoderBuffer buf;
    buf.Init((const char*)data, size);
    auto type = draco::Decoder::GetEncodedGeometryType(&buf);
    if (!type.ok() || type.value() != draco::TRIANGULAR_MESH) return Ref<ArrayMesh>();
    draco::Decoder dec;
    auto got = dec.DecodeMeshFromBuffer(&buf);
    if (!got.ok()) return Ref<ArrayMesh>();
    std::unique_ptr<draco::Mesh> m = std::move(got).value();
    if (!m || m->num_faces() == 0) return Ref<ArrayMesh>();

    const draco::PointAttribute* pos = m->GetNamedAttribute(draco::GeometryAttribute::POSITION);
    const draco::PointAttribute* nrm = m->GetNamedAttribute(draco::GeometryAttribute::NORMAL);
    const draco::PointAttribute* uv = m->GetNamedAttribute(draco::GeometryAttribute::TEX_COORD);
    const draco::PointAttribute* col = m->GetNamedAttribute(draco::GeometryAttribute::COLOR);
    if (!pos) return Ref<ArrayMesh>();

    Ref<SurfaceTool> st; st.instantiate();
    st->begin(Mesh::PRIMITIVE_TRIANGLES);

    // Indexed, because Draco already is: every attribute is one value per point and the faces
    // index into those points. Writing three unshared corners a face instead would forbid
    // smooth shading, since a normal can then only be the face's own.
    for (draco::PointIndex pi(0); pi < m->num_points(); ++pi) {
        float v[4] = {0, 0, 0, 1};
        if (nrm && nrm->ConvertValue<float, 3>(nrm->mapped_index(pi), v)) st->set_normal(Vector3(v[0], v[1], v[2]));
        // V as it is stored, the same as the uncompressed reader below: Draco keeps what the
        // encoder put in. Flipping it walks an atlas's rows, so parts sharing one image wear
        // each other's colours.
        if (uv && uv->ConvertValue<float, 2>(uv->mapped_index(pi), v)) st->set_uv(Vector2(v[0], v[1]));
        if (col) {
            const int n = col->num_components();
            v[3] = 1.0f;
            if ((n == 4 && col->ConvertValue<float, 4>(col->mapped_index(pi), v)) ||
                (n == 3 && col->ConvertValue<float, 3>(col->mapped_index(pi), v)))
                st->set_color(Color(v[0], v[1], v[2], v[3]));
        }
        if (!pos->ConvertValue<float, 3>(pos->mapped_index(pi), v)) { v[0] = v[1] = v[2] = 0; }
        st->add_vertex(Vector3(v[0], v[1], v[2]));
    }
    // Reversed: Roblox winds a face so its right-hand normal points out of the solid, and Godot
    // takes a clockwise face as the front one, so emitted in file order every triangle is a back
    // face -- the roofs and the far side of a car go missing, and generate_normals below would
    // point every normal into the mesh. Measured on 150 closed meshes from a real place: all 150
    // wound right-hand-outward, none the other way.
    int faces = 0;
    for (draco::FaceIndex f(0); f < m->num_faces(); ++f) {
        const draco::Mesh::Face& face = m->face(f);
        st->add_index((int)face[0].value());
        st->add_index((int)face[2].value());
        st->add_index((int)face[1].value());
        faces++;
    }
    if (!faces) return Ref<ArrayMesh>();
    // Roblox stores no normals in these: the payload is position, colour and uv, and nothing
    // else. Without them a mesh draws black with a lit rim. Generated from the geometry, which
    // shades smoothly across a shared point and sharply where the encoder split one -- a seam
    // in the uv is already two points here, so the hard edges stay hard.
    if (!nrm) st->generate_normals();
    if (uv) st->generate_tangents();   // needs normals and uv; a normal map is unusable without
    return st->commit();
}

Ref<ArrayMesh> PulseBlockzWorld::load_rbxmesh(const PackedByteArray& bytes) {
    const uint8_t* d = bytes.ptr(); const size_t n = (size_t)bytes.size();
    if (n < 13 || std::memcmp(d, "version ", 8) != 0) return Ref<ArrayMesh>();
    size_t nl = 0; while (nl < n && d[nl] != '\n') nl++;
    if (nl >= n) return Ref<ArrayMesh>();
    std::string ver((const char*)d + 8, nl - 8);
    auto rd32 = [&](size_t p) -> uint32_t {
        return p + 4 <= n ? (uint32_t)d[p] | ((uint32_t)d[p + 1] << 8) | ((uint32_t)d[p + 2] << 16) | ((uint32_t)d[p + 3] << 24) : 0;
    };
    auto rdf = [&](size_t p) -> float { float v = 0; if (p + 4 <= n) std::memcpy(&v, d + p, 4); return v; };

    // Version 7 is a container rather than a layout, so it is matched on its magic and not on
    // the number: "COREMESH", a format word, a section length, then the section. Format 2 holds
    // one standard Draco blob (major 2, minor 2, TRIANGULAR_MESH); format 1 holds plain arrays.
    // Tagged chunks follow the section to the end of the file -- LODS always, SKINNING on a
    // rigged mesh -- and are skipped by their own lengths. Nothing here reads them yet.
    const size_t head = nl + 1;
    if (head + 16 <= n && std::memcmp(d + head, "COREMESH", 8) == 0) {
        const uint32_t format = rd32(head + 8);
        const size_t secSize = rd32(head + 12);
        if (head + 16 + secSize > n || secSize < 4) return Ref<ArrayMesh>();
        if (format == 2) {
            const size_t dracoLen = rd32(head + 16);
            if (dracoLen != secSize - 4 || head + 20 + dracoLen > n) return Ref<ArrayMesh>();
            return draco_to_mesh(d + head + 20, dracoLen);
        }
        if (format != 1) return Ref<ArrayMesh>();
        // Plain arrays: a count, 40-byte vertices (position, normal, uv, tangent, rgba), a
        // count, then three uint32 indices a face.
        const uint32_t nv = rd32(head + 16);
        const size_t vAt = head + 20;
        if (nv == 0 || nv > 4000000 || vAt + (size_t)nv * 40 + 4 > n) return Ref<ArrayMesh>();
        const size_t fCountAt = vAt + (size_t)nv * 40;
        const uint32_t nf = rd32(fCountAt);
        const size_t fAt = fCountAt + 4;
        if (nf == 0 || nf > 4000000 || fAt + (size_t)nf * 12 > n) return Ref<ArrayMesh>();
        Ref<SurfaceTool> raw; raw.instantiate();
        raw->begin(Mesh::PRIMITIVE_TRIANGLES);
        for (uint32_t i = 0; i < nv; i++) {
            const size_t p = vAt + (size_t)i * 40;
            raw->set_normal(Vector3(rdf(p + 12), rdf(p + 16), rdf(p + 20)));
            raw->set_uv(Vector2(rdf(p + 24), rdf(p + 28)));
            raw->set_color(Color(d[p + 36] / 255.f, d[p + 37] / 255.f, d[p + 38] / 255.f, d[p + 39] / 255.f));
            raw->add_vertex(Vector3(rdf(p), rdf(p + 4), rdf(p + 8)));
        }
        int drawn = 0;
        for (uint32_t i = 0; i < nf; i++) {
            const size_t p = fAt + (size_t)i * 12;
            const uint32_t a = rd32(p), b = rd32(p + 4), c = rd32(p + 8);
            if (a >= nv || b >= nv || c >= nv) continue;
            raw->add_index((int)a); raw->add_index((int)c); raw->add_index((int)b);   // reversed: see draco_to_mesh
            drawn++;
        }
        return drawn ? raw->commit() : Ref<ArrayMesh>();
    }

    Ref<SurfaceTool> st; st.instantiate();
    st->begin(Mesh::PRIMITIVE_TRIANGLES);
    int faces = 0;
    if (ver.rfind("1.", 0) == 0) {
        std::string text((const char*)d + nl + 1, n - nl - 1);
        for (char& ch : text) if (ch == '[' || ch == ']' || ch == ',' || ch == '\r') ch = ' ';
        size_t nlTwo = text.find('\n');
        if (nlTwo == std::string::npos) return Ref<ArrayMesh>();
        std::vector<float> f;
        const char* c = text.c_str() + nlTwo + 1; char* end = nullptr;
        for (;;) { float v = std::strtof(c, &end); if (end == c) break; f.push_back(v); c = end; }
        const float scale = ver == "1.00" ? 0.5f : 1.0f;
        for (size_t i = 0; i + 27 <= f.size(); i += 27) {
            for (int k : {0, 2, 1}) {   // reversed: see draco_to_mesh
                const float* v = &f[i + k * 9];
                st->set_normal(Vector3(v[3], v[4], v[5]));
                st->set_uv(Vector2(v[6], v[7]));
                st->add_vertex(Vector3(v[0], v[1], v[2]) * scale);
            }
            faces++;
        }
    } else {
        size_t at = nl + 1;
        auto u8 = [&](size_t p) -> uint32_t { return p < n ? d[p] : 0; };
        auto u16 = [&](size_t p) -> uint32_t { return u8(p) | (u8(p + 1) << 8); };
        auto u32 = [&](size_t p) -> uint32_t { return u16(p) | (u16(p + 2) << 16); };
        auto f32 = [&](size_t p) -> float { float v = 0; if (p + 4 <= n) std::memcpy(&v, d + p, 4); return v; };
        const uint32_t headerSize = u16(at);
        uint32_t vertSize = 40, numVerts = 0, numFaces = 0, numLods = 0, numBones = 0;
        const int major = std::atoi(ver.c_str());
        if (major == 2) { vertSize = u8(at + 2); numVerts = u32(at + 4); numFaces = u32(at + 8); }
        else if (major == 3) { vertSize = u8(at + 2); numLods = u16(at + 6); numVerts = u32(at + 8); numFaces = u32(at + 12); }
        else { numVerts = u32(at + 4); numFaces = u32(at + 8); numLods = u16(at + 12); numBones = u16(at + 14); }
        if (vertSize < 36 || numVerts == 0 || numFaces == 0 || numVerts > 4000000 || numFaces > 4000000) return Ref<ArrayMesh>();
        size_t vAt = at + headerSize;
        size_t fAt = vAt + (size_t)numVerts * vertSize + (numBones ? (size_t)numVerts * 8 : 0);
        size_t lAt = fAt + (size_t)numFaces * 12;
        if (lAt > n) return Ref<ArrayMesh>();
        uint32_t faceBegin = 0, faceEnd = numFaces;
        if (numLods >= 2 && lAt + (size_t)numLods * 4 <= n) { faceBegin = u32(lAt); faceEnd = u32(lAt + 4); if (faceEnd > numFaces || faceBegin >= faceEnd) { faceBegin = 0; faceEnd = numFaces; } }
        for (uint32_t i = 0; i < numVerts; i++) {
            size_t p = vAt + (size_t)i * vertSize;
            st->set_normal(Vector3(f32(p + 12), f32(p + 16), f32(p + 20)));
            st->set_uv(Vector2(f32(p + 24), f32(p + 28)));
            if (vertSize >= 40) { uint32_t r = u8(p + 36), g = u8(p + 37), b = u8(p + 38), a = u8(p + 39); st->set_color(Color(r / 255.f, g / 255.f, b / 255.f, a / 255.f)); }
            st->add_vertex(Vector3(f32(p), f32(p + 4), f32(p + 8)));
        }
        for (uint32_t i = faceBegin; i < faceEnd; i++) {
            size_t p = fAt + (size_t)i * 12;
            uint32_t a = u32(p), b = u32(p + 4), c = u32(p + 8);
            if (a >= numVerts || b >= numVerts || c >= numVerts) continue;
            st->add_index((int)a); st->add_index((int)c); st->add_index((int)b);   // reversed: see draco_to_mesh
            faces++;
        }
    }
    if (!faces) return Ref<ArrayMesh>();
    return st->commit();
}

void PulseBlockzWorld::cloud_note(const char* kind, const std::string& id, const char* example) {
    CloudNotes& c = cloud_[kind];
    if (!c.ids.insert(id).second) return;
    if (c.ids.size() <= 3) emit_signal("script_warn", String(kind), String::utf8((id + " is a Roblox cloud asset; put the file in the project and name it, e.g. \"" + example + "\"").c_str()));
    else c.pending++;
}

void PulseBlockzWorld::flush_cloud_notes() {
    for (auto& [kind, c] : cloud_) {
        if (!c.pending) continue;
        emit_signal("script_warn", String(kind.c_str()),
                    String::utf8((std::to_string(c.pending) + " more Roblox cloud assets (" + std::to_string(c.ids.size()) + " in all), each wanting its file in the project").c_str()));
        c.pending = 0;
    }
}

// "sounds/thud.ogg" under asset_root, "res://..." as is, "rbxasset://x" as a bare path. An
// imported resource loads through the ResourceLoader; a plain .ogg / .mp3 / .wav on disk is
// read directly.
Ref<AudioStream> PulseBlockzWorld::load_stream(const std::string& soundId) {
    if (soundId.empty()) return Ref<AudioStream>();
    if (auto it = streams_.find(soundId); it != streams_.end()) return it->second;
    auto warn = [&](const std::string& msg) {
        if (soundWarned_.insert(soundId).second) emit_signal("script_warn", String("Sound"), String::utf8(msg.c_str()));
    };
    std::string path = soundId;
    if (isCloudAsset(path)) { path = cloud_local(soundId, "Sound"); if (path.empty()) return Ref<AudioStream>(); }   // on its way, or not to be had
    if (path.rfind("rbxasset://", 0) == 0) path = assetRoot_ + path.substr(11);
    else if (path.rfind("res://", 0) != 0 && path.rfind("user://", 0) != 0) path = assetRoot_ + path;
    String gp = String::utf8(path.c_str());
    Ref<AudioStream> stream;
    if (importedResource(gp) && ResourceLoader::get_singleton()->exists(gp)) stream = ResourceLoader::get_singleton()->load(gp);
    if (stream.is_null() && FileAccess::file_exists(gp)) {
        std::string ext = path.size() > 4 ? path.substr(path.size() - 4) : "";
        for (auto& ch : ext) ch = (char)std::tolower((unsigned char)ch);
        if (ext == ".ogg") stream = AudioStreamOggVorbis::load_from_file(gp);
        else if (ext == ".mp3") { Ref<AudioStreamMP3> m; m.instantiate(); m->set_data(FileAccess::get_file_as_bytes(gp)); stream = m; }
        else if (ext == ".wav") {
            // RIFF/WAVE, PCM 8 or 16 bit: the chunks Godot's own importer reads.
            PackedByteArray b = FileAccess::get_file_as_bytes(gp);
            const uint8_t* d = b.ptr(); int64_t n = b.size();
            auto u16 = [&](int64_t at) { return (int)d[at] | ((int)d[at + 1] << 8); };
            auto u32 = [&](int64_t at) { return (uint32_t)u16(at) | ((uint32_t)u16(at + 2) << 16); };
            if (n > 12 && std::memcmp(d, "RIFF", 4) == 0 && std::memcmp(d + 8, "WAVE", 4) == 0) {
                int fmt = 0, channels = 1, rate = 44100, bits = 16;
                int64_t at = 12, dataAt = -1; uint32_t dataLen = 0;
                while (at + 8 <= n) {
                    uint32_t len = u32(at + 4);
                    if (std::memcmp(d + at, "fmt ", 4) == 0 && len >= 16) { fmt = u16(at + 8); channels = u16(at + 10); rate = (int)u32(at + 12); bits = u16(at + 22); }
                    else if (std::memcmp(d + at, "data", 4) == 0) { dataAt = at + 8; dataLen = (uint32_t)std::min<int64_t>(len, n - dataAt); }
                    at += 8 + len + (len & 1);
                }
                if (fmt == 1 && dataAt >= 0 && (bits == 8 || bits == 16) && (channels == 1 || channels == 2)) {
                    PackedByteArray pcm; pcm.resize(dataLen);
                    uint8_t* w = pcm.ptrw();
                    for (uint32_t k = 0; k < dataLen; k++) w[k] = bits == 8 ? (uint8_t)(d[dataAt + k] ^ 0x80) : d[dataAt + k];   // 8-bit WAV is unsigned
                    Ref<AudioStreamWAV> wav; wav.instantiate();
                    wav->set_format(bits == 8 ? AudioStreamWAV::FORMAT_8_BITS : AudioStreamWAV::FORMAT_16_BITS);
                    wav->set_mix_rate(rate);
                    wav->set_stereo(channels == 2);
                    wav->set_data(pcm);
                    wav->set_loop_begin(0);
                    wav->set_loop_end((int32_t)(dataLen / (bits / 8) / channels));
                    stream = wav;
                }
            }
        }
    }
    if (stream.is_null()) warn(missingNote(soundId, path, "a .ogg, .mp3 or .wav"));
    streams_[soundId] = stream;
    return stream;
}

// Roblox's families by the names they go by on a desktop (Gotham, Source Sans Pro,
// Inconsolata...), with fallbacks of the same cut. Godot emboldens or slants what the installed
// font lacks, and falls back to its own font past the list.
static PackedStringArray fontNamesFor(const std::string& family) {
    std::string name = family;
    const std::string pre = "rbxasset://fonts/families/";
    if (name.rfind(pre, 0) == 0 && name.size() > pre.size() + 5) name = name.substr(pre.size(), name.size() - pre.size() - 5);
    static const struct { const char* family; const char* names[4]; } kNames[] = {
        {"SourceSansPro", {"Source Sans Pro", "Source Sans 3", "Segoe UI", nullptr}}, {"GothamSSm", {"Gotham", "Montserrat", "Segoe UI", nullptr}},
        {"LegacyArial", {"Arial", "Liberation Sans", nullptr}}, {"Arial", {"Arial", "Liberation Sans", nullptr}},
        {"Inconsolata", {"Inconsolata", "Consolas", "Courier New", nullptr}}, {"RobotoMono", {"Roboto Mono", "Consolas", "Courier New", nullptr}},
        {"PressStart2P", {"Press Start 2P", "Consolas", "Courier New", nullptr}}, {"AccanthisADFStd", {"Accanthis ADF Std", "Bodoni MT", "Georgia", nullptr}},
        {"Guru", {"Guru", "Garamond", "Georgia", nullptr}}, {"ComicNeueAngular", {"Comic Neue", "Comic Sans MS", nullptr}},
        {"HighwayGothic", {"Highway Gothic", "Impact", "Arial Narrow", nullptr}}, {"Zekton", {"Zekton", "Bahnschrift", "Arial", nullptr}},
        {"Balthazar", {"Balthazar", "Palatino Linotype", "Georgia", nullptr}}, {"RomanAntique", {"Roman Antique", "Book Antiqua", "Georgia", nullptr}},
        {"Merriweather", {"Merriweather", "Georgia", nullptr}}, {"Oswald", {"Oswald", "Impact", "Arial Narrow", nullptr}},
        {"RobotoCondensed", {"Roboto Condensed", "Arial Narrow", "Arial", nullptr}}, {"BuilderSans", {"Builder Sans", "Segoe UI", "Arial", nullptr}},
    };
    PackedStringArray out;
    for (auto& k : kNames) if (name == k.family) { for (const char* n : k.names) if (n) out.push_back(n); break; }
    if (out.is_empty()) {
        std::string spaced;   // JosefinSans -> Josefin Sans, AmaticSC -> Amatic SC
        for (size_t i = 0; i < name.size(); i++) {
            if (i && std::isupper((unsigned char)name[i]) && std::islower((unsigned char)name[i - 1])) spaced += ' ';
            spaced += name[i];
        }
        out.push_back(String::utf8(spaced.c_str()));
    }
    for (const char* fb : {"Segoe UI", "Noto Sans", "DejaVu Sans", "Arial"}) if (!out.has(fb)) out.push_back(fb);
    return out;
}
/// True for a FontFace naming a file rather than a family.
static bool isFontFile(const std::string& id) {
    if (id.rfind("pblockz://", 0) == 0)   // the uri says what its bytes are
        return id.find("mime=font") != std::string::npos || id.find("mime=application%2Fx-font") != std::string::npos || id.find("mime=application%2Ffont-woff") != std::string::npos;
    static const char* const exts[] = {".ttf", ".otf", ".ttc", ".otc", ".woff", ".woff2", ".fnt", ".font"};
    std::string low = id;
    for (char& c : low) c = (char)std::tolower((unsigned char)c);
    for (const char* e : exts) {
        size_t n = std::strlen(e);
        if (low.size() > n && low.compare(low.size() - n, n, e) == 0) return true;
    }
    return false;
}

// The PixelFont attribute, set on the text object or on anything it sits under. Roblox has no
// such setting and ignores the attribute, so a place that sets it still runs there, drawn smooth.
bool PulseBlockzWorld::gui_pixel_font(int64_t id) const {
    for (int hop = 0; hop < 32 && id; hop++) {
        if (auto g = guis_.find(id); g != guis_.end()) {
            auto a = g->second.props.find("@PixelFont");
            if (a != g->second.props.end() && a->second.type == Value::Bool) return a->second.b;
        }
        auto e = entries_.find(id);
        if (e == entries_.end()) break;
        id = e->second.parent;
    }
    return false;
}

Ref<Font> PulseBlockzWorld::load_font(const Value& face, bool pixel) {
    std::string key = face.s + "|" + std::to_string((int)face.n) + (face.b ? "|i" : "") + (pixel ? "|px" : "");
    auto it = fonts_.find(key);
    if (it != fonts_.end()) return it->second;

    // A FontFace naming a path is read from a file, the way an image is, so a place can ship a
    // face of its own; only a bare family name goes to the system fonts.
    if (isFontFile(face.s)) {
        std::string path = face.s;
        if (isCloudAsset(path)) { path = cloud_local(face.s, "Font"); if (path.empty()) return Ref<Font>(); }
        if (path.rfind("rbxasset://", 0) == 0) path = assetRoot_ + path.substr(11);
        else if (path.rfind("res://", 0) != 0 && path.rfind("user://", 0) != 0) path = assetRoot_ + path;
        String gp = String::utf8(path.c_str());
        Ref<FontFile> ff;
        if (importedResource(gp) && ResourceLoader::get_singleton()->exists(gp))
            ff = ResourceLoader::get_singleton()->load(gp);
        if (ff.is_null() && FileAccess::file_exists(gp)) {
            ff.instantiate();
            if (ff->load_dynamic_font(gp) != OK) ff = Ref<FontFile>();
        }
        if (ff.is_valid()) {
            // A pixel font gets its own copy with the smoothing off: antialiasing turns a hard
            // two-pixel stem into a grey smudge, and subpixel positioning lands the same glyph
            // on a different fraction of a pixel each time it is drawn.
            if (pixel) {
                ff = ff->duplicate();
                ff->set_antialiasing(TextServer::FONT_ANTIALIASING_NONE);
                ff->set_subpixel_positioning(TextServer::SUBPIXEL_POSITIONING_DISABLED);
                ff->set_hinting(TextServer::HINTING_NONE);
            }
            fonts_[key] = ff;
            return ff;
        }
        emit_signal("script_warn", String("Font"),
            String::utf8(missingNote(face.s, path, "a .ttf, .otf or .woff").c_str()));
    }

    Ref<SystemFont> f; f.instantiate();
    f->set_font_names(fontNamesFor(face.s));
    f->set_font_weight((int)face.n);
    f->set_font_italic(face.b);
    f->set_allow_system_fallback(true);
    fonts_[key] = f;
    return f;
}

// What a Content names, as the key the texture and mesh loaders take: the uri, or
// rbxobject://<id> for an object. Content.none is "".
std::string PulseBlockzWorld::content_key(const Value& v) {
    if (v.type != Value::Content) return v.type == Value::String ? v.s : std::string();
    if (v.n == Value::ContentUri) return v.s;
    if (v.n == Value::ContentObject && v.ref) return "rbxobject://" + std::to_string((long long)v.ref);
    if (v.n == Value::ContentOpaque && v.ref) return "rbxopaque://" + std::to_string((long long)v.ref);
    return std::string();
}

// "images/logo.png" under asset_root, or an imported res:// texture. A .png / .jpg / .webp /
// .bmp / .tga / .svg on disk is read directly.
Ref<Texture2D> PulseBlockzWorld::load_texture(const std::string& imageId) {
    if (imageId.empty()) return Ref<Texture2D>();
    if (imageId.rfind("rbxopaque://", 0) == 0) {
        // Baked content: the holder's pixels, made once into a texture of their own.
        if (auto it = textures_.find(imageId); it != textures_.end()) return it->second;
        auto oit = opaqueImages_.find(std::strtoll(imageId.c_str() + 12, nullptr, 10));
        if (oit == opaqueImages_.end() || oit->second.kind != "Image" || oit->second.w <= 0 || oit->second.rgba.size() < (size_t)oit->second.w * oit->second.h * 4) return Ref<Texture2D>();
        PackedByteArray bytes;
        bytes.resize((int64_t)oit->second.w * oit->second.h * 4);
        std::memcpy(bytes.ptrw(), oit->second.rgba.data(), (size_t)bytes.size());
        Ref<Image> im = Image::create_from_data(oit->second.w, oit->second.h, false, Image::FORMAT_RGBA8, bytes);
        Ref<Texture2D> tex = im.is_valid() ? Ref<Texture2D>(ImageTexture::create_from_image(im)) : Ref<Texture2D>();
        if (tex.is_valid()) note_texture("baked image", oit->second.w, oit->second.h);
        textures_[imageId] = tex;
        return tex;
    }
    if (imageId.rfind("rbxobject://", 0) == 0) {
        // An EditableImage: a one-pixel texture now if the pixels have not arrived, filled in
        // place when they do, so whatever took it follows.
        int64_t id = std::strtoll(imageId.c_str() + 12, nullptr, 10);
        EditableTex& t = editableTex_[id];
        if (t.texture.is_null()) {
            Ref<Image> blank = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
            t.texture = ImageTexture::create_from_image(blank);
        }
        return t.texture;
    }
    if (auto it = textures_.find(imageId); it != textures_.end()) return it->second;
    auto warn = [&](const std::string& msg) {
        if (textureWarned_.insert(imageId).second) emit_signal("script_warn", String("Image"), String::utf8(msg.c_str()));
    };
    std::string path = imageId;
    if (isCloudAsset(path)) { path = cloud_local(imageId, "Image"); if (path.empty()) return Ref<Texture2D>(); }
    std::string builtin;
    if (path.rfind("rbxasset://", 0) == 0) { builtin = path.substr(11); path = assetRoot_ + builtin; }
    else if (path.rfind("res://", 0) != 0 && path.rfind("user://", 0) != 0) path = assetRoot_ + path;
    String gp = String::utf8(path.c_str());
    Ref<Texture2D> tex;
    if (importedResource(gp) && ResourceLoader::get_singleton()->exists(gp)) {
        tex = ResourceLoader::get_singleton()->load(gp);
        if (tex.is_valid()) note_texture("project image", tex->get_width(), tex->get_height());
    }
    if (tex.is_null() && FileAccess::file_exists(gp)) {
        Ref<Image> im; im.instantiate();
        if (im->load(gp) == OK && !im->is_empty()) { tex = ImageTexture::create_from_image(im); note_texture("image file", im->get_width(), im->get_height()); }
    }
    // A file in the project wins: a place that ships its own copy of one of Roblox's built-ins
    // gets that, and only what nothing answers for falls back to the drawn stand-in.
    if (tex.is_null() && !builtin.empty()) {
        tex = builtin_texture(builtin);
        if (tex.is_valid()) note_texture("built-in stand-in", tex->get_width(), tex->get_height());
    }
    if (tex.is_null()) warn(missingNote(imageId, path, "a .png, .jpg, .webp, .bmp, .tga or .svg"));
    textures_[imageId] = tex;
    return tex;
}

// Mouse.Icon: the pointer becomes the image, centred on the hotspot; "" is the arrow again.
// Roblox draws it wherever the pointer is, including over buttons, where Godot asks for the
// pointing hand, so both cursor shapes get the picture.
void PulseBlockzWorld::apply_mouse_icon(const std::string& icon) {
    Ref<Texture2D> tex = load_texture(icon);
    Input* in = Input::get_singleton();
    for (Input::CursorShape shape : {Input::CURSOR_ARROW, Input::CURSOR_POINTING_HAND}) {
        if (tex.is_valid()) in->set_custom_mouse_cursor(tex, shape, tex->get_size() / 2);
        else in->set_custom_mouse_cursor(Ref<Resource>(), shape);
    }
}

// ---- meshes ------------------------------------------------------------------------
// "meshes/rock.obj" under asset_root, or an imported res:// mesh or scene. A .obj on disk is
// read directly, a .glb / .gltf through GLTFDocument. A scene's MeshInstance3Ds are flattened,
// in their placement, into one mesh.
Ref<Mesh> PulseBlockzWorld::load_mesh(const std::string& meshId) {
    if (meshId.empty()) return Ref<Mesh>();
    if (meshId.rfind("rbxobject://", 0) == 0) {
        auto it = editableMeshes_.find(std::strtoll(meshId.c_str() + 12, nullptr, 10));
        return it == editableMeshes_.end() ? Ref<Mesh>() : Ref<Mesh>(it->second);
    }
    if (meshId.rfind("rbxopaque://", 0) == 0) {
        if (auto it = meshes_.find(meshId); it != meshes_.end()) return it->second;
        auto oit = opaqueImages_.find(std::strtoll(meshId.c_str() + 12, nullptr, 10));
        if (oit == opaqueImages_.end() || oit->second.kind != "Mesh") return Ref<Mesh>();
        EditableMeshData data;
        if (!data.deserialize(oit->second.rgba)) return Ref<Mesh>();
        std::vector<float> pos, nrm, uv, rgba;
        data.corners(pos, nrm, uv, rgba);
        Ref<Mesh> mesh = editable_to_mesh(pos, nrm, uv, rgba);
        meshes_[meshId] = mesh;
        return mesh;
    }
    if (auto it = meshes_.find(meshId); it != meshes_.end()) return it->second;
    auto warn = [&](const std::string& msg) {
        if (meshWarned_.insert(meshId).second) emit_signal("script_warn", String("Mesh"), String::utf8(msg.c_str()));
    };
    std::string path = meshId;
    if (isCloudAsset(path)) { path = cloud_local(meshId, "Mesh"); if (path.empty()) return Ref<Mesh>(); }
    if (path.rfind("rbxasset://", 0) == 0) path = assetRoot_ + path.substr(11);
    else if (path.rfind("res://", 0) != 0 && path.rfind("user://", 0) != 0) path = assetRoot_ + path;
    String gp = String::utf8(path.c_str());
    Ref<Mesh> mesh;
    if (importedResource(gp) && ResourceLoader::get_singleton()->exists(gp)) {
        Ref<Resource> res = ResourceLoader::get_singleton()->load(gp);
        mesh = res;
        Ref<PackedScene> scene = res;
        if (mesh.is_null() && scene.is_valid()) {
            if (Node* n = scene->instantiate()) {
                Ref<ArrayMesh> out; out.instantiate();
                collect_meshes(out, n, Transform3D());
                memdelete(n);
                if (out->get_surface_count()) mesh = out;
            }
        }
    }
    String ext = gp.get_extension().to_lower();
    if (mesh.is_null() && FileAccess::file_exists(gp)) {
        if (ext == "obj") mesh = load_obj(gp);
        else if (ext == "mesh") mesh = load_rbxmesh(FileAccess::get_file_as_bytes(gp));   // Roblox's own mesh file, as the cloud hands it out
        else if (ext == "glb" || ext == "gltf") {
            Ref<GLTFDocument> doc; doc.instantiate();
            Ref<GLTFState> state; state.instantiate();
            if (doc->append_from_file(gp, state) == OK) {
                if (Node* n = doc->generate_scene(state)) {
                    Ref<ArrayMesh> out; out.instantiate();
                    collect_meshes(out, n, Transform3D());
                    memdelete(n);
                    if (out->get_surface_count()) mesh = out;
                }
            }
        }
    }
    if (mesh.is_null()) warn(missingNote(meshId, path, "a .obj, .glb or .gltf"));
    meshes_[meshId] = mesh;
    return mesh;
}

// Wavefront OBJ: v / vt / vn and f (any polygon, fanned; negative indices count
// from the end). Without normals, flat ones are generated.
Ref<Mesh> PulseBlockzWorld::load_obj(const String& path) {
    std::string text = FileAccess::get_file_as_string(path).utf8().get_data();
    std::vector<Vector3> vs, vns; std::vector<Vector2> vts;
    Ref<SurfaceTool> st; st.instantiate();
    st->begin(Mesh::PRIMITIVE_TRIANGLES);
    bool anyNormal = false, anyFace = false;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream in(line);
        std::string tag; in >> tag;
        if (tag == "v") { float x = 0, y = 0, z = 0; in >> x >> y >> z; vs.push_back(Vector3(x, y, z)); }
        else if (tag == "vt") { float u = 0, v = 0; in >> u >> v; vts.push_back(Vector2(u, 1 - v)); }
        else if (tag == "vn") { float x = 0, y = 0, z = 0; in >> x >> y >> z; vns.push_back(Vector3(x, y, z)); }
        else if (tag == "f") {
            struct Corner { int v = 0, t = 0, n = 0; };
            std::vector<Corner> corners;
            std::string tok;
            while (in >> tok) {
                Corner c; int* slot[3] = {&c.v, &c.t, &c.n}; int k = 0; size_t i = 0;
                while (i <= tok.size() && k < 3) {
                    size_t j = tok.find('/', i); if (j == std::string::npos) j = tok.size();
                    if (j > i) *slot[k] = std::atoi(tok.substr(i, j - i).c_str());
                    k++; i = j + 1;
                }
                corners.push_back(c);
            }
            auto at = [](int idx, size_t n) -> int { return idx > 0 ? idx - 1 : idx < 0 ? (int)n + idx : -1; };
            auto emit = [&](const Corner& c) {
                int ni = at(c.n, vns.size()), ti = at(c.t, vts.size()), vi = at(c.v, vs.size());
                if (ni >= 0 && ni < (int)vns.size()) { st->set_normal(vns[ni]); anyNormal = true; }
                if (ti >= 0 && ti < (int)vts.size()) st->set_uv(vts[ti]);
                st->add_vertex(vi >= 0 && vi < (int)vs.size() ? vs[vi] : Vector3());
            };
            for (size_t i = 2; i < corners.size(); i++) { emit(corners[0]); emit(corners[i - 1]); emit(corners[i]); anyFace = true; }
        }
    }
    if (!anyFace) return Ref<Mesh>();
    if (!anyNormal) st->generate_normals();
    return st->commit();
}

// ---- Terrain --------------------------------------------------------------------
// Voxels, as Roblox's terrain is: a density per sample on a regular grid, solid from 0.5 up, so
// the ground can hold a cave or an overhang. The surface is where the density crosses 0.5, found
// by surface nets -- one vertex per crossed cell, at the mean of the crossings on its edges, and
// a quad across every crossed edge -- with the density's own gradient for a normal.
//
// The blob: "PBVX", u32 version (2, or 3 with material runs after the density runs), u32 nx, ny,
// nz, float cell, 3 floats origin, u32 runs, then runs of (u16 count, u8 density). Almost every
// sample is all-air or all-solid, so the runs crush it small enough to live in a property. The
// older "PBTR" height-per-column blob is still read, and turned into voxels.
static const int kTerrainMax = 512;             // samples along x and z: 2048 studs at 4 a cell
static const int kTerrainMaxY = 257;

// The materials the ground can be, at Roblox's enum values, with the colour each is drawn in.
// Index 0 is what a fresh field is made of.
struct TerrainMat { int value; const char* name; Color color; };
static const TerrainMat kTerrainMats[] = {
    {1280, "Grass",       Color(0.42f, 0.56f, 0.30f)}, {1284, "LeafyGrass",  Color(0.36f, 0.52f, 0.26f)},
    {1296, "Sand",        Color(0.80f, 0.73f, 0.52f)}, {896,  "Rock",        Color(0.40f, 0.40f, 0.42f)},
    {1328, "Snow",        Color(0.93f, 0.95f, 0.98f)}, {1344, "Mud",         Color(0.36f, 0.27f, 0.20f)},
    {1360, "Ground",      Color(0.55f, 0.45f, 0.35f)}, {1376, "Asphalt",     Color(0.24f, 0.24f, 0.25f)},
    {788,  "Basalt",      Color(0.20f, 0.20f, 0.22f)}, {804,  "CrackedLava", Color(0.60f, 0.22f, 0.10f)},
    {1552, "Glacier",     Color(0.70f, 0.85f, 0.95f)}, {1536, "Ice",         Color(0.75f, 0.90f, 1.00f)},
    {820,  "Limestone",   Color(0.85f, 0.82f, 0.70f)}, {836,  "Pavement",    Color(0.55f, 0.55f, 0.55f)},
    {1392, "Salt",        Color(0.92f, 0.92f, 0.90f)}, {912,  "Sandstone",   Color(0.78f, 0.60f, 0.42f)},
    {800,  "Slate",       Color(0.35f, 0.38f, 0.42f)}, {880,  "Cobblestone", Color(0.50f, 0.48f, 0.45f)},
    {816,  "Concrete",    Color(0.62f, 0.62f, 0.60f)}, {848,  "Brick",       Color(0.55f, 0.30f, 0.24f)},
    {528,  "WoodPlanks",  Color(0.62f, 0.48f, 0.32f)}, {2048, "Water",       Color(0.20f, 0.45f, 0.80f)},
};
static const int kTerrainMatCount = (int)(sizeof(kTerrainMats) / sizeof(kTerrainMats[0]));
static int terrainMatIndex(int enumValue) {
    for (int i = 0; i < kTerrainMatCount; i++) if (kTerrainMats[i].value == enumValue) return i;
    return 0;
}
static const int kWaterIndex = terrainMatIndex(2048);   // Enum.Material.Water
static const float kIso = 128.0f / 255.0f;   // exactly byte 128: a sample at 128 IS the surface

void PulseBlockzWorld::terrain_decode(const std::string& blob) {
    terrain_.sent = blob;
    terrain_.nx = terrain_.ny = terrain_.nz = 0;
    terrain_.d.clear();
    terrain_chunks_reset();
    if (blob.empty()) return;
    PackedByteArray raw = Marshalls::get_singleton()->base64_to_raw(String::utf8(blob.c_str()));
    if (raw.size() < 8) return;
    if (raw[0] == 'P' && raw[1] == 'B' && raw[2] == 'T' && raw[3] == 'R') { terrain_decode_heights(raw); return; }
    if (raw[0] != 'P' || raw[1] != 'B' || raw[2] != 'V' || raw[3] != 'X') return;
    const uint32_t version = raw.decode_u32(4);
    if (version != 2 && version != 3) return;
    const int64_t head = 40;
    if (raw.size() < head) return;
    int nx = (int)raw.decode_u32(8), ny = (int)raw.decode_u32(12), nz = (int)raw.decode_u32(16);
    if (nx < 2 || nz < 2 || ny < 2 || nx > kTerrainMax || nz > kTerrainMax || ny > kTerrainMaxY) return;
    float cell = raw.decode_float(20);
    if (!(cell > 0.01f)) cell = 4.0f;
    Vector3 origin(raw.decode_float(24), raw.decode_float(28), raw.decode_float(32));
    uint32_t runs = raw.decode_u32(36);
    const size_t n = (size_t)nx * ny * nz;
    if (raw.size() < head + (int64_t)runs * 3) return;
    std::vector<uint8_t> d;
    d.reserve(n);
    int64_t at = head;
    for (uint32_t r = 0; r < runs && d.size() < n; r++) {
        uint32_t count = raw.decode_u16(at);
        uint8_t v = raw[at + 2];
        at += 3;
        if (d.size() + count > n) count = (uint32_t)(n - d.size());
        d.insert(d.end(), count, v);
    }
    if (d.size() != n) return;
    std::vector<uint8_t> m(n, 0);
    if (version >= 3 && raw.size() >= at + 4) {
        // The material runs follow the density runs, in the same shape.
        uint32_t mruns = raw.decode_u32(at);
        at += 4;
        size_t filled = 0;
        for (uint32_t r = 0; r < mruns && filled < n && raw.size() >= at + 3; r++) {
            uint32_t count = raw.decode_u16(at);
            uint8_t v = raw[at + 2];
            at += 3;
            if (filled + count > n) count = (uint32_t)(n - filled);
            std::fill(m.begin() + filled, m.begin() + filled + count, v < kTerrainMatCount ? v : 0);
            filled += count;
        }
    }
    terrain_.nx = nx; terrain_.ny = ny; terrain_.nz = nz;
    terrain_.cell = cell;
    terrain_.origin = origin;
    terrain_.d = std::move(d);
    terrain_.m = std::move(m);
    terrain_chunks_reset();
}

// The earlier heightfield: one height per column becomes a column of samples, solid below that
// height and air above, with a cell's worth of blend at the surface so it meshes as smoothly.
void PulseBlockzWorld::terrain_decode_heights(const PackedByteArray& raw) {
    const int64_t head = 28;
    if (raw.size() < head || raw.decode_u32(4) != 1) return;
    int res = (int)raw.decode_u32(8);
    if (res < 1 || res + 1 > kTerrainMax) return;
    float cell = raw.decode_float(12);
    if (!(cell > 0.01f)) cell = 4.0f;
    Vector3 origin(raw.decode_float(16), raw.decode_float(20), raw.decode_float(24));
    const int w = res + 1;
    if (raw.size() < head + (int64_t)w * w * 2) return;
    float lo = 1e30f, hi = -1e30f;
    std::vector<float> h((size_t)w * w);
    for (int64_t i = 0; i < (int64_t)w * w; i++) { h[(size_t)i] = raw.decode_s16(head + i * 2) * 0.25f; lo = std::min(lo, h[(size_t)i]); hi = std::max(hi, h[(size_t)i]); }
    int cellsY = std::max(8, std::min(kTerrainMaxY - 1, (int)Math::ceil((hi - lo) / cell) + 16));
    terrain_.nx = w; terrain_.nz = w; terrain_.ny = cellsY + 1;
    terrain_.cell = cell;
    terrain_.origin = Vector3(origin.x, origin.y + lo - 8 * cell, origin.z);
    terrain_.d.assign((size_t)w * w * terrain_.ny, 0);
    terrain_.m.assign(terrain_.d.size(), 0);
    for (int z = 0; z < w; z++) for (int x = 0; x < w; x++) {
        float top = origin.y + h[(size_t)z * w + x];
        for (int y = 0; y < terrain_.ny; y++) {
            float sy = terrain_.origin.y + y * cell;
            float dv = (top - sy) / cell;   // the top voxel holds (height - its centre) / 4: its surface lands on the height
            terrain_.d[terrain_.at(x, y, z)] = (uint8_t)(std::max(0.0f, std::min(1.0f, dv)) * 255.0f + 0.5f);
        }
    }
    terrain_chunks_reset();
}

// ---- Roblox's own voxels: SmoothGrid ---------------------------------------------
// What a place file carries for its Terrain:
//   u8 version = 1, u8 log2 of the chunk side = 5 (32 voxels; a voxel is 4 studs, voxel i
//   spanning [4i, 4i + 4)), then chunks. A chunk is a 12-byte position, three big-endian int32
//   DELTAS from the previous chunk in chunk units, byte-interleaved (byte j of coordinate k at
//   3j + k), then 32768 voxels as runs, index x + 32 (z + 32 y). A run's first byte is
//   material | flags: the low six bits Roblox's terrain material index (0 Air, 1 Water, 2 Grass,
//   3 Slate, 4 Concrete, 5 Brick, 6 Sand, 7 WoodPlanks, 8 Rock, 9 Glacier, 10 Snow,
//   11 Sandstone, 12 Mud, 13 Basalt, 14 Ground, 15 CrackedLava, 16 Asphalt, 17 Cobblestone,
//   18 Ice, 19 LeafyGrass, 20 Salt, 21 Limestone, 22 Pavement -- the order MaterialColors is
//   written in), 0x40 an occupancy byte follows (else a solid voxel is full), 0x80 a count byte
//   follows (count - 1; a count byte of 0 is an escape: one voxel and one more byte, seen only
//   beside water, which this reader steps over).
static const int kRobloxTerrainEnum[23] = {1792, 2048, 1280, 800, 816, 848, 1296, 528, 896, 1552, 1328, 912, 1344, 788, 1360, 804, 1376, 880, 1536, 1284, 1392, 820, 836};
static int robloxTerrainIndex(int enumValue) {
    for (int i = 0; i < 23; i++) if (kRobloxTerrainEnum[i] == enumValue) return i;
    return 2;   // something the palette here has that Roblox's ground does not: grass
}

void PulseBlockzWorld::terrain_decode_smoothgrid(const std::string& b64) {
    auto say = [&](const std::string& s) { emit_signal("script_warn", String("Terrain"), String::utf8(s.c_str())); };
    PackedByteArray raw = Marshalls::get_singleton()->base64_to_raw(String::utf8(b64.c_str()));
    const int64_t n = raw.size();
    if (n < 2 || raw[0] != 1 || raw[1] != 5) { say("SmoothGrid: not the version (1) and chunk size (32) this reads; the terrain was left out"); return; }
    struct Chunk { int cx, cy, cz; std::vector<uint8_t> mat, occ; };
    std::vector<Chunk> chunks;
    int64_t at = 2;
    int px = 0, py = 0, pz = 0, extras = 0;
    while (at + 12 <= n) {
        int32_t d[3];
        for (int k = 0; k < 3; k++) {
            uint32_t v = ((uint32_t)raw[at + k] << 24) | ((uint32_t)raw[at + 3 + k] << 16) | ((uint32_t)raw[at + 6 + k] << 8) | (uint32_t)raw[at + 9 + k];
            d[k] = (int32_t)v;
        }
        at += 12;
        px += d[0]; py += d[1]; pz += d[2];
        Chunk c; c.cx = px; c.cy = py; c.cz = pz;
        c.mat.assign(32768, 0); c.occ.assign(32768, 0);
        int vox = 0;
        while (vox < 32768 && at < n) {
            uint8_t b = raw[at++];
            int m = b & 0x3f, o = m ? 255 : 0, cnt = 1;
            if (b & 0x40) { if (at >= n) break; o = raw[at++]; }
            if (b & 0x80) {
                if (at >= n) break;
                uint8_t cb = raw[at++];
                if (cb == 0) { if (at < n) at++; extras++; } else cnt = cb + 1;
            }
            if (m > 22) m = 0;
            for (int k = 0; k < cnt && vox < 32768; k++, vox++) { c.mat[vox] = (uint8_t)m; c.occ[vox] = (uint8_t)o; }
        }
        if (vox != 32768) { say("SmoothGrid: a chunk ended short at byte " + std::to_string(at) + " of " + std::to_string(n) + "; what was read before it is kept"); break; }
        chunks.push_back(std::move(c));
    }
    if (chunks.empty()) { say("SmoothGrid: no chunks in it"); return; }
    int minCx = chunks[0].cx, maxCx = minCx, minCy = chunks[0].cy, maxCy = minCy, minCz = chunks[0].cz, maxCz = minCz;
    for (const Chunk& c : chunks) {
        minCx = std::min(minCx, c.cx); maxCx = std::max(maxCx, c.cx);
        minCy = std::min(minCy, c.cy); maxCy = std::max(maxCy, c.cy);
        minCz = std::min(minCz, c.cz); maxCz = std::max(maxCz, c.cz);
    }
    const int fullX = (maxCx - minCx + 1) * 32, fullY = (maxCy - minCy + 1) * 32, fullZ = (maxCz - minCz + 1) * 32;
    const int nx = std::min(fullX, kTerrainMax), ny = std::min(fullY, kTerrainMaxY), nz = std::min(fullZ, kTerrainMax);
    // Each voxel is a sample at its centre: the surface then falls between
    // neighbouring centres at half occupancy, where Roblox's mesher puts it too.
    terrain_.nx = nx; terrain_.ny = ny; terrain_.nz = nz;
    terrain_.cell = 4.0f;
    terrain_.origin = Vector3(minCx * 128.0f + 2.0f, minCy * 128.0f + 2.0f, minCz * 128.0f + 2.0f);
    terrain_.d.assign((size_t)nx * ny * nz, 0);
    terrain_.m.assign(terrain_.d.size(), 0);
    size_t solid = 0, water = 0;
    for (const Chunk& c : chunks) {
        const int bx = (c.cx - minCx) * 32, by = (c.cy - minCy) * 32, bz = (c.cz - minCz) * 32;
        for (int y = 0; y < 32; y++) for (int z = 0; z < 32; z++) for (int x = 0; x < 32; x++) {
            const int idx = x + 32 * (z + 32 * y);
            const uint8_t mat = c.mat[idx];
            if (!mat) continue;
            const int gx = bx + x, gy = by + y, gz = bz + z;
            if (gx >= nx || gy >= ny || gz >= nz) continue;
            terrain_.d[terrain_.at(gx, gy, gz)] = c.occ[idx];
            terrain_.m[terrain_.at(gx, gy, gz)] = (uint8_t)terrainMatIndex(kRobloxTerrainEnum[mat]);
            if (mat == 1) water++; else solid++;
        }
    }
    terrain_chunks_reset();
    terrain_.sent = terrain_encode();
    if (terrain_.id) {
        assetWrites_.push_back({terrain_.id, "Heights", Value::string(terrain_.sent)});
        assetWrites_.push_back({terrain_.id, "SmoothGrid", Value::string("")});   // read: ours is the form that persists
    }
    std::string note = "Roblox terrain read: " + std::to_string(chunks.size()) + " chunks, " + std::to_string(fullX) + " x " + std::to_string(fullY) + " x " + std::to_string(fullZ) + " voxels ("
                     + std::to_string(fullX * 4) + " x " + std::to_string(fullY * 4) + " x " + std::to_string(fullZ * 4) + " studs), " + std::to_string(solid) + " solid, " + std::to_string(water) + " water";
    if (nx < fullX || ny < fullY || nz < fullZ) note += "; clipped to " + std::to_string(nx) + " x " + std::to_string(ny) + " x " + std::to_string(nz) + " from the low corner (the field's limit)";
    say(note);
}

// kTerrainMats' colours, or the place's own MaterialColors: a 6-byte header then 21 RGB bytes
// in Roblox's material order, Grass (2) to Pavement (22); Air and Water have none.
Color PulseBlockzWorld::terrain_color(int paletteIndex) const {
    const int i = paletteIndex >= 0 && paletteIndex < kTerrainMatCount ? paletteIndex : 0;
    if ((int)terrain_.colors.size() == kTerrainMatCount) return terrain_.colors[i];
    return kTerrainMats[i].color;
}

void PulseBlockzWorld::terrain_apply_material_colors(const std::string& b64) {
    if (b64.empty()) { terrain_.colors.clear(); terrain_chunks_reset(); return; }
    PackedByteArray raw = Marshalls::get_singleton()->base64_to_raw(String::utf8(b64.c_str()));
    if (raw.size() != 69) return;
    terrain_.colors.assign(kTerrainMatCount, Color());
    for (int i = 0; i < kTerrainMatCount; i++) terrain_.colors[i] = kTerrainMats[i].color;
    for (int rob = 2; rob <= 22; rob++) {
        const int64_t at = 6 + (rob - 2) * 3;
        terrain_.colors[terrainMatIndex(kRobloxTerrainEnum[rob])] = Color(raw[at] / 255.0f, raw[at + 1] / 255.0f, raw[at + 2] / 255.0f);
    }
    terrain_chunks_reset();   // every chunk drawn again in the new colours
}

String PulseBlockzWorld::terrain_material_colors() const {
    PackedByteArray raw;
    raw.resize(69);
    for (int i = 0; i < 6; i++) raw[i] = 0;
    for (int rob = 2; rob <= 22; rob++) {
        const Color c = terrain_color(terrainMatIndex(kRobloxTerrainEnum[rob]));
        const int64_t at = 6 + (rob - 2) * 3;
        raw[at] = (uint8_t)std::lround(std::clamp(c.r, 0.0f, 1.0f) * 255.0f);
        raw[at + 1] = (uint8_t)std::lround(std::clamp(c.g, 0.0f, 1.0f) * 255.0f);
        raw[at + 2] = (uint8_t)std::lround(std::clamp(c.b, 0.0f, 1.0f) * 255.0f);
    }
    return Marshalls::get_singleton()->raw_to_base64(raw);
}

// The field back out as SmoothGrid: Roblox's voxel centres sampled from the field, since a field
// made here is not on Roblox's 4-stud grid. Air-only chunks are left out, as Roblox leaves them.
String PulseBlockzWorld::terrain_smoothgrid() const {
    if (terrain_.empty()) return String();
    const float cell = terrain_.cell;
    auto firstVoxel = [&](float o) { return (int)std::ceil((o - cell * 0.5f - 2.0f) / 4.0f); };
    auto lastVoxel = [&](float o, int count) { return (int)std::floor((o + (count - 1) * cell + cell * 0.5f - 2.0f) / 4.0f); };
    const int ix0 = firstVoxel(terrain_.origin.x), ix1 = lastVoxel(terrain_.origin.x, terrain_.nx);
    const int iy0 = firstVoxel(terrain_.origin.y), iy1 = lastVoxel(terrain_.origin.y, terrain_.ny);
    const int iz0 = firstVoxel(terrain_.origin.z), iz1 = lastVoxel(terrain_.origin.z, terrain_.nz);
    if (ix1 < ix0 || iy1 < iy0 || iz1 < iz0) return String();
    auto floorDiv = [](int a, int b) { return (int)std::floor((double)a / b); };
    PackedByteArray out;
    out.push_back(1); out.push_back(5);
    int px = 0, py = 0, pz = 0;
    std::vector<uint8_t> mat(32768), occ(32768);
    for (int cy = floorDiv(iy0, 32); cy <= floorDiv(iy1, 32); cy++)
        for (int cz = floorDiv(iz0, 32); cz <= floorDiv(iz1, 32); cz++)
            for (int cx = floorDiv(ix0, 32); cx <= floorDiv(ix1, 32); cx++) {
                bool any = false;
                for (int y = 0; y < 32; y++) for (int z = 0; z < 32; z++) for (int x = 0; x < 32; x++) {
                    const int idx = x + 32 * (z + 32 * y);
                    const int vx = cx * 32 + x, vy = cy * 32 + y, vz = cz * 32 + z;
                    mat[idx] = 0; occ[idx] = 0;
                    if (vx < ix0 || vx > ix1 || vy < iy0 || vy > iy1 || vz < iz0 || vz > iz1) continue;
                    const Vector3 centre(vx * 4.0f + 2.0f, vy * 4.0f + 2.0f, vz * 4.0f + 2.0f);
                    const int o = (int)std::lround(std::clamp(terrain_occupancy(centre), 0.0f, 1.0f) * 255.0f);   // the occupancy itself: exact on Roblox's own grid
                    if (o < 1) continue;
                    const int sx = std::clamp((int)std::lround((centre.x - terrain_.origin.x) / cell), 0, terrain_.nx - 1);
                    const int sy = std::clamp((int)std::lround((centre.y - terrain_.origin.y) / cell), 0, terrain_.ny - 1);
                    const int sz = std::clamp((int)std::lround((centre.z - terrain_.origin.z) / cell), 0, terrain_.nz - 1);
                    const uint8_t pal = terrain_.m.size() == terrain_.d.size() ? terrain_.m[terrain_.at(sx, sy, sz)] : 0;
                    int rob = robloxTerrainIndex(kTerrainMats[pal < kTerrainMatCount ? pal : 0].value);
                    if (rob == 0) rob = 2;
                    mat[idx] = (uint8_t)rob; occ[idx] = (uint8_t)o;
                    any = true;
                }
                if (!any) continue;
                const int d[3] = {cx - px, cy - py, cz - pz};
                px = cx; py = cy; pz = cz;
                for (int j = 0; j < 4; j++) for (int k = 0; k < 3; k++) out.push_back((uint8_t)(((uint32_t)d[k] >> (24 - 8 * j)) & 0xff));
                for (int i = 0; i < 32768;) {
                    int j = i + 1;
                    while (j < 32768 && j - i < 256 && mat[j] == mat[i] && occ[j] == occ[i]) j++;
                    const int cnt = j - i;
                    const bool hasOcc = mat[i] != 0 && occ[i] != 255;
                    out.push_back((uint8_t)(mat[i] | (hasOcc ? 0x40 : 0) | (cnt > 1 ? 0x80 : 0)));
                    if (hasOcc) out.push_back(occ[i]);
                    if (cnt > 1) out.push_back((uint8_t)(cnt - 1));
                    i = j;
                }
            }
    return Marshalls::get_singleton()->raw_to_base64(out);
}

std::string PulseBlockzWorld::terrain_encode() const {
    if (terrain_.empty()) return "";
    const size_t n = terrain_.d.size();
    auto rle = [n](const std::vector<uint8_t>& src) {
        std::vector<std::pair<uint16_t, uint8_t>> runs;
        for (size_t i = 0; i < n;) {
            uint8_t v = i < src.size() ? src[i] : 0;
            size_t j = i + 1;
            while (j < n && (j < src.size() ? src[j] : 0) == v && j - i < 65535) j++;
            runs.push_back({(uint16_t)(j - i), v});
            i = j;
        }
        return runs;
    };
    auto runs = rle(terrain_.d);
    auto mruns = rle(terrain_.m);
    PackedByteArray raw;
    raw.resize(40 + (int64_t)runs.size() * 3 + 4 + (int64_t)mruns.size() * 3);
    raw[0] = 'P'; raw[1] = 'B'; raw[2] = 'V'; raw[3] = 'X';
    raw.encode_u32(4, 3);
    raw.encode_u32(8, (uint32_t)terrain_.nx);
    raw.encode_u32(12, (uint32_t)terrain_.ny);
    raw.encode_u32(16, (uint32_t)terrain_.nz);
    raw.encode_float(20, terrain_.cell);
    raw.encode_float(24, terrain_.origin.x);
    raw.encode_float(28, terrain_.origin.y);
    raw.encode_float(32, terrain_.origin.z);
    raw.encode_u32(36, (uint32_t)runs.size());
    int64_t at = 40;
    for (auto& r : runs) { raw.encode_u16(at, r.first); raw[at + 2] = r.second; at += 3; }
    raw.encode_u32(at, (uint32_t)mruns.size());
    at += 4;
    for (auto& r : mruns) { raw.encode_u16(at, r.first); raw[at + 2] = r.second; at += 3; }
    return std::string(Marshalls::get_singleton()->raw_to_base64(raw).utf8().get_data());
}

int64_t PulseBlockzWorld::get_terrain_id() const { return terrain_.id; }

Dictionary PulseBlockzWorld::get_terrain_info() const {
    Dictionary out;
    if (terrain_.empty()) return out;
    out["resolution"] = terrain_.nx - 1;
    out["height_cells"] = terrain_.ny - 1;
    out["cell_size"] = terrain_.cell;
    out["origin"] = terrain_.origin;
    out["span"] = (terrain_.nx - 1) * terrain_.cell;
    return out;
}

// A flat field: solid below y, air above, in a box `resolution` cells across and half that
// tall. The cell of blend at the surface is what makes it mesh as a plane, not a staircase.
void PulseBlockzWorld::terrain_flatten(int resolution, float cell_size, float y) {
    int res = std::max(1, std::min(kTerrainMax - 1, resolution));
    int cellsY = std::max(8, std::min(kTerrainMaxY - 1, res / 2));
    float cell = cell_size > 0.01f ? cell_size : 4.0f;
    terrain_.nx = terrain_.nz = res + 1;
    terrain_.ny = cellsY + 1;
    terrain_.cell = cell;
    terrain_.origin = Vector3(-res * cell * 0.5f, y - cellsY * cell * 0.5f, -res * cell * 0.5f);
    terrain_.d.assign((size_t)terrain_.nx * terrain_.ny * terrain_.nz, 0);
    terrain_.m.assign(terrain_.d.size(), 0);
    for (int yy = 0; yy < terrain_.ny; yy++) {
        float sy = terrain_.origin.y + yy * cell;
        float dv = std::max(0.0f, std::min(1.0f, (y - sy) / cell));   // the surface at y, read Roblox's way (Terrain::s)
        uint8_t v = (uint8_t)(dv * 255.0f + 0.5f);
        for (int z = 0; z < terrain_.nz; z++) for (int x = 0; x < terrain_.nx; x++) terrain_.d[terrain_.at(x, yy, z)] = v;
    }
    terrain_chunks_reset();
}

float PulseBlockzWorld::terrain_density(const Vector3& p) const {
    if (terrain_.empty()) return 0.0f;
    float fx = (p.x - terrain_.origin.x) / terrain_.cell;
    float fy = (p.y - terrain_.origin.y) / terrain_.cell;
    float fz = (p.z - terrain_.origin.z) / terrain_.cell;
    // Just outside is air; the sample getter says so too, so the two agree.
    if (fx < -1 || fy < -1 || fz < -1 || fx > terrain_.nx || fy > terrain_.ny || fz > terrain_.nz) return 0.0f;
    int x0 = (int)Math::floor(fx), y0 = (int)Math::floor(fy), z0 = (int)Math::floor(fz);
    float tx = fx - x0, ty = fy - y0, tz = fz - z0;
    auto c = [&](int a, int b, int e) { return terrain_.dens(x0 + a, y0 + b, z0 + e); };
    float x00 = c(0, 0, 0) * (1 - tx) + c(1, 0, 0) * tx;
    float x10 = c(0, 1, 0) * (1 - tx) + c(1, 1, 0) * tx;
    float x01 = c(0, 0, 1) * (1 - tx) + c(1, 0, 1) * tx;
    float x11 = c(0, 1, 1) * (1 - tx) + c(1, 1, 1) * tx;
    float y0v = x00 * (1 - ty) + x10 * ty;
    float y1v = x01 * (1 - ty) + x11 * ty;
    return y0v * (1 - tz) + y1v * tz;
}

float PulseBlockzWorld::terrain_occupancy(const Vector3& p) const {
    if (terrain_.empty()) return 0.0f;
    float fx = (p.x - terrain_.origin.x) / terrain_.cell;
    float fy = (p.y - terrain_.origin.y) / terrain_.cell;
    float fz = (p.z - terrain_.origin.z) / terrain_.cell;
    if (fx < -1 || fy < -1 || fz < -1 || fx > terrain_.nx || fy > terrain_.ny || fz > terrain_.nz) return 0.0f;
    int x0 = (int)Math::floor(fx), y0 = (int)Math::floor(fy), z0 = (int)Math::floor(fz);
    float tx = fx - x0, ty = fy - y0, tz = fz - z0;
    auto c = [&](int a, int b, int e) { return terrain_.occ(x0 + a, y0 + b, z0 + e); };
    float x00 = c(0, 0, 0) * (1 - tx) + c(1, 0, 0) * tx;
    float x10 = c(0, 1, 0) * (1 - tx) + c(1, 1, 0) * tx;
    float x01 = c(0, 0, 1) * (1 - tx) + c(1, 0, 1) * tx;
    float x11 = c(0, 1, 1) * (1 - tx) + c(1, 1, 1) * tx;
    float y0v = x00 * (1 - ty) + x10 * ty;
    float y1v = x01 * (1 - ty) + x11 * ty;
    return y0v * (1 - tz) + y1v * tz;
}

Vector3 PulseBlockzWorld::terrain_gradient(const Vector3& p) const {
    float e = terrain_.cell * 0.5f;
    return Vector3(terrain_density(p + Vector3(e, 0, 0)) - terrain_density(p - Vector3(e, 0, 0)),
                   terrain_density(p + Vector3(0, e, 0)) - terrain_density(p - Vector3(0, e, 0)),
                   terrain_density(p + Vector3(0, 0, e)) - terrain_density(p - Vector3(0, 0, e)));
}

// The ground's top under (x, z), by Roblox's column rule: the topmost voxel with matter in it
// puts the surface occupancy * a cell above its centre, blended across the four columns round
// the point. -1e30 where there is none; a cave's roof is above the floor a column finds first.
float PulseBlockzWorld::terrain_height_at(float x, float z) const {
    if (terrain_.empty()) return -1e30f;
    const float cell = terrain_.cell;
    float fx = (x - terrain_.origin.x) / cell;
    float fz = (z - terrain_.origin.z) / cell;
    if (fx < 0 || fz < 0 || fx > terrain_.nx - 1 || fz > terrain_.nz - 1) return -1e30f;
    const int ix = std::min(terrain_.nx - 2, std::max(0, (int)Math::floor(fx))), iz = std::min(terrain_.nz - 2, std::max(0, (int)Math::floor(fz)));
    const float tx = std::clamp(fx - ix, 0.0f, 1.0f), tz = std::clamp(fz - iz, 0.0f, 1.0f);
    auto column = [&](int cx, int cz, float& h) {
        for (int y = terrain_.ny - 1; y >= 0; y--) {
            const float o = terrain_.occ(cx, y, cz);
            if (o > 0) { h = terrain_.origin.y + y * cell + cell * std::min(0.995f, o); return true; }
        }
        return false;
    };
    float h00 = 0, h10 = 0, h01 = 0, h11 = 0;
    const bool b00 = column(ix, iz, h00), b10 = column(ix + 1, iz, h10), b01 = column(ix, iz + 1, h01), b11 = column(ix + 1, iz + 1, h11);
    const float w00 = (1 - tx) * (1 - tz), w10 = tx * (1 - tz), w01 = (1 - tx) * tz, w11 = tx * tz;
    float sum = 0, wsum = 0;
    if (b00) { sum += h00 * w00; wsum += w00; }
    if (b10) { sum += h10 * w10; wsum += w10; }
    if (b01) { sum += h01 * w01; wsum += w01; }
    if (b11) { sum += h11 * w11; wsum += w11; }
    if (wsum < 1e-6f) return -1e30f;
    return sum / wsum;
}

// Where the ray meets the ground, against the drawn mesh itself. Parts in the way are looked
// past; only the Terrain counts.
Dictionary PulseBlockzWorld::terrain_raycast(const Vector3& from, const Vector3& dir) const {
    Dictionary out;
    if (terrain_.empty() || !terrain_.body || !is_inside_tree()) return out;
    Ref<World3D> w3 = get_world_3d();
    PhysicsDirectSpaceState3D* space = w3.is_valid() ? w3->get_direct_space_state() : nullptr;
    if (!space) return out;
    const Vector3 d = dir.normalized();
    const float far = std::max(terrain_.nx, std::max(terrain_.ny, terrain_.nz)) * terrain_.cell * 3.0f + 4096.0f;
    TypedArray<RID> exclude;
    for (int hop = 0; hop < 64; hop++) {
        Ref<PhysicsRayQueryParameters3D> q = PhysicsRayQueryParameters3D::create(from, from + d * far, static_bit(), exclude);
        Dictionary hit = space->intersect_ray(q);
        if (hit.is_empty()) return out;
        if (Object* co = Object::cast_to<Object>(hit["collider"]); co == terrain_.body) {
            out["position"] = hit["position"];
            out["normal"] = hit["normal"];
            return out;
        }
        exclude.push_back(hit["rid"]);
    }
    return out;
}

// mode 0 adds solid in a ball, 1 takes it away, 2 smooths, 3 flattens to `level`, 4 paints. Add
// and Subtract set each voxel to its share of the ball, as FillBall and Roblox's own brush do;
// easing them in instead leaves specks of a few percent that Roblox draws as floating blobs.
// Smooth and Flatten ease toward their goal by `amount` (cells of density) under a raised cosine.
void PulseBlockzWorld::terrain_sculpt(const Vector3& at, float radius, float amount, int mode, float level, int material) {
    if (terrain_.empty() || radius <= 0) return;
    if (terrain_.m.size() != terrain_.d.size()) terrain_.m.assign(terrain_.d.size(), 0);
    const int paint = material >= 0 ? terrainMatIndex(material) : -1;
    const float cell = terrain_.cell;
    const float reach = radius + cell * 0.5f;   // a voxel whose centre is just outside the ball still has a share of it
    int x0 = std::max(0, (int)Math::floor((at.x - reach - terrain_.origin.x) / cell));
    int y0 = std::max(0, (int)Math::floor((at.y - reach - terrain_.origin.y) / cell));
    int z0 = std::max(0, (int)Math::floor((at.z - reach - terrain_.origin.z) / cell));
    int x1 = std::min(terrain_.nx - 1, (int)Math::ceil((at.x + reach - terrain_.origin.x) / cell));
    int y1 = std::min(terrain_.ny - 1, (int)Math::ceil((at.y + reach - terrain_.origin.y) / cell));
    int z1 = std::min(terrain_.nz - 1, (int)Math::ceil((at.z + reach - terrain_.origin.z) / cell));
    if (x0 > x1 || y0 > y1 || z0 > z1) return;
    std::vector<uint8_t> was;
    if (mode == 2) was = terrain_.d;                              // smoothing reads the old field
    const float push = amount / cell;                             // density per call
    for (int z = z0; z <= z1; z++) for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        Vector3 sp = terrain_.origin + Vector3(x, y, z) * cell;
        float dist = sp.distance_to(at);
        if (dist > reach) continue;
        const float fill = std::clamp(0.5f + (radius - dist) / cell, 0.0f, 1.0f);   // the voxel's share of the ball
        float fall = dist >= radius ? 0.0f : 0.5f * (1.0f + Math::cos(Math_PI * (dist / radius)));
        uint8_t& cellv = terrain_.d[terrain_.at(x, y, z)];
        float dv = cellv * (1.0f / 255.0f);
        if (mode == 4) {
            // Paint: what the ground is made of changes, its shape does not.
            if (paint >= 0 && dv > 0 && fall > 0.05f) terrain_.m[terrain_.at(x, y, z)] = (uint8_t)paint;   // any matter takes the paint
            continue;
        }
        if (mode == 0) {
            if (fill > dv) { dv = fill; if (paint >= 0) terrain_.m[terrain_.at(x, y, z)] = (uint8_t)paint; }   // added ground is the brush's material
        }
        else if (mode == 1) dv = std::min(dv, 1.0f - fill);
        else if (mode == 2) {
            float sum = 0; int n = 0;
            for (int c = z - 1; c <= z + 1; c++) for (int b = y - 1; b <= y + 1; b++) for (int a = x - 1; a <= x + 1; a++)
                if (a >= 0 && b >= 0 && c >= 0 && a < terrain_.nx && b < terrain_.ny && c < terrain_.nz) { sum += was[terrain_.at(a, b, c)]; n++; }
            if (n) dv += (sum / n * (1.0f / 255.0f) - dv) * std::min(1.0f, fall * std::max(push, 0.25f));
        } else {
            float want = std::max(0.0f, std::min(1.0f, (level - sp.y) / cell));   // the Flatten brush's level, read Roblox's way
            dv += (want - dv) * std::min(1.0f, fall * std::max(push, 0.25f));
        }
        cellv = (uint8_t)(std::max(0.0f, std::min(1.0f, dv)) * 255.0f + 0.5f);
    }
    terrain_mark(x0, y0, z0, x1, y1, z1);
}

String PulseBlockzWorld::terrain_material_at(const Vector3& p) const {
    if (terrain_.empty()) return String();
    if (terrain_density(p) < kIso) return terrain_in_water(p) ? String("Water") : String();
    // Of the eight samples round the point, the one with the most matter in it:
    // the nearest may be the air voxel just over the ground the point is in.
    int x0 = (int)Math::floor((p.x - terrain_.origin.x) / terrain_.cell);
    int y0 = (int)Math::floor((p.y - terrain_.origin.y) / terrain_.cell);
    int z0 = (int)Math::floor((p.z - terrain_.origin.z) / terrain_.cell);
    int best = -1; size_t bestAt = 0;
    for (int dz = 0; dz < 2; dz++) for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) {
        int x = x0 + dx, y = y0 + dy, z = z0 + dz;
        if (x < 0 || y < 0 || z < 0 || x >= terrain_.nx || y >= terrain_.ny || z >= terrain_.nz) continue;
        size_t i = terrain_.at(x, y, z);
        if ((int)terrain_.d[i] > best) { best = terrain_.d[i]; bestAt = i; }
    }
    if (best < 0) return String();
    uint8_t mi = terrain_.m.size() == terrain_.d.size() ? terrain_.m[bestAt] : 0;
    return String(kTerrainMats[mi < kTerrainMatCount ? mi : 0].name);
}

// A script's impulse lands on the rigid body of the part's assembly: at its centre of mass, at
// the point given, or as a torque impulse. A part not simulated here takes none.
void PulseBlockzWorld::apply_impulses(const std::vector<pulseblockz::rbx::Runtime::Impulse>& ims) {
    for (const auto& im : ims) {
        int64_t root = weld_root(im.part) ? weld_root(im.part) : im.part;
        auto it = parts_.find(root);
        if (it == parts_.end()) continue;
        auto* rb = Object::cast_to<RigidBody3D>(it->second.body);
        if (!rb) continue;
        if (im.kind == 0) rb->apply_central_impulse(toGd(im.v));
        else if (im.kind == 1) rb->apply_impulse(toGd(im.v), toGd(im.at) - rb->get_global_position());
        else rb->apply_torque_impulse(toGd(im.v));
    }
}

// A script's FillBlock / FillBall / FillCylinder / FillWedge / ReplaceMaterial / WriteVoxels /
// Clear. A field that has never been made is made now, as air; the result goes back to the tree
// as one Heights write.
void PulseBlockzWorld::apply_terrain_ops(const std::vector<pulseblockz::rbx::Runtime::TerrainOp>& ops) {
    if (ops.empty() || !terrain_.id) return;
    if (terrain_.empty()) {
        // 512 studs across and 256 tall, centred on the origin, all air.
        terrain_.nx = terrain_.nz = 129;
        terrain_.ny = 65;
        terrain_.cell = 4.0f;
        terrain_.origin = Vector3(-256.0f, -128.0f, -256.0f);
        terrain_.d.assign((size_t)terrain_.nx * terrain_.ny * terrain_.nz, 0);
        terrain_.m.assign(terrain_.d.size(), 0);
        terrain_chunks_reset();
    }
    if (terrain_.m.size() != terrain_.d.size()) terrain_.m.assign(terrain_.d.size(), 0);
    const float cell = terrain_.cell;
    for (const auto& op : ops) {
        if (op.kind == 2) { std::fill(terrain_.d.begin(), terrain_.d.end(), 0); terrain_mark(0, 0, 0, terrain_.nx - 1, terrain_.ny - 1, terrain_.nz - 1); continue; }
        const bool air = op.material == 1792;
        const uint8_t mi = (uint8_t)terrainMatIndex(op.material);
        if (op.kind == 6) {
            // WriteVoxels: the region's cells, from its low corner, take the occupancies and materials as given.
            const int cx = (int)Math::round((op.pos.x - terrain_.origin.x) / cell), cy = (int)Math::round((op.pos.y - terrain_.origin.y) / cell), cz = (int)Math::round((op.pos.z - terrain_.origin.z) / cell);
            for (int z = 0; z < op.nz; z++) for (int y = 0; y < op.ny; y++) for (int x = 0; x < op.nx; x++) {
                const int gx = cx + x, gy = cy + y, gz = cz + z;
                if (gx < 0 || gy < 0 || gz < 0 || gx >= terrain_.nx || gy >= terrain_.ny || gz >= terrain_.nz) continue;
                const size_t k = (size_t)x + (size_t)op.nx * ((size_t)y + (size_t)op.ny * z);
                const int mat = k < op.materials.size() ? op.materials[k] : 1792;
                const uint8_t occ = k < op.occupancy.size() ? op.occupancy[k] : 0;
                terrain_.d[terrain_.at(gx, gy, gz)] = mat == 1792 ? 0 : occ;
                terrain_.m[terrain_.at(gx, gy, gz)] = (uint8_t)terrainMatIndex(mat);
            }
            terrain_mark(std::max(0, cx), std::max(0, cy), std::max(0, cz), std::min(terrain_.nx - 1, cx + op.nx), std::min(terrain_.ny - 1, cy + op.ny), std::min(terrain_.nz - 1, cz + op.nz));
            continue;
        }
        if (op.kind == 5) {
            // ReplaceMaterial: within the box, every voxel of one material becomes the other; the occupancy stays.
            const uint8_t from = (uint8_t)terrainMatIndex(op.fromMaterial);
            const bool fromAir = op.fromMaterial == 1792;
            const int x0 = std::max(0, (int)Math::round((op.pos.x - op.size.x * 0.5f - terrain_.origin.x) / cell)), x1 = std::min(terrain_.nx - 1, (int)Math::round((op.pos.x + op.size.x * 0.5f - terrain_.origin.x) / cell) - 1);
            const int y0 = std::max(0, (int)Math::round((op.pos.y - op.size.y * 0.5f - terrain_.origin.y) / cell)), y1 = std::min(terrain_.ny - 1, (int)Math::round((op.pos.y + op.size.y * 0.5f - terrain_.origin.y) / cell) - 1);
            const int z0 = std::max(0, (int)Math::round((op.pos.z - op.size.z * 0.5f - terrain_.origin.z) / cell)), z1 = std::min(terrain_.nz - 1, (int)Math::round((op.pos.z + op.size.z * 0.5f - terrain_.origin.z) / cell) - 1);
            for (int z = z0; z <= z1; z++) for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
                const size_t k = terrain_.at(x, y, z);
                const bool isAir = terrain_.d[k] == 0;
                if (fromAir ? !isAir : (isAir || terrain_.m[k] != from)) continue;
                if (air) terrain_.d[k] = 0;
                else { terrain_.m[k] = mi; if (isAir) terrain_.d[k] = 255; }
            }
            terrain_mark(x0, y0, z0, x1, y1, z1);
            continue;
        }
        CFrameV cf;
        cf.p = op.pos;
        for (int i = 0; i < 9; i++) cf.m[i] = op.rot[i];
        Vector3 centre = toGd(op.pos);
        float reach = op.kind == 1 ? op.radius : op.kind == 3 ? std::sqrt(op.radius * op.radius + op.size.y * op.size.y * 0.25f) : Vector3(op.size.x, op.size.y, op.size.z).length() * 0.5f;
        int x0 = std::max(0, (int)Math::floor((centre.x - reach - terrain_.origin.x) / cell));
        int y0 = std::max(0, (int)Math::floor((centre.y - reach - terrain_.origin.y) / cell));
        int z0 = std::max(0, (int)Math::floor((centre.z - reach - terrain_.origin.z) / cell));
        int x1 = std::min(terrain_.nx - 1, (int)Math::ceil((centre.x + reach - terrain_.origin.x) / cell));
        int y1 = std::min(terrain_.ny - 1, (int)Math::ceil((centre.y + reach - terrain_.origin.y) / cell));
        int z1 = std::min(terrain_.nz - 1, (int)Math::ceil((centre.z + reach - terrain_.origin.z) / cell));
        CFrameV inv = cf.inverse();
        for (int z = z0; z <= z1; z++) for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            Vector3 sp = terrain_.origin + Vector3(x, y, z) * cell;
            float inside;   // how far inside the shape, in studs (negative outside)
            if (op.kind == 1) inside = op.radius - sp.distance_to(centre);
            else {
                Vec3 l = inv * Vec3{sp.x, sp.y, sp.z};
                if (op.kind == 3)   // a cylinder about the frame's Y axis
                    inside = std::min(op.size.y * 0.5f - std::fabs(l.y), op.radius - std::sqrt(l.x * l.x + l.z * l.z));
                else {
                    inside = std::min(std::min(op.size.x * 0.5f - std::fabs(l.x), op.size.y * 0.5f - std::fabs(l.y)), op.size.z * 0.5f - std::fabs(l.z));
                    // a wedge is a block cut by the slope from its back-top edge (+Z) down to its front-bottom edge (-Z)
                    if (op.kind == 4 && op.size.y > 0 && op.size.z > 0) {
                        const float top = -op.size.y * 0.5f + (l.z + op.size.z * 0.5f) / op.size.z * op.size.y;
                        const float n = std::sqrt(op.size.y * op.size.y + op.size.z * op.size.z);
                        inside = std::min(inside, (top - l.y) * op.size.z / n);
                    }
                }
            }
            if (inside < -cell * 0.5f) continue;
            // Solid inside, air outside, with half a cell of blend at the face so
            // the surface lands on the face rather than a sample either side.
            float want = std::max(0.0f, std::min(1.0f, 0.5f + inside / cell));
            uint8_t& dv = terrain_.d[terrain_.at(x, y, z)];
            float now = dv * (1.0f / 255.0f);
            float next = air ? std::min(now, 1.0f - want) : std::max(now, want);
            dv = (uint8_t)(next * 255.0f + 0.5f);
            // The fill's material goes wherever it added matter: keeping the old one on a
            // half-filled boundary voxel would lay a lid of ground over a block of water.
            if (!air && want > 0.0f && want > now) terrain_.m[terrain_.at(x, y, z)] = mi;
        }
        terrain_mark(x0, y0, z0, x1, y1, z1);
    }
    assetWrites_.push_back({terrain_.id, "Heights", Value::string(std::string(terrain_commit().utf8().get_data()))});
}

String PulseBlockzWorld::terrain_commit() {
    terrain_.sent = terrain_encode();     // so the write coming back is not decoded again
    return String::utf8(terrain_.sent.c_str());
}

// ---- drawing the ground, a chunk at a time ---------------------------------------
// Cells run from -1 to n-1 on each axis (the ring outside the field is air, so the sides and the
// bottom close); chunk (i, j, k) owns the CH cells from i*CH - 1. A chunk computes vertices for
// its own cells and a halo of one, but emits quads only for the edges its own cells own, so a
// boundary edge is drawn exactly once and the mesh stays watertight.
//
// The surface field from the occupancies (Terrain::s): a material sample is 128 + 127 * o; an
// air sample is as far below 128 as its fullest neighbour's surface is short of its centre, and
// 0 with no matter round it.
void PulseBlockzWorld::terrain_refresh_surface(int x0, int y0, int z0, int x1, int y1, int z1) {
    if (terrain_.empty()) { terrain_.s.clear(); terrain_.sw.clear(); return; }
    terrain_.waterIndex = kWaterIndex;
    if (terrain_.s.size() != terrain_.d.size()) terrain_.s.assign(terrain_.d.size(), 0);
    if (terrain_.sw.size() != terrain_.d.size()) terrain_.sw.assign(terrain_.d.size(), 0);
    x0 = std::max(0, x0 - 1); y0 = std::max(0, y0 - 1); z0 = std::max(0, z0 - 1);
    x1 = std::min(terrain_.nx - 1, x1 + 1); y1 = std::min(terrain_.ny - 1, y1 + 1); z1 = std::min(terrain_.nz - 1, z1 + 1);
    const int nx = terrain_.nx, ny = terrain_.ny, nz = terrain_.nz;
    const uint8_t* d = terrain_.d.data();
    // the two fields: a voxel's occupancy counts for the ground or for the water by its material
    auto ground = [&](size_t i) -> int { return terrain_.isWater(i) ? 0 : d[i]; };
    auto water = [&](size_t i) -> int { return terrain_.isWater(i) ? d[i] : 0; };
    for (int z = z0; z <= z1; z++) for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        const size_t i = terrain_.at(x, y, z);
        for (int pass = 0; pass < 2; pass++) {
            auto& field = pass ? terrain_.sw : terrain_.s;
            auto of = [&](size_t k) { return pass ? water(k) : ground(k); };
            const int o = of(i);
            if (o > 0) { field[i] = (uint8_t)(128 + (o * 127 + 127) / 255); continue; }
            int m = 0;
            if (x > 0) m = std::max(m, of(i - 1));
            if (x < nx - 1) m = std::max(m, of(i + 1));
            if (y > 0) m = std::max(m, of(i - nx));
            if (y < ny - 1) m = std::max(m, of(i + nx));
            if (z > 0) m = std::max(m, of(i - (size_t)nx * ny));
            if (z < nz - 1) m = std::max(m, of(i + (size_t)nx * ny));
            field[i] = (uint8_t)(m ? 127 - (127 * (255 - m) + 127) / 255 : 0);
        }
    }
}

float PulseBlockzWorld::terrain_water_density(const Vector3& p) const {
    if (terrain_.empty() || terrain_.sw.empty()) return 0.0f;
    float fx = (p.x - terrain_.origin.x) / terrain_.cell;
    float fy = (p.y - terrain_.origin.y) / terrain_.cell;
    float fz = (p.z - terrain_.origin.z) / terrain_.cell;
    if (fx < -1 || fy < -1 || fz < -1 || fx > terrain_.nx || fy > terrain_.ny || fz > terrain_.nz) return 0.0f;
    int x0 = (int)Math::floor(fx), y0 = (int)Math::floor(fy), z0 = (int)Math::floor(fz);
    float tx = fx - x0, ty = fy - y0, tz = fz - z0;
    auto c = [&](int a, int b, int e) { return terrain_.wdens(x0 + a, y0 + b, z0 + e); };
    float x00 = c(0, 0, 0) * (1 - tx) + c(1, 0, 0) * tx;
    float x10 = c(0, 1, 0) * (1 - tx) + c(1, 1, 0) * tx;
    float x01 = c(0, 0, 1) * (1 - tx) + c(1, 0, 1) * tx;
    float x11 = c(0, 1, 1) * (1 - tx) + c(1, 1, 1) * tx;
    float y0v = x00 * (1 - ty) + x10 * ty;
    float y1v = x01 * (1 - ty) + x11 * ty;
    return y0v * (1 - tz) + y1v * tz;
}
bool PulseBlockzWorld::terrain_in_water(const Vector3& p) const { return terrain_water_density(p) >= kIso; }

Dictionary PulseBlockzWorld::terrain_voxel_at(const Vector3& p) const {
    Dictionary out;
    if (terrain_.empty()) return out;
    int x = (int)Math::floor((p.x - terrain_.origin.x) / terrain_.cell + 0.5f);
    int y = (int)Math::floor((p.y - terrain_.origin.y) / terrain_.cell + 0.5f);
    int z = (int)Math::floor((p.z - terrain_.origin.z) / terrain_.cell + 0.5f);
    if (x < 0 || y < 0 || z < 0 || x >= terrain_.nx || y >= terrain_.ny || z >= terrain_.nz) return out;
    size_t i = terrain_.at(x, y, z);
    out["centre"] = terrain_.origin + Vector3(x, y, z) * terrain_.cell;
    out["occupancy"] = terrain_.d[i] / 255.0;
    int mi = terrain_.m.size() == terrain_.d.size() ? terrain_.m[i] : 0;
    out["material"] = String(kTerrainMats[mi < kTerrainMatCount ? mi : 0].name);
    out["material_index"] = mi;
    out["water_index"] = terrain_.waterIndex;
    out["ground"] = terrain_.dens(x, y, z);
    out["water"] = terrain_.wdens(x, y, z);
    return out;
}

// The water's material: WaterColor at 1 - WaterTransparency, seen from both sides, glossy and
// moving. A shader rather than a standard material because two of the four properties are
// motion: the waves are two crossing sines and the normal comes from their slope. WaveSize is
// Roblox's amplitude in studs, WaveSpeed how fast they travel; at size 0 the surface is flat.
static const char* kWaterShader = R"GLSL(
shader_type spatial;
render_mode cull_disabled, depth_prepass_alpha;

uniform vec4 tint : source_color = vec4(0.2, 0.45, 0.8, 0.7);
uniform float wave_size = 0.15;
uniform float wave_speed = 10.0;
uniform float rough = 0.15;
uniform float metal = 0.2;

// Two crossing waves, one longer than the other so the surface never repeats visibly.
float height(vec2 at, float t) {
	return sin(at.x * 0.35 + t) * 0.6 + sin(at.y * 0.23 - t * 0.8) * 0.4;
}

void vertex() {
	if (wave_size > 0.0) {
		float t = TIME * wave_speed * 0.1;
		vec2 at = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xz;
		VERTEX.y += height(at, t) * wave_size;
		// The slope of the same two waves: without it the light sits still on a moving surface.
		float e = 0.5;
		float dx = (height(at + vec2(e, 0.0), t) - height(at - vec2(e, 0.0), t)) * wave_size;
		float dz = (height(at + vec2(0.0, e), t) - height(at - vec2(0.0, e), t)) * wave_size;
		NORMAL = normalize(vec3(-dx, 2.0 * e, -dz));
	}
}

void fragment() {
	ALBEDO = tint.rgb;
	ALPHA = tint.a;
	ROUGHNESS = rough;
	METALLIC = metal;
}
)GLSL";

void PulseBlockzWorld::terrain_style_water() {
    if (terrain_.waterMat.is_null()) {
        Ref<Shader> sh;
        sh.instantiate();
        sh->set_code(String::utf8(kWaterShader));
        terrain_.waterMat.instantiate();
        terrain_.waterMat->set_shader(sh);
    }
    Color c = terrain_.waterColor;
    c.a = std::clamp(1.0f - terrain_.waterTransparency, 0.05f, 1.0f);
    terrain_.waterMat->set_shader_parameter("tint", c);
    terrain_.waterMat->set_shader_parameter("wave_size", std::max(terrain_.waveSize, 0.0f));
    terrain_.waterMat->set_shader_parameter("wave_speed", std::max(terrain_.waveSpeed, 0.0f));
    // Reflectance 0 is a dull pond, 1 a mirror. It takes metal and smoothness together: metal
    // alone on a rough surface is a grey sheet, smooth alone barely reflects except at a glance.
    const float r = std::clamp(terrain_.waterReflectance, 0.0f, 1.0f);
    terrain_.waterMat->set_shader_parameter("rough", 0.5f - 0.45f * r);
    terrain_.waterMat->set_shader_parameter("metal", 0.05f + 0.75f * r);
}

// Archimedes for the loose parts: a body in water is pushed up by the water it displaces, by how
// much of it is under, and dragged. Water's density is 1 against the part's -- Plastic 0.7
// floats, Metal 7.85 sinks. An assembly is judged by its root part.
void PulseBlockzWorld::step_buoyancy(double dt) {
    (void)dt;
    if (terrain_.empty() || terrain_.sw.empty()) return;
    for (auto& [id, p] : parts_) {
        auto* rb = Object::cast_to<RigidBody3D>(p.body);
        if (!rb) continue;
        int64_t root = weld_root(id);
        if (root && root != id) continue;
        Vector3 at = rb->get_global_position();
        float half = std::max(0.5f, p.size.y * 0.5f);
        int under = 0;
        for (int k = -1; k <= 1; k++) if (terrain_in_water(at + Vector3(0, half * k, 0))) under++;
        if (!under) continue;
        double density = physicsOf(p.material, p.custom).density;
        float frac = under / 3.0f;
        float m = rb->get_mass();
        rb->apply_central_force(Vector3(0, (float)(m * gravity_ / std::max(0.05, density)) * frac, 0));
        rb->apply_central_force(-rb->get_linear_velocity() * m * 1.5f * frac);
        rb->apply_torque(-rb->get_angular_velocity() * m * 0.5f * frac);
    }
}

void PulseBlockzWorld::terrain_chunks_reset() {
    terrain_refresh_surface(0, 0, 0, terrain_.nx - 1, terrain_.ny - 1, terrain_.nz - 1);
    for (auto& c : terrain_.chunks) {
        if (c.mesh) c.mesh->queue_free();
        if (c.col) c.col->queue_free();
        if (c.water) c.water->queue_free();
    }
    terrain_.chunks.clear();
    terrain_.cx = terrain_.cy = terrain_.cz = 0;
    if (!terrain_.empty()) {
        // cells -1 .. n-1 is n+1 cells; chunk index = (cell + 1) / CH
        terrain_.cx = (terrain_.nx + 1 + Terrain::CH - 1) / Terrain::CH;
        terrain_.cy = (terrain_.ny + 1 + Terrain::CH - 1) / Terrain::CH;
        terrain_.cz = (terrain_.nz + 1 + Terrain::CH - 1) / Terrain::CH;
        terrain_.chunks.resize((size_t)terrain_.cx * terrain_.cy * terrain_.cz);
    }
    terrain_.dirty = true;
}

// Samples x0..x1 (etc.) changed. A sample corners the cell of its own index and the one below,
// and a vertex reads its neighbours a cell either side, so the chunks two cells round the range
// are the ones that may look different.
void PulseBlockzWorld::terrain_mark(int x0, int y0, int z0, int x1, int y1, int z1) {
    terrain_refresh_surface(x0, y0, z0, x1, y1, z1);
    if (terrain_.chunks.empty()) return;
    auto lo = [](int s) { return std::max(0, (s - 2 + 1) / Terrain::CH); };
    auto hi = [](int s, int n) { return std::min(n - 1, (s + 2 + 1) / Terrain::CH); };
    for (int k = lo(z0); k <= hi(z1, terrain_.cz); k++)
        for (int j = lo(y0); j <= hi(y1, terrain_.cy); j++)
            for (int i = lo(x0); i <= hi(x1, terrain_.cx); i++)
                terrain_.chunks[((size_t)k * terrain_.cy + j) * terrain_.cx + i].dirty = true;
    terrain_.dirty = true;
}

void PulseBlockzWorld::rebuild_terrain() {
    terrain_.dirty = false;
    terrain_.rebuilt = 0;
    if (terrain_.empty()) {
        terrain_chunks_reset();
        terrain_.dirty = false;
        if (terrain_.body) { terrain_.body->queue_free(); terrain_.body = nullptr; }
        return;
    }
    if (!terrain_.body) {
        terrain_.body = memnew(StaticBody3D);
        terrain_.body->set_name("Terrain");
        terrain_.body->set_collision_layer(static_bit());      // this world's space, like every other body
        terrain_.body->set_collision_mask(static_bit() | rigid_bit());
        add_child(terrain_.body);
    }
    if (terrain_.mat.is_null()) {
        terrain_.mat.instantiate();
        terrain_.mat->set_albedo(Color(1, 1, 1));
        terrain_.mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);   // the palette rides on the vertices
        terrain_.mat->set_flag(BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);          // ... as the colours people pick, not linear light
        terrain_.mat->set_roughness(0.95f);
    }
    if (terrain_.waterMat.is_null()) terrain_style_water();
    if (terrain_.chunks.empty()) terrain_chunks_reset();
    for (int k = 0; k < terrain_.cz; k++) for (int j = 0; j < terrain_.cy; j++) for (int i = 0; i < terrain_.cx; i++) {
        Terrain::Chunk& c = terrain_.chunks[((size_t)k * terrain_.cy + j) * terrain_.cx + i];
        if (!c.dirty) continue;
        c.dirty = false;
        terrain_mesh_chunk(i, j, k);
        terrain_.rebuilt++;
    }
}

void PulseBlockzWorld::terrain_mesh_chunk(int ci, int cj, int ck) {
    Terrain::Chunk& c = terrain_.chunks[((size_t)ck * terrain_.cy + cj) * terrain_.cx + ci];
    terrain_mesh_pass(c, ci, cj, ck, false);   // the ground, with its collision
    terrain_mesh_pass(c, ci, cj, ck, true);    // the water, drawn only
}

// One of the two fields: `water` picks the water field and a translucent surface with no
// collision.
void PulseBlockzWorld::terrain_mesh_pass(Terrain::Chunk& c, int ci, int cj, int ck, bool water) {
    auto D = [&](int x, int y, int z) { return water ? terrain_.wdens(x, y, z) : terrain_.dens(x, y, z); };
    auto O = [&](int x, int y, int z) { return water ? terrain_.wocc(x, y, z) : terrain_.occ(x, y, z); };
    const int nx = terrain_.nx, ny = terrain_.ny, nz = terrain_.nz;
    const float cell = terrain_.cell;
    const int CH = Terrain::CH;
    // own cells, then the halo
    const int ox0 = ci * CH - 1, oy0 = cj * CH - 1, oz0 = ck * CH - 1;
    const int ox1 = std::min(nx - 1, ox0 + CH - 1), oy1 = std::min(ny - 1, oy0 + CH - 1), oz1 = std::min(nz - 1, oz0 + CH - 1);
    const int hx0 = ox0 - 1, hy0 = oy0 - 1, hz0 = oz0 - 1;
    const int W = CH + 2;
    std::vector<int32_t> vid((size_t)W * W * W, -1);
    auto cid = [&](int x, int y, int z) -> int32_t& { return vid[((size_t)(z - hz0) * W + (y - hy0)) * W + (x - hx0)]; };
    PackedVector3Array verts, norms;
    PackedColorArray cols;
    PackedInt32Array tris;
    static const int corner[8][3] = {{0,0,0},{1,0,0},{0,1,0},{1,1,0},{0,0,1},{1,0,1},{0,1,1},{1,1,1}};
    static const int edge[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    for (int z = hz0; z <= oz1 + 1; z++) for (int y = hy0; y <= oy1 + 1; y++) for (int x = hx0; x <= ox1 + 1; x++) {
        if (x < -1 || y < -1 || z < -1 || x > nx - 1 || y > ny - 1 || z > nz - 1) continue;
        float cv[8];
        int mask = 0;
        for (int i = 0; i < 8; i++) { cv[i] = D(x + corner[i][0], y + corner[i][1], z + corner[i][2]); if (cv[i] >= kIso) mask |= 1 << i; }
        if (mask == 0 || mask == 255) continue;
        Vector3 sum;
        int n = 0;
        for (int e = 0; e < 12; e++) {
            int a = edge[e][0], b = edge[e][1];
            if ((cv[a] >= kIso) == (cv[b] >= kIso)) continue;
            // Roblox's rule: the surface is occupancy * a cell out from the matter side's
            // centre toward the air side, the edge's own two voxels deciding. A full voxel's
            // face reaches the air centre, kept a hair short so cells keep their own vertex.
            const int solidEnd = cv[a] >= kIso ? a : b;
            const float o = std::min(0.995f, O(x + corner[solidEnd][0], y + corner[solidEnd][1], z + corner[solidEnd][2]));
            float t = solidEnd == a ? o : 1.0f - o;
            Vector3 pa(corner[a][0], corner[a][1], corner[a][2]), pb(corner[b][0], corner[b][1], corner[b][2]);
            sum += pa + (pb - pa) * t;
            n++;
        }
        Vector3 world = terrain_.origin + (Vector3(x, y, z) + sum / (float)std::max(n, 1)) * cell;
        cid(x, y, z) = verts.size();
        verts.push_back(world);
        Vector3 g = terrain_gradient(world);
        norms.push_back(g.length_squared() > 1e-12f ? (-g).normalized() : Vector3(0, 1, 0));
        int mx = std::max(0, std::min(nx - 1, (int)Math::round((world.x - terrain_.origin.x) / cell)));
        int my = std::max(0, std::min(ny - 1, (int)Math::round((world.y - terrain_.origin.y) / cell)));
        int mz = std::max(0, std::min(nz - 1, (int)Math::round((world.z - terrain_.origin.z) / cell)));
        uint8_t mi = terrain_.m.size() == terrain_.d.size() ? terrain_.m[terrain_.at(mx, my, mz)] : 0;
        cols.push_back(water ? Color(1, 1, 1) : terrain_color(mi));
    }
    // A quad faces from the solid cell to the air cell. Godot's front faces are wound CLOCKWISE
    // as seen from the front, so a quad turned to have its right-hand normal outward goes in
    // reversed; the other way round, the whole ground draws from the inside. Shading normals are
    // accumulated from the faces, over the halo too, so a chunk's edge shades like its neighbour.
    std::vector<Vector3> acc(verts.size(), Vector3());
    auto quad = [&](int32_t a, int32_t b, int32_t c2, int32_t d2, const Vector3& outward, bool own) {
        if (a < 0 || b < 0 || c2 < 0 || d2 < 0) return;
        Vector3 fn = (verts[b] - verts[a]).cross(verts[c2] - verts[a]);
        if (fn.dot(outward) < 0) { std::swap(b, d2); fn = -fn; }
        acc[a] += fn; acc[b] += fn; acc[c2] += fn; acc[d2] += fn;
        if (!own) return;
        tris.push_back(a); tris.push_back(c2); tris.push_back(b);
        tris.push_back(a); tris.push_back(d2); tris.push_back(c2);
    };
    // Every edge in the halo for the normals; only this chunk's own edges go into the mesh.
    for (int z = hz0 + 1; z <= oz1 + 1; z++) for (int y = hy0 + 1; y <= oy1 + 1; y++) for (int x = hx0 + 1; x <= ox1 + 1; x++) {
        if (x < -1 || y < -1 || z < -1 || x > nx - 1 || y > ny - 1 || z > nz - 1) continue;
        const bool own = x >= ox0 && x <= ox1 && y >= oy0 && y <= oy1 && z >= oz0 && z <= oz1;
        const bool solid0 = D(x, y, z) >= kIso;
        const float out = solid0 ? 1.0f : -1.0f;
        if ((D(x + 1, y, z) >= kIso) != solid0)
            quad(cid(x, y, z), cid(x, y - 1, z), cid(x, y - 1, z - 1), cid(x, y, z - 1), Vector3(out, 0, 0), own);
        if ((D(x, y + 1, z) >= kIso) != solid0)
            quad(cid(x, y, z), cid(x, y, z - 1), cid(x - 1, y, z - 1), cid(x - 1, y, z), Vector3(0, out, 0), own);
        if ((D(x, y, z + 1) >= kIso) != solid0)
            quad(cid(x, y, z), cid(x - 1, y, z), cid(x - 1, y - 1, z), cid(x, y - 1, z), Vector3(0, 0, out), own);
    }
    for (int64_t i = 0; i < verts.size(); i++) if (acc[i].length_squared() > 1e-12f) norms[i] = acc[i].normalized();
    if (tris.size() < 3) {
        if (water) { if (c.water) { c.water->queue_free(); c.water = nullptr; } return; }
        if (c.mesh) { c.mesh->queue_free(); c.mesh = nullptr; }
        if (c.col) { c.col->queue_free(); c.col = nullptr; }
        return;
    }
    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = verts;
    arrays[Mesh::ARRAY_NORMAL] = norms;
    arrays[Mesh::ARRAY_COLOR] = cols;
    arrays[Mesh::ARRAY_INDEX] = tris;
    Ref<ArrayMesh> made;
    made.instantiate();
    made->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    if (water) {
        if (!c.water) {
            c.water = memnew(MeshInstance3D);
            c.water->set_material_override(terrain_.waterMat);
            c.water->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
            terrain_.body->add_child(c.water);
        }
        c.water->set_mesh(made);
        return;
    }
    if (!c.mesh) {
        c.mesh = memnew(MeshInstance3D);
        c.mesh->set_material_override(terrain_.mat);
        terrain_.body->add_child(c.mesh);
        c.col = memnew(CollisionShape3D);
        terrain_.body->add_child(c.col);
    }
    c.mesh->set_mesh(made);
    c.col->set_shape(made->create_trimesh_shape());     // the ground never moves: the exact triangles
}

Dictionary PulseBlockzWorld::get_terrain_stats() const {
    Dictionary out;
    int dirty = 0;
    for (const auto& c : terrain_.chunks) if (c.dirty) dirty++;
    out["chunks"] = (int64_t)terrain_.chunks.size();
    out["dirty"] = dirty;
    out["rebuilt_last"] = terrain_.rebuilt;
    int water = 0;
    for (const auto& c : terrain_.chunks) if (c.water) water++;
    out["water_chunks"] = water;
    return out;
}

// A PartOperation's MeshData: base64 over a little-endian blob --
//   "PBOP", u32 version, u32 vertices, u32 indices, then the vertices, then u32 indices.
// One surface, triangles; decodeOpMesh has each version's vertex layout.
static Ref<ArrayMesh> decodeOpMesh(const std::string& meshData);

Ref<ArrayMesh> PulseBlockzWorld::op_mesh_for(Part& p) {
    if (p.opMesh.is_valid() || p.opFailed || p.meshData.empty()) return p.opMesh;
    p.opFailed = true;                       // until it has been read successfully
    Ref<ArrayMesh> made = decodeOpMesh(p.meshData);
    if (made.is_null()) return Ref<ArrayMesh>();
    p.opColors = made->get_surface_count() > 0 && made->surface_get_arrays(0)[Mesh::ARRAY_COLOR].get_type() != Variant::NIL;
    p.opMesh = made;
    if (p.mesh) update_material(p);     // whether it has face colours is only known now
    p.opFailed = false;
    return p.opMesh;
}

// MeshData -> mesh. The one reader of the format; encodeOpMesh below is the one writer.
static Ref<ArrayMesh> decodeOpMesh(const std::string& meshData) {
    PackedByteArray raw = Marshalls::get_singleton()->base64_to_raw(String::utf8(meshData.c_str()));
    const int64_t head = 16;
    if (raw.size() < head) return Ref<ArrayMesh>();
    if (raw[0] != 'P' || raw[1] != 'B' || raw[2] != 'O' || raw[3] != 'P') return Ref<ArrayMesh>();
    // Version 1: position and normal per vertex (what Studio's Union writes). Version 2 adds the
    // face colour -- the colour each face kept from the part it came from. Version 3 adds the UV:
    // a MeshPart result's faces from the main part's mesh keep that mesh's, the rest are (0, 0).
    const uint32_t version = raw.decode_u32(4);
    if (version < 1 || version > 3) return Ref<ArrayMesh>();
    const int64_t stride = version == 3 ? 44 : version == 2 ? 36 : 24;
    const int64_t nv = (int64_t)raw.decode_u32(8);
    const int64_t ni = (int64_t)raw.decode_u32(12);
    if (nv < 3 || ni < 3 || ni % 3 != 0 || nv > 4000000 || ni > 12000000) return Ref<ArrayMesh>();
    if (raw.size() < head + nv * stride + ni * 4) return Ref<ArrayMesh>();
    PackedVector3Array verts, norms;
    PackedColorArray colors;
    PackedVector2Array uvs;
    PackedInt32Array tris;
    verts.resize(nv); norms.resize(nv); tris.resize(ni);
    if (version >= 2) colors.resize(nv);
    if (version >= 3) uvs.resize(nv);
    int64_t at = head;
    for (int64_t i = 0; i < nv; i++) {
        verts[i] = Vector3(raw.decode_float(at), raw.decode_float(at + 4), raw.decode_float(at + 8));
        norms[i] = Vector3(raw.decode_float(at + 12), raw.decode_float(at + 16), raw.decode_float(at + 20));
        if (version >= 2) colors[i] = Color(raw.decode_float(at + 24), raw.decode_float(at + 28), raw.decode_float(at + 32));
        if (version >= 3) uvs[i] = Vector2(raw.decode_float(at + 36), raw.decode_float(at + 40));
        at += stride;
    }
    for (int64_t i = 0; i < ni; i++) {
        int64_t k = (int64_t)raw.decode_u32(at);
        if (k < 0 || k >= nv) return Ref<ArrayMesh>();
        tris[i] = (int32_t)k;
        at += 4;
    }
    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = verts;
    arrays[Mesh::ARRAY_NORMAL] = norms;
    if (version >= 2) arrays[Mesh::ARRAY_COLOR] = colors;
    if (version >= 3) arrays[Mesh::ARRAY_TEX_UV] = uvs;
    arrays[Mesh::ARRAY_INDEX] = tris;
    Ref<ArrayMesh> made;
    made.instantiate();
    made->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    return made;
}

// A PartOperation as it is drawn: its geometry scaled to Size, one vertex per triangle corner,
// with
//  - normals smoothed across neighbouring faces of the same colour whose normals are within
//    SmoothingAngle of each other, since Roblox smooths across neither a colour nor a material
//    boundary; 0 is every face flat;
//  - box-mapped UVs, as Roblox applies a texture from one of -x, +x, -y, +y, -z, +z: a triangle
//    takes the direction its normal points most along, and its UV is where it lies on that face
//    of the part, 0..1 across it, oriented as a Decal on that face of a box is.
static int boxFaceOf(const Vector3& n) {   // 0 Right +X, 1 Left -X, 2 Top +Y, 3 Bottom -Y, 4 Back +Z, 5 Front -Z
    Vector3 a = n.abs();
    if (a.x >= a.y && a.x >= a.z) return n.x >= 0 ? 0 : 1;
    if (a.y >= a.z) return n.y >= 0 ? 2 : 3;
    return n.z >= 0 ? 4 : 5;
}
static Vector2 boxUV(int face, const Vector3& p, const Vector3& size) {
    // right and up of each face as seen from outside -- the same pair style_decal uses for a Decal
    static const Vector3 rights[6] = {Vector3(0, 0, -1), Vector3(0, 0, 1), Vector3(1, 0, 0), Vector3(-1, 0, 0), Vector3(1, 0, 0), Vector3(-1, 0, 0)};
    static const Vector3 ups[6] = {Vector3(0, 1, 0), Vector3(0, 1, 0), Vector3(0, 0, -1), Vector3(0, 0, -1), Vector3(0, 1, 0), Vector3(0, 1, 0)};
    const Vector3 r = rights[face], u = ups[face];
    const float w = std::max(std::abs(r.dot(size)), 1e-4f), h = std::max(std::abs(u.dot(size)), 1e-4f);
    return Vector2(p.dot(r) / w + 0.5f, 0.5f - p.dot(u) / h);
}
Ref<ArrayMesh> PulseBlockzWorld::union_drawn(const Ref<ArrayMesh>& decoded, const Vector3& scale, const Vector3& size, float smoothingDegrees, bool smooth) {
    if (decoded.is_null() || decoded->get_surface_count() == 0) return Ref<ArrayMesh>();
    Array src = decoded->surface_get_arrays(0);
    PackedVector3Array v = src[Mesh::ARRAY_VERTEX];
    PackedColorArray col;
    if (src[Mesh::ARRAY_COLOR].get_type() == Variant::PACKED_COLOR_ARRAY) col = src[Mesh::ARRAY_COLOR];
    PackedVector3Array srcN;
    if (src[Mesh::ARRAY_NORMAL].get_type() == Variant::PACKED_VECTOR3_ARRAY) srcN = src[Mesh::ARRAY_NORMAL];
    PackedVector2Array srcUV;
    if (src[Mesh::ARRAY_TEX_UV].get_type() == Variant::PACKED_VECTOR2_ARRAY) srcUV = src[Mesh::ARRAY_TEX_UV];
    PackedInt32Array idx;
    if (src[Mesh::ARRAY_INDEX].get_type() == Variant::PACKED_INT32_ARRAY) idx = src[Mesh::ARRAY_INDEX];
    const int64_t corners = idx.size() ? idx.size() : v.size();
    const int64_t tris = corners / 3;
    PackedVector3Array pos, nrm; PackedColorArray colors; PackedVector2Array uv;
    pos.resize(tris * 3); nrm.resize(tris * 3); uv.resize(tris * 3);
    if (col.size()) colors.resize(tris * 3);
    std::vector<Vector3> faceN((size_t)tris);
    for (int64_t t = 0; t < tris; t++) {
        for (int k = 0; k < 3; k++) {
            int32_t i = idx.size() ? idx[t * 3 + k] : (int32_t)(t * 3 + k);
            pos[t * 3 + k] = v[i] * scale;
            if (col.size()) colors[t * 3 + k] = col[i];
        }
        // Godot's front is clockwise: the face normal comes out of the winding that way round
        Vector3 fn = (pos[t * 3 + 2] - pos[t * 3]).cross(pos[t * 3 + 1] - pos[t * 3]);
        faceN[(size_t)t] = fn;
    }
    if (smooth) {
        // corners that meet at one point with one colour, and which faces are there
        std::unordered_map<std::string, std::vector<int64_t>> at;
        auto key = [&](int64_t c) {
            const Vector3 p = pos[c];
            const Color cc = col.size() ? colors[c] : Color(1, 1, 1);
            char k[160];
            std::snprintf(k, sizeof k, "%lld,%lld,%lld/%d,%d,%d", (long long)std::llround(p.x / 1e-4f), (long long)std::llround(p.y / 1e-4f), (long long)std::llround(p.z / 1e-4f),
                          (int)std::lround(cc.r * 255), (int)std::lround(cc.g * 255), (int)std::lround(cc.b * 255));
            return std::string(k);
        };
        for (int64_t c = 0; c < tris * 3; c++) at[key(c)].push_back(c);
        const float cosLimit = std::cos(std::clamp(smoothingDegrees, 0.0f, 180.0f) * (float)Math_PI / 180.0f);
        for (int64_t c = 0; c < tris * 3; c++) {
            const Vector3 mine = faceN[(size_t)(c / 3)];
            const Vector3 unitMine = mine.normalized();
            Vector3 sum;
            for (int64_t o : at[key(c)]) {
                const Vector3 other = faceN[(size_t)(o / 3)];
                if (o / 3 == c / 3 || (smoothingDegrees > 0 && unitMine.dot(other.normalized()) >= cosLimit - 1e-6f)) sum += other;
            }
            nrm[c] = sum.length_squared() > 1e-20f ? sum.normalized() : unitMine;
        }
    } else {
        // a MeshPart's own normals, as the operation left them
        for (int64_t t = 0; t < tris; t++) for (int k = 0; k < 3; k++) {
            int32_t i = idx.size() ? idx[t * 3 + k] : (int32_t)(t * 3 + k);
            nrm[t * 3 + k] = i < srcN.size() ? srcN[i] : faceN[(size_t)t].normalized();
        }
    }
    for (int64_t t = 0; t < tris; t++) {
        const int face = boxFaceOf(faceN[(size_t)t]);
        for (int k = 0; k < 3; k++) {
            int32_t i = idx.size() ? idx[t * 3 + k] : (int32_t)(t * 3 + k);
            uv[t * 3 + k] = smooth ? boxUV(face, pos[t * 3 + k], size) : (i < srcUV.size() ? srcUV[i] : Vector2());
        }
    }
    Array arrays; arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = pos; arrays[Mesh::ARRAY_NORMAL] = nrm; arrays[Mesh::ARRAY_TEX_UV] = uv;
    if (col.size()) arrays[Mesh::ARRAY_COLOR] = colors;
    Ref<ArrayMesh> out; out.instantiate();
    out->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    return out;
}

// A Decal on a union: the triangles facing the decal's Face, lifted a hair off them, keeping the
// box UVs from that direction.
static Ref<ArrayMesh> unionDecalMesh(const Ref<ArrayMesh>& drawn, int face) {
    if (drawn.is_null() || drawn->get_surface_count() == 0) return Ref<ArrayMesh>();
    Array src = drawn->surface_get_arrays(0);
    PackedVector3Array v = src[Mesh::ARRAY_VERTEX];
    PackedVector2Array uv = src[Mesh::ARRAY_TEX_UV];
    static const Vector3 outs[6] = {Vector3(1, 0, 0), Vector3(-1, 0, 0), Vector3(0, 1, 0), Vector3(0, -1, 0), Vector3(0, 0, 1), Vector3(0, 0, -1)};
    PackedVector3Array pos, nrm; PackedVector2Array tuv;
    for (int64_t t = 0; t + 2 < v.size(); t += 3) {
        Vector3 fn = (v[t + 2] - v[t]).cross(v[t + 1] - v[t]);
        if (boxFaceOf(fn) != face) continue;
        for (int k = 0; k < 3; k++) { pos.push_back(v[t + k] + outs[face] * 0.01f); nrm.push_back(outs[face]); tuv.push_back(uv[t + k]); }
    }
    if (pos.size() < 3) return Ref<ArrayMesh>();
    Array arrays; arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = pos; arrays[Mesh::ARRAY_NORMAL] = nrm; arrays[Mesh::ARRAY_TEX_UV] = tuv;
    Ref<ArrayMesh> out; out.instantiate();
    out->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    return out;
}

// meshoptimizer's simplifier over positions only, with the border locked. `target` is an index
// count, `threshold` the error allowed relative to the mesh's size. Empty when it cannot run.
static PackedInt32Array simplifyIndices(const PackedVector3Array& verts, const int32_t* indices, int64_t indexCount, int32_t target, float threshold) {
    PackedInt32Array out;
    if (verts.is_empty() || indexCount < 3 || indexCount % 3 != 0 || target < 0 || target > indexCount) return out;
    std::vector<float> positions((size_t)verts.size() * 3);
    for (int64_t k = 0; k < verts.size(); k++) {
        positions[(size_t)k * 3] = verts[k].x; positions[(size_t)k * 3 + 1] = verts[k].y; positions[(size_t)k * 3 + 2] = verts[k].z;
    }
    out.resize(indexCount);
    float error = 0.0f;
    size_t kept = meshopt_simplify((unsigned int*)out.ptrw(), (const unsigned int*)indices, (size_t)indexCount, positions.data(),
                                   (size_t)verts.size(), sizeof(float) * 3, (size_t)target, threshold, meshopt_SimplifyLockBorder, &error);
    out.resize((int64_t)kept);
    return out;
}

// ---- RenderFidelity: three levels of detail, chosen by distance ----------------------------------
// Under 250 studs Highest, 250 to 500 Medium, 500 and up Lowest. Precise is the highest at any
// distance; Performance (a MeshPart's) is the lowest.
static Ref<Mesh> simplifiedMesh(const Ref<Mesh>& mesh, float keep) {
    if (mesh.is_null() || mesh->get_surface_count() == 0) return mesh;
    Ref<ArrayMesh> out; out.instantiate();
    bool reduced = false;
    for (int s = 0; s < mesh->get_surface_count(); s++) {
        Ref<SurfaceTool> st; st.instantiate();
        st->create_from(mesh, s);
        st->index();
        Array arrays = st->commit_to_arrays();
        PackedInt32Array idx = arrays[Mesh::ARRAY_INDEX];
        const int32_t target = std::max<int32_t>(12, (int32_t)(idx.size() * keep) / 3 * 3);
        if (idx.size() > 3 * 40 && target < idx.size()) {
            PackedVector3Array verts = arrays[Mesh::ARRAY_VERTEX];
            PackedInt32Array lod = simplifyIndices(verts, idx.ptr(), idx.size(), target, 1e10f);   // error allowance wide open: the count decides
            if (lod.size() >= 3 && lod.size() < idx.size()) { arrays[Mesh::ARRAY_INDEX] = lod; reduced = true; }
        }
        out->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    }
    return reduced ? Ref<Mesh>(out) : mesh;
}
// A union's own geometry, simplified: corners are welded by position and colour first, since as
// drawn every triangle has its own corners and a simplifier can collapse nothing unshared.
static Ref<ArrayMesh> simplifiedDecoded(const Ref<ArrayMesh>& decoded, float keep) {
    if (decoded.is_null() || decoded->get_surface_count() == 0) return decoded;
    Array src = decoded->surface_get_arrays(0);
    PackedVector3Array v = src[Mesh::ARRAY_VERTEX];
    PackedColorArray col;
    if (src[Mesh::ARRAY_COLOR].get_type() == Variant::PACKED_COLOR_ARRAY) col = src[Mesh::ARRAY_COLOR];
    PackedVector2Array uv;
    if (src[Mesh::ARRAY_TEX_UV].get_type() == Variant::PACKED_VECTOR2_ARRAY) uv = src[Mesh::ARRAY_TEX_UV];
    PackedInt32Array idx;
    if (src[Mesh::ARRAY_INDEX].get_type() == Variant::PACKED_INT32_ARRAY) idx = src[Mesh::ARRAY_INDEX];
    const int64_t corners = idx.size() ? idx.size() : v.size();
    if (corners / 3 <= 40) return decoded;
    std::unordered_map<std::string, int32_t> at;
    PackedVector3Array wv; PackedColorArray wc; PackedVector2Array wu; std::vector<int32_t> wi;
    for (int64_t k = 0; k < corners; k++) {
        const int32_t i = idx.size() ? idx[k] : (int32_t)k;
        const Color c = col.size() ? col[i] : Color(1, 1, 1);
        char key[160];
        std::snprintf(key, sizeof key, "%lld,%lld,%lld/%d,%d,%d", (long long)std::llround(v[i].x / 1e-4f), (long long)std::llround(v[i].y / 1e-4f), (long long)std::llround(v[i].z / 1e-4f),
                      (int)std::lround(c.r * 255), (int)std::lround(c.g * 255), (int)std::lround(c.b * 255));
        auto it = at.find(key);
        if (it == at.end()) {
            it = at.emplace(key, (int32_t)wv.size()).first;
            wv.push_back(v[i]); wc.push_back(c); wu.push_back(uv.size() ? uv[i] : Vector2());
        }
        wi.push_back(it->second);
    }
    const int32_t target = std::max<int32_t>(12, (int32_t)((int64_t)wi.size() * keep) / 3 * 3);
    PackedInt32Array lod = simplifyIndices(wv, wi.data(), (int64_t)wi.size(), target, 1e10f);
    if (lod.size() < 3 || lod.size() >= (int64_t)wi.size()) return decoded;
    Array arrays; arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = wv; arrays[Mesh::ARRAY_INDEX] = lod; arrays[Mesh::ARRAY_TEX_UV] = wu;
    if (col.size()) arrays[Mesh::ARRAY_COLOR] = wc;
    Ref<ArrayMesh> out; out.instantiate();
    out->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    return out;
}
// Only the mesh as drawn. A coarser band is meshoptimizer over the whole mesh on the main
// thread, so each is built the first time a part is far enough to need it (ensure_lod).
void PulseBlockzWorld::build_lods(Part& p, const Ref<Mesh>& drawn, const Ref<ArrayMesh>& decoded, const Vector3& scale, bool unionSmoothing) {
    p.lods[0] = drawn;
    p.lods[1] = Ref<Mesh>(); p.lods[2] = Ref<Mesh>();
    p.lodDecoded = decoded; p.lodScale = scale; p.lodSmoothing = unionSmoothing;
    p.lodBand = -1;
}
void PulseBlockzWorld::ensure_lod(Part& p, int band) {
    if (band <= 0 || band > 2 || p.lods[band].is_valid() || p.lods[0].is_null()) return;
    if (p.renderFidelity == "Precise") { p.lods[band] = p.lods[0]; return; }
    const float keep = band == 1 ? 0.5f : 0.25f;
    if (p.lodDecoded.is_valid()) p.lods[band] = union_drawn(simplifiedDecoded(p.lodDecoded, keep), p.lodScale, toGd(p.size), p.smoothingAngle, p.lodSmoothing);
    else p.lods[band] = simplifiedMesh(p.lods[0], keep);
}
int PulseBlockzWorld::lod_band_for(const Part& p) const {
    if (p.renderFidelity == "Precise") return 0;
    if (p.renderFidelity == "Performance") return 2;
    Viewport* vp = get_viewport();
    Camera3D* cam = vp ? vp->get_camera_3d() : nullptr;
    if (!cam || !p.mesh || !p.mesh->is_inside_tree()) return 0;
    const float d = cam->get_global_position().distance_to(p.mesh->get_global_position());
    return d < 250.0f ? 0 : d < 500.0f ? 1 : 2;
}
void PulseBlockzWorld::update_lods() {
    for (auto& [id, p] : parts_) {
        if (!p.mesh || p.lods[0].is_null()) continue;
        const int band = lod_band_for(p);
        if (band == p.lodBand) continue;
        ensure_lod(p, band);
        p.lodBand = band;
        p.mesh->set_mesh(p.lods[band].is_valid() ? p.lods[band] : p.lods[0]);
        if (!highlights_.empty()) highlightsDirty_ = true;
    }
}

// ---- CollisionFidelity ---------------------------------------------------------------------------
// Box is the mesh's bounding box, Hull its convex hull, Default a coarse voxel convex-hull
// decomposition, PreciseConvexDecomposition a fine one. Tunable's tuning is engine data nothing
// documents (rbx-dom lists it as PhysicalConfigData, not scriptable), so it decomposes between
// Default and Precise. The decompositions are V-HACD, run once per mesh and fidelity and kept.
std::vector<Ref<Shape3D>> PulseBlockzWorld::collision_shapes(const std::string& key, const Ref<Mesh>& source, const Transform3D& fit, const std::string& fidelity) {
    std::vector<Ref<Shape3D>> out;
    if (source.is_null() || source->get_surface_count() == 0) return out;
    if (fidelity == "Box") {
        AABB box = fit.xform(source->get_aabb());
        PackedVector3Array corners;
        for (int k = 0; k < 8; k++) corners.push_back(box.get_endpoint(k));
        Ref<ConvexPolygonShape3D> sh; sh.instantiate(); sh->set_points(corners);
        out.push_back(sh);
        return out;
    }
    auto hullsOf = [&](const Ref<Mesh>& m, bool decompose) {
        std::vector<PackedVector3Array> hulls;
        if (!decompose) {
            Ref<ConvexPolygonShape3D> h = m->create_convex_shape(true, false);
            if (h.is_valid()) hulls.push_back(h->get_points());
            return hulls;
        }
        Ref<MeshConvexDecompositionSettings> settings; settings.instantiate();
        settings->set_mode(MeshConvexDecompositionSettings::CONVEX_DECOMPOSITION_MODE_VOXEL);
        if (fidelity == "PreciseConvexDecomposition") { settings->set_resolution(100000); settings->set_max_concavity(0.0015f); settings->set_max_convex_hulls(64); settings->set_max_num_vertices_per_convex_hull(64); }
        else if (fidelity == "Tunable") { settings->set_resolution(40000); settings->set_max_concavity(0.005f); settings->set_max_convex_hulls(32); settings->set_max_num_vertices_per_convex_hull(48); }
        else { settings->set_resolution(10000); settings->set_max_concavity(0.02f); settings->set_max_convex_hulls(16); settings->set_max_num_vertices_per_convex_hull(32); }
        MeshInstance3D* probe = memnew(MeshInstance3D);
        probe->set_mesh(m);
        probe->create_multiple_convex_collisions(settings);
        for (int c = 0; c < probe->get_child_count(); c++) {
            Node* body = probe->get_child(c);
            for (int s = 0; s < body->get_child_count(); s++)
                if (auto* cs = Object::cast_to<CollisionShape3D>(body->get_child(s))) {
                    Ref<ConvexPolygonShape3D> h = cs->get_shape();
                    if (h.is_null()) continue;
                    PackedVector3Array pts = h->get_points();
                    const Transform3D t = cs->get_transform();
                    for (int64_t k = 0; k < pts.size(); k++) pts[k] = t.xform(pts[k]);
                    hulls.push_back(pts);
                }
        }
        memdelete(probe);
        if (hulls.empty()) { Ref<ConvexPolygonShape3D> h = m->create_convex_shape(true, false); if (h.is_valid()) hulls.push_back(h->get_points()); }
        return hulls;
    };
    const std::string cacheKey = key + "#" + fidelity;
    auto it = hullCache_.find(cacheKey);
    if (it == hullCache_.end() && fidelity == "Hull") it = hullCache_.emplace(cacheKey, hullsOf(source, false)).first;
    if (it == hullCache_.end()) {
        // V-HACD over the whole mesh takes a quarter of a second for a car chassis, three
        // quarters for a tree's canopy, so it runs on a thread of its own; the part wears the
        // mesh's single convex hull until the real ones land (poll_hull_jobs).
        if (!hullJobs_.count(cacheKey)) {
            auto job = std::make_unique<HullJob>();
            job->key = cacheKey; job->mesh = source; job->fidelity = fidelity;
            HullJob* raw = job.get();
            job->thread = std::thread([raw] {
                Ref<MeshConvexDecompositionSettings> settings; settings.instantiate();
                settings->set_mode(MeshConvexDecompositionSettings::CONVEX_DECOMPOSITION_MODE_VOXEL);
                if (raw->fidelity == "PreciseConvexDecomposition") { settings->set_resolution(100000); settings->set_max_concavity(0.0015f); settings->set_max_convex_hulls(64); settings->set_max_num_vertices_per_convex_hull(64); }
                else if (raw->fidelity == "Tunable") { settings->set_resolution(40000); settings->set_max_concavity(0.005f); settings->set_max_convex_hulls(32); settings->set_max_num_vertices_per_convex_hull(48); }
                else { settings->set_resolution(10000); settings->set_max_concavity(0.02f); settings->set_max_convex_hulls(16); settings->set_max_num_vertices_per_convex_hull(32); }
                // Through a probe MeshInstance3D, never put in the tree: the decomposition is not
                // in the extension API and this is the only door to it.
                MeshInstance3D* probe = memnew(MeshInstance3D);
                probe->set_mesh(raw->mesh);
                probe->create_multiple_convex_collisions(settings);
                for (int c = 0; c < probe->get_child_count(); c++) {
                    Node* body = probe->get_child(c);
                    for (int s = 0; s < body->get_child_count(); s++)
                        if (auto* cs = Object::cast_to<CollisionShape3D>(body->get_child(s))) {
                            Ref<ConvexPolygonShape3D> h = cs->get_shape();
                            if (h.is_null()) continue;
                            PackedVector3Array pts = h->get_points();
                            const Transform3D t = cs->get_transform();
                            for (int64_t k = 0; k < pts.size(); k++) pts[k] = t.xform(pts[k]);
                            if (pts.size() >= 4) raw->hulls.push_back(pts);
                        }
                }
                memdelete(probe);
                raw->done.store(true);
            });
            hullJobs_.emplace(cacheKey, std::move(job));
        }
        std::vector<PackedVector3Array> one;
        Ref<ConvexPolygonShape3D> quick = source->create_convex_shape(true, false);
        if (quick.is_valid()) one.push_back(quick->get_points());
        for (const PackedVector3Array& hull : one) {
            PackedVector3Array pts = hull;
            for (int64_t k = 0; k < pts.size(); k++) pts[k] = fit.xform(pts[k]);
            Ref<ConvexPolygonShape3D> sh; sh.instantiate(); sh->set_points(pts);
            out.push_back(sh);
        }
        return out;
    }
    for (const PackedVector3Array& hull : it->second) {
        PackedVector3Array pts = hull;
        for (int64_t k = 0; k < pts.size(); k++) pts[k] = fit.xform(pts[k]);   // a linear map of a convex hull is still one
        Ref<ConvexPolygonShape3D> sh; sh.instantiate(); sh->set_points(pts);
        out.push_back(sh);
    }
    return out;
}
// The part's shapes: the first on its own CollisionShape3D, the rest on siblings made for them.
void PulseBlockzWorld::set_collision_shapes(Part& p, const std::vector<Ref<Shape3D>>& shapes, const Transform3D& local) {
    if (!p.col) return;
    const size_t extra = shapes.size() > 1 ? shapes.size() - 1 : 0;
    while (p.extraCols.size() > extra) {
        CollisionShape3D* x = p.extraCols.back();
        shapeToId_.erase(x->get_instance_id());
        x->queue_free();
        p.extraCols.pop_back();
    }
    Node* parent = p.col->get_parent();
    while (p.extraCols.size() < extra && parent) {
        CollisionShape3D* x = memnew(CollisionShape3D);
        x->set_disabled(p.col->is_disabled());
        parent->add_child(x);
        shapeToId_[x->get_instance_id()] = p.id;
        p.extraCols.push_back(x);
    }
    const Transform3D where = p.role == ROLE_LIMB ? p.offset * local : local;
    p.col->set_shape(shapes.empty() ? Ref<Shape3D>() : shapes[0]);
    p.col->set_transform(where);
    for (size_t k = 0; k < p.extraCols.size(); k++) { p.extraCols[k]->set_shape(shapes[k + 1]); p.extraCols[k]->set_transform(where); }
}

// A welded, indexed surface: one entry per distinct vertex, and the triangles over them.
struct OpSurface {
    std::vector<Vector3> v, n;
    std::vector<Color> c;
    std::vector<Vector2> uv;
    std::vector<int32_t> i;
};

// Corners agreeing in position, normal and face colour become one vertex: CSG hands back one
// vertex per triangle corner, several times the bytes a flat-faced solid needs.
static OpSurface weldOpSurface(const PackedVector3Array& verts, const PackedVector3Array& norms,
                               const PackedColorArray& cols, const PackedVector2Array& uvs, const std::vector<int32_t>& tris) {
    OpSurface out;
    std::unordered_map<std::string, int32_t> shared;
    out.i.reserve(tris.size());
    auto q = [](float x, float step) { return (long long)std::llround(x / step); };
    for (int32_t k : tris) {
        const Vector3 v = verts[k];
        const Vector3 n = k < norms.size() ? norms[k] : Vector3(0, 1, 0);
        const Color c = k < cols.size() ? cols[k] : Color(1, 1, 1);
        const Vector2 uv = k < uvs.size() ? uvs[k] : Vector2();
        char key[260];
        std::snprintf(key, sizeof key, "%lld,%lld,%lld/%lld,%lld,%lld/%lld,%lld,%lld/%lld,%lld", q(v.x, 1e-4f), q(v.y, 1e-4f), q(v.z, 1e-4f),
                      q(n.x, 1e-3f), q(n.y, 1e-3f), q(n.z, 1e-3f), q(c.r, 1e-3f), q(c.g, 1e-3f), q(c.b, 1e-3f), q(uv.x, 1e-4f), q(uv.y, 1e-4f));
        auto it = shared.find(key);
        if (it == shared.end()) {
            it = shared.emplace(key, (int32_t)out.v.size()).first;
            out.v.push_back(v);
            out.n.push_back(n);
            out.c.push_back(c);
            out.uv.push_back(uv);
        }
        out.i.push_back(it->second);
    }
    return out;
}

// Roblox caps a solid-modelling result at 20,000 triangles and errors if it cannot be simplified
// to that. The simplifier runs over the indexed surface, loosening the error until it fits.
static bool capOpSurface(OpSurface& s) {
    const int64_t cap = 20000 * 3;
    if ((int64_t)s.i.size() <= cap) return true;
    PackedVector3Array verts;
    verts.resize((int64_t)s.v.size());
    for (size_t k = 0; k < s.v.size(); k++) verts.set((int64_t)k, s.v[k]);
    for (float threshold : {0.01f, 0.1f, 1.0f, 10.0f, 100.0f, 10000.0f}) {
        PackedInt32Array lod = simplifyIndices(verts, s.i.data(), (int64_t)s.i.size(), (int32_t)cap, threshold);
        if (lod.size() >= 3 && lod.size() <= cap) {
            s.i.assign(lod.ptr(), lod.ptr() + lod.size());
            return true;
        }
    }
    return false;
}

// An indexed surface -> MeshData, version 3: position, normal, face colour and UV per vertex.
// Recentred on its own bounds when `recentre` (BasePart's methods: the part sits on its body),
// else kept in the main part's space (GeometryService). `middle` is the bounds' centre, `span`
// the bounds.
static std::string encodeOpMesh(const OpSurface& s, bool recentre, Vector3& middle, Vector3& span) {
    if (s.i.size() < 3) return std::string();
    Vector3 low = s.v[s.i[0]], high = low;
    for (int32_t k : s.i) { low = low.min(s.v[k]); high = high.max(s.v[k]); }
    middle = (low + high) * 0.5f;
    span = (high - low).max(Vector3(0.05f, 0.05f, 0.05f));
    const Vector3 shift = recentre ? middle : Vector3();
    // Only the vertices the triangles still use, which after a simplification is fewer.
    std::vector<int32_t> remap(s.v.size(), -1);
    std::vector<int32_t> used;
    for (int32_t k : s.i) if (remap[k] < 0) { remap[k] = (int32_t)used.size(); used.push_back(k); }
    PackedByteArray raw;
    raw.resize(16 + (int64_t)used.size() * 44 + (int64_t)s.i.size() * 4);
    raw[0] = 'P'; raw[1] = 'B'; raw[2] = 'O'; raw[3] = 'P';
    raw.encode_u32(4, 3);
    raw.encode_u32(8, (uint32_t)used.size());
    raw.encode_u32(12, (uint32_t)s.i.size());
    int64_t at = 16;
    for (int32_t k : used) {
        const Vector3 v = s.v[k] - shift;
        raw.encode_float(at, v.x); raw.encode_float(at + 4, v.y); raw.encode_float(at + 8, v.z);
        raw.encode_float(at + 12, s.n[k].x); raw.encode_float(at + 16, s.n[k].y); raw.encode_float(at + 20, s.n[k].z);
        raw.encode_float(at + 24, s.c[k].r); raw.encode_float(at + 28, s.c[k].g); raw.encode_float(at + 32, s.c[k].b);
        const Vector2 uv = (size_t)k < s.uv.size() ? s.uv[k] : Vector2();
        raw.encode_float(at + 36, uv.x); raw.encode_float(at + 40, uv.y);
        at += 44;
    }
    for (int32_t k : s.i) { raw.encode_u32(at, (uint32_t)remap[k]); at += 4; }
    return toStd(Marshalls::get_singleton()->raw_to_base64(raw));
}

// ---- solid modelling ---------------------------------------------------------------------
// A script's UnionAsync / SubtractAsync / IntersectAsync, done here where the geometry is.

// One operand's shape in its own space, Size baked in: the same shape update_shape draws, so a
// union cuts what was on the screen.
Ref<ArrayMesh> PulseBlockzWorld::solid_operand_mesh(const Runtime::SolidOperand& o, bool keepUVs) {
    const Vector3 size = toGd(o.size);
    Ref<Mesh> base;
    Transform3D xf;
    // A file mesh is recentred on import; a union's or a made MeshPart's own geometry is drawn
    // about the part's origin (see update_shape), so it is scaled about that and not moved.
    auto fitted = [&](const Ref<Mesh>& m, bool recentre) {
        AABB box = m->get_aabb();
        Vector3 sc(box.size.x > 1e-6f ? size.x / box.size.x : 1, box.size.y > 1e-6f ? size.y / box.size.y : 1,
                   box.size.z > 1e-6f ? size.z / box.size.z : 1);
        base = m;
        xf = Transform3D(Basis().scaled(sc), recentre ? -(box.get_center() * sc) : Vector3());
    };
    const ClassDef* cls = findClass(o.className);
    if (o.className == "MeshPart" && o.meshData.empty()) {
        Ref<Mesh> file = load_mesh(o.meshId);
        if (file.is_null()) return Ref<ArrayMesh>();
        fitted(file, true);
    } else if (!o.meshData.empty() && cls && (cls->isA("PartOperation") || o.className == "MeshPart")) {
        Ref<ArrayMesh> own = decodeOpMesh(o.meshData);
        if (own.is_null()) return Ref<ArrayMesh>();
        fitted(own, false);
    } else {
        const std::string s = o.className == "WedgePart" ? std::string("Wedge") : o.className == "Part" ? o.shape : std::string("Block");
        if (s == "Ball") {
            const float r = std::min(std::min(size.x, size.y), size.z) * 0.5f;
            Ref<SphereMesh> m; m.instantiate(); m->set_radius(r); m->set_height(2 * r); base = m;
        } else if (s == "Cylinder") {
            const float r = std::min(size.y, size.z) * 0.5f;
            Ref<CylinderMesh> m; m.instantiate(); m->set_top_radius(r); m->set_bottom_radius(r); m->set_height(size.x); base = m;
            xf = Transform3D(Basis(Vector3(0, 0, 1), -Math_PI / 2), Vector3());   // Y axis -> X axis
        } else if (s == "Wedge") {
            Ref<PrismMesh> m; m.instantiate(); m->set_left_to_right(0); m->set_size(Vector3(size.z, size.y, size.x)); base = m;
            xf = Transform3D(Basis(Vector3(0, 1, 0), Math_PI / 2), Vector3());
        } else {
            Ref<BoxMesh> m; m.instantiate(); m->set_size(size); base = m;
        }
    }
    if (base.is_null()) return Ref<ArrayMesh>();
    Ref<ArrayMesh> out;
    out.instantiate();
    bake_mesh(out, base, xf);
    // Roblox keeps the main part's mesh UVs and gives faces from the other parts (0, 0). A Part
    // is not a mesh and has none to give either.
    const bool meshUVs = keepUVs && o.className == "MeshPart";
    Ref<ArrayMesh> uvd; uvd.instantiate();
    for (int s = 0; s < out->get_surface_count(); s++) {
        Array arrays = out->surface_get_arrays(s);
        PackedVector3Array v = arrays[Mesh::ARRAY_VERTEX];
        PackedVector2Array uv;
        if (meshUVs && arrays[Mesh::ARRAY_TEX_UV].get_type() == Variant::PACKED_VECTOR2_ARRAY) uv = arrays[Mesh::ARRAY_TEX_UV];
        if (uv.size() != v.size()) { uv.resize(v.size()); for (int64_t k = 0; k < uv.size(); k++) uv[k] = Vector2(); }
        arrays[Mesh::ARRAY_TEX_UV] = uv;
        uvd->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    }
    return uvd;
}

void PulseBlockzWorld::start_solid(const SolidAsk& ask, bool server) {
    SolidJob job;
    job.id = ask.id;
    job.server = server;
    job.split = ask.request.splitApart;
    job.keepOrigin = ask.request.keepOrigin;
    const auto& ops = ask.request.operands;
    if (ops.empty()) { job.error = "The solid modeling operation had nothing to operate on"; solidJobs_.push_back(job); return; }
    // Everything is placed relative to the main part, so the result comes out in its frame.
    job.frame = toTransform(ops[0].pos, ops[0].orient);
    const Transform3D inv = job.frame.affine_inverse();
    CSGCombiner3D* root = memnew(CSGCombiner3D);
    root->set_visible(false);
    std::string missing;
    auto add = [&](const Runtime::SolidOperand& o, CSGShape3D::Operation op) {
        Ref<ArrayMesh> m = solid_operand_mesh(o, &o == &ops[0]);
        if (m.is_null() || m->get_surface_count() == 0) { if (missing.empty()) missing = o.className; return; }
        CSGMesh3D* n = memnew(CSGMesh3D);
        n->set_mesh(m);
        n->set_transform(inv * toTransform(o.pos, o.orient));
        n->set_operation(op);
        // The colour rides on a material of its own, which CSG keeps per face, so each face of
        // the result keeps the colour of the part it came from, the faces a cut leaves included.
        Ref<StandardMaterial3D> tag;
        tag.instantiate();
        tag->set_albedo(Color(o.color.r, o.color.g, o.color.b));
        n->set_material(tag);
        root->add_child(n);
    };
    add(ops[0], CSGShape3D::OPERATION_UNION);
    if (ask.request.op == Runtime::SolidRequest::Intersect) {
        for (size_t k = 1; k < ops.size(); k++) add(ops[k], CSGShape3D::OPERATION_INTERSECTION);
    } else {
        // Every addition before any cut: a hole made first would be filled in by a later part.
        for (size_t k = 1; k < ops.size(); k++) if (!ops[k].subtract) add(ops[k], CSGShape3D::OPERATION_UNION);
        for (size_t k = 1; k < ops.size(); k++) if (ops[k].subtract) add(ops[k], CSGShape3D::OPERATION_SUBTRACTION);
    }
    if (!missing.empty()) {
        memdelete(root);
        job.error = "A " + missing + " in the operation has no shape to cut with";
        solidJobs_.push_back(job);
        return;
    }
    add_child(root);
    job.root = root;
    solidJobs_.push_back(job);
}

// A mesh as an EditableMesh's data: each vertex of the file a vertex, with its normal, UV and
// colour as attributes of its own, the triangles turned from Godot's clockwise front to Roblox's
// counterclockwise one.
std::string PulseBlockzWorld::mesh_to_editable(const Ref<Mesh>& mesh) {
    EditableMeshData m;
    for (int s = 0; s < mesh->get_surface_count(); s++) {
        Array arrays = mesh->surface_get_arrays(s);
        if (arrays.size() < Mesh::ARRAY_MAX) continue;
        PackedVector3Array v = arrays[Mesh::ARRAY_VERTEX];
        PackedVector3Array n = arrays[Mesh::ARRAY_NORMAL].get_type() == Variant::PACKED_VECTOR3_ARRAY ? PackedVector3Array(arrays[Mesh::ARRAY_NORMAL]) : PackedVector3Array();
        PackedVector2Array uv = arrays[Mesh::ARRAY_TEX_UV].get_type() == Variant::PACKED_VECTOR2_ARRAY ? PackedVector2Array(arrays[Mesh::ARRAY_TEX_UV]) : PackedVector2Array();
        PackedColorArray col = arrays[Mesh::ARRAY_COLOR].get_type() == Variant::PACKED_COLOR_ARRAY ? PackedColorArray(arrays[Mesh::ARRAY_COLOR]) : PackedColorArray();
        uint32_t base = (uint32_t)m.verts.size();
        for (int64_t k = 0; k < v.size(); k++) {
            EditableMeshData::V vx; vx.p = fromGd(v[k]); m.verts.push_back(vx);
            EditableMeshData::N nx; if (k < n.size()) { nx.n = fromGd(n[k]); nx.autoCalc = false; } m.normals.push_back(nx);
            EditableMeshData::U ux; if (k < uv.size()) { ux.u = uv[k].x; ux.v = uv[k].y; } m.uvs.push_back(ux);
            EditableMeshData::C cx; if (k < col.size()) { cx.c = {col[k].r, col[k].g, col[k].b}; cx.a = col[k].a; } m.colors.push_back(cx);
        }
        PackedInt32Array idx = arrays[Mesh::ARRAY_INDEX].get_type() == Variant::PACKED_INT32_ARRAY ? PackedInt32Array(arrays[Mesh::ARRAY_INDEX]) : PackedInt32Array();
        int64_t count = idx.size() ? idx.size() : v.size();
        for (int64_t k = 0; k + 2 < count; k += 3) {
            uint32_t a = base + (uint32_t)(idx.size() ? idx[k] : k), b = base + (uint32_t)(idx.size() ? idx[k + 1] : k + 1), c = base + (uint32_t)(idx.size() ? idx[k + 2] : k + 2);
            EditableMeshData::F f;
            const uint32_t order[3] = {a, c, b};
            for (int q = 0; q < 3; q++) { f.v[q] = f.n[q] = f.u[q] = f.c[q] = order[q]; }
            m.faces.push_back(f);
        }
    }
    return m.serialize();
}

// An EditableMesh (or baked mesh content) as something to draw: triangles corner by corner, turned
// back to Godot's clockwise front.
Ref<ArrayMesh> PulseBlockzWorld::editable_to_mesh(const std::vector<float>& pos, const std::vector<float>& nrm, const std::vector<float>& uv, const std::vector<float>& rgba) {
    size_t corners = pos.size() / 3;
    if (corners < 3) return Ref<ArrayMesh>();
    PackedVector3Array v, n; PackedVector2Array t; PackedColorArray c;
    v.resize((int64_t)corners); n.resize((int64_t)corners); t.resize((int64_t)corners); c.resize((int64_t)corners);
    for (size_t k = 0; k < corners; k++) {
        size_t src = k - k % 3 + (k % 3 == 1 ? 2 : k % 3 == 2 ? 1 : 0);
        v[(int64_t)k] = Vector3(pos[src * 3], pos[src * 3 + 1], pos[src * 3 + 2]);
        if (nrm.size() >= (src + 1) * 3) n[(int64_t)k] = Vector3(nrm[src * 3], nrm[src * 3 + 1], nrm[src * 3 + 2]);
        if (uv.size() >= (src + 1) * 2) t[(int64_t)k] = Vector2(uv[src * 2], uv[src * 2 + 1]);
        c[(int64_t)k] = rgba.size() >= (src + 1) * 4 ? Color(rgba[src * 4], rgba[src * 4 + 1], rgba[src * 4 + 2], rgba[src * 4 + 3]) : Color(1, 1, 1, 1);
    }
    Array arrays; arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = v; arrays[Mesh::ARRAY_NORMAL] = n; arrays[Mesh::ARRAY_TEX_UV] = t; arrays[Mesh::ARRAY_COLOR] = c;
    Ref<ArrayMesh> out; out.instantiate();
    out->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    return out;
}

// CreateMeshPartAsync's meshes. A file loads or does not; a cloud asset is waited for until its
// fetch lands or fails.
void PulseBlockzWorld::pump_meshes() {
    for (size_t k = 0; k < meshJobs_.size();) {
        MeshJob& j = meshJobs_[k];
        MeshAnswer answer;
        answer.id = j.id;
        Ref<Mesh> mesh = load_mesh(j.uri);
        bool waiting = false;
        if (mesh.is_null() && isCloudAsset(j.uri)) {
            auto it = cloudAssets_.find(cloudNumber(j.uri));
            waiting = it != cloudAssets_.end() && !it->second.failed && it->second.file.empty();
        }
        if (waiting) { k++; continue; }
        if (mesh.is_valid()) {
            AABB box = mesh->get_aabb();
            answer.result.ok = true;
            answer.result.size = fromGd(box.size);
            if (j.geometry) answer.result.geometry = mesh_to_editable(mesh);
        } else {
            answer.result.error = "CreateMeshPartAsync: could not load the mesh " + j.uri;
        }
        (j.server ? meshAnswersServer_ : meshAnswersClient_).push_back(std::move(answer));
        meshJobs_.erase(meshJobs_.begin() + (int64_t)k);
    }
}

// ContentProvider:PreloadAsync: each content loaded by what its uri says it is, answered once it
// is warm in the cache the thing drawing it will read, or cannot be had.
void PulseBlockzWorld::pump_preloads() {
    for (size_t k = 0; k < preloadJobs_.size();) {
        PreloadJob& j = preloadJobs_[k];
        std::string low = j.uri;
        for (char& c : low) c = (char)std::tolower((unsigned char)c);
        auto has = [&](const char* s) { return low.find(s) != std::string::npos; };
        auto endsWith = [&](const char* e) { size_t n = std::strlen(e); return low.size() > n && low.compare(low.size() - n, n, e) == 0; };
        enum { Image, Mesh, Sound, Font, Other } kind = Other;
        if (has("mime=image") || endsWith(".png") || endsWith(".jpg") || endsWith(".jpeg") || endsWith(".webp") || endsWith(".bmp") || endsWith(".tga") || endsWith(".svg")) kind = Image;
        else if (has("mime=model") || has("mime=text%2fplain") || endsWith(".obj") || endsWith(".glb") || endsWith(".gltf") || endsWith(".mesh")) kind = Mesh;
        else if (has("mime=audio") || endsWith(".wav") || endsWith(".ogg") || endsWith(".mp3")) kind = Sound;
        else if (isFontFile(j.uri)) kind = Font;
        bool ok = false;
        switch (kind) {
            case Image: ok = load_texture(j.uri).is_valid(); break;
            case Mesh: ok = load_mesh(j.uri).is_valid(); break;
            case Sound: ok = load_stream(j.uri).is_valid(); break;
            case Font: { Value face = Value::string(j.uri); face.n = 400; ok = load_font(face).is_valid(); break; }
            case Other: ok = isCloudAsset(j.uri) ? !cloud_local(j.uri, "Preload").empty() : FileAccess::file_exists(String::utf8(j.uri.c_str())); break;
        }
        if (!ok && isCloudAsset(j.uri)) {
            auto it = cloudAssets_.find(cloudNumber(j.uri));
            if (it != cloudAssets_.end() && !it->second.failed && it->second.file.empty()) { k++; continue; }   // on its way
        }
        PreloadAnswer answer; answer.id = j.id; answer.ok = ok;
        (j.server ? preloadAnswersServer_ : preloadAnswersClient_).push_back(answer);
        preloadJobs_.erase(preloadJobs_.begin() + (int64_t)k);
    }
}

// CreateEditableImageAsync's images: decoded to RGBA8 bytes, at most 1024 a side.
void PulseBlockzWorld::pump_images() {
    for (size_t k = 0; k < imageJobs_.size();) {
        MeshJob& j = imageJobs_[k];
        ImageAnswer answer;
        answer.id = j.id;
        Ref<Texture2D> tex = load_texture(j.uri);
        if (tex.is_null() && isCloudAsset(j.uri)) {
            auto it = cloudAssets_.find(cloudNumber(j.uri));
            if (it != cloudAssets_.end() && !it->second.failed && it->second.file.empty()) { k++; continue; }
        }
        Ref<Image> img = tex.is_valid() ? tex->get_image() : Ref<Image>();
        if (img.is_valid()) {
            img = img->duplicate();
            if (img->is_compressed()) img->decompress();
            img->convert(Image::FORMAT_RGBA8);
        }
        if (img.is_null() || img->is_empty()) answer.result.error = "CreateEditableImageAsync: could not load the image " + j.uri;
        else if (img->get_width() > 1024 || img->get_height() > 1024) answer.result.error = "CreateEditableImageAsync: the image is larger than 1024x1024";
        else {
            PackedByteArray bytes = img->get_data();
            answer.result.ok = true;
            answer.result.w = img->get_width();
            answer.result.h = img->get_height();
            answer.result.rgba.assign((const char*)bytes.ptr(), (size_t)bytes.size());
        }
        (j.server ? imageAnswersServer_ : imageAnswersClient_).push_back(std::move(answer));
        imageJobs_.erase(imageJobs_.begin() + (int64_t)k);
    }
}

// Once a frame: every boolean whose combiner has built, sent home.
void PulseBlockzWorld::pump_solids() {
    for (size_t k = 0; k < solidJobs_.size();) {
        SolidJob& j = solidJobs_[k];
        SolidAnswer answer;
        answer.id = j.id;
        bool done = false;
        if (!j.root) {
            answer.result.error = j.error;
            done = true;
        } else if (++j.frames >= 2) {
            Ref<ArrayMesh> mesh;
            if (CSGShape3D* shape = Object::cast_to<CSGShape3D>(j.root)) {
                Array made = shape->get_meshes();
                if (made.size() >= 2) mesh = made[1];
            }
            const bool empty = mesh.is_null() || mesh->get_surface_count() == 0;
            // Built on a deferred call: a few frames' grace before it counts as empty.
            if (!empty || j.frames >= 8) {
                if (empty) answer.result.error = "The solid modeling operation resulted in an empty solid";
                else {
                    PackedVector3Array verts, norms;
                    PackedColorArray cols;
                    PackedVector2Array uvs;
                    std::vector<int32_t> tris;
                    for (int s = 0; s < mesh->get_surface_count(); s++) {
                        Array arrays = mesh->surface_get_arrays(s);
                        PackedVector3Array v = arrays[Mesh::ARRAY_VERTEX];
                        PackedVector3Array n;
                        if (arrays[Mesh::ARRAY_NORMAL].get_type() != Variant::NIL) n = arrays[Mesh::ARRAY_NORMAL];
                        PackedInt32Array idx;
                        if (arrays[Mesh::ARRAY_INDEX].get_type() != Variant::NIL) idx = arrays[Mesh::ARRAY_INDEX];
                        PackedVector2Array uv;
                        if (arrays[Mesh::ARRAY_TEX_UV].get_type() == Variant::PACKED_VECTOR2_ARRAY) uv = arrays[Mesh::ARRAY_TEX_UV];
                        // The surface's material is the operand it came from: its colour is the face colour.
                        Color face(1, 1, 1);
                        if (Ref<BaseMaterial3D> m = mesh->surface_get_material(s); m.is_valid()) face = m->get_albedo();
                        const int32_t base = (int32_t)verts.size();
                        for (int64_t i = 0; i < v.size(); i++) {
                            verts.push_back(v[i]);
                            norms.push_back(i < n.size() ? n[i] : Vector3(0, 1, 0));
                            cols.push_back(face);
                            uvs.push_back(i < uv.size() ? uv[i] : Vector2());
                        }
                        if (idx.is_empty()) for (int64_t i = 0; i < v.size(); i++) tris.push_back(base + (int32_t)i);
                        else for (int64_t i = 0; i < idx.size(); i++) tris.push_back(base + idx[i]);
                    }
                    // The pieces: everything as one, or -- SplitApart -- one per group of
                    // triangles that touch, found by the corners they share.
                    std::vector<std::vector<int32_t>> pieces;
                    if (!j.split) pieces.push_back(tris);
                    else {
                        std::vector<int32_t> parent(verts.size());
                        for (size_t i = 0; i < parent.size(); i++) parent[i] = (int32_t)i;
                        auto find = [&](int32_t x) { while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; } return x; };
                        std::unordered_map<std::string, int32_t> at;
                        auto spot = [&](int32_t i) {
                            char key[96];
                            const Vector3 p = verts[i];
                            std::snprintf(key, sizeof key, "%lld,%lld,%lld", (long long)std::llround(p.x / 1e-4f), (long long)std::llround(p.y / 1e-4f), (long long)std::llround(p.z / 1e-4f));
                            return at.emplace(key, i).first->second;
                        };
                        for (size_t t = 0; t + 2 < tris.size(); t += 3) {
                            const int32_t a = find(spot(tris[t])), b = find(spot(tris[t + 1]));
                            parent[b] = a;
                            parent[find(spot(tris[t + 2]))] = a;
                        }
                        std::unordered_map<int32_t, size_t> which;
                        for (size_t t = 0; t + 2 < tris.size(); t += 3) {
                            const int32_t g = find(spot(tris[t]));
                            auto it = which.find(g);
                            if (it == which.end()) { it = which.emplace(g, pieces.size()).first; pieces.emplace_back(); }
                            auto& piece = pieces[it->second];
                            piece.push_back(tris[t]); piece.push_back(tris[t + 1]); piece.push_back(tris[t + 2]);
                        }
                    }
                    for (const auto& piece : pieces) {
                        OpSurface surface = weldOpSurface(verts, norms, cols, uvs, piece);
                        if (!capOpSurface(surface)) {
                            answer.result.pieces.clear();
                            answer.result.error = "The solid modeling operation could not be simplified to 20,000 triangles";
                            break;
                        }
                        Vector3 middle, span;
                        std::string data = encodeOpMesh(surface, !j.keepOrigin, middle, span);
                        if (data.empty()) continue;
                        Runtime::SolidResult::Piece out;
                        out.meshData = std::move(data);
                        out.size = fromGd(span);
                        out.triangles = (int)(surface.i.size() / 3);
                        Vec3 pos, orient;
                        // Recentred: the part sits on its body. Kept: it sits where the main part does.
                        fromTransform(j.keepOrigin ? j.frame : j.frame * Transform3D(Basis(), middle), pos, orient);
                        out.pos = pos;
                        out.orient = orient;
                        answer.result.pieces.push_back(std::move(out));
                    }
                    answer.result.ok = !answer.result.pieces.empty();
                    if (!answer.result.ok && answer.result.error.empty()) answer.result.error = "The solid modeling operation resulted in an empty solid";
                }
                remove_child(j.root);
                j.root->queue_free();
                done = true;
            }
        }
        if (done) {
            (j.server ? solidAnswersServer_ : solidAnswersClient_).push_back(std::move(answer));
            solidJobs_.erase(solidJobs_.begin() + (int64_t)k);
        } else {
            k++;
        }
    }
}

// Copy src's surfaces into out, its points through xf and its normals through the inverse
// transpose, so a non-uniform scale keeps them right.
void PulseBlockzWorld::bake_mesh(const Ref<ArrayMesh>& out, const Ref<Mesh>& src, const Transform3D& xf) {
    Basis nb = xf.basis.inverse().transposed();
    Ref<ArrayMesh> am = src;                                   // a primitive is triangles
    for (int i = 0; i < src->get_surface_count(); i++) {
        Array arrays = src->surface_get_arrays(i);
        if (arrays.size() < Mesh::ARRAY_MAX) continue;
        PackedVector3Array v = arrays[Mesh::ARRAY_VERTEX];
        for (int64_t k = 0; k < v.size(); k++) v[k] = xf.xform(v[k]);
        arrays[Mesh::ARRAY_VERTEX] = v;
        if (arrays[Mesh::ARRAY_NORMAL].get_type() == Variant::PACKED_VECTOR3_ARRAY) {
            PackedVector3Array n = arrays[Mesh::ARRAY_NORMAL];
            for (int64_t k = 0; k < n.size(); k++) n[k] = nb.xform(n[k]).normalized();
            arrays[Mesh::ARRAY_NORMAL] = n;
        }
        arrays[Mesh::ARRAY_TANGENT] = Variant();
        out->add_surface_from_arrays(am.is_valid() ? am->surface_get_primitive_type(i) : Mesh::PRIMITIVE_TRIANGLES, arrays);
    }
}

// bake_mesh from a copy of the source's arrays kept here, for a source drawn again and again at
// different transforms and never itself changed: a file, or a primitive. Reading a mesh's arrays
// asks the renderer and waits on the GPU -- about 8 ms every time -- and SpecialMesh.Scale is
// written every frame. The arrays are copy-on-write; the kept ones are never transformed.
void PulseBlockzWorld::bake_kept(const Ref<ArrayMesh>& out, const Ref<Mesh>& src, const Transform3D& xf) {
    KeptMesh& kept = keptMeshes_[(uint64_t)src->get_instance_id()];
    if (kept.src != src) {
        kept.src = src;                                        // held, so the id cannot come round again
        kept.surfaces.clear();
        Ref<ArrayMesh> am = src;
        for (int i = 0; i < src->get_surface_count(); i++) {
            Array arrays = src->surface_get_arrays(i);
            if (arrays.size() < Mesh::ARRAY_MAX) continue;
            arrays[Mesh::ARRAY_TANGENT] = Variant();
            kept.surfaces.push_back({am.is_valid() ? am->surface_get_primitive_type(i) : Mesh::PRIMITIVE_TRIANGLES, arrays});
        }
    }
    Basis nb = xf.basis.inverse().transposed();
    for (const auto& [primitive, source] : kept.surfaces) {
        Array arrays = source.duplicate(false);
        PackedVector3Array v = arrays[Mesh::ARRAY_VERTEX];
        for (int64_t k = 0; k < v.size(); k++) v[k] = xf.xform(v[k]);
        arrays[Mesh::ARRAY_VERTEX] = v;
        if (arrays[Mesh::ARRAY_NORMAL].get_type() == Variant::PACKED_VECTOR3_ARRAY) {
            PackedVector3Array n = arrays[Mesh::ARRAY_NORMAL];
            for (int64_t k = 0; k < n.size(); k++) n[k] = nb.xform(n[k]).normalized();
            arrays[Mesh::ARRAY_NORMAL] = n;
        }
        out->add_surface_from_arrays(primitive, arrays);
    }
}

// A file that has been replaced: what was kept of the old one goes with it.
void PulseBlockzWorld::forget_mesh(const std::string& meshId) {
    if (auto it = meshes_.find(meshId); it != meshes_.end()) {
        if (it->second.is_valid()) keptMeshes_.erase((uint64_t)it->second->get_instance_id());
        meshes_.erase(it);
    }
}

void PulseBlockzWorld::collect_meshes(const Ref<ArrayMesh>& out, Node* node, const Transform3D& xf) {
    Transform3D here = xf;
    if (auto* n3 = Object::cast_to<Node3D>(node)) here = xf * n3->get_transform();
    Ref<Mesh> mesh;
    if (auto* mi = Object::cast_to<MeshInstance3D>(node)) mesh = mi->get_mesh();
    else if (auto* im = Object::cast_to<ImporterMeshInstance3D>(node)) { if (im->get_mesh().is_valid()) mesh = im->get_mesh()->get_mesh(); }
    if (mesh.is_valid()) bake_mesh(out, mesh, here);
    for (int i = 0; i < node->get_child_count(); i++) collect_meshes(out, node->get_child(i), here);
}

// The first DataModelMesh child of a part draws it. Reparenting one moves the look.
void PulseBlockzWorld::attach_data_mesh(int64_t id, DataMesh& m, int64_t partId) {
    if (m.part == partId) return;
    if (auto old = parts_.find(m.part); old != parts_.end() && old->second.dataMesh == id) {
        old->second.dataMesh = 0;
        for (auto& [oid, om] : dataMeshes_) if (oid != id && om.part == m.part) { old->second.dataMesh = oid; break; }
        update_shape(old->second); update_material(old->second);
    }
    m.part = partId;
    if (auto it = parts_.find(partId); it != parts_.end()) {
        if (!it->second.dataMesh) { it->second.dataMesh = id; update_shape(it->second); update_material(it->second); }
    }
}

// MeshType.Head: a hexagonal prism, its points left and right, filling Size times Scale. The
// unit mesh is 1 wide, sqrt(3)/2 tall and 1 deep, so a regular hexagon stays regular under an
// even scale. Faceted, with hard normals.
static Ref<ArrayMesh> hexagon_head_mesh() {
    Ref<SurfaceTool> st; st.instantiate();
    st->begin(Mesh::PRIMITIVE_TRIANGLES);
    Vector2 ring[6];
    for (int i = 0; i < 6; i++) { double a = i * Math_PI / 3; ring[i] = Vector2((real_t)std::cos(a) * 0.5f, (real_t)std::sin(a) * 0.5f); }
    // a face as a fan, wound clockwise seen from outside (Godot's front)
    auto face = [&](const std::vector<Vector3>& poly, Vector3 n, const std::vector<Vector2>& uv) {
        for (size_t i = 1; i + 1 < poly.size(); i++) {
            size_t ib = i, ic = i + 1;
            if ((poly[ib] - poly[0]).cross(poly[ic] - poly[0]).dot(n) > 0) std::swap(ib, ic);
            st->set_normal(n); st->set_uv(uv[0]); st->add_vertex(poly[0]);
            st->set_normal(n); st->set_uv(uv[ib]); st->add_vertex(poly[ib]);
            st->set_normal(n); st->set_uv(uv[ic]); st->add_vertex(poly[ic]);
        }
    };
    for (int side = 0; side < 2; side++) {   // the front cap (-Z, Roblox's Front) and the back
        real_t z = side ? 0.5f : -0.5f;
        std::vector<Vector3> poly; std::vector<Vector2> uv;
        for (int i = 0; i < 6; i++) { poly.push_back(Vector3(ring[i].x, ring[i].y, z)); uv.push_back(Vector2(ring[i].x + 0.5f, 0.5f - ring[i].y)); }
        face(poly, Vector3(0, 0, z > 0 ? 1.f : -1.f), uv);
    }
    for (int i = 0; i < 6; i++) {            // the six flanks
        const Vector2& a = ring[i]; const Vector2& b = ring[(i + 1) % 6];
        Vector2 mid = (a + b).normalized();
        face({Vector3(a.x, a.y, -0.5f), Vector3(b.x, b.y, -0.5f), Vector3(b.x, b.y, 0.5f), Vector3(a.x, a.y, 0.5f)},
             Vector3(mid.x, mid.y, 0), {Vector2(i / 6.f, 0), Vector2((i + 1) / 6.f, 0), Vector2((i + 1) / 6.f, 1), Vector2(i / 6.f, 1)});
    }
    return st->commit();
}

// What a SpecialMesh draws: FileMesh in the part's own units times Scale; the
// other MeshTypes a primitive the part's Size times Scale. Offset moves it.
Ref<Mesh> PulseBlockzWorld::data_mesh_for(const DataMesh& m, const Part& p) {
    Vector3 sc = toGd(m.scale), off = toGd(m.offset);
    Ref<Mesh> base;
    Transform3D xf;
    if (m.type == "FileMesh") { base = load_mesh(m.meshId); xf = Transform3D(Basis().scaled(sc), off); }
    else {
        Vector3 size = toGd(p.size) * sc;
        Basis rot;
        // The unit shape of each type, made once: it is only ever drawn from (bake_kept).
        const std::string kind = m.type == "Head" || m.type == "Sphere" || m.type == "Cylinder" || m.type == "Wedge" ? m.type : std::string("Brick");   // Brick, Torso, and the rest
        Ref<Mesh>& unit = unitMeshes_[kind];
        if (unit.is_null()) {
            if (kind == "Head") unit = hexagon_head_mesh();
            else if (kind == "Sphere") { Ref<SphereMesh> s; s.instantiate(); s->set_radius(0.5f); s->set_height(1); unit = s; }
            else if (kind == "Cylinder") { Ref<CylinderMesh> c; c.instantiate(); c->set_top_radius(0.5f); c->set_bottom_radius(0.5f); c->set_height(1); unit = c; }
            else if (kind == "Wedge") { Ref<PrismMesh> w; w.instantiate(); w->set_left_to_right(0); w->set_size(Vector3(1, 1, 1)); unit = w; }
            else { Ref<BoxMesh> b; b.instantiate(); b->set_size(Vector3(1, 1, 1)); unit = b; }
        }
        base = unit;
        if (kind == "Wedge") { rot = Basis(Vector3(0, 1, 0), Math_PI / 2); size = Vector3(size.z, size.y, size.x); }
        xf = Transform3D(rot * Basis().scaled(size), off);
    }
    if (base.is_null()) return base;
    Ref<ArrayMesh> out; out.instantiate();
    // An EditableMesh is edited where it is, so what was kept of it would go stale.
    if (m.type == "FileMesh" && m.meshId.rfind("rbxobject://", 0) == 0) bake_mesh(out, base, xf);
    else bake_kept(out, base, xf);
    return out;
}

// PlayOnRemove: gone from the tree, it plays once more, from wherever it was.
void PulseBlockzWorld::sound_removed(int64_t id, Sound& s) {
    if (!s.playOnRemove || s.stream.is_null() || !client_) return;
    Node* orphan;
    if (auto* src = Object::cast_to<AudioStreamPlayer3D>(s.node)) {
        auto* a = memnew(AudioStreamPlayer3D);
        a->set_stream(s.stream); a->set_volume_db(src->get_volume_db()); a->set_pitch_scale(src->get_pitch_scale());
        a->set_unit_size(src->get_unit_size()); a->set_max_distance(src->get_max_distance()); a->set_max_db(0);
        add_child(a);
        a->set_global_transform(src->get_global_transform());
        a->play(0); orphan = a;
    } else {
        auto* a = memnew(AudioStreamPlayer);
        a->set_stream(s.stream); a->set_volume_db(s.volume <= 0 ? -80.f : 20.f * std::log10(s.volume)); a->set_pitch_scale(std::max(s.speed, 0.01f));
        add_child(a);
        a->play(0); orphan = a;
    }
    orphan->set_name("PlayOnRemove");
    orphanSounds_.push_back(orphan);
}

void PulseBlockzWorld::detach_sounds(int64_t partId) {
    for (int64_t id : children_of(partId)) if (auto it = sounds_.find(id); it != sounds_.end() && it->second.part == partId && it->second.node) { it->second.node = nullptr; dirty_sounds(__LINE__); }
}

// A sound in a part plays from its mesh (3D); anywhere else, from everywhere. Only a world with
// a client makes players; a dedicated server still loads the files for TimeLength.
void PulseBlockzWorld::sync_sounds() {
    soundsDirty_ = false;
    for (auto& [id, s] : sounds_) {
        auto eit = entries_.find(id);
        int64_t holder = eit == entries_.end() ? kNoParent : eit->second.parent, partId = 0;
        // A Sound in an Attachment plays where the Attachment is, on its part.
        Transform3D at;
        if (auto ait = attachments_.find(holder); ait != attachments_.end()) {
            at = ait->second;
            auto ae = entries_.find(holder);
            holder = ae == entries_.end() ? holder : ae->second.parent;
        }
        auto pit = parts_.find(holder);
        MeshInstance3D* mesh = pit == parts_.end() ? nullptr : pit->second.mesh;
        if (mesh) partId = holder;
        s.at = at;
        if (s.node && s.part != partId) { stop_sound(s, true); s.node->queue_free(); s.node = nullptr; }
        if (!s.node && client_) {
            if (mesh) { auto* a = memnew(AudioStreamPlayer3D); a->set_stream(s.stream); mesh->add_child(a); s.node = a; }
            else { auto* a = memnew(AudioStreamPlayer); a->set_stream(s.stream); add_child(a); s.node = a; }
            s.node->set_name(String::utf8(eit == entries_.end() ? "Sound" : eit->second.name.c_str()));
            s.part = partId;
            style_sound(s);


            if (s.playing) start_sound(s);
        }
        load_sound(id, s);
        // Every pass: an effect can be added, removed or retuned long after the Sound exists.
        sync_sound_bus(id, s);
    }
}

// The local character's root and Humanoid: the client owns them, so a LocalScript's WalkSpeed,
// Jump or MoveTo drives the character without going through the server.
bool PulseBlockzWorld::local_root(int64_t id) const {
    if (!localChar_) return false;
    auto cit = chars_.find(localChar_);
    return cit != chars_.end() && cit->second.root == id;
}

bool PulseBlockzWorld::local_humanoid(int64_t id) const {
    if (!localChar_) return false;
    auto eit = entries_.find(id);
    return eit != entries_.end() && eit->second.className == "Humanoid" && eit->second.parent == localChar_;
}

// UserInputService.MouseBehavior locks / confines the pointer the way Roblox does;
// MouseIconEnabled hides it.
void PulseBlockzWorld::apply_input_settings(const Change& c) {
    if (c.name == "MouseBehavior") mouseBehavior_ = c.value.s;
    else if (c.name == "MouseIconEnabled") mouseIcon_ = c.value.b;
    else return;
    update_mouse_mode();
}

// Godot keycodes -> Enum.KeyCode item names. Letters and punctuation share ASCII.
static const char* keyName(Key k, InputEventKey* ev) {
    if (k >= KEY_A && k <= KEY_Z) { static char s[2] = {0, 0}; s[0] = (char)('A' + (int)k - (int)KEY_A); return s; }
    static const char* digits[] = {"Zero", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine"};
    if (k >= KEY_0 && k <= KEY_9) return digits[(int)k - (int)KEY_0];
    static const char* fkeys[] = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"};
    if (k >= KEY_F1 && k <= KEY_F12) return fkeys[(int)k - (int)KEY_F1];
    bool right = ev->get_location() == KEY_LOCATION_RIGHT;
    switch (k) {
    case KEY_SPACE: return "Space";
    case KEY_ESCAPE: return "Escape";
    case KEY_ENTER: return "Return";
    case KEY_KP_ENTER: return "KeypadEnter";
    case KEY_TAB: return "Tab";
    case KEY_BACKSPACE: return "Backspace";
    case KEY_DELETE: return "Delete";
    case KEY_INSERT: return "Insert";
    case KEY_HOME: return "Home";
    case KEY_END: return "End";
    case KEY_PAGEUP: return "PageUp";
    case KEY_PAGEDOWN: return "PageDown";
    case KEY_UP: return "Up";
    case KEY_DOWN: return "Down";
    case KEY_LEFT: return "Left";
    case KEY_RIGHT: return "Right";
    case KEY_SHIFT: return right ? "RightShift" : "LeftShift";
    case KEY_CTRL: return right ? "RightControl" : "LeftControl";
    case KEY_ALT: return right ? "RightAlt" : "LeftAlt";
    case KEY_META: return right ? "RightSuper" : "LeftSuper";
    case KEY_CAPSLOCK: return "CapsLock";
    case KEY_APOSTROPHE: return "Quote";
    case KEY_COMMA: return "Comma";
    case KEY_MINUS: return "Minus";
    case KEY_PERIOD: return "Period";
    case KEY_SLASH: return "Slash";
    case KEY_SEMICOLON: return "Semicolon";
    case KEY_EQUAL: return "Equals";
    case KEY_BRACKETLEFT: return "LeftBracket";
    case KEY_BACKSLASH: return "BackSlash";
    case KEY_BRACKETRIGHT: return "RightBracket";
    case KEY_QUOTELEFT: return "Backquote";
    default: return "Unknown";
    }
}

// Keys, mouse buttons, the wheel and motion go to the client runtime as UserInputService /
// ContextActionService input. Only a world with a client has anyone listening.
void PulseBlockzWorld::feed_input(const Ref<InputEvent>& event, bool processed) {
    if (!client_) return;
    Runtime::UserInput in;
    in.processed = processed;
    if (auto* k = Object::cast_to<InputEventKey>(event.ptr())) {
        if (k->is_echo()) return;
        in.type = "Keyboard";
        in.key = keyName(k->get_keycode(), k);
        in.state = k->is_pressed() ? "Begin" : "End";
    } else if (auto* mb = Object::cast_to<InputEventMouseButton>(event.ptr())) {
        Vector2 p = mb->get_position();
        in.position = {(float)p.x, (float)p.y, 0};
        switch (mb->get_button_index()) {
        case MOUSE_BUTTON_LEFT: in.type = "MouseButton1"; break;
        case MOUSE_BUTTON_RIGHT: in.type = "MouseButton2"; break;
        case MOUSE_BUTTON_MIDDLE: in.type = "MouseButton3"; break;
        case MOUSE_BUTTON_WHEEL_UP: in.type = "MouseWheel"; in.position.z = 1; break;
        case MOUSE_BUTTON_WHEEL_DOWN: in.type = "MouseWheel"; in.position.z = -1; break;
        default: return;
        }
        if (in.type == "MouseWheel") { if (!mb->is_pressed()) return; in.state = "Change"; }
        else in.state = mb->is_pressed() ? "Begin" : "End";
    } else if (auto* mm = Object::cast_to<InputEventMouseMotion>(event.ptr())) {
        Vector2 p = mm->get_position(), d = mm->get_relative();
        in.type = "MouseMovement";
        in.state = "Change";
        in.position = {(float)p.x, (float)p.y, 0};
        in.delta = {(float)d.x, (float)d.y, 0};
    } else return;
    // Roblox's camera reads gameProcessedEvent and leaves claimed input alone, so a game that
    // sinks its own right-drag does not spin the view as well.
    const bool rightPress = in.type == "MouseButton2" && in.state == "Begin";
    clientJobs_.push_back([this, in, rightPress](Runtime& rt) {
        rt.input(in);
        if (rightPress) camDragSunk_ = rt.lastInputSunk();
    });
}

// UserInputService.WindowFocusReleased / WindowFocused. A mouse button let go while another
// window has the focus never arrives as an InputEnded, so a charge waiting for one needs this.
void PulseBlockzWorld::_notification(int p_what) {
    if (p_what != NOTIFICATION_APPLICATION_FOCUS_IN && p_what != NOTIFICATION_APPLICATION_FOCUS_OUT) return;
    const bool focused = p_what == NOTIFICATION_APPLICATION_FOCUS_IN;
    clientJobs_.push_back([focused](Runtime& rt) { rt.windowFocus(focused); });
}

// ---- GUI ---------------------------------------------------------------------------
// The space at the top of the screen a ScreenGui keeps clear. Roblox reserves 36 pixels for its
// topbar and insets every ScreenGui below it, and IgnoreGuiInset opts out; no topbar is drawn
// here, but a place written for Roblox expects the gap. It reduces the rect from the top only,
// so bottom-anchored things do not move at all and centred ones move by half.
float PulseBlockzWorld::gui_inset(int64_t id) const {
    auto eit = entries_.find(id);
    if (eit == entries_.end()) return 0;
    auto pit = eit->second.props.find("IgnoreGuiInset");
    if (pit != eit->second.props.end() && pit->second.type == Value::Bool && pit->second.b) return 0;
    return kGuiInsetTop;
}

// The local player's PlayerGui, drawn: each ScreenGui a CanvasLayer with a full-screen root
// Control, each GuiObject a Panel (StyleBoxFlat for BackgroundColor3 / BorderSizePixel / a
// UICorner) holding a `content` Control, inset by a UIPadding, that its children and its Label
// hang from. A Roblox UDim2 is Godot's anchor (scale) plus offset (pixels). A UIListLayout on a
// parent places its children instead, every frame, as their sizes settle.
static const Value* guiProp(const std::unordered_map<std::string, Value>& props, const char* n) {
    auto it = props.find(n);
    return it == props.end() ? nullptr : &it->second;
}
static double guiNum(const std::unordered_map<std::string, Value>& p, const char* n, double d) { const Value* v = guiProp(p, n); return v && v->type == Value::Number ? v->n : d; }
static bool guiBool(const std::unordered_map<std::string, Value>& p, const char* n, bool d) { const Value* v = guiProp(p, n); return v && v->type == Value::Bool ? v->b : d; }

// The speed readout while driving. Roblox shows one when you sit in a VehicleSeat, unless
// HeadsUpDisplay says not to. Built once, then only shown, hidden and retitled.
void PulseBlockzWorld::update_vehicle_hud() {
    if (!client_) return;
    // Whose seat, and is it a vehicle's. A plain Seat has no HUD in Roblox either.
    const Part* seat = nullptr;
    if (localChar_) if (auto cit = chars_.find(localChar_); cit != chars_.end() && cit->second.seat)
        if (auto sit = parts_.find(cit->second.seat); sit != parts_.end() && sit->second.className == "VehicleSeat")
            seat = &sit->second;
    const bool show = seat && seat->headsUp && !(coreGuiHidden_ & (1 << 2));
    if (!show) {
        if (vehicleHud_) vehicleHud_->set_visible(false);
        return;
    }
    if (!vehicleHud_) {
        vehicleHud_ = memnew(Control);
        vehicleHud_->set_name("VehicleHud");
        vehicleHud_->set_anchors_preset(Control::PRESET_FULL_RECT);
        vehicleHud_->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        core_gui()->add_child(vehicleHud_);
        vehicleSpeed_ = memnew(Label);
        vehicleSpeed_->set_name("Speed");
        vehicleSpeed_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        vehicleSpeed_->add_theme_font_size_override("font_size", 22);
        vehicleSpeed_->add_theme_color_override("font_color", Color(1, 1, 1, 0.9f));
        vehicleSpeed_->add_theme_color_override("font_outline_color", Color(0, 0, 0, 0.7f));
        vehicleSpeed_->add_theme_constant_override("outline_size", 4);
        vehicleHud_->add_child(vehicleSpeed_);
        // Above the hotbar, which owns the bottom of the screen.
        vehicleSpeed_->set_anchors_and_offsets_preset(Control::PRESET_CENTER_BOTTOM, Control::PRESET_MODE_MINSIZE, 100);
        vehicleSpeed_->set_h_grow_direction(Control::GROW_DIRECTION_BOTH);
        vehicleSpeed_->set_v_grow_direction(Control::GROW_DIRECTION_BEGIN);
    }
    vehicleHud_->set_visible(true);
    // The assembly's speed: a welded seat has no velocity of its own, and sitting welds the
    // character in, so the assembly's root may be a CharacterBody3D whose velocity is where it
    // is trying to walk. The seat's own body if it has one, else a rigid body in its assembly.
    double speed = 0;
    const int64_t root = weld_root(seat->id) ? weld_root(seat->id) : seat->id;
    auto rigid = [&](int64_t id) -> RigidBody3D* {
        auto it = parts_.find(id);
        return it == parts_.end() ? nullptr : Object::cast_to<RigidBody3D>(it->second.body);
    };
    RigidBody3D* moving = rigid(seat->id);
    if (!moving) moving = rigid(root);
    if (!moving)
        for (auto& [pid, part] : parts_) {
            if (part.role == ROLE_CHAR_ROOT || part.role == ROLE_LIMB) continue;   // the driver, not the car
            if ((weld_root(pid) ? weld_root(pid) : pid) != root) continue;
            if ((moving = rigid(pid))) break;
        }
    if (moving) speed = moving->get_linear_velocity().length();
    vehicleSpeed_->set_text(String::num_int64((int64_t)std::llround(speed)) + " studs/s");
}

// The pointer: locked, confined, shown or hidden. MouseBehavior says what a place wants; a
// visible Modal button overrules it, which is what Modal is for.
void PulseBlockzWorld::update_mouse_mode() {
    if (!client_) return;
    bool modal = false;
    for (auto& [id, g] : guis_) {
        if (!g.button || !g.node || !guiBool(g.props, "Modal", false)) continue;
        if (g.node->is_visible_in_tree()) { modal = true; break; }
    }
    Input::MouseMode mode = modal ? (mouseIcon_ ? Input::MOUSE_MODE_VISIBLE : Input::MOUSE_MODE_HIDDEN)
                          : mouseBehavior_ == "LockCenter" ? Input::MOUSE_MODE_CAPTURED
                          : mouseBehavior_ == "LockCurrentPosition" ? Input::MOUSE_MODE_CONFINED
                          : mouseIcon_ ? Input::MOUSE_MODE_VISIBLE : Input::MOUSE_MODE_HIDDEN;
    wantedMouseMode_ = (int)mode;
    if (Input::get_singleton()->get_mouse_mode() != mode) Input::get_singleton()->set_mouse_mode(mode);
}

// What the pointer was last told to be. Input's own answer is the platform's, and headless has
// no pointer to lock: it reports VISIBLE whatever it was asked for.
int PulseBlockzWorld::wanted_mouse_mode() const { return wantedMouseMode_; }

static std::string guiStr(const std::unordered_map<std::string, Value>& p, const char* n, const char* d) { const Value* v = guiProp(p, n); return v && v->type == Value::String ? v->s : d; }
static std::string guiEnum(const std::unordered_map<std::string, Value>& p, const char* n, const char* d) { const Value* v = guiProp(p, n); return v && v->type == Value::Enum ? enumName(*v) : d; }
static Color guiColor(const std::unordered_map<std::string, Value>& p, const char* n, Color d) { const Value* v = guiProp(p, n); return v && v->type == Value::Color3 ? Color(v->c.r, v->c.g, v->c.b) : d; }
static Vec2 guiVec2(const std::unordered_map<std::string, Value>& p, const char* n, Vec2 d) { const Value* v = guiProp(p, n); return v && v->type == Value::Vector2 ? v->v2() : d; }
static Vec3 guiVec3(const std::unordered_map<std::string, Value>& p, const char* n, Vec3 d) { const Value* v = guiProp(p, n); return v && v->type == Value::Vector3 ? v->v : d; }
static UDim guiUDim(const std::unordered_map<std::string, Value>& p, const char* n, UDim d) { const Value* v = guiProp(p, n); return v && v->type == Value::UDim ? v->udim() : d; }
static UDim2 guiUDim2(const std::unordered_map<std::string, Value>& p, const char* n, UDim2 d) { const Value* v = guiProp(p, n); return v && v->type == Value::UDim2 ? v->udim2() : d; }
static const Color kGuiBackground(163 / 255.f, 162 / 255.f, 165 / 255.f), kGuiBorder(27 / 255.f, 42 / 255.f, 53 / 255.f);

// A parent -> children index over the mirror: gui_child is asked six times for each object
// styled, and walking every entry each time made one restyle cost a whole tree.
void PulseBlockzWorld::kids_add(int64_t id, int64_t parent) { kids_[parent].push_back(id); kidsCount_++; }

void PulseBlockzWorld::kids_drop(int64_t id, int64_t parent) {
    auto it = kids_.find(parent);
    if (it == kids_.end()) return;
    auto& v = it->second;
    auto at = std::find(v.begin(), v.end(), id);
    if (at == v.end()) return;
    v.erase(at);
    if (v.empty()) kids_.erase(it);
    kidsCount_--;
}

void PulseBlockzWorld::kids_move(int64_t id, int64_t from, int64_t to) {
    if (from == to) return;
    kids_drop(id, from);
    kids_add(id, to);
}

const std::vector<int64_t>& PulseBlockzWorld::children_of(int64_t parent) const {
    static const std::vector<int64_t> none;
    if (kidsCount_ != entries_.size()) {   // a path that made or lost an entry without saying so: start over
        kids_.clear();
        for (auto& [id, e] : entries_) kids_[e.parent].push_back(id);
        kidsCount_ = entries_.size();
    }
    auto it = kids_.find(parent);
    return it == kids_.end() ? none : it->second;
}

std::vector<int64_t> PulseBlockzWorld::gui_kids(int64_t parent) const {
    std::vector<int64_t> out;
    for (int64_t id : children_of(parent)) if (guis_.count(id)) out.push_back(id);
    return out;
}

std::vector<int64_t> PulseBlockzWorld::parts_under(int64_t parent) const {
    std::vector<int64_t> out;
    for (int64_t id : children_of(parent)) if (parts_.count(id)) out.push_back(id);
    return out;
}

int64_t PulseBlockzWorld::head_part(int64_t model) const {
    for (int64_t pid : parts_under(model)) {
        auto pe = entries_.find(pid); auto pp = parts_.find(pid);
        if (pe != entries_.end() && pp != parts_.end() && pe->second.name == "Head" && pp->second.mesh) return pid;
    }
    return 0;
}

const std::vector<int64_t>& PulseBlockzWorld::parts_on_root(int64_t root) const {
    static const std::vector<int64_t> none;
    const uint64_t frame = Engine::get_singleton()->get_process_frames();
    if (rootPartsFrame_ != frame) {
        rootParts_.clear();
        for (auto& [id, p] : parts_) if (p.attachedTo) rootParts_[p.attachedTo].push_back(id);
        rootPartsFrame_ = frame;
    }
    auto it = rootParts_.find(root);
    return it == rootParts_.end() ? none : it->second;
}

// The first child of `id` of that class (a UICorner / UIPadding / UIListLayout modifier).
int64_t PulseBlockzWorld::gui_child(int64_t id, const char* className) const {
    for (int64_t cid : gui_kids(id))
        if (auto e = entries_.find(cid); e != entries_.end() && e->second.className == className) return cid;
    return 0;
}

void PulseBlockzWorld::free_gui(Gui& g) {
    if (g.layer) g.layer->queue_free();
    else if (g.viewport) g.viewport->queue_free();               // the root Control goes with it
    else if (g.node) g.node->queue_free();
    if (g.quad) g.quad->queue_free();
    if (g.frame) g.frame->queue_free();          // the world and the camera go with it
    g.layer = nullptr; g.dock = nullptr; g.node = g.content = g.canvas = nullptr; g.label = nullptr; g.edit = nullptr; g.vbar = nullptr; g.hbar = nullptr; g.pic = nullptr;
    g.viewport = nullptr; g.quad = nullptr; g.quadMat.unref();
    g.frame = nullptr; g.frameCam = nullptr; g.frameRoot = nullptr;
    g.hovered = g.pressed = g.listed = g.wasListed = g.onQuad = false;
    g.dirty = true;          // whatever is built in its place starts unstyled
}

// Build / reparent / drop the Controls so the scene has exactly the GUI hanging
// under the local player's PlayerGui, then restyle everything from its properties.
void PulseBlockzWorld::sync_gui() {
    guiDirty_ = false;
    int64_t playerGui = 0;
    if (localPlayerId_)
        for (auto& [id, e] : entries_) if (e.className == "PlayerGui" && e.parent == localPlayerId_) { playerGui = id; break; }
    // The Control the children of `id` attach under, null when it is not on screen. Parents
    // resolve first, so the tree builds top-down whatever order the map has. Memoised in
    // `ensured`: a ScreenGui finds its UIScale by scanning every GUI object, so resolving each
    // object's chain unmemoised is quadratic in the object count.
    std::unordered_map<int64_t, Control*> ensured;
    std::function<Control*(int64_t)> ensure;
    std::function<Control*(int64_t)> ensureOnce = [&](int64_t id) -> Control* {
        auto git = guis_.find(id);
        if (git == guis_.end()) return nullptr;
        Gui& g = git->second;
        auto eit = entries_.find(id);
        int64_t parent = eit == entries_.end() ? kNoParent : eit->second.parent;
        if (g.kind == GUI_SCREEN) {
            // In game a ScreenGui is real only under the local player's PlayerGui; an editor
            // has no player and previews StarterGui in the rect it leaves for the viewport.
            const bool preview = guiPreview_ && starterGuiId_ && parent == starterGuiId_;
            // A plugin's GUI under CoreGui is real in the editor too; a DockWidgetPluginGui is
            // a floating titled frame, stacked down the right edge of the view.
            const bool core = coreGuiId_ && parent == coreGuiId_;
            const bool dock = core && eit != entries_.end() && eit->second.className == "DockWidgetPluginGui";
            if (!preview && !core && (!playerGui || parent != playerGui)) { free_gui(g); return nullptr; }
            if (!g.layer) {
                g.layer = memnew(CanvasLayer);
                g.layer->set_name(String::utf8(eit->second.name.c_str()));
                g.node = memnew(Control);
                g.node->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
                if (dock) {
                    g.dock = memnew(Panel);
                    g.dock->set_name("Dock");
                    Label* title = memnew(Label);
                    title->set_name("Title");
                    title->set_position(Vector2(8, 3));
                    title->add_theme_font_size_override("font_size", 12);
                    title->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
                    g.dock->add_child(title);
                    g.dock->add_child(g.node);
                    g.layer->add_child(g.dock);
                } else {
                    g.node->set_anchors_preset(Control::PRESET_FULL_RECT);
                    g.layer->add_child(g.node);
                }
                add_child(g.layer);
            }
            if (dock && g.dock) {
                const real_t w = (real_t)guiNum(g.props, "FloatingXSize", 300), h = (real_t)guiNum(g.props, "FloatingYSize", 200);
                int slot = 0;
                for (auto& [oid, og] : guis_) if (og.kind == GUI_SCREEN && og.dock && oid < id) slot++;
                Rect2 room = guiPreviewRect_.size.x > 1 ? guiPreviewRect_
                           : Rect2(Vector2(), get_viewport() ? get_viewport()->get_visible_rect().size : Vector2(1152, 648));
                g.dock->set_position(room.position + Vector2(room.size.x - w - 12, 12 + slot * 36));
                g.dock->set_size(Vector2(w, h + 24));
                if (auto* title = Object::cast_to<Label>(g.dock->get_node_or_null("Title"))) {
                    const Value* tv = guiProp(g.props, "Title");
                    title->set_text(String::utf8(tv && tv->type == Value::String && !tv->s.empty() ? tv->s.c_str() : eit->second.name.c_str()));
                }
                g.node->set_position(Vector2(0, 24));
                g.node->set_size(Vector2(w, h));
                return g.node;
            }
            // In game the root is the whole screen and is left as it was made: re-presetting it
            // every sync fights the layout. Only a preview, or a plugin's GUI in the editor,
            // moves it into the rect the editor leaves for the view.
            if ((preview || (core && guiPreview_)) && guiPreviewRect_.size.x > 1 && guiPreviewRect_.size.y > 1) {
                g.node->set_anchors_preset(Control::PRESET_TOP_LEFT);
                g.node->set_position(guiPreviewRect_.position);
                g.node->set_size(guiPreviewRect_.size);
            } else if (float s = screen_scale(id); s != 1.f) {
                // A logical screen of viewport / scale: anchors resolve inside it, and the
                // layer's transform scales the whole thing back up to fill the window.
                Vector2 vp = get_viewport() ? get_viewport()->get_visible_rect().size : Vector2(1280, 800);
                g.node->set_anchors_preset(Control::PRESET_TOP_LEFT);
                g.node->set_position(Vector2(0, gui_inset(id) / s));
                g.node->set_size(Vector2(vp.x / s, (vp.y - gui_inset(id)) / s));
            } else if (float top = gui_inset(id); top > 0) {
                g.node->set_anchors_preset(Control::PRESET_FULL_RECT);
                g.node->set_offset(SIDE_TOP, top);
            } else if (g.node->get_anchor(SIDE_RIGHT) != 1.0f || g.node->get_offset(SIDE_TOP) != 0) {
                g.node->set_anchors_preset(Control::PRESET_FULL_RECT);
            }
            return g.node;
        }
        if (g.kind == GUI_BILLBOARD || g.kind == GUI_SURFACE) {
            bool shown = false;
            for (int64_t a = parent, hops = 0; a != kNoParent && hops < 64; hops++) {
                if (a == workspaceId_) { shown = true; break; }
                auto ait = entries_.find(a);
                if (ait == entries_.end()) break;
                if (ait->second.className == "PlayerGui" && ait->second.parent == localPlayerId_) { shown = true; break; }
                a = ait->second.parent;
            }
            if (!shown || !client_) { free_gui(g); return nullptr; }
            if (g.kind == GUI_SURFACE) {
                // drawn off screen into a SubViewport, whose texture a quad wears on the face
                if (!g.viewport) {
                    g.viewport = memnew(SubViewport);
                    g.viewport->set_name(String::utf8(eit->second.name.c_str()));
                    g.viewport->set_transparent_background(true);
                    g.viewport->set_disable_3d(true);
                    g.viewport->set_update_mode(SubViewport::UPDATE_ALWAYS);
                    g.viewport->set_size(Vector2i(800, 600));
                    g.node = memnew(Control);
                    g.node->set_name("Canvas");
                    g.node->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
                    g.node->set_size(Vector2(800, 600));
                    g.viewport->add_child(g.node);
                    add_child(g.viewport);
                    g.quad = memnew(MeshInstance3D);
                    g.quad->set_name(String::utf8((eit->second.name + "Face").c_str()));
                    Ref<QuadMesh> qm; qm.instantiate();
                    g.quad->set_mesh(qm);
                    g.quadMat.instantiate();
                    g.quadMat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, g.viewport->get_texture());
                    g.quadMat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
                    g.quad->set_material_override(g.quadMat);
                    g.quad->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
                    g.quad->set_visible(false);                      // until it is placed
                    add_child(g.quad);
                }
                return g.node;
            }
            if (!billboardLayer_) {
                billboardLayer_ = memnew(CanvasLayer);
                billboardLayer_->set_name("BillboardGuis");
                billboardLayer_->set_layer(0);                       // under every ScreenGui
                add_child(billboardLayer_);
            }
            if (!g.node) {
                g.node = memnew(Control);
                g.node->set_name(String::utf8(eit->second.name.c_str()));
                g.node->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
                g.node->set_visible(false);                          // until it is placed
                billboardLayer_->add_child(g.node);
            }
            return g.node;
        }
        if (g.kind != GUI_OBJECT) return nullptr;
        Control* under = ensure(parent);
        if (!under) { free_gui(g); return nullptr; }
        if (!g.node) {
            Panel* panel = memnew(Panel);
            g.node = panel;
            g.node->set_name(String::utf8(eit->second.name.c_str()));
            g.content = memnew(Control);
            g.content->set_anchors_preset(Control::PRESET_FULL_RECT);
            g.content->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
            g.node->add_child(g.content);
            Array bound; bound.push_back(id);
            if (g.box) {
                // A TextBox is typed into: a LineEdit drawn flat over the Panel, or a
                // TextEdit when it is MultiLine (Enter is a newline there, as in Roblox).
                Array in; in.push_back(true); in.push_back(id);
                Array out; out.push_back(false); out.push_back(id);
                Ref<StyleBoxEmpty> flat; flat.instantiate();
                Control* box = nullptr;
                if (guiBool(g.props, "MultiLine", false)) {
                    g.multi = memnew(TextEdit);
                    for (const char* s : {"normal", "focus", "read_only"}) g.multi->add_theme_stylebox_override(s, flat);
                    g.multi->set_context_menu_enabled(false);
                    g.multi->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
                    g.multi->connect("text_changed", Callable(this, "_on_multi_changed").bindv(bound));
                    box = g.multi;
                } else {
                    g.edit = memnew(LineEdit);
                    for (const char* s : {"normal", "focus", "read_only"}) g.edit->add_theme_stylebox_override(s, flat);
                    g.edit->set_context_menu_enabled(false);
                    g.edit->connect("text_changed", Callable(this, "_on_edit_changed").bindv(bound));
                    g.edit->connect("text_submitted", Callable(this, "_on_edit_submitted").bindv(bound));
                    box = g.edit;
                }
                box->set_anchors_preset(Control::PRESET_FULL_RECT);
                set_box_text(g, guiStr(g.props, "Text", ""), -1);
                box->connect("focus_entered", Callable(this, "_on_edit_focus").bindv(in));
                box->connect("focus_exited", Callable(this, "_on_edit_focus").bindv(out));
                box->connect("gui_input", Callable(this, "_on_edit_input").bindv(bound));
                g.content->add_child(box);
            } else if (g.text) {
                g.label = memnew(Label);
                g.label->set_anchors_preset(Control::PRESET_FULL_RECT);
                g.label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
                g.content->add_child(g.label);
            } else if (eit->second.className == "ViewportFrame") {
                // Its own World3D: only what is put inside it is in the picture.
                g.frame = memnew(SubViewport);
                g.frame->set_name("Viewport");
                g.frame->set_use_own_world_3d(true);
                g.frame->set_transparent_background(true);
                g.frame->set_update_mode(SubViewport::UPDATE_ALWAYS);
                g.frameRoot = memnew(Node3D);
                g.frameRoot->set_name("World");
                g.frame->add_child(g.frameRoot);
                g.frameCam = memnew(Camera3D);
                g.frameCam->set_name("Camera");
                g.frameCam->set_current(true);
                g.frame->add_child(g.frameCam);
                auto* sun = memnew(DirectionalLight3D);
                sun->set_name("Light");
                g.frame->add_child(sun);
                g.node->add_child(g.frame);
                // Drawn through the same TextureRect an ImageLabel uses, so it is sized and
                // placed by that same code.
                g.pic = memnew(TextureRect);
                g.pic->set_name("Picture");
                g.pic->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
                g.pic->set_anchors_preset(Control::PRESET_FULL_RECT);
                g.pic->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
                g.pic->set_stretch_mode(TextureRect::STRETCH_SCALE);
                g.content->add_child(g.pic);
                g.pic->set_texture(g.frame->get_texture());
                visibilityDirty_ = true;   // the parts inside it were waiting for this
            } else if (g.image) {
                g.pic = memnew(TextureRect);
                g.pic->set_anchors_preset(Control::PRESET_FULL_RECT);
                g.pic->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
                g.pic->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
                g.content->add_child(g.pic);
            } else if (g.scrolling) {
                // The content clips a window onto a canvas the size of the window (Scale-sized
                // children are relative to it, as on Roblox), shifted by CanvasPosition.
                g.content->set_clip_contents(true);
                g.canvas = memnew(Control);
                g.canvas->set_name("Canvas");
                g.canvas->set_anchors_preset(Control::PRESET_FULL_RECT);
                g.canvas->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
                g.content->add_child(g.canvas);
                g.vbar = memnew(VScrollBar);
                g.hbar = memnew(HScrollBar);
                for (Range* bar : {(Range*)g.vbar, (Range*)g.hbar}) {
                    bar->set_step(1);
                    bar->connect("value_changed", Callable(this, "_on_scroll_bar").bindv(bound));
                    g.content->add_child(bar);
                }
            }
            g.node->connect("gui_input", Callable(this, "_on_gui_input").bindv(bound));
            Array enter; enter.push_back(true); enter.push_back(id);
            Array leave; leave.push_back(false); leave.push_back(id);
            g.node->connect("mouse_entered", Callable(this, "_on_gui_mouse").bindv(enter));
            g.node->connect("mouse_exited", Callable(this, "_on_gui_mouse").bindv(leave));
            under->add_child(g.node);
        } else if (g.node->get_parent() != under) {
            g.node->get_parent()->remove_child(g.node);
            under->add_child(g.node);
        }
        return gui_area(g);
    };
    ensure = [&](int64_t id) -> Control* {
        auto it = ensured.find(id);
        if (it != ensured.end()) return it->second;
        Control* under = ensureOnce(id);
        ensured[id] = under;
        return under;
    };
    const auto gt0 = std::chrono::steady_clock::now();
    for (auto& [id, g] : guis_) ensure(id);
    const auto gt1 = std::chrono::steady_clock::now();
    guiStyled_ = 0;
    for (auto& [id, g] : guis_) {
        if (g.kind == GUI_SCREEN && g.layer) {
            g.layer->set_visible(guiBool(g.props, "Enabled", true));
            // DisplayOrder is a script's to set, but must stay below the band the host draws
            // in: the core GUI and the wallet's confirmation sit at kHostGuiLayer, and a script
            // above them could cover a signing prompt with a fake one. Clamped, not trusted.
            int order = (int)guiNum(g.props, "DisplayOrder", 0);
            g.layer->set_layer(1 + std::clamp(order, 0, kHostGuiLayer - 2));
            g.layer->set_transform(Transform2D().scaled(Vector2(screen_scale(id), screen_scale(id))));
        } else if (g.kind == GUI_OBJECT && g.node) {
            // Only what changed: an asset still on its way is not retried every sync, since
            // cloud_arrived marks the objects naming it when it lands.
            if (guiRestyleAll_ || g.dirty) { style_gui(id, g); g.dirty = false; guiStyled_++; }
        }
        else if (g.kind == GUI_BILLBOARD && g.node) g.node->set_clip_contents(guiBool(g.props, "ClipsDescendants", false));
        else if (g.kind == GUI_SURFACE && g.viewport) style_surface(g);
    }
    const auto gt2 = std::chrono::steady_clock::now();
    sync_viewport_frames();
    const auto gt3 = std::chrono::steady_clock::now();
    // Only when something was made, destroyed, reparented, or had its ZIndex written.
    if (guiRestyleAll_ || guiOrderDirty_) { order_gui_siblings(); guiOrderDirty_ = false; }
    guiRestyleAll_ = false;
    const auto gt4 = std::chrono::steady_clock::now();
    auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
    guiEnsureMs_ = ms(gt0, gt1); guiStyleMs_ = ms(gt1, gt2); guiFramesMs_ = ms(gt2, gt3); guiOrderMs_ = ms(gt3, gt4);
}

// What a ScreenGui is magnified by: its own UIScale, applied to the layer, which is the only
// place it can go without moving every panel on it.
float PulseBlockzWorld::screen_scale(int64_t id) const {
    int64_t sid = gui_child(id, "UIScale");
    if (!sid) return 1.f;
    auto it = guis_.find(sid);
    if (it == guis_.end()) return 1.f;
    return std::clamp((float)guiNum(it->second.props, "Scale", 1), 0.25f, 4.f);
}

// Each frame's size, camera and light, from its own properties. The camera is whatever
// CurrentCamera names; without one the frame is aimed at whatever is inside it.
void PulseBlockzWorld::sync_viewport_frames() {
    for (auto& [id, g] : guis_) {
        if (!g.frame || !g.node) continue;
        Vector2 size = g.node->get_size();
        if (size.x >= 1 && size.y >= 1 && g.frame->get_size() != Vector2i(size))
            g.frame->set_size(Vector2i(size));
        // Bound here, not where the frame is built: a ViewportTexture resolves a PATH to its
        // viewport, so one asked for while the control is still detached can never bind, and
        // the TextureRect would hold a dead object for the rest of its life.
        if (g.pic && g.frame) {
            Ref<Texture2D> want = g.frame->get_texture();
            if (want.is_valid() && g.pic->get_texture() != want) g.pic->set_texture(want);
        }
        bool seen = g.node->is_visible_in_tree();
        auto want = seen ? SubViewport::UPDATE_ALWAYS : SubViewport::UPDATE_DISABLED;
        if (g.frame->get_update_mode() != want) g.frame->set_update_mode(want);
        if (!seen) continue;
        if (auto* sun = Object::cast_to<DirectionalLight3D>(g.frame->get_node_or_null("Light"))) {
            Vec3 dir = guiVec3(g.props, "LightDirection", {-1, -1, -1});
            Vector3 d = toGd(dir);
            if (d.length() > 0.001f) sun->look_at_from_position(Vector3(), d.normalized(), Vector3(0, 1, 0));
            sun->set_color(guiColor(g.props, "LightColor", Color(0.55f, 0.55f, 0.55f)));
        }
        // Ambient goes on the frame's own world: the town's lighting is not in here.
        if (Ref<World3D> w = g.frame->find_world_3d(); w.is_valid()) {
            Ref<Environment> env = w->get_environment();
            if (env.is_null()) {
                env.instantiate();
                env->set_background(Environment::BG_CANVAS);
                env->set_ambient_source(Environment::AMBIENT_SOURCE_COLOR);
                w->set_environment(env);
            }
            env->set_ambient_light_color(guiColor(g.props, "Ambient", Color(0.78f, 0.78f, 0.78f)));
            env->set_ambient_light_energy(1.0f);
        }

        // Through the Camera CurrentCamera names, as on Roblox: that CFrame, that vertical
        // FieldOfView. A frame that names one must not be aimed again here.
        if (const Value* cur = guiProp(g.props, "CurrentCamera"); cur && cur->type == Value::Ref && cur->ref) {
            if (auto cit = frameCams_.find(cur->ref); cit != frameCams_.end()) {
                g.frameCam->set_transform(toTransform(cit->second.pos, cit->second.orient));
                g.frameCam->set_fov((float)cit->second.fov);
                continue;
            }
        }
        // Nothing aimed it: far enough back to take in everything under it, looking at the
        // middle. Which way round the model faces is the model's business.
        if (g.frameRoot->get_child_count() > 0) {
            AABB all;
            bool first = true;
            for (int i = 0; i < g.frameRoot->get_child_count(); i++) {
                auto* n = Object::cast_to<Node3D>(g.frameRoot->get_child(i));
                if (!n) continue;
                AABB box(n->get_global_transform().origin, Vector3());
                for (int c = 0; c < n->get_child_count(); c++)
                    if (auto* m = Object::cast_to<VisualInstance3D>(n->get_child(c)))
                        box = box.merge(m->get_global_transform().xform(m->get_aabb()));
                all = first ? box : all.merge(box);
                first = false;
            }
            if (!first) {
                Vector3 mid = all.get_center();
                float reach = std::max(all.get_size().length(), 1.f);
                g.frameCam->set_fov(45);
                float back = reach * 1.15f;
                // FieldOfView is VERTICAL, so a distance that fits the height cuts the sides off
                // a frame taller than it is wide: pull back by 1/aspect when it is narrow.
                if (g.node) {
                    Vector2 box = g.node->get_size();
                    if (box.x > 0 && box.y > 0 && box.x < box.y) back *= box.y / box.x;
                }
                g.frameCam->look_at_from_position(mid + Vector3(0, reach * 0.15f, back), mid, Vector3(0, 1, 0));
            }
        }
    }
}

// Each parent's GUI children in ZIndex order, and within a ZIndex in creation order, as on
// Roblox. Godot picks the last child under the pointer and ignores z_index, which orders drawing
// only, so ordering the nodes themselves is what makes ZIndex decide hit testing too. Ids run up
// on the server and down on the client, so the magnitude is creation order either way.
void PulseBlockzWorld::order_gui_siblings() {
    std::unordered_map<Control*, std::vector<std::pair<std::pair<int, int64_t>, Control*>>> byParent;
    for (auto& [id, g] : guis_) {
        if (g.kind != GUI_OBJECT || !g.node) continue;
        Control* p = Object::cast_to<Control>(g.node->get_parent());
        if (!p) continue;
        byParent[p].push_back({{(int)guiNum(g.props, "ZIndex", 1), std::llabs(id)}, g.node});
    }
    for (auto& [p, kids] : byParent) {
        std::sort(kids.begin(), kids.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        // A parent's own furniture -- a Label, a TextureRect, a ScrollingFrame's canvas and
        // bars -- is made with the node and so sits ahead of every child of the instance.
        int base = p->get_child_count() - (int)kids.size();
        bool ordered = base >= 0;
        for (size_t i = 0; ordered && i < kids.size(); i++)
            if (p->get_child(base + (int)i) != (Node*)kids[i].second) ordered = false;
        if (ordered) continue;
        for (auto& k : kids) p->move_child(k.second, p->get_child_count() - 1);
    }
}

// The ZIndexBehavior of the ScreenGui / BillboardGui / SurfaceGui a GuiObject is on.
std::string PulseBlockzWorld::screen_gui_of(int64_t id) const {
    for (int hop = 0; hop < 12; hop++) {
        auto e = entries_.find(id);
        if (e == entries_.end()) break;
        id = e->second.parent;
        auto g = guis_.find(id);
        if (g == guis_.end()) continue;
        if (g->second.kind != GUI_OBJECT) return guiEnum(g->second.props, "ZIndexBehavior", "Sibling");
    }
    return "Sibling";
}

// Where a GuiObject's ScreenGui begins on the screen. AbsolutePosition is measured from there,
// not from the window corner: a script comparing it with a mouse position adds
// GuiService:GetGuiInset() itself, so the property must not already carry the inset. A
// BillboardGui's or SurfaceGui's objects count from their own canvas, which starts at 0.
Vector2 PulseBlockzWorld::gui_screen_origin(int64_t id) const {
    for (int hop = 0; hop < 12; hop++) {
        auto e = entries_.find(id);
        if (e == entries_.end()) break;
        id = e->second.parent;
        auto g = guis_.find(id);
        if (g == guis_.end()) continue;
        if (g->second.kind == GUI_SCREEN) return g->second.node ? g->second.node->get_global_position() : Vector2();
        if (g->second.kind != GUI_OBJECT) return Vector2();
    }
    return Vector2();
}

// ---- SurfaceGuis -------------------------------------------------------------------
// The quad's look: lit by the world as far as LightInfluence says (0: unshaded,
// Roblox's default), dimmed / brightened by Brightness, over everything when AlwaysOnTop.
void PulseBlockzWorld::style_surface(Gui& g) {
    auto& p = g.props;
    float b = (float)guiNum(p, "Brightness", 1);
    g.quadMat->set_albedo(Color(b, b, b, 1));
    g.quadMat->set_shading_mode(guiNum(p, "LightInfluence", 0) > 0 ? BaseMaterial3D::SHADING_MODE_PER_PIXEL : BaseMaterial3D::SHADING_MODE_UNSHADED);
    g.quadMat->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, guiBool(p, "AlwaysOnTop", false));
    g.quadMat->set_render_priority(guiBool(p, "AlwaysOnTop", false) ? 10 : 0);
    g.node->set_clip_contents(guiBool(p, "ClipsDescendants", false));
}

// Each frame: the quad over the chosen Face of the part, the face's size in
// studs, a hair off it (plus ZOffset); the canvas CanvasSize pixels, or the
// face times PixelsPerStud. Hidden when disabled, partless or past MaxDistance.
void PulseBlockzWorld::place_surface(int64_t id, Gui& g) {
    auto& p = g.props;
    g.part = billboard_part(id);
    auto pit = parts_.find(g.part);
    if (!camera_ || pit == parts_.end() || !pit->second.mesh || !guiBool(p, "Enabled", true)) { g.quad->set_visible(false); return; }
    const Part& part = pit->second;
    Transform3D xf = part.mesh->get_global_transform() * part.shapeLocal.affine_inverse();   // the part's own frame
    std::string face = guiEnum(p, "Face", "Front");
    Vector3 u, v, n;                                                 // the face's right, up and outward normal, in the part
    if (face == "Back") { u = Vector3(1, 0, 0); v = Vector3(0, 1, 0); n = Vector3(0, 0, 1); }
    else if (face == "Right") { u = Vector3(0, 0, -1); v = Vector3(0, 1, 0); n = Vector3(1, 0, 0); }
    else if (face == "Left") { u = Vector3(0, 0, 1); v = Vector3(0, 1, 0); n = Vector3(-1, 0, 0); }
    else if (face == "Top") { u = Vector3(1, 0, 0); v = Vector3(0, 0, -1); n = Vector3(0, 1, 0); }
    else if (face == "Bottom") { u = Vector3(1, 0, 0); v = Vector3(0, 0, 1); n = Vector3(0, -1, 0); }
    else { u = Vector3(-1, 0, 0); v = Vector3(0, 1, 0); n = Vector3(0, 0, -1); }
    Vector3 size = toGd(part.size);
    Vector2 studs(std::abs(size.dot(u)), std::abs(size.dot(v)));
    float out = std::abs(size.dot(n)) / 2 + 0.01f + (float)guiNum(p, "ZOffset", 0);
    Transform3D quadXf = xf * Transform3D(Basis(u, v, n), n * out);
    // SurfaceGui.MaxDistance defaults to 0, and 0 means no limit.
    double maxDist = guiNum(p, "MaxDistance", 0);
    if (maxDist > 0 && (quadXf.origin - camera_->get_global_position()).length() > maxDist) { g.quad->set_visible(false); return; }
    Vector2 canvas;
    if (guiEnum(p, "SizingMode", "FixedSize") == "PixelsPerStud") canvas = studs * (float)guiNum(p, "PixelsPerStud", 50);
    else { Vec2 c = guiVec2(p, "CanvasSize", {800, 600}); canvas = Vector2(c.x, c.y); }
    Vector2i px(std::clamp((int)std::lround(canvas.x), 1, 4096), std::clamp((int)std::lround(canvas.y), 1, 4096));
    if (g.viewport->get_size() != px) { g.viewport->set_size(px); g.node->set_size(Vector2(px)); }
    Ref<QuadMesh> qm = g.quad->get_mesh();
    if (qm.is_valid() && qm->get_size() != studs) qm->set_size(studs);
    g.quad->set_global_transform(quadXf);
    g.quad->set_visible(true);
}

// The pointer over a face: the mouse event goes into that SurfaceGui's viewport at the canvas
// pixel under the ray. Faces behind something are skipped unless AlwaysOnTop.
bool PulseBlockzWorld::surface_input(const Ref<InputEvent>& event) {
    auto* me = Object::cast_to<InputEventMouse>(event.ptr());
    if (!me || !camera_) return false;
    Vector2 at = me->get_position();
    Vector3 from = camera_->project_ray_origin(at), dir = camera_->project_ray_normal(at);
    // An equipped Tool is parented to the character rather than the Backpack, as on Roblox.
    bool holding = false;
    if (localChar_) {
        for (auto& [cid, ce] : entries_) {
            if (ce.parent != localChar_) continue;
            const ClassDef* tc = findClass(ce.className);
            if (tc && tc->isA("Tool")) { holding = true; break; }
        }
    }
    Gui* best = nullptr; double bestT = 1e30; Vector2 bestPx;
    for (auto& [id, g] : guis_) {
        if (g.kind != GUI_SURFACE || !g.quad || !g.quad->is_visible()) continue;
        Transform3D xf = g.quad->get_global_transform();
        Vector3 n = xf.basis.get_column(2);
        double facing = dir.dot(n);
        if (facing >= -1e-6) continue;                               // its back is to us
        double t = (xf.origin - from).dot(n) / facing;
        if (t <= 0 || t >= bestT) continue;
        Vector3 hit = from + dir * (float)t;
        Vector3 local = xf.affine_inverse().xform(hit);
        Ref<QuadMesh> qm = g.quad->get_mesh();
        if (qm.is_null()) continue;
        Vector2 half = qm->get_size() / 2;
        if (std::abs(local.x) > half.x || std::abs(local.y) > half.y) continue;
        if (!guiBool(g.props, "AlwaysOnTop", false) && occluded(from, hit - dir * 0.05f, g.part)) continue;
        // ToolPunchThroughDistance: close up, with a tool in hand, the surface stops catching
        // clicks. Zero, Roblox's default, means it never does.
        double punch = guiNum(g.props, "ToolPunchThroughDistance", 0);
        if (holding && punch > 0 && (hit - from).length() <= punch) continue;
        Vector2 vs = Vector2(g.viewport->get_size());
        bestPx = Vector2((local.x + half.x) / (2 * half.x) * vs.x, (half.y - local.y) / (2 * half.y) * vs.y);
        best = &g; bestT = t;
    }
    for (auto& [id, g] : guis_)
        if (g.kind == GUI_SURFACE && g.viewport && g.onQuad && &g != best) {
            g.onQuad = false;
            Ref<InputEventMouseMotion> leave; leave.instantiate();
            leave->set_position(Vector2(-1000, -1000));
            leave->set_global_position(Vector2(-1000, -1000));
            g.viewport->push_input(leave);
        }
    if (!best) return false;
    best->onQuad = true;
    Ref<InputEventMouse> copy = event->duplicate();
    if (copy.is_null()) return false;
    copy->set_position(bestPx);
    copy->set_global_position(bestPx);
    best->viewport->push_input(copy);
    return true;
}

// ---- BillboardGuis -----------------------------------------------------------------
// The part a BillboardGui hangs on: its Adornee (a part, or a Model's first
// part), else the nearest BasePart above it. 0 when there is none on screen.
int64_t PulseBlockzWorld::billboard_part(int64_t id) const {
    auto git = guis_.find(id);
    if (git == guis_.end()) return 0;
    auto partOf = [&](int64_t holder) -> int64_t {
        if (auto pit = parts_.find(holder); pit != parts_.end()) return pit->second.mesh ? holder : 0;
        for (int64_t pid : parts_under(holder))
            if (auto pp = parts_.find(pid); pp != parts_.end() && pp->second.mesh) return pid;
        return 0;
    };
    if (const Value* a = guiProp(git->second.props, "Adornee"); a && a->ref) return partOf(a->ref);
    auto eit = entries_.find(id);
    for (int64_t a = eit == entries_.end() ? kNoParent : eit->second.parent, hops = 0; a != kNoParent && hops < 64; hops++) {
        if (auto pit = parts_.find(a); pit != parts_.end()) return pit->second.mesh ? a : 0;
        auto ait = entries_.find(a);
        if (ait == entries_.end()) break;
        a = ait->second.parent;
    }
    return 0;
}

// Where a BillboardGui goes this frame: the part plus its offsets, through the camera. Size's
// scale is studs at that distance -- a stud is viewport height / (2 d tan(fov/2)) pixels -- and
// its offset pixels; the box is centred there, shifted by SizeOffset in its own size. Hidden
// behind the camera, past MaxDistance, from PlayerToHideFrom, or (unless AlwaysOnTop) occluded.
void PulseBlockzWorld::place_billboard(int64_t id, Gui& g) {
    auto& p = g.props;
    g.part = billboard_part(id);
    auto pit = parts_.find(g.part);
    Viewport* vp = get_viewport();
    if (!camera_ || !vp || pit == parts_.end() || !pit->second.mesh || !guiBool(p, "Enabled", true)) { g.node->set_visible(false); return; }
    const Part& part = pit->second;
    Transform3D xf = part.mesh->get_global_transform();
    Transform3D cam = camera_->get_global_transform();
    Vector3 right = cam.basis.get_column(0), up = cam.basis.get_column(1), back = cam.basis.get_column(2);
    // the part's half extents along the camera's axes and the world's
    Vector3 half(part.size.x / 2, part.size.y / 2, part.size.z / 2);
    auto extent = [&](const Vector3& axis) {
        float e = 0;
        for (int k = 0; k < 3; k++) e += std::abs(axis.dot(xf.basis.get_column(k))) * half[k];
        return e;
    };
    Vec3 so = guiVec3(p, "StudsOffset", {}), sow = guiVec3(p, "StudsOffsetWorldSpace", {});
    Vec3 eo = guiVec3(p, "ExtentsOffset", {}), eow = guiVec3(p, "ExtentsOffsetWorldSpace", {});
    Vector3 world = xf.origin + Vector3(sow.x, sow.y, sow.z)
        + right * (so.x + eo.x * extent(right)) + up * (so.y + eo.y * extent(up)) + back * (so.z + eo.z * extent(back))
        + Vector3(eow.x * extent(Vector3(1, 0, 0)), eow.y * extent(Vector3(0, 1, 0)), eow.z * extent(Vector3(0, 0, 1)));
    float d = -(cam.basis.inverse().xform(world - cam.origin)).z;
    double maxDist = guiNum(p, "MaxDistance", std::numeric_limits<double>::infinity());
    if (d <= 0.05f || d > maxDist) { g.node->set_visible(false); return; }
    // PlayerToHideFrom is decided here, not by withholding replication: the gui is in everyone's
    // world and only this machine knows which Player it belongs to.
    if (const Value* hide = guiProp(p, "PlayerToHideFrom");
        hide && hide->ref && hide->ref == localPlayerId_) { g.node->set_visible(false); return; }
    if (!guiBool(p, "AlwaysOnTop", false) && occluded(cam.origin, world, g.part)) { g.node->set_visible(false); return; }
    // Size: studs at the (limited, stepped) distance, plus pixels
    double lower = guiNum(p, "DistanceLowerLimit", 0), upper = guiNum(p, "DistanceUpperLimit", -1), step = guiNum(p, "DistanceStep", 0);
    double ds = d;
    if (upper >= 0) ds = std::min(ds, upper);
    ds = std::max(ds, lower);
    if (step > 0) ds = std::max(step, std::round(ds / step) * step);
    Vector2 view = vp->get_visible_rect().size;
    double pixelsPerStud = view.y / (2 * ds * std::tan(Math::deg_to_rad(camera_->get_fov()) / 2));
    UDim2 size = guiUDim2(p, "Size", {{1, 0}, {1, 0}});
    Vector2 px((float)(size.x.scale * pixelsPerStud + size.x.offset), (float)(size.y.scale * pixelsPerStud + size.y.offset));
    Vec2 shift = guiVec2(p, "SizeOffset", {0, 0});
    Vector2 centre = camera_->unproject_position(world);
    Vector2 pos = centre - px / 2 + Vector2(shift.x * px.x, -shift.y * px.y);
    g.node->set_position(pos.round());
    g.node->set_size(px.round());
    g.node->set_visible(true);
}

// ---- the hotbar --------------------------------------------------------------------
// Roblox's Backpack CoreGui: a row of slots along the bottom, `1 Wand`, the one
// in hand lit. Slots come from the client runtime (Tool.HotbarSlot); a click on
// one goes back to it as hotbarSelect, the same as the slot's key.
void PulseBlockzWorld::sync_hotbar() {
    hotbarDirty_ = false;
    int n = 0;
    for (auto& [id, s] : hotbarSlots_) n = std::max(n, std::min(s, 9));
    bool show = client_ && localPlayerId_ && n > 0 && !(coreGuiHidden_ & (1 << 2));
    if (!show) { if (hotbarRoot_) hotbarRoot_->set_visible(false); return; }
    if (!hotbarRoot_) {
        hotbarRoot_ = memnew(Control);
        hotbarRoot_->set_name("Backpack");
        hotbarRoot_->set_anchors_preset(Control::PRESET_FULL_RECT);
        hotbarRoot_->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        core_gui()->add_child(hotbarRoot_);
        hotbarBox_ = memnew(HBoxContainer);
        hotbarBox_->set_name("Hotbar");
        hotbarBox_->add_theme_constant_override("separation", 6);
        hotbarRoot_->add_child(hotbarBox_);
        hotbarBox_->set_anchors_and_offsets_preset(Control::PRESET_CENTER_BOTTOM, Control::PRESET_MODE_MINSIZE, 24);
        hotbarBox_->set_h_grow_direction(Control::GROW_DIRECTION_BOTH);
        hotbarBox_->set_v_grow_direction(Control::GROW_DIRECTION_BEGIN);
        for (int k = 0; k < 9; k++) {
            Button* b = memnew(Button);
            b->set_name(String::num_int64(k + 1));
            b->set_custom_minimum_size(Vector2(64, 64));
            b->set_focus_mode(Control::FOCUS_NONE);              // the keys stay the game's
            b->add_theme_font_size_override("font_size", 13);
            Array bound; bound.push_back(k);
            b->connect("pressed", Callable(this, "_on_hotbar_pressed").bindv(bound));
            hotbarBox_->add_child(b);
            hotbarButtons_.push_back(b);
        }
    }
    hotbarRoot_->set_visible(true);
    for (int k = 0; k < 9; k++) {
        Button* b = hotbarButtons_[k];
        b->set_visible(k < n);
        if (k >= n) continue;
        int64_t tool = 0;
        for (auto& [id, s] : hotbarSlots_) if (s == k + 1) tool = id;
        auto eit = entries_.find(tool);
        std::string name = eit == entries_.end() ? "" : eit->second.name;
        bool held = tool && localChar_ && eit != entries_.end() && eit->second.parent == localChar_;
        b->set_text(String::utf8((std::to_string(k + 1) + "\n" + name).c_str()));
        // Tool.ToolTip: shown on hover, as Roblox shows it in the backpack.
        std::string tip;
        if (eit != entries_.end()) {
            auto tp = eit->second.props.find("ToolTip");
            if (tp != eit->second.props.end() && tp->second.type == Value::String) tip = tp->second.s;
        }
        b->set_tooltip_text(String::utf8(tip.c_str()));
        b->set_disabled(!tool);
        Ref<StyleBoxFlat> box; box.instantiate();
        box->set_bg_color(held ? Color(1, 1, 1, 0.85f) : Color(0, 0, 0, tool ? 0.55f : 0.3f));
        box->set_corner_radius_all(6);
        b->add_theme_stylebox_override("normal", box);
        b->add_theme_stylebox_override("hover", box);
        b->add_theme_stylebox_override("pressed", box);
        b->add_theme_stylebox_override("disabled", box);
        Color text = held ? Color(0, 0, 0) : Color(1, 1, 1);
        b->add_theme_color_override("font_color", text);
        b->add_theme_color_override("font_hover_color", text);
        b->add_theme_color_override("font_pressed_color", text);
        b->add_theme_color_override("font_disabled_color", Color(1, 1, 1, 0.5f));
    }
}

void PulseBlockzWorld::_on_hotbar_pressed(int slot) {
    if (!client_) return;
    clientJobs_.push_back([slot](Runtime& rt) { rt.hotbarSelect(slot); });
}

// The CoreGui layer: over every ScreenGui, made when something first goes on it.
CanvasLayer* PulseBlockzWorld::core_gui() {
    if (!coreGuiLayer_) {
        coreGuiLayer_ = memnew(CanvasLayer);
        coreGuiLayer_->set_name("CoreGui");
        coreGuiLayer_->set_layer(kHostGuiLayer);
        add_child(coreGuiLayer_);
    }
    return coreGuiLayer_;
}

// A part between `from` and `to`, other than `part`'s own body (or the root it rides on) and the
// local character's: the camera looks past those.
// Roblox's popper: the camera never sits inside or behind the world. A ray from the subject to
// where the camera wants to be stops at the first thing in the way -- a wall, a floor, a hill
// -- and the camera comes in to just short of it. The subject's own body is not in the way,
// nor is the local character's. Out again is instant here, where Roblox eases back.
Vector3 PulseBlockzWorld::camera_pop(const Vector3& focus, const Vector3& want, int64_t subject) const {
    Ref<World3D> w = get_world_3d();
    if (!w.is_valid()) return want;
    PhysicsDirectSpaceState3D* space = w->get_direct_space_state();
    if (!space) return want;
    const Vector3 dir = want - focus;
    const float dist = dir.length();
    if (dist < 1e-3f) return want;
    Ref<PhysicsRayQueryParameters3D> q = PhysicsRayQueryParameters3D::create(focus, want);
    q->set_collision_mask(static_bit() | rigid_bit());
    TypedArray<RID> exclude;
    auto excludeBody = [&](int64_t pid) {
        auto it = parts_.find(pid);
        if (it == parts_.end()) return;
        if (auto* co = Object::cast_to<CollisionObject3D>(it->second.body)) exclude.push_back(co->get_rid());
        if (it->second.attachedTo) if (auto it2 = parts_.find(it->second.attachedTo); it2 != parts_.end())
            if (auto* co = Object::cast_to<CollisionObject3D>(it2->second.body)) exclude.push_back(co->get_rid());
    };
    // The whole character is out of the ray, as on Roblox: its root and everything attached to
    // it, head to cape. A cape hangs exactly where the ray from the head to the camera runs.
    auto excludeAssembly = [&](int64_t rootId) {
        excludeBody(rootId);
        for (const auto& [pid, part] : parts_)
            if (part.attachedTo == rootId)
                if (auto* co = Object::cast_to<CollisionObject3D>(part.body)) exclude.push_back(co->get_rid());
    };
    excludeAssembly(subject);
    if (auto cit = chars_.find(localChar_); cit != chars_.end()) excludeAssembly(cit->second.root);
    q->set_exclude(exclude);
    Dictionary hit = space->intersect_ray(q);
    if (hit.is_empty()) return want;
    // Short of the surface by a little, so the near plane does not cut into it; never closer
    // than half a stud, which is inside the head.
    const Vector3 at = hit["position"];
    const float margin = 0.35f;
    float d = std::max(0.5f, (at - focus).length() - margin);
    return focus + dir / dist * d;
}

bool PulseBlockzWorld::occluded(const Vector3& from, const Vector3& to, int64_t part) const {
    Ref<World3D> w = get_world_3d();
    if (!w.is_valid()) return false;
    PhysicsDirectSpaceState3D* space = w->get_direct_space_state();
    if (!space) return false;
    Ref<PhysicsRayQueryParameters3D> q = PhysicsRayQueryParameters3D::create(from, to);
    q->set_collision_mask(static_bit() | rigid_bit());              // this world's parts; the others share the space
    TypedArray<RID> exclude;
    auto excludeBody = [&](int64_t pid) {
        auto it = parts_.find(pid);
        if (it == parts_.end()) return;
        if (auto* co = Object::cast_to<CollisionObject3D>(it->second.body)) exclude.push_back(co->get_rid());
        if (it->second.attachedTo) if (auto it2 = parts_.find(it->second.attachedTo); it2 != parts_.end())
            if (auto* co = Object::cast_to<CollisionObject3D>(it2->second.body)) exclude.push_back(co->get_rid());
    };
    excludeBody(part);
    if (auto cit = chars_.find(localChar_); cit != chars_.end()) excludeBody(cit->second.root);
    q->set_exclude(exclude);
    return !space->intersect_ray(q).is_empty();
}

// ---- names over heads --------------------------------------------------------------
// Roblox's name tag: every other character's Humanoid.DisplayName a little over its Head, the
// same size at any distance, with a health bar under it while it is hurt (HealthDisplayType).
// Shown within NameDisplayDistance / HealthDisplayDistance -- the viewer's own Humanoid's or the
// subject's, as DisplayDistanceType says -- and, under NameOcclusion, only when nothing is in
// the way.
void PulseBlockzWorld::place_name_tags() {
    for (auto it = nameTags_.begin(); it != nameTags_.end();) {
        if (chars_.count(it->first)) { ++it; continue; }
        if (it->second.node) it->second.node->queue_free();
        it = nameTags_.erase(it);
    }
    if (!camera_ || !client_) { for (auto& [id, t] : nameTags_) if (t.node) t.node->set_visible(false); return; }
    const Character* me = nullptr;
    if (auto cit = chars_.find(localChar_); cit != chars_.end()) me = &cit->second;
    Transform3D cam = camera_->get_global_transform();
    for (auto& [model, ch] : chars_) {
        if (model == localChar_ || !ch.humanoid) continue;
        int64_t headId = head_part(model);
        NameTag& tag = nameTags_[model];
        auto hide = [&] { if (tag.node) tag.node->set_visible(false); };
        if (!headId) { hide(); continue; }
        const Character& subject = ch;
        const Character& by = ch.distanceType == "Subject" || !me ? subject : *me;
        Vector3 head = parts_[headId].mesh->get_global_transform().origin;
        Vector3 at = head + Vector3(0, 1.4f, 0);
        float d = -(cam.basis.inverse().xform(at - cam.origin)).z;
        bool showName = ch.distanceType != "None" && d > 0.05f && (at - cam.origin).length() <= by.nameDistance && !ch.displayName.empty();
        bool hurt = ch.healthType == "AlwaysOn" || (ch.healthType == "DisplayWhenDamaged" && ch.health < ch.maxHealth);
        bool showBar = hurt && d > 0.05f && (at - cam.origin).length() <= by.healthDistance;
        if (!showName && !showBar) { hide(); continue; }
        bool occludes = ch.occlusion == "OccludeAll" || (ch.occlusion == "EnemyOcclusion" && !ally(model));
        if (occludes && occluded(cam.origin, head, headId)) { hide(); continue; }
        if (!tag.node) {
            if (!namesRoot_) {
                namesRoot_ = memnew(Control);
                namesRoot_->set_name("PlayerNames");
                namesRoot_->set_anchors_preset(Control::PRESET_FULL_RECT);
                namesRoot_->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
                core_gui()->add_child(namesRoot_);
                namesRoot_->move_to_front();
            }
            tag.node = memnew(Control);
            auto eit = entries_.find(model);
            tag.node->set_name(String::utf8(eit == entries_.end() ? "Character" : eit->second.name.c_str()));
            tag.node->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
            tag.node->set_size(Vector2(240, 30));
            tag.name = memnew(Label);
            tag.name->set_name("Name");
            tag.name->set_position(Vector2(0, 0));
            tag.name->set_size(Vector2(240, 22));
            tag.name->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
            tag.name->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
            tag.name->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
            tag.name->add_theme_font_size_override("font_size", 16);
            tag.name->add_theme_color_override("font_color", Color(1, 1, 1));
            tag.name->add_theme_constant_override("outline_size", 4);
            tag.name->add_theme_color_override("font_outline_color", Color(0, 0, 0, 0.8f));
            tag.node->add_child(tag.name);
            tag.bar = memnew(Panel);
            tag.bar->set_name("Health");
            tag.bar->set_position(Vector2(120 - 32, 24));
            tag.bar->set_size(Vector2(64, 5));
            tag.bar->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
            Ref<StyleBoxFlat> back; back.instantiate();
            back->set_bg_color(Color(0, 0, 0, 0.6f));
            back->set_corner_radius_all(2);
            tag.bar->add_theme_stylebox_override("panel", back);
            tag.fill = memnew(Panel);
            tag.fill->set_name("Fill");
            tag.fill->set_position(Vector2(0, 0));
            tag.fill->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
            tag.bar->add_child(tag.fill);
            tag.node->add_child(tag.bar);
            namesRoot_->add_child(tag.node);
        }
        tag.name->set_text(String::utf8(ch.displayName.c_str()));
        tag.name->set_visible(showName);
        tag.bar->set_visible(showBar);
        if (showBar) {
            float frac = ch.maxHealth > 0 ? (float)std::clamp(ch.health / ch.maxHealth, 0.0, 1.0) : 0;
            tag.fill->set_size(Vector2(64 * frac, 5));
            Ref<StyleBoxFlat> sb; sb.instantiate();
            sb->set_bg_color(frac > 0.5f ? Color(0.16f, 0.82f, 0.2f) : frac > 0.25f ? Color(1, 0.8f, 0.1f) : Color(0.95f, 0.2f, 0.2f));
            sb->set_corner_radius_all(2);
            tag.fill->add_theme_stylebox_override("panel", sb);
        }
        Vector2 centre = camera_->unproject_position(at);
        tag.node->set_position((centre - Vector2(120, showName ? 26 : 2)).round());
        tag.node->set_visible(true);
    }
}

// ---- chat ----------------------------------------------------------------------------
// Roblox's chat window: the last messages at the top left over a box that `/` focuses; Enter
// sends the line to the server (Runtime::chat). CoreGuiType.Chat hides it.
void PulseBlockzWorld::sync_chat() {
    bool show = client_ && localPlayerId_ && !(coreGuiHidden_ & (1 << 3)) && chatActive_;
    if (!show) { if (chatRoot_) { chatRoot_->set_visible(false); if (chatTyping_) chatInput_->release_focus(); } return; }
    if (!chatRoot_) {
        chatRoot_ = memnew(Control);
        chatRoot_->set_name("Chat");
        chatRoot_->set_anchors_preset(Control::PRESET_FULL_RECT);
        chatRoot_->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        core_gui()->add_child(chatRoot_);
        PanelContainer* window = memnew(PanelContainer);
        window->set_name("Window");
        window->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        Ref<StyleBoxFlat> box; box.instantiate();
        box->set_bg_color(Color(0, 0, 0, 0.35f));
        box->set_corner_radius_all(6);
        box->set_content_margin_all(6);
        window->add_theme_stylebox_override("panel", box);
        chatRoot_->add_child(window);
        window->set_anchors_preset(Control::PRESET_TOP_LEFT);
        window->set_offset(SIDE_LEFT, 8); window->set_offset(SIDE_RIGHT, 348);
        window->set_offset(SIDE_TOP, 40); window->set_offset(SIDE_BOTTOM, 200);
        VBoxContainer* vbox = memnew(VBoxContainer);
        vbox->set_name("Box");
        vbox->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        window->add_child(vbox);
        chatMessages_ = memnew(VBoxContainer);
        chatMessages_->set_name("Messages");
        chatMessages_->set_v_size_flags(Control::SIZE_EXPAND_FILL);
        chatMessages_->set_alignment(BoxContainer::ALIGNMENT_END);
        chatMessages_->set_clip_contents(true);
        chatMessages_->add_theme_constant_override("separation", 1);
        chatMessages_->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        vbox->add_child(chatMessages_);
        chatInput_ = memnew(LineEdit);
        chatInput_->set_name("Input");
        chatInput_->set_placeholder("To chat click here or press / key");
        chatInput_->set_max_length(200);
        chatInput_->add_theme_font_size_override("font_size", 13);
        chatInput_->connect("text_submitted", Callable(this, "_on_chat_submitted"));
        Array in; in.push_back(true);
        Array out; out.push_back(false);
        chatInput_->connect("focus_entered", Callable(this, "_on_chat_focus").bindv(in));
        chatInput_->connect("focus_exited", Callable(this, "_on_chat_focus").bindv(out));
        vbox->add_child(chatInput_);
    }
    chatRoot_->set_visible(true);
}

void PulseBlockzWorld::_on_chat_focus(bool in) { chatTyping_ = in; }

// ---- the top bar and the escape menu -----------------------------------------------
static Ref<StyleBoxFlat> flatBox(Color color, int radius, int margin) {
    Ref<StyleBoxFlat> box; box.instantiate();
    box->set_bg_color(color);
    box->set_corner_radius_all(radius);
    box->set_content_margin_all(margin);
    return box;
}
static Button* coreButton(const String& name, const String& text, Color bg, int fontSize, Vector2 minSize) {
    Button* b = memnew(Button);
    b->set_name(name);
    b->set_text(text);
    b->set_custom_minimum_size(minSize);
    b->set_focus_mode(Control::FOCUS_NONE);
    b->add_theme_stylebox_override("normal", flatBox(bg, 6, 4));
    Color lit(bg.r + 0.2f, bg.g + 0.2f, bg.b + 0.2f, std::min(1.f, bg.a + 0.2f));
    b->add_theme_stylebox_override("hover", flatBox(lit, 6, 4));
    b->add_theme_stylebox_override("pressed", flatBox(lit, 6, 4));
    b->add_theme_color_override("font_color", Color(1, 1, 1));
    b->add_theme_color_override("font_hover_color", Color(1, 1, 1));
    b->add_theme_color_override("font_pressed_color", Color(1, 1, 1));
    b->add_theme_font_size_override("font_size", fontSize);
    return b;
}

// Roblox's top bar: the menu button and the chat toggle at the top left. Made
// with the local player; SetCore("TopbarEnabled", false) hides it.
void PulseBlockzWorld::sync_topbar() {
    bool show = client_ && localPlayerId_ && topbarEnabled_;
    if (!show) { if (topbarRoot_) topbarRoot_->set_visible(false); return; }
    if (!topbarRoot_) {
        topbarRoot_ = memnew(Control);
        topbarRoot_->set_name("Topbar");
        topbarRoot_->set_anchors_preset(Control::PRESET_TOP_WIDE);
        topbarRoot_->set_offset(SIDE_BOTTOM, 36);
        topbarRoot_->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        core_gui()->add_child(topbarRoot_);
        HBoxContainer* row = memnew(HBoxContainer);
        row->set_name("Buttons");
        row->set_position(Vector2(8, 4));
        row->add_theme_constant_override("separation", 6);
        topbarRoot_->add_child(row);
        Button* menu = coreButton("Menu", String::utf8("\xE2\x89\xA1"), Color(0, 0, 0, 0.5f), 18, Vector2(32, 28));
        menu->connect("pressed", Callable(this, "_on_menu_pressed"));
        row->add_child(menu);
        chatToggle_ = coreButton("Chat", "Chat", Color(0, 0, 0, 0.5f), 14, Vector2(32, 28));
        chatToggle_->connect("pressed", Callable(this, "_on_chat_toggle_pressed"));
        row->add_child(chatToggle_);
    }
    topbarRoot_->set_visible(true);
    chatToggle_->set_visible(!(coreGuiHidden_ & (1 << 3)));
}

void PulseBlockzWorld::_on_menu_pressed() { set_menu_open(!menuOpen_); }

// The chat toggle: the window comes and goes; GetCore("ChatActive") follows.
void PulseBlockzWorld::_on_chat_toggle_pressed() {
    chatActive_ = !chatActive_;
    sync_chat();
    bool on = chatActive_; int64_t sg = starterGuiId_;
    if (client_ && sg) clientJobs_.push_back([on, sg](Runtime& rt) { rt.hostWrite(sg, "ChatActive", Value::boolean(on)); });
}

// The escape menu: the players in the game, Resume / Reset Character / Leave. Reset and Leave
// ask for confirmation first, as Roblox's do.
void PulseBlockzWorld::set_menu_open(bool open) {
    if (!client_ || !localPlayerId_) open = false;
    if (open == menuOpen_) return;
    if (open && !menuRoot_) {
        menuRoot_ = memnew(Control);
        menuRoot_->set_name("Menu");
        menuRoot_->set_anchors_preset(Control::PRESET_FULL_RECT);
        menuRoot_->set_mouse_filter(Control::MOUSE_FILTER_STOP);   // modal: nothing under it takes the pointer
        core_gui()->add_child(menuRoot_);
        ColorRect* dim = memnew(ColorRect);
        dim->set_name("Dim");
        dim->set_color(Color(0, 0, 0, 0.55f));
        dim->set_anchors_preset(Control::PRESET_FULL_RECT);
        dim->set_mouse_filter(Control::MOUSE_FILTER_STOP);
        menuRoot_->add_child(dim);
        PanelContainer* panel = memnew(PanelContainer);
        panel->set_name("Panel");
        panel->add_theme_stylebox_override("panel", flatBox(Color(0.08f, 0.08f, 0.1f, 0.97f), 8, 16));
        panel->set_custom_minimum_size(Vector2(380, 0));
        panel->set_h_grow_direction(Control::GROW_DIRECTION_BOTH);
        panel->set_v_grow_direction(Control::GROW_DIRECTION_BOTH);
        menuRoot_->add_child(panel);
        panel->set_anchors_and_offsets_preset(Control::PRESET_CENTER, Control::PRESET_MODE_MINSIZE);   // offsets too: anchors alone keep it where it was, the corner
        VBoxContainer* vbox = memnew(VBoxContainer);
        vbox->set_name("Box");
        vbox->add_theme_constant_override("separation", 10);
        panel->add_child(vbox);
        Label* title = memnew(Label);
        title->set_name("Title");
        title->set_text("Menu");
        title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        title->add_theme_font_size_override("font_size", 22);
        title->add_theme_color_override("font_color", Color(1, 1, 1));
        vbox->add_child(title);
        Label* ph = memnew(Label);
        ph->set_name("PlayersHeader");
        ph->set_text("Players");
        ph->add_theme_font_size_override("font_size", 13);
        ph->add_theme_color_override("font_color", Color(0.7f, 0.7f, 0.75f));
        vbox->add_child(ph);
        menuPlayers_ = memnew(VBoxContainer);
        menuPlayers_->set_name("Players");
        menuPlayers_->add_theme_constant_override("separation", 2);
        vbox->add_child(menuPlayers_);
        // Settings, as Roblox's menu has them.
        Label* sh = memnew(Label);
        sh->set_name("SettingsHeader");
        sh->set_text("Settings");
        sh->add_theme_font_size_override("font_size", 13);
        sh->add_theme_color_override("font_color", Color(0.7f, 0.7f, 0.75f));
        vbox->add_child(sh);
        HBoxContainer* quality = memnew(HBoxContainer);
        quality->set_name("GraphicsQuality");
        quality->add_theme_constant_override("separation", 10);
        vbox->add_child(quality);
        Label* ql = memnew(Label);
        ql->set_text("Graphics Quality");
        ql->add_theme_font_size_override("font_size", 15);
        ql->add_theme_color_override("font_color", Color(1, 1, 1));
        quality->add_child(ql);
        qualityAuto_ = memnew(CheckButton);
        qualityAuto_->set_name("Automatic");
        qualityAuto_->set_text("Auto");
        qualityAuto_->set_focus_mode(Control::FOCUS_NONE);
        qualityAuto_->add_theme_font_size_override("font_size", 14);
        qualityAuto_->connect("toggled", Callable(this, "_on_quality_auto"));
        quality->add_child(qualityAuto_);
        qualitySlider_ = memnew(HSlider);
        qualitySlider_->set_name("Level");
        qualitySlider_->set_min(1); qualitySlider_->set_max(10); qualitySlider_->set_step(1);
        qualitySlider_->set_ticks(10); qualitySlider_->set_ticks_on_borders(true);
        qualitySlider_->set_custom_minimum_size(Vector2(130, 0));
        qualitySlider_->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        qualitySlider_->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
        qualitySlider_->set_focus_mode(Control::FOCUS_NONE);
        qualitySlider_->connect("value_changed", Callable(this, "_on_quality_slid"));
        quality->add_child(qualitySlider_);
        qualityValue_ = memnew(Label);
        qualityValue_->set_name("Value");
        qualityValue_->set_custom_minimum_size(Vector2(22, 0));
        qualityValue_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
        qualityValue_->add_theme_font_size_override("font_size", 15);
        qualityValue_->add_theme_color_override("font_color", Color(1, 1, 1));
        quality->add_child(qualityValue_);
        sync_quality_row();
        HBoxContainer* buttons = memnew(HBoxContainer);
        buttons->set_name("Buttons");
        buttons->add_theme_constant_override("separation", 8);
        buttons->set_alignment(BoxContainer::ALIGNMENT_CENTER);
        vbox->add_child(buttons);
        const char* names[] = {"Resume", "Reset", "Leave"};
        const char* texts[] = {"Resume", "Reset Character", "Leave"};
        Color colors[] = {Color(0.2f, 0.45f, 0.85f, 1), Color(0.25f, 0.25f, 0.3f, 1), Color(0.6f, 0.15f, 0.15f, 1)};
        for (int i = 0; i < 3; i++) {
            Button* b = coreButton(names[i], texts[i], colors[i], 15, Vector2(100, 34));
            Array bound; bound.push_back(i);
            b->connect("pressed", Callable(this, "_on_menu_button").bindv(bound));
            buttons->add_child(b);
        }
        // the question, over the panel
        confirmBox_ = memnew(PanelContainer);
        confirmBox_->set_name("Confirm");
        confirmBox_->add_theme_stylebox_override("panel", flatBox(Color(0.12f, 0.12f, 0.15f, 1), 8, 16));
        confirmBox_->set_custom_minimum_size(Vector2(300, 0));
        confirmBox_->set_h_grow_direction(Control::GROW_DIRECTION_BOTH);
        confirmBox_->set_v_grow_direction(Control::GROW_DIRECTION_BOTH);
        confirmBox_->set_visible(false);
        menuRoot_->add_child(confirmBox_);
        confirmBox_->set_anchors_and_offsets_preset(Control::PRESET_CENTER, Control::PRESET_MODE_MINSIZE);
        VBoxContainer* cbox = memnew(VBoxContainer);
        cbox->set_name("Box");
        cbox->add_theme_constant_override("separation", 12);
        confirmBox_->add_child(cbox);
        confirmText_ = memnew(Label);
        confirmText_->set_name("Question");
        confirmText_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        confirmText_->add_theme_font_size_override("font_size", 16);
        confirmText_->add_theme_color_override("font_color", Color(1, 1, 1));
        cbox->add_child(confirmText_);
        HBoxContainer* cb = memnew(HBoxContainer);
        cb->set_name("Buttons");
        cb->add_theme_constant_override("separation", 8);
        cb->set_alignment(BoxContainer::ALIGNMENT_CENTER);
        cbox->add_child(cb);
        confirmYes_ = coreButton("Yes", "Reset", Color(0.6f, 0.15f, 0.15f, 1), 15, Vector2(110, 34));
        Array yes; yes.push_back(true);
        confirmYes_->connect("pressed", Callable(this, "_on_confirm").bindv(yes));
        cb->add_child(confirmYes_);
        Button* no = coreButton("No", "Cancel", Color(0.25f, 0.25f, 0.3f, 1), 15, Vector2(110, 34));
        Array nay; nay.push_back(false);
        no->connect("pressed", Callable(this, "_on_confirm").bindv(nay));
        cb->add_child(no);
    }
    menuOpen_ = open;
    if (menuRoot_) menuRoot_->set_visible(open);
    if (confirmBox_) confirmBox_->set_visible(false);
    confirmKind_ = 0;
    if (open) {
        if (chatTyping_ && chatInput_) chatInput_->release_focus();
        if (typingBox_) release_typing();
        sync_menu_players();
        sync_quality_row();
    }
    if (client_) clientJobs_.push_back([open](Runtime& rt) { rt.menu(open); });
}

// ---- Graphics Quality -----------------------------------------------------------------------
// How much of the window the 3D view is drawn at, by level: whole from 7 up, softer below.
static float quality_scale(int level) {
    static const float k[] = {0.5f, 0.5f, 0.6f, 0.7f, 0.77f, 0.85f, 1, 1, 1, 1, 1};
    return k[std::clamp(level, 1, 10)];
}

// Kept beside the program, not in a place: the choice is the person's everywhere.
void PulseBlockzWorld::load_quality() {
    qualityLoaded_ = true;
    Ref<ConfigFile> cfg; cfg.instantiate();
    int saved = 0, level = 7;
    if (cfg->load("user://settings.cfg") == OK) {
        saved = std::clamp((int)cfg->get_value("graphics", "quality", 0), 0, 10);
        level = std::clamp((int)cfg->get_value("graphics", "automatic_level", 7), 1, 10);
    }
    qualitySaved_ = saved;
    qualityLevel_ = saved ? saved : level;   // Automatic picks up where it had settled
    apply_quality();
}

void PulseBlockzWorld::set_quality(int saved, bool save) {
    qualityLoaded_ = true;
    saved = std::clamp(saved, 0, 10);
    if (saved) qualityLevel_ = saved;
    qualitySaved_ = saved;
    qualityGpuMs_ = qualityFrameMs_ = 0; qualityFrames_ = 0; qualityHold_ = 0;
    qualityPersist_ = save;
    apply_quality();
    if (!save) return;
    Ref<ConfigFile> cfg; cfg.instantiate();
    cfg->load("user://settings.cfg");
    cfg->set_value("graphics", "quality", qualitySaved_);
    cfg->set_value("graphics", "automatic_level", qualityLevel_);
    cfg->save("user://settings.cfg");
}

void PulseBlockzWorld::apply_quality() {
    if (Viewport* vp = get_viewport()) {
        vp->set_scaling_3d_scale(quality_scale(qualityLevel_));
        // Automatic needs to know what a frame costs the graphics chip, and only then.
        RenderingServer::get_singleton()->viewport_set_measure_render_time(vp->get_viewport_rid(), qualitySaved_ == 0);
    }
    RenderingServer::get_singleton()->directional_shadow_atlas_set_size(qualityLevel_ >= 8 ? 4096 : 2048, true);
    update_lighting();
    for (auto& [id, l] : lights_) style_light(l);
    effectsDirty_ = true;
    int saved = qualitySaved_;
    if (client_) clientJobs_.push_back([saved](Runtime& rt) { rt.savedQuality(saved); });
    sync_quality_row();
}

// Automatic, over two seconds of frames: down a level if the 3D view took the graphics chip more
// than half the frame or frames were missed, up one if it took under two fifths. Half, not all,
// because a chip run flat out overheats where processor and graphics share a budget. A step down
// holds for half a minute, so it settles instead of hunting.
void PulseBlockzWorld::step_auto_quality(double delta) {
    if (qualitySaved_ != 0 || menuOpen_) return;
    Viewport* vp = get_viewport();
    if (!vp || DisplayServer::get_singleton()->get_name() == "headless") return;
    qualityHold_ = std::max(0.0, qualityHold_ - delta);
    qualityGpuMs_ += RenderingServer::get_singleton()->viewport_get_measured_render_time_gpu(vp->get_viewport_rid());
    qualityFrameMs_ += delta * 1000.0;
    qualityFrames_++;
    if (qualityFrameMs_ < 2000.0) return;
    const double gpu = qualityGpuMs_ / qualityFrames_, frame = qualityFrameMs_ / qualityFrames_;
    qualityGpuMs_ = qualityFrameMs_ = 0; qualityFrames_ = 0;
    float hz = DisplayServer::get_singleton()->screen_get_refresh_rate();
    const double budget = 1000.0 / (hz > 1 ? std::min(hz, 60.f) : 60.0);
    int next = qualityLevel_;
    if (gpu > budget * 0.5 || frame > budget * 1.25) next--;
    else if (gpu < budget * 0.4 && qualityHold_ <= 0) next++;   // a level is worth a twelfth of the frame at most, so this cannot overshoot the half
    next = std::clamp(next, 1, 10);
    if (next == qualityLevel_) return;
    if (next < qualityLevel_) qualityHold_ = 30.0;
    qualityLevel_ = next;
    apply_quality();
    if (!qualityPersist_) return;
    Ref<ConfigFile> cfg; cfg.instantiate();
    cfg->load("user://settings.cfg");
    cfg->set_value("graphics", "automatic_level", qualityLevel_);
    cfg->save("user://settings.cfg");
}

void PulseBlockzWorld::sync_quality_row() {
    if (!qualitySlider_) return;
    const bool automatic = qualitySaved_ == 0;
    qualityAuto_->set_pressed_no_signal(automatic);
    qualitySlider_->set_editable(!automatic);
    qualitySlider_->set_value_no_signal(qualityLevel_);
    qualityValue_->set_text(String::num_int64(qualityLevel_));
}

void PulseBlockzWorld::_on_quality_auto(bool on) { set_quality(on ? 0 : qualityLevel_, true); }
void PulseBlockzWorld::_on_quality_slid(double value) { if (qualitySaved_ != 0) set_quality((int)std::lround(value), true); }

void PulseBlockzWorld::sync_menu_players() {
    if (!menuPlayers_) return;
    for (int i = menuPlayers_->get_child_count() - 1; i >= 0; i--) { Node* c = menuPlayers_->get_child(i); menuPlayers_->remove_child(c); c->queue_free(); }
    std::vector<std::pair<std::string, int64_t>> rows;
    for (auto& [id, pi] : players_) if (auto e = entries_.find(id); e != entries_.end() && e->second.parent != kNoParent) rows.push_back({e->second.name, id});
    std::sort(rows.begin(), rows.end());
    for (auto& [name, id] : rows) {
        const PlayerInfo& pi = players_[id];
        Label* l = memnew(Label);
        l->set_name(String::utf8(name.c_str()));
        l->set_text(String::utf8((pi.displayName.empty() ? name : pi.displayName).c_str()) + (id == localPlayerId_ ? " (you)" : ""));
        l->add_theme_font_size_override("font_size", 15);
        l->add_theme_color_override("font_color", pi.neutral ? Color(1, 1, 1) : Color(pi.teamColor.r, pi.teamColor.g, pi.teamColor.b));
        menuPlayers_->add_child(l);
    }
}

// ---- SendNotification ----------------------------------------------------------------------
// Roblox's corner toast: a dark card, the title over the text, a picture to the left if there is
// one, and up to two buttons that answer the notification's Callback. Newest at the bottom.
void PulseBlockzWorld::notify(const Runtime::Notification& n) {
    if (!notificationsRoot_) {
        notificationsRoot_ = memnew(VBoxContainer);
        notificationsRoot_->set_name("Notifications");
        notificationsRoot_->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        notificationsRoot_->set_alignment(BoxContainer::ALIGNMENT_END);
        notificationsRoot_->set_anchors_preset(Control::PRESET_BOTTOM_RIGHT);
        notificationsRoot_->set_h_grow_direction(Control::GROW_DIRECTION_BEGIN);
        notificationsRoot_->set_v_grow_direction(Control::GROW_DIRECTION_BEGIN);
        notificationsRoot_->set_offset(SIDE_LEFT, -16);
        notificationsRoot_->set_offset(SIDE_RIGHT, -16);
        notificationsRoot_->set_offset(SIDE_TOP, -16);
        notificationsRoot_->set_offset(SIDE_BOTTOM, -16);
        notificationsRoot_->add_theme_constant_override("separation", 8);
        core_gui()->add_child(notificationsRoot_);
    }
    NotificationCard card;
    card.serial = ++notificationSerial_;
    card.until = Time::get_singleton()->get_ticks_msec() / 1000.0 + std::max(0.5, n.duration);
    card.node = memnew(PanelContainer);
    card.node->set_name(String::utf8(("Notification" + std::to_string((long long)card.serial)).c_str()));
    card.node->set_custom_minimum_size(Vector2(300, 0));
    card.node->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    Ref<StyleBoxFlat> sb; sb.instantiate();
    sb->set_bg_color(Color(0.1f, 0.1f, 0.1f, 0.82f));
    sb->set_corner_radius_all(8);
    sb->set_content_margin_all(10);
    card.node->add_theme_stylebox_override("panel", sb);

    VBoxContainer* column = memnew(VBoxContainer);
    column->add_theme_constant_override("separation", 6);
    column->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    card.node->add_child(column);
    HBoxContainer* row = memnew(HBoxContainer);
    row->add_theme_constant_override("separation", 10);
    row->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    column->add_child(row);
    if (!n.icon.empty()) {
        TextureRect* icon = memnew(TextureRect);
        icon->set_custom_minimum_size(Vector2(48, 48));
        icon->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
        icon->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
        icon->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        icon->set_texture(load_texture(n.icon));
        row->add_child(icon);
    }
    VBoxContainer* words = memnew(VBoxContainer);
    words->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    words->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    row->add_child(words);
    Label* title = memnew(Label);
    title->set_text(String::utf8(n.title.c_str()));
    title->add_theme_font_size_override("font_size", 16);
    title->add_theme_color_override("font_color", Color(1, 1, 1));
    words->add_child(title);
    Label* text = memnew(Label);
    text->set_text(String::utf8(n.text.c_str()));
    text->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    text->set_custom_minimum_size(Vector2(n.icon.empty() ? 280 : 222, 0));
    text->add_theme_font_size_override("font_size", 14);
    text->add_theme_color_override("font_color", Color(0.86f, 0.86f, 0.86f));
    words->add_child(text);
    if (!n.button1.empty() || !n.button2.empty()) {
        HBoxContainer* buttons = memnew(HBoxContainer);
        buttons->add_theme_constant_override("separation", 8);
        column->add_child(buttons);
        for (const std::string* label : {&n.button1, &n.button2}) {
            if (label->empty()) continue;
            Button* b = memnew(Button);
            b->set_text(String::utf8(label->c_str()));
            b->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            Array bound;
            bound.push_back(card.serial);
            bound.push_back(n.callback);
            bound.push_back(String::utf8(label->c_str()));
            b->connect("pressed", Callable(this, "_on_notification_button").bindv(bound));
            buttons->add_child(b);
        }
    }
    notificationsRoot_->add_child(card.node);
    notificationCards_.push_back(card);
    // Five at once is plenty of corner; the oldest makes room.
    while (notificationCards_.size() > 5) {
        notificationCards_.front().node->queue_free();
        notificationCards_.erase(notificationCards_.begin());
    }
}

void PulseBlockzWorld::expire_notifications() {
    if (notificationCards_.empty()) return;
    double now = Time::get_singleton()->get_ticks_msec() / 1000.0;
    for (auto it = notificationCards_.begin(); it != notificationCards_.end();) {
        if (now >= it->until) { it->node->queue_free(); it = notificationCards_.erase(it); } else ++it;
    }
}

void PulseBlockzWorld::_on_notification_button(int64_t serial, int64_t callback, String text) {
    for (auto it = notificationCards_.begin(); it != notificationCards_.end(); ++it) {
        if (it->serial == serial) { it->node->queue_free(); notificationCards_.erase(it); break; }
    }
    if (!callback || !client_) return;
    std::string label = text.utf8().get_data();
    clientJobs_.push_back([callback, label](Runtime& rt) { rt.notificationButton(callback, label); });
}

void PulseBlockzWorld::_on_menu_button(int which) {
    if (!menuOpen_) return;
    if (which == 0) { set_menu_open(false); return; }
    confirmKind_ = which;
    confirmText_->set_text(which == 1 ? "Reset your character?" : "Leave the game?");
    confirmYes_->set_text(which == 1 ? "Reset" : "Leave");
    confirmBox_->set_visible(true);
}

void PulseBlockzWorld::_on_confirm(bool yes) {
    int kind = confirmKind_;
    confirmKind_ = 0;
    if (confirmBox_) confirmBox_->set_visible(false);
    if (!yes || !kind) return;
    set_menu_open(false);
    if (kind == 1) { if (client_) clientJobs_.push_back([](Runtime& rt) { rt.resetCharacter(); }); }
    else if (kind == 2) { if (netClient_) disconnect_from_server(); emit_signal("leave_game"); }
}

void PulseBlockzWorld::_on_chat_submitted(const String& text) {
    chatInput_->set_text("");
    chatInput_->release_focus();
    chat(text);
}

void PulseBlockzWorld::chat(const String& text) {
    if (!client_) return;
    std::string s = text.utf8().get_data();
    clientJobs_.push_back([s](Runtime& rt) { rt.chat(s); });
}

// Roblox's rich text as Godot BBCode: the subset TextLabel.RichText draws -- <font color="#hex"
// size=N>, <b>, <i>, <u>, <s>, <br/> and the entities. Other tags are dropped, and what a player
// typed reaches the label as text.
static String richToBBCode(const std::string& rich) {
    std::string out;
    std::vector<std::string> fonts;   // what each open <font> closes with
    auto attr = [](const std::string& tag, const char* name) -> std::string {
        size_t at = tag.find(name);
        if (at == std::string::npos) return "";
        at = tag.find('=', at);
        if (at == std::string::npos) return "";
        at++;
        while (at < tag.size() && tag[at] == ' ') at++;
        char q = at < tag.size() && (tag[at] == '"' || tag[at] == '\'') ? tag[at++] : 0;
        size_t end = q ? tag.find(q, at) : tag.find_first_of(" >", at);
        return tag.substr(at, (end == std::string::npos ? tag.size() : end) - at);
    };
    for (size_t i = 0; i < rich.size();) {
        char c = rich[i];
        if (c == '<') {
            size_t end = rich.find('>', i);
            if (end == std::string::npos) { out += "[lb]"; i++; continue; }   // a stray '<' shows as it is
            std::string tag = rich.substr(i + 1, end - i - 1);
            i = end + 1;
            bool close = !tag.empty() && tag[0] == '/';
            std::string name = tag.substr(close ? 1 : 0, tag.find_first_of(" /", close ? 1 : 0) - (close ? 1 : 0));
            if (name == "font") {
                if (close) { if (!fonts.empty()) { out += fonts.back(); fonts.pop_back(); } continue; }
                std::string color = attr(tag, "color"), size = attr(tag, "size"), closes;
                if (!size.empty()) { out += "[font_size=" + size + "]"; closes += "[/font_size]"; }
                if (!color.empty()) { out += "[color=" + color + "]"; closes = "[/color]" + closes; }
                fonts.push_back(closes);
            } else if (name == "b" || name == "i" || name == "u" || name == "s") {
                out += (close ? "[/" : "[") + name + "]";
            } else if (name == "br") {
                out += "\n";
            }
            continue;
        }
        if (c == '&') {
            size_t end = rich.find(';', i);
            std::string ent = end == std::string::npos ? "" : rich.substr(i + 1, end - i - 1);
            const char* rep = ent == "lt" ? "<" : ent == "gt" ? ">" : ent == "amp" ? "&" : ent == "quot" ? "\"" : ent == "apos" ? "'" : nullptr;
            if (rep) { out += rep; i = end + 1; continue; }
        }
        if (c == '[') out += "[lb]"; else if (c == ']') out += "[rb]"; else out += c;
        i++;
    }
    return String::utf8(out.c_str());
}

// A line arrived: into the window ("Name: text", a system message in its colour) and, for a
// player or Chat:Chat, a bubble over the head or the part.
void PulseBlockzWorld::chat_line(const Runtime::ChatLine& line) {
    sync_chat();
    if (chatMessages_ && !line.part) {
        RichTextLabel* l = memnew(RichTextLabel);
        l->set_use_bbcode(true);
        l->set_fit_content(true);
        l->set_scroll_active(false);
        l->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
        std::string rich = line.rich;
        if (rich.empty()) rich = line.system ? line.text : line.name + ": " + line.text;
        Color col = line.system ? Color(line.color.r, line.color.g, line.color.b) : Color(1, 1, 1);
        l->set_text(richToBBCode(rich));
        l->add_theme_font_size_override("normal_font_size", 13);
        l->add_theme_font_size_override("bold_font_size", 13);
        l->add_theme_font_size_override("italics_font_size", 13);
        l->add_theme_color_override("default_color", col);
        l->add_theme_constant_override("outline_size", 3);
        l->add_theme_color_override("font_outline_color", Color(0, 0, 0, 0.7f));
        l->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        chatMessages_->add_child(l);
        while (chatMessages_->get_child_count() > 50) { Node* n = chatMessages_->get_child(0); chatMessages_->remove_child(n); n->queue_free(); }
    }
    if (line.system || !line.bubble || !bubbleChat_) return;
    Bubble b;
    b.part = line.part;
    if (line.speaker) { auto pit = players_.find(line.speaker); if (pit == players_.end() || !pit->second.character) return; b.model = pit->second.character; }
    b.text = line.text;
    b.until = Time::get_singleton()->get_ticks_msec() / 1000.0 + 15;   // Roblox's BubbleDuration
    if (!bubblesRoot_) {
        bubblesRoot_ = memnew(Control);
        bubblesRoot_->set_name("BubbleChat");
        bubblesRoot_->set_anchors_preset(Control::PRESET_FULL_RECT);
        bubblesRoot_->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        core_gui()->add_child(bubblesRoot_);
    }
    b.node = memnew(PanelContainer);
    std::string who = line.name;
    if (who.empty()) if (auto e = entries_.find(b.part); e != entries_.end()) who = e->second.name;
    b.node->set_name(String::utf8(who.c_str()));
    b.node->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    Ref<StyleBoxFlat> sb; sb.instantiate();
    sb->set_bg_color(Color(line.color.r, line.color.g, line.color.b, 0.95f));
    sb->set_corner_radius_all(8);
    sb->set_content_margin_all(6);
    b.node->add_theme_stylebox_override("panel", sb);
    Label* l = memnew(Label);
    l->set_name("Text");
    l->set_text(String::utf8(line.text.c_str()));
    // Only wrap what is long; a short bubble takes its natural width.
    if (line.text.size() > 30) {
        l->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
        l->set_custom_minimum_size(Vector2(260, 0));
    } else {
        l->set_autowrap_mode(TextServer::AUTOWRAP_OFF);
    }
    l->add_theme_font_size_override("font_size", 14);
    l->add_theme_color_override("font_color", Color(0.1f, 0.1f, 0.1f));
    l->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    l->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    b.node->add_child(l);
    bubblesRoot_->add_child(b.node);
    b.node->set_visible(false);
    // three per speaker, the oldest going first
    int same = 0;
    for (auto it = bubbles_.begin(); it != bubbles_.end();) {
        if ((it->model && it->model == b.model) || (it->part && it->part == b.part)) same++;
        if (same > 2) { it->node->queue_free(); it = bubbles_.erase(it); same--; } else ++it;
    }
    bubbles_.push_back(b);
}

// Each frame: every bubble over its head (above the name tag) or its part, the
// newest lowest; gone when its time is up or its speaker is.
void PulseBlockzWorld::place_bubbles() {
    double now = Time::get_singleton()->get_ticks_msec() / 1000.0;
    for (auto it = bubbles_.begin(); it != bubbles_.end();) {
        bool alive = now < it->until && (it->model ? chars_.count(it->model) > 0 : parts_.count(it->part) > 0);
        if (!alive) { it->node->queue_free(); it = bubbles_.erase(it); } else ++it;
    }
    if (!camera_) return;
    Transform3D cam = camera_->get_global_transform();
    std::unordered_map<int64_t, float> stacked;   // per anchor: pixels already taken above it
    for (auto it = bubbles_.rbegin(); it != bubbles_.rend(); ++it) {
        Bubble& b = *it;
        Vector3 at;
        int64_t key = b.model ? b.model : b.part;
        if (b.model) {
            int64_t headId = head_part(b.model);
            if (!headId) { b.node->set_visible(false); continue; }
            at = parts_[headId].mesh->get_global_transform().origin + Vector3(0, 1.4f, 0);
        } else {
            auto pit = parts_.find(b.part);
            if (pit == parts_.end() || !pit->second.mesh) { b.node->set_visible(false); continue; }
            at = pit->second.mesh->get_global_transform().origin + Vector3(0, pit->second.size.y / 2 + 0.5f, 0);
        }
        float d = -(cam.basis.inverse().xform(at - cam.origin)).z;
        if (d <= 0.05f) { b.node->set_visible(false); continue; }
        Vector2 centre = camera_->unproject_position(at);
        Vector2 size = b.node->get_size();
        float above = stacked[key];
        b.node->set_position((centre - Vector2(size.x / 2, (b.model ? 34 : 6) + size.y + above)).round());
        b.node->set_visible(true);
        stacked[key] = above + size.y + 4;
    }
}

// EnemyOcclusion: an ally is a character whose Player shares the local player's Team. NPCs and
// Neutral players are not.
bool PulseBlockzWorld::ally(int64_t model) const {
    auto me = players_.find(localPlayerId_);
    if (me == players_.end() || me->second.neutral || !me->second.team) return false;
    for (auto& [id, pi] : players_) if (pi.character == model) return !pi.neutral && pi.team == me->second.team;
    return false;
}

// ---- the player list and the health bar --------------------------------------------
// Roblox's PlayerList CoreGui: every Player at the top right, under its Team's name in the
// Team's colour when there are Teams, the Neutral ones last. Rebuilt whole on any change.
void PulseBlockzWorld::sync_player_list() {
    playerListDirty_ = false;
    std::vector<std::pair<std::string, int64_t>> rows, teams;   // (Name, id), by name
    for (auto& [id, pi] : players_) if (auto e = entries_.find(id); e != entries_.end() && e->second.parent != kNoParent) rows.push_back({e->second.name, id});
    for (auto& [id, col] : teams_) if (auto e = entries_.find(id); e != entries_.end() && e->second.parent != kNoParent) teams.push_back({e->second.name, id});
    bool show = client_ && localPlayerId_ && !rows.empty() && !(coreGuiHidden_ & (1 << 0));
    if (!show) { if (playerListRoot_) playerListRoot_->set_visible(false); return; }
    if (!playerListRoot_) {
        playerListRoot_ = memnew(Control);
        playerListRoot_->set_name("PlayerList");
        playerListRoot_->set_anchors_preset(Control::PRESET_FULL_RECT);
        playerListRoot_->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        core_gui()->add_child(playerListRoot_);
        PanelContainer* panel = memnew(PanelContainer);
        panel->set_name("Panel");
        panel->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        Ref<StyleBoxFlat> box; box.instantiate();
        box->set_bg_color(Color(0, 0, 0, 0.5f));
        box->set_corner_radius_all(6);
        box->set_content_margin_all(4);
        panel->add_theme_stylebox_override("panel", box);
        playerListRoot_->add_child(panel);
        panel->set_anchors_preset(Control::PRESET_TOP_RIGHT);
        panel->set_h_grow_direction(Control::GROW_DIRECTION_BEGIN);
        panel->set_v_grow_direction(Control::GROW_DIRECTION_END);
        panel->set_offset(SIDE_LEFT, -188); panel->set_offset(SIDE_RIGHT, -8);
        panel->set_offset(SIDE_TOP, 40); panel->set_offset(SIDE_BOTTOM, 40);     // under the top bar, as on Roblox
        playerList_ = memnew(VBoxContainer);
        playerList_->set_name("List");
        playerList_->add_theme_constant_override("separation", 2);
        playerList_->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        panel->add_child(playerList_);
    }
    playerListRoot_->set_visible(true);
    for (int k = playerList_->get_child_count() - 1; k >= 0; k--) { Node* n = playerList_->get_child(k); playerList_->remove_child(n); n->queue_free(); }
    std::sort(rows.begin(), rows.end());
    std::sort(teams.begin(), teams.end());
    // leaderstats: the columns are the stats' names in the order they were made,
    // four at most, as on Roblox; a player without one shows nothing there.
    std::vector<std::string> columns;
    std::unordered_map<int64_t, std::vector<std::pair<int64_t, std::string>>> statsOf;   // player -> (stat id, name)
    for (auto& [pname, pid] : rows) {
        int64_t folder = 0;
        for (auto& [id, e] : entries_) if (e.parent == pid && e.name == "leaderstats") { folder = id; break; }
        if (!folder) continue;
        std::vector<std::pair<int64_t, std::string>>& mine = statsOf[pid];
        for (auto& [id, e] : entries_) if (e.parent == folder && statClass(e.className)) mine.push_back({id, e.name});
        std::sort(mine.begin(), mine.end());
        for (auto& [id, name] : mine)
            if (columns.size() < 4 && std::find(columns.begin(), columns.end(), name) == columns.end()) columns.push_back(name);
    }
    auto statText = [](const Value& v) -> String {
        if (v.type == Value::String) return String::utf8(v.s.c_str());
        if (v.type == Value::Bool) return v.b ? "true" : "false";
        double n = v.n, a = std::fabs(n);
        char buf[32];
        if (a >= 1e9) std::snprintf(buf, sizeof buf, "%.1fB+", n / 1e9);
        else if (a >= 1e6) std::snprintf(buf, sizeof buf, "%.1fM+", n / 1e6);
        else if (a >= 1e4) std::snprintf(buf, sizeof buf, "%.1fK+", n / 1e3);
        else if (n == std::floor(n)) std::snprintf(buf, sizeof buf, "%.0f", n);
        else std::snprintf(buf, sizeof buf, "%.2f", n);
        std::string s = buf;
        if (size_t dot = s.find(".0"); dot != std::string::npos && !std::isdigit((unsigned char)s[dot + 2])) s.erase(dot, 2);   // 1.0K+ -> 1K+
        return String::utf8(s.c_str());
    };
    auto statLabel = [&](Control* into, const std::string& name, const String& text, Color color) {
        Label* l = memnew(Label);
        l->set_name(String::utf8(name.c_str()));
        l->set_text(text);
        l->set_custom_minimum_size(Vector2(56, 0));
        l->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
        l->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
        l->add_theme_font_size_override("font_size", 13);
        l->add_theme_color_override("font_color", color);
        l->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        into->add_child(l);
    };
    Control* panel = Object::cast_to<Control>(playerList_->get_parent());
    panel->set_offset(SIDE_LEFT, -188 - 62.f * (float)columns.size());
    if (!columns.empty()) {
        HBoxContainer* h = memnew(HBoxContainer);
        h->set_name("Stats");
        h->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        h->add_theme_constant_override("separation", 6);
        Control* spacer = memnew(Control);
        spacer->set_name("Name");
        spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        spacer->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        h->add_child(spacer);
        for (auto& col : columns) statLabel(h, col, String::utf8(col.c_str()), Color(0.85f, 0.85f, 0.85f));
        playerList_->add_child(h);
    }
    auto header = [&](const std::string& name, Color color) {
        PanelContainer* h = memnew(PanelContainer);
        h->set_name(String::utf8(name.c_str()));
        h->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        Ref<StyleBoxFlat> sb; sb.instantiate();
        sb->set_bg_color(Color(color.r, color.g, color.b, 0.85f));
        sb->set_corner_radius_all(4);
        sb->set_content_margin(SIDE_LEFT, 6); sb->set_content_margin(SIDE_RIGHT, 6); sb->set_content_margin(SIDE_TOP, 1); sb->set_content_margin(SIDE_BOTTOM, 1);
        h->add_theme_stylebox_override("panel", sb);
        Label* l = memnew(Label);
        l->set_name("Name");
        l->set_text(String::utf8(name.c_str()));
        l->add_theme_font_size_override("font_size", 13);
        l->add_theme_color_override("font_color", color.get_luminance() > 0.6f ? Color(0, 0, 0) : Color(1, 1, 1));
        l->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        h->add_child(l);
        playerList_->add_child(h);
    };
    auto row = [&](int64_t id, const std::string& name) {
        const PlayerInfo& pi = players_[id];
        HBoxContainer* r = memnew(HBoxContainer);
        r->set_name(String::utf8(name.c_str()));
        r->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        r->add_theme_constant_override("separation", 6);
        ColorRect* swatch = memnew(ColorRect);
        swatch->set_name("TeamColor");
        swatch->set_custom_minimum_size(Vector2(10, 10));
        swatch->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
        swatch->set_color(pi.neutral ? Color(1, 1, 1, 0.25f) : Color(pi.teamColor.r, pi.teamColor.g, pi.teamColor.b));
        swatch->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        r->add_child(swatch);
        Label* l = memnew(Label);
        l->set_name("Name");
        l->set_text(String::utf8((pi.displayName.empty() ? name : pi.displayName).c_str()));
        l->add_theme_font_size_override("font_size", 13);
        l->add_theme_color_override("font_color", id == localPlayerId_ ? Color(1, 1, 1) : Color(0.85f, 0.85f, 0.85f));
        l->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        l->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
        l->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        r->add_child(l);
        for (auto& col : columns) {
            String text;
            if (auto sit = statsOf.find(id); sit != statsOf.end())
                for (auto& [statId, statName] : sit->second)
                    if (statName == col) {
                        // a stat still at its default never sent a Value: 0 / "" / false
                        if (auto vit = statValues_.find(statId); vit != statValues_.end()) text = statText(vit->second);
                        else { const std::string& cls = entries_[statId].className; text = cls == "StringValue" ? "" : cls == "BoolValue" ? "false" : "0"; }
                        break;
                    }
            statLabel(r, col, text, id == localPlayerId_ ? Color(1, 1, 1) : Color(0.85f, 0.85f, 0.85f));
        }
        playerList_->add_child(r);
    };
    auto onTeam = [&](const PlayerInfo& pi, int64_t team) { return !pi.neutral && pi.team == team && teams_.count(team) && team; };
    for (auto& [name, team] : teams) {
        Col3 c = teams_[team];
        header(name, Color(c.r, c.g, c.b));
        for (auto& [pname, id] : rows) if (onTeam(players_[id], team)) row(id, pname);
    }
    bool first = true;
    for (auto& [pname, id] : rows) {
        if (onTeam(players_[id], players_[id].team)) continue;
        if (!teams.empty() && first) header("Neutral", Color(0.5f, 0.5f, 0.5f));
        first = false;
        row(id, pname);
    }
}

bool PulseBlockzWorld::statClass(const std::string& className) {
    return className == "IntValue" || className == "NumberValue" || className == "StringValue" || className == "BoolValue";
}

// A stat inside a Player's `leaderstats` Folder, or that Folder itself.
bool PulseBlockzWorld::leaderstat(int64_t id) const {
    auto e = entries_.find(id);
    if (e == entries_.end()) return false;
    if (e->second.name == "leaderstats" && players_.count(e->second.parent)) return true;
    auto f = entries_.find(e->second.parent);
    return f != entries_.end() && f->second.name == "leaderstats" && players_.count(f->second.parent) && statClass(e->second.className);
}

// Roblox's Health CoreGui: the local Humanoid's health as a bar at the top right, while hurt.
void PulseBlockzWorld::sync_health() {
    double frac = -1;
    if (localChar_ && !(coreGuiHidden_ & (1 << 1)))
        if (auto cit = chars_.find(localChar_); cit != chars_.end() && cit->second.humanoid && cit->second.health < cit->second.maxHealth)
            frac = cit->second.maxHealth > 0 ? std::clamp(cit->second.health / cit->second.maxHealth, 0.0, 1.0) : 0;
    if (frac == healthShown_) return;
    healthShown_ = frac;
    if (frac < 0) { if (healthRoot_) healthRoot_->set_visible(false); return; }
    if (!healthRoot_) {
        healthRoot_ = memnew(Control);
        healthRoot_->set_name("Health");
        healthRoot_->set_anchors_preset(Control::PRESET_FULL_RECT);
        healthRoot_->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        core_gui()->add_child(healthRoot_);
        Panel* bar = memnew(Panel);
        bar->set_name("Bar");
        bar->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        Ref<StyleBoxFlat> back; back.instantiate();
        back->set_bg_color(Color(0, 0, 0, 0.6f));
        back->set_corner_radius_all(3);
        bar->add_theme_stylebox_override("panel", back);
        healthRoot_->add_child(bar);
        bar->set_anchors_preset(Control::PRESET_TOP_RIGHT);
        bar->set_offset(SIDE_LEFT, -128); bar->set_offset(SIDE_RIGHT, -8);
        bar->set_offset(SIDE_TOP, 14); bar->set_offset(SIDE_BOTTOM, 26);
        healthFill_ = memnew(Panel);
        healthFill_->set_name("Fill");
        healthFill_->set_position(Vector2(0, 0));
        healthFill_->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        bar->add_child(healthFill_);
    }
    healthRoot_->set_visible(true);
    healthFill_->set_size(Vector2(120 * (float)frac, 12));
    Ref<StyleBoxFlat> sb; sb.instantiate();
    sb->set_bg_color(frac > 0.5 ? Color(0.16f, 0.82f, 0.2f) : frac > 0.25 ? Color(1, 0.8f, 0.1f) : Color(0.95f, 0.2f, 0.2f));
    sb->set_corner_radius_all(3);
    healthFill_->add_theme_stylebox_override("panel", sb);
}

void PulseBlockzWorld::style_gui(int64_t id, Gui& g) {
    auto& p = g.props;
    Control* n = g.node;
    Vec2 a = guiVec2(p, "AnchorPoint", {0, 0});
    if (!g.listed) {
        UDim2 pos = guiUDim2(p, "Position", {}), size = guiUDim2(p, "Size", {{0, 100}, {0, 100}});
        int64_t arc = gui_child(id, "UIAspectRatioConstraint"), szc = gui_child(id, "UISizeConstraint");
        g.pixelSized = arc || szc;
        if (g.pixelSized) {
            // A constraint needs the size in pixels: it is laid out from the parent's
            // size now, and again when that changes (layout_gui watches).
            Control* pc = n->get_parent_control();
            Vector2 ps = pc ? pc->get_size() : Vector2();
            g.parentSize = ps;
            Vector2 s(size.x.scale * ps.x + size.x.offset, size.y.scale * ps.y + size.y.offset);
            if (szc) {
                auto& cp = guis_[szc].props;
                Vec2 mn = guiVec2(cp, "MinSize", {0, 0}), mx = guiVec2(cp, "MaxSize", {INFINITY, INFINITY});
                s.x = std::clamp(s.x, mn.x, std::max(mn.x, mx.x)); s.y = std::clamp(s.y, mn.y, std::max(mn.y, mx.y));
            }
            if (arc) {
                auto& ap = guis_[arc].props;
                float ratio = std::max((float)guiNum(ap, "AspectRatio", 1), 1e-6f);
                if (guiEnum(ap, "AspectType", "FitWithinMaxSize") == "ScaleWithParentSize") {
                    if (guiEnum(ap, "DominantAxis", "Width") == "Width") s.y = s.x / ratio; else s.x = s.y * ratio;
                } else if (s.x / ratio <= s.y) s.y = s.x / ratio;   // the largest box of that shape within Size
                else s.x = s.y * ratio;
            }
            Vector2 at(pos.x.scale * ps.x + pos.x.offset - a.x * s.x, pos.y.scale * ps.y + pos.y.offset - a.y * s.y);
            for (int side = 0; side < 4; side++) n->set_anchor((Side)side, 0, false, false);
            n->set_offset(SIDE_LEFT, at.x); n->set_offset(SIDE_TOP, at.y);
            n->set_offset(SIDE_RIGHT, at.x + s.x); n->set_offset(SIDE_BOTTOM, at.y + s.y);
        } else {
            n->set_anchor(SIDE_LEFT, pos.x.scale - a.x * size.x.scale, false, false);
            n->set_anchor(SIDE_TOP, pos.y.scale - a.y * size.y.scale, false, false);
            n->set_anchor(SIDE_RIGHT, pos.x.scale + (1 - a.x) * size.x.scale, false, false);
            n->set_anchor(SIDE_BOTTOM, pos.y.scale + (1 - a.y) * size.y.scale, false, false);
            n->set_offset(SIDE_LEFT, pos.x.offset - a.x * size.x.offset);
            n->set_offset(SIDE_TOP, pos.y.offset - a.y * size.y.offset);
            n->set_offset(SIDE_RIGHT, pos.x.offset + (1 - a.x) * size.x.offset);
            n->set_offset(SIDE_BOTTOM, pos.y.offset + (1 - a.y) * size.y.offset);
        }
    }
    Vector2 sz = n->get_size();
    // A UIScale scales the object and everything in it about its AnchorPoint
    float scale = 1;
    if (int64_t sid = gui_child(id, "UIScale")) scale = std::max((float)guiNum(guis_[sid].props, "Scale", 1), 0.f);
    n->set_scale(Vector2(scale, scale));
    // background: BackgroundColor3 / BackgroundTransparency, the border, a UICorner, a UIStroke
    Ref<StyleBoxFlat> sb; sb.instantiate();
    Color bg = guiColor(p, "BackgroundColor3", kGuiBackground);
    if (g.button && guiBool(p, "AutoButtonColor", true) && (g.hovered || g.pressed)) bg = bg.darkened(g.pressed ? 0.25f : 0.12f);
    bg.a = 1 - (float)guiNum(p, "BackgroundTransparency", 0);
    sb->set_bg_color(bg);
    int border = bg.a <= 0 ? 0 : (int)guiNum(p, "BorderSizePixel", 1);
    Color borderColor = guiColor(p, "BorderColor3", kGuiBorder);
    if (int64_t sid = gui_child(id, "UIStroke")) {
        auto& sp = guis_[sid].props;
        if (guiBool(sp, "Enabled", true)) {
            border = (int)guiNum(sp, "Thickness", 1);
            borderColor = guiColor(sp, "Color", Color(0, 0, 0));
            borderColor.a = 1 - (float)guiNum(sp, "Transparency", 0);
        }
    }
    sb->set_border_width_all(border);
    sb->set_border_color(borderColor);
    if (int64_t cid = gui_child(id, "UICorner")) {
        UDim r = guiUDim(guis_[cid].props, "CornerRadius", {0, 8});
        sb->set_corner_radius_all((int)(r.scale * std::min(sz.x, sz.y) + r.offset));
    }
    n->add_theme_stylebox_override("panel", sb);
    n->set_visible(guiBool(p, "Visible", true));
    // A CanvasGroup's GroupColor3 and GroupTransparency tint it and everything under it.
    if (auto eit = entries_.find(id); eit != entries_.end() && eit->second.className == "CanvasGroup") {
        Color gc = guiColor(p, "GroupColor3", Color(1, 1, 1));
        gc.a = 1 - (float)guiNum(p, "GroupTransparency", 0);
        n->set_modulate(gc);
    }
    // A Modal button appearing or going changes whether the pointer is free.
    if (g.button && guiBool(p, "Modal", false)) mouseModeDirty_ = true;
    // ZIndex orders it among its siblings (Roblox's Sibling default: Godot's relative
    // z), or against every other object on the ScreenGui when it says Global.
    n->set_z_index((int)guiNum(p, "ZIndex", 1));
    n->set_z_as_relative(screen_gui_of(id) != "Global");
    n->set_clip_contents(guiBool(p, "ClipsDescendants", false));
    n->set_pivot_offset(scale != 1 ? Vector2(a.x * sz.x, a.y * sz.y) : sz / 2);
    n->set_rotation(Math::deg_to_rad((float)guiNum(p, "Rotation", 0)));
    // A previewed GUI is a picture: every click belongs to the editor behind it. A plugin's GUI
    // under CoreGui is live there, and its buttons press.
    bool pluginGui = false;
    for (int64_t a = id, hops = 0; coreGuiId_ && a != kNoParent && hops < 64; hops++) {
        auto ait = entries_.find(a);
        if (ait == entries_.end()) break;
        if (ait->second.parent == coreGuiId_) { pluginGui = true; break; }
        a = ait->second.parent;
    }
    // Roblox's rule, one rule and not two: a GuiObject that is Active -- and a GuiButton, which
    // always is -- receives the pointer AND sinks it; one that is not receives nothing and
    // blocks nothing. Godot's PASS hands the event up to the parent and never down to a sibling
    // behind, so it would block; anything inactive is IGNORE.
    n->set_mouse_filter(guiPreview_ && !pluginGui ? Control::MOUSE_FILTER_IGNORE
                        : g.button || guiBool(p, "Active", false) ? Control::MOUSE_FILTER_STOP
                        : Control::MOUSE_FILTER_IGNORE);
    // Selectable: whether a gamepad or the arrow keys can land on it. Roblox's default is false
    // for a plain Frame, true for a button.
    n->set_focus_mode(guiBool(p, "Selectable", g.button) ? Control::FOCUS_ALL : Control::FOCUS_NONE);
    // GuiButton.Selected is an instruction, not a description: setting it moves the gamepad
    // selection to that button.
    if (g.button && guiBool(p, "Selected", false) && !n->has_focus() && n->is_visible_in_tree()
        && n->get_focus_mode() != Control::FOCUS_NONE) n->grab_focus();
    // UIPadding insets where the children and the text go
    UDim l, t, r, b;
    if (int64_t pid = gui_child(id, "UIPadding")) {
        auto& pp = guis_[pid].props;
        l = guiUDim(pp, "PaddingLeft", {}); t = guiUDim(pp, "PaddingTop", {});
        r = guiUDim(pp, "PaddingRight", {}); b = guiUDim(pp, "PaddingBottom", {});
    }
    g.content->set_offset(SIDE_LEFT, l.scale * sz.x + l.offset);
    g.content->set_offset(SIDE_TOP, t.scale * sz.y + t.offset);
    g.content->set_offset(SIDE_RIGHT, -(r.scale * sz.x + r.offset));
    g.content->set_offset(SIDE_BOTTOM, -(b.scale * sz.y + b.offset));
    if (Control* box = box_node(g)) {
        // the TextBox's text is the box's own once it is up; the rest restyles
        Color tc = guiColor(p, "TextColor3", Color(0, 0, 0));
        tc.a = 1 - (float)guiNum(p, "TextTransparency", 0);
        Color pc = guiColor(p, "PlaceholderColor3", Color(178 / 255.f, 178 / 255.f, 178 / 255.f));
        box->add_theme_color_override("font_color", tc);
        box->add_theme_color_override("font_uneditable_color", tc);
        box->add_theme_color_override("font_placeholder_color", pc);
        box->add_theme_color_override("caret_color", tc);
        box->add_theme_font_size_override("font_size", (int)guiNum(p, "TextSize", 14));
        // A face still on its way is left alone: overriding with nothing is an engine error per
        // line, and the default face draws until cloud_arrived restyles what names the file.
        if (const Value* ff = guiProp(p, "FontFace"); ff && ff->type == Value::Font) { Ref<Font> face = load_font(*ff, gui_pixel_font(id)); if (face.is_valid()) box->add_theme_font_override("font", face); }
        bool editable = guiBool(p, "TextEditable", true);
        std::string ph = guiStr(p, "PlaceholderText", "");
        if (g.edit) {
            g.edit->set_placeholder(String::utf8(ph.c_str()));
            std::string xa = guiEnum(p, "TextXAlignment", "Center");
            g.edit->set_horizontal_alignment(xa == "Left" ? HORIZONTAL_ALIGNMENT_LEFT : xa == "Right" ? HORIZONTAL_ALIGNMENT_RIGHT : HORIZONTAL_ALIGNMENT_CENTER);
            g.edit->set_editable(editable);
        } else if (g.multi) {   // lines: no alignment of its own, and it wraps as Roblox's does
            g.multi->set_placeholder(String::utf8(ph.c_str()));
            g.multi->set_editable(editable);
            g.multi->set_line_wrapping_mode(guiBool(p, "TextWrapped", false) ? TextEdit::LINE_WRAPPING_BOUNDARY : TextEdit::LINE_WRAPPING_NONE);
        }
        box->set_mouse_filter(guiBool(p, "Visible", true) ? Control::MOUSE_FILTER_STOP : Control::MOUSE_FILTER_IGNORE);
        return;
    }
    if (g.scrolling && g.vbar) {
        // the bars: ScrollBarThickness wide, ScrollBarImageColor3, along the window's edges
        float t = (float)guiNum(p, "ScrollBarThickness", 12);
        Color bc = guiColor(p, "ScrollBarImageColor3", Color(0, 0, 0));
        bc.a = 1 - (float)guiNum(p, "ScrollBarImageTransparency", 0);
        Ref<StyleBoxFlat> track; track.instantiate(); track->set_bg_color(Color(bc.r, bc.g, bc.b, bc.a * 0.15f));
        // The grabber is Roblox's TopImage / MidImage / BottomImage: a cap, a middle that
        // stretches, a cap. Stacked into one texture and sliced back apart, because a stylebox
        // is one image with margins. With none of the three set, a flat rounded grabber.
        Ref<StyleBox> grab = scrollbar_grabber(p, t, bc);
        for (Control* bar : {(Control*)g.vbar, (Control*)g.hbar}) {
            for (const char* s : {"grabber", "grabber_highlight", "grabber_pressed"}) bar->add_theme_stylebox_override(s, grab);
            bar->add_theme_stylebox_override("scroll", track);
            bar->add_theme_stylebox_override("scroll_focus", track);
            bar->set_custom_minimum_size(Vector2(t, t));
        }
        // How much of the window each bar takes from the canvas: None floats it over the
        // content, ScrollBar takes its width while shown, Always takes it either way -- which
        // is what stops a list jumping sideways when it first grows long enough for a bar.
        std::string vi = guiEnum(p, "VerticalScrollBarInset", "None");
        std::string hi = guiEnum(p, "HorizontalScrollBarInset", "None");
        g.inset = Vector2(vi == "Always" || (vi == "ScrollBar" && g.vbar->is_visible()) ? t : 0,
                          hi == "Always" || (hi == "ScrollBar" && g.hbar->is_visible()) ? t : 0);
        bool left = guiEnum(p, "VerticalScrollBarPosition", "Right") == "Left";
        g.vbar->set_anchor(SIDE_LEFT, left ? 0 : 1, false, false); g.vbar->set_anchor(SIDE_RIGHT, left ? 0 : 1, false, false);
        g.vbar->set_anchor(SIDE_TOP, 0, false, false); g.vbar->set_anchor(SIDE_BOTTOM, 1, false, false);
        g.vbar->set_offset(SIDE_LEFT, left ? 0 : -t); g.vbar->set_offset(SIDE_RIGHT, left ? t : 0);
        g.vbar->set_offset(SIDE_TOP, 0); g.vbar->set_offset(SIDE_BOTTOM, 0);
        g.hbar->set_anchor(SIDE_LEFT, 0, false, false); g.hbar->set_anchor(SIDE_RIGHT, 1, false, false);
        g.hbar->set_anchor(SIDE_TOP, 1, false, false); g.hbar->set_anchor(SIDE_BOTTOM, 1, false, false);
        g.hbar->set_offset(SIDE_LEFT, 0); g.hbar->set_offset(SIDE_RIGHT, 0);
        g.hbar->set_offset(SIDE_TOP, -t); g.hbar->set_offset(SIDE_BOTTOM, 0);
        return;
    }
    if (g.pic) {
        // the image: Image (HoverImage / PressedImage on a button in those states), a
        // sprite of it by ImageRectOffset / ImageRectSize, tinted, fitted by ScaleType
        std::string img = guiStr(p, "Image", "");
        if (g.button && g.pressed) { std::string s = guiStr(p, "PressedImage", ""); if (!s.empty()) img = s; }
        else if (g.button && g.hovered) { std::string s = guiStr(p, "HoverImage", ""); if (!s.empty()) img = s; }
        Ref<Texture2D> tex = load_texture(img);
        g.picLoaded = tex.is_valid() ? 1 : 0;
        g.picSize = tex.is_valid() ? tex->get_size() : Vector2();
        Vec2 ro = guiVec2(p, "ImageRectOffset", {0, 0}), rs = guiVec2(p, "ImageRectSize", {0, 0});
        if (tex.is_valid() && rs.x > 0 && rs.y > 0) {
            Ref<AtlasTexture> at; at.instantiate();
            at->set_atlas(tex); at->set_region(Rect2(ro.x, ro.y, rs.x, rs.y));
            tex = at;
        }
        g.pic->set_texture(tex);
        Color ic = guiColor(p, "ImageColor3", Color(1, 1, 1));
        ic.a = 1 - (float)guiNum(p, "ImageTransparency", 0);
        g.pic->set_modulate(ic);
        std::string st = guiEnum(p, "ScaleType", "Stretch");
        g.pic->set_stretch_mode(st == "Fit" ? TextureRect::STRETCH_KEEP_ASPECT_CENTERED : st == "Crop" ? TextureRect::STRETCH_KEEP_ASPECT_COVERED
                                : st == "Tile" ? TextureRect::STRETCH_TILE : TextureRect::STRETCH_SCALE);
        g.pic->set_texture_filter(guiEnum(p, "ResampleMode", "Default") == "Pixelated" ? CanvasItem::TEXTURE_FILTER_NEAREST : CanvasItem::TEXTURE_FILTER_LINEAR);
        g.pic->set_visible(guiBool(p, "Visible", true));
    }
    if (!g.label) return;
    // text
    std::string text = guiStr(p, "Text", "");
    Color tc = guiColor(p, "TextColor3", Color(0, 0, 0));
    if (text.empty() && guiProp(p, "PlaceholderText")) {   // a TextBox showing its placeholder
        text = guiStr(p, "PlaceholderText", "");
        tc = guiColor(p, "PlaceholderColor3", Color(178 / 255.f, 178 / 255.f, 178 / 255.f));
    }
    tc.a = 1 - (float)guiNum(p, "TextTransparency", 0);
    g.label->set_text(String::utf8(text.c_str()));
    g.label->set_visible_characters((int)guiNum(p, "MaxVisibleGraphemes", -1));   // characters, not graphemes: Godot counts UTF-32 units
    g.label->add_theme_color_override("font_color", tc);
    if (!guiBool(p, "TextScaled", false)) { g.fontSize = 0; g.label->add_theme_font_size_override("font_size", (int)guiNum(p, "TextSize", 14)); }
    if (const Value* ff = guiProp(p, "FontFace"); ff && ff->type == Value::Font) { Ref<Font> face = load_font(*ff, gui_pixel_font(id)); if (face.is_valid()) g.label->add_theme_font_override("font", face); }
    std::string xa = guiEnum(p, "TextXAlignment", "Center"), ya = guiEnum(p, "TextYAlignment", "Center");
    g.label->set_horizontal_alignment(xa == "Left" ? HORIZONTAL_ALIGNMENT_LEFT : xa == "Right" ? HORIZONTAL_ALIGNMENT_RIGHT : HORIZONTAL_ALIGNMENT_CENTER);
    g.label->set_vertical_alignment(ya == "Top" ? VERTICAL_ALIGNMENT_TOP : ya == "Bottom" ? VERTICAL_ALIGNMENT_BOTTOM : VERTICAL_ALIGNMENT_CENTER);
    g.label->set_autowrap_mode(guiBool(p, "TextWrapped", false) || guiBool(p, "TextScaled", false) ? TextServer::AUTOWRAP_WORD_SMART : TextServer::AUTOWRAP_OFF);
    g.label->set_text_overrun_behavior(guiEnum(p, "TextTruncate", "None") == "None" ? TextServer::OVERRUN_NO_TRIMMING : TextServer::OVERRUN_TRIM_ELLIPSIS);
    float stroke = (float)guiNum(p, "TextStrokeTransparency", 1);
    Color sc = guiColor(p, "TextStrokeColor3", Color(0, 0, 0));
    sc.a = 1 - stroke;
    g.label->add_theme_constant_override("outline_size", stroke < 1 ? 2 : 0);
    g.label->add_theme_color_override("font_outline_color", sc);
}

// Every frame: UIListLayouts place their parents' children, TextScaled text fits
// its box, and AbsolutePosition / AbsoluteSize go back to the runtime.
void PulseBlockzWorld::layout_gui(std::vector<HostWrite>& writes) {
    if (guis_.empty()) return;
    const auto lt0 = std::chrono::steady_clock::now();
    for (auto& [id, g] : guis_) {
        g.wasListed = g.listed; g.listed = false;
        if (g.kind == GUI_BILLBOARD && g.node) place_billboard(id, g);
        else if (g.kind == GUI_SURFACE && g.viewport) place_surface(id, g);
    }
    const auto lt1 = std::chrono::steady_clock::now();
    int layouts = 0;
    for (auto& [lid, lg] : guis_) {
        if (lg.kind != GUI_MODIFIER) continue;
        auto leit = entries_.find(lid);
        if (leit == entries_.end()) continue;
        bool list = leit->second.className == "UIListLayout", grid = leit->second.className == "UIGridLayout";
        if (!list && !grid) continue;
        auto pit = guis_.find(leit->second.parent);
        if (pit == guis_.end() || !pit->second.node) continue;
        int64_t parentId = leit->second.parent;
        Control* area = gui_area(pit->second);
        Vector2 as = area->get_size();
        auto& lp = lg.props;
        bool vertical = guiEnum(lp, "FillDirection", list ? "Vertical" : "Horizontal") == "Vertical";
        bool byOrder = guiEnum(lp, "SortOrder", "Name") == "LayoutOrder";
        std::string ha = guiEnum(lp, "HorizontalAlignment", "Left"), va = guiEnum(lp, "VerticalAlignment", "Top");
        UDim pad = guiUDim(lp, "Padding", {});
        float padPx = pad.scale * (vertical ? as.y : as.x) + pad.offset;
        struct Item { int64_t id; Gui* g; std::string name; double order; int64_t made; Vector2 size; };
        std::vector<Item> items;
        for (int64_t cid : gui_kids(parentId)) {
            auto ceit = entries_.find(cid);
            if (ceit == entries_.end()) continue;
            const auto& e = ceit->second;
            auto git = guis_.find(cid);
            if (git == guis_.end() || git->second.kind != GUI_OBJECT || !git->second.node || !guiBool(git->second.props, "Visible", true)) continue;
            UDim2 s = guiUDim2(git->second.props, "Size", {{0, 100}, {0, 100}});
            items.push_back({cid, &git->second, e.name, guiNum(git->second.props, "LayoutOrder", 0), e.made,
                             Vector2(s.x.scale * as.x + s.x.offset, s.y.scale * as.y + s.y.offset)});
        }
        // Ties go to creation order, as Roblox breaks them: a layout sorts by LayoutOrder or by
        // Name and leaves siblings where they are. Ties are the normal case -- a grid of unnamed
        // Frames at LayoutOrder 0 is all ties -- and the walk above is over an unordered_map.
        std::stable_sort(items.begin(), items.end(), [&](const Item& a, const Item& b) {
            if (byOrder && a.order != b.order) return a.order < b.order;
            if (!byOrder && a.name != b.name) return a.name < b.name;
            return a.made < b.made;
        });
        std::vector<Rect2> rects(items.size());
        Vector2 content;
        if (list) {
            float along = 0, across = 0;
            for (auto& it : items) { along += vertical ? it.size.y : it.size.x; across = std::max(across, vertical ? it.size.x : it.size.y); }
            if (!items.empty()) along += padPx * (items.size() - 1);
            float total = vertical ? as.y : as.x;
            const std::string& alongAlign = vertical ? va : ha;
            float cursor = alongAlign == "Center" ? (total - along) / 2 : (alongAlign == "Bottom" || alongAlign == "Right") ? total - along : 0;
            for (size_t i = 0; i < items.size(); i++) {
                auto& it = items[i];
                const std::string& acrossAlign = vertical ? ha : va;
                float span = vertical ? as.x : as.y, w = vertical ? it.size.x : it.size.y;
                float off = acrossAlign == "Center" ? (span - w) / 2 : (acrossAlign == "Right" || acrossAlign == "Bottom") ? span - w : 0;
                rects[i] = vertical ? Rect2(off, cursor, it.size.x, it.size.y) : Rect2(cursor, off, it.size.x, it.size.y);
                cursor += (vertical ? it.size.y : it.size.x) + padPx;
            }
            content = vertical ? Vector2(across, along) : Vector2(along, across);
        } else {
            // UIGridLayout: every child a CellSize cell, CellPadding apart, filling
            // along FillDirection (as many as fit, or FillDirectionMaxCells) from
            // StartCorner; the block of cells aligned in the area.
            UDim2 cs = guiUDim2(lp, "CellSize", {{0, 100}, {0, 100}}), cp = guiUDim2(lp, "CellPadding", {{0, 5}, {0, 5}});
            Vector2 cell(cs.x.scale * as.x + cs.x.offset, cs.y.scale * as.y + cs.y.offset), gap(cp.x.scale * as.x + cp.x.offset, cp.y.scale * as.y + cp.y.offset);
            int maxCells = (int)guiNum(lp, "FillDirectionMaxCells", 0);
            float span = vertical ? as.y : as.x, step = (vertical ? cell.y : cell.x) + (vertical ? gap.y : gap.x);
            int perLine = step > 0 ? std::max(1, (int)std::floor((span + (vertical ? gap.y : gap.x) + 0.001f) / step)) : 1;
            if (maxCells > 0) perLine = std::min(perLine, maxCells);
            int n = (int)items.size(), lines = n ? (n + perLine - 1) / perLine : 0, alongCount = std::min(n, perLine);
            Vector2 count = vertical ? Vector2((float)lines, (float)alongCount) : Vector2((float)alongCount, (float)lines);   // columns, rows
            content = Vector2(count.x * cell.x + std::max(0.f, count.x - 1) * gap.x, count.y * cell.y + std::max(0.f, count.y - 1) * gap.y);
            std::string corner = guiEnum(lp, "StartCorner", "TopLeft");
            bool fromRight = corner == "TopRight" || corner == "BottomRight", fromBottom = corner == "BottomLeft" || corner == "BottomRight";
            Vector2 origin(ha == "Center" ? (as.x - content.x) / 2 : ha == "Right" ? as.x - content.x : 0,
                           va == "Center" ? (as.y - content.y) / 2 : va == "Bottom" ? as.y - content.y : 0);
            for (int i = 0; i < n; i++) {
                int line = i / perLine, at = i % perLine;
                int col = vertical ? line : at, row = vertical ? at : line;
                if (fromRight) col = (int)count.x - 1 - col;
                if (fromBottom) row = (int)count.y - 1 - row;
                rects[i] = Rect2(origin.x + col * (cell.x + gap.x), origin.y + row * (cell.y + gap.y), cell.x, cell.y);
            }
            if (count != lg.cellCount) { lg.cellCount = count; writes.push_back({lid, "AbsoluteCellCount", Value::vector2(count.x, count.y)}); }
            if (cell != lg.cellSize) { lg.cellSize = cell; writes.push_back({lid, "AbsoluteCellSize", Value::vector2(cell.x, cell.y)}); }
        }
        for (size_t i = 0; i < items.size(); i++) {
            auto& it = items[i];
            const Rect2& rect = rects[i];
            it.g->listed = true;
            if (it.g->wasListed && it.g->listRect == rect) continue;
            it.g->listRect = rect;
            Control* n = it.g->node;
            for (int side = 0; side < 4; side++) n->set_anchor((Side)side, 0, false, false);
            n->set_offset(SIDE_LEFT, rect.position.x); n->set_offset(SIDE_TOP, rect.position.y);
            n->set_offset(SIDE_RIGHT, rect.position.x + rect.size.x); n->set_offset(SIDE_BOTTOM, rect.position.y + rect.size.y);
        }
        if (content != lg.absSize) { lg.absSize = content; writes.push_back({lid, "AbsoluteContentSize", Value::vector2(content.x, content.y)}); }
        layouts++;
    }
    const auto lt2 = std::chrono::steady_clock::now();
    int styled = 0, fitted = 0, measured = 0;
    for (auto& [id, g] : guis_) {
        if (!g.node) continue;
        if (g.wasListed && !g.listed) { style_gui(id, g); styled++; }          // back to its own Position / Size
        if (g.pixelSized && !g.listed && g.kind == GUI_OBJECT) {   // a constraint's pixels follow the parent's size
            Control* pc = g.node->get_parent_control();
            if (pc && pc->get_size() != g.parentSize) style_gui(id, g);
        }
        Vector2 pos = g.node->get_global_position(), size = g.node->get_size();
        if (size != g.absSize && g.kind == GUI_OBJECT) style_gui(id, g);   // corner radius / padding / pivot follow the size
        if (g.scrolling && g.canvas) layout_scroll(id, g, writes);
        if (g.pic && g.picLoaded >= 0 && (g.picLoaded != g.sentPicLoaded || g.picSize != g.sentPicSize)) {
            g.sentPicLoaded = g.picLoaded; g.sentPicSize = g.picSize;
            writes.push_back({id, "IsLoaded", Value::boolean(g.picLoaded == 1)});
            writes.push_back({id, "ContentImageSize", Value::vector2(g.picSize.x, g.picSize.y)});
        }
        if (g.label && guiBool(g.props, "TextScaled", false)) {
            // The largest size, up to 100 as on Roblox, at which the text fits the box, measured
            // in the label's own font: the box's height alone is half a size out for a pixel
            // face with short glyphs. Refitted only when the box, text or face changes.
            Vector2 box = g.content->get_size().round();
            String text = g.label->get_text();
            Ref<Font> font = g.label->get_theme_font("font");
            uint64_t fontId = font.is_valid() ? (uint64_t)font->get_instance_id() : 0;
            if (box != g.fitBox || text != g.fitText || fontId != g.fitFont) {
                g.fitBox = box; g.fitText = text; g.fitFont = fontId; fitted++;
                double lo = 1, hi = 100;
                if (int64_t tc = gui_child(id, "UITextSizeConstraint")) { lo = guiNum(guis_[tc].props, "MinTextSize", 1); hi = std::max(lo, guiNum(guis_[tc].props, "MaxTextSize", 100)); }
                int fs = (int)lo;
                if (font.is_valid() && box.x > 0 && box.y > 0 && !text.is_empty()) {
                    // Measured at one reference size and scaled, never at the size being tried:
                    // measuring rasterises that size's glyphs, whose atlas page (megabytes) is
                    // kept for the run. Hinting is off, so a face measures linearly with size
                    // to within a pixel, and the wrap width is scaled the other way.
                    const int REF = 24;
                    auto fits = [&](int S) {
                        Vector2 need = font->get_multiline_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, box.x * REF / (float)S, REF);
                        return need.x * S / (float)REF <= box.x + 0.5f && need.y * S / (float)REF <= box.y + 0.5f;
                    };
                    // Unwrapped text scales with its size exactly, so one measurement guesses and
                    // a second confirms; only text that must wrap needs the search below.
                    Vector2 one = font->get_multiline_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, REF);
                    int guess = (int)std::floor(std::min(box.x / std::max(one.x, 1.f), box.y / std::max(one.y, 1.f)) * REF);
                    guess = std::clamp(guess, (int)lo, (int)hi);
                    if (fits(guess)) fs = guess;
                    else {
                        int a = (int)lo, b = guess;
                        while (a < b) {                    // the largest size whose wrapped text fits
                            int mid = (a + b + 1) / 2;
                            if (fits(mid)) a = mid; else b = mid - 1;
                        }
                        fs = a;
                    }
                } else {
                    int64_t len = std::max<int64_t>(1, text.length());
                    fs = (int)std::clamp(std::min(box.y * 0.75, box.x * 1.7 / (double)len), lo, hi);
                }
                // Sizes come in steps -- four above 24, two below, rounded down so the text still
                // fits -- because every distinct pixel size a font is drawn at costs an atlas
                // page of its own, megabytes each, kept for the run.
                if (fs > 24) fs = 24 + ((fs - 24) / 4) * 4;
                else fs = (fs / 2) * 2;
                fs = std::max(fs, (int)lo);
                if (fs != g.fontSize) { g.fontSize = fs; g.label->add_theme_font_size_override("font_size", fs); }
            }
        }
        // TextBounds and TextFits, as on Roblox: the size the text takes in its font, wrapped to
        // the box when it wraps. Measured only when the text, size, face or wrap width changes.
        if (g.label) {
            String text = g.label->get_text();
            Ref<Font> font = g.label->get_theme_font("font");
            const int fs = g.label->get_theme_font_size("font_size");
            const bool wrap = guiBool(g.props, "TextWrapped", false) || guiBool(g.props, "TextScaled", false);
            const float width = wrap ? g.content->get_size().x : -1.0f;
            const uint64_t fontId = font.is_valid() ? (uint64_t)font->get_instance_id() : 0;
            if (text != g.boundsText || fs != g.boundsFs || wrap != g.boundsWrap || width != g.boundsWidth || fontId != g.boundsFont) {
                g.boundsText = text; g.boundsFs = fs; g.boundsWrap = wrap; g.boundsWidth = width; g.boundsFont = fontId; measured++;
                // Measured at one reference size and scaled, never at the label's own: measuring
                // rasterises that size's glyphs, and a billboard's label is a new size most
                // frames the camera moves.
                const int REF = 24;
                Vector2 need;
                if (font.is_valid() && !text.is_empty()) {
                    const float k = fs > 0 ? (float)fs / (float)REF : 1.0f;
                    need = font->get_multiline_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, width > 0 ? width / k : -1.0f, REF) * k;
                }
                const Vector2 box = g.content->get_size();
                const int fits = (need.x <= box.x + 0.5f && need.y <= box.y + 0.5f) ? 1 : 0;
                if (need != g.sentBounds) { g.sentBounds = need; writes.push_back({id, "TextBounds", Value::vector2(need.x, need.y)}); }
                if (fits != g.sentFits) { g.sentFits = fits; writes.push_back({id, "TextFits", Value::boolean(fits == 1)}); }
            }
        }
        Vector2 gs = g.node->get_global_transform().get_scale();   // a UIScale above: AbsoluteSize is what is on screen
        if (g.kind == GUI_OBJECT) pos -= gui_screen_origin(id);   // from the ScreenGui's corner, under the topbar inset
        if (pos != g.absPos) { g.absPos = pos; writes.push_back({id, "AbsolutePosition", Value::vector2(pos.x, pos.y)}); }
        // Rotation accumulates down the tree, and AbsoluteRotation is the sum. Read off the
        // node rather than added up by hand, so it agrees with what is drawn.
        float rot = Math::rad_to_deg(g.node->get_global_transform().get_rotation());
        if (std::fabs(rot - g.absRot) > 0.01f) { g.absRot = rot; writes.push_back({id, "AbsoluteRotation", Value::number(rot)}); }
        if (size != g.absSize || gs != g.absScale) { g.absSize = size; g.absScale = gs; writes.push_back({id, "AbsoluteSize", Value::vector2(size.x * gs.x, size.y * gs.y)}); }
    }
    const auto lt3 = std::chrono::steady_clock::now();
    auto ms = [](auto a, auto b) { return (int)std::chrono::duration<double, std::milli>(b - a).count(); };
    layoutNote_ = " (billboards " + std::to_string(ms(lt0, lt1)) + " ms; " + std::to_string(layouts) + " list/grid layouts " + std::to_string(ms(lt1, lt2))
        + " ms; objects " + std::to_string(ms(lt2, lt3)) + " ms: " + std::to_string(styled) + " restyled, " + std::to_string(fitted) + " fitted, " + std::to_string(measured) + " measured)";
}

Control* PulseBlockzWorld::gui_area(Gui& g) { return g.canvas ? g.canvas : g.kind == GUI_OBJECT ? g.content : g.node; }

// A scrollbar's grabber out of Roblox's TopImage / MidImage / BottomImage: a cap, a stretching
// middle, a cap. A Godot stylebox is one texture with margins, so the three are stacked into one
// image and the margins cut them apart. Any may be missing; with none, the flat rounded grabber.
Ref<StyleBox> PulseBlockzWorld::scrollbar_grabber(const std::unordered_map<std::string, Value>& p,
                                                float thickness, const Color& tint) {
    const std::string top = guiStr(p, "TopImage", "");
    const std::string mid = guiStr(p, "MidImage", "");
    const std::string bottom = guiStr(p, "BottomImage", "");
    if (top.empty() && mid.empty() && bottom.empty()) {
        Ref<StyleBoxFlat> flat; flat.instantiate();
        flat->set_bg_color(tint);
        flat->set_corner_radius_all((int)(thickness / 3));
        return flat;
    }
    Ref<Texture2D> tt = top.empty() ? Ref<Texture2D>() : load_texture(top);
    Ref<Texture2D> mt = mid.empty() ? Ref<Texture2D>() : load_texture(mid);
    Ref<Texture2D> bt = bottom.empty() ? Ref<Texture2D>() : load_texture(bottom);
    Ref<Image> ti = tt.is_valid() ? tt->get_image() : Ref<Image>();
    Ref<Image> mi = mt.is_valid() ? mt->get_image() : Ref<Image>();
    Ref<Image> bi = bt.is_valid() ? bt->get_image() : Ref<Image>();
    // Still arriving, or none of them decoded: the flat one until they do.
    if (ti.is_null() && mi.is_null() && bi.is_null()) {
        Ref<StyleBoxFlat> flat; flat.instantiate();
        flat->set_bg_color(tint);
        flat->set_corner_radius_all((int)(thickness / 3));
        return flat;
    }
    int w = 0;
    for (const Ref<Image>& im : {ti, mi, bi}) if (im.is_valid()) w = std::max(w, im->get_width());
    if (w <= 0) w = (int)std::max(thickness, 1.0f);
    auto fitted = [&](const Ref<Image>& im, int fallbackH) -> Ref<Image> {
        if (im.is_null()) return Ref<Image>();
        Ref<Image> c = Image::create_from_data(im->get_width(), im->get_height(), false, im->get_format(), im->get_data());
        if (c.is_null()) return Ref<Image>();
        c->convert(Image::FORMAT_RGBA8);
        if (c->get_width() != w) c->resize(w, std::max(c->get_height(), 1), Image::INTERPOLATE_BILINEAR);
        (void)fallbackH;
        return c;
    };
    Ref<Image> a = fitted(ti, 0), b = fitted(mi, 0), c = fitted(bi, 0);
    const int ah = a.is_valid() ? a->get_height() : 0;
    const int bh = b.is_valid() ? b->get_height() : 1;
    const int ch = c.is_valid() ? c->get_height() : 0;
    Ref<Image> whole = Image::create(w, std::max(ah + bh + ch, 1), false, Image::FORMAT_RGBA8);
    if (whole.is_null()) return Ref<StyleBox>();
    whole->fill(Color(0, 0, 0, 0));
    if (a.is_valid()) whole->blit_rect(a, Rect2i(0, 0, w, ah), Point2i(0, 0));
    if (b.is_valid()) whole->blit_rect(b, Rect2i(0, 0, w, bh), Point2i(0, ah));
    if (c.is_valid()) whole->blit_rect(c, Rect2i(0, 0, w, ch), Point2i(0, ah + bh));
    Ref<StyleBoxTexture> sb; sb.instantiate();
    sb->set_texture(ImageTexture::create_from_image(whole));
    note_texture("window frame", whole->get_width(), whole->get_height());
    // The caps do not stretch; the middle does. That is the whole point of three images.
    sb->set_texture_margin(SIDE_TOP, (float)ah);
    sb->set_texture_margin(SIDE_BOTTOM, (float)ch);
    sb->set_modulate(tint);
    return sb;
}

// ---- ScrollingFrames ---------------------------------------------------------------
// Each frame: the canvas is CanvasSize (relative to the window) or, with
// AutomaticCanvasSize, as far as the children reach; CanvasPosition stays within it;
// the bars show along the axes that scroll, and what changed goes back to the runtime.
void PulseBlockzWorld::layout_scroll(int64_t id, Gui& g, std::vector<HostWrite>& writes) {
    auto& p = g.props;
    // What the bars leave: an inset bar takes its own thickness off the window, so a child
    // sized in Scale is a fraction of what is actually visible.
    Vector2 window = g.content->get_size() - g.inset;
    window = Vector2(std::max(window.x, 0.f), std::max(window.y, 0.f));
    UDim2 cs = guiUDim2(p, "CanvasSize", {});
    Vector2 canvas(cs.x.scale * window.x + cs.x.offset, cs.y.scale * window.y + cs.y.offset);
    std::string autoSize = guiEnum(p, "AutomaticCanvasSize", "None");
    if (autoSize != "None") {
        Vector2 reach;
        for (int64_t cid : gui_kids(id)) {
            auto git = guis_.find(cid);
            if (git == guis_.end() || git->second.kind != GUI_OBJECT || !git->second.node || !guiBool(git->second.props, "Visible", true)) continue;
            Vector2 end = git->second.node->get_position() + git->second.node->get_size() * git->second.node->get_scale();
            reach = Vector2(std::max(reach.x, end.x), std::max(reach.y, end.y));
        }
        if (autoSize == "X" || autoSize == "XY") canvas.x = std::max(canvas.x, reach.x);
        if (autoSize == "Y" || autoSize == "XY") canvas.y = std::max(canvas.y, reach.y);
    }
    canvas = Vector2(std::max(canvas.x, window.x), std::max(canvas.y, window.y));   // never smaller than the window
    g.canvasSize = canvas; g.windowSize = window;
    bool enabled = guiBool(p, "ScrollingEnabled", true);
    std::string dir = guiEnum(p, "ScrollingDirection", "XY");
    bool x = enabled && dir != "Y" && canvas.x > window.x + 0.5f, y = enabled && dir != "X" && canvas.y > window.y + 0.5f;
    g.vbar->set_visible(y); g.hbar->set_visible(x);
    g.vbar->set_max(canvas.y); g.vbar->set_page(window.y);
    g.hbar->set_max(canvas.x); g.hbar->set_page(window.x);
    scroll_to(g, g.canvasPos);
    if (g.canvasPos != g.sentCanvasPos) { g.sentCanvasPos = g.canvasPos; writes.push_back({id, "CanvasPosition", Value::vector2(g.canvasPos.x, g.canvasPos.y)}); }
    if (canvas != g.sentCanvasSize) { g.sentCanvasSize = canvas; writes.push_back({id, "AbsoluteCanvasSize", Value::vector2(canvas.x, canvas.y)}); }
    if (window != g.sentWindowSize) { g.sentWindowSize = window; writes.push_back({id, "AbsoluteWindowSize", Value::vector2(window.x, window.y)}); }
}

void PulseBlockzWorld::scroll_to(Gui& g, Vector2 pos) {
    if (!g.canvas) return;
    pos.x = std::clamp(pos.x, 0.f, std::max(0.f, g.canvasSize.x - g.windowSize.x));
    pos.y = std::clamp(pos.y, 0.f, std::max(0.f, g.canvasSize.y - g.windowSize.y));
    g.canvasPos = pos;
    g.canvas->set_offset(SIDE_LEFT, -pos.x); g.canvas->set_offset(SIDE_RIGHT, -pos.x - g.inset.x);
    g.canvas->set_offset(SIDE_TOP, -pos.y); g.canvas->set_offset(SIDE_BOTTOM, -pos.y - g.inset.y);
    if (g.vbar) g.vbar->set_value_no_signal(pos.y);
    if (g.hbar) g.hbar->set_value_no_signal(pos.x);
}

void PulseBlockzWorld::_on_scroll_bar(double, int64_t id) {
    auto git = guis_.find(id);
    if (git == guis_.end() || !git->second.vbar) return;
    scroll_to(git->second, Vector2((float)git->second.hbar->get_value(), (float)git->second.vbar->get_value()));
}

// An event a Control received, put back into screen coordinates. Godot hands _gui_input a copy
// whose position is LOCAL to the control, which would reach Runtime::input as the mouse position
// and leave UserInputService:GetMouseLocation answering from inside the last control clicked.
static Ref<InputEvent> in_screen_space(const Ref<InputEvent>& event, Control* node) {
    if (!node) return event;
    return event->xformed_by(node->get_global_transform());
}

// A mouse button on a GuiObject: MouseButton1Down / Up, then Click for a button released where
// it was pressed; UserInputService still hears it, as processed.
void PulseBlockzWorld::_on_gui_input(const Ref<InputEvent>& event, int64_t id) {
    auto git = guis_.find(id);
    if (git == guis_.end() || !git->second.node) return;
    Gui& g = git->second;
    bool sinks = g.node->get_mouse_filter() == Control::MOUSE_FILTER_STOP;
    auto* mb = Object::cast_to<InputEventMouseButton>(event.ptr());
    // Motion is reported from _input, whatever it lands on, so a sinking control must not
    // send it a second time.
    if (!mb) {
        if (sinks && !Object::cast_to<InputEventMouseMotion>(event.ptr())) feed_input(in_screen_space(event, g.node), true);
        return;
    }
    MouseButton idx = mb->get_button_index();
    if (idx >= MOUSE_BUTTON_WHEEL_UP && idx <= MOUSE_BUTTON_WHEEL_RIGHT) {
        // MouseWheelForward / MouseWheelBackward on the object under the pointer
        if (mb->is_pressed() && (idx == MOUSE_BUTTON_WHEEL_UP || idx == MOUSE_BUTTON_WHEEL_DOWN)) {
            Vector2 at = g.node->get_global_transform().xform(mb->get_position());
            Vec2 pos{(float)at.x, (float)at.y};
            const char* phase = idx == MOUSE_BUTTON_WHEEL_UP ? "WheelForward" : "WheelBackward";
            (client_ && !editMode_ ? clientJobs_ : jobs_).push_back([id, phase, pos](Runtime& rt) { rt.guiInput(id, phase, 0, pos); });
        }
        // the wheel scrolls the ScrollingFrame this is in (Shift: sideways)
        if (mb->is_pressed())
            for (int64_t a = id, hops = 0; hops < 64; hops++) {
                auto ait = guis_.find(a);
                if (ait != guis_.end() && ait->second.scrolling && ait->second.canvas && guiBool(ait->second.props, "ScrollingEnabled", true)) {
                    float step = 40 * (mb->get_factor() > 0 ? mb->get_factor() : 1);
                    Vector2 d = idx == MOUSE_BUTTON_WHEEL_UP ? Vector2(0, -step) : idx == MOUSE_BUTTON_WHEEL_DOWN ? Vector2(0, step) : idx == MOUSE_BUTTON_WHEEL_LEFT ? Vector2(-step, 0) : Vector2(step, 0);
                    if (mb->is_shift_pressed()) d = Vector2(d.y, d.x);
                    std::string dir = guiEnum(ait->second.props, "ScrollingDirection", "XY");
                    if (dir == "X") d.y = 0; else if (dir == "Y") d.x = 0;
                    scroll_to(ait->second, ait->second.canvasPos + d);
                    g.node->accept_event();
                    return;
                }
                auto eit = entries_.find(a);
                if (eit == entries_.end()) break;
                a = eit->second.parent;
            }
        if (sinks) feed_input(in_screen_space(event, g.node), true);
        return;
    }
    int button = idx == MOUSE_BUTTON_LEFT ? 1 : idx == MOUSE_BUTTON_RIGHT ? 2 : 0;
    if (!button) { if (sinks) feed_input(in_screen_space(event, g.node), true); return; }
    Vector2 at = g.node->get_global_transform().xform(mb->get_position());
    Vec2 pos{(float)at.x, (float)at.y};
    if (mb->is_pressed() && typingBox_ && typingBox_ != id) release_typing();   // a click elsewhere ends the typing
    // Clicking a button does not take the keyboard: Roblox's Selectable is about GAMEPAD
    // selection, and a mouse click on a TextButton never moves keyboard focus off the game,
    // while a focused Godot control swallows the keys. `Selected` still grabs focus for a
    // gamepad, which is deliberate; a TextBox is left alone, since typing is what it is for.
    if (mb->is_pressed() && !g.edit && !g.multi) {
        Viewport* vp = g.node->get_viewport();
        Control* held = vp ? vp->gui_get_focus_owner() : nullptr;
        if (held && (held == g.node || g.node->is_ancestor_of(held))) held->release_focus();
    }
    // to the client's runtime -- or, in the edit world, the server that runs the plugins whose GUIs these are
    auto fire = [&](const char* phase) { (client_ && !editMode_ ? clientJobs_ : jobs_).push_back([id, phase, button, pos](Runtime& rt) { rt.guiInput(id, phase, button, pos); }); };
    // A Click is a press and a release on the same control, one per button: MouseButton1Click
    // and MouseButton2Click. `Activated` stays left-only, which is Roblox's rule.
    if (mb->is_pressed()) {
        fire("Down");
        (button == 1 ? g.pressed : g.pressed2) = true;
    } else {
        fire("Up");
        if (g.button && (button == 1 ? g.pressed : g.pressed2)) fire("Click");
        (button == 1 ? g.pressed : g.pressed2) = false;
    }
    if (g.button) style_gui(id, g);
    if (sinks) feed_input(in_screen_space(event, g.node), true);
}

// A TextBox is a LineEdit, or a TextEdit when it is MultiLine. Roblox counts the
// cursor in characters over the whole text either way, from 1.
std::string PulseBlockzWorld::box_text(const Gui& g) const {
    if (g.edit) return g.edit->get_text().utf8().get_data();
    if (g.multi) return g.multi->get_text().utf8().get_data();
    return "";
}
int PulseBlockzWorld::box_caret(const Gui& g) const {
    if (g.edit) return g.edit->get_caret_column() + 1;
    if (!g.multi) return 1;
    int at = 0;
    for (int line = 0; line < g.multi->get_caret_line(); line++) at += (int)g.multi->get_line(line).length() + 1;
    return at + g.multi->get_caret_column() + 1;
}
void PulseBlockzWorld::set_box_text(Gui& g, const std::string& text, int caret) {
    String t = String::utf8(text.c_str());
    if (g.edit) {
        if (g.edit->get_text() != t) g.edit->set_text(t);
        if (caret >= 1) g.edit->set_caret_column(std::min(caret - 1, (int)t.length()));
        return;
    }
    if (!g.multi) return;
    if (g.multi->get_text() != t) g.multi->set_text(t);
    if (caret < 1) return;
    int left = caret - 1;
    for (int line = 0; line < g.multi->get_line_count(); line++) {
        int len = (int)g.multi->get_line(line).length();
        if (left <= len || line == g.multi->get_line_count() - 1) {
            g.multi->set_caret_line(line);
            g.multi->set_caret_column(std::min(left, len));
            return;
        }
        left -= len + 1;
    }
}

// The TextBox's editor: focus in / out become Focused / FocusLost on the runtime's box, each
// edit its Text and CursorPosition (as the engine's writes), Enter submits -- FocusLost(true) --
// and Escape lets go. The keys it eats still reach UserInputService, as processed.
void PulseBlockzWorld::_on_edit_focus(bool in, int64_t id) {
    auto git = guis_.find(id);
    if (git == guis_.end() || !box_node(git->second)) return;
    Gui& g = git->second;
    if (in) {
        if (typingBox_ && typingBox_ != id) release_typing();
        typingBox_ = id; g.submitted = false;
        if (chatTyping_ && chatInput_) chatInput_->release_focus();
        clientJobs_.push_back([id](Runtime& rt) { rt.guiText(id, "Focus"); });
    } else {
        if (typingBox_ == id) typingBox_ = 0;
        bool enter = g.submitted; g.submitted = false;
        clientJobs_.push_back([id, enter](Runtime& rt) { rt.guiText(id, "Blur", "", -1, enter); });
    }
}
void PulseBlockzWorld::_on_edit_changed(const String& text, int64_t id) {
    auto git = guis_.find(id);
    if (git == guis_.end() || !git->second.edit) return;
    std::string t = text.utf8().get_data();
    int caret = box_caret(git->second);
    clientJobs_.push_back([id, t, caret](Runtime& rt) { rt.guiText(id, "Change", t, caret); });
}
void PulseBlockzWorld::_on_multi_changed(int64_t id) {
    auto git = guis_.find(id);
    if (git == guis_.end() || !git->second.multi) return;
    std::string t = box_text(git->second);
    int caret = box_caret(git->second);
    clientJobs_.push_back([id, t, caret](Runtime& rt) { rt.guiText(id, "Change", t, caret); });
}
void PulseBlockzWorld::_on_edit_submitted(const String&, int64_t id) {
    auto git = guis_.find(id);
    if (git == guis_.end() || !git->second.edit) return;
    git->second.submitted = true;
    git->second.edit->release_focus();   // a MultiLine box takes the Enter as a newline instead
}
void PulseBlockzWorld::_on_edit_input(const Ref<InputEvent>& event, int64_t id) {
    auto git = guis_.find(id);
    if (git == guis_.end() || !box_node(git->second)) return;
    Gui& g = git->second;
    Control* box = box_node(g);
    if (auto* k = Object::cast_to<InputEventKey>(event.ptr())) {
        if (k->is_pressed() && !k->is_echo() && k->get_keycode() == KEY_ESCAPE && box->has_focus()) { box->release_focus(); box->accept_event(); return; }
        feed_input(event, true);
    } else if (auto* mb = Object::cast_to<InputEventMouseButton>(event.ptr()); mb && g.node) {
        Ref<InputEventMouseButton> e = mb->duplicate();
        e->set_position(mb->get_position() + box->get_global_position() - g.node->get_global_position());
        _on_gui_input(e, id);
    }
}
void PulseBlockzWorld::release_typing() {
    if (!typingBox_) return;
    auto git = guis_.find(typingBox_);
    if (git != guis_.end()) if (Control* box = box_node(git->second); box && box->has_focus()) box->release_focus();
    typingBox_ = 0;
}

void PulseBlockzWorld::_on_gui_mouse(bool entered, int64_t id) {
    auto git = guis_.find(id);
    if (git == guis_.end() || !git->second.node) return;
    Gui& g = git->second;
    g.hovered = entered;
    // Leaving drops both buttons: releasing outside the control is not a click on it.
    if (!entered) { g.pressed = false; g.pressed2 = false; }
    Vector2 at = get_viewport()->get_mouse_position();
    Vec2 pos{(float)at.x, (float)at.y};
    (client_ && !editMode_ ? clientJobs_ : jobs_).push_back([id, entered, pos](Runtime& rt) { rt.guiInput(id, entered ? "Enter" : "Leave", 0, pos); });
    if (g.button) style_gui(id, g);
}

void PulseBlockzWorld::apply_lighting(const Change& c) {
    const std::string& n = c.name;
    LightingState& l = lighting_;
    if (n == "ClockTime") l.clockTime = c.value.n;
    else if (n == "Brightness") l.brightness = c.value.n;
    else if (n == "Ambient") l.ambient = {c.value.c.r, c.value.c.g, c.value.c.b};
    else if (n == "OutdoorAmbient") l.outdoorAmbient = {c.value.c.r, c.value.c.g, c.value.c.b};
    else if (n == "FogColor") l.fogColor = {c.value.c.r, c.value.c.g, c.value.c.b};
    else if (n == "FogStart") l.fogStart = c.value.n;
    else if (n == "FogEnd") l.fogEnd = c.value.n;
    else if (n == "ExposureCompensation") l.exposure = c.value.n;
    else if (n == "EnvironmentDiffuseScale") l.envDiffuse = c.value.n;
    else if (n == "EnvironmentSpecularScale") l.envSpecular = c.value.n;
    else if (n == "GlobalShadows") l.shadows = c.value.b;
    else return;   // TimeOfDay is ClockTime's echo; the rest has no rendering here
    update_lighting();
}

// The Lighting service as Godot sees it. ClockTime turns the sun: 6 is sunrise in
// the east (+X), 12 overhead, 18 sunset in the west; at night it is below the
// horizon and off, with the sky and ambient dimmed to match.
void PulseBlockzWorld::update_lighting() {
    if (!sun_ || !env_) return;
    const LightingState& l = lighting_;
    double a = (l.clockTime - 6.0) / 12.0 * Math_PI;
    Vector3 toSun((float)std::cos(a), (float)std::sin(a), 0);
    double daylight = std::clamp(std::sin(a) * 2.0, 0.0, 1.0);
    Vector3 up = std::fabs(toSun.y) > 0.999f ? Vector3(0, 0, 1) : Vector3(0, 1, 0);
    Transform3D t(Basis::looking_at(-toSun, up), Vector3(0, 5, 0));
    sun_->set_transform(t);
    sun_->set_param(Light3D::PARAM_ENERGY, (float)(l.brightness / 3.0 * daylight));
    sun_->set_shadow(l.shadows && qualityLevel_ >= 4);   // GlobalShadows is the place's; whether to pay for them is the person's
    sun_->set_visible(daylight > 0);
    Ref<Environment> e = env_->get_environment();
    if (e.is_null()) return;
    Color amb(std::max(l.ambient.x, l.outdoorAmbient.x), std::max(l.ambient.y, l.outdoorAmbient.y), std::max(l.ambient.z, l.outdoorAmbient.z));
    e->set_ambient_light_color(amb);
    e->set_ambient_light_energy((float)(0.3 + 0.7 * daylight));
    // Roblox lights the shade twice: OutdoorAmbient, and the sky itself scaled by
    // EnvironmentDiffuseScale. Godot has one ambient, mixed between a flat colour and the sky by
    // sky_contribution, so the scale drives that mix. It matters most where the two disagree
    // hardest: a place on the moon has a black sky above and a brilliant white ground below, and
    // lit from a flat grey alone its shaded faces come out near black while Roblox's do not.
    e->set_ambient_source(l.envDiffuse > 0.0 ? Environment::AMBIENT_SOURCE_BG : Environment::AMBIENT_SOURCE_COLOR);
    e->set_ambient_light_sky_contribution((float)std::clamp(l.envDiffuse, 0.0, 1.0));
    e->set_reflection_source(l.envSpecular > 0.0 ? Environment::REFLECTION_SOURCE_BG : Environment::REFLECTION_SOURCE_DISABLED);
    Ref<Sky> sky = e->get_sky();
    if (sky.is_valid()) {
        Ref<ProceduralSkyMaterial> m = sky->get_material();
        if (m.is_valid()) m->set_energy_multiplier((float)(0.05 + 0.95 * daylight));
    }
    update_sky_bodies();   // the sun and the moon follow the same clock the light does
    e->set_tonemap_exposure((float)std::pow(2.0, l.exposure));
    // An Atmosphere replaces the FogStart/FogEnd fog: Density is how far one sees, Color the
    // haze's tint, Haze how much of the sky it takes, Glare the sun's bleed into it.
    const Effect* atmo = nullptr;
    for (auto& [id, fx] : effects_) if (fx.className == "Atmosphere" && effect_active(id)) { atmo = &fx; break; }
    if (atmo) {
        auto num = [&](const char* k) { auto it = atmo->props.find(k); return it == atmo->props.end() ? 0.0 : it->second.n; };
        auto col = [&](const char* k) { auto it = atmo->props.find(k); return it == atmo->props.end() ? Color(1, 1, 1) : Color(it->second.c.r, it->second.c.g, it->second.c.b); };
        // Roblox's Density is steeply non-linear: the default 0.395 is a light haze, 1 is
        // opaque within a few dozen studs. 0.06 * Density^5 in Godot's per-stud exponential
        // matches it (0.395 -> 25% at 500 studs, 69% at 2000).
        double density = std::clamp(num("Density"), 0.0, 1.0);
        e->set_fog_enabled(density > 0);
        e->set_fog_mode(Environment::FOG_MODE_EXPONENTIAL);
        e->set_fog_density((float)(0.06 * std::pow(density, 5.0)));
        e->set_fog_light_color(col("Color"));
        e->set_fog_sky_affect((float)std::clamp(num("Haze") / 10.0, 0.0, 1.0));
        e->set_fog_sun_scatter((float)std::clamp(num("Glare") / 10.0, 0.0, 1.0));
        e->set_fog_aerial_perspective(0.5f);
        return;
    }
    bool fog = l.fogEnd < 100000;
    e->set_fog_enabled(fog);
    e->set_fog_mode(Environment::FOG_MODE_DEPTH);
    e->set_fog_sky_affect(1);
    e->set_fog_sun_scatter(0);
    e->set_fog_aerial_perspective(0);
    if (fog) {
        e->set_fog_depth_begin((float)l.fogStart);
        e->set_fog_depth_end((float)l.fogEnd);
        e->set_fog_light_color(Color(l.fogColor.x, l.fogColor.y, l.fogColor.z));
    }
}

// An effect counts when it is Enabled and a child of Lighting or of the camera,
// as in Studio (a Sky or Atmosphere has no Enabled; being there is enough).
bool PulseBlockzWorld::effect_active(int64_t id) const {
    auto eit = entries_.find(id);
    auto fit = effects_.find(id);
    if (eit == entries_.end() || fit == effects_.end()) return false;
    if (eit->second.parent != lightingId_ && (eit->second.parent != cameraId_ || !cameraId_)) return false;
    auto en = fit->second.props.find("Enabled");
    return en == fit->second.props.end() || en->second.type != Value::Bool || en->second.b;
}

// How much brighter than its colour Neon is drawn, so a BloomEffect finds it first. Brighter
// bleaches leaves to pastel under a bloom and flares lamps; dimmer barely glows.
static constexpr float kNeonHdr = 1.6f;

// Lighting's effects as Godot sees them: a BloomEffect is the Environment's glow, a
// ColorCorrectionEffect its adjustments (the tint as a black-to-tint ramp), a DepthOfFieldEffect
// the camera's depth of field, a BlurEffect a screen-sized quad re-sampling the frame through
// its mipmaps, a SunRaysEffect volumetric fog, an Atmosphere the fog and a Sky the sun's size.
// Of one kind the first child of Lighting wins, as in Studio.
void PulseBlockzWorld::sync_effects() {
    effectsDirty_ = false;
    if (!env_) return;
    Ref<Environment> e = env_->get_environment();
    if (e.is_null()) return;
    std::map<std::string, const Effect*> active;   // by class, the first active one
    for (auto& [id, fx] : effects_) if (effect_active(id) && !active.count(fx.className)) active[fx.className] = &fx;
    auto num = [](const Effect* fx, const char* k, double d) { auto it = fx->props.find(k); return it == fx->props.end() ? d : it->second.n; };

    const Effect* bloom = active.count("BloomEffect") ? active["BloomEffect"] : nullptr;
    // A BloomEffect is the place's bloom, one setting for the whole place. Without one, Neon
    // still glows above quality 7, as on Roblox: a glow only what is drawn past white reaches,
    // which is Neon at kNeonHdr. Additive; Godot's soft light all but vanishes on a bright scene.
    e->set_glow_enabled(bloom != nullptr ? qualityLevel_ >= 3 : qualityLevel_ >= 8);
    e->set_glow_blend_mode(Environment::GLOW_BLEND_MODE_ADDITIVE);
    if (!bloom) {
        e->set_glow_intensity(0.8f);
        e->set_glow_hdr_bleed_threshold(1.0f);
        for (int i = 0; i < 7; i++) e->set_glow_level(i, i < 3 ? 1.0f : 0.0f);
    }
    if (bloom) {
        // Intensity 0.4 and Size 24 are Studio's defaults: a soft halo on what is
        // brighter than Threshold. Godot blurs in mip levels; Size picks how many.
        e->set_glow_intensity((float)std::clamp(num(bloom, "Intensity", 0.4) * 2.0, 0.0, 8.0));
        e->set_glow_hdr_bleed_threshold((float)std::clamp(num(bloom, "Threshold", 0.95), 0.0, 4.0));
        int levels = std::clamp((int)std::ceil(num(bloom, "Size", 24) / 8.0), 1, 7);
        for (int i = 0; i < 7; i++) e->set_glow_level(i, i < levels ? 1.0f : 0.0f);   // Godot's levels are 0..6
    }

    const Effect* cc = active.count("ColorCorrectionEffect") ? active["ColorCorrectionEffect"] : nullptr;
    e->set_adjustment_enabled(cc != nullptr);
    if (cc) {
        // Studio's are offsets around 0; Godot's are factors around 1.
        e->set_adjustment_brightness((float)std::clamp(1.0 + num(cc, "Brightness", 0), 0.0, 8.0));
        e->set_adjustment_contrast((float)std::clamp(1.0 + num(cc, "Contrast", 0), 0.0, 8.0));
        e->set_adjustment_saturation((float)std::clamp(1.0 + num(cc, "Saturation", 0), 0.0, 8.0));
        auto tit = cc->props.find("TintColor");
        Color tint = tit == cc->props.end() || tit->second.type != Value::Color3 ? Color(1, 1, 1) : Color(tit->second.c.r, tit->second.c.g, tit->second.c.b);
        if (tint == Color(1, 1, 1)) e->set_adjustment_color_correction(Ref<Texture>());
        else {
            Ref<Gradient> g; g.instantiate();
            g->set_color(0, Color(0, 0, 0));
            g->set_color(1, tint);
            Ref<GradientTexture1D> t; t.instantiate();
            t->set_gradient(g);
            e->set_adjustment_color_correction(t);
        }
    }

    if (camera_) {
        const Effect* dof = active.count("DepthOfFieldEffect") ? active["DepthOfFieldEffect"] : nullptr;
        Ref<CameraAttributesPractical> attrs = camera_->get_attributes();
        if (dof && attrs.is_null()) { attrs.instantiate(); camera_->set_attributes(attrs); }
        if (attrs.is_valid()) {
            // Everything within InFocusRadius of FocusDistance is sharp; the blur
            // ramps over that radius on each side, as strong as the intensities.
            double focus = dof ? num(dof, "FocusDistance", 0.05) : 0.05, radius = dof ? std::max(num(dof, "InFocusRadius", 10), 0.01) : 10;
            double farI = dof ? std::clamp(num(dof, "FarIntensity", 0.75), 0.0, 1.0) : 0, nearI = dof ? std::clamp(num(dof, "NearIntensity", 0.75), 0.0, 1.0) : 0;
            attrs->set_dof_blur_far_enabled(dof && farI > 0);
            attrs->set_dof_blur_far_distance((float)(focus + radius));
            attrs->set_dof_blur_far_transition((float)radius);
            attrs->set_dof_blur_near_enabled(dof && nearI > 0 && focus - radius > 0);
            attrs->set_dof_blur_near_distance((float)std::max(focus - radius, 0.0));
            attrs->set_dof_blur_near_transition((float)radius);
            attrs->set_dof_blur_amount((float)(std::max(farI, nearI) * 0.3));
        }
    }

    const Effect* blur = active.count("BlurEffect") ? active["BlurEffect"] : nullptr;
    double blurSize = blur ? std::clamp(num(blur, "Size", 24), 0.0, 56.0) : 0;
    if (blurSize > 0 && !blurLayer_) {
        blurLayer_ = memnew(CanvasLayer);
        blurLayer_->set_name("Blur");
        blurLayer_->set_layer(-1);                                // over the world, under every ScreenGui
        add_child(blurLayer_);
        ColorRect* rect = memnew(ColorRect);
        rect->set_anchors_preset(Control::PRESET_FULL_RECT);
        rect->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        Ref<Shader> sh; sh.instantiate();
        sh->set_code("shader_type canvas_item;\n"
                     "uniform sampler2D screen : hint_screen_texture, filter_linear_mipmap, repeat_disable;\n"
                     "uniform float lod = 3.0;\n"
                     "void fragment() { COLOR = textureLod(screen, SCREEN_UV, lod); }\n");
        blurMaterial_.instantiate();
        blurMaterial_->set_shader(sh);
        rect->set_material(blurMaterial_);
        blurLayer_->add_child(rect);
    }
    if (blurLayer_) {
        blurLayer_->set_visible(blurSize > 0);
        // Size is the blur's radius in pixels; a mip level n averages ~2^n of them
        if (blurSize > 0) blurMaterial_->set_shader_parameter("lod", (float)std::log2(blurSize));
    }

    // SunRaysEffect: shafts of light through the air, drawn with volumetric fog and a sun that
    // lights it. Intensity is how much light is in the air; Spread maps onto anisotropy the
    // other way round, since high anisotropy scatters tightly forward into narrow rays.
    const Effect* rays = active.count("SunRaysEffect") ? active["SunRaysEffect"] : nullptr;
    e->set_volumetric_fog_enabled(rays != nullptr);
    if (rays) {
        const double intensity = std::clamp(num(rays, "Intensity", 0.25), 0.0, 1.0);
        const double spread = std::clamp(num(rays, "Spread", 1.0), 0.0, 1.0);
        e->set_volumetric_fog_density((float)(0.001 + 0.05 * intensity));
        e->set_volumetric_fog_anisotropy((float)(0.95 - 0.85 * spread));
        e->set_volumetric_fog_length(512);
        e->set_volumetric_fog_gi_inject(0);
    }
    if (sun_) sun_->set_param(Light3D::PARAM_VOLUMETRIC_FOG_ENERGY, rays ? 1.0f : 0.0f);

    const Effect* sky = active.count("Sky") ? active["Sky"] : nullptr;
    Ref<Sky> skyRes = e->get_sky();
    if (skyRes.is_valid()) {
        // The sky a Sky asked for, else the procedural one -- put back when a Sky's faces are
        // cleared, so a black shader is not left behind.
        if (!apply_skybox(skyRes, sky)) {
            Ref<ProceduralSkyMaterial> m = skyRes->get_material();
            if (m.is_null()) {
                m.instantiate();
                skyRes->set_material(m);
            }
            if (m.is_valid()) {
                bool shown = true;
                if (sky) { auto it = sky->props.find("CelestialBodiesShown"); shown = it == sky->props.end() || it->second.type != Value::Bool || it->second.b; }
                m->set_sun_angle_max(shown ? (float)std::clamp(sky ? num(sky, "SunAngularSize", 21) : 30.0, 0.0, 360.0) : 0.0f);
            }
        }
    }
    update_lighting();   // the Atmosphere's fog, or Lighting's own
}

// The sky a Sky asks for: six images as a cubemap, a sun, a moon and stars. True when it took,
// so the caller knows whether the procedural sky is still the one on screen. It takes over only
// for what the procedural sky cannot do -- all six faces, a sun or moon texture, or stars.
//
// Godot's cube order is +X, -X, +Y, -Y, +Z, -Z, which is Rt, Lf, Up, Dn, Bk, Ft. The four sides
// go across as they are; Roblox's SkyboxUp and SkyboxDn are turned a quarter against the way a
// cubemap wants them, so Up takes a quarter anticlockwise and Dn the mirror of it, or the top
// meets the sides along a visible seam. All six faces or none, and rebuilt only when the set of
// images changes, since a cubemap is six textures and this runs on every lighting pass.
//
// A body is a disc at a direction with its texture mapped across it; the directions move with
// ClockTime and belong to update_sky_bodies. StarCount is approximate: the shader cuts the sky
// into cells, puts at most one star in each, and the count sets what fraction of cells get one.
static const char* kSkyShader = R"GLSL(
shader_type sky;

uniform samplerCube faces;
uniform bool has_faces = false;
uniform mat3 faces_rot = mat3(1.0);   // SkyboxOrientation's inverse: the lookup turns the other way
// Only when there are no faces: an approximation of the procedural sky, so stars alone do
// not cost a place its sky.
uniform vec3 sky_top = vec3(0.216, 0.404, 0.671);
uniform vec3 sky_horizon = vec3(0.647, 0.761, 0.851);
uniform vec3 ground_tint = vec3(0.157, 0.169, 0.192);

uniform sampler2D sun_tex;
uniform bool has_sun_tex = false;
uniform vec3 sun_dir = vec3(0.0, 1.0, 0.0);
uniform float sun_cos = 1.0;          // cosine of half the angular size; 1 draws nothing
uniform float sun_show = 1.0;

uniform sampler2D moon_tex;
uniform bool has_moon_tex = false;
uniform vec3 moon_dir = vec3(0.0, -1.0, 0.0);
uniform float moon_cos = 1.0;
uniform float moon_show = 1.0;

uniform float star_scale = 0.0;       // 0 for no stars
uniform float star_chance = 0.0;      // how many cells hold one
uniform float star_fade = 0.0;        // 0 by day, 1 at night

float hash13(vec3 p) {
	p = fract(p * 0.1031);
	p += dot(p, p.yzx + 33.33);
	return fract((p.x + p.y) * p.z);
}

// A textured disc facing the viewer. `c` is the cosine of half the angular size, so the
// test is one dot product; the offsets across the disc become the texture's uv.
vec3 body(vec3 eye, vec3 dir, float c, sampler2D tex, bool has_tex) {
	if (c >= 1.0) { return vec3(0.0); }
	float d = dot(eye, dir);
	if (d < c) { return vec3(0.0); }
	vec3 up = abs(dir.y) > 0.99 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
	vec3 rt = normalize(cross(up, dir));
	vec3 dn = cross(dir, rt);
	float r = sqrt(max(1e-6, 1.0 - c * c));
	vec2 uv = vec2(dot(eye, rt), dot(eye, dn)) / r * 0.5 + 0.5;
	if (has_tex) {
		vec4 t = texture(tex, clamp(uv, 0.0, 1.0));
		return t.rgb * t.a;
	}
	// No texture: a plain disc with a soft edge, which is what Roblox draws too.
	return vec3(smoothstep(c, mix(c, 1.0, 0.35), d));
}

float stars(vec3 eye) {
	if (star_scale <= 0.0) { return 0.0; }
	vec3 p = eye * star_scale;
	vec3 cell = floor(p);
	if (hash13(cell) > star_chance) { return 0.0; }
	vec3 at = cell + vec3(hash13(cell + 11.0), hash13(cell + 23.0), hash13(cell + 37.0));
	float d = length(p - at);
	return smoothstep(0.16, 0.0, d) * (0.35 + 0.65 * hash13(cell + 53.0));
}

void sky() {
	vec3 c;
	if (has_faces) {
		c = texture(faces, faces_rot * EYEDIR).rgb;
	} else {
		float h = clamp(EYEDIR.y, -1.0, 1.0);
		c = h >= 0.0 ? mix(sky_horizon, sky_top, pow(h, 0.55)) : mix(sky_horizon, ground_tint, pow(-h, 0.4));
	}
	c += vec3(stars(EYEDIR)) * star_fade;
	c += body(EYEDIR, sun_dir, sun_cos, sun_tex, has_sun_tex) * sun_show;
	c += body(EYEDIR, moon_dir, moon_cos, moon_tex, has_moon_tex) * moon_show;
	COLOR = c;
}
)GLSL";

bool PulseBlockzWorld::apply_skybox(Ref<Sky>& skyRes, const Effect* sky) {
    auto str = [&](const char* k) -> std::string {
        if (!sky) return std::string();
        auto it = sky->props.find(k);
        return it != sky->props.end() && it->second.type == Value::String ? it->second.s : std::string();
    };
    auto number = [&](const char* k, double d) {
        if (!sky) return d;
        auto it = sky->props.find(k);
        return it == sky->props.end() || it->second.type != Value::Number ? d : it->second.n;
    };

    static const char* kFaces[6] = {"SkyboxRt", "SkyboxLf", "SkyboxUp", "SkyboxDn", "SkyboxBk", "SkyboxFt"};
    std::vector<std::string> ids;
    std::string want;
    bool allFaces = true;
    for (const char* f : kFaces) {
        std::string id = str(f);
        if (id.empty()) { allFaces = false; break; }
        ids.push_back(id);
        want += id + "|";
    }

    const std::string sunTex = str("SunTextureId");
    const std::string moonTex = str("MoonTextureId");
    const double starCount = std::clamp(number("StarCount", 0), 0.0, 100000.0);
    const bool wantBodies = !sunTex.empty() || !moonTex.empty() || starCount > 0;

    // Nothing here Godot's own sky cannot do. Hand it back rather than take it over.
    if (!allFaces && !wantBodies) {
        skyboxIds_.clear();
        skyBodyIds_.clear();
        skyMaterial_.unref();
        return false;
    }

    Ref<ShaderMaterial> mat = skyRes->get_material();
    if (mat.is_null() || mat != skyMaterial_) {
        Ref<Shader> sh;
        sh.instantiate();
        sh->set_code(String::utf8(kSkyShader));
        mat.instantiate();
        mat->set_shader(sh);
        skyRes->set_material(mat);
        skyMaterial_ = mat;
        skyboxIds_.clear();
        skyBodyIds_.clear();
    }

    // The bodies' textures, when they change; either may still be on its way.
    const std::string bodyWant = sunTex + "|" + moonTex;
    if (bodyWant != skyBodyIds_) {
        Ref<Texture2D> st = sunTex.empty() ? Ref<Texture2D>() : load_texture(sunTex);
        Ref<Texture2D> mt = moonTex.empty() ? Ref<Texture2D>() : load_texture(moonTex);
        mat->set_shader_parameter("has_sun_tex", st.is_valid());
        mat->set_shader_parameter("has_moon_tex", mt.is_valid());
        if (st.is_valid()) mat->set_shader_parameter("sun_tex", st);
        if (mt.is_valid()) mat->set_shader_parameter("moon_tex", mt);
        // Settled only once both have actually loaded, so one still arriving is retried.
        if (sunTex.empty() == st.is_null() && moonTex.empty() == mt.is_null()) skyBodyIds_ = bodyWant;
    }

    // Stars. The cells are a cube of side 2 * star_scale around the viewer, and what lies on
    // the sky is the shell of it: about 4*pi*scale^2 cells. The chance is the count over
    // that, so asking for more stars fills more cells rather than making finer ones.
    if (starCount > 0) {
        const double scale = 36.0;
        mat->set_shader_parameter("star_scale", (float)scale);
        mat->set_shader_parameter("star_chance",
            (float)std::clamp(starCount / (4.0 * Math_PI * scale * scale), 0.0, 1.0));
    } else {
        mat->set_shader_parameter("star_scale", 0.0f);
        mat->set_shader_parameter("star_chance", 0.0f);
    }

    update_sky_bodies();

    if (!allFaces) {
        mat->set_shader_parameter("has_faces", false);
        skyboxIds_.clear();
        return true;
    }
    if (want == skyboxIds_) return true;          // already up, and the same six

    TypedArray<Image> images;
    for (const std::string& id : ids) {
        Ref<Texture2D> t = load_texture(id);
        if (t.is_null()) return true;             // still arriving: try again next pass
        Ref<Image> im = t->get_image();
        if (im.is_null() || im->is_empty()) return true;
        Ref<Image> copy = Image::create_from_data(im->get_width(), im->get_height(), false, im->get_format(), im->get_data());
        if (copy.is_null()) return true;
        // Every face has to be the same size and format for a cubemap; the first one sets
        // both and the rest are made to match rather than refused.
        if (images.size() > 0) {
            Ref<Image> first = images[0];
            if (copy->get_format() != first->get_format()) copy->convert(first->get_format());
            if (copy->get_width() != first->get_width() || copy->get_height() != first->get_height())
                copy->resize(first->get_width(), first->get_height(), Image::INTERPOLATE_BILINEAR);
        }
        images.push_back(copy);
    }
    // The Y faces, turned to the cubemap's convention. Image::rotate_90 needs a square, which
    // every face here is by the time the first one has set the size.
    if (images.size() == 6) {
        Ref<Image> up = images[2], dn = images[3];
        if (up.is_valid() && up->get_width() == up->get_height()) up->rotate_90(COUNTERCLOCKWISE);
        if (dn.is_valid() && dn->get_width() == dn->get_height()) dn->rotate_90(CLOCKWISE);
    }

    Ref<Cubemap> cube;
    cube.instantiate();
    if (cube->create_from_images(images) != OK) return true;
    for (int i = 0; i < images.size(); i++) { Ref<Image> im = images[i]; if (im.is_valid()) note_texture("skybox face", im->get_width(), im->get_height()); }
    mat->set_shader_parameter("faces", cube);
    mat->set_shader_parameter("has_faces", true);
    skyboxIds_ = want;
    return true;
}

// Where the sun and the moon are, and how bright the stars are behind them. Separate from
// apply_skybox because these move with ClockTime while the images almost never change. The sun's
// direction is the one update_lighting turns the DirectionalLight to, so the two agree exactly.
void PulseBlockzWorld::update_sky_bodies() {
    if (skyMaterial_.is_null()) return;
    const double a = (lighting_.clockTime - 6.0) / 12.0 * Math_PI;
    const Vector3 toSun((float)std::cos(a), (float)std::sin(a), 0);
    const double daylight = std::clamp(std::sin(a) * 2.0, 0.0, 1.0);

    const Effect* sky = nullptr;
    for (auto& [id, fx] : effects_) if (fx.className == "Sky" && effect_active(id)) { sky = &fx; break; }
    auto number = [&](const char* k, double d) {
        if (!sky) return d;
        auto it = sky->props.find(k);
        return it == sky->props.end() || it->second.type != Value::Number ? d : it->second.n;
    };
    bool shown = true;
    if (sky) {
        auto it = sky->props.find("CelestialBodiesShown");
        shown = it == sky->props.end() || it->second.type != Value::Bool || it->second.b;
    }

    // Roblox's angular sizes are degrees across the whole body; the shader wants the cosine of
    // half. Clamped below 180 degrees, since cos(0) is 1 and draws nothing at all.
    auto halfCos = [](double degrees) {
        return (float)std::cos(std::clamp(degrees, 0.0, 179.0) * 0.5 * Math_PI / 180.0);
    };
    skyMaterial_->set_shader_parameter("sun_dir", toSun);
    skyMaterial_->set_shader_parameter("moon_dir", -toSun);   // opposite the sun, as in Studio
    // SkyboxOrientation turns the six faces, degrees about Y, X then Z as a part's Orientation.
    Vec3 orient;
    if (sky) if (auto it = sky->props.find("SkyboxOrientation"); it != sky->props.end() && it->second.type == Value::Vector3) orient = it->second.v;
    skyMaterial_->set_shader_parameter("faces_rot", toTransform(Vec3{0, 0, 0}, orient).basis.inverse());
    skyMaterial_->set_shader_parameter("sun_cos", halfCos(number("SunAngularSize", 21)));
    skyMaterial_->set_shader_parameter("moon_cos", halfCos(number("MoonAngularSize", 11)));
    skyMaterial_->set_shader_parameter("sun_show", shown ? 1.0f : 0.0f);
    skyMaterial_->set_shader_parameter("moon_show", shown ? 1.0f : 0.0f);
    // Stars come out as the sun goes down, and are gone before it is properly up.
    skyMaterial_->set_shader_parameter("star_fade", (float)std::clamp(1.0 - daylight * 2.0, 0.0, 1.0));
}

void PulseBlockzWorld::replay_humanoid(int64_t humanoidId) {
    auto it = humanoidProps_.find(humanoidId);
    if (it == humanoidProps_.end()) return;
    for (const auto& [name, v] : it->second) {
        Change c; c.kind = Change::Property; c.id = humanoidId; c.name = name; c.value = v;
        apply_humanoid(c);
    }
}

void PulseBlockzWorld::apply_humanoid(const Change& c) {
    auto eit = entries_.find(c.id);
    if (eit == entries_.end()) return;
    humanoidProps_[c.id][c.name] = c.value;   // kept, in case its character is not here yet
    auto cit = chars_.find(eit->second.parent);
    if (cit == chars_.end()) return;
    Character& ch = cit->second;
    if (c.name == "WalkSpeed") ch.walkSpeed = c.value.n;
    else if (c.name == "JumpPower") ch.jumpPower = c.value.n;
    // The other way of asking for a jump; UseJumpPower decides which is read.
    else if (c.name == "JumpHeight") ch.jumpHeight = c.value.n;
    else if (c.name == "UseJumpPower") ch.useJumpPower = c.value.b;
    // How far the root floats above the floor; zero is the legacy behaviour.
    else if (c.name == "HipHeight") {
        ch.hipHeight = c.value.n;
        if (auto rp = parts_.find(ch.root); rp != parts_.end()) {
            rp->second.hipHeight = (float)ch.hipHeight;
            if (rp->second.col)
                rp->second.col->set_transform(Transform3D(Basis(), Vector3(0, -0.5f - rp->second.hipHeight, 0)));
        }
    }
    else if (c.name == "DisplayName") ch.displayName = c.value.s;
    else if (c.name == "Health") ch.health = c.value.n;
    // The owner's own word on what this body is doing: it writes StateName for the character it
    // simulates and the property replicates. See the remote branch in step_characters.
    else if (c.name == "StateName") ch.state = c.value.s;
    else if (c.name == "MaxHealth") ch.maxHealth = c.value.n;
    else if (c.name == "NameDisplayDistance") ch.nameDistance = c.value.n;
    else if (c.name == "HealthDisplayDistance") ch.healthDistance = c.value.n;
    else if (c.name == "DisplayDistanceType") ch.distanceType = enumName(c.value);
    else if (c.name == "HealthDisplayType") ch.healthType = enumName(c.value);
    else if (c.name == "NameOcclusion") ch.occlusion = enumName(c.value);
    else if (c.name == "AutoRotate") ch.autoRotate = c.value.b;
    else if (c.name == "CameraOffset") ch.cameraOffset = c.value.v;
    else if (c.name == "Jump") {
        // Kept whoever wrote it: a stale true is the thing to catch (reset in step_characters).
        ch.jumpProp = c.value.b;
        if (!c.fromHost && c.value.b) ch.jump = true;   // scripted jump; the engine puts it back down
    }
    else if (c.name == "SeatPart") { ch.seat = c.value.ref; if (ch.seat) ch.standUp = false; }
    else if (c.name == "MoveDirection" && c.fromHost && remote_char(eit->second.parent)) ch.moveDir = c.value.v;   // reported by its owner
    else if (c.name == "MoveDirection" && !c.fromHost) { ch.moveDir = c.value.v; ch.walking = false; }            // Humanoid:Move
    else if (c.name == "WalkToPoint" && !remote_char(eit->second.parent)) { ch.walking = true; ch.walkTo = c.value.v; ch.walkT = 0; }   // Humanoid:MoveTo, from either side: whoever simulates it walks
}

// A character simulated by another world: a client's on the server, every one but the local
// player's on a client. The engine does not move it; its owner reports it.
bool PulseBlockzWorld::remote_char(int64_t model) const {
    return netClient_ ? model != localChar_ : remoteChars_.count(model) > 0;
}

// Whether a fromHost Position/Orientation moves the scene node: only for what is simulated
// elsewhere -- remote characters' roots, and on a client everything the server owns. Limbs
// never: they ride on their root at the rest offset.
bool PulseBlockzWorld::takes_pose_from_tree(int64_t id, const Part& p) const {
    if (p.role == ROLE_CHAR_ROOT) { auto it = entries_.find(id); return it != entries_.end() && remote_char(it->second.parent); }
    if (p.role == ROLE_LIMB) return false;
    return netClient_ && id > 0;
}

// The ViewportFrame this instance is inside, if it is inside one.
int64_t PulseBlockzWorld::viewport_frame_of(int64_t id) const {
    for (int guard = 0; guard < 256; guard++) {
        auto it = entries_.find(id);
        if (it == entries_.end() || it->second.parent == kNoParent) return 0;
        id = it->second.parent;
        auto pe = entries_.find(id);
        if (pe == entries_.end()) return 0;
        if (pe->second.className == "ViewportFrame") return id;
        if (id == workspaceId_) return 0;          // out into the world: not in a frame
    }
    return 0;
}

// Is this part inside a ViewportFrame that is actually built?
bool PulseBlockzWorld::viewport_ready(int64_t id) const {
    int64_t fid = viewport_frame_of(id);
    if (!fid) return false;
    auto git = guis_.find(fid);
    return git != guis_.end() && git->second.frameRoot != nullptr;
}

// Where a part's body belongs: a frame's own root, or the world.
Node* PulseBlockzWorld::viewport_root(int64_t id) {
    int64_t fid = viewport_frame_of(id);
    if (!fid) return this;
    auto git = guis_.find(fid);
    if (git == guis_.end() || !git->second.frameRoot) return this;
    return git->second.frameRoot;
}

// A part is drawn while it sits under Workspace, a Model or Folder in between being fine;
// ReplicatedStorage and ServerStorage parts stay invisible.
bool PulseBlockzWorld::in_workspace(int64_t id) const {
    for (int guard = 0; guard < 256; guard++) {
        if (id == workspaceId_) return true;
        auto it = entries_.find(id);
        if (it == entries_.end() || it->second.parent == kNoParent) return false;
        id = it->second.parent;
    }
    return false;
}

// What a character carries or wears: a Tool's parts ride the right arm, an Accessory's the limb
// its Handle's Attachment names.
static bool worn_class(const std::string& cls) { return cls == "Tool" || cls == "Accessory" || cls == "Accoutrement" || cls == "Hat"; }

// A part directly under a Model with a Humanoid is part of a character: the HumanoidRootPart
// becomes the body, the rest ride on it.
PulseBlockzWorld::Role PulseBlockzWorld::role_of(int64_t id) const {
    auto it = entries_.find(id);
    if (it == entries_.end()) return ROLE_FREE;
    if (chars_.count(it->second.parent)) return it->second.name == "HumanoidRootPart" ? ROLE_CHAR_ROOT : ROLE_LIMB;
    auto tit = entries_.find(it->second.parent);
    if (tit != entries_.end() && worn_class(tit->second.className) && chars_.count(tit->second.parent)) return ROLE_LIMB;
    if (limbWeld_.count(id)) return ROLE_LIMB;                   // welded to a limb: it rides one
    // A weld makes a limb only where there is physics to save: in an edit world a part keeps
    // its own body, which is what the Studio needs to box it, click it and write its Position.
    // A character's limbs stay limbs either way, since those are posed by the rig.
    if (!editMode_) {
        int64_t w = weld_root(id);
        if (w && w != id) return ROLE_LIMB;
    }
    return ROLE_FREE;
}

int64_t PulseBlockzWorld::character_of(int64_t id) const {
    auto it = entries_.find(id);
    if (it == entries_.end()) return 0;
    if (chars_.count(it->second.parent)) return it->second.parent;
    auto tit = entries_.find(it->second.parent);
    if (tit != entries_.end() && worn_class(tit->second.className) && chars_.count(tit->second.parent)) return tit->second.parent;
    // Welded to a limb: the limb's character. One hop only, so a chain of welds cannot loop.
    if (auto lw = limbWeld_.find(id); lw != limbWeld_.end()) {
        auto le = entries_.find(lw->second.limb);
        if (le != entries_.end() && chars_.count(le->second.parent)) return le->second.parent;
    }
    return 0;
}

void PulseBlockzWorld::refresh_visibility(int64_t id) {
    auto it = parts_.find(id);
    if (it == parts_.end()) return;
    Part& p = it->second;
    // Under Workspace, or inside a ViewportFrame that is already built. Parts are refreshed
    // before the GUI, so on the pass where a frame first appears its root does not exist yet; a
    // part built then would land in the town and stay, since nothing rebuilds a visible part.
    bool vis = in_workspace(id) || viewport_ready(id);
    Role role = vis ? role_of(id) : ROLE_FREE;
    int64_t root = 0;
    bool tool = false, welded = false, worn = false;
    if (role == ROLE_LIMB) {
        int64_t model = character_of(id);
        if (model) {
            root = chars_[model].root;
            bool carried = !chars_.count(entries_[id].parent);
            worn = carried && entries_[entries_[id].parent].className != "Tool";
            tool = carried && !worn;
        } else {
            root = weld_root(id);                            // an assembly's part rides the root part's body
            welded = true;
            auto rit = parts_.find(root);
            if (rit == parts_.end() || !rit->second.body) root = 0;
        }
        if (!root) vis = false;                              // the root is not built yet: next pass
    }
    if (role == ROLE_CHAR_ROOT && p.visible && p.body && p.role == ROLE_CHAR_ROOT) chars_[entries_[id].parent].root = id;
    if (vis == p.visible && role == p.role && root == p.attachedTo && tool == p.tool && worn == p.worn && welded == p.welded) return;
    if (p.visible) free_part(id, p);
    p.visible = vis; p.role = role; p.attachedTo = root; p.tool = tool; p.worn = worn; p.welded = welded;
    if (vis) build_part(id, p);
}

void PulseBlockzWorld::start_watchdog() {
    if (watchdog_.joinable()) return;
    lastTickUsec_.store((int64_t)Time::get_singleton()->get_ticks_usec());
    watchdog_ = std::thread([this] {
        int64_t said = 0;
        while (!watchdogStop_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            const int64_t now = (int64_t)Time::get_singleton()->get_ticks_usec();
            const int64_t late = now - lastTickUsec_.load();
            if (late > 5000000 && now - said > 5000000) {
                said = now;
                UtilityFunctions::print(String("[World] the main thread has not ticked for ") + String::num_int64(late / 1000000) + " s; it was in: " + String(phase_.load()) + " (instance " + String::num_int64(phaseId_.load()) + ")");
            }
        }
    });
}

// Decompositions that have landed: into the cache, and every part that wore the stand-in has its
// shapes made again -- the same update_shape, which now finds the hulls and uses them.
void PulseBlockzWorld::poll_hull_jobs() {
    for (auto it = hullJobs_.begin(); it != hullJobs_.end();) {
        HullJob& job = *it->second;
        if (!job.done.load()) { ++it; continue; }
        if (job.thread.joinable()) job.thread.join();
        if (job.hulls.empty()) { Ref<ConvexPolygonShape3D> h = job.mesh->create_convex_shape(true, false); if (h.is_valid()) job.hulls.push_back(h->get_points()); }
        hullCache_[job.key] = std::move(job.hulls);
        const std::string key = job.key;
        it = hullJobs_.erase(it);                 // gone before the parts are redone, or they would wait on it again
        auto wit = hullWaiters_.find(key);
        if (wit != hullWaiters_.end()) {
            std::vector<int64_t> ids = std::move(wit->second);
            hullWaiters_.erase(wit);
            for (int64_t id : ids) { auto pit = parts_.find(id); if (pit != parts_.end() && pit->second.visible && pit->second.col) update_shape(pit->second); }
        }
    }
}

void PulseBlockzWorld::refresh_all_visibility() {
    visibilityDirty_ = false;
    // Timed per part, and said when the pass ran long: which parts were built, and what they cost.
    std::vector<std::pair<double, int64_t>> costly;
    const auto t0 = std::chrono::steady_clock::now();
    auto one = [&](int64_t id) {
        const auto a = std::chrono::steady_clock::now();
        refresh_visibility(id);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count();
        if (ms >= 1.0) costly.push_back({ms, id});
    };
    for (auto& [id, p] : parts_) if (p.role == ROLE_CHAR_ROOT || role_of(id) == ROLE_CHAR_ROOT || weld_root(id) == id) one(id);
    for (auto& [id, p] : parts_) one(id);
    const double total = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (total >= 100.0 && client_) {
        std::sort(costly.begin(), costly.end(), [](auto& a, auto& b) { return a.first > b.first; });
        double named = 0;
        std::string line = "[World] visibility pass: " + std::to_string((int)total) + " ms over " + std::to_string(parts_.size()) + " part(s), " + std::to_string(costly.size()) + " cost a millisecond or more:";
        for (size_t r = 0; r < costly.size() && r < 8; r++) {
            auto eit = entries_.find(costly[r].second);
            line += " " + (eit != entries_.end() ? eit->second.className + " " + eit->second.name : std::string("?")) + " " + std::to_string((int)costly[r].first) + " ms";
            if (auto nit = buildNote_.find(costly[r].second); nit != buildNote_.end()) line += " [" + nit->second + "]";
            line += ";";
            named += costly[r].first;
        }
        for (auto& c : costly) named += 0;
        UtilityFunctions::print(String::utf8(line.c_str()));
    }
}

// ---- scene nodes -------------------------------------------------------------------
void PulseBlockzWorld::free_part(int64_t id, Part& p) {
    if (!highlights_.empty()) highlightsDirty_ = true;
    if (!constraints_.empty()) constraintsDirty_ = true;
    detach_lights(id);
    detach_prompts(id);
    detach_sounds(id);
    if (p.col) shapeToId_.erase(p.col->get_instance_id());
    for (auto* x : p.extraCols) shapeToId_.erase(x->get_instance_id());
    if (p.role == ROLE_LIMB) {
        if (p.mesh) p.mesh->queue_free();
        if (p.col) p.col->queue_free();
        for (auto* x : p.extraCols) x->queue_free();
    } else if (p.body) {
        bodyToId_.erase(p.body->get_instance_id());
        p.body->queue_free();
        // The limbs (a character's, or a welded assembly's parts) went with the
        // body node; they rebuild once a root is back.
        for (int64_t lid : parts_on_root(id))
            if (Part& lp = parts_[lid]; lp.role == ROLE_LIMB) { if (lp.col) shapeToId_.erase(lp.col->get_instance_id()); for (auto* x : lp.extraCols) shapeToId_.erase(x->get_instance_id()); lp.extraCols.clear(); lp.mesh = nullptr; lp.col = nullptr; lp.visible = false; lp.attachedTo = 0; detach_lights(lid); detach_prompts(lid); detach_sounds(lid); }
        if (p.role == ROLE_CHAR_ROOT)
            for (auto& [m, ch] : chars_) if (ch.root == id) ch.root = 0;
    }
    p.body = nullptr; p.mesh = nullptr; p.col = nullptr; p.rigid = false;
    p.extraCols.clear();   // freed with the body, or above
    p.lods[0] = Ref<Mesh>(); p.lods[1] = Ref<Mesh>(); p.lods[2] = Ref<Mesh>();
    p.lodBand = -1;
}

void PulseBlockzWorld::build_part(int64_t id, Part& p) {
    if (!p.visible) return;
    const std::string& name = entries_[id].name;
    if (p.role == ROLE_LIMB) {
        auto rit = parts_.find(p.attachedTo);
        if (rit == parts_.end() || !rit->second.body) { p.visible = false; return; }
        p.mesh = memnew(MeshInstance3D);
        p.mesh->set_name(String::utf8(name.c_str()));
        p.mesh->set_cast_shadows_setting(p.castShadow ? GeometryInstance3D::SHADOW_CASTING_SETTING_ON : GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
        rit->second.body->add_child(p.mesh, true);   // readable: a second Handle (a Tool's beside an Accessory's) reads as Handle2
        if (p.welded) {
            p.col = memnew(CollisionShape3D);
            p.col->set_name(String::utf8((name + " Collision").c_str()));
            p.col->set_disabled(!p.canCollide);
            rit->second.body->add_child(p.col, true);
            shapeToId_[p.col->get_instance_id()] = id;
        }
        update_shape(p);
        update_material(p);
        update_limb_offset(id, p);
        p.sentPos = p.pos; p.sentOrient = p.orient; p.sentVel = p.vel;
        lightsDirty_ = promptsDirty_ = true; dirty_sounds(__LINE__);
        return;
    }
    Node3D* body;
    if (p.role == ROLE_CHAR_ROOT) {
        auto* cb = memnew(CharacterBody3D);
        cb->set_floor_snap_length(0.4f);
        body = cb;
        chars_[entries_[id].parent].root = id;
    } else if (p.anchored || editMode_ || (netClient_ && id > 0)) {
        // Static in an edit world too: nothing falls while you are building.
        body = memnew(StaticBody3D);          // on a client the server's parts are where the server says
    } else {
        auto* rb = memnew(RigidBody3D);
        rb->set_gravity_scale(gravity_ / engineGravity_);
        rb->set_contact_monitor(true);
        rb->set_max_contacts_reported(8);
        Array bound; bound.push_back((int64_t)id);
        rb->connect("body_shape_entered", Callable(this, "_on_body_shape_entered").bindv(bound));
        rb->connect("body_shape_exited", Callable(this, "_on_body_shape_exited").bindv(bound));
        rb->set_linear_velocity(toGd(p.vel));
        body = rb;
        p.rigid = true;
        massDirty_ = true;
    }
    body->set_name(String::utf8(name.c_str()));
    p.body = body;
    update_surface(p);
    if (auto* co = Object::cast_to<CollisionObject3D>(body)) {   // two worlds in one scene (a test) keep their physics apart
        co->set_collision_layer(p.rigid ? rigid_bit() : static_bit());
        co->set_collision_mask(static_bit() | rigid_bit());
    }
    p.col = memnew(CollisionShape3D);
    shapeToId_[p.col->get_instance_id()] = id;
    p.col->set_disabled(p.role == ROLE_FREE && !p.canCollide);
    p.mesh = memnew(MeshInstance3D);
    p.mesh->set_cast_shadows_setting(p.castShadow ? GeometryInstance3D::SHADOW_CASTING_SETTING_ON : GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
    body->add_child(p.col);
    body->add_child(p.mesh);
    update_shape(p);
    update_material(p);
    update_transform(p);
    p.sentPos = p.pos; p.sentOrient = p.orient; p.sentVel = p.vel;
    // The world node, or a ViewportFrame's own root when the part lives inside one.
    viewport_root(id)->add_child(body);
    bodyToId_[body->get_instance_id()] = id;
    lightsDirty_ = promptsDirty_ = true; dirty_sounds(__LINE__);
}

void PulseBlockzWorld::update_transform(Part& p) {
    if (p.role == ROLE_LIMB && p.welded && !editMode_) {
        // Roblox moves the whole assembly to keep the weld: the root goes where this part's
        // offset says it must be. Not while editing, where a weld is data and every Position is
        // authored outright -- following one write there would tear a Model apart.
        auto rit = parts_.find(p.attachedTo);
        if (rit != parts_.end() && rit->second.body) rit->second.body->set_transform(toTransform(p.pos, p.orient) * p.offset.affine_inverse());
        return;
    }
    if (p.role == ROLE_LIMB) { p.offsetDirty = true; return; }   // relative to the root, settled after the frame's changes
    if (!p.body) return;
    p.body->set_transform(toTransform(p.pos, p.orient));
}

// A reported pose for a remote character: snapped when it is a teleport (respawn, a fresh body,
// a big jump) or the first one, else eased from where the body is over the interval the reports
// arrive at, clamped to 1/60..0.2 s. Position and Orientation share one ease.
void PulseBlockzWorld::remote_pose(Character& ch, const Part& root) {
    if (!root.body) return;
    Transform3D target = toTransform(root.pos, root.orient);
    uint64_t now = Time::get_singleton()->get_ticks_msec();
    Transform3D cur = root.body->get_transform();
    bool sameBatch = ch.poseAtMs && now - ch.poseAtMs < 2;
    if (sameBatch && ch.posing) { ch.poseTo = target; return; }
    if (sameBatch && !ch.posing) { root.body->set_transform(target); return; }
    double gap = ch.poseAtMs ? (double)(now - ch.poseAtMs) / 1000.0 : 0;
    ch.poseAtMs = now;
    if (gap <= 0 || gap > 0.5 || cur.origin.distance_to(target.origin) > 10) {
        root.body->set_transform(target);
        ch.posing = false;
        return;
    }
    ch.poseFrom = cur;
    ch.poseTo = target;
    ch.poseDur = std::clamp(gap, 1.0 / 60.0, 0.2);
    ch.poseT = 0;
    ch.posing = true;
}

void PulseBlockzWorld::update_limb_offset(int64_t id, Part& p) {
    auto rit = parts_.find(p.attachedTo);
    if (!p.mesh || rit == parts_.end() || !rit->second.body) {
        // Nothing to hang it on yet: stay dirty and ask again next frame. Clearing the flag
        // here drops the request, and whether it happens depends on the map's walk order.
        p.offsetDirty = p.visible;
        return;
    }
    p.offsetDirty = false;
    if (p.tool) {
        // Roblox's RightGrip weld: the Handle hangs off the bottom of the right arm
        // turned to point forward (C0), moved by the Tool's Grip (C1); the tool's
        // other parts keep their offsets from it.
        Transform3D arm(Basis(), Vector3(1.5f, 0, 0));
        double drop = 1;   // how far below the limb's middle the grip sits
        for (int64_t lid : parts_on_root(p.attachedTo))
            if (Part& lp = parts_[lid]; lp.role == ROLE_LIMB && !lp.tool
                && (entries_[lid].name == "Right Arm" || entries_[lid].name == "RightHand")) {
                arm = lp.offset;
                drop = lp.size.y * 0.5;   // an R15 hand is short: the grip sits at its bottom
                if (entries_[lid].name == "RightHand") break;   // an R15 rig holds it in the hand
            }
        int64_t toolId = entries_[id].parent, handle = 0;
        handle = handle_of(toolId);
        Transform3D rel;
        if (handle && handle != id) {
            auto hit = parts_.find(handle);
            if (hit != parts_.end()) rel = toTransform(hit->second.pos, hit->second.orient).affine_inverse() * toTransform(p.pos, p.orient);
        }
        Transform3D grip;
        if (auto git = grips_.find(toolId); git != grips_.end()) {
            const Grip& g = git->second;
            grip = Transform3D(Basis(g.right, g.up, -g.forward), g.pos);
        }
        p.offset = arm * Transform3D(Basis(Vector3(1, 0, 0), -Math_PI / 2), Vector3(0, (real_t)-drop, 0)) * grip.affine_inverse() * rel;
        p.mesh->set_transform(p.offset * p.shapeLocal);
        return;
    }
    if (p.worn) {
        // Welded straight to a limb: its place is the weld's, in the limb's own space, and the
        // limb's offset from the root was settled in the pass before this one.
        if (auto lw = limbWeld_.find(id); lw != limbWeld_.end()) {
            auto lp = parts_.find(lw->second.limb);
            if (lp != parts_.end() && lp->second.role == ROLE_LIMB && !lp->second.worn && lp->second.attachedTo == p.attachedTo) {
                p.offset = lp->second.offset * lw->second.local;
                p.wornOn = lw->second.limb;
                p.mesh->set_transform(p.offset * p.shapeLocal);
                return;
            }
        }
        // The Handle's Attachment sits on the limb's of the same name (HatAttachment
        // on the Head), and the accessory's other parts keep their offsets from it.
        int64_t acc = entries_[id].parent, handle = 0;
        handle = handle_of(acc);
        if (!handle) handle = id;
        Transform3D worn;
        bool found = false;
        // Every attachment in the town indexed by name, read once: pairing them by scanning
        // is quadratic, and an outfit landing runs it for every part of every accessory.
        std::unordered_map<std::string, std::vector<int64_t>> byName;
        std::vector<int64_t> onHandle;
        for (auto& [aid, local] : attachments_) {
            auto ae = entries_.find(aid);
            if (ae == entries_.end()) continue;
            if (ae->second.parent == handle) onHandle.push_back(aid);
            byName[ae->second.name].push_back(aid);
        }
        for (int64_t aid : onHandle) {
            auto ae = entries_.find(aid);
            const auto& local = attachments_.at(aid);
            for (int64_t bid : byName[ae->second.name]) {
                auto be = entries_.find(bid);
                if (be == entries_.end()) continue;
                const auto& blocal = attachments_.at(bid);
                auto lp = parts_.find(be->second.parent);
                if (lp == parts_.end() || lp->second.role != ROLE_LIMB || lp->second.tool || lp->second.worn
                    || lp->second.attachedTo != p.attachedTo) continue;
                worn = lp->second.offset * blocal * local.affine_inverse();
                p.wornOn = be->second.parent;
                found = true;
                break;
            }
            if (found) break;
        }
        // No pair of Attachments: an older hat instead names an offset and a basis from the
        // head, and Roblox welds it there. The Handle goes at head * (pos, basis), not at its
        // inverse -- a hat sits ABOVE a head -- with the basis right, up, -forward.
        if (!found) {
            auto mit = mounts_.find(acc);
            if (mit != mounts_.end() && mit->second.stated) {
                for (auto& [lid, limb] : parts_) {
                    if (limb.role != ROLE_LIMB || limb.tool || limb.worn || limb.attachedTo != p.attachedTo) continue;
                    auto le = entries_.find(lid);
                    if (le == entries_.end() || le->second.name != "Head") continue;
                    const WornMount& m = mit->second;
                    Vector3 right = m.right.length() > 1e-4f ? m.right.normalized() : Vector3(1, 0, 0);
                    Vector3 up = m.up.length() > 1e-4f ? m.up.normalized() : Vector3(0, 1, 0);
                    Vector3 fwd = m.forward.length() > 1e-4f ? m.forward.normalized() : Vector3(0, 0, -1);
                    worn = limb.offset * Transform3D(Basis(right, up, -fwd), m.pos);
                    p.wornOn = lid;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {   // nothing to hang it on yet: it rides where it was put, and asks again
            p.offset = rit->second.body->get_transform().affine_inverse() * toTransform(p.pos, p.orient);
            p.offsetDirty = true;
        } else if (handle == id) {
            p.offset = worn;
        } else {
            auto hit = parts_.find(handle);
            Transform3D rel = hit == parts_.end() ? Transform3D()
                            : toTransform(hit->second.pos, hit->second.orient).affine_inverse() * toTransform(p.pos, p.orient);
            p.offset = worn * rel;
        }
        p.mesh->set_transform(p.offset * p.shapeLocal);
        return;
    }
    if (p.welded) {
        if (auto wit = weldOffset_.find(id); wit != weldOffset_.end()) p.offset = wit->second;
        p.mesh->set_transform(p.offset * p.shapeLocal);
        if (p.col) p.col->set_transform(p.offset * p.colLocal);
        for (auto* x : p.extraCols) x->set_transform(p.offset * p.colLocal);
        return;
    }
    p.offset = rit->second.body->get_transform().affine_inverse() * toTransform(p.pos, p.orient);
    p.mesh->set_transform(p.offset * p.shapeLocal);
    update_skin(p);
}

// An emitter's shape as a cloud of starting points, each with the way it leaves. Godot's
// built-in emission shapes fire every particle one way, which cannot answer Roblox's three
// questions at once: a Cylinder or Disc is neither box nor sphere, ShapeStyle says skin or
// volume, and ShapeInOut can send particles INWARD. DIRECTED_POINTS is a texture of positions
// and a texture of directions, a pixel each. 256 points is a sampling of the shape, not the
// particle count -- particles pick from the cloud at random and reuse it.
void PulseBlockzWorld::set_emitter_cloud(Ref<ParticleProcessMaterial>& pm, const std::string& shape,
                                       bool surfaceOnly, const std::string& inOut, const Vector3& half) {
    const int kPoints = 256;
    PackedByteArray pos, nrm;
    pos.resize(kPoints * 3 * 4);
    nrm.resize(kPoints * 3 * 4);
    float* pf = reinterpret_cast<float*>(pos.ptrw());
    float* nf = reinterpret_cast<float*>(nrm.ptrw());
    // Deterministic, so two runs of the same place scatter the same way and a difference on
    // screen is a difference in the place rather than in the dice.
    uint32_t seed = 0x9E3779B9u;
    auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return (float)(seed >> 8) / 16777216.0f; };
    auto sym = [&]() { return rnd() * 2.0f - 1.0f; };

    const Vector3 r(std::max(half.x, 0.01f), std::max(half.y, 0.01f), std::max(half.z, 0.01f));
    for (int i = 0; i < kPoints; i++) {
        Vector3 at, out;
        if (shape == "Sphere") {
            // A direction, then how far along it. Cube-rooting the radius fills a volume
            // evenly; without it everything crowds the middle.
            Vector3 d(sym(), sym(), sym());
            if (d.length() < 1e-4f) d = Vector3(0, 1, 0);
            d.normalize();
            float t = surfaceOnly ? 1.0f : std::cbrt(rnd());
            at = Vector3(d.x * r.x, d.y * r.y, d.z * r.z) * t;
            out = d;
        } else if (shape == "Cylinder" || shape == "Disc") {
            // A disc is a cylinder with no height, which is exactly how Roblox draws it.
            float a = rnd() * (float)Math_TAU;
            Vector2 d(std::cos(a), std::sin(a));
            float t = surfaceOnly ? 1.0f : std::sqrt(rnd());        // sqrt for an even disc
            float y = shape == "Disc" ? 0.0f : sym() * r.y;
            at = Vector3(d.x * r.x * t, y, d.y * r.z * t);
            out = Vector3(d.x, 0, d.y);
        } else {
            if (surfaceOnly) {
                // A face at random, then a point on it. Picking a face by area rather than
                // by count keeps a long thin box from emitting mostly off its two ends.
                float ax = r.y * r.z, ay = r.x * r.z, az = r.x * r.y;
                float pick = rnd() * (ax + ay + az);
                Vector3 n = pick < ax ? Vector3(1, 0, 0) : pick < ax + ay ? Vector3(0, 1, 0) : Vector3(0, 0, 1);
                if (rnd() < 0.5f) n = -n;
                at = Vector3(sym() * r.x, sym() * r.y, sym() * r.z);
                if (n.x) at.x = n.x * r.x;
                else if (n.y) at.y = n.y * r.y;
                else at.z = n.z * r.z;
                out = n;
            } else {
                at = Vector3(sym() * r.x, sym() * r.y, sym() * r.z);
                out = at.length() > 1e-4f ? at.normalized() : Vector3(0, 1, 0);
            }
        }
        // Inward is outward turned round; InAndOut is half of each, which is what Roblox
        // draws -- not a random direction, but the same shape firing both ways.
        if (inOut == "Inward") out = -out;
        else if (inOut == "InAndOut" && (i & 1)) out = -out;
        pf[i * 3] = at.x; pf[i * 3 + 1] = at.y; pf[i * 3 + 2] = at.z;
        nf[i * 3] = out.x; nf[i * 3 + 1] = out.y; nf[i * 3 + 2] = out.z;
    }
    Ref<Image> pim = Image::create_from_data(kPoints, 1, false, Image::FORMAT_RGBF, pos);
    Ref<Image> nim = Image::create_from_data(kPoints, 1, false, Image::FORMAT_RGBF, nrm);
    pm->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_DIRECTED_POINTS);
    pm->set_emission_point_count(kPoints);
    pm->set_emission_point_texture(ImageTexture::create_from_image(pim));
    pm->set_emission_normal_texture(ImageTexture::create_from_image(nim));
    note_texture("particle emission", pim->get_width(), pim->get_height(), 8);
    note_texture("particle emission", nim->get_width(), nim->get_height(), 8);
}

// The bubble a ForceField draws: a sphere on the character's root in the ForceField material's
// blue, drawn from both sides so it reads as a skin when the camera is inside it.
void PulseBlockzWorld::sync_shields() {
    shieldsDirty_ = false;
    std::unordered_map<int64_t, MeshInstance3D*> kept;
    for (auto& [id, e] : entries_) {
        if (e.className != "ForceField") continue;
        // Visible decides the drawing only; a field with it off still stops damage.
        auto vit = e.props.find("Visible");
        if (vit != e.props.end() && vit->second.type == Value::Bool && !vit->second.b) continue;
        // The character's root, which is what the sphere rides on.
        int64_t root = 0;
        for (int64_t pid : parts_under(e.parent))
            if (auto pp = parts_.find(pid); pp != parts_.end() && pp->second.role == ROLE_CHAR_ROOT) { root = pid; break; }
        if (!root) continue;
        auto rp = parts_.find(root);
        if (rp == parts_.end() || !rp->second.body) continue;
        MeshInstance3D*& node = shields_[id];
        if (!node) {
            node = memnew(MeshInstance3D);
            node->set_name("ForceField");
            Ref<SphereMesh> ball; ball.instantiate();
            ball->set_radius(3.2f); ball->set_height(6.4f);
            node->set_mesh(ball);
            Ref<StandardMaterial3D> m; m.instantiate();
            m->set_albedo(Color(0.35f, 0.65f, 1.0f, 0.25f));
            m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
            m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
            m->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
            m->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
            m->set_emission(Color(0.35f, 0.65f, 1.0f));
            m->set_emission_energy_multiplier(1.5f);
            node->set_material_override(m);
            node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
            rp->second.body->add_child(node);
        }
        kept[id] = node;
    }
    // Out of the tree first, then freed: queue_free is deferred to the end of the frame, so a
    // bubble whose field has just gone would be drawn one more time.
    for (auto& [id, node] : shields_)
        if (!kept.count(id) && node) {
            if (Node* parent = node->get_parent()) parent->remove_child(node);
            node->queue_free();
        }
    shields_ = std::move(kept);
}

// The Handle of a Tool or an Accessory, chosen the same way every time: with two children of
// that name the earliest made wins, which is the one an authoring tool means by "the Handle".
// entries_ is an unordered_map, so taking the first match would pick arbitrarily.
int64_t PulseBlockzWorld::handle_of(int64_t owner) const {
    int64_t found = 0, earliest = 0;
    for (int64_t eid : parts_under(owner)) {   // a Handle is a part; there is nothing else it could be
        auto it = entries_.find(eid);
        if (it == entries_.end() || it->second.name != "Handle") continue;
        if (!found || it->second.made < earliest) { found = eid; earliest = it->second.made; }
    }
    return found;
}

// Studs and inlets as real geometry, not a texture: a stud has a silhouette against the sky and
// catches the light from the side. One MultiMesh for a part's studs and one for its inlets, so a
// wall costs two draws. A stud is Roblox's -- half a stud across, a fifth proud, one per stud of
// face -- and an inlet is drawn as the rim of the hole, since the face behind it is opaque.
// Blocks only: a ball or cylinder has no faces for them and a mesh brings its own surface.
void PulseBlockzWorld::update_studs(Part& p) {
    if (!p.mesh) return;
    const bool block = p.className == "Part" && p.shape == "Block" && !p.dataMesh && p.meshId.empty();

    // The faces that want something, and how many pieces each needs.
    struct Face { Vector3 normal, across, down; float w, h; };
    const Vector3 half = toGd(p.size) * 0.5f;
    const Face faces[6] = {
        {Vector3(1, 0, 0), Vector3(0, 0, 1), Vector3(0, 1, 0), p.size.z, p.size.y},   // Right
        {Vector3(-1, 0, 0), Vector3(0, 0, 1), Vector3(0, 1, 0), p.size.z, p.size.y},  // Left
        {Vector3(0, 1, 0), Vector3(1, 0, 0), Vector3(0, 0, 1), p.size.x, p.size.z},   // Top
        {Vector3(0, -1, 0), Vector3(1, 0, 0), Vector3(0, 0, 1), p.size.x, p.size.z},  // Bottom
        {Vector3(0, 0, 1), Vector3(1, 0, 0), Vector3(0, 1, 0), p.size.x, p.size.y},   // Back
        {Vector3(0, 0, -1), Vector3(1, 0, 0), Vector3(0, 1, 0), p.size.x, p.size.y},  // Front
    };

    // A part is rebuilt when what it wears or how big it is changes, and at no other time --
    // this runs off property changes, and Size changes on every step of a moving door.
    std::string key;
    if (block) {
        for (int f = 0; f < 6; f++) key += std::to_string((int)p.surface[f]);
        key += "|" + std::to_string(p.size.x) + "," + std::to_string(p.size.y) + "," + std::to_string(p.size.z);
    }
    if (key == p.studKey) return;
    p.studKey = key;

    // Where every piece goes, by kind. One per whole stud of face, centred in its square, so
    // a 4 x 2 face carries eight and a face under a stud across carries none.
    std::vector<Transform3D> at[3];
    if (block) {
        for (int f = 0; f < 6; f++) {
            const uint8_t wears = p.surface[f];
            if (wears == 0) continue;
            const Face& face = faces[f];
            const int across = (int)std::floor(face.w + 1e-4f);
            const int down = (int)std::floor(face.h + 1e-4f);
            if (across < 1 || down < 1) continue;
            // A wall of studs is a wall of draws in the physics of it too; past this many the
            // face is being used as a texture and should have one.
            if ((size_t)(across * down) + at[wears].size() > 4096) continue;
            const Vector3 centre = face.normal * (face.normal.abs().dot(half));
            // The piece stands along the face's normal, so its own Y points out of the part.
            Basis stand = Basis::looking_at(face.across, face.normal);   // -Z across, Y out
            stand = Basis(stand.get_column(0), face.normal, stand.get_column(2));
            for (int i = 0; i < across; i++) {
                for (int j = 0; j < down; j++) {
                    Vector3 o = face.across * (i + 0.5f - face.w * 0.5f) + face.down * (j + 0.5f - face.h * 0.5f);
                    at[wears].push_back(Transform3D(stand, centre + o));
                }
            }
        }
    }

    for (int wears = 1; wears <= 2; wears++) {
        MultiMeshInstance3D*& node = wears == 1 ? p.studs : p.inlets;
        if (at[wears].empty()) {
            (wears == 1 ? p.studAt : p.inletAt).clear();
            // Out of the tree before it is freed, as the shields are: a smoothed face would
            // otherwise keep its studs for one more frame.
            if (node) {
                if (Node* parent = node->get_parent()) parent->remove_child(node);
                node->queue_free();
                node = nullptr;
            }
            continue;
        }
        if (!node) {
            node = memnew(MultiMeshInstance3D);
            node->set_name(wears == 1 ? "Studs" : "Inlets");
            p.mesh->add_child(node);
        }
        Ref<MultiMesh> mm = node->get_multimesh();
        if (mm.is_null()) {
            mm.instantiate();
            mm->set_transform_format(MultiMesh::TRANSFORM_3D);
            if (wears == 1) {
                Ref<CylinderMesh> c; c.instantiate();
                c->set_top_radius(0.25f); c->set_bottom_radius(0.25f);
                c->set_height(0.2f); c->set_radial_segments(12); c->set_rings(1);
                mm->set_mesh(c);
            } else {
                Ref<TorusMesh> t; t.instantiate();
                t->set_inner_radius(0.20f); t->set_outer_radius(0.28f);
                t->set_rings(12); t->set_ring_segments(6);
                mm->set_mesh(t);
            }
            node->set_multimesh(mm);
        }
        std::vector<Transform3D>& kept = wears == 1 ? p.studAt : p.inletAt;
        kept.clear();
        mm->set_instance_count((int)at[wears].size());
        for (size_t i = 0; i < at[wears].size(); i++) {
            // A stud stands half its height proud of the face; a ring lies in it.
            Transform3D t = at[wears][i];
            if (wears == 1) t.origin += t.basis.get_column(1) * 0.1f;
            mm->set_instance_transform((int)i, t);
            kept.push_back(t);
        }
        node->set_material_override(material_for(p));
    }
}

// Where a part rides inside the body it belongs to: what every limb, worn accessory and held
// tool offset calculation produces. By id, since a scene node's name is not stable -- a second
// Handle on a character reads as "Handle2".
Transform3D PulseBlockzWorld::part_offset(int64_t id) const {
    auto it = parts_.find(id);
    return it == parts_.end() ? Transform3D() : it->second.offset;
}

// The Control a GuiObject is drawn as, or null. The GUI tree is built without names -- a Roblox
// name is not unique among its siblings and Godot's would have to be -- so this crosses by id.
Object* PulseBlockzWorld::gui_control(int64_t id) const {
    auto it = guis_.find(id);
    return it == guis_.end() ? nullptr : (Object*)it->second.node;
}

// Where a part's studs or inlets are, in part space. `which` is "Studs" or "Inlets"; empty for a
// part wearing neither and for anything that is not a plain block. The renderer is the only
// other place the answer lives, and a headless one drops the MultiMesh's transforms.
Array PulseBlockzWorld::part_surface_pieces(int64_t id, const String& which) const {
    Array out;
    auto it = parts_.find(id);
    if (it == parts_.end()) return out;
    const std::vector<Transform3D>& at = which == "Inlets" ? it->second.inletAt : it->second.studAt;
    for (const Transform3D& t : at) out.push_back(t);
    return out;
}

void PulseBlockzWorld::update_shape(Part& p) {
    const auto shapeT0 = std::chrono::steady_clock::now();
    Vector3 size = toGd(p.size);
    // A MeshPart: the file scaled so its bounds are Size, its centre at the part's
    // (Roblox recentres a mesh on import). Collision is the hull of that, a box for
    // CollisionFidelity Box, the exact triangles for PreciseConvexDecomposition on an
    // anchored part (a moving body needs a convex shape). MeshSize is the file's bounds.
    Ref<Mesh> drawn;
    // What the part collides as, when it is a mesh: the geometry, the fit to Size, and a key the
    // decomposition is kept under. Built below, once it is known the part has a collision shape.
    Ref<Mesh> collideSource;
    Transform3D collideFit;
    std::string collideKey;
    Ref<ArrayMesh> lodDecoded;   // a union's (or a made MeshPart's) own geometry, which its levels of detail are simplified from
    Vector3 lodScale(1, 1, 1);
    bool lodSmoothing = false;
    bool meshShaped = false, noCollision = false;
    const ClassDef* opc = findClass(p.className);
    auto fitTo = [&](const AABB& box, bool recentre) {
        Vector3 sc(box.size.x > 1e-6f ? size.x / box.size.x : 1, box.size.y > 1e-6f ? size.y / box.size.y : 1, box.size.z > 1e-6f ? size.z / box.size.z : 1);
        return std::make_pair(sc, Transform3D(Basis().scaled(sc), recentre ? -(box.get_center() * sc) : Vector3()));
    };
    if (p.className == "MeshPart" && p.meshId.empty() && !p.meshData.empty()) {
        // A MeshPart GeometryService made: its geometry is in memory, in the main part's space,
        // drawn about the part's origin like a union's, with its own normals and the main mesh's
        // UVs -- a MeshPart has no smoothing angle to adjust.
        Ref<ArrayMesh> own = op_mesh_for(p);
        if (own.is_valid()) {
            AABB box = own->get_aabb();
            p.meshSize = box.size;
            auto [sc, fit] = fitTo(box, false);
            drawn = union_drawn(own, sc, size, 0, false);
            lodDecoded = own; lodScale = sc; lodSmoothing = false;
            collideSource = own; collideFit = fit; collideKey = "data:" + std::to_string(std::hash<std::string>()(p.meshData));
            meshShaped = true;
        }
        if (p.meshSize != p.sentMeshSize) { p.sentMeshSize = p.meshSize; assetWrites_.push_back({p.id, "MeshSize", Value::vector3(p.meshSize.x, p.meshSize.y, p.meshSize.z)}); }
    } else if (p.className == "MeshPart") {
        Ref<Mesh> file = load_mesh(p.meshId);
        if (file.is_valid()) {
            // An EditableMesh keeps what it was when applied until it is applied again: its
            // shape for collision and the bounds (MeshSize) Size scales, so an edit pulling a
            // corner out draws past the part, as Roblox's Size / MeshSize does.
            const bool editable = p.meshId.rfind("rbxobject://", 0) == 0;
            if (editable && p.collisionMesh.is_null()) p.collisionMesh = file->duplicate();
            AABB box = editable ? p.collisionMesh->get_aabb() : file->get_aabb();
            p.meshSize = box.size;
            auto [sc, fit] = fitTo(box, true);
            Ref<ArrayMesh> baked; baked.instantiate();
            if (editable) bake_mesh(baked, file, fit);   // edited where it is: nothing kept of it stays true
            else bake_kept(baked, file, fit);
            drawn = baked;
            collideSource = editable ? p.collisionMesh : file;
            collideFit = fit;
            collideKey = editable ? "editable:" + std::to_string((long long)p.collisionMesh->get_instance_id()) : "file:" + p.meshId;
            meshShaped = true;
        } else p.meshSize = Vector3();
        if (p.meshSize != p.sentMeshSize) { p.sentMeshSize = p.meshSize; assetWrites_.push_back({p.id, "MeshSize", Value::vector3(p.meshSize.x, p.meshSize.y, p.meshSize.z)}); }
    } else if (p.dataMesh && p.mesh) {
        if (auto it = dataMeshes_.find(p.dataMesh); it != dataMeshes_.end()) drawn = data_mesh_for(it->second, p);
    } else if (opc && opc->isA("PartOperation")) {
        // A union's geometry is its own, in its own space, fitted to Size the way a MeshPart's
        // file is, so scaling one scales the shape rather than stretching a box round it.
        Ref<ArrayMesh> decoded = op_mesh_for(p);
        if (decoded.is_valid()) {
            AABB box = decoded->get_aabb();
            // About the part's own origin, not the centre of the geometry: a union Studio or a
            // BasePart method made is recentred already, while one from GeometryService keeps
            // the main part's space and would be moved by recentring here.
            auto [sc, fit] = fitTo(box, false);
            p.meshSize = box.size;
            if (p.meshSize != p.sentMeshSize) { p.sentMeshSize = p.meshSize; assetWrites_.push_back({p.id, "MeshSize", Value::vector3(p.meshSize.x, p.meshSize.y, p.meshSize.z)}); }
            int triangles = 0;
            for (int s = 0; s < decoded->get_surface_count(); s++) {
                Array arrays = decoded->surface_get_arrays(s);
                triangles += arrays[Mesh::ARRAY_INDEX].get_type() != Variant::NIL
                    ? (int)PackedInt32Array(arrays[Mesh::ARRAY_INDEX]).size() / 3
                    : (int)PackedVector3Array(arrays[Mesh::ARRAY_VERTEX]).size() / 3;
            }
            if (triangles != p.sentTriangles) { p.sentTriangles = triangles; assetWrites_.push_back({p.id, "TriangleCount", Value::number(triangles)}); }
            p.opDrawn = union_drawn(decoded, sc, size, p.smoothingAngle, true);
            lodDecoded = decoded; lodScale = sc; lodSmoothing = true;
            drawn = p.opDrawn;
            decalsDirty_ = true;   // a Decal on it is projected from what it now is
            // A negation is a hole, not a thing: it collides with nothing, and what
            // it is subtracted from carries the collision.
            if (p.className == "NegateOperation") noCollision = true;
            else { collideSource = decoded; collideFit = fit; collideKey = "data:" + std::to_string(std::hash<std::string>()(p.meshData)); meshShaped = true; }
        }
    }
    if (!p.mesh) return;
    Ref<Mesh> mesh;
    Ref<Shape3D> shape;
    Transform3D local;   // mesh/shape offset inside the body (cylinders lie along X)
    const bool wedge = p.className == "WedgePart";
    const std::string& s = wedge ? std::string("Wedge") : p.className == "Part" ? p.shape : std::string("Block");
    if (s == "Ball") {
        float r = std::min(std::min(p.size.x, p.size.y), p.size.z) * 0.5f;
        Ref<SphereMesh> m; m.instantiate(); m->set_radius(r); m->set_height(2 * r); mesh = m;
        Ref<SphereShape3D> sh; sh.instantiate(); sh->set_radius(r); shape = sh;
    } else if (s == "Cylinder") {
        float r = std::min(p.size.y, p.size.z) * 0.5f;
        Ref<CylinderMesh> m; m.instantiate(); m->set_top_radius(r); m->set_bottom_radius(r); m->set_height(p.size.x); mesh = m;
        Ref<CylinderShape3D> sh; sh.instantiate(); sh->set_radius(r); sh->set_height(p.size.x); shape = sh;
        local = Transform3D(Basis(Vector3(0, 0, 1), -Math_PI / 2), Vector3());   // Y axis -> X axis
    } else if (s == "Wedge") {
        // PrismMesh: right triangle in XY extruded along Z, vertical face at -X.
        // A Roblox wedge is vertical at its back (+Z), sloping down to the front.
        Ref<PrismMesh> m; m.instantiate(); m->set_left_to_right(0); m->set_size(Vector3(p.size.z, p.size.y, p.size.x)); mesh = m;
        shape = m->create_convex_shape();
        local = Transform3D(Basis(Vector3(0, 1, 0), Math_PI / 2), Vector3());
    } else {
        Ref<BoxMesh> m; m.instantiate(); m->set_size(size); mesh = m;
        Ref<BoxShape3D> sh; sh.instantiate(); sh->set_size(size); shape = sh;
    }
    Transform3D meshLocal = local;
    if (drawn.is_valid()) { mesh = drawn; meshLocal = Transform3D(); }             // a file / SpecialMesh sits in part space
    if (meshShaped || noCollision) local = Transform3D();
    if (!(p.shapeLocal == meshLocal)) lightsDirty_ = promptsDirty_ = true; dirty_sounds(__LINE__);   // a light / prompt / sound under the mesh undoes this offset
    p.shapeLocal = meshLocal;
    // A union or a MeshPart shows the level of detail its RenderFidelity and distance call for.
    if (meshShaped || (opc && opc->isA("PartOperation") && drawn.is_valid())) {
        build_lods(p, mesh, lodDecoded, lodScale, lodSmoothing);
        p.lodBand = lod_band_for(p);
        ensure_lod(p, p.lodBand);
        p.mesh->set_mesh(p.lods[p.lodBand].is_valid() ? p.lods[p.lodBand] : p.lods[0]);
    } else {
        p.lods[0] = Ref<Mesh>(); p.lods[1] = Ref<Mesh>(); p.lods[2] = Ref<Mesh>();
        p.lodDecoded = Ref<ArrayMesh>();
        p.lodBand = -1;
        p.mesh->set_mesh(mesh);
    }
    // Through the pose it is being held in, not past it: a rebuild happens on every Size, MeshId,
    // Shape or mesh-data change, and would otherwise write an animated limb back to its rest pose.
    p.mesh->set_transform(p.role == ROLE_LIMB ? p.posed * p.offset * meshLocal : meshLocal);
    update_studs(p);   // a face two studs wide carries two, and Size is what says so
    if (!highlights_.empty()) highlightsDirty_ = true;                              // a copy of the old mesh is stale
    if (!p.col) return;
    if (p.role == ROLE_CHAR_ROOT) {
        // One capsule for the whole R6 body: 2 wide, from the feet (3 below the
        // root) to the top of the head (2 above). Limbs carry no collision.
        Ref<CapsuleShape3D> cap; cap.instantiate(); cap->set_radius(1.0f); cap->set_height(5.0f);
        p.col->set_shape(cap);
        // Dropping the capsule raises the root off the floor by the same amount, which is
        // what HipHeight means: the body stands taller, the shape does not change size.
        p.col->set_transform(Transform3D(Basis(), Vector3(0, -0.5f - p.hipHeight, 0)));
        return;
    }
    p.colLocal = local;
    const double meshMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - shapeT0).count();
    const auto colT0 = std::chrono::steady_clock::now();
    std::vector<Ref<Shape3D>> shapes;
    if (meshShaped) {
        shapes = collision_shapes(collideKey, collideSource, collideFit, p.collisionFidelity);
        const std::string cacheKey = collideKey + "#" + p.collisionFidelity;
        if (hullJobs_.count(cacheKey)) hullWaiters_[cacheKey].push_back(p.id);
    }
    else if (!noCollision) shapes.push_back(shape);
    set_collision_shapes(p, shapes, local);
    const double colMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - colT0).count();
    // What a slow build was spent on, for the visibility pass to say.
    if (meshMs + colMs >= 10.0) buildNote_[p.id] = "mesh " + std::to_string((int)meshMs) + " ms, collision " + std::to_string((int)colMs) + " ms (" + p.collisionFidelity + ", key " + collideKey.substr(0, 24) + ")";
    else buildNote_.erase(p.id);
}

// Union the parts the active joints join into assemblies. A joint counts when it is enabled, in
// the Workspace with both its parts, and neither part is a character's -- characters are their
// own assemblies. A network client does not assemble: the server says where every part is.
void PulseBlockzWorld::rebuild_assemblies() {
    jointsDirty_ = false;
    std::unordered_map<int64_t, int64_t> oldRoot = std::move(weldRoot_);
    weldRoot_.clear(); weldOffset_.clear();
    // What hangs off a character's limbs. A joint with a limb on exactly one side is not an
    // assembly -- the limb is posed by the rig, not by physics -- but the part on the other
    // side has to ride it, as on Roblox, or a place that dresses its own rig sees the armour
    // fall straight through the floor the moment it spawns. Kept on clients too: this is
    // posing, which every side does, not assembly, which only the server does.
    std::unordered_map<int64_t, LimbWeld> oldLimb = std::move(limbWeld_);
    limbWeld_.clear();
    for (auto& [jid, j] : joints_) {
        if (!j.enabled || !j.part0 || !j.part1 || j.part0 == j.part1) continue;
        auto a = parts_.find(j.part0), b = parts_.find(j.part1);
        if (a == parts_.end() || b == parts_.end()) continue;
        if (!in_workspace(j.part0) || !in_workspace(j.part1)) continue;
        const bool c0 = character_of(j.part0) != 0, c1 = character_of(j.part1) != 0;
        if (c0 == c1) continue;                 // both a character's (the rig itself) or neither (an assembly)
        // Part1 = Part0 * rel for a Weld or Motor6D; a WeldConstraint holds where it took hold.
        Transform3D rel;
        if (j.className == "WeldConstraint") {
            if (!j.captured) { j.rel = toTransform(a->second.pos, a->second.orient).affine_inverse() * toTransform(b->second.pos, b->second.orient); j.captured = true; }
            rel = j.rel;
        } else {
            rel = j.c0 * j.transform * Transform3D(Basis(Vector3(0, 0, 1), (real_t)j.angle), Vector3()) * j.c1.affine_inverse();
        }
        if (c0) limbWeld_[j.part1] = {j.part0, rel};                       // the limb is Part0: the part sits at limb * rel
        else    limbWeld_[j.part0] = {j.part1, rel.affine_inverse()};      // the limb is Part1: limb = part * rel, so part = limb * rel^-1
    }
    for (auto& [id, lw] : limbWeld_) {
        auto o = oldLimb.find(id);
        if (o == oldLimb.end() || o->second.limb != lw.limb) visibilityDirty_ = true;
        else if (auto pit = parts_.find(id); pit != parts_.end()) pit->second.offsetDirty = true;
    }
    for (auto& [id, _] : oldLimb) if (!limbWeld_.count(id)) visibilityDirty_ = true;
    if (!netClient_) {
        struct Edge { int64_t a, b; Transform3D rel; };
        std::vector<Edge> edges;
        std::unordered_map<int64_t, int64_t> up;
        auto find = [&](int64_t x) { while (up[x] != x) { up[x] = up[up[x]]; x = up[x]; } return x; };
        for (auto& [jid, j] : joints_) {
            if (!j.enabled || !j.part0 || !j.part1 || j.part0 == j.part1) continue;
            auto a = parts_.find(j.part0), b = parts_.find(j.part1);
            if (a == parts_.end() || b == parts_.end()) continue;
            if (!in_workspace(j.part0) || !in_workspace(j.part1) || !in_workspace(jid)) continue;
            if (character_of(j.part0) || character_of(j.part1)) continue;
            Transform3D rel;
            if (j.className == "WeldConstraint") {
                // Holds the parts where they were when it took hold.
                if (!j.captured) { j.rel = toTransform(a->second.pos, a->second.orient).affine_inverse() * toTransform(b->second.pos, b->second.orient); j.captured = true; }
                rel = j.rel;
            } else {
                rel = j.c0 * j.transform * Transform3D(Basis(Vector3(0, 0, 1), (real_t)j.angle), Vector3()) * j.c1.affine_inverse();   // Part1 = Part0 * C0 * Transform * C1^-1
            }
            up.try_emplace(j.part0, j.part0); up.try_emplace(j.part1, j.part1);
            up[find(j.part0)] = find(j.part1);
            edges.push_back({j.part0, j.part1, rel});
        }
        std::unordered_map<int64_t, std::vector<int64_t>> groups;
        for (auto& [id, _] : up) groups[find(id)].push_back(id);
        massDirty_ = true;
        for (auto& [_, members] : groups) {
            if (members.size() < 2) continue;
            // The root: an anchored part (the assembly stays put), else the biggest.
            int64_t root = 0; double best = -1;
            for (int64_t id : members) {
                const Part& p = parts_[id];
                double score = (p.anchored ? 1e12 : 0) + (double)p.size.x * p.size.y * p.size.z;
                if (score > best || (score == best && id < root)) { best = score; root = id; }
            }
            std::unordered_map<int64_t, Transform3D> off; off[root] = Transform3D();
            std::vector<int64_t> queue{root};
            for (size_t qi = 0; qi < queue.size(); ++qi) {
                int64_t cur = queue[qi];
                for (const Edge& e : edges) {
                    if (e.a == cur && !off.count(e.b)) { off[e.b] = off[cur] * e.rel; queue.push_back(e.b); }
                    else if (e.b == cur && !off.count(e.a)) { off[e.a] = off[cur] * e.rel.affine_inverse(); queue.push_back(e.a); }
                }
            }
            for (int64_t id : members) { weldRoot_[id] = root; weldOffset_[id] = off[id]; }
        }
    }
    // A part whose root changed hands rebuilds; one whose offset may have re-poses.
    for (auto& [id, p] : parts_) {
        auto o = oldRoot.find(id);
        int64_t before = o == oldRoot.end() ? 0 : o->second, after = weld_root(id);
        if (before != after) visibilityDirty_ = true;
        else if (after && after != id) p.offsetDirty = true;
    }
}

// The SurfaceAppearance hanging under a part, or the MaterialVariant it wears by name. A variant
// counts only when its BaseMaterial is the part's Material, as on Roblox: a variant of Concrete
// does nothing to a part made of Plastic.
const PulseBlockzWorld::Effect* PulseBlockzWorld::surface_for(const Part& p) const {
    for (auto& [id, fx] : effects_) {
        if (fx.className != "SurfaceAppearance") continue;
        auto e = entries_.find(id);
        if (e != entries_.end() && e->second.parent == p.id) return &fx;
    }
    // No variant of its own: MaterialService's override for its Material, if one is set.
    std::string want = p.materialVariant;
    if (want.empty()) if (auto o = materialOverrides_.find(p.material); o != materialOverrides_.end()) want = o->second;
    if (want.empty()) return nullptr;
    for (auto& [id, fx] : effects_) {
        if (fx.className != "MaterialVariant") continue;
        auto e = entries_.find(id);
        if (e == entries_.end() || e->second.name != want) continue;
        auto b = fx.props.find("BaseMaterial");
        if (b != fx.props.end() && b->second.type == Value::Enum && b->second.s != p.material) continue;
        return &fx;
    }
    return nullptr;
}

Ref<Material> PulseBlockzWorld::material_for(const Part& p) {
    // MeshPart.TextureID, or a FileMesh SpecialMesh's TextureId: the albedo image
    std::string texture = p.textureId;
    if (p.dataMesh) if (auto it = dataMeshes_.find(p.dataMesh); it != dataMeshes_.end() && it->second.type == "FileMesh") texture = it->second.textureId;
    // The maps, if this part wears any. They go in the key: two parts with the same colour and
    // different maps are two materials.
    const Effect* surface = surface_for(p);
    auto map = [&](const char* n) -> std::string {
        if (!surface) return std::string();
        auto it = surface->props.find(n);
        return it == surface->props.end() ? std::string() : content_key(it->second);
    };
    const std::string colorMap = map("ColorMap"), normalMap = map("NormalMap");
    const std::string metalMap = map("MetalnessMap"), roughMap = map("RoughnessMap");
    const std::string emissiveMask = map("EmissiveMaskContent");
    double emissiveStrength = 1;
    Col3 emissiveTint{1, 1, 1};
    if (surface) {
        if (auto it = surface->props.find("EmissiveStrength"); it != surface->props.end() && it->second.type == Value::Number) emissiveStrength = it->second.n;
        if (auto it = surface->props.find("EmissiveTint"); it != surface->props.end() && it->second.type == Value::Color3) emissiveTint = it->second.c;
    }
    double studsPerTile = 0;
    if (surface && surface->className == "MaterialVariant") {
        auto it = surface->props.find("StudsPerTile");
        studsPerTile = it != surface->props.end() && it->second.type == Value::Number ? it->second.n : 10;
    }
    std::string key = std::to_string(p.color.r) + "," + std::to_string(p.color.g) + "," + std::to_string(p.color.b) + "|" + std::to_string(p.transparency) + "|" +
                      std::to_string(p.reflectance) + "|" + p.material + "|" + texture +
                      (p.doubleSided ? "|2" : "") + "|" + colorMap + "|" + normalMap + "|" + metalMap + "|" + roughMap;
    if (!emissiveMask.empty())
        key += "|e" + emissiveMask + "*" + std::to_string(emissiveStrength) + "*" + std::to_string(emissiveTint.r) + "," + std::to_string(emissiveTint.g) + "," + std::to_string(emissiveTint.b);
    if (p.dataMesh) if (auto dm = dataMeshes_.find(p.dataMesh); dm != dataMeshes_.end()) {
        const Vec3& v = dm->second.vertexColor;
        if (v.x != 1 || v.y != 1 || v.z != 1)
            key += "|v" + std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z);
    }
    // Only when something tiles, or an ordinary part would get a material per size.
    if (studsPerTile > 0)
        key += "|" + std::to_string(studsPerTile) + "@" + std::to_string(p.size.x) + "," + std::to_string(p.size.y);
    // Face colours: a PartOperation shows the colour each face kept from its part unless
    // UsePartColor is on; a MeshPart GeometryService made always shows them, times its Color.
    const ClassDef* pcls = findClass(p.className);
    const bool isOp = pcls && pcls->isA("PartOperation");
    // An EditableMesh's corners carry vertex colours of their own, as does content baked from one.
    const bool editableColors = p.className == "MeshPart" && (p.meshId.rfind("rbxobject://", 0) == 0 || p.meshId.rfind("rbxopaque://", 0) == 0);
    const bool faceColors = editableColors || (p.opColors && ((isOp && !p.usePartColor) || (p.className == "MeshPart" && p.meshId.empty())));
    if (faceColors) key += isOp ? "|faces" : "|faces*";
    auto it = materials_.find(key);
    if (it != materials_.end()) return it->second;
    Ref<StandardMaterial3D> m; m.instantiate();
    bool textured = false;
    if (!texture.empty()) { Ref<Texture2D> tex = load_texture(texture); if (tex.is_valid()) { m->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, tex); textured = true; } }
    // The mesh's own tint, if it has one. A legacy SpecialMesh could be a different colour
    // from the part it sat on, and this is how -- multiplied in, so white changes nothing.
    Col3 tint{1, 1, 1};
    bool fileMeshTexture = false;
    if (p.dataMesh) if (auto dm = dataMeshes_.find(p.dataMesh); dm != dataMeshes_.end()) {
        tint = Col3{dm->second.vertexColor.x, dm->second.vertexColor.y, dm->second.vertexColor.z};
        fileMeshTexture = textured && dm->second.type == "FileMesh" && !dm->second.textureId.empty();
    }
    // A FileMesh wearing its own texture shows the texture, not the part's colour: a classic hat
    // is the colours of its picture whatever BrickColor its Handle has, and VertexColor is the
    // only tint. Multiplying the part's colour in as well draws it through the default grey.
    const Col3 base = fileMeshTexture ? Col3{1, 1, 1} : p.color;
    m->set_albedo(Color(base.r * tint.r, base.g * tint.g, base.b * tint.b, 1.0f - p.transparency));
    if (faceColors) {
        m->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
        m->set_flag(BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
        if (isOp) m->set_albedo(Color(1, 1, 1, 1.0f - p.transparency));   // the faces, not Color
    }
    if (p.transparency > 0) m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
    if (p.doubleSided) m->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
    m->set_metallic(p.reflectance);
    float rough = 0.85f;
    if (p.material == "SmoothPlastic" || p.material == "Glass" || p.material == "Ice") rough = 0.25f;
    else if (p.material == "Metal" || p.material == "DiamondPlate" || p.material == "Foil") { rough = 0.4f; m->set_metallic(std::max(p.reflectance, 0.8f)); }
    else if (p.material == "Neon") {
        // Neon glows on Roblox by itself, so it is drawn brighter than white and the
        // Environment's glow (sync_effects) bleeds a halo off exactly those pixels. The albedo,
        // not emission: an unshaded material draws its albedo and nothing else.
        const Color a = m->get_albedo();
        m->set_albedo(Color(a.r * kNeonHdr, a.g * kNeonHdr, a.b * kNeonHdr, a.a));
        m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
    }
    m->set_roughness(rough);

    // The maps last, because each overrides what the Material enum just decided: on Roblox a
    // SurfaceAppearance replaces the look of the material under it rather than blending with it.
    if (!colorMap.empty()) if (Ref<Texture2D> t = load_texture(colorMap); t.is_valid())
        m->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, t);
    if (!normalMap.empty()) if (Ref<Texture2D> t = load_texture(normalMap); t.is_valid()) {
        m->set_feature(BaseMaterial3D::FEATURE_NORMAL_MAPPING, true);
        m->set_texture(BaseMaterial3D::TEXTURE_NORMAL, t);
    }
    // A map modulates the scalar, so the scalar goes to 1 and the picture decides. Leaving
    // Roughness at 0.85 under a roughness map would cap the shiniest part of it at 0.85.
    if (!metalMap.empty()) if (Ref<Texture2D> t = load_texture(metalMap); t.is_valid()) {
        m->set_texture(BaseMaterial3D::TEXTURE_METALLIC, t);
        m->set_metallic(1.0f);
    }
    if (!roughMap.empty()) if (Ref<Texture2D> t = load_texture(roughMap); t.is_valid()) {
        m->set_texture(BaseMaterial3D::TEXTURE_ROUGHNESS, t);
        m->set_roughness(1.0f);
    }
    // The emissive mask: light of its own, as bright as the mask is white, EmissiveTint times
    // EmissiveStrength, in the colours of the albedo. A second unlit pass adds exactly that --
    // albedo (map and colour) times mask times tint and strength -- blended over the lit one.
    if (!emissiveMask.empty()) if (Ref<Texture2D> mask = load_texture(emissiveMask); mask.is_valid()) {
        // A shader rather than a detail layer: the mask is greyscale, and a detail layer weighs
        // by alpha, where an EditableImage's unpainted pixels are transparent black.
        static Ref<Shader> glowShader;
        if (glowShader.is_null()) {
            glowShader.instantiate();
            glowShader->set_code(R"(shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_back;
uniform sampler2D albedo_tex : source_color, hint_default_white, repeat_enable;
uniform sampler2D mask_tex : hint_default_black, repeat_enable;
uniform vec3 glow = vec3(1.0);
uniform vec2 uv_scale = vec2(1.0);
void fragment() {
	vec2 uv = UV * uv_scale;
	ALBEDO = texture(albedo_tex, uv).rgb * texture(mask_tex, uv).r * glow;
})");
        }
        Ref<ShaderMaterial> glow; glow.instantiate();
        glow->set_shader(glowShader);
        if (Ref<Texture2D> albedo = m->get_texture(BaseMaterial3D::TEXTURE_ALBEDO); albedo.is_valid()) glow->set_shader_parameter("albedo_tex", albedo);
        glow->set_shader_parameter("mask_tex", mask);
        Color base = m->get_albedo();
        const float k = (float)std::max(emissiveStrength, 0.0);
        glow->set_shader_parameter("glow", Vector3(base.r * emissiveTint.r * k, base.g * emissiveTint.g * k, base.b * emissiveTint.b * k));
        Vector3 uvScale = m->get_uv1_scale();
        glow->set_shader_parameter("uv_scale", Vector2(uvScale.x, uvScale.y));
        m->set_next_pass(glow);
    }
    // How big one tile of it is on the part, in studs. A box's uvs run 0..1 across each face,
    // so the repeat is the face measured in tiles.
    if (studsPerTile > 0)
        m->set_uv1_scale(Vector3((float)std::max(p.size.x / studsPerTile, 0.01),
                                 (float)std::max(p.size.y / studsPerTile, 0.01), 1));

    // Cached only once its picture has arrived; otherwise every part of that colour would be
    // handed the untextured one for the rest of the session.
    if (!(!texture.empty() && !textured)) materials_[key] = m;
    return m;
}

// The default character skin: the PulseBlockz gradient, red -> magenta -> purple -> blue ->
// cyan at 45 degrees from the outer-bottom corner of the left foot to the top-right of the head,
// painted continuously across all six limbs. Each limb gets its own material with the line in
// that limb's rest-pose space, so the ramp stays put when the limbs swing.
static const char* kSkinShader = R"(
shader_type spatial;
uniform vec3 g_origin;
uniform vec3 g_dir;
uniform sampler2D ramp : source_color, filter_linear, repeat_disable;
uniform float alpha : hint_range(0.0, 1.0) = 1.0;
uniform float face = 0.0;          // 1 on a head: draw the default face on its front
varying float t;
varying vec3 vpos;                 // model space, so the face stays put as the head turns
varying vec3 vnrm;

// A regular hexagon's signed distance, flat top and bottom with its points left and
// right -- the same orientation the head itself is. The eyes turn thirty degrees off
// this, which is what stops them reading as little copies of the head.
float hexd(vec2 p, float a) {
    p = abs(p);
    return max(p.x * 0.8660254 + p.y * 0.5, p.y) - a;
}

float seg(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    return length(pa - ba * clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0));
}

void vertex() { t = dot(VERTEX - g_origin, g_dir); vpos = VERTEX; vnrm = NORMAL; }
void fragment() {
    ALBEDO = texture(ramp, vec2(clamp(t, 0.0, 1.0), 0.5)).rgb;
    // The face, drawn only on the front of a head. The hexagon spans y -0.49 to 1.24
    // and x -1 to 1, which is where these numbers come from.
    if (face > 0.5 && vnrm.z < -0.5) {
        vec2 p = vpos.xy;
        // Two hexagon outlines, rotated thirty degrees: points up, not sideways.
        vec2 e = vec2(abs(p.x) - 0.40, p.y - 0.66);
        vec2 er = vec2(e.x * 0.8660254 - e.y * 0.5, e.x * 0.5 + e.y * 0.8660254);
        float eye = abs(hexd(er, 0.17)) - 0.032;
        // The mouth is the pulse line off the PulseChain mark: flat, up, hard down,
        // back to flat.
        vec2 m = p - vec2(0.0, 0.16);
        float d = seg(m, vec2(-0.46, 0.0), vec2(-0.21, 0.0));
        d = min(d, seg(m, vec2(-0.21, 0.0), vec2(-0.10, 0.23)));
        d = min(d, seg(m, vec2(-0.10, 0.23), vec2(0.03, -0.21)));
        d = min(d, seg(m, vec2(0.03, -0.21), vec2(0.13, 0.0)));
        d = min(d, seg(m, vec2(0.13, 0.0), vec2(0.46, 0.0)));
        float ink = min(eye, d - 0.033);
        ALBEDO = mix(vec3(0.03, 0.03, 0.05), ALBEDO, smoothstep(0.0, 0.014, ink));
    }
    ROUGHNESS = 0.85;
    METALLIC = 0.0;
)";
static const char* kSkinShaderAlpha = "    ALPHA = alpha;\n";

// Worn by a limb still at kDefaultSkin, the colour spawnCharacter set, with the Plastic material.
bool PulseBlockzWorld::wears_skin(const Part& p) const {
    const Col3 k = pulseblockz::rbx::kDefaultSkin;
    return p.role == ROLE_LIMB && !p.tool && !p.welded && p.material == "Plastic" && p.transparency < 1 &&
           std::fabs(p.color.r - k.r) < 1e-3f && std::fabs(p.color.g - k.g) < 1e-3f && std::fabs(p.color.b - k.b) < 1e-3f;
}

// Set the limb's gradient line from its rest offset inside the root body.
void PulseBlockzWorld::update_skin(Part& p) {
    if (p.skin.is_null()) return;
    // 45 degrees like the logo: from the outer-bottom corner of the left foot
    // (x + y = -4, the body's lowest point along the diagonal) to the top-right
    // of the head / right shoulder (x + y = 3), in root space.
    const Vector3 start(-1, -3, 0), end(2.5f, 0.5f, 0);
    Vector3 d = end - start;
    d /= d.length_squared();                              // dot(P - start, d): 0 at the foot corner, 1 at the top-right corner
    Transform3D local = p.offset * p.shapeLocal;          // mesh space -> root space
    p.skin->set_shader_parameter("g_origin", local.affine_inverse().xform(start));
    p.skin->set_shader_parameter("g_dir", local.basis.inverse().xform(d));
    p.skin->set_shader_parameter("alpha", 1.0f - p.transparency);
    p.skin->set_shader_parameter("ramp", skinRamp_);
    // Only a head wears a face, and only the default skin draws one.
    auto it = entries_.find(p.id);
    p.skin->set_shader_parameter("face", it != entries_.end() && it->second.name == "Head" ? 1.0f : 0.0f);
}

void PulseBlockzWorld::update_material(Part& p) {
    // A part waiting on a picture is looked at again next frame.
    {
        std::string want = p.textureId;
        if (p.dataMesh) if (auto it = dataMeshes_.find(p.dataMesh); it != dataMeshes_.end() && it->second.type == "FileMesh") want = it->second.textureId;
        if (!want.empty() && load_texture(want).is_null()) awaitingTexture_.insert(p.id);
        else awaitingTexture_.erase(p.id);
    }
    if (!p.mesh) return;
    if (!wears_skin(p)) {
        p.skin.unref();
        Ref<Material> m = material_for(p);
        p.mesh->set_material_override(m);
        // Studs are the part's own colour and material, as they are in Roblox: a red brick
        // has red studs, and a neon one glows out of them too.
        if (p.studs) p.studs->set_material_override(m);
        if (p.inlets) p.inlets->set_material_override(m);
        return;
    }
    if (skinRamp_.is_null()) {
        // The stops come from pulse_gradient.json -- the one definition the cane's orb and the
        // Engram are painted from too -- by way of the header SConstruct writes. Cubic through
        // OKLab, so the blends stay clean and nothing reads as a stripe.
        Ref<Gradient> g; g.instantiate();
        PackedFloat32Array offs; PackedColorArray cols;
        for (const PulseGradientStop& s : kPulseGradient) { offs.push_back(s.t); cols.push_back(Color(s.r, s.g, s.b)); }
        g->set_offsets(offs);
        g->set_colors(cols);
        g->set_interpolation_mode(Gradient::GRADIENT_INTERPOLATE_CUBIC);
        g->set_interpolation_color_space(Gradient::GRADIENT_COLOR_SPACE_OKLAB);
        skinRamp_.instantiate();
        skinRamp_->set_width(512);
        skinRamp_->set_gradient(g);
    }
    int v = p.transparency > 0 ? 1 : 0;
    if (skinShader_[v].is_null()) {
        skinShader_[v].instantiate();
        skinShader_[v]->set_code(String(kSkinShader) + (v ? kSkinShaderAlpha : "") + "}\n");
    }
    if (p.skin.is_null() || p.skin->get_shader() != skinShader_[v]) {
        p.skin.instantiate();
        p.skin->set_shader(skinShader_[v]);
    }
    update_skin(p);
    p.mesh->set_material_override(p.skin);
}

// ---- characters --------------------------------------------------------------------
// Roblox's default controls: WASD/arrows relative to the camera, space to jump,
// Humanoid.WalkSpeed / JumpPower / AutoRotate honoured. Every character with a
// built root is stepped here (NPCs stand still unless a script moves them).
void PulseBlockzWorld::_physics_process(double delta) {
    if ((!server_ && !client_) || editMode_ || Engine::get_singleton()->is_editor_hint()) return;
    if (debugPausedServer_ || debugPausedClient_) return;   // stopped in the debugger: nothing moves
    step_characters(delta);
    step_buoyancy(delta);
    if (!constraints_.empty()) step_constraints(delta);
    step_vehicle_seats(delta);
}

// A VehicleSeat drives the assembly it is part of, as Roblox's does without a wheel in sight:
// Throttle toward MaxSpeed along the seat's flat look vector, Steer turning it at TurnSpeed,
// each reached at Torque per second. The runtime reads the occupant's controls into Throttle /
// Steer and zeroes them when they stand.
void PulseBlockzWorld::step_vehicle_seats(double dt) {
    if (netClient_) return;
    for (auto& [id, p] : parts_) {
        if (p.className != "VehicleSeat" || (p.throttle == 0 && p.steer == 0) || !in_workspace(id)) continue;
        int64_t r = weld_root(id) ? weld_root(id) : id;
        auto it = parts_.find(r);
        if (it == parts_.end()) continue;
        auto* rb = Object::cast_to<RigidBody3D>(it->second.body);
        if (!rb) continue;
        Vector3 look = -part_frame(p).basis.get_column(2);
        look.y = 0;
        if (look.length_squared() < 1e-6) continue;
        look.normalize();
        double m = rb->get_mass(), step = p.driveTorque * dt;   // the velocity a step may add
        Vector3 v = rb->get_linear_velocity();
        double want = p.throttle * p.maxSpeed, have = v.dot(look), dv = std::clamp(want - have, -step, step);
        rb->apply_central_impulse(look * (real_t)(dv * m));
        Vector3 w = rb->get_angular_velocity();
        double wantW = -p.steer * p.turnSpeed, dw = std::clamp(wantW - w.y, -step, step);
        auto* st = PhysicsServer3D::get_singleton()->body_get_direct_state(rb->get_rid());
        Basis invI = st ? st->get_inverse_inertia_tensor() : Basis();
        if (std::fabs(invI.determinant()) > 1e-12) rb->apply_torque_impulse(invI.inverse().xform(Vector3(0, (real_t)dw, 0)));
    }
}

// A loose body weighs what Roblox's GetMass() says: Size's volume at the material's density
// (Plastic 0.7), an assembly the sum of its parts. So a script's GetMass() * Gravity holds a
// part up, and a force or torque moves it as it would there.
void PulseBlockzWorld::refresh_masses() {
    massDirty_ = false;
    std::unordered_map<int64_t, double> mass;
    for (auto& [id, p] : parts_) {
        if (!in_workspace(id) || p.role == ROLE_LIMB && character_of(id)) continue;
        int64_t root = weld_root(id) ? weld_root(id) : id;
        double density = physicsOf(p.material, p.custom).density;   // Plastic 0.7, Metal 7.85, ...
        mass[root] += p.massless ? 0 : (double)p.size.x * p.size.y * p.size.z * density;
    }
    for (auto& [id, p] : parts_)
        if (auto* rb = Object::cast_to<RigidBody3D>(p.body)) {
            auto m = mass.find(id);
            if (m != mass.end()) { rb->set_mass((real_t)std::max(1e-3, m->second)); rb->set_inertia(Vector3()); }   // a hinged body's floor is worked out again
        }
}

// A part rubs as its material does (Plastic 0.3, Ice 0.02, Concrete 0.7), unless
// CustomPhysicalProperties says otherwise; Roblox's friction weights are not modelled. Bounce
// stays 0, since Godot's restitution makes a Plastic floor at 0.5 a trampoline a character
// cannot jump off. Materials that rub alike share one PhysicsMaterial.
void PulseBlockzWorld::update_surface(Part& p) {
    if (!p.body) return;
    MaterialPhysics m = physicsOf(p.material, p.custom);
    Ref<PhysicsMaterial>& mat = physMats_[(uint64_t)(m.friction * 1000 + 0.5)];
    if (mat.is_null()) { mat.instantiate(); mat->set_friction((real_t)m.friction); mat->set_bounce(0); }
    if (auto* sb = Object::cast_to<StaticBody3D>(p.body)) sb->set_physics_material_override(mat);
    else if (auto* rb = Object::cast_to<RigidBody3D>(p.body)) rb->set_physics_material_override(mat);
}

// A PrismaticConstraint's / CylindricalConstraint's actuator, the joint itself
// holding the pair on the axis: a Motor pushes toward Velocity (AngularVelocity)
// under MotorMaxForce (MotorMaxTorque), a Servo toward TargetPosition
// (TargetAngle) at up to Speed (AngularSpeed) under ServoMaxForce (ServoMaxTorque).
void PulseBlockzWorld::step_sliders(double dt) {
    for (auto& [id, k] : constraints_) {
        bool prismatic = k.className == "PrismaticConstraint", cylindrical = k.className == "CylindricalConstraint";
        if (!(prismatic || cylindrical) || !k.enabled || netClient_ || !k.node) continue;
        bool drive = k.actuator == "Motor" || k.actuator == "Servo";
        bool turn = cylindrical && (k.angularActuator == "Motor" || k.angularActuator == "Servo");
        if (!drive && !turn) continue;
        Transform3D l0, l1;
        int64_t p0 = attachment_part(k.att0, l0), p1 = attachment_part(k.att1, l1);
        if (!p0 || !p1) continue;
        int64_t r0 = weld_root(p0) ? weld_root(p0) : p0, r1 = weld_root(p1) ? weld_root(p1) : p1;
        auto a = parts_.find(r0), b = parts_.find(r1);
        if (a == parts_.end() || b == parts_.end()) continue;
        auto* rb0 = Object::cast_to<RigidBody3D>(a->second.body);
        auto* rb1 = Object::cast_to<RigidBody3D>(b->second.body);
        if (!rb0 && !rb1) continue;
        Transform3D w0 = part_frame(parts_[p0]) * l0, w1 = part_frame(parts_[p1]) * l1;
        Vector3 axis = w0.basis.get_column(0).normalized();
        double m0 = rb0 ? rb0->get_mass() : 0, m1 = rb1 ? rb1->get_mass() : 0;
        double meff = (rb0 && rb1) ? 1.0 / (1.0 / m0 + 1.0 / m1) : (rb0 ? m0 : m1);
        if (drive) {
            double pos = slider_position(w0, w1);
            double v = (rb1 ? rb1->get_linear_velocity() : Vector3()).dot(axis) - (rb0 ? rb0->get_linear_velocity() : Vector3()).dot(axis);
            double want = k.velocity;
            if (k.actuator == "Servo") want = std::max(-k.speed, std::min(k.speed, (k.targetPosition - pos) * 6.0));
            // MotorMaxAcceleration caps the change in speed; Roblox's default is no limit.
            if (k.motorMaxAccel > 0) want = std::max(v - k.motorMaxAccel * dt, std::min(v + k.motorMaxAccel * dt, want));
            // gravity's share of the coming step is already spoken for (it lands before the solver)
            double g = ((rb1 ? -gravity_ : 0.0) - (rb0 ? -gravity_ : 0.0)) * dt * axis.y;
            double cap = (k.actuator == "Servo" ? k.servoMaxForce : k.motorMaxForce) * dt;
            double j = std::max(-cap, std::min(cap, (want - v - g) * meff));
            if (rb1) rb1->apply_central_impulse(axis * (real_t)j);
            if (rb0) rb0->apply_central_impulse(axis * (real_t)-j);
        }
        if (turn) {
            double angle = hinge_angle(w0, w1);
            double w = (rb1 ? rb1->get_angular_velocity() : Vector3()).dot(axis) - (rb0 ? rb0->get_angular_velocity() : Vector3()).dot(axis);
            double want = k.angularVelocity;
            if (k.angularActuator == "Servo") want = std::max(-k.angularSpeed, std::min(k.angularSpeed, (Math::deg_to_rad(k.targetAngle) - angle) * 6.0));
            if (k.motorMaxAccel > 0) want = std::max(w - k.motorMaxAccel * dt, std::min(w + k.motorMaxAccel * dt, want));
            double cap = (k.angularActuator == "Servo" ? k.servoMaxTorque : k.motorMaxTorque) * dt;
            double dw = want - w;
            auto spin = [&](RigidBody3D* rb, double sign) {
                if (!rb) return;
                auto* st = PhysicsServer3D::get_singleton()->body_get_direct_state(rb->get_rid());
                Basis invI = st ? st->get_inverse_inertia_tensor() : Basis();
                if (std::fabs(invI.determinant()) < 1e-12) return;
                Vector3 imp = invI.inverse().xform(axis * (real_t)(dw * sign));
                double n = imp.length();
                if (n > cap && n > 0) imp *= (real_t)(cap / n);
                rb->apply_torque_impulse(imp);
            };
            spin(rb1, 1); spin(rb0, -1);
        }
    }
}

// A RopeConstraint (no further apart than Length), a RodConstraint (Length apart) and a
// SpringConstraint (a force toward FreeLength, damped) between two attachments, kept by impulses
// at the attachment points each step: the separating velocity is taken out, a rope's Restitution
// bouncing it, and the overshoot pulled back over a few steps. An anchored side is immovable and
// a character's part is not joined.
void PulseBlockzWorld::step_constraints(double dt) {
    step_movers(dt);
    sync_no_collide();
    step_sliders(dt);
    // A ball socket's friction: Godot's cone-twist and pin joints have no friction parameter,
    // so it is a torque opposing the relative spin, capped at MaxFrictionTorque.
    for (auto& [id, k] : constraints_) {
        if (k.className != "BallSocketConstraint" || !k.enabled || netClient_ || k.maxTorque <= 0) continue;
        Transform3D l0, l1;
        int64_t p0 = attachment_part(k.att0, l0), p1 = attachment_part(k.att1, l1);
        if (!p0 || !p1 || !in_workspace(id)) continue;
        int64_t r0 = weld_root(p0) ? weld_root(p0) : p0, r1 = weld_root(p1) ? weld_root(p1) : p1;
        if (r0 == r1) continue;
        auto a = parts_.find(r0), b = parts_.find(r1);
        if (a == parts_.end() || b == parts_.end()) continue;
        auto* rb0 = Object::cast_to<RigidBody3D>(a->second.body);
        auto* rb1 = Object::cast_to<RigidBody3D>(b->second.body);
        if (!rb0 && !rb1) continue;
        Vector3 rel = (rb1 ? rb1->get_angular_velocity() : Vector3()) - (rb0 ? rb0->get_angular_velocity() : Vector3());
        if (rel.length_squared() < 1e-8) continue;
        auto rub = [&](RigidBody3D* rb, double sign) {
            if (!rb) return;
            auto* st = PhysicsServer3D::get_singleton()->body_get_direct_state(rb->get_rid());
            Basis invI = st ? st->get_inverse_inertia_tensor() : Basis();
            if (std::fabs(invI.determinant()) < 1e-12) return;
            // Enough to stop it this step, and no more than the friction allows -- so a
            // strong friction locks the joint and a weak one only slows it.
            Vector3 imp = invI.inverse().xform(rel * (real_t)-sign);
            double n = imp.length(), cap = k.maxTorque * dt;
            if (n > cap && n > 0) imp *= (real_t)(cap / n);
            rb->apply_torque_impulse(imp);
        };
        rub(rb1, 1); rub(rb0, -1);
    }
    for (auto& [id, k] : constraints_) {
        bool rope = k.className == "RopeConstraint", rod = k.className == "RodConstraint", spring = k.className == "SpringConstraint";
        if (!(rope || rod || spring) || !k.enabled || netClient_) continue;
        Transform3D l0, l1;
        int64_t p0 = attachment_part(k.att0, l0), p1 = attachment_part(k.att1, l1);
        if (!p0 || !p1 || !in_workspace(id) || !in_workspace(p0) || !in_workspace(p1)) continue;
        int64_t r0 = weld_root(p0) ? weld_root(p0) : p0, r1 = weld_root(p1) ? weld_root(p1) : p1;
        if (r0 == r1) continue;
        auto a = parts_.find(r0), b = parts_.find(r1);
        if (a == parts_.end() || b == parts_.end() || !a->second.body || !b->second.body) continue;
        auto* rb0 = Object::cast_to<RigidBody3D>(a->second.body);
        auto* rb1 = Object::cast_to<RigidBody3D>(b->second.body);
        if (!rb0 && !rb1) continue;
        Vector3 x0 = (part_frame(parts_[p0]) * l0).origin, x1 = (part_frame(parts_[p1]) * l1).origin;
        Vector3 d = x1 - x0;
        double dist = d.length();
        if (dist < 1e-6) continue;
        Vector3 n = d / (real_t)dist;
        auto velAt = [](RigidBody3D* rb, const Vector3& p) { return rb ? rb->get_linear_velocity() + rb->get_angular_velocity().cross(p - rb->get_global_position()) : Vector3(); };
        Vector3 dv = velAt(rb1, x1) - velAt(rb0, x0);
        double vrel = dv.dot(n);   // > 0: moving apart
        double m0 = rb0 ? rb0->get_mass() : 0, m1 = rb1 ? rb1->get_mass() : 0;
        double meff = (rb0 && rb1) ? 1.0 / (1.0 / m0 + 1.0 / m1) : (rb0 ? m0 : m1);
        // the separation this step will add before the bodies move: gravity on the
        // moving side(s) and the swing's centripetal pull, taken out ahead of time
        Vector3 g(0, (real_t)-gravity_, 0);
        double vpred = vrel + ((rb1 ? g : Vector3()) - (rb0 ? g : Vector3())).dot(n) * dt + (dv - n * (real_t)vrel).length_squared() / dist * dt;
        // an impulse j along n on the pair: body 1 pulled by -j n, body 0 by +j n
        auto pull = [&](double j) {
            if (rb1) rb1->apply_impulse(n * (real_t)-j, x1 - rb1->get_global_position());
            if (rb0) rb0->apply_impulse(n * (real_t)j, x0 - rb0->get_global_position());
        };
        // hold the distance where it is `over` past the limit: kill the velocity
        // through it (bounced by e), pull the overshoot back a fifth per step;
        // side -1 only keeps the points from separating, +1 from closing, 0 both
        auto hold = [&](double over, double e, int side, double cap = 0) {
            double j = 0;
            if (side <= 0 && over >= 0 && vpred > 0) j += (1 + e) * vpred * meff;
            if (side >= 0 && over <= 0 && vpred < 0) j += (1 + e) * vpred * meff;
            j += over * 0.2 / dt * meff;
            // A winch pulls only as hard as WinchForce allows.
            if (cap > 0) j = std::max(-cap * dt, std::min(cap * dt, j));
            pull(j);
        };
        const double slack = 0.02;   // taut a hair short of the length, so it never sags through it
        if (rope) {
            if (k.winch && k.winchSpeed > 0) {
                // WinchResponsiveness is a first-order lag on the winding speed: a low one
                // spools up, and Roblox's default of 10 is most of the way there in one step.
                const double gain = std::clamp(std::max(k.winchResponsiveness, 0.0) * dt, 0.0, 1.0);
                k.winchNow += (k.winchSpeed - k.winchNow) * gain;
                double len = k.length, step = k.winchNow * dt;
                double next = std::fabs(k.winchTarget - len) <= step ? k.winchTarget : len + (k.winchTarget > len ? step : -step);
                if (next != len) { k.length = next; pendingWrites_.push_back({id, "Length", Value::number(next)}); }
            } else k.winchNow = 0;
            if (dist > k.length - slack)
                hold(std::max(0.0, dist - k.length), k.restitution, -1, k.winch ? k.winchForce : 0);
        } else if (rod) {
            hold(dist - k.length, 0, 0);
            // LimitAngle0 / LimitAngle1: how far the rod may swing from each attachment's own
            // axis -- Roblox draws a cone at each end and the rod stays inside both.
            if (k.limits) {
                Transform3D f0 = part_frame(parts_[p0]) * l0, f1 = part_frame(parts_[p1]) * l1;
                // The rod leaves attachment 0 along +n and arrives at attachment 1 along -n.
                auto coneHold = [&](const Basis& frame, Vector3 along, double limitDeg, RigidBody3D* far, const Vector3& farAt) {
                    if (!far || limitDeg >= 179.0) return;
                    Vector3 axis = frame.get_column(0).normalized();
                    double c = std::clamp((double)axis.dot(along), -1.0, 1.0);
                    double angle = std::acos(c);
                    double limit = Math::deg_to_rad(std::max(limitDeg, 0.0));
                    if (angle <= limit) return;
                    // Where the far end would be if it sat on the edge of the cone: turn the
                    // rod back toward the axis by the overshoot, about the axis they share.
                    Vector3 turn = axis.cross(along);
                    if (turn.length_squared() < 1e-10) return;
                    Vector3 want = Basis(turn.normalized(), (real_t)(limit - angle)).xform(along) * (real_t)dist;
                    Vector3 err = (frame.get_column(0) * 0 + want) - along * (real_t)dist;
                    // A fifth of the way back per step, as the distance hold does: firm
                    // enough to stop it, soft enough not to fling anything.
                    far->apply_impulse(err * (real_t)(0.2 / dt * meff), farAt - far->get_global_position());
                };
                coneHold(f0.basis, n, k.limitAngle0, rb1, x1);
                coneHold(f1.basis, -n, k.limitAngle1, rb0, x0);
            }
        } else {
            double f = k.stiffness * (k.freeLength - dist) - k.damping * vrel;   // > 0 pushes apart
            f = std::max(-k.maxForce, std::min(k.maxForce, f));
            pull(-f * dt);
            if (k.limits) {
                if (dist > k.maxLength - slack) hold(std::max(0.0, dist - k.maxLength), 0, -1);
                else if (dist < k.minLength + slack) hold(std::min(0.0, dist - k.minLength), 0, 1);
            }
        }
        if (std::fabs(dist - k.sentDist) > 0.01) { pendingWrites_.push_back({id, spring ? "CurrentLength" : "CurrentDistance", Value::number(dist)}); k.sentDist = dist; }
    }
}

// A NoCollisionConstraint is a collision exception between its parts' bodies.
// Bodies come and go as assemblies are rebuilt, so this runs every step and
// re-applies when the pair is not the one it was put on; idempotent otherwise.
void PulseBlockzWorld::no_collide_clear(NoCollide& n) {
    auto* a = Object::cast_to<PhysicsBody3D>(ObjectDB::get_instance(n.a));
    auto* b = Object::cast_to<PhysicsBody3D>(ObjectDB::get_instance(n.b));
    if (a && b) { a->remove_collision_exception_with(b); b->remove_collision_exception_with(a); }
    n.a = n.b = 0;
}

void PulseBlockzWorld::sync_no_collide() {
    for (auto& [id, n] : noCollide_) {
        PhysicsBody3D* a = nullptr;
        PhysicsBody3D* b = nullptr;
        if (n.enabled && n.p0 && n.p1 && in_workspace(id)) {
            int64_t r0 = weld_root(n.p0) ? weld_root(n.p0) : n.p0, r1 = weld_root(n.p1) ? weld_root(n.p1) : n.p1;
            if (r0 != r1) {
                auto i0 = parts_.find(r0), i1 = parts_.find(r1);
                if (i0 != parts_.end() && i1 != parts_.end()) { a = Object::cast_to<PhysicsBody3D>(i0->second.body); b = Object::cast_to<PhysicsBody3D>(i1->second.body); }
            }
        }
        if (!a || !b) { if (n.a || n.b) no_collide_clear(n); continue; }
        if (n.a == a->get_instance_id() && n.b == b->get_instance_id()) continue;
        no_collide_clear(n);
        a->add_collision_exception_with(b);
        b->add_collision_exception_with(a);
        n.a = a->get_instance_id();
        n.b = b->get_instance_id();
    }
}

// The movers act on Attachment0's assembly each physics step: a VectorForce or Torque as it
// says, in Attachment0's, Attachment1's or the world's frame; a LinearVelocity or AngularVelocity
// as the impulse that reaches the velocity, under MaxForce / MaxTorque; an AlignPosition or
// AlignOrientation toward Attachment1 (a Position / CFrame with OneAttachment) as a critically
// damped spring of rate Responsiveness, or straight there when RigidityEnabled.
void PulseBlockzWorld::step_movers(double dt) {
    for (auto& [id, k] : constraints_) {
        const std::string& cls = k.className;
        bool vforce = cls == "VectorForce", torque = cls == "Torque", linear = cls == "LinearVelocity", angular = cls == "AngularVelocity", alignPos = cls == "AlignPosition", alignOri = cls == "AlignOrientation";
        bool lineForce = cls == "LineForce";
        // The legacy BodyMovers sit in the part itself, no Attachment: its assembly is the one moved.
        bool mover = cls == "BodyGyro" || cls == "BodyVelocity" || cls == "BodyPosition" || cls == "BodyAngularVelocity" || cls == "BodyForce" || cls == "BodyThrust";
        if (!(vforce || torque || linear || angular || alignPos || alignOri || lineForce || mover) || !k.enabled || netClient_) continue;
        Transform3D l0, l1;
        int64_t p0 = 0, p1 = 0;
        if (mover) { auto e = entries_.find(id); if (e != entries_.end() && parts_.count(e->second.parent)) p0 = e->second.parent; }
        else { p0 = attachment_part(k.att0, l0); p1 = k.att1 ? attachment_part(k.att1, l1) : 0; }
        if (!p0 || !in_workspace(id) || !in_workspace(p0)) continue;
        int64_t r0 = weld_root(p0) ? weld_root(p0) : p0;
        auto a = parts_.find(r0);
        if (a == parts_.end()) continue;
        auto* rb = Object::cast_to<RigidBody3D>(a->second.body);
        if (!rb) continue;
        Transform3D w0 = part_frame(parts_[p0]) * l0;
        Transform3D w1 = p1 && in_workspace(p1) ? part_frame(parts_[p1]) * l1 : Transform3D();
        bool have1 = p1 && in_workspace(p1);
        auto inFrame = [&](const Vec3& v) {   // RelativeTo
            Vector3 g = toGd(v);
            if (k.relativeTo == "Attachment0") return w0.basis.xform(g);
            if (k.relativeTo == "Attachment1") return have1 ? w1.basis.xform(g) : g;
            return g;
        };
        double m = rb->get_mass();
        Vector3 com = rb->get_global_transform().xform(rb->get_center_of_mass());
        Vector3 gdt(0, (real_t)(-gravity_ * dt), 0);   // what this step's gravity adds after the impulse: taken out ahead
        Vector3 at = k.atCenterOfMass ? com : w0.origin;
        auto capped = [](Vector3 v, double cap) { double l = v.length(); return cap > 0 && l > cap ? v * (real_t)(cap / l) : v; };
        // Magnitude caps the whole vector; PerAxis caps each axis on its own, in the frame
        // ForceRelativeTo names, so one axis may hold while another gives way.
        auto limited = [&](Vector3 v, double cap, double dtScale) {
            if (k.forceLimitMode != "PerAxis") return capped(v, cap * dtScale);
            Basis f = k.relativeTo == "Attachment0" ? w0.basis
                    : k.relativeTo == "Attachment1" && have1 ? w1.basis : Basis();
            Vector3 local = f.inverse().xform(v);
            const Vec3& m = k.maxAxesForce;
            local = Vector3(std::clamp(local.x, (real_t)(-std::fabs(m.x) * dtScale), (real_t)(std::fabs(m.x) * dtScale)),
                            std::clamp(local.y, (real_t)(-std::fabs(m.y) * dtScale), (real_t)(std::fabs(m.y) * dtScale)),
                            std::clamp(local.z, (real_t)(-std::fabs(m.z) * dtScale), (real_t)(std::fabs(m.z) * dtScale)));
            return f.xform(local);
        };
        // The other end: Roblox applies the equal-and-opposite to Attachment1's assembly only
        // when the constraint says to, so by default the other body never notices.
        RigidBody3D* other = nullptr;
        if (k.reaction && have1) {
            int64_t r1 = weld_root(p1) ? weld_root(p1) : p1;
            if (auto b = parts_.find(r1); b != parts_.end() && b->first != r0)
                other = Object::cast_to<RigidBody3D>(b->second.body);
        }
        auto* st = PhysicsServer3D::get_singleton()->body_get_direct_state(rb->get_rid());
        // an angular impulse that changes the angular velocity by dw
        auto angularImpulse = [&](Vector3 dw) {
            Basis invI = st ? st->get_inverse_inertia_tensor() : Basis();
            if (std::fabs(invI.determinant()) < 1e-12) return Vector3();   // not yet stepped: no inertia to speak of
            return invI.inverse().xform(dw);
        };
        if (mover) {
            // Each is a proportional pull toward its goal, capped per world axis by MaxForce /
            // MaxTorque as Roblox's are. A step never carries the assembly past the goal (P is a
            // rate, not a spring stiffness, at this step size), and a BodyVelocity or
            // BodyPosition holds against gravity when its cap allows.
            auto clampAxes = [](Vector3 v, const Vec3& cap) {
                return Vector3(std::clamp(v.x, (real_t)-std::fabs(cap.x), (real_t)std::fabs(cap.x)), std::clamp(v.y, (real_t)-std::fabs(cap.y), (real_t)std::fabs(cap.y)),
                               std::clamp(v.z, (real_t)-std::fabs(cap.z), (real_t)std::fabs(cap.z)));
            };
            auto perStep = [&](const Vec3& cap) { return Vec3{(float)(cap.x * dt), (float)(cap.y * dt), (float)(cap.z * dt)}; };   // a force cap as this step's impulse cap
            // a change of velocity toward err, at most err itself, at most P * err * dt / m of it
            auto towards = [&](Vector3 err, double gain) {
                double f = std::min(1.0, gain * dt / std::max(m, 1e-6));
                return err * (real_t)f;
            };
            if (cls == "BodyForce") rb->apply_central_force(toGd(k.force));
            else if (cls == "BodyThrust") rb->apply_force(w0.basis.xform(toGd(k.force)), w0.xform(toGd(k.location)) - rb->get_global_position());
            else if (cls == "BodyVelocity") {
                Vector3 dv = towards(toGd(k.vel) - rb->get_linear_velocity(), k.pGain) - gdt;
                rb->apply_central_impulse(clampAxes(dv * (real_t)m, perStep(k.maxForceV)));
            } else if (cls == "BodyPosition") {
                Vector3 err = toGd(k.position) - com, v = rb->get_linear_velocity();
                double kp = k.pGain / std::max(m, 1e-6), kd = k.dGain / std::max(m, 1e-6);
                Vector3 dv = (err * (real_t)(kp * dt) - v * (real_t)(kd * dt)) / (real_t)(1 + kd * dt);   // the damping implicit, so a stiff one holds
                Vector3 next = v + dv;
                double reach = err.length() / dt;                                                        // there next step, no further
                if (next.length() > reach) dv = next.normalized() * (real_t)reach - v;
                dv -= gdt;
                rb->apply_central_impulse(clampAxes(dv * (real_t)m, perStep(k.maxForceV)));
            } else if (cls == "BodyAngularVelocity") {
                Vector3 dw = towards(toGd(k.angVel) - rb->get_angular_velocity(), k.pGain);
                rb->apply_torque_impulse(clampAxes(angularImpulse(dw), perStep(k.maxTorqueV)));
            } else {   // BodyGyro: turned toward CFrame's orientation, about the shortest axis
                Basis goal = toTransform(Vec3{0, 0, 0}, k.goalOrient).basis;
                Quaternion q = (goal * w0.basis.inverse()).get_rotation_quaternion().normalized();
                double angle = q.get_angle();
                Vector3 axis = angle > 1e-6 ? q.get_axis().normalized() : Vector3();
                if (angle > Math_PI) { angle = 2 * Math_PI - angle; axis = -axis; }
                Vector3 w = rb->get_angular_velocity();
                Vector3 dw = (axis * (real_t)(angle * k.pGain * dt) - w * (real_t)(k.dGain * dt)) / (real_t)(1 + k.dGain * dt);
                Vector3 next = w + dw;
                double reach = angle / dt;
                if (next.length() > reach) dw = next.normalized() * (real_t)reach - w;
                rb->apply_torque_impulse(clampAxes(angularImpulse(dw), perStep(k.maxTorqueV)));
            }
            continue;
        }
        if (vforce) {
            rb->apply_force(inFrame(k.force), at - rb->get_global_position());
        } else if (lineForce) {
            // Magnitude along the line from Attachment0 to Attachment1 (over the squared distance
            // when InverseSquareLaw), capped by MaxForce; the reaction on Attachment1's assembly.
            if (!have1) continue;
            Vector3 d = w1.origin - w0.origin;
            double dist = d.length();
            if (dist < 1e-6) continue;
            double mag = k.inverseSquare ? k.magnitude / (dist * dist) : k.magnitude;
            Vector3 f = capped(d * (real_t)(mag / dist), k.maxForce);
            rb->apply_force(f, at - rb->get_global_position());
            if (other) other->apply_force(-f, w1.origin - other->get_global_position());
        } else if (torque) {
            rb->apply_torque(inFrame(k.torque));
        } else if (linear) {
            Vector3 v = rb->get_linear_velocity(), dv;
            if (k.velocityMode == "Line") {
                Vector3 d = inFrame(k.lineDir).normalized();
                dv = d * (real_t)(k.lineVelocity - v.dot(d));
            } else if (k.velocityMode == "Plane") {
                Vector3 t0 = inFrame(k.tangent0).normalized(), t1 = inFrame(k.tangent1).normalized();
                dv = t0 * (real_t)(k.planeVel[0] - v.dot(t0)) + t1 * (real_t)(k.planeVel[1] - v.dot(t1));
            } else dv = inFrame(k.vel) - v;
            if (k.velocityMode == "Line") dv -= inFrame(k.lineDir).normalized() * gdt.dot(inFrame(k.lineDir).normalized());
            else if (k.velocityMode == "Plane") { Vector3 t0 = inFrame(k.tangent0).normalized(), t1 = inFrame(k.tangent1).normalized(); dv -= t0 * gdt.dot(t0) + t1 * gdt.dot(t1); }
            else dv -= gdt;
            Vector3 impulse = limited(dv * (real_t)m, k.maxForce, dt);
            rb->apply_central_impulse(impulse);
            if (other) other->apply_central_impulse(-impulse);
        } else if (angular) {
            Vector3 dw = inFrame(k.angVel) - rb->get_angular_velocity();
            Vector3 twist = capped(angularImpulse(dw), k.maxTorque * dt);
            rb->apply_torque_impulse(twist);
            if (other) other->apply_torque_impulse(-twist);
        } else if (alignPos) {
            Vector3 goal = k.twoAttachment ? (have1 ? w1.origin : at) : toGd(k.position);
            Vector3 err = goal - at;
            Vector3 v = rb->get_linear_velocity() + rb->get_angular_velocity().cross(at - com);
            Vector3 dv;
            if (k.rigidity) dv = capped(err / (real_t)dt, k.maxVelocity) - v - gdt;   // there next step
            else {
                double r = k.responsiveness;
                dv = (err * (real_t)(r * r) - v * (real_t)(2 * r)) * (real_t)dt;   // critically damped
                Vector3 next = v + dv;
                if (next.length() > k.maxVelocity) dv = next.normalized() * (real_t)k.maxVelocity - v;
            }
            Vector3 push = k.rigidity ? dv * (real_t)m : limited(dv * (real_t)m, k.maxForce, dt);
            rb->apply_impulse(push, at - rb->get_global_position());
            if (other) other->apply_impulse(-push, at - other->get_global_position());
        } else if (alignOri) {
            Basis goal = k.twoAttachment ? (have1 ? w1.basis : w0.basis) : toTransform(Vec3{0, 0, 0}, k.goalOrient).basis;
            Vector3 axis; double angle;
            if (k.primaryAxisOnly) {
                Vector3 x0 = w0.basis.get_column(0).normalized(), x1 = goal.get_column(0).normalized();
                axis = x0.cross(x1); angle = std::acos(std::clamp((double)x0.dot(x1), -1.0, 1.0));
                if (axis.length_squared() < 1e-12) { if (angle < 1e-6) continue; axis = x0.cross(Vector3(0, 1, 0)); if (axis.length_squared() < 1e-6) axis = Vector3(0, 0, 1); }
                axis.normalize();
            } else {
                Quaternion q = (goal * w0.basis.inverse()).get_rotation_quaternion().normalized();
                angle = q.get_angle(); axis = angle > 1e-6 ? q.get_axis().normalized() : Vector3();
                if (angle > Math_PI) { angle = 2 * Math_PI - angle; axis = -axis; }
            }
            Vector3 w = rb->get_angular_velocity(), dw;
            if (k.rigidity) dw = capped(axis * (real_t)(angle / dt), k.maxAngularVelocity) - w;
            else {
                double r = k.responsiveness;
                dw = (axis * (real_t)(angle * r * r) - w * (real_t)(2 * r)) * (real_t)dt;
                Vector3 next = w + dw;
                if (next.length() > k.maxAngularVelocity) dw = next.normalized() * (real_t)k.maxAngularVelocity - w;
            }
            Vector3 twist = capped(angularImpulse(dw), k.rigidity ? 0 : k.maxTorque * dt);
            rb->apply_torque_impulse(twist);
            if (other) other->apply_torque_impulse(-twist);
        }
    }
}

void PulseBlockzWorld::step_characters(double dt) {
    step_tracks(dt);   // every playing AnimationTrack's clock, before the limbs are posed
    Input* input = Input::get_singleton();
    for (auto& [model, ch] : chars_) {
        if (!ch.root) continue;
        auto pit = parts_.find(ch.root);
        if (pit == parts_.end()) continue;
        auto* cb = Object::cast_to<CharacterBody3D>(pit->second.body);
        if (!cb) continue;
        // An anchored root holds the whole character still, as on Roblox: a mannequin, a corpse
        // or a paper doll with a Humanoid in it stays where it is put.
        if (pit->second.anchored) {
            cb->set_velocity(Vector3());
            ch.onFloor = true;
            ch.walking = false;
            continue;
        }
        if (ch.seat) {
            // Seated: the runtime's SeatWeld holds the HumanoidRootPart on the seat's
            // top; character joints stay out of the assemblies, so pin it here. A
            // jump (space, or a script's Jump = true) asks the runtime to stand it up.
            auto sit = parts_.find(ch.seat);
            if (sit != parts_.end() && sit->second.mesh) {
                Transform3D frame = sit->second.mesh->get_global_transform() * sit->second.shapeLocal.affine_inverse();
                cb->set_global_transform(frame * Transform3D(Basis(), Vector3(0, sit->second.size.y * 0.5f + 1.0f, 0)));
            }
            cb->set_velocity(Vector3());
            ch.onFloor = true;
            ch.walking = false;
            if (ch.state != "Seated") { ch.state = "Seated"; if (ch.humanoid) pendingWrites_.push_back({ch.humanoid, "StateName", Value::string("Seated")}); }
            if (!remote_char(model)) {
                Vec3 dir{0, 0, 0};
                if (model == localChar_ && defaultControls_ && !controls_scripted() && input && !chatTyping_ && !typingBox_ && !menuOpen_) {
                    // the controls still steer a VehicleSeat through MoveDirection
                    float f = (input->is_physical_key_pressed(KEY_W) || input->is_physical_key_pressed(KEY_UP) ? 1.f : 0.f)
                            - (input->is_physical_key_pressed(KEY_S) || input->is_physical_key_pressed(KEY_DOWN) ? 1.f : 0.f);
                    float r = (input->is_physical_key_pressed(KEY_D) || input->is_physical_key_pressed(KEY_RIGHT) ? 1.f : 0.f)
                            - (input->is_physical_key_pressed(KEY_A) || input->is_physical_key_pressed(KEY_LEFT) ? 1.f : 0.f);
                    Vector3 fwd = camera_forward();
                    Vector3 d = fwd * f + Vector3(-fwd.z, 0, fwd.x) * r;
                    if (d.length_squared() > 0) { d.normalize(); dir = fromGd(d); }
                    if (input->is_physical_key_pressed(KEY_SPACE)) ch.jump = true;
                    ch.moveDir = dir;
                }
                // Standing up asks the same question as jumping: the request, not the edge.
                if (ch.jump || ch.jumpProp) {
                    ch.jump = false;
                    ch.jumpProp = false;
                    ch.standUp = true;   // the hop, once the runtime lets go
                    // The runtime must SEE Jump go true: that edge is what unseats a Humanoid,
                    // where Runtime::unseat clears SeatPart and Sit and destroys the SeatWeld.
                    // Down, up, down -- down first because a write of the value a property
                    // already holds records no change, and Jump is often already true here.
                    // jumpConsumed stays off, or the trailing false would be written twice.
                    if (ch.humanoid) {
                        pendingWrites_.push_back({ch.humanoid, "Jump", Value::boolean(false)});
                        pendingWrites_.push_back({ch.humanoid, "Jump", Value::boolean(true)});
                        pendingWrites_.push_back({ch.humanoid, "Jump", Value::boolean(false)});
                    }
                    ch.jumpConsumed = false;
                }
            }
            if (defaultAnimations_) animate_character(ch, dt);
            continue;
        }
        if (remote_char(model)) {
            // Its owner moves it and reports the pose; this side eases toward it and animates.
            if (ch.posing) {
                ch.poseT += dt;
                double f = ch.poseDur > 0 ? std::min(ch.poseT / ch.poseDur, 1.0) : 1.0;
                cb->set_transform(ch.poseFrom.interpolate_with(ch.poseTo, (float)f));
                if (f >= 1.0) ch.posing = false;
            }
            // Whether a body somebody else simulates is standing comes from its owner: it
            // writes StateName -- Running, Freefall, Seated, Swimming -- and the property
            // replicates. Only the falling states count as falling; listing the standing ones
            // instead would give anything they forgot, Dead above all, the freefall pose.
            ch.onFloor = !(ch.state == "Freefall" || ch.state == "Jumping");
            if (defaultAnimations_) animate_character(ch, dt);
            continue;
        }
        Vec3 dir{0, 0, 0};
        bool onFloor = cb->is_on_floor();
        // Just thrown: airborne for this step whatever is underfoot. See Character::launched.
        if (ch.launched) { onFloor = false; ch.launched = false; }
        if (model == localChar_ && defaultControls_ && !controls_scripted() && input && !chatTyping_ && !typingBox_ && !menuOpen_) {
            float f = (input->is_physical_key_pressed(KEY_W) || input->is_physical_key_pressed(KEY_UP) ? 1.f : 0.f)
                    - (input->is_physical_key_pressed(KEY_S) || input->is_physical_key_pressed(KEY_DOWN) ? 1.f : 0.f);
            float r = (input->is_physical_key_pressed(KEY_D) || input->is_physical_key_pressed(KEY_RIGHT) ? 1.f : 0.f)
                    - (input->is_physical_key_pressed(KEY_A) || input->is_physical_key_pressed(KEY_LEFT) ? 1.f : 0.f);
            Vector3 fwd = camera_forward();
            Vector3 right(-fwd.z, 0, fwd.x);
            Vector3 d = fwd * f + right * r;
            if (d.length_squared() > 0) { d.normalize(); dir = fromGd(d); }
            if (input->is_physical_key_pressed(KEY_SPACE) && onFloor) ch.jump = true;
            // The controls own MoveDirection, every frame (Roblox's control script
            // calls Humanoid:Move each frame too); a key press ends a MoveTo.
            if (dir.x != 0 || dir.z != 0) ch.walking = false;
            if (!ch.walking) ch.moveDir = dir;
        }
        if (ch.walking) {
            // Humanoid:MoveTo: straight at the point (no pathing), done within half a
            // stud, given up after 8 s. The runtime hears MoveToFinished as a host event.
            Vector3 here = cb->get_position();
            float dx = ch.walkTo.x - (float)here.x, dz = ch.walkTo.z - (float)here.z;
            float dist = std::sqrt(dx * dx + dz * dz);
            ch.walkT += dt;
            if (dist < 0.5f || ch.walkT > 8.0) {
                ch.walking = false;
                events_.push_back({ch.humanoid, "MoveToFinished", {Value::boolean(dist < 0.5f)}});
            } else dir = {dx / dist, 0, dz / dist};
        } else dir = {ch.moveDir.x, 0, ch.moveDir.z};   // Humanoid:Move: until the next one
        Vector3 vel = cb->get_velocity();
        // In terrain water the character swims, as Roblox's does: no gravity, the
        // water damps the drop and floats it up until the head is out, a stroke
        // goes where the camera looks (the pitch too) and Space swims up.
        const Vector3 here3 = cb->get_position();
        const bool swimming = terrain_in_water(here3 + Vector3(0, -1.0f, 0));
        const bool headUnder = swimming && terrain_in_water(here3 + Vector3(0, 1.5f, 0));
        ch.swimming = swimming;
        if (swimming) {
            vel.y *= (float)std::max(0.0, 1.0 - 6.0 * dt);
            if (headUnder) vel.y += (float)(gravity_ * 0.6 * dt);
            if (ch.jump) { vel.y = (float)(ch.walkSpeed * 0.75); ch.jump = false; ch.jumpConsumed = true; }
            onFloor = false;
        } else {
            if (!onFloor) vel.y -= (float)(gravity_ * dt);
            // The request, not the edge: Humanoid.Jump is a flag this machine's keyboard, a
            // script or the server can write, and a write of true onto a true records no change.
            // The machine simulating the body reads the flag and puts it down the moment it
            // takes it, here rather than when the write-back comes round. Holding the key
            // re-jumps on landing, as Roblox does.
            if (ch.jump || ch.jumpProp) {
                if (onFloor) vel.y = (float)jump_speed(ch);
                ch.jump = false;
                ch.jumpProp = false;        // taken, right now: a later frame must not see it
                ch.jumpConsumed = true;     // and the runtime is told it is down
            }
        }
        if (ch.standUp) { vel.y = (float)jump_speed(ch); ch.standUp = false; }   // a jump off the seat: a hop, floor or not
        // The walk PUSHES toward its speed; it does not set it. Roblox's Humanoid applies a
        // limited force toward MoveDirection * WalkSpeed, which is why momentum carries through
        // a jump and a held direction still steers in the air. On the ground the push reads as
        // instant: walking speed inside a tenth of a second.
        //
        // In the air the rule is never to BRAKE. Below walking speed the push is the same as on
        // the ground; above it -- thrown by a trampoline, shoved -- the steering may turn the
        // velocity but takes no speed out of it, so a throw stays a throw.
        Vector3 want((float)(dir.x * ch.walkSpeed), 0, (float)(dir.z * ch.walkSpeed));
        Vector3 flat(vel.x, 0, vel.z);
        const float have = flat.length();
        const bool thrown = !onFloor && have > (float)ch.walkSpeed + 0.01f;
        if (!thrown) {
            // Walking, and every ordinary jump. Letting go of the keys on the ground stops
            // you; in the air there is nothing to slow you below a walk anyway.
            if (onFloor || want.length_squared() > 0.01f)
                flat = flat.move_toward(want, (float)(240.0 * dt));
        } else if (want.length_squared() > 0.01f) {
            // Thrown, and steering. Turn the direction and keep the speed: a quarter turn
            // over a second, which is enough to pick your landing on a long flight and not
            // enough to fly.
            Vector3 aim = flat / have, at = want.normalized();
            aim = aim.move_toward(at, (float)(AIR_TURN * dt));
            if (aim.length_squared() > 1e-6f) flat = aim.normalized() * have;
        }
        // else: thrown and not steering, so it carries. That is what makes a throw a throw.
        vel.x = flat.x;
        vel.z = flat.z;
        if (swimming && model == localChar_ && !ch.walking && camera_ && (dir.x != 0 || dir.z != 0)) {
            Vector3 look = -camera_->get_global_transform().basis.get_column(2);
            Vector3 flat = camera_forward();
            float ahead = (float)(dir.x * flat.x + dir.z * flat.z);   // how much of the stroke is forward
            vel.y += (float)(look.y * ahead * ch.walkSpeed);
        }
        // Walking into something is a touch too (a kinematic body has no contact
        // monitor): the lava's Touched sees the HumanoidRootPart.
        std::set<int64_t> touching;
        auto slid = [&]() {
            for (int i = 0; i < cb->get_slide_collision_count(); i++) {
                Ref<KinematicCollision3D> kc = cb->get_slide_collision(i);
                if (kc.is_null()) continue;
                auto* co = Object::cast_to<CollisionObject3D>(kc->get_collider());
                int64_t other = get_part_id(co);
                other = part_of_shape(co, kc->get_collider_shape_index(), other);
                if (other && other != ch.root && parts_.count(other)) touching.insert(other);
            }
        };
        Vector3 before = cb->get_position();
        cb->set_velocity(vel);
        cb->move_and_slide();
        slid();
        // A loose part resting against the character sinks a hair into it and Godot cancels the
        // whole sweep as stuck. Roblox's character carries or shoves such parts, so when the
        // body asked to move and went nowhere, sweep again against the anchored world alone;
        // the physics step pushes the loose part out.
        if (vel.length_squared() * dt * dt > 1e-8 && (cb->get_position() - before).length_squared() < 1e-10) {
            cb->set_collision_mask(static_bit());
            cb->set_velocity(vel);
            cb->move_and_slide();
            cb->set_collision_mask(static_bit() | rigid_bit());
            slid();
        }
        // Roblox's humanoid walks up onto anything under 2.5 studs high without a jump -- a
        // kerb, a stair, a chair's seat. Godot's capsule only rolls over what its round bottom
        // clears, so a walk stopped short is tried again from up to that much higher and settled
        // back onto whatever floor is there. The floor stepped onto counts as touched.
        {
            Vector3 want(vel.x * (float)dt, 0, vel.z * (float)dt);
            Vector3 moved = cb->get_position() - before; moved.y = 0;
            if (want.length_squared() > 1e-8f && moved.length_squared() < want.length_squared() * 0.25f && cb->is_on_wall() && cb->is_on_floor()) {
                const real_t kStep = 2.5f, kRadius = 1.0f;
                Ref<KinematicCollision3D> kc; kc.instantiate();
                Transform3D up = cb->get_global_transform();
                real_t rise = kStep;
                if (cb->test_move(up, Vector3(0, kStep, 0), kc)) rise = (real_t)kc->get_travel().y;
                if (rise > 0.05f) {
                    up.origin.y += rise;
                    // Past the capsule's own radius, so it comes down on the top and not
                    // on the edge (where its round bottom would read a slope, not a floor).
                    Vector3 fwd = want.normalized() * std::max(want.length(), kRadius + 0.15f);
                    if (cb->test_move(up, fwd, kc)) fwd = kc->get_travel();
                    if (fwd.length_squared() > kRadius * kRadius) {
                        Transform3D over = up; over.origin += fwd;
                        if (cb->test_move(over, Vector3(0, -rise - 0.05f, 0), kc)) {
                            real_t drop = -(real_t)kc->get_travel().y, lift = rise - drop;
                            if (lift > 0.02f && lift <= kStep && kc->get_normal().y > std::cos(cb->get_floor_max_angle())) {
                                over.origin.y -= drop;
                                cb->set_global_transform(over);
                                Vector3 v = cb->get_velocity(); v.y = 0; cb->set_velocity(v);
                                onFloor = true;
                                auto* co = Object::cast_to<CollisionObject3D>(kc->get_collider());
                                int64_t other = part_of_shape(co, kc->get_collider_shape_index(), get_part_id(co));
                                if (other && other != ch.root && parts_.count(other)) touching.insert(other);
                            }
                        }
                    }
                }
            }
        }
        ch.onFloor = onFloor || cb->is_on_floor();
        {
            // Humanoid:GetState() and its word in the runtime
            std::string state = ch.seat ? "Seated" : swimming ? "Swimming" : (ch.onFloor ? "Running" : "Freefall");
            if (state != ch.state) { ch.state = state; if (ch.humanoid) pendingWrites_.push_back({ch.humanoid, "StateName", Value::string(state)}); }
        }
        for (int64_t other : touching)
            if (!ch.touching.count(other)) {
                events_.push_back({ch.root, "Touched", {Value::instance(other)}});
                events_.push_back({other, "Touched", {Value::instance(ch.root)}});
            }
        for (int64_t other : ch.touching)
            if (!touching.count(other) && parts_.count(other)) {
                events_.push_back({ch.root, "TouchEnded", {Value::instance(other)}});
                events_.push_back({other, "TouchEnded", {Value::instance(ch.root)}});
            }
        ch.touching.swap(touching);
        ch.moveDir = dir;
        if (ch.autoRotate && model == localChar_ && cameraRelative_ && camera_) {
            // UserGameSettings.RotationType CameraRelative: the body follows the camera's look
            // direction continuously, walking or not, so the pose the server hears turns too.
            Vector3 fwd = camera_forward();
            Vector3 rot = cb->get_rotation();
            rot.x = 0; rot.z = 0;
            rot.y = (float)std::atan2(-fwd.x, -fwd.z);
            cb->set_rotation(rot);
        } else if (ch.autoRotate && (dir.x != 0 || dir.z != 0)) {
            Vector3 rot = cb->get_rotation();
            rot.x = 0; rot.z = 0;
            rot.y = (float)lerpAngle(rot.y, std::atan2(-dir.x, -dir.z), std::min(1.0, 18.0 * dt));
            cb->set_rotation(rot);
        }
        if (defaultAnimations_) animate_character(ch, dt);
    }
}

// ---- animations --------------------------------------------------------------------
// A path in the tree, the way an Animation names its KeyframeSequence.
int64_t PulseBlockzWorld::resolve_path(const std::string& path) const {
    int64_t at = 0;   // game
    size_t start = 0;
    while (start <= path.size()) {
        size_t dot = path.find('.', start);
        std::string part = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (!part.empty()) {
            int64_t next = 0;
            for (auto& [id, e] : entries_) if (e.parent == at && e.name == part) { next = id; break; }
            if (!next) return 0;
            at = next;
        }
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    return at;
}

// The clock of every playing AnimationTrack: it runs at Speed, loops or ends at
// Length, tells the script where it is (TimePosition) and fires the keyframes it
// passes, DidLoop, and Stopped / Ended when it runs out.
void PulseBlockzWorld::step_tracks(double dt) {
    for (auto& [id, t] : tracks_) {
        if (!t.resolved) {
            t.resolved = true;
            auto ait = animationIds_.find(t.animation);
            std::string idstr = ait == animationIds_.end() ? "" : ait->second;
            if (isCloudAsset(idstr)) {
                // A KeyframeSequence from the cloud: fetched, then put in the tree under
                // ReplicatedStorage.RobloxAssets by its number; until then this asks again.
                std::string local = cloud_local(idstr, "Animation");
                std::string num = cloudNumber(idstr);
                t.sequence = local.empty() ? 0 : resolve_path("ReplicatedStorage.RobloxAssets." + num);
                if (!t.sequence) {
                    if (!local.empty()) if (auto cit = cloudAssets_.find(num); cit != cloudAssets_.end()) cloud_into_tree(cit->second);
                    if (auto cit = cloudAssets_.find(num); cit == cloudAssets_.end() || !cit->second.failed) t.resolved = false;
                }
            } else t.sequence = resolve_path(idstr);
        }
        if (!t.playing) continue;
        // the fade Play / Stop / AdjustWeight asked for: the weight walks to its target
        if (t.weight != t.weightTarget) {
            double step = t.fade > 0 ? dt / t.fade : 1.0, gap = t.weightTarget - t.weight;
            t.weight += std::max(-step, std::min(step, gap));
            if (std::fabs(t.weight - t.weightTarget) < 1e-3) t.weight = t.weightTarget;
            if (std::fabs(t.weight - t.sentWeight) > 0.02 || t.weight == t.weightTarget) {
                pendingWrites_.push_back({id, "WeightCurrent", Value::number(t.weight)});
                t.sentWeight = t.weight;
            }
        }
        if (t.stopping && t.weight <= 0) {   // faded out: that is the end of it
            t.playing = false;
            t.stopping = false;
            pendingWrites_.push_back({id, "IsPlaying", Value::boolean(false)});
            events_.push_back({id, "Stopped", {}});
            events_.push_back({id, "Ended", {}});
            continue;
        }
        double before = t.time;
        t.time += dt * t.speed;
        bool ended = false, looped = false;
        if (t.length > 0 && t.time >= t.length) {
            if (t.looped) { t.time = std::fmod(t.time, t.length); looped = true; }
            else { t.time = t.length; ended = true; }
        }
        // every Keyframe with a name of its own between where it was and where it is
        if (t.sequence) {
            for (auto& [kid, ktime] : keyframes_) {
                auto e = entries_.find(kid);
                if (e == entries_.end() || e->second.parent != t.sequence || e->second.name.empty() || e->second.name == "Keyframe") continue;
                bool passed = looped ? (ktime > before || ktime <= t.time) : (ktime > before && ktime <= t.time);
                if (passed && e->second.name != t.lastKeyframe) {
                    events_.push_back({id, "KeyframeReached", {Value::string(e->second.name)}});
                    t.lastKeyframe = e->second.name;
                }
            }
        }
        if (looped) events_.push_back({id, "DidLoop", {}});
        if (std::fabs(t.time - t.sentTime) > 0.03 || ended) {
            pendingWrites_.push_back({id, "TimePosition", Value::number(t.time)});
            t.sentTime = t.time;
        }
        if (ended) {
            t.playing = false;
            pendingWrites_.push_back({id, "IsPlaying", Value::boolean(false)});
            events_.push_back({id, "Stopped", {}});
            events_.push_back({id, "Ended", {}});
        }
    }
}

// What a character's playing track asks of each limb this frame: the Poses of the
// Keyframes either side of the clock, interpolated. A Pose's CFrame turns its limb
// about the joint it hangs from, as Roblox's does (an R6 rig, poses by limb name).
bool PulseBlockzWorld::animation_pose(int64_t model, std::map<std::string, Transform3D>& out) {
    std::vector<const Track*> playing;
    for (auto& [id, t] : tracks_) {
        if (!t.playing || !t.sequence || t.weight <= 0) continue;
        auto track = entries_.find(id);                                     // AnimationTrack -> Animator
        if (track == entries_.end()) continue;
        auto animator = entries_.find(track->second.parent);                // Animator -> Humanoid
        if (animator == entries_.end()) continue;
        auto humanoid = entries_.find(animator->second.parent);             // Humanoid -> the character
        if (humanoid == entries_.end() || humanoid->second.parent != model) continue;
        playing.push_back(&t);
    }
    if (playing.empty()) return false;
    // every Pose under a keyframe, by the limb it names (they nest as in Studio)
    auto collect = [&](int64_t keyframe, std::map<std::string, Transform3D>& into) {
        for (auto& [pid, pose] : poses_) {
            auto e = entries_.find(pid);
            if (e == entries_.end()) continue;
            int64_t up = e->second.parent;
            bool under = false;
            for (int hop = 0; hop < 8 && up; hop++) {
                if (up == keyframe) { under = true; break; }
                auto pe = entries_.find(up);
                if (pe == entries_.end()) break;
                up = pe->second.parent;
            }
            if (under) into[e->second.name] = pose.cframe;
        }
    };
    // Each track's own pose this instant, then the blend: the highest Priority
    // posing a limb takes it, and tracks of that priority mix by weight, the way
    // Roblox layers an Action over the Movement underneath.
    struct Blend { int priority = -1; double weight = 0; Transform3D pose; };
    std::map<std::string, Blend> mixed;

    // KeyframeSequence.AuthoredHipHeight is the hip height of the rig the poses were authored
    // against. A pose is an offset in studs, so on a rig of another size the translations are
    // scaled by the ratio of hip heights, as Roblox scales them.
    double wearerHip = 0;
    for (auto& [cid, ce] : entries_) {
        if (ce.parent != model) continue;
        const ClassDef* hc = findClass(ce.className);
        if (!hc || !hc->isA("Humanoid")) continue;
        if (auto hh = ce.props.find("HipHeight"); hh != ce.props.end()) wearerHip = hh->second.n;
        break;
    }
    auto authoredHip = [&](int64_t sequence) -> double {
        auto e = entries_.find(sequence);
        if (e == entries_.end()) return 0;
        auto it = e->second.props.find("AuthoredHipHeight");
        return it == e->second.props.end() ? 0.0 : it->second.n;
    };

    for (const Track* t : playing) {
        int64_t k0 = 0, k1 = 0;
        double t0 = -1e9, t1 = 1e9;
        for (auto& [kid, ktime] : keyframes_) {
            auto e = entries_.find(kid);
            if (e == entries_.end() || e->second.parent != t->sequence) continue;
            if (ktime <= t->time && ktime > t0) { k0 = kid; t0 = ktime; }
            if (ktime > t->time && ktime < t1) { k1 = kid; t1 = ktime; }
        }
        if (!k0 && !k1) continue;
        if (!k0) { k0 = k1; t0 = t1; }
        if (!k1) { k1 = k0; t1 = t0; }
        double f = t1 > t0 ? (t->time - t0) / (t1 - t0) : 0;
        std::map<std::string, Transform3D> a, b;
        collect(k0, a);
        collect(k1, b);
        // Only the offsets, never the turns: a shoulder rotates by the same angle whatever
        // size the arm is, and scaling the basis would shear the limb.
        double auth = authoredHip(t->sequence);
        double grow = (auth > 0.001 && wearerHip > 0.001) ? wearerHip / auth : 1.0;
        for (auto& [name, from] : a) {
            auto to = b.find(name);
            Transform3D pose = to == b.end() ? from : from.interpolate_with(to->second, (real_t)f);
            if (grow != 1.0) pose.origin *= (real_t)grow;
            Blend& into = mixed[name];
            if (t->priority < into.priority) continue;                      // a louder track already has this limb
            if (t->priority > into.priority) { into = Blend{t->priority, 0, Transform3D()}; }
            double w = std::max(0.0, t->weight);
            into.weight += w;
            into.pose = into.weight > 0 ? into.pose.interpolate_with(pose, (real_t)(w / into.weight)) : pose;
        }
    }
    for (auto& [name, blend] : mixed) {
        double w = std::min(1.0, blend.weight);
        out[name] = w >= 1 ? blend.pose : Transform3D().interpolate_with(blend.pose, (real_t)w);
    }
    return !out.empty();
}

// Which swinging chain a limb belongs to: an R6 arm or leg is one part, an R15 one
// is three (upper, lower, hand or foot), and they turn together.
static std::string limb_group(const std::string& n) {
    if (n == "Left Arm" || n == "LeftUpperArm" || n == "LeftLowerArm" || n == "LeftHand") return "LeftArm";
    if (n == "Right Arm" || n == "RightUpperArm" || n == "RightLowerArm" || n == "RightHand") return "RightArm";
    if (n == "Left Leg" || n == "LeftUpperLeg" || n == "LeftLowerLeg" || n == "LeftFoot") return "LeftLeg";
    if (n == "Right Leg" || n == "RightUpperLeg" || n == "RightLowerLeg" || n == "RightFoot") return "RightLeg";
    return "";
}

// What Roblox's default Animate script does for a rig, procedurally: arms and legs swing from
// the shoulders and hips with the stride, arms go up in the air, a slow sway while standing.
// Only the meshes move; the limbs' Positions in the tree stay at the rest pose.
void PulseBlockzWorld::animate_character(Character& ch, double dt) {
    auto rit = parts_.find(ch.root);
    if (rit == parts_.end() || !rit->second.body) return;
    auto* cb = Object::cast_to<CharacterBody3D>(rit->second.body);
    Vector3 v = cb ? cb->get_velocity() : Vector3();
    double speed = std::sqrt(double(v.x * v.x + v.z * v.z));
    bool walking = speed > 0.5 && ch.onFloor;
    auto approach = [&](double& x, double target, double rate) { x += (target - x) * std::min(1.0, rate * dt); };
    approach(ch.animAmp, walking ? 1.0 : 0.0, 12.0);
    approach(ch.jumpBlend, ch.onFloor ? 0.0 : 1.0, ch.onFloor ? 14.0 : 7.0);   // a short drop barely lifts the arms
    approach(ch.sitBlend, ch.seat ? 1.0 : 0.0, 10.0);   // seated: legs straight out, arms a little forward
    if (walking) ch.animPhase += speed * 0.55 * dt;   // ~1.4 strides/s at WalkSpeed 16
    else ch.animPhase = std::round(ch.animPhase / Math_PI) * Math_PI;   // settle at the neutral pose
    ch.idleT += dt;
    // A loaded animation takes the limbs over while it plays, as it does in Roblox.
    std::map<std::string, Transform3D> keyed;
    bool animated = animation_pose(entries_.count(ch.root) ? entries_[ch.root].parent : 0, keyed);
    // The character's own Motor6Ds (Roblox's RootJoint, Neck, shoulders and hips; an R15's
    // fifteen): a playing track's Pose becomes the joint's Transform, and a Transform a script
    // wrote drives the limb the same way -- Part1 = Part0 * C0 * Transform * C1^-1, down the
    // chain. A limb with no such joint takes a Pose about its own top instead.
    std::unordered_map<int64_t, Joint*> motorOf;   // limb -> the Motor6D whose Part1 it is
    for (auto& [jid, j] : joints_) {
        if (j.className != "Motor6D" || !j.enabled || !j.part0 || !j.part1 || j.part1 == ch.root) continue;
        auto p1 = parts_.find(j.part1), p0 = parts_.find(j.part0);
        if (p1 == parts_.end() || p0 == parts_.end() || p1->second.attachedTo != ch.root) continue;
        if (j.part0 != ch.root && p0->second.attachedTo != ch.root) continue;
        if (!in_workspace(jid)) continue;
        motorOf[j.part1] = &j;
    }
    for (auto& [pid, j] : motorOf) {
        auto eit = entries_.find(pid);
        auto k = eit == entries_.end() || !animated ? keyed.end() : keyed.find(eit->second.name);
        if (k != keyed.end()) { j->transform = k->second; j->animatedByTrack = true; }
        else if (j->animatedByTrack) { j->transform = Transform3D(); j->animatedByTrack = false; }
    }
    std::function<Transform3D(int64_t, int)> frameOf = [&](int64_t pid, int hop) -> Transform3D {   // a limb in the root's space, through its joints
        if (pid == ch.root || hop > 8) return Transform3D();
        auto mit = motorOf.find(pid);
        auto pit = parts_.find(pid);
        if (mit == motorOf.end()) return pit == parts_.end() ? Transform3D() : pit->second.offset;
        const Joint& j = *mit->second;
        return frameOf(j.part0, hop + 1) * j.c0 * j.transform * j.c1.affine_inverse();
    };
    std::function<bool(int64_t, int)> driven = [&](int64_t pid, int hop) -> bool {   // a Transform set somewhere up the chain
        if (pid == ch.root || hop > 8) return false;
        auto mit = motorOf.find(pid);
        if (mit == motorOf.end()) return false;
        return !mit->second->transform.is_equal_approx(Transform3D()) || driven(mit->second->part0, hop + 1);
    };
    const double swing = std::sin(ch.animPhase) * 0.9 * ch.animAmp;
    const double sway = std::sin(ch.idleT * 1.5) * 0.04 * (1.0 - ch.animAmp);
    bool holding = false;   // a Tool in hand: the right arm holds it out in front
    for (int64_t id : parts_on_root(ch.root)) { Part& p = parts_[id]; if (p.role == ROLE_LIMB && p.tool && p.mesh) { holding = true; break; } }
    const double jb = ch.jumpBlend, sb = ch.sitBlend;
    // The four limbs that swing, whichever rig this is: an R6 arm is one part, an
    // R15 arm three, and the whole chain turns about the shoulder / hip together.
    struct Limb { double ax = 0, az = 0; Vector3 pivot; bool have = false; Transform3D pose; };
    std::map<std::string, Limb> groups;
    groups["LeftArm"].ax = (swing + sway) * (1 - jb) + Math_PI * jb;
    groups["LeftArm"].az = -0.15 * jb;
    groups["RightArm"].ax = holding ? Math_PI / 2 : (-swing + sway) * (1 - jb) + Math_PI * jb;
    groups["RightArm"].az = holding ? 0 : 0.15 * jb;
    groups["LeftLeg"].ax = -swing * (1 - jb);
    groups["RightLeg"].ax = swing * (1 - jb);
    if (sb > 0.001)
        for (auto& [g, l] : groups) {
            bool leg = g == "LeftLeg" || g == "RightLeg";
            double seated = leg ? Math_PI / 2 : (holding && g == "RightArm" ? Math_PI / 2 : 0.35);
            l.ax = l.ax * (1 - sb) + seated * sb;
        }
    // where each chain turns: the top of its first part, in the root's space
    for (int64_t id : parts_on_root(ch.root)) { Part& p = parts_[id];
        if (p.role != ROLE_LIMB || p.tool || p.worn || p.attachedTo != ch.root) continue;
        auto eit = entries_.find(id);
        if (eit == entries_.end()) continue;
        const std::string& n = eit->second.name;
        std::string g;
        if (n == "Left Arm" || n == "LeftUpperArm") g = "LeftArm";
        else if (n == "Right Arm" || n == "RightUpperArm") g = "RightArm";
        else if (n == "Left Leg" || n == "LeftUpperLeg") g = "LeftLeg";
        else if (n == "Right Leg" || n == "RightUpperLeg") g = "RightLeg";
        else continue;
        Limb& l = groups[g];
        l.pivot = p.offset.xform(Vector3(0, p.size.y * 0.5f, 0));
        l.have = true;
        Basis b = Basis(Vector3(1, 0, 0), (real_t)l.ax) * Basis(Vector3(0, 0, 1), (real_t)l.az);
        l.pose = Transform3D(b, l.pivot - b.xform(l.pivot));
    }
    Transform3D armPose;   // what the held tool rides
    std::unordered_map<int64_t, Transform3D> posed;   // what an accessory on that limb swings with
    for (int64_t id : parts_on_root(ch.root)) { Part& p = parts_[id];
        if (p.role != ROLE_LIMB || p.tool || p.worn || p.attachedTo != ch.root || !p.mesh) continue;
        auto eit = entries_.find(id);
        if (eit == entries_.end()) continue;
        const std::string& n = eit->second.name;
        std::string g = limb_group(n);
        auto git = groups.find(g);
        Transform3D pose;   // in the root's space, about the chain's joint
        if (driven(id, 0)) pose = frameOf(id, 0) * p.offset.affine_inverse();   // its Motor6D chain says where it is
        else if (animated) {   // a playing animation owns the limbs: one it does not name rests
            auto k = keyed.find(n);
            if (k == keyed.end() || motorOf.count(id)) pose = Transform3D();
            else {   // no joint to turn through: the Pose turns the limb about its own top
                Vector3 pivot(0, p.size.y * 0.5f, 0);
                pose = p.offset * Transform3D(k->second.basis, pivot - k->second.basis.xform(pivot) + k->second.origin) * p.offset.affine_inverse();
            }
        }
        else if (git != groups.end() && git->second.have) pose = git->second.pose;
        // Nothing is posing it, so it goes back to rest rather than keeping the last pose that
        // did: identity here is the limb at its own offset, and writing it every frame is cheap.
        p.posed = pose;
        p.mesh->set_transform(pose * p.offset * p.shapeLocal);
        posed[id] = pose;
        if (g == "RightArm") armPose = pose;
    }
    for (int64_t id : parts_on_root(ch.root)) { Part& p = parts_[id];   // a hat on the head sits still; a bracelet swings with the arm
        if (!p.worn || p.attachedTo != ch.root || !p.mesh) continue;
        auto sw = posed.find(p.wornOn);
        if (sw != posed.end()) { p.posed = sw->second; p.mesh->set_transform(sw->second * p.offset * p.shapeLocal); }
    }
    if (holding)
        for (int64_t id : parts_on_root(ch.root))
            if (Part& p = parts_[id]; p.role == ROLE_LIMB && p.tool && p.mesh) {
                p.posed = armPose;
                p.mesh->set_transform(armPose * p.offset * p.shapeLocal);
            }
}

// A rig that is not a character -- an AnimationController's puppet, an NPC nobody drives -- is
// posed as Roblox's Animator poses one: each playing track's Pose for a limb becomes the
// Transform of the Motor6D whose Part1 the limb is, and the weld offsets do the rest. A limb the
// tracks let go of goes back to rest; a Transform a script wrote itself is left alone.
void PulseBlockzWorld::animate_rigs() {
    if (tracks_.empty()) return;
    std::set<int64_t> rigs;   // every model with a track on it, playing or not
    for (auto& [id, t] : tracks_) {
        auto track = entries_.find(id);
        if (track == entries_.end()) continue;
        auto animator = entries_.find(track->second.parent);
        if (animator == entries_.end()) continue;
        auto host = entries_.find(animator->second.parent);   // Humanoid or AnimationController
        if (host == entries_.end() || !host->second.parent || chars_.count(host->second.parent)) continue;
        rigs.insert(host->second.parent);
    }
    for (int64_t model : rigs) {
        std::map<std::string, Transform3D> keyed;
        animation_pose(model, keyed);
        for (auto& [jid, j] : joints_) {
            if (j.className != "Motor6D" || !j.part1) continue;
            auto limb = entries_.find(j.part1);
            if (limb == entries_.end()) continue;
            bool inRig = false;
            for (int64_t up = limb->second.parent, hop = 0; up && hop < 8; hop++) {
                if (up == model) { inRig = true; break; }
                auto e = entries_.find(up);
                if (e == entries_.end()) break;
                up = e->second.parent;
            }
            if (!inRig) continue;
            auto k = keyed.find(limb->second.name);
            if (k != keyed.end()) {
                if (!k->second.is_equal_approx(j.transform)) { j.transform = k->second; jointsDirty_ = true; }
                j.animatedByTrack = true;
            } else if (j.animatedByTrack) {
                j.transform = Transform3D();
                j.animatedByTrack = false;
                jointsDirty_ = true;
            }
        }
    }
}

// ---- camera ------------------------------------------------------------------------
Vector3 PulseBlockzWorld::camera_forward() const {
    Vector3 f = camera_ ? -camera_->get_global_transform().basis.get_column(2) : Vector3(0, 0, -1);
    f.y = 0;
    return f.length_squared() > 1e-6f ? f.normalized() : Vector3(0, 0, -1);
}

// Roblox's default camera: right-drag turns, the wheel zooms, and Camera.CameraSubject picks
// what it follows -- a Humanoid means its character's root part a head's height up, a part means
// the part, nothing means the local character. CameraType says how: Custom / Follow / Orbital
// orbit it at the user's yaw and pitch; Attach sits behind it, turning with it; Track moves with
// it and keeps its own heading; Watch stays put and turns to face it; Fixed stays put;
// Scriptable is the script's alone.
void PulseBlockzWorld::update_camera() {
    if (!camera_ || cameraType_ == "Scriptable" || cameraType_ == "Fixed") return;
    int64_t focusPart = 0;
    bool humanoid = false;
    const Character* focusChar = nullptr;
    if (cameraSubject_) {
        auto eit = entries_.find(cameraSubject_);
        if (eit != entries_.end()) {
            if (parts_.count(cameraSubject_)) focusPart = cameraSubject_;
            else if (eit->second.className == "Humanoid" && eit->second.parent != kNoParent) {
                auto cit = chars_.find(eit->second.parent);
                if (cit != chars_.end() && cit->second.root) { focusPart = cit->second.root; humanoid = true; focusChar = &cit->second; }
            }
        }
    }
    if (!focusPart) {
        if (!localChar_) return;
        auto cit = chars_.find(localChar_);
        if (cit == chars_.end() || !cit->second.root) return;
        focusPart = cit->second.root;
        humanoid = true;
        focusChar = &cit->second;
    }
    auto pit = parts_.find(focusPart);
    if (pit == parts_.end() || !pit->second.body) return;
    Transform3D frame = part_frame(pit->second);
    Vector3 focus = frame.origin + (humanoid ? Vector3(0, 1.5f, 0) : Vector3());
    if (focusChar) focus += frame.basis.xform(toGd(focusChar->cameraOffset));   // Humanoid.CameraOffset, in the root's own frame
    if (cameraType_ == "Watch") { camera_->look_at(focus, Vector3(0, 1, 0)); camLastFocus_ = focus; return; }
    if (cameraType_ == "Track") {
        if (camLastFocus_ != Vector3()) camera_->set_position(camera_->get_position() + (focus - camLastFocus_));
        camLastFocus_ = focus;
        return;
    }
    double yaw = camYaw_;
    if (cameraType_ == "Attach") {   // behind the subject, the way it faces: its -Z is its LookVector
        Vector3 back = frame.basis.get_column(2);
        back.y = 0;
        if (back.length_squared() > 1e-6f) yaw = std::atan2(back.x, back.z);
    }
    Vector3 off((float)(std::sin(yaw) * std::cos(camPitch_)), (float)std::sin(camPitch_), (float)(std::cos(yaw) * std::cos(camPitch_)));
    camera_->look_at_from_position(camera_pop(focus, focus + off * (float)camDist_, focusPart), focus, Vector3(0, 1, 0));
    camLastFocus_ = focus;
}

// Motion, before anything can swallow it. Everything else reaches scripts through
// _unhandled_input, but a control the pointer is merely over consumes movement, which would
// freeze GetMouseLocation and Mouse.X the moment the pointer entered a panel. Motion is reported
// here and nowhere else, so nothing reports it twice.
void PulseBlockzWorld::_input(const Ref<InputEvent>& event) {
    if (!Object::cast_to<InputEventMouseMotion>(event.ptr())) return;
    // Processed when a control is actually under the pointer: ClickDetector hover is gated on
    // NOT processed, and gui_get_hovered_control() is the answer Roblox puts in
    // gameProcessedEvent.
    Viewport* vp = get_viewport();
    feed_input(event, vp && vp->gui_get_hovered_control() != nullptr);
}

void PulseBlockzWorld::_unhandled_input(const Ref<InputEvent>& event) {
    // `/` opens the chat (Roblox's key), Escape leaves it; the box keeps every other key.
    if (auto* k = Object::cast_to<InputEventKey>(event.ptr()); k && k->is_pressed() && !k->is_echo() && client_ && defaultControls_) {
        if (k->get_keycode() == KEY_SLASH && chatInput_ && chatRoot_ && chatRoot_->is_visible() && !chatTyping_) { chatInput_->grab_focus(); return; }
        if (k->get_keycode() == KEY_ESCAPE && chatTyping_) { chatInput_->release_focus(); return; }
    }
    // Escape: the menu (or the question on it) closes, else it opens. Under the
    // menu, scripts see every input as processed and nothing moves or turns.
    if (auto* k = Object::cast_to<InputEventKey>(event.ptr()); k && k->is_pressed() && !k->is_echo() && client_ && localPlayerId_ && k->get_keycode() == KEY_ESCAPE && !chatTyping_) {
        if (confirmBox_ && confirmBox_->is_visible()) _on_confirm(false);
        else set_menu_open(!menuOpen_);
        return;
    }
    // Motion was already reported from _input, where nothing has had the chance to take
    // it. Sending it again here would fire every listener twice for one movement.
    if (Object::cast_to<InputEventMouseMotion>(event.ptr()) == nullptr) {
        if (menuOpen_) { feed_input(event, true); return; }
    } else if (menuOpen_) {
        return;
    }
    if (auto* mb = Object::cast_to<InputEventMouseButton>(event.ptr()); mb && mb->is_pressed() && typingBox_) release_typing();
    bool onSurface = surface_input(event);
    // Motion has already been reported from _input; everything else is reported here.
    if (!Object::cast_to<InputEventMouseMotion>(event.ptr())) feed_input(event, onSurface);
    if (onSurface) return;                                           // the face took it: no orbit / zoom through it
    if (!camera_ || !defaultCamera_) return;
    if (auto* mm = Object::cast_to<InputEventMouseMotion>(event.ptr())) {
        if (Input::get_singleton()->is_mouse_button_pressed(MOUSE_BUTTON_RIGHT) && !camDragSunk_) {
            Vector2 rel = mm->get_relative();
            camYaw_ -= rel.x * 0.005;
            camPitch_ = std::clamp(camPitch_ + rel.y * 0.005, -1.4, 1.4);
        }
    } else if (auto* mb = Object::cast_to<InputEventMouseButton>(event.ptr())) {
        if (!mb->is_pressed()) return;
        // Player.CameraMinZoomDistance and CameraMaxZoomDistance bound the wheel; the engine's
        // own 128-stud ceiling stays under Roblox's 400 default.
        double minZoom = 0.5, maxZoom = 128.0;
        if (auto it = players_.find(localPlayerId_); it != players_.end()) { minZoom = std::max(0.5, it->second.minZoom); maxZoom = std::min(128.0, it->second.maxZoom); }
        if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_UP) camDist_ = std::max(minZoom, camDist_ * 0.85);
        else if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN) camDist_ = std::min(maxZoom, camDist_ / 0.85);
    }
}

// ---- network -----------------------------------------------------------------------
bool PulseBlockzWorld::listen(int port, int max_clients) {
    ensure_runtime();
    if (!server_ || peer_.is_valid()) return false;
    peer_.instantiate();
    peer_->set_bind_ip(String::utf8(bindAddress_.c_str()));
    if (peer_->create_server(port, max_clients) != OK) { peer_.unref(); return false; }
    listeningPort_ = port;
    peer_->set_transfer_mode(MultiplayerPeer::TRANSFER_MODE_RELIABLE);
    peer_->connect("peer_connected", Callable(this, "_on_peer_connected"));
    peer_->connect("peer_disconnected", Callable(this, "_on_peer_disconnected"));
    return true;
}

bool PulseBlockzWorld::connect_to_server(const String& host, int port) {
    ensure_runtime();
    if (!netClient_ || peer_.is_valid()) return false;
    peer_.instantiate();
    if (peer_->create_client(host, port) != OK) { peer_.unref(); return false; }
    peer_->set_transfer_mode(MultiplayerPeer::TRANSFER_MODE_RELIABLE);
    helloSent_ = false;
    return true;
}

void PulseBlockzWorld::disconnect_from_server() {
    if (peer_.is_valid()) peer_->close();
    peer_.unref();
    if (serverConnected_) emit_signal("server_disconnected");
    serverConnected_ = helloSent_ = false;
}

bool PulseBlockzWorld::is_server_connected() const { return serverConnected_; }
int PulseBlockzWorld::get_client_count() const {
    int n = 0;
    for (auto& [peer, nc] : clients_) n += nc.joined;
    return n;
}

void PulseBlockzWorld::_on_peer_connected(int64_t peer) {
    clients_[(int32_t)peer] = NetClient();   // named by its Hello
}

void PulseBlockzWorld::_on_peer_disconnected(int64_t peer) {
    auto it = clients_.find((int32_t)peer);
    if (it == clients_.end()) return;
    NetClient nc = it->second;
    clients_.erase(it);
    joinQueue_.erase(std::remove_if(joinQueue_.begin(), joinQueue_.end(), [&](auto& j) { return j->peer == peer; }), joinQueue_.end());
    if (join_ && join_->peer == peer) join_->peer = -1;   // its Player exists by now: removed below, never welcomed
    remoteChars_.erase(nc.charId);
    // By id, not name: two clients may share a name, and the one leaving must not take the
    // other's Player with it.
    if (nc.playerId) { int64_t pid = nc.playerId; queue_job([pid](Runtime& rt) { if (Instance* p = rt.dataModel().find(pid)) if (p->isA("Player")) rt.removePlayer(p); }); }
    else if (!nc.name.empty()) remove_player(String::utf8(nc.name.c_str()));
    if (nc.joined) emit_signal("client_left", String::utf8(nc.name.c_str()), nc.playerId, peer);
}

void PulseBlockzWorld::net_send(int32_t peer, const NetPacket& pk, bool reliable) {
    if (peer_.is_null()) return;
    std::string bytes = encodePacket(pk);
    PackedByteArray a;
    a.resize((int64_t)bytes.size());
    std::memcpy(a.ptrw(), bytes.data(), bytes.size());
    peer_->set_transfer_mode(reliable ? MultiplayerPeer::TRANSFER_MODE_RELIABLE : MultiplayerPeer::TRANSFER_MODE_UNRELIABLE_ORDERED);
    peer_->set_target_peer(peer);
    peer_->put_packet(a);
}

// The pose a part came to rest at, from the mirror, for a reliable resend after a
// run of unreliable ones (the last of which may have been lost).
void PulseBlockzWorld::rest_pose(int64_t id, std::vector<HostWrite>* writes, std::vector<Change>* changes) {
    auto pit = parts_.find(id);
    if (pit == parts_.end()) return;   // destroyed: that went reliably
    const Part& p = pit->second;
    const char* names[] = {"Position", "Orientation", "AssemblyLinearVelocity", "AssemblyAngularVelocity"};
    Vec3 vals[] = {p.sentPos, p.sentOrient, p.sentVel, p.sentAngVel};   // what was last reported, not the echo of it
    for (int i = 0; i < 4; i++) {
        if (writes) writes->push_back({id, names[i], Value::vector3(vals[i])});
        if (changes) { Change c; c.kind = Change::Property; c.id = id; c.name = names[i]; c.value = Value::vector3(vals[i]); c.fromHost = true; changes->push_back(std::move(c)); }
    }
}

void PulseBlockzWorld::net_poll() {
    if (peer_.is_null()) return;
    peer_->poll();
    if (netClient_) {
        auto st = peer_->get_connection_status();
        if (st == MultiplayerPeer::CONNECTION_CONNECTED && !helloSent_) {
            NetPacket pk; pk.type = NetPacket::Hello;
            pk.name = playerName_; pk.userId = userId_;
            net_send(1, pk);
            helloSent_ = true;
        } else if (st == MultiplayerPeer::CONNECTION_DISCONNECTED) {
            bool was = serverConnected_;
            peer_.unref();
            serverConnected_ = helloSent_ = false;
            if (was) emit_signal("server_disconnected");
            return;
        }
    }
    while (peer_->get_available_packet_count() > 0) {
        int32_t from = peer_->get_packet_peer();
        PackedByteArray a = peer_->get_packet();
        std::string bytes((const char*)a.ptr(), (size_t)a.size());
        NetPacket pk;
        if (!decodePacket(bytes, pk)) {
            if (server_) peer_->disconnect_peer(from);   // not speaking the protocol
            continue;
        }
        net_receive(from, pk);
    }
}

// A packet is input. The server takes from a client exactly what Roblox lets
// a client own: its character's pose and touches, and remote calls. Anything
// else in the packet is dropped.
void PulseBlockzWorld::net_receive(int32_t from, NetPacket& pk) {
    if (server_) {
        auto it = clients_.find(from);
        if (it == clients_.end()) return;
        NetClient& nc = it->second;
        if (pk.type == NetPacket::Hello) {
            if (!nc.name.empty() || pk.protocol != kWireProtocol || pk.name.empty() || pk.name.size() > 32
                || pk.name.find_first_of("\t\n\r/\\") != std::string::npos) { peer_->disconnect_peer(from); return; }
            nc.name = pk.name; nc.userId = pk.userId;
            auto j = std::make_shared<Join>();
            j->name = pk.name; j->userId = pk.userId; j->peer = from;
            joinQueue_.push_back(j);
        } else if (pk.type == NetPacket::ClientFrame && nc.joined) {
            auto cit = chars_.find(nc.charId);
            int64_t root = cit == chars_.end() ? 0 : cit->second.root;
            int64_t hum = cit == chars_.end() ? 0 : cit->second.humanoid;
            for (HostWrite& w : pk.writes) {
                // What the owner of a character gets to say about it: where it is, how it is
                // moving, and which state its Humanoid is in, as on Roblox.
                bool ok = (w.id == root && root && w.value.type == Value::Vector3
                           && (w.prop == "Position" || w.prop == "Orientation" || w.prop == "AssemblyLinearVelocity"))
                       || (w.id == hum && hum && ((w.prop == "MoveDirection" && w.value.type == Value::Vector3) || (w.prop == "Jump" && w.value.type == Value::Bool)
                                                  || (w.prop == "StateName" && w.value.type == Value::String && w.value.s.size() < 32
                                                      && findEnum("HumanoidStateType") && findEnum("HumanoidStateType")->find(w.value.s))));   // a HumanoidStateType, nothing else
                if (!ok) continue;
                if (w.value.type == Value::Vector3 && !(std::isfinite(w.value.v.x) && std::isfinite(w.value.v.y) && std::isfinite(w.value.v.z))) continue;
                netWrites_.push_back(std::move(w));
            }
            for (HostEvent& e : pk.events) {
                // Its Humanoid's MoveToFinished: the owner walks it, so the owner knows when it got there.
                if (e.event == "MoveToFinished" && hum && e.id == hum && e.args.size() == 1 && e.args[0].type == Value::Bool) { events_.push_back(std::move(e)); continue; }
                if ((e.event != "Touched" && e.event != "TouchEnded") || e.args.size() != 1 || e.args[0].type != Value::Ref || !root) continue;
                int64_t other = e.args[0].ref;
                bool ok = (e.id == root && parts_.count(other)) || (other == root && parts_.count(e.id));
                if (ok) events_.push_back(std::move(e));
            }
            for (RemoteMsg& m : pk.remotes) { m.player = nc.playerId; toServerRemotes_.push_back(std::move(m)); }
            if (editMode_) for (const NetEdit& e : pk.edits) apply_edit(e);   // Team Create: a joined Studio's edits; a game's server takes none
        } else if (pk.type == NetPacket::Proof && nc.joined) {
            // An empty proof is a guest saying so; a wrong one claims a wallet that is not
            // theirs and is shown the door.
            if (pk.address.empty() && pk.signature.empty()) return;
            // Signed for another server: a relayed sign-in, refused like a forged one.
            if (!accepts_server_name(pk.server)) {
                emit_signal("script_warn", String("SignIn"), String::utf8(("a sign-in signed for \"" + pk.server + "\" was refused: this server does not answer to that name").c_str()));
                peer_->disconnect_peer(from);
                return;
            }
            const std::string proven = verify_sign_in(pk.server, nc.nonce, pk.address, pk.signature);
            if (proven.empty()) { peer_->disconnect_peer(from); return; }
            nc.nonce.clear();   // one proof per challenge
            sign_in_player(nc.playerId, proven);
        }
        return;
    }
    // client
    if (pk.type == NetPacket::Welcome && !localPlayerId_) {
        localPlayerId_ = pk.playerId;
        serverConnected_ = true;
        placeUri_ = pk.place;                // what to fetch; nothing is run off the server's word
        // The tree comes with the Welcome and carries no script Source: a server sends no code
        // (rbx_net.cpp). The host fetches the published place, checks it file by file and loads
        // it over these instances, which fills them in.
        toClientRep_ = std::move(pk.changes);
        // Becoming the local player is what copies StarterPlayerScripts into PlayerScripts, so
        // it waits for that filling-in; ready_for_place() is the host saying it is done. The
        // join is on record before the host hears the place's name: a host whose fetch finds
        // every file in its cache answers ready_for_place() inside the signal.
        auto j = std::make_shared<Join>();
        j->name = playerName_; j->userId = userId_; j->playerId = pk.playerId; j->peer = 1;
        j->done = j->sent = true;
        pendingJoin_ = j;
        awaitingTree_ = true;
        join_ = j;
        emit_signal("server_place_named", String::utf8(placeUri_.c_str()));
        // Nobody is going to load a checked place over this: become the player now.
        if (!holdForPlace_) ready_for_place();
        emit_signal("server_connected", localPlayerId_);
        if (!pk.nonce.empty()) call_deferred("emit_signal", "sign_in_requested", String::utf8(pk.nonce.c_str()));
        emit_signal("player_joined", String::utf8(playerName_.c_str()), localPlayerId_);
    } else if (pk.type == NetPacket::ServerFrame && localPlayerId_) {
        for (Change& c : pk.changes) toClientRep_.push_back(std::move(c));
        for (RemoteMsg& m : pk.remotes) toClientRemotes_.push_back(std::move(m));
    }
}

// Two packets per client per frame at most: a reliable one with the structural changes, the
// remotes and the poses that start or end a motion, and an unreliable-ordered one with the poses
// of parts already in motion. A part that moved last frame and not this one gets its resting
// pose resent reliably, since the last unreliable one may not have arrived.
void PulseBlockzWorld::net_flush_clients() {
    for (auto& [peer, nc] : clients_) {
        if (!nc.joined) continue;
        if (nc.rep.empty() && nc.remotes.empty() && nc.moving.empty()) continue;
        NetPacket rel; rel.type = NetPacket::ServerFrame;
        NetPacket fast; fast.type = NetPacket::ServerFrame;
        std::set<int64_t> movingNow;
        for (auto& c : nc.rep) {
            bool pose = c.kind == Change::Property && c.fromHost && poseProp(c.name) && parts_.count(c.id);
            if (pose) { movingNow.insert(c.id); (nc.moving.count(c.id) ? fast : rel).changes.push_back(std::move(c)); }
            else rel.changes.push_back(std::move(c));
        }
        for (int64_t id : nc.moving) if (!movingNow.count(id)) rest_pose(id, nullptr, &rel.changes);
        nc.moving = std::move(movingNow);
        rel.remotes = std::move(nc.remotes);
        nc.rep.clear(); nc.remotes.clear();
        if (!rel.changes.empty() || !rel.remotes.empty()) net_send(peer, rel, true);
        // Under the MTU, a packet at a time: an unreliable packet over the MTU goes as
        // fragments, and one lost fragment loses the lot.
        static const size_t kPosesPerPacket = 16;
        for (size_t at = 0; at < fast.changes.size(); at += kPosesPerPacket) {
            NetPacket part; part.type = NetPacket::ServerFrame;
            for (size_t k = at; k < fast.changes.size() && k < at + kPosesPerPacket; k++) part.changes.push_back(std::move(fast.changes[k]));
            net_send(peer, part, false);
        }
    }
}
