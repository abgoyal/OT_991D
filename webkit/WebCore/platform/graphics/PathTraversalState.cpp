

#include "config.h"
#include "PathTraversalState.h"

#include "Path.h"

#include <math.h>

namespace WebCore {

static const float kPathSegmentLengthTolerance = 0.00001f;

static inline FloatPoint midPoint(const FloatPoint& first, const FloatPoint& second)
{
    return FloatPoint((first.x() + second.x()) / 2.0f, (first.y() + second.y()) / 2.0f);
}

static inline float distanceLine(const FloatPoint& start, const FloatPoint& end)
{
    return sqrtf((end.x() - start.x()) * (end.x() - start.x()) + (end.y() - start.y()) * (end.y() - start.y()));
}

struct QuadraticBezier {
    QuadraticBezier() { }
    QuadraticBezier(const FloatPoint& s, const FloatPoint& c, const FloatPoint& e)
        : start(s)
        , control(c)
        , end(e)
    {
    }
    
    float approximateDistance() const
    {
        return distanceLine(start, control) + distanceLine(control, end);
    }
    
    void split(QuadraticBezier& left, QuadraticBezier& right) const
    {
        left.control = midPoint(start, control);
        right.control = midPoint(control, end);
        
        FloatPoint leftControlToRightControl = midPoint(left.control, right.control);
        left.end = leftControlToRightControl;
        right.start = leftControlToRightControl;

        left.start = start;
        right.end = end;
    }
    
    FloatPoint start;
    FloatPoint control;
    FloatPoint end;
};

struct CubicBezier {
    CubicBezier() { }
    CubicBezier(const FloatPoint& s, const FloatPoint& c1, const FloatPoint& c2, const FloatPoint& e)
        : start(s)
        , control1(c1)
        , control2(c2)
        , end(e)
    {
    }
    
    float approximateDistance() const
    {
        return distanceLine(start, control1) + distanceLine(control1, control2) + distanceLine(control2, end);
    }
        
    void split(CubicBezier& left, CubicBezier& right) const
    {    
        FloatPoint startToControl1 = midPoint(control1, control2);
        
        left.start = start;
        left.control1 = midPoint(start, control1);
        left.control2 = midPoint(left.control1, startToControl1);
        
        right.control2 = midPoint(control2, end);
        right.control1 = midPoint(right.control2, startToControl1);
        right.end = end;
        
        FloatPoint leftControl2ToRightControl1 = midPoint(left.control2, right.control1);
        left.end = leftControl2ToRightControl1;
        right.start = leftControl2ToRightControl1;
    }
    
    FloatPoint start;
    FloatPoint control1;
    FloatPoint control2;
    FloatPoint end;
};

// FIXME: This function is possibly very slow due to the ifs required for proper path measuring
// A simple speed-up would be to use an additional boolean template parameter to control whether
// to use the "fast" version of this function with no PathTraversalState updating, vs. the slow
// version which does update the PathTraversalState.  We'll have to shark it to see if that's necessary.
// Another check which is possible up-front (to send us down the fast path) would be to check if
// approximateDistance() + current total distance > desired distance
template<class CurveType>
static float curveLength(PathTraversalState& traversalState, CurveType curve)
{
    Vector<CurveType> curveStack;
    curveStack.append(curve);

    float totalLength = 0.0f;
    do {
        float length = curve.approximateDistance();
        if ((length - distanceLine(curve.start, curve.end)) > kPathSegmentLengthTolerance) {
            CurveType left, right;
            curve.split(left, right);
            curve = left;
            curveStack.append(right);
        } else {
            totalLength += length;
            if (traversalState.m_action == PathTraversalState::TraversalPointAtLength
             || traversalState.m_action == PathTraversalState::TraversalNormalAngleAtLength) {
                traversalState.m_previous = curve.start;
                traversalState.m_current = curve.end;
                if (traversalState.m_totalLength + totalLength > traversalState.m_desiredLength)
                    return totalLength;
            }
            curve = curveStack.last();
            curveStack.removeLast();
        }
    } while (!curveStack.isEmpty());
    
    return totalLength;
}

PathTraversalState::PathTraversalState(PathTraversalAction action)
    : m_action(action)
    , m_success(false)
    , m_totalLength(0.0f)
    , m_segmentIndex(0)
    , m_desiredLength(0.0f)
    , m_normalAngle(0.0f)
{
}

float PathTraversalState::closeSubpath()
{
    float distance = distanceLine(m_current, m_start);
    m_start = m_control1 = m_control2 = m_current;
    return distance;
}

float PathTraversalState::moveTo(const FloatPoint& point)
{
    m_current = m_start = m_control1 = m_control2 = point;
    return 0.0f;
}

float PathTraversalState::lineTo(const FloatPoint& point)
{
    float distance = distanceLine(m_current, point);
    m_current = m_control1 = m_control2 = point;
    return distance;
}

float PathTraversalState::quadraticBezierTo(const FloatPoint& newControl, const FloatPoint& newEnd)
{
    float distance = curveLength<QuadraticBezier>(*this, QuadraticBezier(m_current, newControl, newEnd));

    m_control1 = newControl;
    m_control2 = newEnd;

    if (m_action != TraversalPointAtLength && m_action != TraversalNormalAngleAtLength) 
        m_current = newEnd;

    return distance;
}

float PathTraversalState::cubicBezierTo(const FloatPoint& newControl1, const FloatPoint& newControl2, const FloatPoint& newEnd)
{
    float distance = curveLength<CubicBezier>(*this, CubicBezier(m_current, newControl1, newControl2, newEnd));

    m_control1 = newEnd;
    m_control2 = newControl2;
 
    if (m_action != TraversalPointAtLength && m_action != TraversalNormalAngleAtLength) 
        m_current = newEnd;

    return distance;
}

}
