#include "GAFPrecompiled.h"
#include "GAFObject.h"
#include "GAFAsset.h"
#include "GAFTimeline.h"
#include "GAFTextureAtlas.h"
#include "GAFAssetTextureManager.h"
#include "GAFTextureAtlasElement.h"
#include "GAFMovieClip.h"
#include "GAFMask.h"
#include "GAFAnimationFrame.h"
#include "GAFSubobjectState.h"
#include "GAFFilterData.h"
#include "GAFTextField.h"

#include <math/TransformUtils.h>

#define ENABLE_RUNTIME_FILTERS 1

NS_GAF_BEGIN

static const AnimationSequences_t s_emptySequences = AnimationSequences_t();

const ax::AffineTransform GAFObject::AffineTransformFlashToCocos(const ax::AffineTransform& aTransform)
{
    ax::AffineTransform transform = aTransform;
    transform.b = -transform.b;
    transform.c = -transform.c;
    float flipMul = isFlippedY() ? -2 : 2;
    transform.ty = getAnchorPointInPoints().y * flipMul - transform.ty;
    return transform;
}


GAFObject::GAFObject()
    : m_timelineParentObject(nullptr)
    , m_container(nullptr)
    , m_totalFrameCount(0)
    , m_currentSequenceStart(0)
    , m_currentSequenceEnd(0)
    , m_isRunning(false)
    , m_isLooped(false)
    , m_isReversed(false)
    , m_timeDelta(0.0)
    , m_fps(0)
    , m_skipFpsCheck(false)
    , m_asset(nullptr)
    , m_timeline(nullptr)
    , m_currentFrame(GAFFirstFrameIndex)
    , m_showingFrame(GAFFirstFrameIndex)
    , m_lastVisibleInFrame(0)
    , m_objectType(GAFObjectType::None)
    , m_animationsSelectorScheduled(false)
    , m_isInResetState(false)
    , m_customFilter(nullptr)
    , m_isManualColor(false)
{
    m_charType = GAFCharacterType::Timeline;
    m_parentColorTransforms[0] = ax::Vec4::ONE;
    m_parentColorTransforms[1] = ax::Vec4::ZERO;
}

GAFObject::~GAFObject()
{
    stop();
    GAF_SAFE_RELEASE_ARRAY_WITH_NULL_CHECK(MaskList_t, m_masks);
    GAF_SAFE_RELEASE_ARRAY_WITH_NULL_CHECK(DisplayList_t, m_displayList);
    AX_SAFE_RELEASE(m_asset);
    AX_SAFE_DELETE(m_customFilter);
}

GAFObject * GAFObject::create(GAFAsset * anAsset, GAFTimeline* timeline)
{
    GAFObject* ret = new GAFObject();

    if (ret && ret->init(anAsset, timeline))
    {
        ret->autorelease();
        return ret;
    }

    AX_SAFE_RELEASE(ret);
    return nullptr;
}

bool GAFObject::init(GAFAsset * anAnimationData, GAFTimeline* timeline)
{
    AXASSERT(anAnimationData, "anAssetData data should not be nil");
    AXASSERT(timeline, "Timeline data should not be nil");

    if (!anAnimationData || !timeline)
    {
        return false;
    }

    if (m_asset != anAnimationData)
    {
        AX_SAFE_RELEASE(m_asset);
        m_asset = anAnimationData;
        AX_SAFE_RETAIN(m_asset);
    }

    if (m_timeline != timeline)
    {
        AX_SAFE_RELEASE(m_timeline);
        m_timeline = timeline;
        AX_SAFE_RETAIN(m_timeline);
    }
    m_container = ax::Node::create();
    addChild(m_container);
    m_container->setContentSize(getContentSize());

    m_currentSequenceStart = m_currentFrame = GAFFirstFrameIndex;

    m_currentSequenceEnd = m_totalFrameCount = m_timeline->getFramesCount();

    constructObject();

    return true;
}

void GAFObject::constructObject()
{
    ax::Rect size = m_timeline->getRect();

    setContentSize(ax::Size(size.size.width, size.size.height));

    GAF_SAFE_RELEASE_ARRAY_WITH_NULL_CHECK(DisplayList_t, m_displayList);

    m_fps = m_asset->getSceneFps();

    m_animationsSelectorScheduled = false;

    instantiateObject(m_timeline->getAnimationObjects(), m_timeline->getAnimationMasks());
}

