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
#include "FoveatedChroma.h"
#include <io.h>
#include <cstdio>
#include <cstdlib>

static const glm::vec4 kClearColor(0.28f, 0.52f, 0.90f, 1);

#pragma comment(lib, "FoveClient.lib")

#define TRANSITION_TIME 1.5f
#define TRANSITION_TIME_START 5.f
#define BASE_PATH ""

const char *GetStateString(ExperimentState state)
{
    switch (state) {
    case ExperimentState::Init:
        return "[init]";
    case ExperimentState::Transition:
        return "[transition]";
    case ExperimentState::Trial:
        return "[trial]";
    case ExperimentState::End:
        return "[end]";
    default:
        return "[?]";
    }
}

float graph_func(void *ud, int idx)
{
    if (idx == 0) return 0.f;
    glm::vec4 levels = *reinterpret_cast<glm::vec4*>(ud);

    return levels[idx - 1];
}

void StereoRendering::onGuiRender()
{

    if (VRSystem::instance())
    {
        mpGui->addCheckBox("Display VR FBO", mShowStereoViews);
    }

    if (mpGui->addDropdown("Submission Mode", mSubmitModeList, (uint32_t&)mRenderMode))
    {
        setRenderMode();
    }
}

void StereoRendering::startExperiment()
{
    mpExperiment->clear();

    Psychophysics::ExperimentalDesignParameter parameters = {};
    parameters.mMeasuringMethod = Psychophysics::PsychophysicsMethod::DiscreteStaircase;
    parameters.mIsDefault = false;
    if (mpCurrentLayer >= 1)
        parameters.mInitLevel = mpFoveationLevels[mpCurrentLayer - 1];
    else
        parameters.mInitLevel = 0.f;

    parameters.mInitLevelStepSize = 1.f;
    parameters.mMinLevelStepSize = 1.f;
    parameters.mNumUp = 1;
    parameters.mNumDown = 2;

    parameters.mMaxReversals = 3;
    parameters.mMaxTotalTrialCount = 20;

    parameters.mMaxLimitHitCount = 2;
    parameters.mMaxLevel = 10.f;
    if (mpCurrentLayer >= 1)
        parameters.mMinLevel = mpFoveationLevels[mpCurrentLayer - 1];
    else
        parameters.mMinLevel = 0.f;

    Psychophysics::ConditionParameter p = {};
    mpExperiment->addCondition(p, parameters);
}

void StereoRendering::dumpResults()
{

    char filename[128];
    for (int i = 1;; ++i) {
        snprintf(filename, 128, BASE_PATH "participant_results_%d.txt", i);

        if (_access(filename, 0) != -1) {

            FILE *f = fopen(filename, "wb+");

            for (int j = 0; j < 4; ++j) {
                LayerResult &result = mpResults[j];

                fprintf(f, "layer=%d,starting=%d\n", j, result.startingLayer);
                for (auto r : result.responses) {
                    fprintf(f, "\t>%d\n", r);
                }
            }

            fprintf(f, "\ndebug:\n");
            for (int j = 0; j < 4; ++j) {
                LayerResult &result = mpResults[j];

                fprintf(f, "layer=%d,starting=%d\n", j, result.startingLayer);
                for (auto msg : result.debug) {
                    fprintf(f, "\t>%s\n", msg.c_str());
                }
            }

            fclose(f);
            break;
        }
    }
}

void StereoRendering::debug(const char * s, ...)
{
    char buffer[128];
    va_list args;
    va_start(args, s);
    vsnprintf(buffer, sizeof(buffer), s, args);
    va_end(args);

    mpResults[mpCurrentLayer].debug.push_back(std::string(buffer));
}

bool displaySpsWarning()
{
#ifdef FALCOR_D3D12
    logWarning("The sample will run faster if you use NVIDIA's Single Pass Stereo\nIf you have an NVIDIA GPU, download the NVAPI SDK and enable NVAPI support in FalcorConfig.h.\nCheck the readme for instructions");
#endif
    return false;
}

