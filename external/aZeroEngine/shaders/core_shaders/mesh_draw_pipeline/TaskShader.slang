import Draw_Structs;
import Cross_Pipeline_Structs;
import Volumes;

groupshared Payload payload;

[shader("task")]
[NumThreads(g_ThreadCount, 1, 1)]
void asMain(uint3 dtid: SV_DispatchThreadID, uint3 gid: SV_GroupThreadID, uint32_t drawIndex : SV_DrawIndex)
{
    DrawMetaData *drawDataBuffer = (DrawMetaData *)Constants.DrawMetadataBufferPtr;
    DrawMetaData drawData = drawDataBuffer[drawIndex];

    bool visible = false;

    if (dtid.x < drawData.MeshletCount)
    {
        CameraData *camera = (CameraData *)Constants.CameraBufferPtr;
        InstanceData *instanceData = (InstanceData*)Constants.InstanceDataBufferPtr;
        BoundingSphere* meshletBounds = (BoundingSphere *)instanceData.MeshletBoundsBDA;

        // visible = camera.Frustum.Intersects( CreateBoundingSphere(mul(instanceData[drawIndex].WorldMatrix, float4(meshletBounds[dtid.x].Position, 1.f)).xyz, meshletBounds[dtid.x].Radius), camera->ViewMatrix);
        visible = true;
        if (visible)
        {
            uint index = WavePrefixCountBits(visible);

            payload.MeshletIndex[index] = dtid.x;
            payload.InstanceIndex = drawData.InstanceIndex;
            payload.MeshletCount = drawData.MeshletCount;
        }
    }
    
    DispatchMesh(WaveActiveCountBits(visible), 1, 1, payload);
}