GAFObject* GAFObject::_instantiateObject(uint32_t id, GAFCharacterType type, uint32_t reference, bool isMask)
{
    GAFObject* result = nullptr;
    if (type == GAFCharacterType::Timeline)
    {
        result = encloseNewTimeline(reference);
    }
    else if (type == GAFCharacterType::TextField)
    {
        TextsData_t::const_iterator it = m_timeline->getTextsData().find(reference);
        if (it != m_timeline->getTextsData().end())
        {
            GAFTextField *tf = new GAFTextField();
            tf->initWithTextData(it->second);
            result = tf;
        }
    }
    else if (type == GAFCharacterType::Texture)
    {
        GAFTextureAtlas* atlas = m_timeline->getTextureAtlas();
        const GAFTextureAtlas::Elements_t& elementsMap = atlas->getElements();
        ax::SpriteFrame * spriteFrame = nullptr;
        GAFTextureAtlas::Elements_t::const_iterator elIt = elementsMap.find(reference); // Search for atlas element by its xref
        const GAFTextureAtlasElement* txElemet = nullptr;
        if (elIt != elementsMap.end())
        {
            txElemet = elIt->second;
            GAFAssetTextureManager* txMgr = m_asset->getTextureManager();
            ax::Texture2D * texture = txMgr->getTextureById(txElemet->atlasIdx + 1);
            if (texture)
            {
                spriteFrame = ax::SpriteFrame::createWithTexture(texture, txElemet->bounds);
            }
            else
            {
                AXLOGERROR("Cannot add sub object with Id: %d, atlas with idx: %d not found.", id, txElemet->atlasIdx);
            }
        }
        if (spriteFrame)
        {
            if (!isMask)
                result = new GAFMovieClip();
            else
                result = new GAFMask();
            result->initWithSpriteFrame(spriteFrame, txElemet->rotation);
            result->objectIdRef = id;
            ax::Vec2 pt = ax::Vec2(0 - (0 - (txElemet->pivotPoint.x / result->getContentSize().width)),
                0 + (1 - (txElemet->pivotPoint.y / result->getContentSize().height)));
            result->setAnchorPoint(pt);

            if (txElemet->getScale() != 1.0f)
            {
                result->setAtlasScale(1.0f / txElemet->getScale());
            }
            result->setBlendFunc(ax::BlendFunc::ALPHA_PREMULTIPLIED);
        }
    }
    if (result)
        result->setTimelineParentObject(this);
    return result;
}

void GAFObject::instantiateObject(const AnimationObjects_t& objs, const AnimationMasks_t& masks)
{
    uint32_t maxIdx = 0;
    for (AnimationObjects_t::const_iterator it = objs.begin(), e = objs.end(); it != e; ++it)
    {
        maxIdx = std::max(it->first, maxIdx);
    }
    for (AnimationMasks_t::const_iterator it = masks.begin(), e = masks.end(); it != e; ++it)
    {
        maxIdx = std::max(it->first, maxIdx);
    }
    maxIdx += 1;

    m_displayList.resize(maxIdx);
    m_masks.resize(maxIdx);

    for (AnimationObjects_t::const_iterator i = objs.begin(), e = objs.end(); i != e; ++i)
    {
        GAFCharacterType charType = std::get<1>(i->second);
        uint32_t reference = std::get<0>(i->second);
        uint32_t objectId = i->first;

        AXASSERT(m_displayList[objectId] == nullptr, "Obeject is already created. Memory will be leaked.");
        m_displayList[objectId] = _instantiateObject(objectId, charType, reference, false);
    }
    for (AnimationMasks_t::const_iterator i = masks.begin(), e = masks.end(); i != e; ++i)
    {
        GAFCharacterType charType = std::get<1>(i->second);
        uint32_t reference = std::get<0>(i->second);
        uint32_t objectId = i->first;

        AXASSERT(m_displayList[objectId] == nullptr, "Obeject is already created. Memory will be leaked.");
        GAFObject* stencil = _instantiateObject(objectId, charType, reference, true);
        m_displayList[objectId] = stencil;
        ax::ClippingNode* mask = ax::ClippingNode::create(stencil);
        mask->retain();
        mask->setAlphaThreshold(0.1f);
        m_masks[objectId] = mask;
    }
}

