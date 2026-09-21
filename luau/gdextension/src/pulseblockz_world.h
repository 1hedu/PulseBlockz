// PulseBlockzWorld: the Godot node hosting one Roblox-shaped world. Owns the scripting
// Runtimes (luau/src/rbx_*) on worker threads, one frame in flight each, and mirrors their
// instance tree into the scene. Modes: Play Solo (both runtimes in process), Server, Client.
#pragma once
#include <godot_cpp/classes/http_request.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/audio_listener3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/immediate_mesh.hpp>
#include <godot_cpp/classes/light3d.hpp>
#include <godot_cpp/classes/gpu_particles3d.hpp>
#include <godot_cpp/classes/joint3d.hpp>
#include <godot_cpp/classes/physics_material.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_effect.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <map>
#include <unordered_set>
#include <thread>
#include <atomic>
#include <memory>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/gradient_texture1_d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/text_edit.hpp>
#include <godot_cpp/classes/v_scroll_bar.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/check_button.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/atlas_texture.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/h_scroll_bar.hpp>
#include <godot_cpp/classes/e_net_multiplayer_peer.hpp>
#include <godot_cpp/classes/e_net_packet_peer.hpp>
#include <godot_cpp/classes/e_net_connection.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <deque>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>
#include "rbx_host.h"
#include "rbx_wire.h"

namespace godot {

class PulseBlockzWorld : public Node3D {
    GDCLASS(PulseBlockzWorld, Node3D)

public:
    enum Mode { MODE_PLAY_SOLO = 0, MODE_SERVER = 1, MODE_CLIENT = 2 };

    PulseBlockzWorld();
    ~PulseBlockzWorld() override;

    // Configuration; read when the runtimes are created (first use / _ready).
    void set_mode(int v);                 int get_mode() const;
    void set_player_name(const String& v); String get_player_name() const;
    void set_user_id(int64_t v);          int64_t get_user_id() const;
    void set_auto_join(bool v);           bool get_auto_join() const;
    void set_default_controls(bool v);    bool get_default_controls() const;
    void set_default_camera(bool v);      bool get_default_camera() const;
    void set_default_lighting(bool v);    bool get_default_lighting() const;
    void set_default_animations(bool v);  bool get_default_animations() const;
    void set_threaded(bool v);            bool get_threaded() const;
    void set_auto_step(bool v);           bool get_auto_step() const;
    void push_budget();                   // the running runtimes take the budget as it stands now
    void set_max_millis(double v);        double get_max_millis() const;
    void set_max_frame_millis(double v);  double get_max_frame_millis() const;
    void set_max_steps(int64_t v);        int64_t get_max_steps() const;
    void set_max_memory_mb(int v);        int get_max_memory_mb() const;
    void set_listen_port(int v);          int get_listen_port() const;
    // How many clients the socket will take; what a server's `--max` flag sets.
    void set_max_clients(int v);          int get_max_clients() const;
    void set_bind_address(const String& v); String get_bind_address() const;
    void set_data_store_path(const String& v); String get_data_store_path() const;
    void set_asset_root(const String& v);      String get_asset_root() const;
    void set_server_address(const String& v); String get_server_address() const;
    void set_server_port(int v);          int get_server_port() const;
    void set_physics_layer(int v);        int get_physics_layer() const;

    /// Server / Play Solo: accept Client worlds on `port` (listen_port does this in _ready).
    bool listen(int port, int max_clients);
    /// Client: connect and join as player_name / user_id (auto_join does this in _ready).
    bool connect_to_server(const String& host, int port);
    void disconnect_from_server();
    bool is_server_connected() const;
    int get_client_count() const;

    /// Create/replace the instance a source file stands for (Rojo path), on the
    /// server. Runs at the next frame; a bad path arrives as script_error("", err).
    void load_file(const String& rel_path, const String& source);
    void unload_file(const String& rel_path);
    /// A Roblox place file (.rbxlx or .rbxl) merged into the tree on the next frame;
    /// what was and was not imported comes back as script_warn from "import".
    void import_place(const PackedByteArray& bytes);
    /// Run a chunk on the server with `script` = nil (console). Errors: script_error(name, err).
    void run_chunk(const String& name, const String& source);
    /// The same on the client runtime, where the camera and the PlayerGui live.
    void run_client_chunk(const String& name, const String& source);
    // Sign-in: the wallet's answer to sign_in_requested (the address it holds, and the nonce signed).
    void answer_sign_in(const String& address, const String& signature, const String& server = String());
    // The exact text a wallet signs to sign in here: the nonce and the server it is for.
    String sign_in_message(const String& nonce) const;
    // Host names this server answers to besides its own interface addresses: the DNS name
    // players dial. A proof signed for any other name is somebody else's sign-in, relayed.
    void set_public_names(const PackedStringArray& v); PackedStringArray get_public_names() const;
    // The published place this server runs, as a pblockz:// uri. Clients are told it on
    // Welcome and fetch it from the chain, checking each file against its hash; the server
    // never sends code, so a server naming no place leaves its clients nothing to run.
    void set_place_uri(const String& v); String get_place_uri() const;
    // On a client: do not become the local player until the place's code is in. The tree
    // arrives with no script source; loading the published place fills the Source of the
    // instances already there, and only then is there anything to copy into PlayerScripts.
    void set_hold_for_place(bool v); bool get_hold_for_place() const;
    void ready_for_place();
    bool holdForPlace_ = false;
    bool awaitingTree_ = false;          // Welcome landed; the tree has not
    // On a client: the place the server named, once Welcome has landed. Empty before that.
    String server_place() const;
    std::string placeUri_;
    bool accepts_server_name(const std::string& server) const;
    std::vector<std::string> publicNames_;
    int listeningPort_ = 0;
    void sign_in_player(int64_t playerId, const std::string& address);
    std::string localNonce_;                 // Play Solo: the local player's sign-in challenge
    // The Studio's plugins: scripts run in the edit world with a `plugin` global.
    void plugin_add(const String& name, const String& source);        // a Script of that name under PluginDebugService
    void plugin_add_file(const String& file, const String& source);   // a plugin that is a file (.rbxmx / .model.json of scripts), placed under it
    void plugin_unload_all();
    void plugin_click(int64_t button);                                 // a toolbar button on the Plugins tab: its Click
    void plugin_selection(const PackedInt64Array& ids);                // the Studio's selection, for Selection:Get()
    void plugin_setting(const String& plugin, const String& key, const String& json);   // a kept setting, before the plugin runs
    void plugin_input(const Ref<InputEvent>& event, Vector3 ray_origin, Vector3 ray_direction);   // the pointer over the view while a plugin is active
    // Team Create: a Client world in edit mode is a Studio joined to a hosting one. Its
    // edits go to the host as NetEdits; the host applies them and the change replicates back.
    bool collaborating() const { return netClient_ && editMode_; }
    bool is_hosting() const;
    void forward_edit(pulseblockz::rbx::NetEdit e);
    void apply_edit(const pulseblockz::rbx::NetEdit& e);
    std::vector<pulseblockz::rbx::NetEdit> pendingEdits_;
    /// add_player fires PlayerAdded and spawns a character. In Play Solo the first becomes
    /// the local player; later ones are headless.
    void add_player(const String& name, int64_t user_id);
    void remove_player(const String& name);

    /// Run every queued job now and apply the results before returning.
    void flush();
    bool is_idle() const;
    /// Stats of the last finished frame (see frame_finished).
    Dictionary get_stats() const;
    /// The scene node mirroring instance `id` (parts only), or null.
    Node3D* get_part_node(int64_t id) const;
    /// The MeshInstance3D that is the part's own shape (a limb's rides its root's body), or null.
    Node3D* get_part_mesh(int64_t id) const;
    int64_t get_part_id(Node* node) const;
    /// The default camera (PlaySolo with default_camera), or null.
    Camera3D* get_camera() const;
    /// The local player's Player id on the server / character model id, or 0.
    int64_t get_local_player_id() const;
    int64_t get_local_character_id() const;
    /// The CharacterBody3D driving the local player's character, or null.
    Node3D* get_local_character_node() const;

    /// Edit mode: no script starts (the tree is still built, Sources and all) and every
    /// part is a static body, so an unanchored one stays where it was put. Set it before
    /// the world runs, as with `mode`.
    void set_edit_mode(bool v);
    bool get_edit_mode() const;
    /// Draw StarterGui's ScreenGuis as they would look in game. A ScreenGui is otherwise
    /// only built under a PlayerGui, which an edit world has none of. Laid out in
    /// `gui_preview_rect`, and never takes a click.
    void set_gui_preview(bool v);
    bool get_gui_preview() const;
    void set_gui_preview_rect(const godot::Rect2& r);
    godot::Rect2 get_gui_preview_rect() const;
    /// The GuiObject drawn at a point on screen, innermost first, or 0.
    int64_t gui_at(const godot::Vector2& point) const;
    /// {properties, events, methods} a class has, each a sorted PackedStringArray, read
    /// from the class and method tables so the list cannot drift from what the runtime takes.
    godot::Dictionary get_class_members(const godot::String& class_name) const;
    /// Where a GuiObject is drawn on screen, or an empty rect.
    godot::Rect2 gui_rect(int64_t id) const;

    /// ---- The mirrored tree, for an Explorer and a Properties panel ----------
    ///
    /// Create / Destroy / Parent are always mirrored; `track_properties` adds Property
    /// changes. Reads are lock-free from the main thread; writes go back as a job on the
    /// runtime's own thread. Server instances have positive ids, a client's own negative.
    void set_track_properties(bool v);
    bool get_track_properties() const;
    /// The instance ids of `id`'s children, in no particular order; 0 is the DataModel,
    /// whose children are the services. `client` picks which of a Play Solo world's two
    /// trees that root means. (Node::get_children() is unrelated: those are scene children.)
    PackedInt64Array get_child_ids(int64_t id, bool client = false) const;
    /// { id, name, class_name, parent, path }, empty if there is no such instance.
    Dictionary get_instance(int64_t id) const;
    /// One dictionary per property the class declares, in declaration order:
    /// { name, type, value, read_only, is_default, enum_type, enum_items }. `value` is what
    /// the mirror last saw, or the class default. Hidden ones (a Script's Source) need `hidden`.
    Array get_properties(int64_t id, bool hidden = false) const;
    /// Assign as a script would: ReadOnly is refused and the type checked. Lands on the
    /// runtime's next frame; false means nothing was queued.
    bool set_property(int64_t id, const String& name, const Variant& value);
    /// Attributes (name -> value) and CollectionService tags. Both ride the change log as
    /// properties -- an attribute prefixed `@`, a tag `#` -- so the mirror already holds them.
    Dictionary get_attributes(int64_t id) const;
    PackedStringArray get_tags(int64_t id) const;
    /// Set one as a script would; a nil value removes an attribute. Queued for the next frame.
    bool set_attribute(int64_t id, const String& name, const Variant& value);
    bool set_tag(int64_t id, const String& tag, bool on);

    /// Every class `create_instance` will make, sorted, from the runtime's own class table.
    PackedStringArray get_creatable_classes() const;
    /// A *.model.json subtree as the XML Roblox reads (*.rbxmx), `name` being the root
    /// instance's; "" if the JSON is not a model. The file's encoding is not the tree's:
    /// an enum is a numeric token, a BasePart's colour is packed under another name, and
    /// Position and Orientation are one CFrame.
    String to_rbxmx(const String& model_json, const String& name) const;
    /// A DataModel node (its children the services, as Model.node writes them
    /// against root 0) as the .rbxlx Roblox Studio opens; "" and a warning if not.
    String to_rbxlx(const String& place_json) const;
    /// Bumped by every create, destroy, reparent and rename: the shape of the tree changed.
    int64_t get_tree_version() const;
    /// As Instance.new(class_name) then a Parent write. Queued for the runtime's next
    /// frame; false if no class in this world goes by that name.
    bool create_instance(const String& class_name, int64_t parent);
    /// Destroy it and its descendants, as :Destroy() would. Queued the same way.
    bool destroy_instance(int64_t id);
    /// As writing `Parent` (the runtime refuses a cycle or a destroyed parent); 0 is the
    /// DataModel. Parent is not a declared property, so `set_property` cannot reach it.
    bool set_parent(int64_t id, int64_t parent);
    /// Add a *.model.json subtree under a container path ("Workspace/Map", "" for the
    /// DataModel), Refs and all. Nothing is replaced, so two children may share a name.
    /// Queued for the runtime's next frame; a bad model comes back as a script error.
    void add_model(const String& parent_path, const String& name, const String& model_json);