void StereoRendering::initVR()
{
    mColorSpaceList.clear();
    mColorSpaceList.push_back({ (int)ColorSpace::YCoCg, "YCoCg" });
    mColorSpaceList.push_back({ (int)ColorSpace::YCoCg24, "YCoCg24" });

    mSubmitModeList.clear();
    mSubmitModeList.push_back({ (int)RenderMode::Mono, "Render to Screen" });

    
    if (VRSystem::instance())
    {
        mpFove = std::unique_ptr<Fove::IFVRHeadset> { Fove::GetFVRHeadset() };
        if (!mpFove)
            return;
        mpFove->Initialise(Fove::EFVR_ClientCapabilities::Orientation | Fove::EFVR_ClientCapabilities::Gaze);

        // Create the FBOs
        Fbo::Desc vrFboDesc;

        vrFboDesc.setColorTarget(0, mpDefaultFBO->getColorTexture(0)->getFormat());
        // TODO: maybe?
        //vrFboDesc.setColorTarget(1, mpDefaultFBO->getColorTexture(0)->getFormat());
        vrFboDesc.setDepthStencilTarget(mpDefaultFBO->getDepthStencilTexture()->getFormat());
        vrFboDesc.setSampleCount(4);

        mpVrFbo = VrFbo::create(vrFboDesc);

        mSubmitModeList.push_back({ (int)RenderMode::Stereo, "Stereo" });

        if (mSPSSupported)
        {
            mSubmitModeList.push_back({ (int)RenderMode::SinglePassStereo, "Single Pass Stereo" });
        }
        else
        {
            displaySpsWarning();
        }
    }
    else
    {
        msgBox("Can't initialize the VR system. Make sure that your HMD is connected properly");
    }
}

void StereoRendering::submitStereo(bool singlePassStereo)
{
    // TODO: create fbo per eye for postfx:
    //       CopySubresource into VrFbo::Left/RightView


    PROFILE(STEREO);
    VRSystem::instance()->refresh();

    // Clear the FBO
    mpRenderContext->clearFbo(mpVrFbo->getFbo().get(), kClearColor, 1.0f, 0, FboAttachmentType::All);
    
    // update state
    if (singlePassStereo)
    {
        mpGraphicsState->setProgram(mpMonoSPSProgram);
        mpRenderContext->setGraphicsVars(mpMonoSPSVars);
    }
    else
    {
        mpGraphicsState->setProgram(mpStereoProgram);
        mpRenderContext->setGraphicsVars(mpStereoVars);
    }
    mpGraphicsState->setFbo(mpVrFbo->getFbo());
    mpRenderContext->pushGraphicsState(mpGraphicsState);

    // Render
    mpSceneRenderer->renderScene(mpRenderContext.get());

    mpGraphicsState->setDepthStencilState(mSkyBox.pDS);
    //mSkyBox.pEffect->render(mpRenderContext.get(), mpSceneRenderer->getScene()->getActiveCamera().get());
    mpGraphicsState->setDepthStencilState(nullptr);

    VRDisplay::Eye eyes[] = { VRDisplay::Eye::Left, VRDisplay::Eye::Right };
    Fbo::SharedPtr fbos[] = { mpTempVrFBLeft, mpTempVrFBRight };
    if (mpDebugFoveation) {
        mpVrFbo->unwrap(mpRenderContext.get());

        for (int i = 0; i < 2; ++i) {
            auto eye = eyes[i];
            auto fbo = fbos[i];

            mpColorVars->setTexture("gTexture", mpVrFbo->getEyeResourceView(eye));
            mpColorVars->setSampler("gSampler", mpTriLinearSampler);

            mpGraphicsState->setFbo(fbo);
            mpRenderContext->setGraphicsState(mpGraphicsState);
            mpRenderContext->setGraphicsVars(mpColorVars);
            mpColorSplit->execute(mpRenderContext.get());

            fbo->getColorTexture(0)->generateMips();
        }

        //mpVrFbo->unwrap(mpRenderContext.get());

        mpBlitVrLeftVars->setTexture("gTextureLeft", mpTempVrFBLeft->getColorTexture(0));
        mpBlitVrLeftVars->setTexture("gTextureRight", mpTempVrFBRight->getColorTexture(0));
        mpBlitVrLeftVars->setSampler("gSampler", mpBilinearSampler);

        mpGraphicsState->setFbo(mpVrFbo->getFboDouble());
        mpRenderContext->setGraphicsState(mpGraphicsState);
        mpRenderContext->setGraphicsVars(mpBlitVrLeftVars);
        mpBlitVrLeft->execute(mpRenderContext.get());
    }

    // Restore the state
    mpRenderContext->popGraphicsState();

    // Submit the views and display them
    mpVrFbo->submitToHmd(mpRenderContext.get());
    blitTexture(mpVrFbo->getEyeResourceView(VRDisplay::Eye::Left), 0);
    blitTexture(mpVrFbo->getEyeResourceView(VRDisplay::Eye::Right), mpDefaultFBO->getWidth() / 2);
}

