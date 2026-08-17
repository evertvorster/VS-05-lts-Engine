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
// All world math is in QVector (double) because a sector can span ~1e11+ units
// (light-minutes between clusters); float would lose the relative offsets.

static const double PI       = 3.14159265;
static const double HALFPI   = 0.5 * PI;
static const double POLE_EPS = 0.05;

// Upper bound on the orbit distance (zoom-out). A sector is huge (units at
// ~1e11, light-minutes apart) and the galaxy vaster still; allow ~1e15 so the
// whole thing frames. Increase if a bigger universe.
static const double MAX_DIST = 1e15;

NavMap::NavMap()
{
    yaw_      = -0.6f;
    pitch_    = 0.35f;
    distance_ = 120.0f;
    roll_     = 0.0f;
    panX_     = 0.0f;
    panY_     = 0.0f;
    topDown_  = false;
    axis_     = 2;
    target_   = QVector( 0, 0, 0 );
}

NavMap::~NavMap() {}

void NavMap::setCamera( float yaw, float pitch, float distance )
{
    yaw_      = yaw;
    pitch_    = pitch;
    distance_ = distance;
}

void NavMap::setTarget( const QVector &center )
{
    target_ = center;
    panX_   = 0.0f;      // re-centring on a new pivot clears any pan
    panY_   = 0.0f;
}

void NavMap::setDistanceFromExtent( double halfx, double halfy, double halfz, float fov_rad )
{
    // Fit the extent's half-diagonal into the viewport at the given fov.
    double halfdiag = std::sqrt( halfx*halfx + halfy*halfy + halfz*halfz );
    double tanhalf  = std::tan( 0.5 * (fov_rad > 0.01f ? fov_rad : 1.0f) );
    distance_ = (float)( halfdiag / tanhalf );
    if (distance_ < 1.0f) distance_ = 1.0f;
    if (distance_ > MAX_DIST) distance_ = (float)MAX_DIST;
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
    // Pan is a screen-space translation of the view — it shifts the projected
    // positions sideways/vertical in HUD units, completely independent of the
    // orbit target (pivot) and the yaw/pitch rotation. So panning never changes
    // where the camera points or what it rotates around.
    panX_ += dright * 0.5f;
    panY_ += dup    * 0.5f;
}

void NavMap::zoomBy( float factor )
{
    distance_ *= factor;
    if (distance_ < 1.0f) distance_ = 1.0f;
    if (distance_ > MAX_DIST) distance_ = (float)MAX_DIST;
}

void NavMap::computeBasis( QVector &forward, QVector &right, QVector &up ) const
{
    if (topDown_) {
        // Camera looks straight down the chosen world axis; right/up span the
        // plane perpendicular to it. Built directly from the axis so the basis
        // never degenerates (forward × worldUp would be 0 at the pole).
        switch (axis_) {
        case 0:  // down X
            forward = QVector( -1, 0, 0 );
            right   = QVector( 0, 0, -1 );
            up      = QVector( 0, 1, 0 );
            break;
        case 1:  // down Y
            forward = QVector( 0, -1, 0 );
            right   = QVector( 1, 0, 0 );
            up      = QVector( 0, 0, 1 );
            break;
        default: // down Z
            forward = QVector( 0, 0, -1 );
            right   = QVector( 1, 0, 0 );
            up      = QVector( 0, 1, 0 );
            break;
        }
    } else {
        // Orbit camera: forward from yaw/pitch, right/up from forward × worldUp.
        double cp = std::cos( pitch_ ), sp = std::sin( pitch_ );
        double cy_ = std::cos( yaw_ ),  sy_ = std::sin( yaw_ );
        forward = QVector( cp * cy_, sp, -cp * sy_ );
        QVector worldUp( 0, 1, 0 );
        right = forward.Cross( worldUp );
        right.Normalize();
        up = right.Cross( forward );
        up.Normalize();
    }
    // Roll: spin the screen axes around the view direction (top-down rotation).
    if (roll_ != 0.0f) {
        double c = std::cos( roll_ ), s = std::sin( roll_ );
        QVector r = right, u = up;
        right = r * c + u * s;
        up    = u * c - r * s;
    }
}

bool NavMap::project( const QVector &world, float &sx, float &sy, float &sscale ) const
{
    QVector forward, right, up;
    computeBasis( forward, right, up );

    // Camera position behind the target along -forward.
    QVector camPos = target_ - forward * distance_;

    // Vector from the camera to the world point.
    QVector d = world - camPos;
    double along = d.Dot( forward );

    // If behind the camera, cull.
    if (along <= 0.01)
        return false;

    double rightAmt = d.Dot( right );
    double upAmt    = d.Dot( up );

    // Perspective: divide by along (distance ahead), scaled so 1.0 is at the
    // target distance. focal ~ distance_ so the target fills a reasonable area.
    double scale = distance_ / along;
    sx     = (float)( rightAmt * scale / distance_ ) + panX_;   // ~ -0.5..0.5 at target distance
    sy     = (float)( upAmt    * scale / distance_ ) + panY_;
    sscale = (float)scale;                              // 1.0 at the target distance

    return true;
}