    void _ready() override;
    // Two phases, in this order, and only while the workers are idle: take their FrameOuts
    // first -- tree changes into the scene, replication and remotes to the other side,
    // script_print signals, stats -- then build the next FrameIns (the physics snapshot of
    // unanchored parts and characters, Touched/TouchEnded from contact monitoring, queued
    // jobs) and submit them carrying the render time elapsed since the previous submit.
    void _process(double delta) override;
    void _physics_process(double delta) override;
    void _input(const Ref<InputEvent>& event) override;
    void _notification(int p_what);
    void _unhandled_input(const Ref<InputEvent>& event) override;
    void _on_body_shape_entered(const godot::RID& rid, Node* body, int64_t bodyShape, int64_t localShape, int64_t id);
    void _on_body_shape_exited(const godot::RID& rid, Node* body, int64_t bodyShape, int64_t localShape, int64_t id);
    void _on_peer_connected(int64_t peer);
    void _on_peer_disconnected(int64_t peer);
    void _on_gui_input(const Ref<InputEvent>& event, int64_t id);
    void _on_edit_focus(bool in, int64_t id);
    void _on_edit_changed(const String& text, int64_t id);
    void _on_multi_changed(int64_t id);          // a MultiLine TextBox: TextEdit says nothing about the text
    void _on_edit_submitted(const String& text, int64_t id);
    void _on_edit_input(const Ref<InputEvent>& event, int64_t id);
    void release_typing();                                      // a click elsewhere takes the focus from a TextBox
    void _on_scroll_bar(double value, int64_t id);              // a ScrollingFrame's bar dragged
    void _on_hotbar_pressed(int slot);
    /// Send a chat message as the local player (what Enter in the chat box does).
    void chat(const String& text);
    void _on_gui_mouse(bool entered, int64_t id);

protected:
    static void _bind_methods();

private:
    // The mirrored tree. `props` is filled only while track_properties is on.
    struct Entry {
        std::string className, name;
        int64_t parent = pulseblockz::rbx::kNoParent;
        int64_t made = 0;    // creation order: an Explorer lists a tree as it was built
        std::map<std::string, pulseblockz::rbx::Value> props;
    };
    // Accoutrement.AttachmentPos and the three axes: the pre-Attachment way a hat hangs,
    // an offset and a basis measured from the head. Roblox still honours it.
    struct WornMount {
        Vector3 pos, forward{0, 0, -1}, right{1, 0, 0}, up{0, 1, 0};
        bool stated = false;   // whether the place said any of it, rather than the defaults
    };
    std::unordered_map<int64_t, WornMount> mounts_;

    // ForceField: one translucent sphere per field on the character's root, R6-sized.
    std::unordered_map<int64_t, MeshInstance3D*> shields_;
    bool shieldsDirty_ = false;
    void sync_shields();

