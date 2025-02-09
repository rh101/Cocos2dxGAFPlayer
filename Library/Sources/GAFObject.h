#pragma once

#include "GAFDelegates.h"
#include "GAFSprite.h"
#include "GAFCollections.h"
#include "GAFTextureAtlas.h"
#include "GAFFilterData.h"

NS_GAF_BEGIN

class GAFAsset;
class GAFTimeline;

class GAFObject : public GAFSprite
{
private:
    const ax::AffineTransform AffineTransformFlashToCocos(const ax::AffineTransform& aTransform);

public:

    typedef std::vector<GAFObject*> DisplayList_t;
    typedef std::vector<ax::ClippingNode*> MaskList_t;
private:
    GAFSequenceDelegate_t                   m_sequenceDelegate;
    GAFAnimationFinishedPlayDelegate_t      m_animationFinishedPlayDelegate;
    GAFAnimationStartedNextLoopDelegate_t   m_animationStartedNextLoopDelegate;
    GAFFramePlayedDelegate_t                m_framePlayedDelegate;
    
    ax::Node*                          m_container;

    uint32_t                                m_totalFrameCount;
    uint32_t                                m_currentSequenceStart;
    uint32_t                                m_currentSequenceEnd;

    bool                                    m_isRunning;
    bool                                    m_isLooped;
    bool                                    m_isReversed;

    double                                  m_timeDelta;
    uint32_t                                m_fps;
    bool                                    m_skipFpsCheck;

    bool                                    m_animationsSelectorScheduled;

    bool                                    m_isInResetState;

private:
    void constructObject();
    GAFObject* _instantiateObject(uint32_t id, GAFCharacterType type, uint32_t reference, bool isMask);
    
    /// schedule/unschedule
    /// @note this function is automatically called in start/stop
    void enableTick(bool val);
    void realizeFrame(ax::Node* out, uint32_t frameIndex);
    void rearrangeSubobject(ax::Node* out, ax::Node* child, int zIndex);

protected:
    GAFObject*                              m_timelineParentObject;
    GAFAsset*                               m_asset;
    GAFTimeline*                            m_timeline;
    DisplayList_t                           m_displayList;
    MaskList_t                              m_masks;
    GAFCharacterType                        m_charType;
    GAFObjectType                           m_objectType;
    uint32_t                                m_currentFrame;
    uint32_t                                m_showingFrame; // Frame number that is valid from the beginning of realize frame
    uint32_t                                m_lastVisibleInFrame; // Last frame that object was visible in
    Filters_t                               m_parentFilters;
    ax::Vec4                           m_parentColorTransforms[2];

    GAFFilterData*                          m_customFilter;

    bool                                    m_isManualColor;

    void    setTimelineParentObject(GAFObject* obj) { m_timelineParentObject = obj; }
    
    void    processAnimations(float dt);

    void    instantiateObject(const AnimationObjects_t& objs, const AnimationMasks_t& masks);

    GAFObject*   encloseNewTimeline(uint32_t reference);

    void        step();
    bool        isCurrentFrameLastInSequence() const;
    uint32_t    nextFrame();

public:
    GAFObject();

    /// @note do not forget to call setSequenceDelegate(nullptr) before deleting your subscriber
    void setSequenceDelegate(GAFSequenceDelegate_t delegate);

    /// @note do not forget to call setAnimationFinishedPlayDelegate(nullptr) before deleting your subscriber
    void setAnimationFinishedPlayDelegate(GAFAnimationFinishedPlayDelegate_t delegate);

    /// @note do not forget to call setAnimationStartedNextLoopDelegate(nullptr) before deleting your subscriber
    void setAnimationStartedNextLoopDelegate(GAFAnimationStartedNextLoopDelegate_t delegate);

    /// @note do not forget to call setFramePlayedDelegate(nullptr) before deleting your subscriber
    void setFramePlayedDelegate(GAFFramePlayedDelegate_t delegate);

