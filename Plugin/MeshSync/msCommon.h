#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include "MeshUtils/RawVector.h"
#include "MeshUtils/MeshUtils.h"

#ifdef GetMessage
    #undef GetMessage
#endif


namespace ms {

void LogImpl(const char *fmt, ...);
#define msLogInfo(...)    ::ms::LogImpl("MeshSync info: " __VA_ARGS__)
#define msLogWarning(...) ::ms::LogImpl("MeshSync warning: " __VA_ARGS__)
#define msLogError(...)   ::ms::LogImpl("MeshSync error: " __VA_ARGS__)


class SceneEntity
{
public:
    int id = 0;
    std::string path;

    uint32_t getSerializeSize() const;
    void serialize(std::ostream& os) const;
    void deserialize(std::istream& is);
};


struct TRS
{
    float3   position = float3::zero();
    quatf    rotation = quatf::identity();
    float3   rotation_eularZXY = float3::zero();
    float3   scale = float3::one();
};


class Transform : public SceneEntity
{
using super = SceneEntity;
public:
    TRS transform;

    uint32_t getSerializeSize() const;
    void serialize(std::ostream& os) const;
    void deserialize(std::istream& is);
};
using TransformPtr = std::shared_ptr<Transform>;


class Camera : public Transform
{
using super = Transform;
public:
    float fov = 30.0f;

    uint32_t getSerializeSize() const;
    void serialize(std::ostream& os) const;
    void deserialize(std::istream& is);
};
using CameraPtr = std::shared_ptr<Camera>;


// Mesh

struct MeshDataFlags
{
    uint32_t visible : 1;
    uint32_t has_refine_settings : 1;
    uint32_t has_indices : 1;
    uint32_t has_counts : 1;
    uint32_t has_points : 1;
    uint32_t has_normals : 1;
    uint32_t has_tangents : 1;
    uint32_t has_uv : 1;
    uint32_t has_materialIDs : 1;
    uint32_t has_bones : 1;
};

struct MeshRefineFlags
{
    uint32_t split : 1;
    uint32_t triangulate : 1;
    uint32_t optimize_topology : 1;
    uint32_t swap_handedness : 1;
    uint32_t swap_faces : 1;
    uint32_t gen_normals : 1;
    uint32_t gen_normals_with_smooth_angle : 1;
    uint32_t gen_tangents : 1;
    uint32_t apply_local2world : 1;
    uint32_t apply_world2local : 1;
    uint32_t bake_skin : 1;

    // Metasequoia - equivalent
    uint32_t invert_v : 1;
    uint32_t mirror_x : 1;
    uint32_t mirror_y : 1;
    uint32_t mirror_z : 1;
};

struct MeshRefineSettings
{
    MeshRefineFlags flags = { 0 };
    float scale_factor = 1.0f;
    float smooth_angle = 0.0f;
    int split_unit = 65000;
    float4x4 local2world = float4x4::identity();
    float4x4 world2local = float4x4::identity();
};

template<int N>
struct Weights
{
    float   weight[N] = {};
    int     indices[N] = {};
};
using Weights4 = Weights<4>;

struct SubmeshData
{
    IArray<int> indices;
    int materialID = 0;
};

struct SplitData
{
    IArray<float3> points;
    IArray<float3> normals;
    IArray<float4> tangents;
    IArray<float2> uv;
    IArray<int> indices;
    IArray<SubmeshData> submeshes;
};

class Mesh : public Transform
{
using super = Transform;
public:
    MeshDataFlags     flags = { 0 };
    MeshRefineSettings refine_settings;

    RawVector<float3> points;
    RawVector<float3> normals;
    RawVector<float4> tangents;
    RawVector<float2> uv;
    RawVector<int>    counts;
    RawVector<int>    indices;
    RawVector<int>    materialIDs;

    // bone data
    int bones_par_vertex = 0;
    RawVector<float> bone_weights;
    RawVector<int> bone_indices;
    std::vector<std::string> bones;
    RawVector<float4x4> bindposes;

    // not serialized
    RawVector<SubmeshData> submeshes;
    RawVector<SplitData> splits;
    RawVector<Weights4> weights4;

public:
    Mesh();
    void clear();
    uint32_t getSerializeSize() const;
    void serialize(std::ostream& os) const;
    void deserialize(std::istream& is);

    const char* getName() const;
    void refine();
    void applyMirror(const float3& plane_n, float plane_d);
    void applyTransform(const float4x4& t);
};
using MeshPtr = std::shared_ptr<Mesh>;


struct Scene
{
public:
    std::vector<MeshPtr> meshes;
    std::vector<TransformPtr> transforms;
    std::vector<CameraPtr> cameras;

public:
    uint32_t getSerializeSize() const;
    void serialize(std::ostream& os) const;
    void deserialize(std::istream& is);
};
using ScenePtr = std::shared_ptr<Scene>;




enum class MessageType
{
    Unknown,
    Get,
    Post,
    Delete,
    Screenshot,
};

enum class SenderType
{
    Unknown,
    Unity,
    Metasequoia,
};



class Message
{
public:
    virtual ~Message();
    virtual uint32_t getSerializeSize() const = 0;
    virtual void serialize(std::ostream& os) const = 0;
    virtual void deserialize(std::istream& is) = 0;
};


struct GetFlags
{
    uint32_t get_transform : 1;
    uint32_t get_points : 1;
    uint32_t get_normals : 1;
    uint32_t get_tangents : 1;
    uint32_t get_uv : 1;
    uint32_t get_indices : 1;
    uint32_t get_materialIDs : 1;
    uint32_t get_bones : 1;
    uint32_t apply_culling : 1;
};


class GetMessage : public Message
{
public:
    GetFlags flags = {0};
    MeshRefineSettings refine_settings;

    // non-serializable
    std::shared_ptr<std::atomic_int> wait_flag;

public:
    GetMessage();
    uint32_t getSerializeSize() const override;
    void serialize(std::ostream& os) const override;
    void deserialize(std::istream& is) override;
};


class SetMessage : public Message
{
public:
    Scene scene;

public:
    SetMessage();
    uint32_t getSerializeSize() const override;
    void serialize(std::ostream& os) const override;
    void deserialize(std::istream& is) override;
};


class DeleteMessage : public Message
{
public:
    struct Identifier
    {
        std::string path;
        int id;
    };
    std::vector<Identifier> targets;

    DeleteMessage();
    uint32_t getSerializeSize() const override;
    void serialize(std::ostream& os) const override;
    void deserialize(std::istream& is) override;
};


class ScreenshotMessage : public Message
{
public:

    // non-serializable
    std::shared_ptr<std::atomic_int> wait_flag;

public:
    ScreenshotMessage();
    uint32_t getSerializeSize() const override;
    void serialize(std::ostream& os) const override;
    void deserialize(std::istream& is) override;
};



} // namespace ms
