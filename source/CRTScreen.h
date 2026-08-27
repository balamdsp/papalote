#pragma once

#include <JuceHeader.h>

#include "CRTMath.h"
#include "CRTNoise.h"

// OpenGL (GPU) port of the cool-retro-term shader pipeline (static + dynamic
// + frame + burn-in passes). Shaders are embedded GLSL 1.20 -- JUCE's default
// Windows context is a legacy 2.1 compatibility profile.
class CRTScreen : public Component, public OpenGLRenderer, private Timer
{
public:
    explicit CRTScreen (Component* screenSourceToFilter, const std::atomic<bool>* enabledSource = nullptr)
        : screenSource (screenSourceToFilter),
          enabledFlag (enabledSource),
          lastEnabled (enabledSource == nullptr || enabledSource->load())
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (true);

        openGLContext.setComponentPaintingEnabled (false);
    }

    ~CRTScreen() override
    {
        detachGL();
    }

    void parentHierarchyChanged() override
    {
        if (getParentComponent() != nullptr && ! glAttached)
        {
            openGLContext.setRenderer (this);
            openGLContext.attachTo (*this);
            startTimerHz (30);
            glAttached = true;
        }
        else if (getParentComponent() == nullptr && glAttached)
        {
            detachGL();
        }
    }

    void paint (Graphics&) override {}

    void resized() override {}

    void setBloomIntensity (float amount) noexcept       { bloomIntensity = jlimit (0.0f, 1.0f, amount); }
    void setScanlineIntensity (float amount) noexcept    { scanlineIntensity = jlimit (0.0f, 1.0f, amount); }
    void setGlowingLineIntensity (float amount) noexcept { glowingLineIntensity = jlimit (0.0f, 1.0f, amount); }
    void setFlickerIntensity (float amount) noexcept     { flickerIntensity = jlimit (0.0f, 1.0f, amount); }

    void setBurnInIntensity (float amount) noexcept        { burnInIntensity = jlimit (0.0f, 1.0f, amount); }
    void setCurvatureIntensity (float amount) noexcept     { curvatureIntensity = jlimit (0.0f, 1.0f, amount); }
    void setStaticNoiseIntensity (float amount) noexcept   { staticNoiseIntensity = jlimit (0.0f, 0.5f, amount); }
    void setRgbShiftIntensity (float amount) noexcept      { rgbShiftIntensity = jlimit (0.0f, 1.0f, amount); }
    void setJitterIntensity (float amount) noexcept        { jitterIntensity = jlimit (0.0f, 1.0f, amount); }
    void setHorizontalSyncIntensity (float amount) noexcept { horizontalSyncIntensity = jlimit (0.0f, 1.0f, amount); }
    void setAmbientLight (float amount) noexcept           { ambientLight = jlimit (0.0f, 1.0f, amount); }
    void setFrameShininess (float amount) noexcept         { frameShininess = jlimit (0.0f, 1.0f, amount); }
    void setJitterYScale (float amount) noexcept           { jitterYScale = jlimit (0.0f, 2.0f, amount); }

    float getBloomIntensity() const noexcept              { return bloomIntensity; }
    float getScanlineIntensity() const noexcept           { return scanlineIntensity; }
    float getGlowingLineIntensity() const noexcept        { return glowingLineIntensity; }
    float getFlickerIntensity() const noexcept            { return flickerIntensity; }

    float getBurnInIntensity() const noexcept             { return burnInIntensity; }
    float getCurvatureIntensity() const noexcept          { return curvatureIntensity; }
    float getStaticNoiseIntensity() const noexcept        { return staticNoiseIntensity; }
    float getRgbShiftIntensity() const noexcept           { return rgbShiftIntensity; }
    float getJitterIntensity() const noexcept             { return jitterIntensity; }
    float getHorizontalSyncIntensity() const noexcept     { return horizontalSyncIntensity; }
    float getAmbientLight() const noexcept                { return ambientLight; }
    float getFrameShininess() const noexcept              { return frameShininess; }
    float getJitterYScale() const noexcept               { return jitterYScale; }

    bool isCrtEnabled() const noexcept { return enabledFlag == nullptr || enabledFlag->load(); }

    static constexpr float getFrameSize() noexcept { return kFrameSize; }

