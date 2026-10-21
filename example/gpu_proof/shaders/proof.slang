// M3 proof: a compute pass fills instances and visible indices, then an
// instanced draw reads them back through storage buffers.

[__AttributeUsage(_AttributeTargets.Var)]
struct ScopeAttribute {
    string name;
};

struct Instance {
    float2 position;
    float2 size;
    uint material;
    uint padding;
};

struct Material {
    float4 color;
};

struct Grid {
    uint columns;
    uint count;
    float cell;
    float margin;
};

struct View {
    float2 target_size;
};

[Scope("pass")] ConstantBuffer<Grid> grid;
[Scope("instance")] RWStructuredBuffer<Instance> out_instances;
[Scope("pass")] RWStructuredBuffer<uint> out_visible;

[Scope("view")] ConstantBuffer<View> view;
[Scope("instance")] StructuredBuffer<Instance> instances;
[Scope("material")] StructuredBuffer<Material> materials;

[shader("compute")]
[numthreads(64, 1, 1)]
void build(uint3 id: SV_DispatchThreadID) {
    uint i = id.x;
    if (i >= grid.count)
        return;
    Instance instance;
    instance.position = float2(i % grid.columns, i / grid.columns) * grid.cell + grid.margin;
    instance.size = grid.cell - 2.0 * grid.margin;
    instance.material = (i / grid.columns) % 2;
    instance.padding = 0;
    out_instances[i] = instance;
    if (i % 2 == 0)
        out_visible[i / 2] = i;
}

struct VertexOutput {
    float4 position: SV_Position;
    nointerpolation uint material: MATERIAL;
};

[shader("vertex")]
VertexOutput vertex_main(float2 corner: CORNER, uint visible: VISIBLE) {
    Instance instance = instances[visible];
    float2 pixel = instance.position + corner * instance.size;
    float2 ndc = pixel / view.target_size * float2(2.0, -2.0) + float2(-1.0, 1.0);
    VertexOutput output;
    output.position = float4(ndc, 0.0, 1.0);
    output.material = instance.material;
    return output;
}

[shader("fragment")]
float4 fragment_main(VertexOutput input) : SV_Target {
    return materials[input.material].color;
}
