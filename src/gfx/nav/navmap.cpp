#include "navmap.h"
#include "gfxlib_struct.h"
#include <cmath>

// Clean-room 3D nav map projection.
//
// Camera: orbits the target point at a distance along a direction given by
// yaw/pitch. It looks at the target. A world point is placed in a camera-relative
// frame, then perspective-projected to a 2D screen position (plus a size factor so
// far objects render smaller). This replaces the broken 2013 themaxvalue/center
// projection with something simple, predictable, and correct in 3D.
//
// Coherent camera model: zoom = distance (camera forward/back), pan = translate
// the target_ along the camera right/up axes (perpendicular to view), rotate =
// orbit (3D) or roll (top-down). Top-down snaps the camera to look straight down
// a chosen world axis; the screen axes are built directly from the axis so the
// projection never degenerates at the pole.

static const float PI       = 3.14159265f;
static const float HALFPI   = 0.5f * PI;
static const float POLE_EPS = 0.05f;    // stay this far from the ±90° pole

NavMap::NavMap()
{
    yaw_      = -0.6f;
    pitch_    = 0.35f;
    distance_ = 120.0f;
    roll_     = 0.0f;
    topDown_  = false;
    axis_     = 2;
    target_   = Vector( 0, 0, 0 );
}

NavMap::~NavMap() {}

void NavMap::setCamera( float yaw, float pitch, float distance )
{
    yaw_      = yaw;
    pitch_    = pitch;
    distance_ = distance;
}

void NavMap::setTarget( const Vector &center )
{
    target_ = center;
}

void NavMap::setDistanceFromExtent( float halfx, float halfy, float halfz, float fov_rad )
{
    // Fit the extent's half-diagonal into the viewport at the given fov.
    float halfdiag = std::sqrt( halfx*halfx + halfy*halfy + halfz*halfz );
    float tanhalf  = std::tan( 0.5f * (fov_rad > 0.01f ? fov_rad : 1.0f) );
    distance_ = halfdiag / tanhalf;
    if (distance_ < 1.0f) distance_ = 1.0f;
    if (distance_ > 1e7f) distance_ = 1e7f;
}

void NavMap::setTopDown( bool on, int axis )
{
    topDown_ = on;
    axis_    = on ? axis : axis_;
    roll_    = 0.0f;      // fresh top-down snap: no residual spin
}

void NavMap::orbitBy( float dyaw, float dpitch )
{
    if (topDown_)
        return;                 // orbit is meaningless in top-down; rotate via rollBy
    yaw_   += dyaw;
    pitch_ += dpitch;
    if (pitch_ >  HALFPI - POLE_EPS) pitch_ =  HALFPI - POLE_EPS;
    if (pitch_ < -HALFPI + POLE_EPS) pitch_ = -HALFPI + POLE_EPS;
}

void NavMap::rollBy( float droll )
{
    roll_ += droll;
}

void NavMap::panBy( float dright, float dup )
{
    // Translate the orbit target along the camera's right/up axes — i.e. move
    // the camera perpendicular to the view without changing direction/distance.
    Vector forward, right, up;
    computeBasis( forward, right, up );
    target_ += right * (dright * distance_ * 0.5f);
    target_ += up    * (dup    * distance_ * 0.5f);
}

void NavMap::zoomBy( float factor )
{
    distance_ *= factor;
    if (distance_ < 1.0f) distance_ = 1.0f;
    if (distance_ > 1e7f) distance_ = 1e7f;
}

void NavMap::computeBasis( Vector &forward, Vector &right, Vector &up ) const
{
    if (topDown_) {
        // Camera looks straight down the chosen world axis; right/up span the
        // plane perpendicular to it. Built directly from the axis so the basis
        // never degenerates (forward × worldUp would be 0 at the pole).
        switch (axis_) {
        case 0:  // down X
            forward = Vector( -1, 0, 0 );
            right   = Vector( 0, 0, -1 );
            up      = Vector( 0, 1, 0 );
            break;
        case 1:  // down Y
            forward = Vector( 0, -1, 0 );
            right   = Vector( 1, 0, 0 );
            up      = Vector( 0, 0, 1 );
            break;
        default: // down Z
            forward = Vector( 0, 0, -1 );
            right   = Vector( 1, 0, 0 );
            up      = Vector( 0, 1, 0 );
            break;
        }
    } else {
        // Orbit camera: forward from yaw/pitch, right/up from forward × worldUp.
        float cp = std::cos( pitch_ ), sp = std::sin( pitch_ );
        float cy_ = std::cos( yaw_ ),  sy_ = std::sin( yaw_ );
        forward = Vector( cp * cy_, sp, -cp * sy_ );
        Vector worldUp( 0, 1, 0 );
        right = forward.Cross( worldUp );
        right.Normalize();
        up = right.Cross( forward );
        up.Normalize();
    }
    // Roll: spin the screen axes around the view direction (top-down rotation).
    if (roll_ != 0.0f) {
        float c = std::cos( roll_ ), s = std::sin( roll_ );
        Vector r = right, u = up;
        right = r * c + u * s;
        up    = u * c - r * s;
    }
}

bool NavMap::project( const Vector &world, float &sx, float &sy, float &sscale ) const
{
    Vector forward, right, up;
    computeBasis( forward, right, up );

    // Camera position behind the target along -forward.
    Vector camPos = target_ - forward * distance_;

    // Vector from the camera to the world point.
    Vector d = world - camPos;
    float along = d.Dot( forward );

    // If behind the camera, cull.
    if (along <= 0.01f)
        return false;

    float rightAmt = d.Dot( right );
    float upAmt    = d.Dot( up );

    // Perspective: divide by along (distance ahead), scaled so 1.0 is at the
    // target distance. focal ~ distance_ so the target fills a reasonable area.
    float scale = distance_ / along;
    sx     = rightAmt * scale / distance_;   // ~ -0.5..0.5 at target distance
    sy     = upAmt    * scale / distance_;
    sscale = scale;                           // 1.0 at the target distance

    return true;
}