    enum Role { ROLE_FREE, ROLE_CHAR_ROOT, ROLE_LIMB };
    struct Part {
        pulseblockz::rbx::Vec3 pos, orient, size{4, 1.2f, 2}, vel;
        pulseblockz::rbx::Col3 color{163 / 255.f, 162 / 255.f, 165 / 255.f};
        float transparency = 0, reflectance = 0;   // transparency is the drawn one: Transparency and LocalTransparencyModifier folded
        float baseTransparency = 0, localTransparency = 0;
        float paramB[6] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};   // Right Left Top Bottom Back Front: a legacy Motor face's speed ...
        uint8_t surfaceInput[6] = {0, 0, 0, 0, 0, 0};              // ... and whether it runs (1 = Constant)
        // What each face wears: 0 smooth, 1 studs, 2 inlet, in surfaceInput's order. Roblox's
        // other SurfaceTypes (Glue, Weld, Hinge, Motor) render smooth. Default Smooth here and
        // in rbx_instance.cpp; Roblox's own new Part is Studs on top and Inlet below.
        uint8_t surface[6] = {0, 0, 0, 0, 0, 0};
        MultiMeshInstance3D* studs = nullptr;                      // one instance per stud, on every studded face
        MultiMeshInstance3D* inlets = nullptr;
        // The stud and inlet transforms, kept here too: a MultiMesh's live in the
        // RenderingServer's buffer, which the headless dummy renderer reads back as identity.
        std::vector<Transform3D> studAt, inletAt;
        std::string studKey;                                       // what those were built for, so they are not rebuilt every frame
        bool anchored = false, canCollide = true, castShadow = true;
        // BasePart.CanTouch: Roblox gates Touched on this separately from collision.
        bool canTouch = true;
        // Humanoid.HipHeight, mirrored onto the root part: update_shape rebuilds the
        // capsule and has no Character to ask.
        float hipHeight = 0;
        // MeshPart.DoubleSided: draw the back faces too, for a mesh whose winding is inside-out.
        bool doubleSided = false;
        float throttle = 0, steer = 0, maxSpeed = 25, driveTorque = 100, turnSpeed = 1;   // a VehicleSeat's drive
        // VehicleSeat.HeadsUpDisplay: the speed readout while driving; Roblox defaults it on.
        bool headsUp = true;
        std::string material = "Plastic", shape = "Block", className;
        std::string materialVariant;      // BasePart.MaterialVariant: which one, by name
        pulseblockz::rbx::Value custom;     // CustomPhysicalProperties (nil: the material's own)
        bool massless = false;
        int64_t id = 0;
        // MeshPart: the file fitted to Size (MeshSize is the file's own bounds); or a
        // SpecialMesh / BlockMesh / CylinderMesh child drawn instead of the Shape
        std::string meshId, textureId, collisionFidelity = "Default";
        std::string meshData;           // a PartOperation's baked geometry, base64 (or a MeshPart GeometryService made)
        Ref<ArrayMesh> opMesh;          // decoded from it, once
        bool opFailed = false;          // the decode failed: not retried
        bool opColors = false;          // it carries face colours
        bool usePartColor = false;      // PartOperation.UsePartColor: one colour all over instead of the faces'
        int sentTriangles = -1;         // TriangleCount, as last written back
        Vector3 meshSize, sentMeshSize{-1, -1, -1};
        Ref<Mesh> collisionMesh;        // an EditableMesh's shape as it was when MeshContent was last set: what it collides as
        std::string renderFidelity = "Automatic";
        float smoothingAngle = 0;       // PartOperation.SmoothingAngle, degrees
        Ref<Mesh> lods[3];              // highest, medium, lowest (RenderFidelity); 1 and 2 made when first needed
        int lodBand = -1;               // which is showing
        Ref<ArrayMesh> lodDecoded;      // what the coarser bands are made from, when one is first needed
        Vector3 lodScale{1, 1, 1};
        bool lodSmoothing = false;
        Ref<ArrayMesh> opDrawn;         // a union as drawn: where a Decal on it is projected from
        std::vector<CollisionShape3D*> extraCols;   // a decomposition's hulls past the first
        int64_t dataMesh = 0;
        // scene side
        Node3D* body = nullptr;              // StaticBody3D, RigidBody3D or CharacterBody3D (null for limbs)
        MeshInstance3D* mesh = nullptr;
        CollisionShape3D* col = nullptr;
        bool rigid = false, visible = false;
        Role role = ROLE_FREE;
        int64_t attachedTo = 0;              // limb: the HumanoidRootPart it rides on
        bool tool = false;                   // limb: a held Tool's part, riding the right arm
        bool worn = false;                   // limb: an Accessory's part, riding the limb its Attachment names
        int64_t wornOn = 0;                  // which limb that is (it swings with it)
        Transform3D offset, shapeLocal;      // limb: where inside the root body; mesh offset (cylinders lie along X)
        // The pose the animator last put on this limb, identity at rest. Anything that
        // rebuilds the mesh writes the transform from scratch and would drop back to rest.
        Transform3D posed;
        Transform3D colLocal;                // the collision shape's offset in part space
        bool welded = false;                 // a limb by a Weld / Motor6D / WeldConstraint: rides its assembly's root
        Ref<ShaderMaterial> skin;            // limb wearing the default gradient skin (its own: the ramp is per limb)
        bool offsetDirty = false;
        pulseblockz::rbx::Vec3 sentPos, sentOrient, sentVel;   // last snapshot handed to the runtime
        // The two before that: an echo of this host's own report can come back a frame or
        // two late, which a frame-stamped guard misses.
        pulseblockz::rbx::Vec3 sentVelWas, sentVelWasWas;
        pulseblockz::rbx::Vec3 angVel, sentAngVel;             // AssemblyAngularVelocity, radians a second
    };
    // A SpecialMesh / BlockMesh / CylinderMesh under a part: what it draws instead of its Shape.
    struct DataMesh {
        std::string type = "Head", meshId, textureId;
        pulseblockz::rbx::Vec3 scale{1, 1, 1}, offset;
        // SpecialMesh.VertexColor: a tint multiplied into the part's colour; white is no tint.
        pulseblockz::rbx::Vec3 vertexColor{1, 1, 1};
        int64_t part = 0;
    };
    // A PointLight / SpotLight / SurfaceLight: a Light3D under its part's mesh.
    struct Light {
        std::string className, face = "Front";
        float brightness = 1, range = 8, angle = 90;
        pulseblockz::rbx::Col3 color{1, 1, 1};
        bool enabled = true, shadows = false;
        int64_t part = 0;                    // the part whose mesh the node hangs under
        Transform3D offset;                  // in an Attachment: where it is on that part
        Light3D* node = nullptr;
    };
    // A Decal / Texture: a quad a hair off one face of its part, under the part's
    // mesh like a light; a Texture repeats its image every StudsPerTile studs.
    struct Decal {
        std::string face = "Front", texture;
        std::string normalMap, metalnessMap, roughnessMap;   // the Content keys: PBR once any is set
        float transparency = 0, studsU = 2, studsV = 2, offsetU = 0, offsetV = 0;
        float localT = 0;                                       // LocalTransparencyModifier, folded in with Transparency
        pulseblockz::rbx::Vec2 uvOffset{0, 0}, uvScale{1, 1};   // a Decal's uv * UVScale + UVOffset (a Texture tiles on its own)
        pulseblockz::rbx::Col3 color{1, 1, 1};
        bool tiled = false;
        int64_t part = 0;
        MeshInstance3D* node = nullptr;
    };
    std::unordered_map<int64_t, Decal> decals_;
    bool decalsDirty_ = false;
    void sync_decals();
    void style_decal(Decal& d);
    void detach_decals(int64_t partId);
    // A ParticleEmitter / Fire / Smoke / Sparkles: a GPUParticles3D under its part's mesh,
    // its properties kept as runtime Values; a second one-shot node serves Emit(n).
    struct Emitter {
        std::string className;
        std::map<std::string, pulseblockz::rbx::Value> props;
        int64_t part = 0;
        Transform3D offset;                  // an Attachment's frame in the part, identity straight in a part
        int pendingBurst = 0;                // Emit(n) heard before the node exists (the frame it was parented)
        GPUParticles3D* node = nullptr;
        GPUParticles3D* burst = nullptr;
    };
    std::unordered_map<int64_t, Emitter> emitters_;
    // An Explosion: once it sits in the Workspace it flings the loose bodies within
    // BlastRadius by BlastPressure and draws a one-shot burst there; the runtime removes it.
    struct Explosion {
        pulseblockz::rbx::Vec3 pos;
        float radius = 4, pressure = 500000;
        bool visible = true, fired = false;
        GPUParticles3D* node = nullptr;
    };
    std::unordered_map<int64_t, Explosion> explosions_;
    // A Trail: the ribbon between its two Attachments over the last Lifetime seconds, both
    // points sampled per frame past MinLength, capped at MaxLength, time 0 at the attachments.
    struct Trail {
        int64_t att0 = 0, att1 = 0;
        bool enabled = true;
        double lifetime = 2, minLength = 0.1, maxLength = 0, brightness = 1, clock = 0, localT = 0;
        pulseblockz::rbx::Value colorSeq = pulseblockz::rbx::Value::colorSequence(pulseblockz::rbx::Col3{1, 1, 1});
        pulseblockz::rbx::Value transSeq = pulseblockz::rbx::Value::numberSequence(0.5f, 0.5f);
        struct Sample { godot::Vector3 a, b; double t, s; };   // s: how far the attachments had travelled by then
        std::deque<Sample> pts;
        godot::Vector3 lastMid;
        double travelled = 0;
        std::string texture, textureLoaded;
        double textureLength = 1;
        int textureMode = 0;                   // Stretch / Wrap / Static
        bool faceCamera = false;
        pulseblockz::rbx::Value widthScale = pulseblockz::rbx::Value::numberSequence(1, 1);
        godot::MeshInstance3D* node = nullptr;
        godot::Ref<godot::ImmediateMesh> mesh;
        godot::Ref<godot::StandardMaterial3D> mat;
    };
    std::unordered_map<int64_t, Trail> trails_;
    void step_trails(double dt);
    // A Beam: Segments quads along a cubic Bezier between two attachments, where
    // P1 = P0 + Attachment0's X * CurveSize0 and P2 = P3 - Attachment1's X * CurveSize1;
    // Width0 to Width1 across, faced along the attachments' Y or turned to the camera.
    struct Beam {
        int64_t att0 = 0, att1 = 0;
        bool enabled = true, faceCamera = false;
        double width0 = 1, width1 = 1, curve0 = 0, curve1 = 0, brightness = 1, localT = 0;
        double textureOffset = 0, offsetAt = 0;   // SetTextureOffset's cycle offset and the host second it was set
        int segments = 10;
        pulseblockz::rbx::Value colorSeq = pulseblockz::rbx::Value::colorSequence(pulseblockz::rbx::Col3{1, 1, 1});
        pulseblockz::rbx::Value transSeq = pulseblockz::rbx::Value::numberSequence(0.5f, 0.5f);
        std::string texture, textureLoaded;
        double textureLength = 1, textureSpeed = 1;
        int textureMode = 0;                   // Stretch / Wrap / Static
        godot::MeshInstance3D* node = nullptr;
        godot::Ref<godot::ImmediateMesh> mesh;
        godot::Ref<godot::StandardMaterial3D> mat;
    };
    std::unordered_map<int64_t, Beam> beams_;
    void step_beams();
    // A Highlight: a tinted copy and a grown, inside-out copy of every mesh of its
    // Adornee (its parent when nil), drawn over everything unless DepthMode is Occluded.
    struct Highlight {
        int64_t adornee = 0;
        bool enabled = true, onTop = true;
        pulseblockz::rbx::Col3 fill{1, 0, 0}, outline{1, 1, 1};
        float fillT = 0.5f, outlineT = 0;
        std::vector<MeshInstance3D*> nodes;
    };
    std::unordered_map<int64_t, Highlight> highlights_;
    bool highlightsDirty_ = false;
    void sync_highlights();
    bool explosionsDirty_ = false;
    void sync_explosions();
    // Constraints: a HingeConstraint is a HingeJoint3D about Attachment0's X, a
    // BallSocketConstraint a PinJoint3D (a ConeTwistJoint3D once its limits are on). Both
    // join the bodies of the attachments' assemblies; CurrentAngle is the angle between
    // the attachments' secondary axes. A part this machine does not simulate gets none.
    struct Constraint {
        std::string className;
        int64_t att0 = 0, att1 = 0;
        bool enabled = true, limits = false, twistLimits = false;
        std::string actuator = "None";
        double angularVelocity = 0, motorMaxTorque = 0, angularSpeed = 0, servoMaxTorque = 0, targetAngle = 0;
        double lower = -45, upper = 45, twistLower = -45, twistUpper = 45;   // degrees, as Roblox has them
        // rope / rod / spring: kept by impulses and forces each physics step, no Godot joint
        double length = 5, restitution = 0, stiffness = 100, damping = 0, freeLength = 5, minLength = 0, maxLength = 1e6, maxForce = 1e6;
        bool winch = false;
        double winchTarget = 0, winchSpeed = 0, sentDist = 1e9;
        // the movers
        pulseblockz::rbx::Vec3 force{0, 0, 0}, torque{0, 0, 0}, vel{0, 0, 0}, angVel{0, 0, 0}, position{0, 0, 0}, lineDir{1, 0, 0}, tangent0{1, 0, 0}, tangent1{0, 1, 0};
        pulseblockz::rbx::Vec3 goalPos{0, 0, 0}, goalOrient{0, 0, 0};   // AlignOrientation.CFrame
        std::string relativeTo = "Attachment0", velocityMode = "Vector";
        bool twoAttachment = true, rigidity = false, atCenterOfMass = false, primaryAxisOnly = false;
        double velocity = 0, motorMaxForce = 0, speed = 0, servoMaxForce = 0, targetPosition = 0;   // a slider's own actuator
        double lowerLimit = -5, upperLimit = 5, pos0 = 0, sentPos = 1e9;
        std::string angularActuator = "None";
        bool angularLimits = false;
        double maxTorque = 0, maxVelocity = 1e6, maxAngularVelocity = 1e6, responsiveness = 10, lineVelocity = 0;
        // ForceLimitMode PerAxis: a per-axis cap instead of maxForce; Roblox defaults to Magnitude.
        pulseblockz::rbx::Vec3 maxAxesForce{1e6f, 1e6f, 1e6f};
        std::string forceLimitMode = "Magnitude";
        // ReactionForceEnabled: put the equal-and-opposite on Attachment1's assembly too.
        // Roblox's default is false, so the target does not move.
        bool reaction = false;
        // MotorMaxAcceleration, rad/s^2 or studs/s^2; 0 is Roblox's infinite default.
        double motorMaxAccel = 0;
        double angularRestitution = 0;   // how much a cylinder bounces off its angular limit
        // LineForce: Magnitude toward Attachment1, over the squared distance when InverseSquareLaw
        double magnitude = 1000;
        bool inverseSquare = false;
        double inclination = 0;          // CylindricalConstraint.InclinationAngle, degrees
        double winchForce = 0, winchResponsiveness = 10;
        double winchNow = 0;   // the speed it is actually winding at, which responsiveness eases toward WinchSpeed
        double limitAngle0 = 0, limitAngle1 = 0;   // a rod's cone at each end, degrees
        float planeVel[2] = {0, 0};
        // the legacy BodyMovers: per-axis caps, P / D gains, a BodyThrust's point
        pulseblockz::rbx::Vec3 maxForceV{4000, 4000, 4000}, maxTorqueV{4000, 4000, 4000}, location{0, 0, 0};
        double pGain = 0, dGain = 0;
        double desiredAngle = 0;               // RotateP: where it serves to, radians
        // DynamicRotate.BaseAngle: the servo goes to BaseAngle + DesiredAngle.
        double baseAngle = 0;
        // the legacy Rotate joints: Part0 / Part1 and C0 / C1 stand in for the attachments; the hinge is C0's Z
        bool rotateJoint = false;
        int64_t part0 = 0, part1 = 0;
        pulseblockz::rbx::Vec3 c0Pos{0, 0, 0}, c0Ori{0, 0, 0}, c1Pos{0, 0, 0}, c1Ori{0, 0, 0};
        godot::Joint3D* node = nullptr;
        godot::Node3D* bodyA = nullptr;
        godot::Node3D* bodyB = nullptr;
        int kind = 0;                        // 1 hinge, 2 pin, 3 cone-twist: what the node is
        double angle0 = 0;                   // the hinge's angle when the joint was made: Godot's zero
        double sentAngle = 1e9;
    };
    std::unordered_map<int64_t, Constraint> constraints_;
    bool constraintsDirty_ = false;
    // NoCollisionConstraint: a collision exception between the two parts' assembly bodies
    // (Godot excepts bodies, not shapes); the pair is remembered so it can be taken off.
    struct NoCollide { int64_t p0 = 0, p1 = 0; bool enabled = true; uint64_t a = 0, b = 0; };
    std::unordered_map<int64_t, NoCollide> noCollide_;
    void sync_no_collide();
    void no_collide_clear(NoCollide& n);
    void update_surface(Part& p);                  // friction / elasticity from the material or CustomPhysicalProperties
    std::unordered_map<uint64_t, godot::Ref<godot::PhysicsMaterial>> physMats_;   // shared by surface
    bool massDirty_ = false;             // a Size or an assembly changed: the rigid bodies' masses
    void refresh_masses();
    void sync_constraints();
    void step_constraints(double dt);    // ropes, rods and springs
    void step_sliders(double dt);        // a prismatic / cylindrical motor or servo
    void step_movers(double dt);         // forces, torques, velocities, alignments
    void step_vehicle_seats(double dt);  // Throttle / Steer push the seat's assembly
    int64_t attachment_part(int64_t att, Transform3D& local) const;
    Transform3D part_frame(const Part& p) const;
    std::vector<pulseblockz::rbx::HostWrite> pendingWrites_;      // the engine's writes gathered between snapshots (CurrentAngle)
    bool emittersDirty_ = false;
    std::unordered_map<int64_t, Transform3D> attachments_;      // an Attachment's frame in its part
    // An AnimationTrack a script made; the engine runs its clock and poses the limbs.
    struct Track {
        int64_t animation = 0, sequence = 0;   // the Animation instance, the KeyframeSequence it names
        bool playing = false, looped = false, resolved = false, stopping = false;
        int priority = 2;                      // Enum.AnimationPriority: Idle 0 .. Core 1000
        double time = 0, speed = 1, weight = 1, weightTarget = 1, fade = 0, length = 0, sentTime = -1, sentWeight = -1;
        std::string lastKeyframe;
    };
    struct PoseData { Transform3D cframe; double weight = 1; };
    std::unordered_map<int64_t, Track> tracks_;
    std::unordered_map<int64_t, std::string> animationIds_;      // an Animation's AnimationId
    std::unordered_map<int64_t, double> keyframes_;              // a Keyframe's Time
    std::unordered_map<int64_t, PoseData> poses_;                // a Pose's CFrame and Weight
    void step_tracks(double dt);                                 // the clock, the loop and the events
    int64_t resolve_path(const std::string& path) const;         // "ReplicatedStorage.Animations.Wave"
    // The limb poses a character's playing track asks for, by limb name.
    bool animation_pose(int64_t characterModel, std::map<std::string, Transform3D>& out);
    Ref<Texture2D> softDot_;                                    // the default particle: a soft round spot
    Ref<Texture2D> soft_dot();
    void sync_emitters();
    void style_emitter(Emitter& e);
    void burst_emitter(Emitter& e, int count);
    void detach_emitters(int64_t partId);
    int64_t emitter_host(int64_t id, Transform3D& offset) const;   // the part an emitter's node hangs under
    // A ProximityPrompt: a billboard Label3D under its part's mesh, drawn while Shown.
    struct Prompt {
        std::string actionText, objectText, key = "E", style = "Default";
        // UIOffset, in pixels: applied in the label's own screen-facing plane and scaled
        // by pixel_size so it stays a pixel measurement at any distance.
        float offsetX = 0, offsetY = 0;
        bool shown = false;
        int64_t part = 0;
        Label3D* node = nullptr;
    };
    // A Sound: an AudioStreamPlayer3D under its part's mesh, or an AudioStreamPlayer heard
    // everywhere. The engine loads the SoundId and reports TimeLength / IsLoaded back.
    struct Sound {
        std::string soundId, loadedId;       // loadedId: what `stream` was made from
        float volume = 0.5f, speed = 1, minDist = 10, maxDist = 10000;
        // RollOffMode. Godot's inverse-distance is Roblox's Inverse exactly; the other
        // three are worked out per frame in attenuate_sounds() with Godot's model off.
        std::string rollOff = "Inverse";
        bool looped = false, playing = false, playOnRemove = false;
        bool regions = false;                // PlaybackRegionsEnabled; the two ranges in seconds
        double region[2] = {0, 60000}, loopRegion[2] = {0, 60000};
        double timePosition = 0;             // where to (re)start from; the node keeps the running clock
        double reportedLoudness = 0;         // last PlaybackLoudness sent, so a steady sound is quiet on the wire
        int64_t part = 0;                    // the part whose mesh the node hangs under; 0 = everywhere
        int64_t group = 0;                   // the SoundGroup it belongs to (its volume rides along)
        godot::Transform3D at;               // where under the mesh it sits (an Attachment moves it)
        Node* node = nullptr;                // AudioStreamPlayer3D or AudioStreamPlayer
        Ref<AudioStream> stream;             // this sound's own copy (Looped is set on it)
    };
    // A Model with a Humanoid in it. The engine moves it as one CharacterBody3D.
    struct Character {
        int64_t humanoid = 0, root = 0;      // root: the built HumanoidRootPart, 0 until then
        double walkSpeed = 16, jumpPower = 50;
        // HipHeight: how far the root floats above the floor. JumpHeight wins over
        // JumpPower when UseJumpPower is off; the two relate by v = sqrt(2 g h).
        double hipHeight = 0, jumpHeight = 7.2;
        pulseblockz::rbx::Vec3 cameraOffset;     // Humanoid.CameraOffset: the camera's focus, in the root's frame
        bool useJumpPower = false;   // Roblox's default for a new Humanoid
        bool jump = false, jumpConsumed = false, autoRotate = true, onFloor = false;
        // Humanoid.Jump as last heard, the host's own write-back included: a request the
        // engine puts down again after reading it.
        bool jumpProp = false;
        bool swimming = false;               // in terrain water: no gravity, strokes where the camera looks
        std::string state;                   // the Humanoid's StateName as last written
        // the name and health bar over the head, from the Humanoid
        std::string displayName;
        double health = 100, maxHealth = 100, nameDistance = 100, healthDistance = 100;
        std::string distanceType = "Viewer", healthType = "DisplayWhenDamaged", occlusion = "OccludeAll";
        pulseblockz::rbx::Vec3 moveDir, sentMoveDir;
        bool walking = false;                // Humanoid:MoveTo in progress
        pulseblockz::rbx::Vec3 walkTo;
        double walkT = 0;                    // seconds into it; MoveToFinished(false) at 8
        std::set<int64_t> touching;          // parts the body slid against last step -> Touched / TouchEnded
        double animPhase = 0, animAmp = 0, jumpBlend = 0, idleT = 0;   // the R6 walk/jump/idle pose
        int64_t seat = 0;                    // Humanoid.SeatPart: pinned on it, legs out
        bool standUp = false;                // just left the seat: the jump that did it
        // A script just set this body's velocity: airborne for one step whatever it stands
        // on. move_and_slide projects into the floor, which would sweep a launch along a slope.
        bool launched = false;
        double sitBlend = 0;
        // Remote character: the owner's poses are eased into over the interval they arrive at.
        Transform3D poseFrom, poseTo;
        double poseT = 0, poseDur = 0;
        uint64_t poseAtMs = 0;
        bool posing = false;
    };
    // A GUI instance on the client -- ScreenGui, GuiObject or a UI modifier -- with its
    // properties as last heard and the Controls drawing it under the local PlayerGui.
    enum GuiKind { GUI_OTHER, GUI_SCREEN, GUI_OBJECT, GUI_MODIFIER, GUI_BILLBOARD, GUI_SURFACE };
    struct Gui {
        GuiKind kind = GUI_OTHER;
        // Its properties changed: style this object again, not every object on screen.
        bool dirty = true;
        bool button = false, text = false, box = false;   // GuiButton; TextLabel / TextButton / TextBox; TextBox
        bool scrolling = false;                           // ScrollingFrame
        bool image = false;                               // ImageLabel / ImageButton
        TextureRect* pic = nullptr;                       // the image, over the Panel
        int picLoaded = -1; Vector2 picSize;              // what the image came to; -1 until styled
        int sentPicLoaded = -1; Vector2 sentPicSize;      // IsLoaded / ContentImageSize last reported
        std::unordered_map<std::string, pulseblockz::rbx::Value> props;
        CanvasLayer* layer = nullptr;                     // ScreenGui
        Panel* dock = nullptr;                            // DockWidgetPluginGui: the floating frame, with its title, the root inside
        Control* node = nullptr;                          // ScreenGui: full-screen root; GuiObject: its Panel; BillboardGui: its box on screen
        int64_t part = 0;                                 // BillboardGui / SurfaceGui: the part it hangs on this frame
        SubViewport* viewport = nullptr;                  // SurfaceGui: the canvas is drawn off screen here (node is its root)...
        MeshInstance3D* quad = nullptr;                   // ...and painted onto the face by this quad
        Ref<StandardMaterial3D> quadMat;
        bool onQuad = false;                              // the pointer was over the face last event
        Control* content = nullptr;                       // GuiObject: children go here (inset by a UIPadding)
        Label* label = nullptr;
        LineEdit* edit = nullptr;                         // TextBox: typed into (in place of the Label)
        TextEdit* multi = nullptr;                        // a MultiLine TextBox: lines instead of the one
        bool submitted = false;                           // TextBox: Enter is taking the focus away
        SubViewport* frame = nullptr;                     // ViewportFrame: its own world, drawn into `pic`
        Camera3D* frameCam = nullptr;                     // ... and what looks at it
        Node3D* frameRoot = nullptr;                      // ... and what the parts hang from
        Control* canvas = nullptr;                        // ScrollingFrame: the children hang here, shifted by CanvasPosition under the clipping content
        VScrollBar* vbar = nullptr; HScrollBar* hbar = nullptr;
        Vector2 canvasPos, canvasSize, windowSize;        // ScrollingFrame: where it is scrolled to, AbsoluteCanvasSize, AbsoluteWindowSize
        // ScrollBarInset: None floats the bar over the content, ScrollBar takes its width
        // while it is shown, Always takes it either way so a list does not jump sideways
        // when it first needs a bar.
        Vector2 inset;
        Vector2 sentCanvasPos{-1, -1}, sentCanvasSize{-1, -1}, sentWindowSize{-1, -1};   // last reported
        bool pixelSized = false; Vector2 parentSize;      // a UIAspectRatioConstraint / UISizeConstraint sized it in pixels from the parent's size
        Vector2 cellCount, cellSize;                      // UIGridLayout: last AbsoluteCellCount / AbsoluteCellSize reported
        Vector2 absScale{1, 1};                           // the scale AbsoluteSize was last reported at (a UIScale above)
        bool hovered = false, pressed = false;
        // The right button's press, kept apart: a Click is a press and a release on the
        // same control and the same button.
        bool pressed2 = false;
        bool listed = false, wasListed = false;           // placed by the parent's UIListLayout this frame
        Rect2 listRect;
        int fontSize = 0;                                 // last TextScaled size
        Vector2 fitBox; String fitText; uint64_t fitFont = 0; // what fontSize was fitted to
        String boundsText; int boundsFs = 0; bool boundsWrap = false; float boundsWidth = -1; uint64_t boundsFont = 0;   // what TextBounds was measured for
        Vector2 sentBounds = Vector2(-1, -1); int sentFits = -1;
        Vector2 absPos, absSize;                          // last AbsolutePosition / AbsoluteSize reported
        // AbsoluteRotation: this Rotation plus every ancestor's. Cached so an unchanged
        // value is not written back every frame.
        float absRot = 0;
    };
    // A TextBox is one node or the other; these reach whichever it is.
    godot::Control* box_node(const Gui& g) const { return g.edit ? (godot::Control*)g.edit : (godot::Control*)g.multi; }
    std::string box_text(const Gui& g) const;
    void set_box_text(Gui& g, const std::string& text, int caret);   // caret: Roblox's CursorPosition, -1 to leave it alone
    int box_caret(const Gui& g) const;                               // Roblox's CursorPosition (1-based, over the lines)
    // A client joining: filled by the server job, consumed when its frame lands. One at a
    // time, so `before` -- replicated ahead of the snapshot -- is what every other client needs.
    struct Join {
        std::string name; int64_t userId = 0;
        int32_t peer = 0;                    // 0: the local client (Play Solo); else the network peer
        int64_t playerId = 0, cameraId = 0;
        std::vector<pulseblockz::rbx::Change> before, snapshot;
        bool done = false, sent = false;     // done: server job ran; sent: snapshot + client job queued
    };
    // The local player, made once the tree lands rather than at Welcome: addPlayer copies
    // StarterPlayerScripts into PlayerScripts, and there is no StarterPlayer before then.
    std::shared_ptr<Join> pendingJoin_;