private:
    // Per-frame snapshot produced on the UI thread (timerCallback) and
    // consumed by the GL render thread. Must be declared before the members
    // it is initialized from.
    struct Params
    {
        float bloom = 0.0f;
        float scanline = 0.0f;
        float glowingLine = 0.0f;
        float flicker = 0.0f;
        float burnIn = 0.0f;
        float curvature = 0.0f;
        float staticNoise = 0.0f;
        float rgbShift = 0.0f;
        float jitter = 0.0f;
        float jitterYScale = 0.0f;
        float horizontalSync = 0.0f;
        float ambientLight = 0.0f;
        float frameShininess = 0.0f;
    };

    void newOpenGLContextCreated() override
    {
        gl::glViewport (0, 0, getWidth(), getHeight());
        gl::glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
        gl::glClear (gl::GL_COLOR_BUFFER_BIT);

        buildShaders();
    }

    void openGLContextClosing() override
    {
        releaseGLResources();
    }

    void renderOpenGL() override
    {
        if (! openGLContext.isActive())
            return;

        const int w = getWidth();
        const int h = getHeight();
        if (screenSource == nullptr || w <= 0 || h <= 0)
            return;

        if (! shadersReady())
            return;

        Params renderParams;
        Image snapToUpload;
        {
            const ScopedLock sl (sharedLock);
            renderParams = params;
            if (snapshotPending)
            {
                snapToUpload = uiSnapshot;
                snapshotPending = false;
            }
        }

        if (snapToUpload.isValid())
            uiTexture.loadImage (snapToUpload);

        ensureNoiseTexture();
        ensureFramebuffers (w, h);

        gl::glViewport (0, 0, w, h);
        gl::glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
        gl::glClear (gl::GL_COLOR_BUFFER_BIT);

        if (! framebuffersReady() || uiTexture.getTextureID() == 0)
            return;

        const float timeSec = (float) (Time::getMillisecondCounterHiRes() * 0.001);
        const float prevTime = lastFrameTime;
        lastFrameTime = timeSec;

        if (isCrtEnabled())
        {
            renderBloomPass (renderParams);
            renderStaticPass (renderParams);
            renderFramePass (renderParams);
            renderDynamicPass (renderParams, timeSec, prevTime);
            renderBurnInPass (renderParams, timeSec, prevTime);
            presentScene (renderParams, timeSec);
        }
        else
        {
            presentRawUI();
        }
    }

    void detachGL()
    {
        if (glAttached)
        {
            stopTimer();
            openGLContext.detach();
            openGLContext.setRenderer (nullptr);
            glAttached = false;
        }
    }

    bool shadersReady() const
    {
        return bloomProgram && staticProgram && dynamicProgram && burnInProgram
            && frameProgram && presentProgram && rawUIProgram;
    }

    bool framebuffersReady() const
    {
        return bloomFBO != nullptr && screenFBO != nullptr && dynamicFBO != nullptr
            && frameFBO != nullptr && burnFBO[0] != nullptr && burnFBO[1] != nullptr;
    }

    std::unique_ptr<OpenGLShaderProgram> createProgram (const char* vs, const char* fs)
    {
        auto program = std::make_unique<OpenGLShaderProgram> (openGLContext);
        program->addVertexShader (vs);
        program->addFragmentShader (fs);

        if (program->link())
            return program;

        Logger::writeToLog ("Papalote CRT shader link failed: " + program->getLastError());
        return nullptr;
    }

    void buildShaders()
    {
        bloomProgram   = createProgram (kVertexShader,       kBloomFragment);
        staticProgram  = createProgram (kVertexShader,       kStaticFragment);
        frameProgram   = createProgram (kVertexShader,       kFrameFragment);
        burnInProgram  = createProgram (kVertexShader,       kBurnInFragment);
        presentProgram = createProgram (kVertexShader,       kPresentFragment);
        rawUIProgram   = createProgram (kVertexShader,       kRawUIFragment);
        dynamicProgram = createProgram (kDynamicVertexShader, kDynamicFragment);
    }

    void releaseGLResources()
    {
        bloomProgram.reset();
        staticProgram.reset();
        dynamicProgram.reset();
        burnInProgram.reset();
        frameProgram.reset();
        presentProgram.reset();
        rawUIProgram.reset();

        bloomFBO.reset();
        screenFBO.reset();
        dynamicFBO.reset();
        frameFBO.reset();
        burnFBO[0].reset();
        burnFBO[1].reset();
        burnRead = 0;
        burnWrite = 1;

        uiTexture.release();
        noiseTexture.release();
        lastFrameTime = 0.0f;
    }

    void ensureNoiseTexture()
    {
        if (noiseTexture.getTextureID() != 0)
            return;

        Image tile;
        crt::buildNoiseTile (tile);
        noiseTileImage = tile;
        noiseTexture.loadImage (tile);

        gl::glBindTexture (gl::GL_TEXTURE_2D, noiseTexture.getTextureID());
        gl::glTexParameteri (gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_S, gl::GL_REPEAT);
        gl::glTexParameteri (gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_T, gl::GL_REPEAT);
        gl::glBindTexture (gl::GL_TEXTURE_2D, 0);
    }

    // Per-frame "vertex pass" computed on the CPU: no vertex texture fetch,
    // so no driver-dependent flicker.
    void computeVertexPass (float timeSec, float flickering, float horizontalSync,
                            float horizontalSyncStrength,
                            float& vBrightness, float& vDistortionScale, float& vDistortionFreq) const
    {
        vBrightness = 1.0f;
        vDistortionScale = 0.0f;
        vDistortionFreq = 0.0f;

        if (noiseTileImage.getWidth() <= 0)
            return;

        const float vx = crt::fractF (timeSec / 2.048f) * (float) crt::crtNoiseTileSize;
        const float vy = crt::fractF (timeSec / 1048.576f) * (float) crt::crtNoiseTileSize;

        const Image::BitmapData noise (noiseTileImage, Image::BitmapData::readOnly);
        const int size = crt::crtNoiseTileSize;
        const int x0 = (((int) std::floor (vx)) % size + size) % size;
        const int y0 = (((int) std::floor (vy)) % size + size) % size;
        const int x1 = (x0 + 1) % size;
        const int y1 = (y0 + 1) % size;
        const float tx = crt::fractF (vx);
        const float ty = crt::fractF (vy);

        const PixelARGB* r0 = reinterpret_cast<const PixelARGB*> (noise.getLinePointer (y0));
        const PixelARGB* r1 = reinterpret_cast<const PixelARGB*> (noise.getLinePointer (y1));
        const float g00 = (float) r0[x0].getGreen() / 255.0f;
        const float g10 = (float) r0[x1].getGreen() / 255.0f;
        const float g01 = (float) r1[x0].getGreen() / 255.0f;
        const float g11 = (float) r1[x1].getGreen() / 255.0f;
        const float g01b = crt::mixF (crt::mixF (g00, g10, tx), crt::mixF (g01, g11, tx), ty);

        const float r00 = (float) r0[x0].getRed() / 255.0f;
        const float r10 = (float) r0[x1].getRed() / 255.0f;
        const float r01 = (float) r1[x0].getRed() / 255.0f;
        const float r11 = (float) r1[x1].getRed() / 255.0f;
        const float r01b = crt::mixF (crt::mixF (r00, r10, tx), crt::mixF (r01, r11, tx), ty);

        vBrightness = 1.0f + (g01b - 0.5f) * flickering;

        const float randval = horizontalSyncStrength - r01b;
        vDistortionScale = crt::stepF (0.0f, randval) * randval * horizontalSyncStrength * horizontalSync;
        vDistortionFreq  = crt::mixF (4.0f, 40.0f, g01b) * crt::stepF (0.0f, horizontalSync);
    }

    void ensureFramebuffers (int w, int h)
    {
        const int bw = jmax (16, roundToInt (w * 0.25f));
        const int bh = jmax (16, roundToInt (h * 0.25f));

        auto recreate = [&] (std::unique_ptr<OpenGLFrameBuffer>& fb, int fw, int fh)
        {
            if (fb != nullptr && fb->getWidth() == fw && fb->getHeight() == fh)
                return;

            auto created = std::make_unique<OpenGLFrameBuffer>();
            if (created->initialise (openGLContext, fw, fh))
            {
                created->makeCurrentAndClear();
                created->releaseAsRenderingTarget();
                fb = std::move (created);
            }
        };

        recreate (bloomFBO,  bw, bh);
        recreate (screenFBO, w, h);
        recreate (dynamicFBO, w, h);
        recreate (frameFBO,  w, h);
        recreate (burnFBO[0], w, h);
        recreate (burnFBO[1], w, h);
    }

    static void drawFullscreen (const OpenGLShaderProgram& program, bool flipped)
    {
        static const float quads[8][4] =
        {
            { -1.0f, -1.0f, 0.0f, 1.0f },
            {  1.0f, -1.0f, 1.0f, 1.0f },
            { -1.0f,  1.0f, 0.0f, 0.0f },
            {  1.0f,  1.0f, 1.0f, 0.0f },
            { -1.0f, -1.0f, 0.0f, 0.0f },
            {  1.0f, -1.0f, 1.0f, 0.0f },
            { -1.0f,  1.0f, 0.0f, 1.0f },
            {  1.0f,  1.0f, 1.0f, 1.0f },
        };

        const float* p = quads[flipped ? 4 : 0];
        const int stride = 4 * (int) sizeof (float);

        const OpenGLShaderProgram::Attribute position (program, "position");
        const OpenGLShaderProgram::Attribute texCoord (program, "texCoord");

        gl::glEnableVertexAttribArray (position.attributeID);
        gl::glEnableVertexAttribArray (texCoord.attributeID);
        gl::glVertexAttribPointer (position.attributeID, 2, gl::GL_FLOAT, gl::GL_FALSE, stride, p);
        gl::glVertexAttribPointer (texCoord.attributeID, 2, gl::GL_FLOAT, gl::GL_FALSE, stride, p + 2);
        gl::glDrawArrays (gl::GL_TRIANGLE_STRIP, 0, 4);
        gl::glDisableVertexAttribArray (position.attributeID);
        gl::glDisableVertexAttribArray (texCoord.attributeID);
    }

    static void bindTextureUnit (int unit, GLuint id)
    {
        gl::glActiveTexture (gl::GL_TEXTURE0 + unit);
        gl::glBindTexture (gl::GL_TEXTURE_2D, id);
    }

    static void bindSampler (const OpenGLShaderProgram& program, const char* name, int unit, GLuint id)
    {
        OpenGLShaderProgram::Uniform (program, name).set (unit);
        bindTextureUnit (unit, id);
    }

    void renderBloomPass (const Params& p)
    {
        bloomFBO->makeCurrentRenderingTarget();
        gl::glViewport (0, 0, bloomFBO->getWidth(), bloomFBO->getHeight());

        bloomProgram->use();
        bindSampler (*bloomProgram, "source", 0, uiTexture.getTextureID());
        OpenGLShaderProgram::Uniform (*bloomProgram, "pixelSize")
            .set (1.0f / (float) bloomFBO->getWidth(), 1.0f / (float) bloomFBO->getHeight());
        drawFullscreen (*bloomProgram, true);

        bloomFBO->releaseAsRenderingTarget();
    }

    void renderStaticPass (const Params& p)
    {
        screenFBO->makeCurrentRenderingTarget();
        gl::glViewport (0, 0, screenFBO->getWidth(), screenFBO->getHeight());

        staticProgram->use();
        bindSampler (*staticProgram, "source", 0, uiTexture.getTextureID());
        bindSampler (*staticProgram, "bloomSource", 1, bloomFBO->getTextureID());
        OpenGLShaderProgram::Uniform (*staticProgram, "screenCurvature").set (p.curvature * kCurvatureScale);
        OpenGLShaderProgram::Uniform (*staticProgram, "rgbShift")
            .set (p.rgbShift * (4.0f / (float) screenFBO->getWidth()));
        OpenGLShaderProgram::Uniform (*staticProgram, "screenBrightness").set (1.0f);
        OpenGLShaderProgram::Uniform (*staticProgram, "bloom").set (p.bloom * 2.5f);
        OpenGLShaderProgram::Uniform (*staticProgram, "frameSize").set (kFrameSize);
        OpenGLShaderProgram::Uniform (*staticProgram, "frameShininess").set (p.frameShininess);
        drawFullscreen (*staticProgram, true);

        screenFBO->releaseAsRenderingTarget();
    }

    void renderFramePass (const Params& p)
    {
        frameFBO->makeCurrentRenderingTarget();
        gl::glViewport (0, 0, frameFBO->getWidth(), frameFBO->getHeight());

        frameProgram->use();
        OpenGLShaderProgram::Uniform (*frameProgram, "screenCurvature").set (p.curvature * kCurvatureScale);
        OpenGLShaderProgram::Uniform (*frameProgram, "frameSize").set (kFrameSize);
        OpenGLShaderProgram::Uniform (*frameProgram, "screenRadius").set (kScreenRadius);
        OpenGLShaderProgram::Uniform (*frameProgram, "viewportSize")
            .set ((float) frameFBO->getWidth(), (float) frameFBO->getHeight());
        OpenGLShaderProgram::Uniform (*frameProgram, "ambientLight").set (p.ambientLight);
        OpenGLShaderProgram::Uniform (*frameProgram, "frameShininess").set (p.frameShininess);

        const float lightR = crt::mixF (kFontColorR, kBackgroundColorR, 0.2f);
        const float lightG = crt::mixF (kFontColorG, kBackgroundColorG, 0.2f);
        const float lightB = crt::mixF (kFontColorB, kBackgroundColorB, 0.2f);
        const float staticFrame = 0.12f;
        const float t = 0.125f + 0.75f * p.ambientLight;
        OpenGLShaderProgram::Uniform (*frameProgram, "frameColor").set (crt::mixF (lightR * 0.2f, staticFrame, t),
                                                                         crt::mixF (lightG * 0.2f, staticFrame, t),
                                                                         crt::mixF (lightB * 0.2f, staticFrame, t),
                                                                         1.0f);
        drawFullscreen (*frameProgram, false);

        frameFBO->releaseAsRenderingTarget();
    }

    void renderDynamicPass (const Params& p, float timeSec, float prevTime)
    {
        dynamicFBO->makeCurrentRenderingTarget();
        gl::glViewport (0, 0, dynamicFBO->getWidth(), dynamicFBO->getHeight());

        const int w = dynamicFBO->getWidth();
        const int h = dynamicFBO->getHeight();
        const float curvature = p.curvature * kCurvatureScale;
        const float burnInTime = 1.0f / crt::lint (0.16f, 1.6f, p.burnIn);
        const float bloomU = p.bloom * 2.5f;

        dynamicProgram->use();

        float vBrightness = 1.0f;
        float vDistortionScale = 0.0f;
        float vDistortionFreq = 0.0f;
        computeVertexPass (timeSec, p.flicker, p.horizontalSync,
                           crt::lint (0.05f, 0.35f, p.horizontalSync),
                           vBrightness, vDistortionScale, vDistortionFreq);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "uBrightness").set (vBrightness);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "uDistortionScale").set (vDistortionScale);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "uDistortionFreq").set (vDistortionFreq);

        OpenGLShaderProgram::Uniform (*dynamicProgram, "time").set (timeSec);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "flickering").set (p.flicker);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "horizontalSync").set (p.horizontalSync);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "horizontalSyncStrength")
            .set (crt::lint (0.05f, 0.35f, p.horizontalSync));

        OpenGLShaderProgram::Uniform (*dynamicProgram, "screenCurvature").set (curvature);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "frameSize").set (kFrameSize);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "staticNoise").set (p.staticNoise);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "glowingLine").set (p.glowingLine * 0.2f);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "jitterDisplacement")
            .set (0.007f * p.jitter, 0.002f * p.jitter * p.jitterYScale);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "jitter").set (p.jitter);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "rasterizationIntensity").set (p.scanline);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "bloom").set (bloomU);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "burnInTime").set (burnInTime);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "burnInLastUpdate").set (prevTime);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "virtualResolution").set ((float) w * 0.25f, (float) h * 0.25f);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "scaleNoiseSize")
            .set ((float) w * 0.75f / (float) crt::crtNoiseTileSize,
                  (float) h * 0.75f / (float) crt::crtNoiseTileSize);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "fontColor").set (kFontColorR, kFontColorG, kFontColorB, 1.0f);
        OpenGLShaderProgram::Uniform (*dynamicProgram, "backgroundColor")
            .set (kBackgroundColorR, kBackgroundColorG, kBackgroundColorB, 1.0f);

        bindSampler (*dynamicProgram, "noiseSource", 0, noiseTexture.getTextureID());
        bindSampler (*dynamicProgram, "screenBuffer", 1, screenFBO->getTextureID());
        bindSampler (*dynamicProgram, "burnInSource", 2, burnFBO[burnRead]->getTextureID());
        bindSampler (*dynamicProgram, "frameSource", 3, frameFBO->getTextureID());

        drawFullscreen (*dynamicProgram, true);

        dynamicFBO->releaseAsRenderingTarget();
    }

    void renderBurnInPass (const Params& p, float timeSec, float prevTime)
    {
        burnFBO[burnWrite]->makeCurrentRenderingTarget();
        gl::glViewport (0, 0, burnFBO[burnWrite]->getWidth(), burnFBO[burnWrite]->getHeight());

        burnInProgram->use();
        OpenGLShaderProgram::Uniform (*burnInProgram, "burnInLastUpdate").set (timeSec);
        OpenGLShaderProgram::Uniform (*burnInProgram, "prevLastUpdate").set (prevTime);
        OpenGLShaderProgram::Uniform (*burnInProgram, "burnInTime")
            .set (1.0f / crt::lint (0.16f, 1.6f, p.burnIn));
        bindSampler (*burnInProgram, "txt_source", 0, dynamicFBO->getTextureID());
        bindSampler (*burnInProgram, "burnInSource", 1, burnFBO[burnRead]->getTextureID());
        drawFullscreen (*burnInProgram, true);

        burnFBO[burnWrite]->releaseAsRenderingTarget();
        std::swap (burnRead, burnWrite);
    }

    void presentScene (const Params& p, float timeSec)
    {
        gl::glViewport (0, 0, getWidth(), getHeight());

        presentProgram->use();
        OpenGLShaderProgram::Uniform (*presentProgram, "time").set (timeSec);
        OpenGLShaderProgram::Uniform (*presentProgram, "staticNoise").set (p.staticNoise);
        OpenGLShaderProgram::Uniform (*presentProgram, "scaleNoiseSize")
            .set ((float) getWidth() * 0.75f / (float) crt::crtNoiseTileSize,
                  (float) getHeight() * 0.75f / (float) crt::crtNoiseTileSize);
        bindSampler (*presentProgram, "dynamicTexture", 0, dynamicFBO->getTextureID());
        bindSampler (*presentProgram, "frameTexture", 1, frameFBO->getTextureID());
        bindSampler (*presentProgram, "noiseSource", 2, noiseTexture.getTextureID());
        drawFullscreen (*presentProgram, true);
    }

    void presentRawUI()
    {
        gl::glViewport (0, 0, getWidth(), getHeight());

        rawUIProgram->use();
        bindSampler (*rawUIProgram, "uiTexture", 0, uiTexture.getTextureID());
        drawFullscreen (*rawUIProgram, true);
    }

    void timerCallback() override
    {
        if (! isVisible() || screenSource == nullptr)
            return;
        if (screenSource->getWidth() <= 0 || screenSource->getHeight() <= 0)
            return;

        Image snap = screenSource->createComponentSnapshot (getLocalBounds(), false, 1.0f);
        if (! snap.isValid())
            return;

        const AffineTransform screenTransform = screenSource->getTransform();
        if (! screenTransform.isIdentity())
        {
            Image squeezed (Image::ARGB, snap.getWidth(), snap.getHeight(), true);
            Graphics g (squeezed);
            g.addTransform (screenTransform);
            g.drawImageAt (snap, 0, 0);
            snap = squeezed;
        }

        Params current;
        current.bloom              = bloomIntensity;
        current.scanline           = scanlineIntensity;
        current.glowingLine        = glowingLineIntensity;
        current.flicker            = flickerIntensity;
        current.burnIn             = burnInIntensity;
        current.curvature          = curvatureIntensity;
        current.staticNoise        = staticNoiseIntensity;
        current.rgbShift           = rgbShiftIntensity;
        current.jitter             = jitterIntensity;
        current.jitterYScale       = jitterYScale;
        current.horizontalSync     = horizontalSyncIntensity;
        current.ambientLight       = ambientLight;
        current.frameShininess     = frameShininess;

        {
            const ScopedLock sl (sharedLock);
            uiSnapshot = snap;
            params = current;
            snapshotPending = true;
        }

        const bool enabled = isCrtEnabled();
        if (enabled != lastEnabled)
        {
            lastEnabled = enabled;
            if (auto* parent = getParentComponent())
                juce::MessageManager::callAsync ([parent = juce::Component::SafePointer<juce::Component> (parent)]()
                                                 { if (parent != nullptr) parent->resized(); });
        }

        if (openGLContext.isAttached())
            openGLContext.triggerRepaint();
    }

    static constexpr const char* kVertexShader = R"(