GAFObject* GAFObject::encloseNewTimeline(uint32_t reference)
{
    Timelines_t& timelines = m_asset->getTimelines();

    AXASSERT(reference != IDNONE, "Invalid object reference.");

    Timelines_t::iterator tl = timelines.find(reference);

    AXASSERT(tl != timelines.end(), "Invalid object reference.");

    GAFObject* newObject = new GAFObject();
    newObject->init(m_asset, tl->second);
    return newObject;
}

void GAFObject::useExternalTextureAtlas(std::vector<ax::Texture2D*>& textures, GAFTextureAtlas::Elements_t& elements)
{
    m_asset->useExternalTextureAtlas(textures, elements);
}

void GAFObject::processAnimation()
{
    realizeFrame(m_container, m_currentFrame);
}

void GAFObject::setAnimationRunning(bool value, bool recursive)
{
    m_isRunning = value;

    if (recursive)
    {
        for (auto obj : m_displayList)
        {
            if (obj == nullptr)
            {
                continue;
            }
            obj->setAnimationRunning(value, recursive);
        }
    }
}

bool GAFObject::getIsAnimationRunning() const
{
    return m_isRunning;
}

void GAFObject::setSequenceDelegate(GAFSequenceDelegate_t delegate)
{
    m_sequenceDelegate = delegate;
}

void GAFObject::setAnimationFinishedPlayDelegate(GAFAnimationFinishedPlayDelegate_t delegate)
{
    m_animationFinishedPlayDelegate = delegate;
}

void GAFObject::setAnimationStartedNextLoopDelegate(GAFAnimationStartedNextLoopDelegate_t delegate)
{
    m_animationStartedNextLoopDelegate = delegate;
}

void GAFObject::setFramePlayedDelegate(GAFFramePlayedDelegate_t delegate)
{
    m_framePlayedDelegate = delegate;
}

void GAFObject::start()
{
    enableTick(true);

    if (!m_isRunning)
    {
        m_currentFrame = m_isReversed ? m_totalFrameCount - 1 : GAFFirstFrameIndex;
        setAnimationRunning(true, true);
    }
}

void GAFObject::stop()
{
    enableTick(false);
    if (m_isRunning)
    {
        setFrame(GAFFirstFrameIndex);
        setAnimationRunning(false, true);
    }
}

void GAFObject::processAnimations(float dt)
{
    if (m_skipFpsCheck)
    {
        step();
        if (m_framePlayedDelegate)
        {
            m_framePlayedDelegate(this, m_currentFrame);
        }
    }
    else
    {
        m_timeDelta += dt;
        double frameTime = 1.0 / m_fps;
        while (m_timeDelta >= frameTime)
        {
            m_timeDelta -= frameTime;
            step();

            if (m_framePlayedDelegate)
            {
                m_framePlayedDelegate(this, m_currentFrame);
            }
        }
    }
}

void GAFObject::pauseAnimation()
{
    if (m_isRunning)
    {
        setAnimationRunning(false, false);

        if (m_animationFinishedPlayDelegate)
        {
            m_animationFinishedPlayDelegate(this);
        }
    }
}

void GAFObject::resumeAnimation()
{
    if (!m_isRunning)
    {
        setAnimationRunning(true, false);
    }
}

bool GAFObject::isDone() const
{
    if (m_isLooped)
    {
        return false;
    }

    return isCurrentFrameLastInSequence();
    
    /*if (!m_isReversed)
    {
        return m_currentFrame > m_totalFrameCount;
    }
    else
    {
        return m_currentFrame < GAFFirstFrameIndex - 1;
    }*/
}

bool GAFObject::isLooped() const
{
    return m_isLooped;
}

void GAFObject::setLooped(bool looped, bool recursive /*= false*/)
{
    m_isLooped = looped;

    if (recursive)
    {
        for (auto obj : m_displayList)
        {
            if (obj == nullptr)
            {
                continue;
            }
            obj->setLooped(looped, recursive);
        }
    }
}

bool GAFObject::isReversed() const
{
    return m_isReversed;
}

void GAFObject::setReversed(bool reversed, bool fromCurrentFrame /* = true */)
{
    m_isReversed = reversed;
    if (!fromCurrentFrame)
    {
        m_currentFrame = reversed ? m_currentSequenceEnd - 1 : m_currentSequenceStart;
    }

    for (auto obj : m_displayList)
    {
        if (obj == nullptr)
        {
            continue;
        }
        obj->setReversed(reversed, fromCurrentFrame);
    }
}

