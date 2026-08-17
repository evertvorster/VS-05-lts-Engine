#ifndef VS05_NAVMAP_H
#define VS05_NAVMAP_H

// Free-fly 3D nav map camera for the VS-05 nav computer.
// Replaces the broken 2013 projection and the earlier orbit camera with a simple
// position + orientation camera: you fly through the scene. World points are
// projected through the camera's position and yaw/pitch to 2D screen positions,
// with markers/labels drawn there.
//
// Camera model (VS-05 design §5):
//   - Rotate = change yaw/pitch (look around); the camera stays put.
//   - Pan    = translate the camera position sideways/vertical along its
//              right/up axes (a world-space strafe). Independent of rotation.
//   - Zoom   = move the camera forward/back along its view direction. The step
//              is scaled by the distance to the nearest significant object
//              (navNearDist), so it's fast through empty space and fine near
//              objects.
//
// All world/camera math uses QVector (double) so huge sector coordinates (a
// sector can span ~1e11+ units — light-minutes between clusters) keep enough
// precision.

#include "../vec.h"
#include "gfxlib_struct.h"

class Matrix;

class NavMap
{
public:
    NavMap();
    ~NavMap();

    // View state: set the camera position and orientation.
    void setCamera( float yaw, float pitch );
    void setPosition( const QVector &pos );

    // Auto-frame: place the camera so the given extent (half-widths of the
    // bounding box) fits the viewport at the given vertical fov, looking at the
    // centre. Nominal distance (nomDist_) is recorded for size normalisation.
    void setFraming( const QVector &center, double halfx, double halfy, double halfz, float fov_rad );

    // Project a world point to screen space. Returns true if it's in front of
    // the camera. On success fills sx, sy (HUD coords, roughly -1..1) and
    // sscale (a perspective size factor, 1.0 at the nominal distance).
    bool project( const QVector &world, float &sx, float &sy, float &sscale ) const;

    // Interaction helpers (call from the nav input handler)
    void orbitBy( float dyaw, float dpitch );      // look around (change yaw/pitch)
    void panBy( float dright, float dup );         // strafe: translate camera right/up
    void zoomBy( float dist );                     // move camera forward by dist (zoom in if >0)

    float yaw() const { return yaw_; }
    float pitch() const { return pitch_; }
    float nominalDistance() const { return nomDist_; }
    const QVector& position() const { return pos_; }

    // View direction (unit), from yaw/pitch. Used to test whether an object is
    // in front of the camera and to find the view focus.
    QVector forward() const;

private:
    // Fill forward (view direction), right, up for the current orientation.
    void computeBasis( QVector &forward, QVector &right, QVector &up ) const;

    QVector pos_;          // camera position (world space)
    float   yaw_, pitch_;
    float   nomDist_;      // camera-to-centre distance set at auto-frame (for sizing)
};

#endif