#version 120
attribute vec2 position;
attribute vec2 texCoord;
varying vec2 vTexCoord;
void main()
{
    vTexCoord = texCoord;
    gl_Position = vec4 (position, 0.0, 1.0);
}
)";

    static constexpr const char* kDynamicVertexShader = R"(
#version 120
attribute vec2 position;
attribute vec2 texCoord;

uniform float uBrightness;
uniform float uDistortionScale;
uniform float uDistortionFreq;

varying vec2 vTexCoord;
varying float vBrightness;
varying float vDistortionScale;
varying float vDistortionFreq;

void main()
{
    vTexCoord = texCoord;
    vBrightness = uBrightness;
    vDistortionScale = uDistortionScale;
    vDistortionFreq = uDistortionFreq;
    gl_Position = vec4 (position, 0.0, 1.0);
}
)";

    static constexpr const char* kBloomFragment = R"(
#version 120
varying vec2 vTexCoord;
uniform sampler2D source;
uniform vec2 pixelSize;

float rgb2grey (vec3 v) { return dot (v, vec3 (0.21, 0.72, 0.04)); }

void main()
{
    vec3 sum = vec3 (0.0);
    float wSum = 0.0;
    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            float d2 = float (x * x + y * y);
            float w = exp (-d2 / 8.0);
            sum += texture2D (source, vTexCoord + vec2 (x, y) * pixelSize).rgb * w;
            wSum += w;
        }
    }
    vec3 blurred = sum / wSum;
    gl_FragColor = vec4 (blurred, rgb2grey (blurred));
}
)";

    static constexpr const char* kStaticFragment = R"(
