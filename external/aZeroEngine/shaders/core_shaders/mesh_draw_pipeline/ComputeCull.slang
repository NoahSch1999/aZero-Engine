import Cross_Pipeline_Structs;

[[vk::push_constant]]
ConstantBuffer<ComputeCullConstants> Constants;

[numthreads(g_ThreadCount, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x < Constants.InstanceCount)
    {
        InstanceCullData *instanceCullDataBuffer = (InstanceCullData *)Constants.InstanceCullDataBufferPtr;
        const InstanceCullData instanceCullData = instanceCullDataBuffer[dtid.x];
        CameraData *camera = (CameraData *)Constants.CameraBufferPtr;
        // if (camera.Frustum.Intersects(instanceCullData.WorldSpaceBounds, camera.ViewMatrix))
        {
            uint32_t *indirectArgumentCounterBuffer = (uint32_t *)Constants.IndirectArgumentCounterBufferPtr;
            MeshTaskIndirectArgs *indirectArgumentsBuffer = (MeshTaskIndirectArgs*)Constants.IndirectArgumentBufferPtr;

            uint meshInstanceIndex;
            // InterlockAdd the counter for the specific materialtype. The buffer value is initiated to the start offset of the materialtype.
            InterlockedAdd(indirectArgumentCounterBuffer[instanceCullData.MaterialTypeIndex], 1, meshInstanceIndex);
            indirectArgumentsBuffer[meshInstanceIndex].GroupX = (uint32_t)(ceil(instanceCullData.MeshletCount / (float)g_ThreadCount));
            indirectArgumentsBuffer[meshInstanceIndex].GroupY = 1;
            indirectArgumentsBuffer[meshInstanceIndex].GroupZ = 1;

            DrawMetaData *drawDataBuffer = (DrawMetaData *)Constants.DrawMetadataBufferPtr;
            drawDataBuffer[meshInstanceIndex].InstanceIndex = dtid.x;
            drawDataBuffer[meshInstanceIndex].MeshletCount = instanceCullData.MeshletCount;
        }
    }
}