    // A network client, on the server.
    struct NetClient {
        std::string name; int64_t userId = 0;
        std::string nonce;                   // its sign-in challenge, until a wallet proves itself against it
        int64_t playerId = 0, charId = 0;    // charId: the Model the client owns (simulates and reports)
        bool joined = false;                 // Welcome sent
        std::vector<pulseblockz::rbx::Change> rep;
        std::vector<pulseblockz::rbx::RemoteMsg> remotes;
        std::set<int64_t> moving;            // parts whose last pose went unreliably (see net_flush_clients)
    };

    void ensure_runtime();
    // A change the host wrote itself -- the physics snapshot going back in -- carries
    // fromHost, which is what stops it echoing into the scene. A script's write
    // (p.Position = ...) is not fromHost and always applies.
    void apply_server(pulseblockz::rbx::FrameOut& out);
    // On a client only client-made instances (negative ids) and the purely visual properties
    // of replicated ones (Color, Transparency) reach the scene: the server owns where
    // everything is. This machine simulates and reports its own character alone, which is
    // Roblox's network ownership.
    void apply_client(pulseblockz::rbx::FrameOut& out);
    void finish_frame(pulseblockz::rbx::FrameOut& out, bool server);
    void apply_change(const pulseblockz::rbx::Change& c);
    void apply_humanoid(const pulseblockz::rbx::Change& c);
    // Humanoid properties by Humanoid id, replayed once the character is known: a body
    // built off to one side is written before it is parented, so the writes arrive first.
    std::unordered_map<int64_t, std::unordered_map<std::string, pulseblockz::rbx::Value>> humanoidProps_;
    void replay_humanoid(int64_t humanoidId);
    void apply_lighting(const pulseblockz::rbx::Change& c);
    void apply_input_settings(const pulseblockz::rbx::Change& c);   // UserInputService.MouseBehavior / MouseIconEnabled
    void feed_input(const Ref<InputEvent>& event, bool processed = false);   // engine input -> the client runtime
    void sync_gui();
    void order_gui_siblings();   // ZIndex, then creation order, for drawing and for picking
    float screen_scale(int64_t id) const;   // a ScreenGui's own UIScale, applied to its layer
    // A Camera that is not the client's own: where a ViewportFrame naming it looks from.
    // Only what a frame needs -- CFrame as Position / Orientation, and vertical FieldOfView.
    struct FrameCam {
        pulseblockz::rbx::Vec3 pos{0, 0, 0}, orient{0, 0, 0};
        double fov = 70;
    };
    std::unordered_map<int64_t, FrameCam> frameCams_;
    // A ViewportFrame's own world: the SubViewport its parts are built into, and the camera.
    int64_t viewport_frame_of(int64_t id) const;   // the ViewportFrame above `id`, if any
    Node* viewport_root(int64_t id);               // where a part under one is parented
    bool viewport_ready(int64_t id) const;          // ... and whether that frame exists yet
    void sync_viewport_frames();
    void style_gui(int64_t id, Gui& g);
    static Control* gui_area(Gui& g);                           // the Control a GuiObject's children hang under
    void scroll_to(Gui& g, Vector2 pos);                        // ScrollingFrame: CanvasPosition, clamped to the canvas
    void layout_scroll(int64_t id, Gui& g, std::vector<pulseblockz::rbx::HostWrite>& writes);
    void apply_light(Light& l, const pulseblockz::rbx::Change& c);
    void apply_sound(int64_t id, Sound& s, const pulseblockz::rbx::Change& c);
    void sync_sounds();
    // The Audio API (see sync_audio).
    struct AudioNode { std::string className; std::map<std::string, pulseblockz::rbx::Value> props; };
    struct AudioVoice {
        int64_t player = 0, sink = 0;
        std::vector<int64_t> chain;          // the effects between them, the player's end first
        Node* node = nullptr;                // AudioStreamPlayer3D at an emitter, AudioStreamPlayer otherwise
        int64_t part = 0;                    // the part whose mesh a 3D voice hangs under
        std::string asset;                   // what `stream` was made from
        Ref<AudioStream> stream;
        bool playing = false;
    };
    void apply_audio(int64_t id, AudioNode& a, const pulseblockz::rbx::Change& c);
    static void audio_effects_for(const AudioNode& a, const std::vector<Ref<AudioEffect>>& have, size_t& at,
                                  std::vector<Ref<AudioEffect>>& want);
    void sync_audio();
    void sync_voice_bus(AudioVoice& v);
    void play_voice(AudioVoice& v);
    void attenuate_voices();
    void style_sound(Sound& s);
    void attenuate_sounds();   // RollOffMode's three non-Godot curves, per frame
    float group_volume(int64_t group) const;       // a SoundGroup's volume, its parents' included
    std::unordered_map<int64_t, std::pair<double, int64_t>> soundGroups_;   // id -> volume, parent group
    void start_sound(Sound& s);
    void stop_sound(Sound& s, bool remember);
    void load_sound(int64_t id, Sound& s);
    void sound_removed(int64_t id, Sound& s);
    void detach_sounds(int64_t partId);
    Ref<AudioStream> load_stream(const std::string& soundId);
    void sync_lights();
    void style_light(Light& l);
    void detach_lights(int64_t partId);
    CanvasLayer* core_gui();
    void sync_hotbar();
    void place_name_tags();
    bool occluded(const Vector3& from, const Vector3& to, int64_t part) const;
    Vector3 camera_pop(const Vector3& focus, const Vector3& want, int64_t subject) const;   // Roblox's popper: never inside the world
    int64_t billboard_part(int64_t id) const;
    // Children by parent, so "what is under X" does not walk every entry. Kept exact:
    // added at Create, moved at Parent, dropped at Destroy, and rebuilt whole if kidsCount_
    // ever disagrees with entries_. gui_kids and parts_under are this, filtered.
    mutable std::unordered_map<int64_t, std::vector<int64_t>> kids_;
    mutable size_t kidsCount_ = 0;
    void kids_add(int64_t id, int64_t parent);
    void kids_move(int64_t id, int64_t from, int64_t to);
    void kids_drop(int64_t id, int64_t parent);
    const std::vector<int64_t>& children_of(int64_t parent) const;
    std::vector<int64_t> gui_kids(int64_t parent) const;
    std::vector<int64_t> parts_under(int64_t parent) const;
    int64_t head_part(int64_t model) const;   // the Head under a character model, drawn
    // The parts on each character root -- limbs, the tool, what is worn -- for the
    // animation step. Rebuilt once a frame.
    mutable std::unordered_map<int64_t, std::vector<int64_t>> rootParts_;
    mutable uint64_t rootPartsFrame_ = ~0ull;
    const std::vector<int64_t>& parts_on_root(int64_t root) const;
    std::string layoutNote_;   // how the last layout_gui split its time, for the slow-layout line
    void place_billboard(int64_t id, Gui& g);
    void place_surface(int64_t id, Gui& g);
    void style_surface(Gui& g);
    std::string screen_gui_of(int64_t id) const;   // the ZIndexBehavior of the gui a GuiObject is on
    Vector2 gui_screen_origin(int64_t id) const;   // where a GuiObject's ScreenGui begins: AbsolutePosition counts from there
    bool surface_input(const Ref<InputEvent>& event);
    void apply_prompt(Prompt& pr, const pulseblockz::rbx::Change& c);
    void sync_prompts();
    void style_prompt(Prompt& pr);
    void detach_prompts(int64_t partId);
    void layout_gui(std::vector<pulseblockz::rbx::HostWrite>& writes);
    void free_gui(Gui& g);
    int64_t gui_child(int64_t id, const char* className) const;
    void update_lighting();
    bool local_humanoid(int64_t id) const;
    bool local_root(int64_t id) const;     // the root part of the character this machine simulates
    void submit_frames();
    void snapshot_physics(pulseblockz::rbx::FrameIn& server, pulseblockz::rbx::FrameIn* client);
    bool in_workspace(int64_t id) const;
    Role role_of(int64_t id) const;
    int64_t character_of(int64_t id) const;   // the character Model a limb or a held Tool's part belongs to, else 0
    void refresh_visibility(int64_t id);
    void refresh_all_visibility();
    void build_part(int64_t id, Part& p);
    void free_part(int64_t id, Part& p);
    void update_shape(Part& p);
    void update_material(Part& p);
    void update_transform(Part& p);
    void update_limb_offset(int64_t id, Part& p);
    Ref<Material> material_for(const Part& p);
    std::unordered_map<int64_t, DataMesh> dataMeshes_;
    void attach_data_mesh(int64_t id, DataMesh& m, int64_t partId);
    Ref<Mesh> data_mesh_for(const DataMesh& m, const Part& p);
    std::unordered_map<std::string, Ref<Mesh>> meshes_;        // loaded files, by MeshId
    struct KeptMesh { Ref<Mesh> src; std::vector<std::pair<Mesh::PrimitiveType, Array>> surfaces; };
    std::unordered_map<std::string, Ref<Mesh>> unitMeshes_;    // a SpecialMesh's unit shapes, by MeshType
    std::unordered_map<uint64_t, KeptMesh> keptMeshes_;        // a drawn-from source's arrays, by the mesh's instance id (bake_kept)
    std::set<std::string> meshWarned_;
    // Roblox cloud assets seen, per kind: the first few named in the Output, the rest counted.
    struct CloudNotes { std::set<std::string> ids; int pending = 0; };
    std::map<std::string, CloudNotes> cloud_;
    void cloud_note(const char* kind, const std::string& id, const char* example);
    void flush_cloud_notes();
    Ref<Mesh> load_mesh(const std::string& meshId);
    Ref<Mesh> load_obj(const String& path);
    /// The Workspace's Terrain, or 0 before the runtime has made one.
    int64_t get_terrain_id() const;
    /// {resolution, cell_size, origin, sculpted} -- empty before there is a field.
    godot::Dictionary get_terrain_info() const;
    /// Lay a flat field of `resolution` squares of `cell_size` studs at height `y`,
    /// centred on the origin; replaces whatever was there.
    void terrain_flatten(int resolution, float cell_size, float y);
    /// A ray against the heightfield itself, no physics step: {position, normal}, or {} for a miss.
    godot::Dictionary terrain_raycast(const godot::Vector3& from, const godot::Vector3& dir) const;
    /// Push the ground inside `radius` of `at`. `mode`: 0 raise, 1 lower, 2 smooth,
    /// 3 flatten to `level`. Works on the copy here; nothing reaches the tree until commit.
    void terrain_sculpt(const godot::Vector3& at, float radius, float amount, int mode, float level, int material = -1);
    /// The Enum.Material name of the ground at a point ("Grass"), or "" for air.
    godot::String terrain_material_at(const godot::Vector3& p) const;
    /// What a script asked of the ground, carried out on the copy here.
    void apply_terrain_ops(const std::vector<pulseblockz::rbx::Runtime::TerrainOp>& ops);
    void apply_impulses(const std::vector<pulseblockz::rbx::Runtime::Impulse>& ims);   // a script's ApplyImpulse*, onto the assembly's body
    /// The working copy as the blob Heights carries; writes nothing into the tree.
    godot::String terrain_commit();
    /// The ground height under a point, or a very large negative for "off the field".
    float terrain_height_at(float x, float z) const;