#version 120
varying vec2 vTexCoord;

uniform float screenCurvature;
uniform float rgbShift;
uniform float screenBrightness;
uniform float bloom;
uniform float frameSize;
uniform float frameShininess;
uniform sampler2D source;
uniform sampler2D bloomSource;

float rand2 (vec2 v) { return fract (sin (dot (v, vec2 (12.9898, 78.233))) * 43758.5453); }

// Barrel only, NO bezel padding: the UI is already squeezed into
// [frameSize, 1-frameSize] by the screen transform, so the sample here is
// 1:1 with the window. Only the residual mild curvature remains.
vec2 distortCoordinates (vec2 coords)
{
    vec2 cc = (coords - vec2 (0.5));
    float dist = dot (cc, cc) * screenCurvature;
    return (coords + cc * (1.0 + dist) * dist);
}

void main()
{
    vec2 curvatureCoords = distortCoordinates (vTexCoord);
    vec2 txt_coords = clamp (curvatureCoords, vec2 (0.0), vec2 (1.0));

    // Reflection detection (upstream terminal_static.frag logic, adapted to
    // Boreal's [pad, 1-pad] content boundary): isReflection = 1.0 when
    // exactly one axis is outside the tube (straight edges), 0.0 when neither
    // (on-screen) or both (corner) axes are outside.
    float pad = frameSize;
    float inRangeX = step (pad, curvatureCoords.x) - step (1.0 - pad, curvatureCoords.x);
    float inRangeY = step (pad, curvatureCoords.y) - step (1.0 - pad, curvatureCoords.y);
    float isReflection = abs (inRangeX - inRangeY);

    // Mirror-wrap curvature-distorted coords across the tube boundary so
    // the reflection samples content just inside the opposite edge.
    vec2 mirr;
    mirr.x = curvatureCoords.x < pad        ? 2.0 * pad - curvatureCoords.x
           : (curvatureCoords.x > 1.0 - pad ? 2.0 * (1.0 - pad) - curvatureCoords.x
                                             : curvatureCoords.x);
    mirr.y = curvatureCoords.y < pad        ? 2.0 * pad - curvatureCoords.y
           : (curvatureCoords.y > 1.0 - pad ? 2.0 * (1.0 - pad) - curvatureCoords.y
                                             : curvatureCoords.y);
    vec2 reflCoords = clamp (vec2 (mirr.x, mirr.y), vec2 (0.0), vec2 (1.0));

    vec3 txt_color = texture2D (source, txt_coords).rgb;

    vec2 displacement = vec2 (rgbShift, 0.0);
    vec3 rightColor = texture2D (source, clamp (txt_coords + displacement, vec2 (0.0), vec2 (1.0))).rgb;
    vec3 leftColor = texture2D (source, clamp (txt_coords - displacement, vec2 (0.0), vec2 (1.0))).rgb;
    txt_color.r = leftColor.r * 0.10 + rightColor.r * 0.30 + txt_color.r * 0.60;
    txt_color.g = leftColor.g * 0.20 + rightColor.g * 0.20 + txt_color.g * 0.60;
    txt_color.b = leftColor.b * 0.30 + rightColor.b * 0.10 + txt_color.b * 0.60;

    vec3 finalColor = txt_color;

    vec4 bloomFullColor = texture2D (bloomSource, txt_coords);
    vec3 bloomColor = bloomFullColor.rgb;
    float bloomAlpha = bloomFullColor.a;

    finalColor += clamp (bloomColor * bloom * bloomAlpha, 0.0, 0.5);
    float bloomScale = 1.0 + max (bloom, 0.0);
    finalColor /= bloomScale;

    // Bezel reflection (upstream terminal_static.frag lines 87-90):
    // reflectionColor = mix(bloom*bloomAlpha*2, finalColor, shininess*0.5),
    // applied only in the isReflection zone.  The bloom sample uses the
    // mirror-wrapped coords so the reflected glow matches the mirrored edge.
    vec4 bloomReflColor = texture2D (bloomSource, reflCoords);
    vec3 reflectionColor = mix (bloomReflColor.rgb * bloomReflColor.a * 2.0,
                                finalColor, frameShininess * 0.5);
    finalColor = mix (finalColor, reflectionColor, isReflection);

    finalColor *= screenBrightness;

    float noise = rand2 (vTexCoord) - 0.5;
    finalColor = clamp (finalColor + vec3 (noise * 0.025), 0.0, 1.0);

    gl_FragColor = vec4 (finalColor, 1.0);
}
)";

    static constexpr const char* kFrameFragment = R"(
