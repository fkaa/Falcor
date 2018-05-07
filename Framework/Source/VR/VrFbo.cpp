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
#include "Framework.h"
#include "VrFbo.h"

#include "Graphics/FboHelper.h"
#include "API/Texture.h"

#ifndef NO_FOVE
#include "OpenVR/VRSystem.h"
#include "OpenVR/VRDisplay.h"
#else
#include "FoveVR/VRSystem.h"
#include "FoveVR/VRDisplay.h"
#endif

namespace Falcor
{
    glm::ivec2 getHmdRenderSize()
    {
        VRSystem* pVrSystem = VRSystem::instance();
        VRDisplay* pDisplay = pVrSystem->getHMD().get();
        glm::ivec2 renderSize = pDisplay->getRecommendedRenderSize();

        // HACK: FOVE returns full display, not eye
        renderSize.y /= 2;
        int temp = renderSize.x;
        renderSize.x = renderSize.y;
        renderSize.y = temp;
        return renderSize;
    }

    VrFbo::UniquePtr VrFbo::create(const Fbo::Desc& desc, uint32_t width, uint32_t height)
    {
        width = (width == 0) ? getHmdRenderSize().x : width;
        height = (height == 0) ? getHmdRenderSize().y : height;

        // Create the FBO
        VrFbo::UniquePtr pVrFbo = std::make_unique<VrFbo>();

        Fbo::SharedPtr pFbo = Fbo::create();

        auto samples = desc.getSampleCount();

        Texture::BindFlags flags = FboHelper::getBindFlags(false, desc.isColorTargetUav(0));
        Texture::SharedPtr pTex = FboHelper::createTexture2D(width, height, desc.getColorTargetFormat(0), samples, 2, 1, flags);
        pFbo->attachColorTarget(pTex, 0, 0, 0, Fbo::kAttachEntireMipLevel);

        Texture::BindFlags dflags = FboHelper::getBindFlags(true, desc.isDepthStencilUav());
        Texture::SharedPtr pDepth = FboHelper::createTexture2D(width, height, desc.getDepthStencilFormat(), samples, 2, 1, dflags);
        pFbo->attachDepthStencilTarget(pDepth, 0, 0, Fbo::kAttachEntireMipLevel);


        pVrFbo->mpFbo = pFbo;

        // create the textures
        // in the future we should use SRVs directly
        // or some other way to avoid copying resources

        pVrFbo->mpLeftView = Texture::create2D(width, height, desc.getColorTargetFormat(0),1,1);
        pVrFbo->mpResolveLeft = Fbo::create();
        pVrFbo->mpResolveLeft->attachColorTarget(pVrFbo->mpLeftView, 0, 0, 0);
        pVrFbo->mpRightView = Texture::create2D(width, height, desc.getColorTargetFormat(0),1,1);
        pVrFbo->mpResolveRight = Fbo::create();
        pVrFbo->mpResolveRight->attachColorTarget(pVrFbo->mpRightView, 0, 0, 0);

        Fbo::SharedPtr pFboDouble = Fbo::create();
        pFboDouble->attachColorTarget(pVrFbo->mpLeftView, 0, 0, 0, 1);
        pFboDouble->attachColorTarget(pVrFbo->mpRightView, 1, 0, 0, 1);
        pVrFbo->mpFboDouble = pFboDouble;

        return pVrFbo;
    }
    
    void VrFbo::unwrap(RenderContext* pRenderCtx) const
    {
        /*uint32_t ltSrcSubresourceIdx = mpFbo->getColorTexture(0)->getSubresourceIndex(0, 0);
        uint32_t rtSrcSubresourceIdx = mpFbo->getColorTexture(0)->getSubresourceIndex(1, 0);

        uint32_t ltDstSubresourceIdx = mpLeftView->getSubresourceIndex(0, 0);
        uint32_t rtDstSubresourceIdx = mpRightView->getSubresourceIndex(0, 0);

        pRenderCtx->copySubresource(mpLeftView.get(), ltDstSubresourceIdx, mpFbo->getColorTexture(0).get(), ltSrcSubresourceIdx);
        pRenderCtx->copySubresource(mpRightView.get(), rtDstSubresourceIdx, mpFbo->getColorTexture(0).get(), rtSrcSubresourceIdx);*/

        pRenderCtx->blit(mpFbo->getColorTexture(0)->getSRV(0, 1, 0, 1), mpResolveLeft->getRenderTargetView(0));
        pRenderCtx->blit(mpFbo->getColorTexture(0)->getSRV(0, 1, 1, 1), mpResolveRight->getRenderTargetView(0));
    }

    void VrFbo::submitToHmd(RenderContext* pRenderCtx) const
    {
        VRSystem* pVrSystem = VRSystem::instance();

        /*uint32_t ltSrcSubresourceIdx = mpFbo->getColorTexture(0)->getSubresourceIndex(0, 0);
        uint32_t rtSrcSubresourceIdx = mpFbo->getColorTexture(0)->getSubresourceIndex(1, 0);

        uint32_t ltDstSubresourceIdx = mpLeftView->getSubresourceIndex(0, 0);
        uint32_t rtDstSubresourceIdx = mpRightView->getSubresourceIndex(0, 0);

        pRenderCtx->copySubresource(mpLeftView.get(),  ltDstSubresourceIdx, mpFbo->getColorTexture(0).get(), ltSrcSubresourceIdx);
        pRenderCtx->copySubresource(mpRightView.get(), rtDstSubresourceIdx, mpFbo->getColorTexture(0).get(), rtSrcSubresourceIdx);*/

        pVrSystem->submit(VRDisplay::Eye::Left, mpLeftView, pRenderCtx);
        pVrSystem->submit(VRDisplay::Eye::Right, mpRightView, pRenderCtx);
    }
}