void StereoRendering::submitToScreen()
{
    mpRenderContext->pushGraphicsState(mpGraphicsState);

    mpGraphicsState->setDepthStencilState(mpDepthStencilState);
    mpGraphicsState->setProgram(mpMonoSPSProgram);
    mpGraphicsState->setFbo(mpMainFB);
    mpRenderContext->setGraphicsState(mpGraphicsState);
    mpRenderContext->setGraphicsVars(mpMonoSPSVars);
    mpSceneRenderer->renderScene(mpRenderContext.get());

    mpGraphicsState->setDepthStencilState(mSkyBox.pDS);
    mSkyBox.pEffect->render(mpRenderContext.get(), mpSceneRenderer->getScene()->getActiveCamera().get());
    mpGraphicsState->setDepthStencilState(nullptr);

    if (mpDebugFoveation)
        mpRenderContext->blit(mpMainFB->getColorTexture(0)->getSRV(), mpResolveFB->getRenderTargetView(0));
    else
        mpRenderContext->blit(mpMainFB->getColorTexture(0)->getSRV(), mpDefaultFBO->getRenderTargetView(0));
    //mpRenderContext->blit(mpMainFB->getDepthStencilTexture()->getSRV(), mpResolveFB->getRenderTargetView(2));

    if (mpDebugFoveation) {
        if (mpTransitionTime > 0.f) {
            mpBlur->setSigma((0.01f + sinf((1.f - mpTransitionTime / TRANSITION_TIME) * (float)M_PI)) * 5.5f);
            //mpBlur->setSigma(mpTransitionTime);
            mpBlur->execute(mpRenderContext.get(), mpResolveFB->getColorTexture(0), mpResolveFB);
        }

        mpColorVars->setTexture("gTexture", mpResolveFB->getColorTexture(0));
        mpColorVars->setSampler("gSampler", mpTriLinearSampler);

        mpGraphicsState->setFbo(mpTempFB);
        mpRenderContext->setGraphicsState(mpGraphicsState);
        mpRenderContext->setGraphicsVars(mpColorVars);
        mpColorSplit->execute(mpRenderContext.get());

        mpTempFB->getColorTexture(0)->generateMips();

        mpBlitVars->setTexture("gTexture", mpTempFB->getColorTexture(0));
        mpBlitVars->setSampler("gSampler", mpBilinearSampler);

        mpGraphicsState->setFbo(mpDefaultFBO);
        mpRenderContext->setGraphicsState(mpGraphicsState);
        mpRenderContext->setGraphicsVars(mpBlitVars);
        mpBlit->execute(mpRenderContext.get());
    }
    //mpRenderContext->blit(mpTempFB2->getColorTexture(0)->getSRV(), mpDefaultFBO->getRenderTargetView(0));
}