#version 120
varying vec2 vTexCoord;

uniform float screenCurvature;
uniform vec4 frameColor;
uniform float frameSize;
uniform float screenRadius;
uniform vec2 viewportSize;
uniform float ambientLight;
uniform float frameShininess;

float min2 (vec2 v) { return min (v.x, v.y); }
float prod2 (vec2 v) { return v.x * v.y; }
float rand2 (vec2 v) { return fract (sin (dot (v, vec2 (12.9898, 78.233))) * 43758.5453); }

// Same no-padding barrel as the static pass: the tube SDF here lives in the
// same coordinate space as the 1:1 screen content, so the bezel band aligns
// with the content edge.
vec2 distortCoordinates (vec2 coords)
{
    vec2 cc = (coords - vec2 (0.5));
    float dist = dot (cc, cc) * screenCurvature;
    return (coords + cc * (1.0 + dist) * dist);
}

float roundedRectSdfPixels (vec2 p, vec2 topLeft, vec2 bottomRight, float radiusPixels)
{
    vec2 sizePixels = (bottomRight - topLeft) * viewportSize;
    vec2 centerPixels = (topLeft + bottomRight) * 0.5 * viewportSize;
    vec2 localPixels = p * viewportSize - centerPixels;
    vec2 halfSize = sizePixels * 0.5 - vec2 (radiusPixels);
    vec2 d = abs (localPixels) - halfSize;
    return length (max (d, vec2 (0.0))) + min (max (d.x, d.y), 0.0) - radiusPixels;
}

