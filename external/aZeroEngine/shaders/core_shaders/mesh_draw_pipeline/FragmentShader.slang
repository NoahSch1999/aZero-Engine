import IMaterial;
import RasterVertex;
import Lights;
import Draw_Structs;

struct Output
{
    float4 output : SV_TARGET0;
};

[shader("fragment")]
Output fsMain<T : IMaterial>(RasterVertex input)
{
    Output outputStruct;
    outputStruct.output = float4(0, 0, 0, 1);

    T material;
    material.PreLightCalc(input.MaterialPtr, input);
    outputStruct.output = material.CalcPointLights(outputStruct.output, input.MaterialPtr, input, Constants.PointLightsPtr, Constants.NumPointLights);
    outputStruct.output = material.CalcSpotLights(outputStruct.output, input.MaterialPtr, input, Constants.SpotLightsPtr, Constants.NumSpotLights);
    outputStruct.output = material.CalcDirectionalLights(outputStruct.output, input.MaterialPtr, input, Constants.DirectionalLightsPtr, Constants.NumDirectionalLights);

    return outputStruct;
}