void StereoRendering::setRenderMode()
{
    if(mpScene)
    {
        std::string lights;
        getSceneLightString(mpScene.get(), lights);
        mpMonoSPSProgram->addDefine("_LIGHT_SOURCES", lights);
        mpStereoProgram->addDefine("_LIGHT_SOURCES", lights);
        mpMonoSPSProgram->removeDefine("_SINGLE_PASS_STEREO");

        mpGraphicsState->toggleSinglePassStereo(false);
        switch(mRenderMode)
        {
        case RenderMode::SinglePassStereo:
            mpMonoSPSProgram->addDefine("_SINGLE_PASS_STEREO");
            mpGraphicsState->toggleSinglePassStereo(true);
            mpSceneRenderer->setCameraControllerType(SceneRenderer::CameraControllerType::Hmd);
            break;
        case RenderMode::Stereo:
            mpSceneRenderer->setCameraControllerType(SceneRenderer::CameraControllerType::Hmd);
            break;
        case RenderMode::Mono:
            mpSceneRenderer->setCameraControllerType(SceneRenderer::CameraControllerType::FirstPerson);
            break;
        }
    }
}

void StereoRendering::loadScene()
{
    std::string filename;
    if(openFileDialog("Scene files\0*.fscene\0\0", filename))
    {
        loadScene(filename);
    }
}

void StereoRendering::loadScene(const std::string& filename)
{
    mpScene = Scene::loadFromFile(filename);
    mpEditor = SceneEditor::create(mpScene);

    mpSceneRenderer = SceneRenderer::create(mpScene);
    mpMonoSPSProgram = GraphicsProgram::createFromFile("", appendShaderExtension("StereoRendering.ps"));
    mpStereoProgram = GraphicsProgram::createFromFile(appendShaderExtension("StereoRendering.vs"), appendShaderExtension("StereoRendering.ps"), appendShaderExtension("StereoRendering.gs"), "", "");

    mpColorSplit = FullScreenPass::create(appendShaderExtension("ColorSplit.ps"));
    mpColorVars = GraphicsVars::create(mpColorSplit->getProgram()->getActiveVersion()->getReflector());

    mpBlit = FullScreenPass::create(appendShaderExtension("FoveatedBlit.ps"));
    mpBlitVars = GraphicsVars::create(mpBlit->getProgram()->getActiveVersion()->getReflector());
    Program::DefineList defines;
    defines.add("EYE_SLICE", "0");
    mpBlitVrLeft = FullScreenPass::create(appendShaderExtension("FoveatedBlitStereo.ps"), defines);
    mpBlitVrLeftVars = GraphicsVars::create(mpBlitVrLeft->getProgram()->getActiveVersion()->getReflector());
    defines.add("EYE_SLICE", "1");
    mpBlitVrRight = FullScreenPass::create(appendShaderExtension("FoveatedBlitStereo.ps"), defines);
    mpBlitVrRightVars = GraphicsVars::create(mpBlitVrRight->getProgram()->getActiveVersion()->getReflector());

    RasterizerState::Desc rdesc;
    rdesc.setCullMode(RasterizerState::CullMode::None);
    mpFSRasterizerState = RasterizerState::create(rdesc);

    DepthStencilState::Desc desc;
    mpDepthStencilState = DepthStencilState::create(desc);

    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    mSkyBox.pSampler = Sampler::create(samplerDesc);
    mSkyBox.pEffect = SkyBox::createFromTexture("Bistro/BlueSky.exr", true, mSkyBox.pSampler);
    DepthStencilState::Desc dsDesc;
    dsDesc.setDepthFunc(DepthStencilState::Func::Always);
    mSkyBox.pDS = DepthStencilState::create(dsDesc);

    setRenderMode();
    mpMonoSPSVars = GraphicsVars::create(mpMonoSPSProgram->getActiveVersion()->getReflector());
    mpStereoVars = GraphicsVars::create(mpStereoProgram->getActiveVersion()->getReflector());

    for (uint32_t m = 0; m < mpScene->getModelCount(); m++)
    {
        mpScene->getModel(m)->bindSamplerToMaterials(mpBilinearSampler2);
    }
}