void main()
{
    vec2 coords = distortCoordinates (vTexCoord);

    // The tube = the inset rect [frameSize, 1-frameSize], mirroring the
    // screen transform that squeezes the UI content into that same band.
    float pad = frameSize;
    vec2 tubeTopLeft = vec2 (pad);
    vec2 tubeBottomRight = vec2 (1.0 - pad);

    float screenRadiusPixels = screenRadius;
    float edgeSoftPixels = 1.0;

    float seamWidth = max (screenRadiusPixels, 0.5) / min2 (viewportSize);

    float e = min (smoothstep (-seamWidth, seamWidth, coords.x - coords.y),
                   smoothstep (-seamWidth, seamWidth, coords.x - (1.0 - coords.y)));
    float s = min (smoothstep (-seamWidth, seamWidth, coords.y - coords.x),
                   smoothstep (-seamWidth, seamWidth, coords.x - (1.0 - coords.y)));
    float w = min (smoothstep (-seamWidth, seamWidth, coords.y - coords.x),
                   smoothstep (-seamWidth, seamWidth, (1.0 - coords.x) - coords.y));
    float n = min (smoothstep (-seamWidth, seamWidth, coords.x - coords.y),
                   smoothstep (-seamWidth, seamWidth, (1.0 - coords.x) - coords.y));

    float distPixels = roundedRectSdfPixels (coords, tubeTopLeft, tubeBottomRight, screenRadiusPixels);
    float frameShadow = (e * 0.66 + w * 0.66 + n * 0.33 + s);
    frameShadow *= smoothstep (0.0, edgeSoftPixels * 5.0, distPixels);

    float frameAlpha = 1.0 - frameShininess * 0.4;
    float inScreen = smoothstep (0.0, edgeSoftPixels, -distPixels);
    float alpha = mix (frameAlpha, mix (0.0, 0.3, ambientLight), inScreen);

    // x(1-x)y(1-y) * 25, sqrt'd, scaled by ambientLight, masked to the screen.
    // Only evaluated on-screen: off-screen the product goes negative and the
    // pow() would feed NaN into the mix below.
    float glass = 0.0;
    if (inScreen > 0.0)
        glass = clamp (ambientLight * pow (prod2 (coords * (1.0 - coords.yx)) * 25.0, 0.5) * inScreen, 0.0, 1.0);

    vec3 frameTint = frameColor.rgb * frameShadow;
    float noise = rand2 (vTexCoord * viewportSize) - 0.5;
    frameTint = clamp (frameTint + vec3 (noise * 0.04), 0.0, 1.0);
    vec3 color = mix (frameTint, vec3 (glass), inScreen);

    // Bezel reflection now lives in the static pass (kStaticFragment),
    // matching upstream terminal_static.frag.  This pass is purely
    // procedural: glass panel SDF, shadow, glass highlights.

    gl_FragColor = vec4 (color, alpha);
}
)";

    static constexpr const char* kBurnInFragment = R"(