uint32_t GAFObject::getTotalFrameCount() const
{
    return m_totalFrameCount;
}

uint32_t GAFObject::getCurrentFrameIndex() const
{
    return m_showingFrame;
}

bool GAFObject::setFrame(uint32_t index)
{
    if (index < m_totalFrameCount)
    {
        m_showingFrame = m_currentFrame = index;
        processAnimation();
        return true;
    }
    return false;
}

bool GAFObject::gotoAndStop(const std::string& frameLabel)
{
    uint32_t f = getStartFrame(frameLabel);
    if (IDNONE == f)
    {
        uint32_t frameNumber = atoi(frameLabel.c_str());
        if (frameNumber == 0)
        {
            return false;
        }
        return gotoAndStop(frameNumber - 1);
    }
    return gotoAndStop(f);
}

bool GAFObject::gotoAndStop(uint32_t frameNumber)
{
    if (setFrame(frameNumber))
    {
        m_isRunning = false;

        if (m_animationFinishedPlayDelegate)
        {
            m_animationFinishedPlayDelegate(this);
        }

        return true;
    }
    return false;
}

bool GAFObject::gotoAndPlay(const std::string& frameLabel)
{
    uint32_t f = getStartFrame(frameLabel);
    if (IDNONE == f)
    {
        uint32_t frameNumber = atoi(frameLabel.c_str());
        if (frameNumber == 0)
        {
            return false;
        }
        return gotoAndPlay(frameNumber - 1);
    }
    return gotoAndPlay(f);
}

bool GAFObject::gotoAndPlay(uint32_t frameNumber)
{
    if (setFrame(frameNumber))
    {
        m_isRunning = true;
        return true;
    }
    return false;
}

uint32_t GAFObject::getStartFrame(const std::string& frameLabel)
{
    if (!m_asset)
    {
        return IDNONE;
    }

    const GAFAnimationSequence * seq = m_timeline->getSequence(frameLabel);
    if (seq)
    {
        return seq->startFrameNo;
    }
    return IDNONE;

}

uint32_t GAFObject::getEndFrame(const std::string& frameLabel)
{
    if (!m_asset)
    {
        return IDNONE;
    }
    const GAFAnimationSequence * seq = m_timeline->getSequence(frameLabel);
    if (seq)
    {
        return seq->endFrameNo;
    }
    return IDNONE;
}

bool GAFObject::playSequence(const std::string& name, bool looped, bool resume /*= true*/)
{
    if (!m_asset || !m_timeline)
    {
        return false;
    }

    if (name.empty())
    {
        return false;
    }

    uint32_t s = getStartFrame(name);
    uint32_t e = getEndFrame(name);

    if (IDNONE == s || IDNONE == e)
    {
        return false;
    }

    m_currentSequenceStart = s;
    m_currentSequenceEnd = e;

    uint32_t frame = m_isReversed ? (e - 1) : (s);
    setFrame(frame);
    
    setLooped(looped, false);

    if (resume)
    {
        resumeAnimation();
    }
    else
    {
        stop();
    }

    return true;
}

void GAFObject::clearSequence()
{
    m_currentSequenceStart = GAFFirstFrameIndex;
    m_currentSequenceEnd = m_totalFrameCount;
}

void GAFObject::step()
{
    m_showingFrame = m_currentFrame;

    if (!getIsAnimationRunning())
    {
        processAnimation();
        return;
    }

    if (m_sequenceDelegate && m_timeline)
    {
        const GAFAnimationSequence * seq = nullptr;
        if (!m_isReversed)
        {
            seq = m_timeline->getSequenceByLastFrame(m_currentFrame);
        }
        else
        {
            seq = m_timeline->getSequenceByFirstFrame(m_currentFrame + 1);
        }

        if (seq)
        {
            m_sequenceDelegate(this, seq->name);
        }
    }

    if (isCurrentFrameLastInSequence())
    {
        if (m_isLooped)
        {
            if (m_animationStartedNextLoopDelegate)
                m_animationStartedNextLoopDelegate(this);
        }
        else
        {
            setAnimationRunning(false, false);

            if (m_animationFinishedPlayDelegate)
                m_animationFinishedPlayDelegate(this);
        }
    }

    processAnimation();

    m_showingFrame = m_currentFrame;
    m_currentFrame = nextFrame();
}