void StereoRendering::onLoad()
{
    mpExperiment = std::make_unique<Psychophysics::Experiment>();
    mpFont = TextRenderer::create();
    mpBlur = GaussianBlur::create(11, 1.f);
    startExperiment();

    mSPSSupported = gpDevice->isExtensionSupported("VK_NVX_multiview_per_view_attributes");

    initVR();

    mpGraphicsState = GraphicsState::create();
    setRenderMode();

    Fbo::Desc fboDesc;
    fboDesc.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float);
    fboDesc.setDepthStencilTarget(ResourceFormat::D32Float);
    if (mpVrFbo) {
        mpTempVrFBLeft = FboHelper::create2D(mpVrFbo->getFbo()->getWidth(), mpVrFbo->getFbo()->getHeight(), fboDesc, 1, 10);
        mpTempVrFBRight = FboHelper::create2D(mpVrFbo->getFbo()->getWidth(), mpVrFbo->getFbo()->getHeight(), fboDesc, 1, 10);
    }
    fboDesc.setSampleCount(4);
    fboDesc.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float);
    mpMainFB = FboHelper::create2D(mpDefaultFBO->getWidth(), mpDefaultFBO->getHeight(), fboDesc, 1, 1);
    fboDesc.setSampleCount(1);
    mpResolveFB = FboHelper::create2D(mpDefaultFBO->getWidth(), mpDefaultFBO->getHeight(), fboDesc, 1, Texture::kMaxPossible);
    fboDesc.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float);
    mpTempFB = FboHelper::create2D(mpDefaultFBO->getWidth(), mpDefaultFBO->getHeight(), fboDesc, 1, Texture::kMaxPossible);

    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    mpTriLinearSampler = Sampler::create(samplerDesc);

    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    samplerDesc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
    mpBilinearSampler = Sampler::create(samplerDesc);
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    samplerDesc.setAddressingMode(Sampler::AddressMode::Mirror, Sampler::AddressMode::Mirror, Sampler::AddressMode::Wrap);
    samplerDesc.setLodParams(0, 5, 0);
    mpBilinearSampler2 = Sampler::create(samplerDesc);;

    loadScene("Scenes/breakfast.fscene");
    //loadScene("Bistro/Bistro_Interior.fscene");
}

void StereoRendering::blitTexture(Texture::SharedPtr pTexture, uint32_t xStart)
{
    if(mShowStereoViews)
    {
        uvec4 dstRect;
        dstRect.x = xStart;
        dstRect.y = 0;
        dstRect.z = xStart + (mpDefaultFBO->getWidth() / 2);
        dstRect.w = mpDefaultFBO->getHeight();
        mpRenderContext->blit(pTexture->getSRV(0, 1, 0, 1), mpDefaultFBO->getRenderTargetView(0), uvec4(-1), dstRect);
    }
}

