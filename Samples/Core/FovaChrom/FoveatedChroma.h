/***************************************************************************
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************/
#pragma once
#include "Falcor.h"

#include "FoveTypes.h"
#include "IFVRHeadset.h"

#include "Utils\Psychophysics\Experiment.h"

using namespace Falcor;

enum class ExperimentState {
    Init,
    Trial,
    Transition,
    End
};

struct LayerResult {
    int startingLayer;
    std::vector<int> responses;
    std::vector<std::string> debug;
    int responseLimit;
    int reverses;
    int reverseLimit;
};

class StereoRendering : public Sample
{
public:
    void onLoad() override;
    void onFrameRender() override;
    void onResizeSwapChain() override;
    bool onKeyEvent(const KeyboardEvent& keyEvent) override;
    bool onMouseEvent(const MouseEvent& mouseEvent) override;
    void onDataReload() override;
    void onGuiRender() override;
    void startExperiment();
    void dumpResults();
    void debug(const char *s, ...);

private:
    std::unique_ptr<Psychophysics::Experiment> mpExperiment = nullptr;
    std::unique_ptr<TextRenderer> mpFont = nullptr;
    std::unique_ptr<GaussianBlur> mpBlur = nullptr;
    CpuTimer mpTimer = {};
    LayerResult mpResults[4] = {};
    std::string mpDescription = "";
    float mpTrialTime = 0.f;
    float mpTransitionTime = 0.f;
    ExperimentState mpState = ExperimentState::Init;

    std::unique_ptr<Fove::IFVRHeadset> mpFove = nullptr;
    unsigned int mpCurrentLayer = 0;
    Scene::SharedPtr mpScene;
    SceneRenderer::SharedPtr mpSceneRenderer;
    SceneEditor::UniquePtr mpEditor = nullptr;

    DepthStencilState::SharedPtr mpDepthStencilState = nullptr;
    GraphicsProgram::SharedPtr mpMonoSPSProgram = nullptr;
    GraphicsVars::SharedPtr mpMonoSPSVars = nullptr;

    GraphicsProgram::SharedPtr mpStereoProgram = nullptr;
    GraphicsVars::SharedPtr mpStereoVars = nullptr;

    GraphicsState::SharedPtr mpGraphicsState = nullptr;
    Sampler::SharedPtr mpTriLinearSampler;
    Sampler::SharedPtr mpBilinearSampler;
    Sampler::SharedPtr mpBilinearSampler2;
    RasterizerState::SharedPtr mpFSRasterizerState;

    Fbo::SharedPtr mpMainFB;
    Fbo::SharedPtr mpTempFB;
    Fbo::SharedPtr mpResolveFB;

    Fbo::SharedPtr mpMainVrFBLeft;
    Fbo::SharedPtr mpMainVrFBRight;
    Fbo::SharedPtr mpResolveVrFBLeft;
    Fbo::SharedPtr mpResolveVrFBRight;
    Fbo::SharedPtr mpTempVrFBLeft;
    Fbo::SharedPtr mpTempVrFBRight;
    ConstantBuffer::SharedPtr mpFoveatedBuffer;
    FullScreenPass::UniquePtr mpColorSplit;
    GraphicsVars::SharedPtr mpColorVars;

    bool mpDebugFoveation = true;
    bool mpDebugViz = false;
    bool mpLerpGaze = false;
    bool mpMouseMovingGaze = false;
    glm::vec2 mpGazePosition;
    glm::vec2 mpPrevGazePosition;
    glm::vec4 mpFoveationLevels;
    float mpPrevLevel = 0;

    FullScreenPass::UniquePtr mpBlit;
    GraphicsVars::SharedPtr mpBlitVars;
    FullScreenPass::UniquePtr mpBlitVrLeft;
    GraphicsVars::SharedPtr mpBlitVrLeftVars;
    FullScreenPass::UniquePtr mpBlitVrRight;
    GraphicsVars::SharedPtr mpBlitVrRightVars;


    void loadScene();
    void loadScene(const std::string & filename);

    enum class RenderMode
    {
        Mono,
        Stereo,
        SinglePassStereo
    };

    struct
    {
        SkyBox::UniquePtr pEffect;
        DepthStencilState::SharedPtr pDS;
        Sampler::SharedPtr pSampler;
    } mSkyBox;

    enum class ColorSpace {
        YCoCg = 0,
        YCoCg24,
    };

    bool mSPSSupported = false;
    RenderMode mRenderMode = RenderMode::Mono;
    Gui::DropdownList mSubmitModeList;
    Gui::DropdownList mColorSpaceList;
    ColorSpace mpColorSpace = ColorSpace::YCoCg;

    void submitToScreen();
    void initVR();
    void blitTexture(Texture::SharedPtr pTexture, uint32_t xStart);
    VrFbo::UniquePtr mpVrFbo;
    bool mShowStereoViews = true;
    void submitStereo(bool singlePassStereo);
    void setRenderMode();
};