    void visit(ax::Renderer *renderer, const ax::Mat4 &transform, uint32_t flags) override;
    void draw(ax::Renderer *renderer, const ax::Mat4 &transform, uint32_t flags) override
    {
        (void)flags;
        (void)renderer;
        (void)transform;
    }

    void useExternalTextureAtlas(std::vector<ax::Texture2D*>& textures, GAFTextureAtlas::Elements_t& elements);

public:
    void    processAnimation();
    // Playback accessing
    void        start();
    void        stop();

    /// Pauses animation including enclosed timelines
    void        pauseAnimation();

    /// Resumes animation including enclosed timelines
    void        resumeAnimation();


    bool        isDone() const;
    bool        getIsAnimationRunning() const;

    bool        isLooped() const;
    void        setLooped(bool looped, bool recursive = false);

    bool        isReversed() const;
    void        setReversed(bool reversed, bool fromCurrentFrame = true);

    uint32_t    getTotalFrameCount() const;
    uint32_t    getCurrentFrameIndex() const;

    bool        setFrame(uint32_t index);

    /// Plays specified frame and then stops excluding enclosed timelines
    bool        gotoAndStop(const std::string& frameLabel);
    /// Plays specified frame and then stops excluding enclosed timelines
    bool        gotoAndStop(uint32_t frameNumber);

    /// Plays animation from specified frame excluding enclosed timelines
    bool        gotoAndPlay(const std::string& frameLabel);
    /// Plays animation from specified frame excluding enclosed timelines
    bool        gotoAndPlay(uint32_t frameNumber);

    uint32_t    getStartFrame(const std::string& frameLabel);
    uint32_t    getEndFrame(const std::string& frameLabel);

    /// Plays animation sequence with specified name
    /// @param name a sequence name
    /// @param looped if true - sequence should play in cycle
    /// @param resume if true - animation will be played immediately, if false - playback will be paused after the first frame is shown
    /// @param hint specific animation playback parameters

    bool        playSequence(const std::string& name, bool looped, bool resume = true);

    /// Stops playing an animation as a sequence
    void        clearSequence();

    void        setAnimationRunning(bool value, bool recurcive);
public:

    ~GAFObject() override;

    static GAFObject * create(GAFAsset * anAsset, GAFTimeline* timeline);

    bool init(GAFAsset * anAnimationData, GAFTimeline* timeline);

    bool hasSequences() const;

    bool isVisibleInCurrentFrame() const;

    ax::Rect getBoundingBoxForCurrentFrame();

    const AnimationSequences_t& getSequences() const;
    GAFTimeline* getTimeLine() { return m_timeline; }
    DisplayList_t& getDisplayList() { return m_displayList; }
    const DisplayList_t& getDisplayList() const { return m_displayList; }

    virtual const ax::Mat4& getNodeToParentTransform() const override;
    virtual ax::AffineTransform getNodeToParentAffineTransform() const override;

    virtual void setColor(const ax::Color3B& color) override;
    virtual void setOpacity(uint8_t opacity) override;

    template <typename FilterSubtype>
    void setCustomFilter(const FilterSubtype* filter)
    {
        AX_SAFE_DELETE(m_customFilter);
        if (filter)
        {
            m_customFilter = new FilterSubtype(*filter);
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Accessors

    // Searches for an object by given string
    // @param object name e.g. "head" or object path e.g. "knight.body.arm"
    // @note it can slow down the real-time performance
    // @returns instance of GAFObject or null. Warning: the instance could be invalidated when the system catches EVENT_COME_TO_FOREGROUND event
    GAFObject* getObjectByName(const std::string& name);
    const GAFObject* getObjectByName(const std::string& name) const;

    uint32_t getFps() const;

    void setFps(uint32_t value);

    void setFpsLimitations(bool fpsLimitations);
};

NS_GAF_END