    /// A UnionOperation's baked geometry, decoded from its MeshData blob and cached; null if none.
    Ref<ArrayMesh> op_mesh_for(Part& p);
    // BasePart:UnionAsync and the rest, on Godot's CSG, which builds its mesh only once
    // the combiner has been in the tree for a frame: each job waits here, hidden outside
    // the place, and answers on the FrameIn of whichever runtime asked.
    struct SolidJob {
        uint64_t id = 0;
        bool server = true;
        bool split = false;
        Node3D* root = nullptr;                 // the CSGCombiner3D; null when the ask could not be built
        Transform3D frame;                      // the calling part's: the result is built in it
        bool keepOrigin = false;                // GeometryService: results stay in the main part's space
        int frames = 0;
        std::string error;
    };
    std::vector<SolidJob> solidJobs_;
    // CreateMeshPartAsync: waiting on a mesh, which may be a download still in flight.
    struct MeshJob { uint64_t id = 0; std::string uri; bool server = false; bool geometry = false; };
    std::vector<MeshJob> meshJobs_;
    std::vector<MeshJob> imageJobs_;                                  // CreateEditableImageAsync: the same wait, for an image
    std::vector<pulseblockz::rbx::ImageAnswer> imageAnswersServer_, imageAnswersClient_;
    // ContentProvider:PreloadAsync: each content loaded (or fetched) here, answered when it settles.
    struct PreloadJob { uint64_t id = 0; std::string uri; bool server = false; };
    std::vector<PreloadJob> preloadJobs_;
    std::vector<pulseblockz::rbx::PreloadAnswer> preloadAnswersServer_, preloadAnswersClient_;
    void pump_preloads();
    // DataModelContent holders by id: an Opaque Content's pixels, as the runtime (or the server) sent them.
    struct OpaqueImage { int w = 0, h = 0; std::string rgba, kind = "Image"; };   // rgba: the holder's Data, a mesh's bytes when kind is Mesh
    std::unordered_map<int64_t, OpaqueImage> opaqueImages_;
    std::vector<pulseblockz::rbx::MeshAnswer> meshAnswersServer_, meshAnswersClient_;
    void pump_meshes();
    static std::string mesh_to_editable(const Ref<Mesh>& mesh);
    static Ref<ArrayMesh> editable_to_mesh(const std::vector<float>& pos, const std::vector<float>& nrm, const std::vector<float>& uv, const std::vector<float>& rgba);
    std::unordered_map<int64_t, Ref<ArrayMesh>> editableMeshes_;   // EditableMeshes as the runtime last sent them, by id
    void editable_mesh_report(const pulseblockz::rbx::FrameOut& out, bool mirrored);
    void pump_images();
    std::vector<pulseblockz::rbx::SolidAnswer> solidAnswersServer_, solidAnswersClient_;
    void start_solid(const pulseblockz::rbx::SolidAsk& ask, bool server);
    void pump_solids();
    Ref<ArrayMesh> solid_operand_mesh(const pulseblockz::rbx::Runtime::SolidOperand& o, bool keepUVs);
    Ref<ArrayMesh> union_drawn(const Ref<ArrayMesh>& decoded, const Vector3& scale, const Vector3& size, float smoothingDegrees, bool smooth);
    void build_lods(Part& p, const Ref<Mesh>& drawn, const Ref<ArrayMesh>& decoded, const Vector3& scale, bool unionSmoothing);
    void ensure_lod(Part& p, int band);
    void apply_sound_service(const pulseblockz::rbx::Change& c);
    void update_listener();
    Transform3D ear() const;   // where this client hears from: the listener when one is current, else the camera
    int lod_band_for(const Part& p) const;
    void update_lods();
    std::unordered_map<std::string, std::vector<PackedVector3Array>> hullCache_;   // convex decompositions, by mesh and fidelity
    // A decomposition in progress on its own thread. The part wears one quick hull until it
    // lands; then every part waiting on that key has its shapes made again from the cache.
    struct HullJob {
        std::string key;
        Ref<Mesh> mesh;
        std::string fidelity;
        std::vector<PackedVector3Array> hulls;
        std::atomic<bool> done{false};
        std::thread thread;
    };
    std::unordered_map<std::string, std::unique_ptr<HullJob>> hullJobs_;
    std::unordered_map<std::string, std::vector<int64_t>> hullWaiters_;   // key -> parts wearing the stand-in
    // Parts resized this frame: only their decals and emitters are styled again.
    std::unordered_set<int64_t> resizedParts_;
    std::map<std::string, int> guiDirtBy_;    // what dirtied the GUI this frame, for the slow-frame line
    // Changes over the wire in the last second, by class and property; a second past
    // fifteen hundred prints its top five.
    std::map<std::string, int> wireTally_;
    int wireCount_ = 0;
    std::chrono::steady_clock::time_point wireSince_ = std::chrono::steady_clock::now();
    double guiEnsureMs_ = 0, guiStyleMs_ = 0, guiFramesMs_ = 0, guiOrderMs_ = 0;   // the last sync_gui, by part
    std::map<int, int> emitterDirtBy_, soundDirtBy_;   // source lines that dirtied them this frame
    void dirty_emitters(int line) { emittersDirty_ = true; emitterDirtBy_[line]++; }
    void dirty_sounds(int line) { soundsDirty_ = true; soundDirtBy_[line]++; }
    int guiStyled_ = 0;
    void poll_hull_jobs();
    // Restyle only what names `key` -- the GUI objects, decals and emitters using that
    // asset. A client with an empty cache fetches hundreds, so a full restyle each is too much.
    void touch_asset(const std::string& key);
    // Watchdog: `phase_` is a literal the frame sets as it goes, `phaseId_` the instance it
    // was on. The thread prints them once a frame is five seconds late, and every five after.
    std::atomic<const char*> phase_{"idle"};
    std::atomic<int64_t> phaseId_{0};
    std::atomic<int64_t> lastTickUsec_{0};
    std::atomic<bool> watchdogStop_{false};
    std::thread watchdog_;
    void start_watchdog();
    std::vector<Ref<Shape3D>> collision_shapes(const std::string& key, const Ref<Mesh>& source, const Transform3D& fit, const std::string& fidelity);
    void set_collision_shapes(Part& p, const std::vector<Ref<Shape3D>>& shapes, const Transform3D& local);
    static void bake_mesh(const Ref<ArrayMesh>& out, const Ref<Mesh>& src, const Transform3D& xf);
    void bake_kept(const Ref<ArrayMesh>& out, const Ref<Mesh>& src, const Transform3D& xf);   // the same, from arrays kept on this side
    void forget_mesh(const std::string& meshId);
    static void collect_meshes(const Ref<ArrayMesh>& out, Node* node, const Transform3D& xf);
    bool wears_skin(const Part& p) const;
    void update_skin(Part& p);
    void queue_job(std::function<void(pulseblockz::rbx::Runtime&)> job);
    // The debugger: breakpoints by script full name go to both runtimes; the one
    // with that script stops at it. While either stands still, so does the physics.
    void set_breakpoint(const godot::String& script, int line, bool on);
    void debug_continue();
    void debug_step(int kind);
    void debug_report(const pulseblockz::rbx::FrameOut& out, bool server);
    void plugin_report(const pulseblockz::rbx::FrameOut& out);   // what the plugins built this frame, as signals to the Studio
    bool plugin_script(int64_t script) const;                  // a script under PluginDebugService
    bool in_place(int64_t id) const;                           // in the tree, and not a plugin's own (CoreGui, PluginDebugService)
    bool debugPausedServer_ = false, debugPausedClient_ = false, physicsHeld_ = false;
    void dispatch_join();
    bool remote_char(int64_t model) const;
    bool takes_pose_from_tree(int64_t id, const Part& p) const;
    void net_poll();
    void net_send(int32_t peer, const pulseblockz::rbx::NetPacket& pk, bool reliable = true);
    void rest_pose(int64_t id, std::vector<pulseblockz::rbx::HostWrite>* writes, std::vector<pulseblockz::rbx::Change>* changes);
    void net_receive(int32_t peer, pulseblockz::rbx::NetPacket& pk);
    void net_flush_clients();
    void step_characters(double dt);
    void remote_pose(Character& ch, const Part& root);
    void animate_character(Character& ch, double dt);
    void update_camera();
    Vector3 camera_forward() const;

