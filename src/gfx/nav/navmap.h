#ifndef VS05_NAVMAP_H
#define VS05_NAVMAP_H

// Clean-room 3D nav map renderer for the VS-05 nav computer.
// Replaces the broken 2013 projection (TranslateCoordinates/themaxvalue/center)
// with a simple orbit camera: world points are projected through yaw/pitch/
// distance to 2D screen positions, and markers/labels are drawn there. Reuses
// the engine's data sources (universe systems/units) and line-draw primitives.
//
// Coherent camera model (VS-05 design §5): ONE camera used uniformly.
//   - Zoom  = move the camera forward/back toward the focus (the orbit distance).
//   - Pan   = translate the camera perpendicular to the view (pan the target_
//             along the camera right/up axes).
//   - Rotate = orbit (yaw/pitch) in 3D; roll (spin the screen axes around the
//              view direction) in top-down.
//   - Top-down = camera snapped to look straight down a chosen world axis; a
//     fresh snap each time 2D is entered. Left-drag rolls the map in its plane.

#include "../vec.h"
#include "gfxlib_struct.h"

class Matrix;
struct QVector;

class NavMap
{
public:
    NavMap();
    ~NavMap();

    // View state
    void setCamera( float yaw, float pitch, float distance );
    void setTarget( const Vector &center );      // orbit target (galaxy or system center)

    // Auto-frame: set the orbit distance so the given extent (half-widths of the
    // bounding box) fits the viewport at the given vertical fov (radians).
    void setDistanceFromExtent( float halfx, float halfy, float halfz, float fov_rad );

    // Top-down mode: snap the camera to look straight down the given world axis
    // (0=X, 1=Y, 2=Z), aimed at the target. Each call is a fresh top-down snap.
    void setTopDown( bool on, int axis );
    bool topDown() const { return topDown_; }

    // Project a world point to screen space. Returns true if it's in front of
    // the camera. On success fills sx, sy (HUD coords, roughly -1..1) and
    // sscale (a perspective size factor, 1.0 at the target distance).
    bool project( const Vector &world, float &sx, float &sy, float &sscale ) const;

    // Interaction helpers (call from the nav input handler)
    void orbitBy( float dyaw, float dpitch );     // 3D rotate (left-drag)
    void rollBy( float droll );                   // top-down rotate (left-drag in 2D)
    void panBy( float dright, float dup );        // pan (translate target_ along camera right/up)
    void zoomBy( float factor );                  // wheel zoom (move camera forward/back)

    float yaw() const { return yaw_; }
    float pitch() const { return pitch_; }
    float distance() const { return distance_; }
    float roll() const { return roll_; }

private:
    // Fill forward (camera->target), right, up for the current view state.
    // Applies roll_ to right/up (spin around the view direction).
    void computeBasis( Vector &forward, Vector &right, Vector &up ) const;

    float yaw_, pitch_, distance_, roll_;
    bool  topDown_;
    int   axis_;          // 0=X, 1=Y, 2=Z — which world axis is "down" in top-down
    Vector target_;
};

#endif
