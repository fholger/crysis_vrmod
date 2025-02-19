﻿#include "StdAfx.h"
#include "HandPoses.h"

#include "Cry_Quat.h"
#include "ICryAnimation.h"

namespace
{
    struct JointData
    {
        const char* name;
        QuatT pose;
    };

    const int NUM_FINGER_JOINTS = 20;
    
    JointData openHandLeft[NUM_FINGER_JOINTS] = {
        { "thumb_L01", QuatT(Vec3(0.029541f, -0.030269f, -0.023727f), Quat(0.938655f, 0.199065f, 0.148439f, -0.239304f)) },
        { "thumb_L02", QuatT(Vec3(0.052911f, -0.000002f, -0.000000f), Quat(0.997893f, 0.000014f, 0.000014f, 0.064883f)) },
        { "thumb_L03", QuatT(Vec3(0.036524f, -0.000001f, 0.000000f), Quat(0.990752f, 0.031866f, -0.032225f, 0.127895f)) },
        { "thumb_L04", QuatT(Vec3(0.031123f, 0.000000f, 0.000000f), Quat(0.999988f, -0.000000f, 0.000000f, 0.004857f)) },
        { "indexfinger_L01", QuatT(Vec3(0.091930f, -0.034366f, 0.016744f), Quat(0.994423f, 0.029665f, 0.085642f, -0.053935f)) },
        { "indexfinger_L02", QuatT(Vec3(0.049201f, 0.000015f, 0.002123f), Quat(0.993991f, -0.027823f, 0.103726f, 0.021163f)) },
        { "indexfinger_L03", QuatT(Vec3(0.025985f, 0.000012f, 0.001577f), Quat(0.995270f, -0.004258f, 0.086549f, 0.043907f)) },
        { "indexfinger_L04", QuatT(Vec3(0.022366f, 0.000000f, 0.000000f), Quat(0.999293f, -0.000001f, 0.000000f, -0.037608f)) },
        { "middlefinger_L01", QuatT(Vec3(0.094119f, -0.011077f, 0.020161f), Quat(0.997243f, -0.031794f, 0.064926f, 0.016717f)) },
        { "middlefinger_L02", QuatT(Vec3(0.051566f, 0.000005f, 0.002341f), Quat(0.974386f, -0.003481f, 0.224227f, 0.016803f)) },
        { "middlefinger_L03", QuatT(Vec3(0.026943f, -0.000000f, 0.001719f), Quat(0.997647f, 0.000403f, 0.068120f, -0.007711f)) },
        { "middlefinger_L04", QuatT(Vec3(0.021961f, 0.000000f, 0.000000f), Quat(0.999904f, 0.000319f, 0.013551f, -0.003027f)) },
        { "ringfinger_L01", QuatT(Vec3(0.093776f, 0.013765f, 0.019229f), Quat(0.989230f, -0.062307f, 0.125522f, 0.042267f)) },
        { "ringfinger_L02", QuatT(Vec3(0.048213f, 0.000006f, 0.002290f), Quat(0.957488f, 0.000144f, 0.288448f, -0.003740f)) },
        { "ringfinger_L03", QuatT(Vec3(0.026269f, -0.000002f, 0.001688f), Quat(0.999344f, 0.003295f, 0.032298f, 0.016027f)) },
        { "ringfinger_L04", QuatT(Vec3(0.019120f, 0.000000f, -0.000000f), Quat(0.999102f, -0.001710f, -0.039147f, -0.016116f)) },
        { "pinky_L01", QuatT(Vec3(0.090513f, 0.035393f, 0.008370f), Quat(0.971568f, -0.111682f, 0.181024f, 0.103985f)) },
        { "pinky_L02", QuatT(Vec3(0.045267f, 0.000006f, 0.002150f), Quat(0.975746f, 0.003769f, 0.218357f, -0.015005f)) },
        { "pinky_L03", QuatT(Vec3(0.024558f, -0.000002f, 0.001578f), Quat(0.992004f, 0.000705f, 0.125176f, -0.016084f)) },
        { "pinky_L04", QuatT(Vec3(0.018585f, 0.000000f, -0.000000f), Quat(0.999986f, -0.000001f, -0.000000f, -0.005335f)) },
    };