bool GAFObject::isCurrentFrameLastInSequence() const
{
    if (m_isReversed)
        return m_currentFrame == m_currentSequenceStart;
    return m_currentFrame == m_currentSequenceEnd - 1;
}

uint32_t GAFObject::nextFrame()
{
    if (!getIsAnimationRunning())
    {
        return m_currentFrame;
    }

    if (isCurrentFrameLastInSequence())
    {
        if (!m_isLooped)
            return m_currentFrame;

        if (m_isReversed)
            return m_currentSequenceEnd - 1;
        else
            return m_currentSequenceStart;
    }

    return m_currentFrame + (m_isReversed ? -1 : 1);
}

bool GAFObject::hasSequences() const
{
    return !m_timeline->getAnimationSequences().empty();
}

const AnimationSequences_t& GAFObject::getSequences() const
{
    if (m_timeline)
        return m_timeline->getAnimationSequences();
    return s_emptySequences;
}

static ax::Rect GAFCCRectUnion(const ax::Rect& src1, const ax::Rect& src2)
{
    float thisLeftX = src1.origin.x;
    float thisRightX = src1.origin.x + src1.size.width;
    float thisTopY = src1.origin.y + src1.size.height;
    float thisBottomY = src1.origin.y;

    if (thisRightX < thisLeftX)
    {
        std::swap(thisRightX, thisLeftX); // This rect has negative width
    }

    if (thisTopY < thisBottomY)
    {
        std::swap(thisTopY, thisBottomY); // This rect has negative height
    }

    float otherLeftX = src2.origin.x;
    float otherRightX = src2.origin.x + src2.size.width;
    float otherTopY = src2.origin.y + src2.size.height;
    float otherBottomY = src2.origin.y;

    if (otherRightX < otherLeftX)
    {
        std::swap(otherRightX, otherLeftX); // Other rect has negative width
    }

    if (otherTopY < otherBottomY)
    {
        std::swap(otherTopY, otherBottomY); // Other rect has negative height
    }

    float combinedLeftX = std::min(thisLeftX, otherLeftX);
    float combinedRightX = std::max(thisRightX, otherRightX);
    float combinedTopY = std::max(thisTopY, otherTopY);
    float combinedBottomY = std::min(thisBottomY, otherBottomY);

    return ax::Rect(combinedLeftX, combinedBottomY, combinedRightX - combinedLeftX, combinedTopY - combinedBottomY);
}

ax::Rect GAFObject::getBoundingBoxForCurrentFrame()
{
    ax::Rect result = ax::Rect::ZERO;

    bool isFirstObj = true;
    for (DisplayList_t::iterator i = m_displayList.begin(), e = m_displayList.end(); i != e; ++i)
    {
        if (*i == nullptr)
        {
            continue;
        }

        GAFSprite* anim = *i;
        if (anim->isVisible())
        {
            ax::Rect bb = anim->getBoundingBox();
            if (isFirstObj)
                result = bb;
            else
                result = GAFCCRectUnion(result, bb);
        }

        isFirstObj = false;
    }

    return ax::RectApplyTransform(result, getNodeToParentTransform());
}

ax::Mat4 const& GAFObject::getNodeToParentTransform() const
{
    if (m_charType == GAFCharacterType::Timeline)
        return ax::Node::getNodeToParentTransform();
    else
        return GAFSprite::getNodeToParentTransform();
}

ax::AffineTransform GAFObject::getNodeToParentAffineTransform() const
{
    if (m_charType == GAFCharacterType::Timeline)
        return ax::Node::getNodeToParentAffineTransform();
    else
        return GAFSprite::getNodeToParentAffineTransform();
}

void GAFObject::setColor(const ax::Color3B& color)
{
    m_isManualColor = true;
    Node::setColor(color);
}

void GAFObject::setOpacity(uint8_t opacity)
{
    m_isManualColor = true;
    Node::setOpacity(opacity);
}

void GAFObject::rearrangeSubobject(ax::Node* out, ax::Node* child, int zIndex)
{
    ax::Node* parent = child->getParent();
    child->setCameraMask(getCameraMask(), false);
    if (parent != out)
    {
        child->removeFromParentAndCleanup(false);
        out->addChild(child, zIndex);
    }
    else
    {
        static_cast<GAFObject*>(child)->_transformUpdated = true;
        child->setLocalZOrder(zIndex);
    }
}