void StereoRendering::onFrameRender()
{
    static uint32_t frameCount = 0u;

    Fove::SFVR_Vec2 left, right;
    if (mpFove && Fove::EFVR_ErrorCode::None == mpFove->GetGazeVectors2D(&left, &right)) {
        Fove::EFVR_Eye eye;
        mpFove->CheckEyesClosed(&eye);
        if (eye == Fove::EFVR_Eye::Neither) {
            mpGazePosition.x = left.x * 0.5f + 0.5f;
            mpGazePosition.y = 1 - (left.y * 0.5f + 0.5f);
        }
    }

    float t = (1.f - mpTransitionTime / TRANSITION_TIME);
    for (int i = mpCurrentLayer; i < 4; ++i) {
        mpFoveationLevels[i] = glm::lerp(mpPrevLevel, mpExperiment->getLevelForCurrentTrial(), t);
    }


    ConstantBuffer::SharedPtr pCB = mpBlitVars->getConstantBuffer("FoveatedCB");
    pCB["gEyePos"] = glm::vec4(mpGazePosition, mpDebugViz ? 1.0 : 0.0, mpColorSpace);
    pCB["gEyeLevels"] = mpFoveationLevels;

    ConstantBuffer::SharedPtr pVRCB = mpBlitVrLeftVars->getConstantBuffer("FoveatedCB");
    pVRCB["gEyePos"] = glm::vec4(mpGazePosition, mpDebugViz ? 1.0 : 0.0, mpColorSpace);
    pVRCB["gEyeLevels"] = mpFoveationLevels;

    ConstantBuffer::SharedPtr pCCB = mpColorVars->getConstantBuffer("FoveatedCB");
    pCCB["gEyePos"] = glm::vec4(mpGazePosition, mpDebugViz ? 1.0 : 0.0, mpColorSpace);
    pCCB["gEyeLevels"] = glm::vec4(mpTransitionTime > 0.f ? 1.f : 0.f, sinf((1.f - mpTransitionTime / TRANSITION_TIME) * (float)M_PI), 0.f, 0.f);

    mpRenderContext->clearFbo(mpMainFB.get(), kClearColor, 1.0f, 0, FboAttachmentType::All);
    mpRenderContext->clearFbo(mpTempFB.get(), kClearColor, 1.0f, 0, FboAttachmentType::All);

    if (mpSceneRenderer)
    {
        mpEditor->update(mCurrentTime);
        mpSceneRenderer->update(mCurrentTime);

        switch (mRenderMode)
        {
        case RenderMode::Mono:
            submitToScreen();
            break;
        case RenderMode::SinglePassStereo:
            submitStereo(true);
            break;
        case RenderMode::Stereo:
            submitStereo(false);
            break;
        default:
            should_not_get_here();
        }
    }

    //renderText(message, glm::vec2(10, 10));

    mpFont->begin(mpRenderContext, glm::vec2(10, 10));

    auto red = glm::vec3(0.89, 0.72, 0.09);
    auto green = glm::vec3(0.19, 0.92, 0.12);
    auto gray = glm::vec3(0.85, 0.85, 0.85);

    mpFont->setTextColor(gray);
    mpFont->renderLine(getFpsMsg());
    mpFont->renderLine(std::string("state: ") + GetStateString(mpState));
    mpFont->renderLine("levels:");
    for (int i = 0; i < 4; ++i) {
        char label[128];
        int len = snprintf(label, 128, "  %d: %.3f (t=%d/20,r=%d/3)", i, mpFoveationLevels[i], (int)mpResults[i].responses.size(), mpResults[i].reverses);


        if (i == mpCurrentLayer && mpState != ExperimentState::End) {
            if (mpState == ExperimentState::Trial)
                snprintf(label + len, 128 - len, " %.1f seconds left in current trial", mpTrialTime);
            else if (mpState == ExperimentState::Transition)
                snprintf(label + len, 128 - len, " %.1f seconds left in transition", mpTransitionTime);
        }

        mpFont->renderLine(std::string(label));
    }

    mpFont->renderLine("debug:");
    for (int i = 0; i < mpResults[mpCurrentLayer].debug.size(); ++i) {
        std::string msg = mpResults[mpCurrentLayer].debug[i];

        char label[128];
        snprintf(label, 128, "  %d: %s", i, msg.c_str());

        mpFont->renderLine(std::string(label));
    }

    mpFont->end();

    frameCount++;
    mpTimer.update();
    mpTransitionTime -= mpTimer.getElapsedTime();
    if (mpTransitionTime < 0.f) mpTransitionTime = 0.f;
    mpTrialTime -= mpTimer.getElapsedTime();
    if (mpTrialTime < 0.f) mpTrialTime = 0.f;

    Psychophysics::SingleThresholdMeasurement m = mpExperiment->getMeasurement();
    mpResults[mpCurrentLayer].reverses = m.mReversalCount;
    mpResults[mpCurrentLayer].reverseLimit = m.mReversalCount;

    if (mpExperiment->isComplete() && mpState != ExperimentState::Transition) {
        if (mpCurrentLayer + 1 > 3 && mpState != ExperimentState::End) {
            mpState = ExperimentState::End;
            dumpResults();
            debug("finished experiment");
        }
        else if (mpState != ExperimentState::End) {
            mpCurrentLayer++;
            startExperiment();
            debug("finished layer %d, starting %d", mpCurrentLayer - 1, mpCurrentLayer);
        }
    }

    // TODO: check if finished => goto ::End
    switch (mpState) {
    case ExperimentState::Init:
        // TODO: instructions
        break;
    case ExperimentState::Transition:
        if (mpTransitionTime <= 0.f) {
            mpState = ExperimentState::Trial;
            mpTrialTime = 8.f + (float)(rand() % 4);

            debug("starting trial with stimulus %.1f", mpFoveationLevels[mpCurrentLayer]);
        }
        break;
    case ExperimentState::Trial:
        if (mpTrialTime <= 0.f) {
            mpPrevLevel = mpFoveationLevels[mpCurrentLayer];
            mpState = ExperimentState::Transition;
            mpTransitionTime = TRANSITION_TIME;

            mpExperiment->processResponse(0);
            mpResults[mpCurrentLayer].responses.push_back(0);

            debug("participant did not detect change.., change=%.1f", mpExperiment->getMeasurement().getCurrentLevel() - mpPrevLevel);
        }
        break;
    case ExperimentState::End:
        break;
    }
}