    JointData openHandRight[NUM_FINGER_JOINTS] = {
        { "thumb_R01", QuatT(Vec3(0.027691f, -0.031355f, 0.024529f), Quat(0.956204f, 0.040455f, -0.072277f, -0.280737f)) },
        { "thumb_R02", QuatT(Vec3(0.052911f, -0.000002f, -0.000000f), Quat(0.999917f, 0.000014f, 0.000014f, 0.012876f)) },
        { "thumb_R03", QuatT(Vec3(0.036524f, -0.000001f, 0.000000f), Quat(0.996249f, -0.014789f, 0.070839f, 0.047446f)) },
        { "thumb_R04", QuatT(Vec3(0.031123f, -0.000000f, -0.000000f), Quat(0.999988f, 0.000000f, 0.000000f, 0.004857f)) },
        { "indexfinger_R01", QuatT(Vec3(0.090690f, -0.038410f, -0.014565f), Quat(0.994125f, -0.035765f, -0.032916f, -0.096705f)) },
        { "indexfinger_R02", QuatT(Vec3(0.049201f, 0.000015f, -0.002123f), Quat(0.996071f, 0.003381f, -0.070896f, 0.052971f)) },
        { "indexfinger_R03", QuatT(Vec3(0.025985f, 0.000012f, -0.001577f), Quat(0.988659f, 0.006747f, -0.143533f, 0.043648f)) },
        { "indexfinger_R04", QuatT(Vec3(0.022366f, -0.000000f, -0.000000f), Quat(0.999293f, 0.000001f, -0.000001f, -0.037608f)) },
        { "middlefinger_R01", QuatT(Vec3(0.093959f, -0.015260f, -0.018067f), Quat(0.998930f, 0.023277f, -0.038872f, -0.009308f)) },
        { "middlefinger_R02", QuatT(Vec3(0.051566f, 0.000005f, -0.002341f), Quat(0.971080f, 0.003726f, -0.238138f, 0.016717f)) },
        { "middlefinger_R03", QuatT(Vec3(0.026943f, -0.000000f, -0.001719f), Quat(0.999927f, 0.000058f, -0.009265f, -0.007754f)) },
        { "middlefinger_R04", QuatT(Vec3(0.021961f, 0.000000f, 0.000000f), Quat(0.999904f, -0.000319f, -0.013551f, -0.003028f)) },
        { "ringfinger_R01", QuatT(Vec3(0.094673f, 0.009579f, -0.017284f), Quat(0.981419f, 0.078694f, -0.174867f, 0.006704f)) },
        { "ringfinger_R02", QuatT(Vec3(0.048213f, 0.000006f, -0.002290f), Quat(0.987154f, 0.000446f, -0.159718f, -0.004043f)) },
        { "ringfinger_R03", QuatT(Vec3(0.026269f, -0.000002f, -0.001688f), Quat(0.996793f, -0.002532f, -0.078406f, 0.015811f)) },
        { "ringfinger_R04", QuatT(Vec3(0.019120f, -0.000000f, -0.000000f), Quat(0.999102f, 0.001710f, 0.039147f, -0.016117f)) },
        { "pinky_R01", QuatT(Vec3(0.092119f, 0.031399f, -0.006621f), Quat(0.952899f, 0.218789f, -0.192433f, 0.084175f)) },
        { "pinky_R02", QuatT(Vec3(0.045267f, 0.000006f, -0.002150f), Quat(0.950745f, -0.006287f, -0.309610f, -0.013667f)) },
        { "pinky_R03", QuatT(Vec3(0.024558f, -0.000002f, -0.001578f), Quat(0.979942f, -0.001928f, -0.198648f, -0.015782f)) },
        { "pinky_R04", QuatT(Vec3(0.018585f, 0.000000f, 0.000000f), Quat(0.999986f, 0.000001f, 0.000000f, -0.005335f)) },
    };