void GAFObject::realizeFrame(ax::Node* out, uint32_t frameIndex)
{
    const AnimationFrames_t& animationFrames = m_timeline->getAnimationFrames();

    if (animationFrames.size() <= frameIndex)
    {
        return;
    }

    GAFAnimationFrame *currentFrame = animationFrames[frameIndex];

    const GAFAnimationFrame::SubobjectStates_t& states = currentFrame->getObjectStates();

    for (const GAFSubobjectState* state : states)
    {
        GAFObject* subObject = m_displayList[state->objectIdRef];

        if (!subObject)
            continue;

        if (state->colorMults()[GAFColorTransformIndex::GAFCTI_A] >= 0.f && subObject->m_isInResetState)
        {
            subObject->m_currentFrame = subObject->m_currentSequenceStart;
        }
        subObject->m_isInResetState = state->colorMults()[GAFColorTransformIndex::GAFCTI_A] < 0.f;

        if (!state->isVisible())
            continue;

        if (subObject->m_charType == GAFCharacterType::Timeline)
        {
            if (!subObject->m_isInResetState)
            {
                ax::AffineTransform stateTransform = state->affineTransform;
                float csf = m_timeline->usedAtlasScale();
                stateTransform.tx *= csf;
                stateTransform.ty *= csf;
                ax::AffineTransform t = AffineTransformFlashToCocos(stateTransform);

                t.tx /= subObject->getScaleX();
                t.ty /= subObject->getScaleY();
                
                subObject->setAdditionalTransform(t);
                
                subObject->m_parentFilters.clear();
                if (m_customFilter)
                {
                    subObject->m_parentFilters.push_back(m_customFilter);
                }

                const Filters_t& filters = state->getFilters();
                subObject->m_parentFilters.insert(subObject->m_parentFilters.end(), filters.begin(), filters.end());
                
                const float* cm = state->colorMults();
                subObject->m_parentColorTransforms[0] = ax::Vec4(
                    m_parentColorTransforms[0].x * cm[0],
                    m_parentColorTransforms[0].y * cm[1],
                    m_parentColorTransforms[0].z * cm[2],
                    m_parentColorTransforms[0].w * cm[3]);
                subObject->m_parentColorTransforms[1] = ax::Vec4(state->colorOffsets()) + m_parentColorTransforms[1];

                if (m_masks[state->objectIdRef])
                {
                    rearrangeSubobject(out, m_masks[state->objectIdRef], state->zIndex);
                }
                else
                {
                    //subObject->removeFromParentAndCleanup(false);
                    if (state->maskObjectIdRef == IDNONE)
                    {
                        rearrangeSubobject(out, subObject, state->zIndex);
                    }
                    else
                    {
                        // If the state has a mask, then attach it 
                        // to the clipping node. Clipping node will be attached on its state
                        auto mask = m_masks[state->maskObjectIdRef];
                        AXASSERT(mask, "Error. No mask found for this ID");
                        if (mask)
                            rearrangeSubobject(mask, subObject, state->zIndex);
                    }
                }

                subObject->step();
            }
        }
        else if (subObject->m_charType == GAFCharacterType::Texture)
        {
            ax::Vec2 prevAP = subObject->getAnchorPoint();
            ax::Size  prevCS = subObject->getContentSize();

#if ENABLE_RUNTIME_FILTERS
            if (subObject->m_objectType == GAFObjectType::MovieClip)
            {
                // Validate sprite type (w/ or w/o filter)
                const Filters_t& filters = state->getFilters();
                GAFFilterData* filter = NULL;

                GAFMovieClip* mc = static_cast<GAFMovieClip*>(subObject);

                if (m_customFilter)
                {
                    filter = m_customFilter;
                }
                else if (!m_parentFilters.empty())
                {
                    filter = *m_parentFilters.begin();
                }
                else if (!filters.empty())
                {
                    filter = *filters.begin();
                }

                if (filter)
                {
                    filter->apply(mc);
                }

                if (!filter || filter->getType() != GAFFilterType::Blur)
                {
                    mc->setBlurFilterData(nullptr);
                }

                if (!filter || filter->getType() != GAFFilterType::ColorMatrix)
                {
                    mc->setColorMarixFilterData(nullptr);
                }

                if (!filter || filter->getType() != GAFFilterType::Glow)
                {
                    mc->setGlowFilterData(nullptr);
                }

                if (!filter || filter->getType() != GAFFilterType::DropShadow)
                {
                    GAFDropShadowFilterData::reset(mc);
                }
            }
#endif

            ax::Size newCS = subObject->getContentSize();
            ax::Vec2 newAP = ax::Vec2(((prevAP.x - 0.5f) * prevCS.width) / newCS.width + 0.5f,
                ((prevAP.y - 0.5f) * prevCS.height) / newCS.height + 0.5f);
            subObject->setAnchorPoint(newAP);


            if (m_masks[state->objectIdRef])
            {
                rearrangeSubobject(out, m_masks[state->objectIdRef], state->zIndex);
            }
            else
            {
                //subObject->removeFromParentAndCleanup(false);
                if (state->maskObjectIdRef == IDNONE)
                {
                    rearrangeSubobject(out, subObject, state->zIndex);
                }
                else
                {
                    // If the state has a mask, then attach it 
                    // to the clipping node. Clipping node will be attached on its state
                    auto mask = m_masks[state->maskObjectIdRef];
                    AXASSERT(mask, "Error. No mask found for this ID");
                    if (mask)
                        rearrangeSubobject(mask, subObject, state->zIndex);
                }
            }

            ax::AffineTransform stateTransform = state->affineTransform;
            float csf = m_timeline->usedAtlasScale();
            stateTransform.tx *= csf;
            stateTransform.ty *= csf;
            ax::AffineTransform t = AffineTransformFlashToCocos(state->affineTransform);
            
            if (isFlippedX() || isFlippedY())
            {
                float flipMulX = isFlippedX() ? -1 : 1;
                float flipOffsetX = isFlippedX() ? getContentSize().width - m_asset->getHeader().frameSize.getMinX() : 0;
                float flipMulY = isFlippedY() ? -1 : 1;
                float flipOffsetY = isFlippedY() ? -getContentSize().height + m_asset->getHeader().frameSize.getMinY() : 0;

                ax::AffineTransform flipCenterTransform = ax::AffineTransformMake(flipMulX, 0, 0, flipMulY, flipOffsetX, flipOffsetY);
                t = AffineTransformConcat(t, flipCenterTransform);
            }

            float curScale = subObject->getScale();
            if (fabs(curScale - 1.0) > std::numeric_limits<float>::epsilon())
            {
                t.a *= curScale;
                t.d *= curScale;
            }

            subObject->setExternalTransform(t);

            if (subObject->m_objectType == GAFObjectType::MovieClip)
            {
                GAFMovieClip* mc = static_cast<GAFMovieClip*>(subObject);
                float colorMults[4] = {
                    state->colorMults()[0] * m_parentColorTransforms[0].x * _displayedColor.r / 255,
                    state->colorMults()[1] * m_parentColorTransforms[0].y * _displayedColor.g / 255,
                    state->colorMults()[2] * m_parentColorTransforms[0].z * _displayedColor.b / 255,
                    state->colorMults()[3] * m_parentColorTransforms[0].w * _displayedOpacity / 255
                };
                float colorOffsets[4] = {
                    state->colorOffsets()[0] + m_parentColorTransforms[1].x,
                    state->colorOffsets()[1] + m_parentColorTransforms[1].y,
                    state->colorOffsets()[2] + m_parentColorTransforms[1].z,
                    state->colorOffsets()[3] + m_parentColorTransforms[1].w
                };

                mc->setColorTransform(colorMults, colorOffsets);
            }
        }
        else if (subObject->m_charType == GAFCharacterType::TextField)
        {
            //GAFTextField *tf = static_cast<GAFTextField*>(subObject);
            rearrangeSubobject(out, subObject, state->zIndex);

            ax::AffineTransform stateTransform = state->affineTransform;
            float csf = m_timeline->usedAtlasScale();
            stateTransform.tx *= csf;
            stateTransform.ty *= csf;
            ax::AffineTransform t = AffineTransformFlashToCocos(state->affineTransform);

            if (isFlippedX() || isFlippedY())
            {
                float flipMulX = isFlippedX() ? -1 : 1;
                float flipOffsetX = isFlippedX() ? getContentSize().width - m_asset->getHeader().frameSize.getMinX() : 0;
                float flipMulY = isFlippedY() ? -1 : 1;
                float flipOffsetY = isFlippedY() ? -getContentSize().height + m_asset->getHeader().frameSize.getMinY() : 0;

                ax::AffineTransform flipCenterTransform = ax::AffineTransformMake(flipMulX, 0, 0, flipMulY, flipOffsetX, flipOffsetY);
                t = AffineTransformConcat(t, flipCenterTransform);
            }

            subObject->setExternalTransform(t);
        }

        if (state->isVisible())
        {
            subObject->m_lastVisibleInFrame = frameIndex + 1;
        }
    }

    GAFAnimationFrame::TimelineActions_t timelineActions = currentFrame->getTimelineActions();
    for (GAFTimelineAction action : timelineActions)
    {
        switch (action.getType())
        {
        case GAFActionType::Stop:
            pauseAnimation();
            break;
        case GAFActionType::Play:
            resumeAnimation();
            break;
        case GAFActionType::GotoAndStop:
            gotoAndStop(action.getParam(GAFTimelineAction::PI_FRAME));
            break;
        case GAFActionType::GotoAndPlay:
            gotoAndPlay(action.getParam(GAFTimelineAction::PI_FRAME));
            break;
        case GAFActionType::DispatchEvent:
            {
                std::string type = action.getParam(GAFTimelineAction::PI_EVENT_TYPE);
                if (type.compare(GAFSoundInfo::SoundEvent) == 0)
                {
                    m_asset->soundEvent(&action);
                }
                else
                {
                    _eventDispatcher->dispatchCustomEvent(action.getParam(GAFTimelineAction::PI_EVENT_TYPE), &action);
                }
            }
            break;

        case GAFActionType::None:
        default:
            break;
        }
    }
}