    pulseblockz::rbx::Runtime::Options opts_;
    bool trackProps_ = false;      // keep every property change in the mirror (a Studio does)
    bool editMode_ = false;        // nothing runs, nothing falls
    // The Workspace's ground: a field of voxel density, as Roblox's is, so it can hold a
    // cave or an overhang. The instance's blob is what the place keeps; this is the decoded
    // copy the main thread draws, collides and sculpts on.
    struct Terrain {
        int64_t id = 0;
        int nx = 0, ny = 0, nz = 0;        // samples along each axis
        float cell = 4.0f;                 // studs between samples
        godot::Vector3 origin;             // world position of sample (0, 0, 0)
        std::vector<uint8_t> d;            // occupancy 0..255 per voxel, Roblox's own meaning (see s)
        std::vector<uint8_t> m;            // material per sample, as an index into the palette
        // The surface field the mesh, the height sampler and the rays read. Roblox's
        // occupancy is matter around the voxel centre: a voxel of occupancy o has its
        // surface 4*o studs out from its centre, and the air voxel beside it is 4*(1-o)
        // short of it. Encoded with 128 as the surface -- material o -> 128 + 127*o, air
        // next to matter -> 127 - 127*(1 - max neighbour o) -- so the crossing lands there.
        std::vector<uint8_t> s;
        // The same field for the Water voxels, which the ground field leaves out: nothing
        // stands on water, a character swims in it, a loose part floats or sinks by density.
        std::vector<uint8_t> sw;
        godot::Color waterColor = godot::Color(12 / 255.f, 84 / 255.f, 91 / 255.f);
        float waterTransparency = 0.3f;
        // A shader, not a standard material: WaveSize and WaveSpeed are motion.
        godot::Ref<godot::ShaderMaterial> waterMat;
        float waterReflectance = 1, waveSize = 0.15f, waveSpeed = 10;
        std::string sent;                  // the blob this was decoded from
        godot::Color color = godot::Color(0.42f, 0.56f, 0.30f);
        bool dirty = false;                // some chunk wants redrawing
        godot::StaticBody3D* body = nullptr;
        // Drawn and collided in chunks of CH cells a side, so a stroke or a FillBlock
        // remeshes the handful it touched and not the field.
        static const int CH = 16;
        struct Chunk { godot::MeshInstance3D* mesh = nullptr; godot::CollisionShape3D* col = nullptr; godot::MeshInstance3D* water = nullptr; bool dirty = true; };
        int cx = 0, cy = 0, cz = 0;        // chunks along each axis
        std::vector<Chunk> chunks;
        godot::Ref<godot::StandardMaterial3D> mat;
        std::vector<godot::Color> colors;  // the palette the ground is drawn in: kTerrainMats' unless the place brought its own
        int rebuilt = 0;                   // chunks remeshed by the last rebuild, for a test to see
        bool empty() const { return nx < 2 || ny < 2 || nz < 2 || d.empty(); }
        size_t at(int x, int y, int z) const { return ((size_t)z * ny + y) * nx + x; }
        // Air outside the field, so the mesh closes over its sides and bottom.
        float dens(int x, int y, int z) const {
            if (x < 0 || y < 0 || z < 0 || x >= nx || y >= ny || z >= nz) return 0.0f;
            return (s.size() == d.size() ? s : d)[at(x, y, z)] * (1.0f / 255.0f);
        }
        // The ground's occupancy itself (a water voxel is not ground), 0 outside the field.
        float occ(int x, int y, int z) const {
            if (x < 0 || y < 0 || z < 0 || x >= nx || y >= ny || z >= nz) return 0.0f;
            size_t i = at(x, y, z);
            return isWater(i) ? 0.0f : d[i] * (1.0f / 255.0f);
        }
        // The water's, the same way.
        float wdens(int x, int y, int z) const {
            if (x < 0 || y < 0 || z < 0 || x >= nx || y >= ny || z >= nz || sw.size() != d.size()) return 0.0f;
            return sw[at(x, y, z)] * (1.0f / 255.0f);
        }
        float wocc(int x, int y, int z) const {
            if (x < 0 || y < 0 || z < 0 || x >= nx || y >= ny || z >= nz) return 0.0f;
            size_t i = at(x, y, z);
            return isWater(i) ? d[i] * (1.0f / 255.0f) : 0.0f;
        }
        int waterIndex = -1;               // Water's place in the palette
        bool isWater(size_t i) const { return waterIndex >= 0 && m.size() == d.size() && m[i] == waterIndex; }
    } terrain_;
    float terrain_water_density(const godot::Vector3& p) const;     // the water field between samples
    bool terrain_in_water(const godot::Vector3& p) const;
    void terrain_style_water();                                       // the water material from WaterColor / WaterTransparency
    godot::Dictionary terrain_voxel_at(const godot::Vector3& p) const; // the voxel round a point: occupancy, material, its ground and water readings (for tests)
    void step_buoyancy(double dt);                                    // loose parts in water float or sink by density
    /// The occupancy field between samples (trilinear), for writing Roblox's voxels back out.
    float terrain_occupancy(const godot::Vector3& p) const;
    /// Recompute the surface field s from the occupancies d over that box of
    /// samples (and one more each way, since an air sample reads its neighbours).
    void terrain_refresh_surface(int x0, int y0, int z0, int x1, int y1, int z1);
    void terrain_decode(const std::string& blob);
    /// Roblox's SmoothGrid (base64, as a place file has it) read into the field;
    /// the byte layout is in the .cpp. Writes the field back to the tree as Heights
    /// and clears the SmoothGrid, so this engine's blob is the one that persists.
    void terrain_decode_smoothgrid(const std::string& b64);
    /// The field as Roblox's SmoothGrid (base64), for a place file Studio opens.
    godot::String terrain_smoothgrid() const;
    /// Roblox's MaterialColors (base64 of its 69-byte blob): read into the palette,
    /// and the palette written back out in that shape.
    void terrain_apply_material_colors(const std::string& b64);
    godot::String terrain_material_colors() const;
    godot::Color terrain_color(int paletteIndex) const;
    void terrain_decode_heights(const godot::PackedByteArray& raw);   // the older height-per-column blob
    std::string terrain_encode() const;
    float terrain_density(const godot::Vector3& p) const;              // trilinear, air outside
    godot::Vector3 terrain_gradient(const godot::Vector3& p) const;
    void rebuild_terrain();
    void terrain_chunks_reset();                                     // the field changed shape: every chunk anew
    void terrain_mark(int x0, int y0, int z0, int x1, int y1, int z1); // samples touched -> chunks dirty
    void terrain_mesh_chunk(int i, int j, int k);
    void terrain_mesh_pass(Terrain::Chunk& c, int ci, int cj, int ck, bool water);   // one of the two fields, ground or water
    /// {chunks, dirty, rebuilt_last}: how much of the ground the last change redrew.
    godot::Dictionary get_terrain_stats() const;

    bool guiPreview_ = false;      // StarterGui drawn as a picture, for an editor
    godot::Rect2 guiPreviewRect_;  // where on screen it is laid out
    int64_t madeCount_ = 0;        // stamps Entry::made, so children come back in order
    int64_t treeVersion_ = 1;      // bumped by every create / destroy / reparent / rename
    int mode_ = MODE_PLAY_SOLO;
    std::string playerName_ = "Player1";
    int64_t userId_ = 1;