bool StereoRendering::onKeyEvent(const KeyboardEvent& keyEvent)
{
    if (keyEvent.key == KeyboardEvent::Key::LeftAlt && keyEvent.type == KeyboardEvent::Type::KeyReleased) {
        mpDebugFoveation = !mpDebugFoveation;
    }
    if (keyEvent.key == KeyboardEvent::Key::R && keyEvent.type == KeyboardEvent::Type::KeyReleased) {
        mpDebugViz = !mpDebugViz;
    }

    if (keyEvent.key == KeyboardEvent::Key::U && keyEvent.type == KeyboardEvent::Type::KeyPressed) {
        mShowUI = !mShowUI;
    }
    if (keyEvent.key == KeyboardEvent::Key::K && keyEvent.type == KeyboardEvent::Type::KeyPressed) {
        mpExperiment->clear();
        mpCurrentLayer = 0;
    }
    if (keyEvent.key == KeyboardEvent::Key::O && keyEvent.type == KeyboardEvent::Type::KeyPressed) {
        mpCurrentLayer++;
        startExperiment();
    }

    if (keyEvent.key == KeyboardEvent::Key::Space && keyEvent.type == KeyboardEvent::Type::KeyPressed) {
        switch (mpState) {
        case ExperimentState::Init:
            mpState = ExperimentState::Transition;
            mpTransitionTime = TRANSITION_TIME_START;
            debug("started experiment");
            break;
        case ExperimentState::Transition:
            break;
        case ExperimentState::Trial:
            mpPrevLevel = mpFoveationLevels[mpCurrentLayer];

            mpExperiment->processResponse(1);
            mpResults[mpCurrentLayer].responses.push_back(1);

            mpState = ExperimentState::Transition;
            mpTransitionTime = TRANSITION_TIME;

            debug("participant detected change.., change=%.1f", mpExperiment->getMeasurement().getCurrentLevel() - mpPrevLevel);
            break;
        case ExperimentState::End:
            break;
        }
    }


    return mpSceneRenderer ? mpSceneRenderer->onKeyEvent(keyEvent) : false;
}

bool StereoRendering::onMouseEvent(const MouseEvent& mouseEvent)
{
    switch (mouseEvent.type) {
    case MouseEvent::Type::RightButtonDown:
        mpMouseMovingGaze = true;
        break;
    case MouseEvent::Type::RightButtonUp:
        mpMouseMovingGaze = false;
        break;
    case MouseEvent::Type::Move:
        if (mpMouseMovingGaze) {
            mpGazePosition = mouseEvent.pos;
        }
        break;
    default:
        break;
    }
    return mpDebugFoveation ? true : mpSceneRenderer ? mpSceneRenderer->onMouseEvent(mouseEvent) : false;
}

void StereoRendering::onDataReload()
{

}

void StereoRendering::onResizeSwapChain()
{
    initVR();
}

#ifdef _WIN32
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
#else
int main(int argc, char** argv)
#endif
{
    StereoRendering sample;
    SampleConfig config;
    config.windowDesc.title = "Stereo Rendering";
    config.windowDesc.height = 720;
    config.windowDesc.width = 1280;
    config.windowDesc.resizableWindow = true;
    config.deviceDesc.enableVR = true;
#ifdef FALCOR_VK
    config.deviceDesc.enableDebugLayer = false; // OpenVR requires an extension that the debug layer doesn't recognize. It causes the application to crash
#endif

#ifdef _WIN32
    sample.run(config);
#else
    sample.run(config, (uint32_t)argc, argv);
#endif
    return 0;
}