    JointData closedHandLeft[NUM_FINGER_JOINTS] = {
        { "thumb_L01", QuatT(Vec3(0.029541f, -0.030269f, -0.023727f), Quat(0.938655f, 0.199065f, 0.148439f, -0.239304f)) },
        { "thumb_L02", QuatT(Vec3(0.052911f, -0.000002f, -0.000000f), Quat(0.967856f, 0.000014f, 0.000014f, 0.251503f)) },
        { "thumb_L03", QuatT(Vec3(0.036524f, -0.000001f, 0.000000f), Quat(0.901660f, -0.038699f, -0.061487f, 0.426298f)) },
        { "thumb_L04", QuatT(Vec3(0.031123f, 0.000000f, 0.000000f), Quat(0.999988f, -0.000000f, 0.000000f, 0.004857f)) },
        { "indexfinger_L01", QuatT(Vec3(0.091930f, -0.034366f, 0.016744f), Quat(0.838306f, 0.014430f, 0.544468f, -0.024284f)) },
        { "indexfinger_L02", QuatT(Vec3(0.049201f, 0.000015f, 0.002123f), Quat(0.611236f, -0.032441f, 0.790651f, -0.014487f)) },
        { "indexfinger_L03", QuatT(Vec3(0.025985f, 0.000012f, 0.001577f), Quat(0.769257f, -0.028427f, 0.637390f, 0.034197f)) },
        { "indexfinger_L04", QuatT(Vec3(0.022366f, 0.000000f, 0.000000f), Quat(0.999293f, -0.000001f, 0.000000f, -0.037608f)) },
        { "middlefinger_L01", QuatT(Vec3(0.094119f, -0.011077f, 0.020161f), Quat(0.845943f, -0.004819f, 0.533161f, 0.009812f)) },
        { "middlefinger_L02", QuatT(Vec3(0.051566f, 0.000005f, 0.002341f), Quat(0.655776f, -0.012804f, 0.754766f, 0.011063f)) },
        { "middlefinger_L03", QuatT(Vec3(0.026943f, -0.000000f, 0.001719f), Quat(0.876153f, 0.003597f, 0.481974f, -0.006718f)) },
        { "middlefinger_L04", QuatT(Vec3(0.021961f, 0.000000f, 0.000000f), Quat(0.999904f, 0.000319f, 0.013551f, -0.003027f)) },
        { "ringfinger_L01", QuatT(Vec3(0.093776f, 0.013765f, 0.019229f), Quat(0.785264f, -0.068695f, 0.615336f, 0.001655f)) },
        { "ringfinger_L02", QuatT(Vec3(0.048213f, 0.000006f, 0.002290f), Quat(0.662898f, 0.002431f, 0.748704f, -0.001971f)) },
        { "ringfinger_L03", QuatT(Vec3(0.026269f, -0.000002f, 0.001688f), Quat(0.777221f, -0.007236f, 0.629104f, 0.010200f)) },
        { "ringfinger_L04", QuatT(Vec3(0.019120f, 0.000000f, -0.000000f), Quat(0.999102f, -0.001710f, -0.039147f, -0.016116f)) },
        { "pinky_L01", QuatT(Vec3(0.090513f, 0.035393f, 0.008370f), Quat(0.712868f, -0.093080f, 0.694835f, 0.018961f)) },
        { "pinky_L02", QuatT(Vec3(0.045267f, 0.000006f, 0.002150f), Quat(0.678996f, 0.011236f, 0.733979f, -0.010646f)) },
        { "pinky_L03", QuatT(Vec3(0.024558f, -0.000002f, 0.001578f), Quat(0.875149f, 0.006704f, 0.483614f, -0.013667f)) },
        { "pinky_L04", QuatT(Vec3(0.018585f, 0.000000f, -0.000000f), Quat(0.999986f, -0.000001f, -0.000000f, -0.005335f)) },
    };