    bool autoJoin_ = true, defaultControls_ = true, defaultCamera_ = true, defaultAnimations_ = true;
    bool threaded_ = true, autoStep_ = true;
    std::unique_ptr<pulseblockz::rbx::RuntimeThread> server_, client_;
    bool inFlight_ = false;
    int framesDone_ = 0;
    double pendingDt_ = 0;
    std::vector<std::function<void(pulseblockz::rbx::Runtime&)>> jobs_, clientJobs_;
    std::vector<pulseblockz::rbx::HostEvent> events_;
    std::set<std::pair<int64_t, int64_t>> touchedThisFrame_;
    std::vector<pulseblockz::rbx::Change> toClientRep_;        // server -> client, next client frame
    std::vector<pulseblockz::rbx::RemoteMsg> toClientRemotes_, toServerRemotes_;
    std::shared_ptr<Join> join_;                                // the join in flight
    std::deque<std::shared_ptr<Join>> joinQueue_;
    int64_t localPlayerId_ = 0, localChar_ = 0;

    // network
    Ref<ENetMultiplayerPeer> peer_;
    bool netClient_ = false;                                    // MODE_CLIENT: client_ is fed over the wire
    bool helloSent_ = false, serverConnected_ = false;
    int listenPort_ = 0, serverPort_ = 8800, physicsLayer_ = 1;
    int maxClients_ = 32;
    // Two collision layers: anchored parts and characters on one, loose parts on the
    // other, so a character's sweep can be retried against the anchored world alone.
    uint32_t static_bit() const { return 1u << (physicsLayer_ - 1); }
    uint32_t rigid_bit() const { return 1u << (physicsLayer_ + 15); }
    std::string serverAddress_ = "127.0.0.1", bindAddress_ = "*";   // bind "127.0.0.1" to listen on loopback only
    std::string dataStorePath_ = "user://datastores.json";          // DataStoreService's file; "" keeps it in memory
    std::unordered_map<int32_t, NetClient> clients_;            // server: by peer id
    std::set<int64_t> remoteChars_;                             // server: character Models a client owns
    std::vector<pulseblockz::rbx::HostWrite> netWrites_;   // server: accepted client writes, next frame
    std::set<int64_t> movingUp_;         // Client: parts whose last reported pose went unreliably
    Dictionary stats_;
    std::unordered_map<int64_t, std::string> buildNote_;   // what a slow part build was spent on, by part
    double hostSubmitMs_ = 0.0;   // last frame's snapshot-and-submit, for the per-second line

    std::unordered_map<int64_t, Entry> entries_;               // every instance heard of
    std::unordered_map<int64_t, Part> parts_;                  // BaseParts
    std::unordered_map<int64_t, Character> chars_;             // character model id -> its body
    double jump_speed(const Character& ch) const;
    void report_loudness(std::vector<pulseblockz::rbx::HostWrite>& writes);
    bool sound_regions(const Sound& s, double& lo, double& hi, double& ls, double& le) const;
    void clock_sound_regions();
    void sync_sound_bus(int64_t id, Sound& s);
    // Roblox reserves 36px at the top for its topbar; ScreenGui sits below unless IgnoreGuiInset.
    static constexpr float kGuiInsetTop = 36.0f;
    float gui_inset(int64_t screenGuiId) const;
    std::unordered_map<uint64_t, int64_t> bodyToId_;           // body node instance id -> part id
    std::unordered_map<uint64_t, int64_t> shapeToId_;          // collision shape instance id -> part id (a welded assembly shares a body)
    // Which part of an assembly a contact is on: the shape's own, not the body's root.
    int64_t part_of_shape(godot::CollisionObject3D* co, int64_t shapeIndex, int64_t fallback) const;
    std::unordered_map<std::string, Ref<Material>> materials_; // shared by look
    // Parts whose look wanted a texture that had not arrived: looked at again each frame.
    std::set<int64_t> awaitingTexture_;
    Ref<Shader> skinShader_[2];                                // opaque / transparent gradient skin
    Ref<GradientTexture1D> skinRamp_;                          // the colour sweep, shared
    int64_t workspaceId_ = -1;
    bool visibilityDirty_ = false;
    std::unordered_map<int64_t, Gui> guis_;                    // client: GUI instances
    bool guiDirty_ = false;
    // What a sync must do beyond the dirty objects: restyle everything, re-sort siblings.
    bool guiRestyleAll_ = true;
    bool guiOrderDirty_ = true;
    std::unordered_map<int64_t, Light> lights_;
    bool lightsDirty_ = false;
    std::unordered_map<int64_t, Prompt> prompts_;
    bool promptsDirty_ = false;
    // The Backpack's hotbar, in the engine's own CoreGui: a button per Tool.HotbarSlot, the
    // one in hand lit, a click equips. SetCoreGuiEnabled(Backpack, false) hides it.
    std::unordered_map<int64_t, int> hotbarSlots_;             // client: tool id -> slot 1-9
    bool hotbarDirty_ = false;
    int coreGuiHidden_ = 0;                                     // StarterGui.CoreGuiHidden: CoreGuiType bits
    // Roblox's cloud: an rbxassetid:// fetched once into user://roblox_cache/<id>.<ext>,
    // its kind told by its bytes; whatever asked for it re-asks when it lands.
    struct CloudAsset {
        std::string num, kind, file;
        std::set<std::string> ids;         // the spellings that asked
        bool failed = false, inTree = false;
        double failedAt = 0;               // when it failed: a pblockz:// is asked for again after a while
        bool asked = false;                // pblockz://: the host has been asked and has not answered
        double askedAt = 0;                // when, so an ask nobody was listening for is asked again
        godot::HTTPRequest* req = nullptr;
    };
    // A pblockz:// is the host's to fetch and check: the world asks (asset_wanted), the host answers here.
    void asset_arrived(const String& uri, const String& path);
    // ENet's own account of this connection: rtt, loss, bytes and packets each way, queue depth.
    Dictionary net_stats() const;
    // Every texture this program made, by the site that made it: bytes and count. Godot 4.3
    // reports texture memory as one number and will not list what is in it.
    Dictionary texture_tally() const;
    void note_texture(const char* site, int w, int h, int bytesPerPixel = 4);
    std::map<std::string, std::pair<int64_t, int64_t>> texTally_;   // site -> (bytes, count)
    std::unordered_map<std::string, CloudAsset> cloudAssets_;   // by numeric id
    // Requests in flight, by the runtime's id, so an answer wakes the script that parked on it.
    std::unordered_map<uint64_t, godot::HTTPRequest*> httpJobs_;
    std::vector<pulseblockz::rbx::HttpAnswer> httpAnswers_;      // for the next FrameIn
    void start_http(const pulseblockz::rbx::HttpAsk& ask);
    void _on_http_done(int result, int code, const godot::PackedStringArray& headers,
                       const godot::PackedByteArray& body, int64_t id);
    // EditableImages: a texture per one, updated from the bitmap the runtime hands over each frame it was drawn into
    struct EditableTex { godot::Ref<godot::Image> image; godot::Ref<godot::ImageTexture> texture; };
    std::unordered_map<int64_t, EditableTex> editableTex_;
    void editable_report(const pulseblockz::rbx::FrameOut& out, bool mirrored);
    std::string cloudFetchBase_ = "https://assetdelivery.roblox.com/v1/asset/?id=";
    // A .ROBLOSECURITY session, so a place's own meshes and textures can be fetched. Roblox
    // serves an uploaded mesh only to a session with rights to it, which is why an anonymous
    // fetch of one answers 401 while a public 2020 decal answers 302.
    //
    // It is full account access, so: never written to a scene (the setter is bound, no
    // ADD_PROPERTY), never printed, and sent ONLY when cloudFetchBase_ is still Roblox's own
    // https host -- kCloudAuthHost. A base pointed elsewhere gets no cookie.
    std::string cloudCookie_;
    static constexpr const char* kCloudAuthHost = "https://assetdelivery.roblox.com/";
    // HttpService, off unless the person running the place turns it on -- Roblox's "Allow
    // HTTP Requests". Server only: a client's asks are refused in the runtime and only
    // apply_server drains them here.
    void set_http_enabled(bool v);
    bool get_http_enabled() const;
    void set_cloud_fetch_base(const String& v);
    String get_cloud_fetch_base() const;
    void set_cloud_cookie(const String& v);
    bool has_cloud_cookie() const;   // whether one is set, never the value
    int cloud_cookie_len() const;    // its length, so a test can check the parsing
    std::string cloud_local(const std::string& id, const char* kind);   // the cached file once it is here; starts the fetch otherwise ("")
    void _on_cloud_done(int result, int code, const PackedStringArray& headers, const PackedByteArray& body, const String& num);
    void cloud_arrived(CloudAsset& a);
    void cloud_into_tree(CloudAsset& a);
    Ref<ArrayMesh> load_rbxmesh(const PackedByteArray& bytes);
    int64_t starterGuiId_ = 0;
    int64_t coreGuiId_ = 0;        // where a plugin's GUIs are: drawn in the editor as in game
    CanvasLayer* coreGuiLayer_ = nullptr;                       // the engine's own GUI: the hotbar, names over heads
    Control* hotbarRoot_ = nullptr;
    // Roblox's name tag: DisplayName over every other character's head with a
    // health bar under it when hurt, within the Humanoid's display distances.
    struct NameTag { Control* node = nullptr; Label* name = nullptr; Panel* bar = nullptr; Panel* fill = nullptr; };
    std::unordered_map<int64_t, NameTag> nameTags_;             // character model id -> its tag
    Control* namesRoot_ = nullptr;
    // Roblox's player list (top right, grouped by Team when there are Teams) and the local
    // Humanoid's health bar while hurt; CoreGuiType PlayerList / Health turn them off.
    struct PlayerInfo { std::string displayName; int64_t team = 0, character = 0; pulseblockz::rbx::Col3 teamColor{0xF2 / 255.f, 0xF3 / 255.f, 0xF3 / 255.f}; bool neutral = true; std::string movementMode = "UserChoice";
                        double minZoom = 0.5, maxZoom = 400; };   // Player.CameraMinZoomDistance / CameraMaxZoomDistance
    // DevComputerMovementMode Scriptable: the default controls leave the character to the place.
    bool controls_scripted() const { auto it = players_.find(localPlayerId_); return it != players_.end() && it->second.movementMode == "Scriptable"; }
    std::unordered_map<int64_t, PlayerInfo> players_;
    std::unordered_map<int64_t, pulseblockz::rbx::Col3> teams_;   // team id -> TeamColor
    bool playerListDirty_ = false;
    Control* playerListRoot_ = nullptr;
    VBoxContainer* playerList_ = nullptr;
    Control* healthRoot_ = nullptr;
    Panel* healthFill_ = nullptr;
    double healthShown_ = -1;                                   // the fraction drawn; -1 hidden
    void sync_player_list();
    void sync_health();
    bool ally(int64_t model) const;
    // Values of StringValue / IntValue / NumberValue / BoolValue; those in a leaderstats Folder are columns.
    std::unordered_map<int64_t, pulseblockz::rbx::Value> statValues_;
    // Tool.Grip (the four Grip vectors) per Tool: where the right hand holds the Handle
    struct Grip { Vector3 pos{0, 0, 0}, forward{0, 0, -1}, right{1, 0, 0}, up{0, 1, 0}; };
    std::unordered_map<int64_t, Grip> grips_;
    bool leaderstat(int64_t id) const;                          // a stat in a Player's leaderstats, or that Folder
    static bool statClass(const std::string& className);
    // Roblox's chat: the window at the top left (`/` focuses the box, Enter sends) and a
    // bubble over the speaker for Chat.BubbleChatEnabled. CoreGuiType.Chat hides the window.
    struct Bubble { int64_t model = 0, part = 0; std::string text; double until = 0; PanelContainer* node = nullptr; };
    // StarterGui:SetCore("SendNotification"): cards stacked in the bottom-right corner, each gone
    // when its Duration is up or one of its buttons is pressed.
    struct NotificationCard { int64_t serial = 0; double until = 0; PanelContainer* node = nullptr; };
    std::vector<NotificationCard> notificationCards_;
    VBoxContainer* notificationsRoot_ = nullptr;
    int64_t notificationSerial_ = 0;
    void notify(const pulseblockz::rbx::Runtime::Notification& n);
    void expire_notifications();
    std::vector<Bubble> bubbles_;
    Control* chatRoot_ = nullptr;
    VBoxContainer* chatMessages_ = nullptr;
    LineEdit* chatInput_ = nullptr;
    Control* bubblesRoot_ = nullptr;
    int64_t chatServiceId_ = 0;
    bool bubbleChat_ = true;
    bool chatTyping_ = false;                                   // the box has focus: the keys are its
    int64_t typingBox_ = 0;                                     // the TextBox with focus, likewise
    void sync_chat();
    // Roblox's top bar and escape menu: Escape or the button opens it -- Resume, Reset
    // Character, Leave. SetCore TopbarEnabled hides the bar, ChatActive the chat window.
    // While it is open the controls and the camera let go, every input reaches scripts as
    // processed, and GuiService.MenuIsOpen follows it.
    Control* topbarRoot_ = nullptr;
    Button* chatToggle_ = nullptr;
    Control* menuRoot_ = nullptr;
    VBoxContainer* menuPlayers_ = nullptr;
    PanelContainer* confirmBox_ = nullptr;
    Label* confirmText_ = nullptr;
    Button* confirmYes_ = nullptr;
    int confirmKind_ = 0;                                       // 1 Reset Character, 2 Leave
    bool menuOpen_ = false, topbarEnabled_ = true, chatActive_ = true;
    void sync_topbar();
    void sync_menu_players();
    void set_menu_open(bool open);
    bool is_menu_open() const { return menuOpen_; }
    void _on_menu_pressed();
    void _on_chat_toggle_pressed();
    void _on_menu_button(int which);                            // 0 Resume, 1 Reset Character, 2 Leave
    // Graphics Quality: Automatic, or a level 1 to 10, read by a script as
    // UserGameSettings.SavedQualityLevel. The level governs shadows, bloom and the
    // fraction of the window the 3D view is drawn at; Automatic holds the GPU's share of
    // a frame under half, moving one level at a time.
    int qualitySaved_ = 0;                                      // 0 Automatic, 1..10
    int qualityLevel_ = 10;                                     // the level in effect
    bool qualityLoaded_ = false, qualityPersist_ = true;         // qualityPersist_ false: a level set without being saved
    double qualityGpuMs_ = 0, qualityFrameMs_ = 0, qualityHold_ = 0;
    int qualityFrames_ = 0;
    CheckButton* qualityAuto_ = nullptr;
    HSlider* qualitySlider_ = nullptr;
    Label* qualityValue_ = nullptr;
    void set_quality(int saved, bool save);
    int get_quality() const { return qualitySaved_; }
    int get_quality_level() const { return qualityLevel_; }
    void load_quality();
    void apply_quality();
    void step_auto_quality(double delta);
    void sync_quality_row();
    void _on_quality_auto(bool on);
    void _on_quality_slid(double value);
    void _on_confirm(bool yes);
    void _on_notification_button(int64_t serial, int64_t callback, String text);
    void chat_line(const pulseblockz::rbx::Runtime::ChatLine& line);
    void place_bubbles();
    void _on_chat_submitted(const String& text);
    void _on_chat_focus(bool in);
    CanvasLayer* billboardLayer_ = nullptr;                     // every BillboardGui's box, under the ScreenGuis
    HBoxContainer* hotbarBox_ = nullptr;
    std::vector<Button*> hotbarButtons_;
    std::unordered_map<int64_t, Sound> sounds_;
    std::unordered_map<int64_t, AudioNode> audio_;
    std::map<std::pair<int64_t, int64_t>, AudioVoice> voices_;   // (player, sink)
    std::unordered_map<int64_t, std::string> audioReported_;     // player -> the asset its length was reported for
    void report_audio_player(int64_t player, const std::string& asset, bool ok);
    bool audioDirty_ = false;
    bool soundsDirty_ = false;
    std::vector<pulseblockz::rbx::HostWrite> assetWrites_;      // Sound TimeLength / IsLoaded, MeshPart MeshSize, for the next frame
    std::vector<Node*> orphanSounds_;                          // PlayOnRemove: players outliving their Sound
    std::unordered_map<std::string, Ref<AudioStream>> streams_; // loaded files, by SoundId
    std::set<std::string> soundWarned_;                        // SoundIds already complained about
    std::unordered_map<std::string, Ref<Texture2D>> textures_;  // loaded images, by Image / Icon
    std::unordered_map<std::string, Ref<Font>> fonts_;          // system fonts, by FontFace family / weight / style
    Ref<Font> load_font(const pulseblockz::rbx::Value& face, bool pixel = false);
    bool gui_pixel_font(int64_t id) const;   // a PixelFont attribute on it or anything it is under
    std::set<std::string> textureWarned_;
    static std::string content_key(const pulseblockz::rbx::Value& v);
    bool forwardingContent_ = false;           // apply_change is passing a Content on under its string twin's name
    std::string uisMouseIcon_, mouseIcon_s_;   // UserInputService.MouseIcon wins over Mouse.Icon while it is not empty
    Ref<Texture2D> load_texture(const std::string& imageId);
    int64_t mouseId_ = 0;                                       // the local client's Mouse (Player:GetMouse())
    int64_t soundServiceId_ = 0;                                // the client's SoundService: where it listens from
    int64_t materialServiceId_ = 0;
    std::unordered_map<std::string, std::string> materialOverrides_;   // Material name -> the MaterialVariant name it wears by default
    AudioListener3D* listener_ = nullptr;                       // SoundService:SetListener, when not the camera
    int listenerType_ = 0;                                      // Enum.ListenerType: 0 Camera, 1 CFrame, 2 ObjectPosition, 3 ObjectCFrame
    int64_t listenerObject_ = 0;
    pulseblockz::rbx::Vec3 listenerPos_{}, listenerOrient_{};
    void apply_mouse_icon(const std::string& icon);
    std::string assetRoot_ = "res://";                         // where a bare SoundId ("sounds/thud.ogg") is
    double gravity_ = 196.2, engineGravity_ = 9.8, fallenHeight_ = -500;

