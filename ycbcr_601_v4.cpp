// SPDX-FileCopyrightText: 2022 Amyspark <amy@amyspark.me>
// SPDX-License-Identifier: BSD-3-Clause

#include <lcms2.h>

#include <array>
#include <iostream>

#include "version.h"

#define RESOLUTION 24

// Source: Tooms (2015), table 11.1, p.192
constexpr cmsCIExyY d65 = {0.3127, 0.3290, 1.0};

#if !defined BT1886
// Inverse OETF curve
//
// Source: basic algebra on ITU-R BT.601-7, ss. 2.6.4
constexpr std::array<cmsFloat64Number, 5> rec601ParametersInv = {1.0 / 0.45, 1.0 / 1.099, 0.099 / 1.099, 1.0 / 4.5, 0.081};

// OETF curve
//
// Source: ITU-R BT.601-7, ss. 2.6.4
const std::array<cmsFloat64Number, 7> rec601Parameters = {0.45, 1.099, 0, 4.5, 0.018, -0.099, 0};
#endif

void log(cmsContext ctx, unsigned int errorCode, const char *msg)
{
    std::cerr << "context " << ctx << " error: " << errorCode << " (" << msg << ")" << std::endl;
}

cmsInt32Number sample(const cmsUInt16Number In[], cmsUInt16Number Out[], void *cargo)
{
    cmsPipelineEval16(In, Out, reinterpret_cast<cmsPipeline *>(cargo));
    return TRUE;
}

// From the V4 profile above EXTRACT:
// - cmsSigMediaWhitePointTag
// - any of the cmsSigRedTRCTag, cmsSigGreenTRCTag, cmsSigBlueTRCTag
// - cmsSigChromaticAdaptationTag
// and use with the YCbCr profile
cmsHPROFILE createBaseRec709Profile(cmsContext ctx)
{
    // Elle Stone's prequantized sRGB primaries
    // Match: Tooms (2015), table 19.1
    const cmsCIExyYTRIPLE sRGBPrimariesPreQuantized = {{0.639998686, 0.330010138, 1.0}, {0.300003784, 0.600003357, 1.0}, {0.150002046, 0.059997204, 1.0}};

#if defined BT1886
    auto toneCurveInv = cmsBuildGamma(ctx, 2.4);
#else
    auto toneCurveInv = cmsBuildParametricToneCurve(ctx, 4, rec601ParametersInv.data());
#endif
    const std::array<cmsToneCurve *, T_CHANNELS(TYPE_RGB_16)> curves = {toneCurveInv, toneCurveInv, toneCurveInv};

    return cmsCreateRGBProfileTHR(ctx, &d65, &sRGBPrimariesPreQuantized, curves.data());
}

void setupMetadata(cmsContext ctx, cmsHPROFILE profile)
{
    std::string version{COMMIT};

    auto copyright = cmsMLUalloc(ctx, 1);
    cmsMLUsetASCII(copyright,
                   "en",
                   "US",
                   "(C) 2022 Amyspark <amy@amyspark.me>. This work is licensed under the Creative Commons Attribution-ShareAlike 4.0 International License. To "
                   "view a copy of this license, visit <http://creativecommons.org/licenses/by-sa/4.0/>.");
    cmsWriteTag(profile, cmsSigCopyrightTag, copyright);

    auto description = cmsMLUalloc(ctx, 1);
#if defined BT1886
    cmsMLUsetASCII(description, "en", "US", "ITU-R BT.601-7 + BT.1886 YCbCr ICC V4 profile");
#else
    cmsMLUsetASCII(description, "en", "US", "ITU-R BT.601-7 YCbCr ICC V4 profile");
#endif
    cmsWriteTag(profile, cmsSigProfileDescriptionTag, description);
    auto MfgDesc = cmsMLUalloc(ctx, 1);
    cmsMLUsetASCII(MfgDesc, "en", "US", "Amyspark");
    cmsWriteTag(profile, cmsSigDeviceMfgDescTag, MfgDesc);
    auto ModelDesc = cmsMLUalloc(ctx, 1);
    cmsMLUsetASCII(ModelDesc, "en", "US", version.c_str());
    cmsWriteTag(profile, cmsSigDeviceModelDescTag, ModelDesc);
    cmsSetHeaderManufacturer(profile, 0x494E544C);
    cmsSetHeaderModel(profile, 0x494E544C);
}