#version 120
varying vec2 vTexCoord;

uniform float burnInLastUpdate;
uniform float burnInTime;
uniform float prevLastUpdate;
uniform sampler2D txt_source;
uniform sampler2D burnInSource;

float rgb2grey (vec3 v) { return dot (v, vec3 (0.21, 0.72, 0.04)); }

void main()
{
    vec3 txtColor = texture2D (txt_source, vTexCoord).rgb;
    vec4 accColor = texture2D (burnInSource, vTexCoord);

    float prevMask = accColor.a;
    float blurDecay = clamp ((burnInLastUpdate - prevLastUpdate) * burnInTime, 0.0, 1.0);
    blurDecay = max (0.0, blurDecay - prevMask);
    vec3 color = max (accColor.rgb - vec3 (blurDecay), txtColor);

    float currMask = step (rgb2grey (color), rgb2grey (txtColor));

    gl_FragColor = vec4 (color, currMask);
}
)";

    static constexpr const char* kDynamicFragment = R"(
#version 120
varying vec2 vTexCoord;
varying float vBrightness;
varying float vDistortionScale;
varying float vDistortionFreq;

uniform float time;
uniform vec4 fontColor;
uniform vec4 backgroundColor;
uniform vec2 virtualResolution;
uniform float rasterizationIntensity;
uniform float burnInLastUpdate;
uniform float burnInTime;
uniform float staticNoise;
uniform float screenCurvature;
uniform float glowingLine;
uniform vec2 jitterDisplacement;
uniform float jitter;
uniform float horizontalSync;
uniform float horizontalSyncStrength;
uniform float flickering;
uniform vec2 scaleNoiseSize;
uniform float frameSize;
uniform float bloom;

uniform sampler2D noiseSource;
uniform sampler2D screenBuffer;
uniform sampler2D burnInSource;
uniform sampler2D frameSource;

float rgb2grey (vec3 v) { return dot (v, vec3 (0.21, 0.72, 0.04)); }

vec2 distortCoordinates (vec2 coords)
{
    vec2 cc = (coords - vec2 (0.5));
    float dist = dot (cc, cc) * screenCurvature;
    return (coords + cc * (1.0 + dist) * dist);
}

vec3 applyRasterization (vec2 screenCoords, vec3 texel, vec2 virtualRes, float intensity)
{
    if (intensity <= 0.0)
        return texel;

    const float INTENSITY = 0.30;
    const float BRIGHTBOOST = 0.30;

    vec3 pixelHigh = ((1.0 + BRIGHTBOOST) - (0.2 * texel)) * texel;
    vec3 pixelLow  = ((1.0 - INTENSITY) + (0.1 * texel)) * texel;

    vec2 coords = fract (screenCoords * virtualRes) * 2.0 - vec2 (1.0);
    float mask = 1.0 - abs (coords.y);

    vec3 rasterizationColor = mix (pixelLow, pixelHigh, mask);
    return mix (texel, rasterizationColor, intensity);
}

float randomPass (vec2 coords)
{
    return fract (smoothstep (-120.0, 0.0, coords.y - (virtualResolution.y + 120.0) * fract (time * 0.15)));
}

vec3 convertWithChroma (vec3 inColor)
{
    float grey = rgb2grey (inColor);
    return mix (backgroundColor.rgb, fontColor.rgb, grey);
}

