import Draw_Structs;
import Cross_Pipeline_Structs;
import RasterVertex;
import Util;

[shader("mesh")]
[NumThreads(g_PrimitivesPerMeshlet, 1, 1)]
[outputtopology("triangle")]
void msMain(
    uint localThreadIndex : SV_GroupIndex,
    uint groupIndex : SV_GroupID,
    in payload Payload payload,
    out vertices RasterVertex verts[g_VerticesPerMeshlet],
    out indices uint3 tris[g_PrimitivesPerMeshlet]
)
{
    if (payload.MeshletCount > groupIndex)
    {
        InstanceData *instanceDataBuffer = (InstanceData *)Constants.InstanceDataBufferPtr;
        InstanceData instanceData = instanceDataBuffer[payload.InstanceIndex];

        CameraData *camera = (CameraData *)Constants.CameraBufferPtr;

        Meshlet *meshletBuffer = (Meshlet *)payload.MeshletBufferPtr;
        Meshlet *meshlet = &meshletBuffer[payload.MeshletIndex[groupIndex]];
        const uint vertexCount = meshlet.VertexCount;
        const uint primitiveCount = meshlet.PrimitiveCount;
        SetMeshOutputCounts(vertexCount, primitiveCount);

        if (localThreadIndex < primitiveCount)
        {
            tris[localThreadIndex] = Unpack32To8(meshlet.Primitives[localThreadIndex]);
        }
    
        if (localThreadIndex < vertexCount)
        {
            MeshVertex *vertexBuffer = (MeshVertex *)payload.VertexBufferPtr;
            MeshVertex vertex = vertexBuffer[meshlet.VertexOffset + localThreadIndex];
            float4 positionWorld = mul(instanceData.WorldMatrix, float4(vertex.Position.xyz, 1.f));
            verts[localThreadIndex].WorldPosition = positionWorld.xyz;
            positionWorld = mul(camera.ViewProjectionMatrix, positionWorld);
            verts[localThreadIndex].Position = positionWorld;
            verts[localThreadIndex].Normal = normalize(mul(instanceData.WorldMatrix, float4(DecodeNormalOctahedral(UnpackOct16(Unpack32To16(vertex.Normal))), 0.f))).xyz;
            verts[localThreadIndex].UV = Unpack32ToHalfFloats(vertex.UV);
            verts[localThreadIndex].MaterialPtr = instanceData.MaterialPtr;
            verts[localThreadIndex].Meshletid = HashColor(groupIndex);
        }
    }
}