#include "GAFPrecompiled.h"
#include "PrimitiveDeserializer.h"

#include "GAFStream.h"

NS_GAF_BEGIN

void PrimitiveDeserializer::deserialize(GAFStream* in, ax::Vec2* out)
{
    out->x = in->readFloat();
    out->y = in->readFloat();
}

void PrimitiveDeserializer::deserialize(GAFStream* in, ax::Rect* out)
{
    deserialize(in, &out->origin);
    deserialize(in, &out->size);
}

void PrimitiveDeserializer::deserialize(GAFStream* in, ax::AffineTransform* out)
{
    in->readNBytesOfT(out, sizeof(ax::AffineTransform));
}

void PrimitiveDeserializer::deserializeSize(GAFStream* in, ax::Size* out)
{
    out->width = in->readFloat();
    out->height = in->readFloat();
}

void PrimitiveDeserializer::deserialize(GAFStream* in, ax::Color4B* out)
{
    in->readNBytesOfT(out, 4);
}

void PrimitiveDeserializer::translateColor(ax::Color4F& out, unsigned int in)
{
    GAFReadColor gcol;

    memcpy(&gcol, &in, 4);

    out.b = gcol.b / 255.f;
    out.g = gcol.g / 255.f;
    out.r = gcol.r / 255.f;
    out.a = gcol.a / 255.f;
}

NS_GAF_END