void main()
{
    vec2 staticCoords = distortCoordinates (vTexCoord);
    vec2 coords = vTexCoord;

    float dst = sin ((coords.y + time) * vDistortionFreq);
    coords.x += dst * vDistortionScale;

    vec4 noiseTexel = texture2D (noiseSource, scaleNoiseSize * coords + vec2 (fract (time / 0.051), fract (time / 0.237)));

    vec2 txt_coords = coords + (noiseTexel.ba - vec2 (0.5)) * jitterDisplacement * jitter;

    float color = 0.0001;
    color += randomPass (coords * virtualResolution) * glowingLine;

    vec4 frameColor = texture2D (frameSource, vTexCoord);
    color *= (1.0 - frameColor.a);

    vec3 txt_color = texture2D (screenBuffer, txt_coords).rgb;
    float bloomScale = 1.0 + max (bloom, 0.0);
    txt_color *= bloomScale;

    vec4 txt_blur = texture2D (burnInSource, vTexCoord);
    float blurDecay = clamp ((time - burnInLastUpdate) * burnInTime, 0.0, 1.0);
    vec3 burnInColor = 0.65 * (txt_blur.rgb - vec3 (blurDecay)) * (1.0 - txt_blur.a);
    txt_color = max (txt_color, burnInColor);

    txt_color += vec3 (color);
    txt_color = applyRasterization (staticCoords, txt_color, virtualResolution, rasterizationIntensity);

    vec3 finalColor = convertWithChroma (txt_color);
    float brightness = mix (1.0, vBrightness, step (0.0, flickering));
    finalColor *= brightness;

    // The frame is blended exactly once, by the present pass -- the original
    // cool-retro-term blends frameColor.a here, but the present pass re-applies
    // it, which double-blends the glass and buries the bezel reflection.
    gl_FragColor = vec4 (finalColor, 1.0);
}
)";

    static constexpr const char* kPresentFragment = R"(
#version 120
varying vec2 vTexCoord;

uniform float time;
uniform float staticNoise;
uniform vec2 scaleNoiseSize;
uniform sampler2D dynamicTexture;
uniform sampler2D frameTexture;
uniform sampler2D noiseSource;

void main()
{
    vec4 dyn = texture2D (dynamicTexture, vTexCoord);
    vec4 fr  = texture2D (frameTexture, vTexCoord);

    // Live analogue grain, applied here instead of inside the dynamic pass so
    // the per-frame scrolling noise never feeds the burn-in accumulation (it
    // would otherwise smear into a ghost trail that converges on the screen
    // centre via the distorted burn-in read). Masked to the screen area.
    vec2 cc = vec2 (0.5) - vTexCoord;
    float distance = length (cc);
    vec4 noiseTexel = texture2D (noiseSource, scaleNoiseSize * vTexCoord + vec2 (fract (time / 0.051), fract (time / 0.237)));
    float grain = 0.0001 + noiseTexel.a * staticNoise * (1.0 - distance * 1.3);
    grain *= (1.0 - fr.a);

    gl_FragColor = vec4 (mix (dyn.rgb + vec3 (grain), fr.rgb, fr.a), 1.0);
}
)";

    static constexpr const char* kRawUIFragment = R"(
#version 120
varying vec2 vTexCoord;
uniform sampler2D uiTexture;
void main()
{
    gl_FragColor = vec4 (texture2D (uiTexture, vTexCoord).rgb, 1.0);
}
)";

    Component* screenSource = nullptr;

    const std::atomic<bool>* enabledFlag = nullptr;

    bool lastEnabled = true;

    float bloomIntensity = 0.95f;
    float scanlineIntensity = 0.35f;
    float glowingLineIntensity = 0.25f;
    float flickerIntensity = 0.15f;

    float burnInIntensity = 0.015f;
    float curvatureIntensity = 0.25f;
    float staticNoiseIntensity = 0.08f;
    float rgbShiftIntensity = 0.05f;
    float jitterIntensity = 0.30f;
    float jitterYScale = 0.30f;
    float horizontalSyncIntensity = 0.0f;

    float ambientLight = 0.25f;
    float frameShininess = 0.30f;

    static constexpr float kFrameSize   = 0.05f;
    static constexpr float kScreenRadius = 16.0f;
    // Residual-curvature scale: content is 1:1 with the window (the squeeze
    // to the tube is the screen transform), so the param is kept mild.
    static constexpr float kCurvatureScale = 0.2f;
    static constexpr float kFontColorR   = 1.00f;
    static constexpr float kFontColorG   = 0.63f;
    static constexpr float kFontColorB   = 0.00f;
    static constexpr float kBackgroundColorR = 0.04f;
    static constexpr float kBackgroundColorG = 0.02f;
    static constexpr float kBackgroundColorB = 0.01f;

    // UI thread writes the snapshot + params under this lock; the GL render
    // thread consumes them without needing the MessageManager lock.
    CriticalSection sharedLock;
    Image uiSnapshot;
    Params params;
    bool snapshotPending = false;

    OpenGLContext openGLContext;

    std::unique_ptr<OpenGLShaderProgram> bloomProgram;
    std::unique_ptr<OpenGLShaderProgram> staticProgram;
    std::unique_ptr<OpenGLShaderProgram> dynamicProgram;
    std::unique_ptr<OpenGLShaderProgram> burnInProgram;
    std::unique_ptr<OpenGLShaderProgram> frameProgram;
    std::unique_ptr<OpenGLShaderProgram> presentProgram;
    std::unique_ptr<OpenGLShaderProgram> rawUIProgram;

    OpenGLTexture uiTexture;
    OpenGLTexture noiseTexture;

    Image noiseTileImage;

    std::unique_ptr<OpenGLFrameBuffer> bloomFBO;
    std::unique_ptr<OpenGLFrameBuffer> screenFBO;
    std::unique_ptr<OpenGLFrameBuffer> dynamicFBO;
    std::unique_ptr<OpenGLFrameBuffer> frameFBO;
    std::unique_ptr<OpenGLFrameBuffer> burnFBO[2];
    int burnRead = 0;
    int burnWrite = 1;

    float lastFrameTime = 0.0f;

    bool glAttached = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CRTScreen)
};