int main()
{
    cmsSetLogErrorHandlerTHR(nullptr, log);
    auto ctx = cmsCreateContext(nullptr, nullptr);

    auto baseProfile = createBaseRec709Profile(ctx);
    // cmsSaveProfileToFile(baseProfile, "srgb.icc");

    auto yCbrProfile = cmsCreateLab4Profile(&d65);
    setupMetadata(ctx, yCbrProfile);

    // Strict transformation between YCbCr and XYZ
#if defined BT1886
    cmsSetDeviceClass(yCbrProfile, cmsSigDisplayClass);
#else
    cmsSetDeviceClass(yCbrProfile, cmsSigColorSpaceClass);
#endif
    cmsSetColorSpace(yCbrProfile, cmsSigYCbCrData);
    cmsSetPCS(yCbrProfile, cmsSigXYZData);
    cmsSetHeaderRenderingIntent(yCbrProfile, INTENT_PERCEPTUAL);

    // The YCbCr -> XYZ conversion goes as follows:
    auto yCbrPipeline = cmsPipelineAlloc(ctx, T_CHANNELS(TYPE_YCbCr_16), T_CHANNELS(TYPE_XYZ_16));
    // 0. Dummy curves for the "gamma"-corrected YCbCr.
    auto pipeline1_M = cmsStageAllocToneCurves(ctx, T_CHANNELS(TYPE_YCbCr_16), nullptr);
    // 1. Chrominance channels are [-0.5, 0.5]. Adjust.
    // The offset is pre-applied before the transform.
    const std::array<double, T_CHANNELS(TYPE_YCbCr_16) * T_CHANNELS(TYPE_YCbCr_16)> identity = {{1, 0, 0, 0, 1, 0, 0, 0, 1}};
    const std::array<double, T_CHANNELS(TYPE_YCbCr_16)> offset_ycbcr_to_rgb = {0, -0.5, -0.5};
    auto yCbrOffset = cmsStageAllocMatrix(ctx, T_CHANNELS(TYPE_YCbCr_16), T_CHANNELS(TYPE_YCbCr_16), identity.data(), offset_ycbcr_to_rgb.data());
    // 2. YCbCr -> normalized R'G'B. Source: Wolfram Alpha, inverted matrix.
    const std::array<double, T_CHANNELS(TYPE_YCbCr_16) * T_CHANNELS(TYPE_RGB_16)> ycbcr_to_rgb = {
        {1, 1.402, 0, 1, -0.714136, -0.344136, 1., 4.93315e-17, 1.772}};
    auto yCbrMatrix = cmsStageAllocMatrix(ctx, T_CHANNELS(TYPE_YCbCr_16), T_CHANNELS(TYPE_RGB_16), ycbcr_to_rgb.data(), nullptr);
    // 2. Normalized R'G'B -> linear RGB. Source: ITU-R BT.601-7 ss. 2.6.4
    auto trc = reinterpret_cast<cmsToneCurve *>(cmsReadTag(baseProfile, cmsSigRedTRCTag));
    const std::array<cmsToneCurve *, T_CHANNELS(TYPE_RGB_16)> gamma = {trc, trc, trc};
    auto pipeline1_B = cmsStageAllocToneCurves(ctx, T_CHANNELS(TYPE_RGB_16), gamma.data());
    // 3. Linear RGB -> XYZ.
    // Source: <https://photosauce.net/blog/post/making-a-minimal-srgb-icc-profile-part-3-choose-your-colors-carefully>
    // NOTE: these must be computed under D65!!!
    const std::array<double, T_CHANNELS(TYPE_RGB_16) * T_CHANNELS(TYPE_XYZ_16)> rgb_to_xyz = {
        {0.4124, 0.3576, 0.1805, 0.2126, 0.7152, 0.0722, 0.0193, 0.1192, 0.9505}};
    auto pipeline1_C = cmsStageAllocMatrix(ctx, T_CHANNELS(TYPE_RGB_16), T_CHANNELS(TYPE_XYZ_16), rgb_to_xyz.data(), nullptr);

    // Assemble the YCbCr -> R'G'B' pipeline.
    cmsPipelineInsertStage(yCbrPipeline, cmsAT_END, yCbrOffset);
    cmsPipelineInsertStage(yCbrPipeline, cmsAT_END, yCbrMatrix);

    // The CLUT is needed because AtoB0 can't pack the matrices.
    auto lut1 = cmsStageAllocCLut16bit(ctx, RESOLUTION, T_CHANNELS(TYPE_YCbCr_16), T_CHANNELS(TYPE_RGB_16), nullptr);
    cmsStageSampleCLut16bit(lut1, &sample, yCbrPipeline, 0);

    // This LUT is then saved to the profile
    // ICC 4.3 requires two dummy M and B curves
    auto p = cmsPipelineAlloc(ctx, T_CHANNELS(TYPE_YCbCr_16), T_CHANNELS(TYPE_XYZ_16));
    cmsPipelineInsertStage(p, cmsAT_END, pipeline1_M);              // A = dummy curves
    cmsPipelineInsertStage(p, cmsAT_END, lut1);                     // CLUT = YCbr -> R'G'B
    cmsPipelineInsertStage(p, cmsAT_END, pipeline1_B);              // M = OETF
    cmsPipelineInsertStage(p, cmsAT_END, pipeline1_C);              // Matrix = RGB -> XYZ
    cmsPipelineInsertStage(p, cmsAT_END, cmsStageDup(pipeline1_M)); // B = dummy curves
    cmsWriteTag(yCbrProfile, cmsSigAToB0Tag, p);

    // Add DtoB0 tag as requested by Wolthera.
#if defined BT1886
    const std::array<cmsFloat64Number, 4> trcParameters = {2.4, 1, 0, 0};
    auto *trcD = cmsBuildParametricToneCurve(ctx, 6, trcParameters.data());
#else
    // The Rec.601 parametric curve is incompatible with the available
    // shapes, it must be sampled. (This is the same workaround as in v2.)
    auto trcD = [ctx, trc]() {
        std::array<cmsFloat32Number, 1024> test{};
        for (auto i = 0; i < 1024; i++) {
            cmsFloat32Number x = 1.0f / 1024.0f * (cmsFloat32Number)i;
            test[i] = cmsEvalToneCurveFloat(trc, x);
        }
        return cmsBuildTabulatedToneCurveFloat(ctx, test.size(), test.data());
    }();
#endif
    const std::array<cmsToneCurve *, T_CHANNELS(TYPE_RGB_16)> gammaClut = {trcD, trcD, trcD};
    auto *pipelineD1_B = cmsStageAllocToneCurves(ctx, T_CHANNELS(TYPE_RGB_16), gammaClut.data());
    auto d2b0 = cmsPipelineAlloc(ctx, T_CHANNELS(TYPE_YCbCr_16), T_CHANNELS(TYPE_XYZ_16));
    cmsPipelineInsertStage(d2b0, cmsAT_END, cmsStageDup(yCbrOffset));
    cmsPipelineInsertStage(d2b0, cmsAT_END, cmsStageDup(yCbrMatrix));
    cmsPipelineInsertStage(d2b0, cmsAT_END, cmsStageDup(pipelineD1_B)); // M = OETF
    cmsPipelineInsertStage(d2b0, cmsAT_END, cmsStageDup(pipeline1_C));  // Matrix = RGB -> XYZ
    cmsWriteTag(yCbrProfile, cmsSigDToB0Tag, d2b0);

    // The XYZ -> YCbCr conversion goes as follows:
    auto yCbrPipeline2 = cmsPipelineAlloc(ctx, T_CHANNELS(TYPE_XYZ_16), T_CHANNELS(TYPE_YCbCr_16));
    // 0. Dummy curves for the gamma-uncorrected YCbCr.
    auto pipeline2_M = cmsStageAllocToneCurves(ctx, T_CHANNELS(TYPE_RGB_16), nullptr);
    // 1. XYZ -> Linear RGB.
    // Source: <https://photosauce.net/blog/post/making-a-minimal-srgb-icc-profile-part-3-choose-your-colors-carefully>
    const std::array<double, T_CHANNELS(TYPE_XYZ_16) * T_CHANNELS(TYPE_RGB_16)> xyz_to_rgb = {
        {3.2406, -1.5372, -0.4986, -0.9689, 1.8758, 0.0415, 0.0557, -0.2040, 1.0570}};
    auto pipeline2_C = cmsStageAllocMatrix(ctx, T_CHANNELS(TYPE_XYZ_16), T_CHANNELS(TYPE_RGB_16), xyz_to_rgb.data(), nullptr);
#if defined BT1886
    // 2. Linear RGB -> Normalized R'G'B. Source: ITU-R BT.1886
    auto trcI = cmsBuildGamma(ctx, 1.0 / 2.4);
#else
    // 2. Linear RGB -> Normalized R'G'B. Source: ITU-R BT.601-7, ss. 2.6.4
    auto trcI = cmsBuildParametricToneCurve(ctx, 5, rec601Parameters.data());
#endif
    const std::array<cmsToneCurve *, T_CHANNELS(TYPE_RGB_16)> gamma_i = {trcI, trcI, trcI};
    auto pipeline2_B = cmsStageAllocToneCurves(ctx, T_CHANNELS(TYPE_RGB_16), gamma_i.data());
    // 3. Normalized R'G'B -> YCbCr. Source: ITU-R BT.601-7, ss. 2.5.1
    // XXX: nudge these with xicclu?
    const std::array<double, T_CHANNELS(TYPE_RGB_16) * T_CHANNELS(TYPE_YCbCr_16)> rgb_to_ycbcr = {
        {0.299, 0.587, 0.114, 0.701 / 1.402, -0.587 / 1.402, -0.114 / 1.402, -0.299 / 1.772, -0.587 / 1.772, 0.886 / 1.772}};
    // 4. Chrominance channels are [-0.5, 0.5]. Adjust.
    // The offset is applied after the transform, so no additional matrix is
    // needed.
    const std::array<double, T_CHANNELS(TYPE_YCbCr_16)> offset_i = {0, 0.5, 0.5};
    auto pipeline2_Matrix = cmsStageAllocMatrix(ctx, T_CHANNELS(TYPE_RGB_16), T_CHANNELS(TYPE_YCbCr_16), rgb_to_ycbcr.data(), offset_i.data());

    cmsPipelineInsertStage(yCbrPipeline2, cmsAT_END, pipeline2_Matrix);

    auto lut2 = cmsStageAllocCLut16bit(ctx, RESOLUTION, T_CHANNELS(TYPE_RGB_16), T_CHANNELS(TYPE_YCbCr_16), nullptr);
    cmsStageSampleCLut16bit(lut2, &sample, yCbrPipeline2, 0);

    auto p2 = cmsPipelineAlloc(ctx, T_CHANNELS(TYPE_XYZ_16), T_CHANNELS(TYPE_YCbCr_16));
    cmsPipelineInsertStage(p2, cmsAT_END, pipeline2_M);              // B = dummy
    cmsPipelineInsertStage(p2, cmsAT_END, pipeline2_C);              // Matrix = XYZ -> RGB
    cmsPipelineInsertStage(p2, cmsAT_END, pipeline2_B);              // M = OETF^-1
    cmsPipelineInsertStage(p2, cmsAT_END, lut2);                     // CLUT = R'G'B' -> YCbr
    cmsPipelineInsertStage(p2, cmsAT_END, cmsStageDup(pipeline2_M)); // A = dummy
    cmsWriteTag(yCbrProfile, cmsSigBToA0Tag, p2);

    // Add BtoD0 tag as requested by Wolthera.
#if defined BT1886
    const std::array<cmsFloat64Number, 4> trcIParameters = {1.0 / 2.4, 1, 0, 0};
    auto *trcID = cmsBuildParametricToneCurve(ctx, 6, trcIParameters.data());
#else
    // The Rec.601 parametric curve is incompatible with the available
    // shapes, it must be sampled. (This is the same workaround as in v2.)
    auto trcID = [ctx, trcI]() {
        std::array<cmsFloat32Number, 1024> test{};
        for (auto i = 0; i < 1024; i++) {
            cmsFloat32Number x = 1.0f / 1024.0f * (cmsFloat32Number)i;
            test[i] = cmsEvalToneCurveFloat(trcI, x);
        }
        return cmsBuildTabulatedToneCurveFloat(ctx, test.size(), test.data());
    }();
#endif
    const std::array<cmsToneCurve *, T_CHANNELS(TYPE_RGB_16)> gammaIClut = {trcID, trcID, trcID};
    auto *pipeline2_B_Clut = cmsStageAllocToneCurves(ctx, T_CHANNELS(TYPE_RGB_16), gammaIClut.data());
    auto b2d0 = cmsPipelineAlloc(ctx, T_CHANNELS(TYPE_YCbCr_16), T_CHANNELS(TYPE_XYZ_16));
    cmsPipelineInsertStage(b2d0, cmsAT_END, cmsStageDup(pipeline2_C));      // Matrix = XYZ -> RGB
    cmsPipelineInsertStage(b2d0, cmsAT_END, cmsStageDup(pipeline2_B_Clut)); // M = OETF^-1
    cmsPipelineInsertStage(b2d0, cmsAT_END, cmsStageDup(pipeline2_Matrix)); // CLUT = R'G'B' -> YCbr
    cmsWriteTag(yCbrProfile, cmsSigBToD0Tag, b2d0);

    // Bradford transform from D65 (BT.601-7) to D50 (ICC 4.3)
    // Source: Elle Stone's well behaved sRGB profile
    // Thanks to Doug Walker from ILM for pointing it out.
    auto bradford = cmsReadTag(baseProfile, cmsSigChromaticAdaptationTag);
    cmsWriteTag(yCbrProfile, cmsSigChromaticAdaptationTag, bradford);

    if (!cmsMD5computeID(yCbrProfile)) {
        std::cerr << "Failed MD5 computation" << std::endl;
        return -1;
    }
#if defined BT1886
    const std::string profileName{"bt601-7_bt1886_ycbcr_v4.icc"};
#else
    const std::string profileName{"bt601-7_ycbcr_v4.icc"};
#endif
    if (!cmsSaveProfileToFile(yCbrProfile, profileName.c_str())) {
        std::cerr << "CANNOT WRITE PROFILE" << std::endl;
        return -2;
    }
}
