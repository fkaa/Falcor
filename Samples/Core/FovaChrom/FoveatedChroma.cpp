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

static const glm::vec4 kClearColor(0.28f, 0.52f, 0.90f, 1);

#pragma comment(lib, "FoveClient.lib")

void StereoRendering::onGuiRender()
{
    if (mpGui->addButton("Load Scene"))
    {
        loadScene();
    }

    if(VRSystem::instance())
    {
        mpGui->addCheckBox("Display VR FBO", mShowStereoViews);
    }

    if (mpGui->addDropdown("Submission Mode", mSubmitModeList, (uint32_t&)mRenderMode))
    {
        setRenderMode();
    }

    if (mpGui->addDropdown("Colorspace", mColorSpaceList, (uint32_t&)mpColorSpace))
    {
    }

    if (mpEditor)
    {
        mpEditor->renderGui(mpGui.get());
    }
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
    mpGraphicsState->setFbo(mpDefaultFBO);
    mpRenderContext->setGraphicsState(mpGraphicsState);
    mpRenderContext->setGraphicsVars(mpMonoSPSVars);
    mpSceneRenderer->renderScene(mpRenderContext.get());

    mpGraphicsState->setDepthStencilState(mSkyBox.pDS);
    mSkyBox.pEffect->render(mpRenderContext.get(), mpSceneRenderer->getScene()->getActiveCamera().get());
    mpGraphicsState->setDepthStencilState(nullptr);

    if (mpDebugFoveation) {
        mpColorVars->setTexture("gTexture", mpDefaultFBO->getColorTexture(0));
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
    mSPSSupported = gpDevice->isExtensionSupported("VK_NVX_multiview_per_view_attributes");

    initVR();

    mpGraphicsState = GraphicsState::create();
    setRenderMode();

    Fbo::Desc fboDesc;
    fboDesc.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float);
    fboDesc.setDepthStencilTarget(ResourceFormat::D32Float);
    if (mpVrFbo) {
        mpTempVrFBLeft = FboHelper::create2D(mpVrFbo->getFbo()->getWidth(), mpVrFbo->getFbo()->getHeight(), fboDesc, 1, Texture::kMaxPossible);
        mpTempVrFBRight = FboHelper::create2D(mpVrFbo->getFbo()->getWidth(), mpVrFbo->getFbo()->getHeight(), fboDesc, 1, Texture::kMaxPossible);
    }
    mpTempFB = FboHelper::create2D(mpDefaultFBO->getWidth(), mpDefaultFBO->getHeight(), fboDesc, 1, Texture::kMaxPossible);

    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
    mpTriLinearSampler = Sampler::create(samplerDesc);

    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    samplerDesc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
    mpBilinearSampler = Sampler::create(samplerDesc);
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Linear);
    samplerDesc.setAddressingMode(Sampler::AddressMode::Mirror, Sampler::AddressMode::Mirror, Sampler::AddressMode::Wrap);
    samplerDesc.setLodParams(0, 0, 0);
    mpBilinearSampler2 = Sampler::create(samplerDesc);;

    loadScene("Scenes/ogre.fscene");
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
    if (Fove::EFVR_ErrorCode::None == mpFove->GetGazeVectors2D(&left, &right)) {
        mpGazePosition.x = left.x * 0.5f + 0.5f;
        mpGazePosition.y = 1 - (left.y * 0.5f + 0.5f);
    }
    
    ConstantBuffer::SharedPtr pCB = mpBlitVars->getConstantBuffer("FoveatedCB");
    pCB["gEyePos"] = glm::vec4(mpGazePosition, mpDebugViz ? 1.0 : 0.0, mpColorSpace);

    ConstantBuffer::SharedPtr pVRCB = mpBlitVrLeftVars->getConstantBuffer("FoveatedCB");
    pVRCB["gEyePos"] = glm::vec4(mpGazePosition, mpDebugViz ? 1.0 : 0.0, mpColorSpace);

    ConstantBuffer::SharedPtr pCCB = mpColorVars->getConstantBuffer("FoveatedCB");
    pCCB["gEyePos"] = glm::vec4(mpGazePosition, mpDebugViz ? 1.0 : 0.0, mpColorSpace);
    
    mpRenderContext->clearFbo(mpDefaultFBO.get(), kClearColor, 1.0f, 0, FboAttachmentType::All);
    mpRenderContext->clearFbo(mpTempFB.get(), kClearColor, 1.0f, 0, FboAttachmentType::All);

    if(mpSceneRenderer)
    {
        mpEditor->update(mCurrentTime);
        mpSceneRenderer->update(mCurrentTime);

        switch(mRenderMode)
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

    std::string message = getFpsMsg();
    message += "\nFrame counter: " + std::to_string(frameCount);

    renderText(message, glm::vec2(10, 10));

    frameCount++;
}

bool StereoRendering::onKeyEvent(const KeyboardEvent& keyEvent)
{
    if (keyEvent.key == KeyboardEvent::Key::LeftAlt && keyEvent.type == KeyboardEvent::Type::KeyReleased) {
        mpDebugFoveation = !mpDebugFoveation;
    }
    if (keyEvent.key == KeyboardEvent::Key::R && keyEvent.type == KeyboardEvent::Type::KeyReleased) {
        mpDebugViz = !mpDebugViz;
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
    config.windowDesc.height = 1024;
    config.windowDesc.width = 1600;
    config.windowDesc.resizableWindow = true;
    config.deviceDesc.enableVR = true;
    config.deviceDesc.enableVsync = false;
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
