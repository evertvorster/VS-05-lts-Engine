#include "navmap.h"
#include "gfxlib_struct.h"
#include <cmath>

// Free-fly 3D nav map camera.
//
// The camera has a position and an orientation (yaw/pitch). A world point is
// placed in a camera-relative frame, then perspective-projected to a 2D screen
// position (plus a size factor so far objects render smaller).
//
// All world math is in QVector (double) because a sector can span ~1e11+ units
// (light-minutes between clusters); float would lose the relative offsets.

static const double PI       = 3.14159265;
static const double HALFPI   = 0.5 * PI;
static const double POLE_EPS = 0.05;

// Upper bound on the camera-to-centre distance (zoom-out). A sector is huge
// (units at ~1e11, light-minutes apart) and the galaxy vaster still; allow
// ~1e15 so the whole thing frames. Increase if a bigger universe.
static const double MAX_DIST = 1e15;

NavMap::NavMap()
{
    yaw_     = -0.6f;
    pitch_   = 0.35f;
    nomDist_ = 120.0f;
    pos_     = QVector( 0, 0, -120 );   // default: a little in front of the origin
}

NavMap::~NavMap() {}

void NavMap::setCamera( float yaw, float pitch )
{
    yaw_   = yaw;
    pitch_ = pitch;
}

void NavMap::setPosition( const QVector &pos )
{
    pos_ = pos;
}

void NavMap::setFraming( const QVector &center, double halfx, double halfy, double halfz, float fov_rad )
{
    // Fit the extent's half-diagonal into the viewport at the given fov.
    double halfdiag = std::sqrt( halfx*halfx + halfy*halfy + halfz*halfz );
    double tanhalf  = std::tan( 0.5 * (fov_rad > 0.01f ? fov_rad : 1.0f) );
    double dist     = halfdiag / tanhalf;
    if (dist < 1.0) dist = 1.0;
    if (dist > MAX_DIST) dist = MAX_DIST;
    nomDist_ = (float)dist;

    // Place the camera looking at the centre, from the current orientation.
    QVector forward, right, up;
    computeBasis( forward, right, up );
    pos_ = center - forward * dist;
}

void NavMap::orbitBy( float dyaw, float dpitch )
{
    yaw_   += dyaw;
    pitch_ += dpitch;
    if (pitch_ >  HALFPI - POLE_EPS) pitch_ =  HALFPI - POLE_EPS;
    if (pitch_ < -HALFPI + POLE_EPS) pitch_ = -HALFPI + POLE_EPS;
}

void NavMap::panBy( float dright, float dup )
{
    // Strafe the camera along its right/up axes (perpendicular to the view).
    // A pure world-space translation — rotation and the view direction are
    // unaffected, so panning never re-aims the camera.
    QVector forward, right, up;
    computeBasis( forward, right, up );
    pos_ += right * dright;
    pos_ += up    * dup;
}

void NavMap::zoomBy( float dist )
{
    // Move the camera along its view direction. dist is a world-space distance
    // (positive = forward = zoom in). The caller scales it by the distance to
    // the nearest object so zoom is fast in empty space and fine near objects.
    QVector forward, right, up;
    computeBasis( forward, right, up );
    pos_ += forward * dist;
}

QVector NavMap::forward() const
{
    double cp = std::cos( pitch_ ), sp = std::sin( pitch_ );
    double cy_ = std::cos( yaw_ ),  sy_ = std::sin( yaw_ );
    return QVector( cp * cy_, sp, -cp * sy_ );
}

void NavMap::computeBasis( QVector &forward, QVector &right, QVector &up ) const
{
    // Forward from yaw/pitch, right/up from forward × worldUp.
    double cp = std::cos( pitch_ ), sp = std::sin( pitch_ );
    double cy_ = std::cos( yaw_ ),  sy_ = std::sin( yaw_ );
    forward = QVector( cp * cy_, sp, -cp * sy_ );
    QVector worldUp( 0, 1, 0 );
    right = forward.Cross( worldUp );
    right.Normalize();
    up = right.Cross( forward );
    up.Normalize();
}

bool NavMap::project( const QVector &world, float &sx, float &sy, float &sscale ) const
{
    QVector forward, right, up;
    computeBasis( forward, right, up );

    // Vector from the camera to the world point.
    QVector d = world - pos_;
    double along = d.Dot( forward );

    // If behind the camera, cull.
    if (along <= 0.01)
        return false;

    double rightAmt = d.Dot( right );
    double upAmt    = d.Dot( up );

    // Perspective: divide by along (distance ahead). focal ~ nomDist_ so the
    // extent fills a reasonable area at the nominal distance.
    double focal = nomDist_;
    double scale = focal / along;
    sx     = (float)( rightAmt * scale / focal );   // ~ -0.5..0.5 at nominal distance
    sy     = (float)( upAmt    * scale / focal );
    sscale = (float)scale;                          // 1.0 at the nominal distance

    return true;
}