    JointData closedHandRight[NUM_FINGER_JOINTS] = {
        { "thumb_R01", QuatT(Vec3(0.027691f, -0.031355f, 0.024529f), Quat(0.912799f, -0.106762f, -0.156049f, -0.362005f)) },
        { "thumb_R02", QuatT(Vec3(0.052911f, -0.000002f, -0.000000f), Quat(0.906897f, 0.019825f, -0.069170f, 0.415163f)) },
        { "thumb_R03", QuatT(Vec3(0.036524f, -0.000001f, 0.000000f), Quat(0.931936f, 0.017106f, 0.043692f, 0.359574f)) },
        { "thumb_R04", QuatT(Vec3(0.031123f, -0.000000f, -0.000000f), Quat(0.999988f, 0.000000f, 0.000000f, 0.004857f)) },
        { "indexfinger_R01", QuatT(Vec3(0.090690f, -0.038410f, -0.014565f), Quat(0.814487f, 0.120472f, -0.567530f, -0.002748f)) },
        { "indexfinger_R02", QuatT(Vec3(0.049201f, 0.000015f, -0.002123f), Quat(0.717804f, 0.036657f, -0.694245f, 0.037908f)) },
        { "indexfinger_R03", QuatT(Vec3(0.025985f, 0.000012f, -0.001577f), Quat(0.726036f, 0.030571f, -0.686218f, 0.032298f)) },
        { "indexfinger_R04", QuatT(Vec3(0.022366f, -0.000000f, -0.000000f), Quat(0.999293f, 0.000001f, -0.000001f, -0.037608f)) },
        { "middlefinger_R01", QuatT(Vec3(0.093959f, -0.015260f, -0.018067f), Quat(0.732077f, 0.092936f, -0.674694f, -0.014616f)) },
        { "middlefinger_R02", QuatT(Vec3(0.051566f, 0.000005f, -0.002341f), Quat(0.730038f, 0.011538f, -0.683197f, 0.012401f)) },
        { "middlefinger_R03", QuatT(Vec3(0.026943f, -0.000000f, -0.001719f), Quat(0.842602f, -0.004043f, -0.538484f, -0.006459f)) },
        { "middlefinger_R04", QuatT(Vec3(0.021961f, 0.000000f, 0.000000f), Quat(0.999904f, -0.000319f, -0.013551f, -0.003028f)) },
        { "ringfinger_R01", QuatT(Vec3(0.094673f, 0.009579f, -0.017284f), Quat(-0.655661f, -0.090059f, 0.742101f, 0.106229f)) },
        { "ringfinger_R02", QuatT(Vec3(0.048213f, 0.000006f, -0.002290f), Quat(0.763592f, -0.001885f, -0.645691f, -0.002532f)) },
        { "ringfinger_R03", QuatT(Vec3(0.026269f, -0.000002f, -0.001688f), Quat(0.822182f, 0.006100f, -0.569084f, 0.011150f)) },
        { "ringfinger_R04", QuatT(Vec3(0.019120f, -0.000000f, -0.000000f), Quat(0.999102f, 0.001710f, 0.039147f, -0.016117f)) },
        { "pinky_R01", QuatT(Vec3(0.092119f, 0.031399f, -0.006621f), Quat(-0.645821f, -0.047245f, 0.758434f, 0.073903f)) },
        { "pinky_R02", QuatT(Vec3(0.045267f, 0.000006f, -0.002150f), Quat(0.708652f, -0.011164f, -0.705380f, -0.011207f)) },
        { "pinky_R03", QuatT(Vec3(0.024558f, -0.000002f, -0.001578f), Quat(0.794741f, -0.008833f, -0.606762f, -0.012200f)) },
        { "pinky_R04", QuatT(Vec3(0.018585f, 0.000000f, 0.000000f), Quat(0.999986f, 0.000001f, 0.000000f, -0.005335f)) },
    };
}

void ApplyHandPose(int side, ISkeletonPose* skeleton, float openToClosed)
{
    openToClosed = clamp(openToClosed, 0.f, 1.f);
    JointData* open = side == 0 ? openHandLeft : openHandRight;
    JointData* closed = side == 0 ? closedHandLeft : closedHandRight;

    for (int i = 0; i < NUM_FINGER_JOINTS; ++i)
    {
        int jointId = skeleton->GetJointIDByName(open[i].name);
        if (jointId >= 0)
        {
            QuatT pose = QuatT::CreateNLerp(open[i].pose, closed[i].pose, openToClosed);
            skeleton->SetPostProcessQuat(jointId, pose);
        }
    }
}