    // default lighting: the Lighting service as a sun, a sky, ambient and fog
    bool defaultLighting_ = true;
    DirectionalLight3D* sun_ = nullptr;
    WorldEnvironment* env_ = nullptr;
    int64_t lightingId_ = 0;
    int64_t uisId_ = 0;                                        // UserInputService, once created
    int64_t userGameSettingsId_ = 0;                           // the client's UserGameSettings, once a script asks for it
    bool cameraRelative_ = false;                              // its RotationType is CameraRelative
    std::string mouseBehavior_ = "Default";
    bool mouseIcon_ = true;
    struct LightingState {
        pulseblockz::rbx::Vec3 ambient{0, 0, 0}, outdoorAmbient{0.5f, 0.5f, 0.5f}, fogColor{0.75f, 0.75f, 0.75f};
        double brightness = 3, clockTime = 14, fogStart = 0, fogEnd = 100000, exposure = 0;
        // Roblox lights the shade from the sky as well as from OutdoorAmbient, scaled by these.
        double envDiffuse = 1, envSpecular = 1;
        bool shadows = true;
    } lighting_;
    // Lighting's children -- post-processing effects, the Atmosphere, the Sky -- as their
    // properties were last heard; sync_effects folds the enabled ones under Lighting (or
    // the camera) into the Environment.
    struct Effect {
        std::string className;
        std::map<std::string, pulseblockz::rbx::Value> props;
    };
    std::unordered_map<int64_t, Effect> effects_;
    bool effectsDirty_ = false;
    // A SurfaceAppearance or a MaterialVariant moved, so every part has to be dressed again.
    bool materialsDirty_ = false;
    const Effect* surface_for(const Part& p) const;
    void sync_effects();
    void set_emitter_cloud(Ref<class ParticleProcessMaterial>& pm, const std::string& shape,
                           bool surfaceOnly, const std::string& inOut, const Vector3& half);
    Ref<StyleBox> scrollbar_grabber(const std::unordered_map<std::string, pulseblockz::rbx::Value>& p,
                                    float thickness, const Color& tint);
    void update_vehicle_hud();
    Control* vehicleHud_ = nullptr;
    Label* vehicleSpeed_ = nullptr;
    void update_mouse_mode();
    bool mouseModeDirty_ = false;
    int wantedMouseMode_ = 0;
    int64_t handle_of(int64_t owner) const;
    void update_studs(Part& p);
public:
    Array part_surface_pieces(int64_t id, const String& which) const;
    Object* gui_control(int64_t id) const;
    Transform3D part_offset(int64_t id) const;
    int wanted_mouse_mode() const;
private:
    // A Sky's six images as a cubemap; false when it has none and the procedural sky stands.
    bool apply_skybox(Ref<Sky>& skyRes, const Effect* sky);
    void update_sky_bodies();    // where the sun and the moon are, which ClockTime moves
    std::string skyboxIds_;      // which six are up, so it is not rebuilt every pass
    std::string skyBodyIds_;     // and which sun and moon textures, for the same reason
    // The sky's own shader, held so update_lighting can move the sun and the moon with
    // ClockTime without going near the cubemap.
    Ref<ShaderMaterial> skyMaterial_;
    // Weld / Snap / ManualWeld / Motor6D (Part1 = Part0 * C0 * Rz(angle) * C1^-1) or a
    // WeldConstraint (the offset the parts had when it took hold). rebuild_assemblies
    // unions the joined parts: one root body -- anchored if any, else the biggest --
    // carries every other part's mesh and collision shape, as a limb does on a character.
    struct Joint {
        std::string className;
        int64_t part0 = 0, part1 = 0;
        bool enabled = true, captured = false;
        Transform3D c0, c1, rel;
        Transform3D transform;                 // a Motor6D's Transform: the Animator's pose, or a script's
        bool animatedByTrack = false;          // the Animator wrote it last, so it goes back to rest when the track ends
        double angle = 0;
    };
    void animate_rigs();                       // playing tracks -> the Motor6D Transforms of rigs that are not characters
    std::unordered_map<int64_t, Joint> joints_;
    std::unordered_map<int64_t, int64_t> weldRoot_;              // part -> its assembly's root (the root maps to itself)
    std::unordered_map<int64_t, Transform3D> weldOffset_;        // part -> where inside the root
    // A part that is not a character's own but is welded to one of its limbs -- armour a place
    // built round an invisible rig, a shield welded to a hand. It rides the limb as a worn
    // accessory does: this is the limb, and the part's place on it in the limb's own space.
    struct LimbWeld { int64_t limb = 0; Transform3D local; };
    std::unordered_map<int64_t, LimbWeld> limbWeld_;
    bool jointsDirty_ = false;
    void rebuild_assemblies();
    int64_t weld_root(int64_t id) const { auto it = weldRoot_.find(id); return it == weldRoot_.end() ? 0 : it->second; }
    bool effect_active(int64_t id) const;
    CanvasLayer* blurLayer_ = nullptr;                          // BlurEffect: the screen, re-sampled through its mipmaps
    Ref<ShaderMaterial> blurMaterial_;

    // default camera: orbits the local character's head
    Camera3D* camera_ = nullptr;
    int64_t cameraId_ = 0;                                     // the client's workspace.CurrentCamera
    int64_t cameraSubject_ = 0;                                // Camera.CameraSubject: a Humanoid or a part, else the local character
    godot::Vector3 camLastFocus_;                              // where the subject was last frame, for Track
    std::string cameraType_ = "Custom";
    double camYaw_ = 0, camPitch_ = 0.35, camDist_ = 12.5;
    bool camDragSunk_ = false;   // this right-drag began as input a script (or the GUI) took: the camera leaves it alone
    pulseblockz::rbx::Vec3 camSentPos, camSentOrient;
    Vector2 camSentViewport;                                   // Camera.ViewportSize, written when the viewport changes
};

} // namespace godot

VARIANT_ENUM_CAST(godot::PulseBlockzWorld::Mode);