uint32_t GAFObject::getFps() const
{
    return m_fps;
}

void GAFObject::setFps(uint32_t value)
{
    AXASSERT(value, "Error! Fps is set to zero.");
    m_fps = value;
}

void GAFObject::setFpsLimitations(bool fpsLimitations)
{
    m_skipFpsCheck = !fpsLimitations;
}

GAFObject* GAFObject::getObjectByName(const std::string& name)
{
    if (name.empty())
    {
        return nullptr;
    }

    std::stringstream ss(name);
    std::string item;
    typedef std::vector<std::string> StringVec_t;
    StringVec_t elems;
    while (std::getline(ss, item, '.'))
    {
        elems.push_back(item);
    }

    GAFObject* retval = nullptr;

    if (!elems.empty())
    {
        const NamedParts_t& np = m_timeline->getNamedParts();
        NamedParts_t::const_iterator it = np.find(elems[0]);

        if (it != np.end())
        {
            retval = m_displayList[it->second];

            if (elems.size() == 1)
            {
                return retval;
            }
            StringVec_t::iterator begIt = elems.begin() + 1;

            while (begIt != elems.end())
            {
                const NamedParts_t& np = retval->m_timeline->getNamedParts();
                NamedParts_t::const_iterator it = np.find(*begIt);
                if (it != np.end())
                {
                    retval = retval->m_displayList[it->second];
                }
                else
                {
                    // Sequence is incorrect
                    //break;

                    // It is better to return nil instead of the last found object in a chain
                    return nullptr;
                }

                ++begIt;
            }

            return retval;
        }
    }
    return nullptr;
}

const GAFObject* GAFObject::getObjectByName(const std::string& name) const
{
    return const_cast<GAFObject*>(this)->getObjectByName(name);
}

bool GAFObject::isVisibleInCurrentFrame() const
{
    // If sprite is a part of timeline object - check it for visibility in current frame
    if (m_timelineParentObject && (m_timelineParentObject->getCurrentFrameIndex() + 1 != m_lastVisibleInFrame))
        return false;
    return true;
}

void GAFObject::visit(ax::Renderer *renderer, const ax::Mat4 &transform, uint32_t flags)
{
    if (isVisibleInCurrentFrame())
    {
        GAFSprite::visit(renderer, transform, flags);
    }
}

void GAFObject::enableTick(bool val)
{
    if (!m_animationsSelectorScheduled && val)
    {
        schedule(ax::SEL_SCHEDULE(&GAFObject::processAnimations));

        m_animationsSelectorScheduled = true;
    }
    else if (m_animationsSelectorScheduled && !val)
    {
        unschedule(ax::SEL_SCHEDULE(&GAFObject::processAnimations));

        m_animationsSelectorScheduled = false;
    }
}

NS_GAF_END
