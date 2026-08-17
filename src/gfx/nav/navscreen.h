#ifndef _NAVSCREEN_H_
#define _NAVSCREEN_H_

#include "gui/glut_support.h"
#include "navscreenoccupied.h"
#include "drawlist.h"
#include "navitemtypes.h"
#include "gfx/masks.h"
#include "navcomputer.h"
#include "navpath.h"
#include "navmap.h"
#include "gfx/hud.h"
#include "gnuhash.h"

#define NAVTOTALMESHCOUNT 8     //same as the button count, 1 mesh for screen and 1 per button(1+7)
#define MAXZOOM 10

// Vertical fov (radians) used when auto-framing a NavMap camera to an extent.
#define NAV_FIT_FOV 1.0f

// Minimum icon size in HUD units (~15px). HUD coords span [-1,1] = 2 units
// across the screen, so 1 unit = x_res/2 pixels; 15px ≈ 30/x_res units.
// Items never shrink below this, so zooming out to a huge extent keeps them
// visible (the current 2013-style sscale clamp of 0.2 leaves them tiny).
static inline float NavMinItemSize()
{
    const float px = 15.0f;
    const float res = (float) g_game.x_resolution;
    return (res > 0.0f) ? (2.0f*px/res) : 0.015f;
}

void Beautify( string systemfile, string &sector, string &system );
class NavigationSystem
{
public:
    class SystemIterator
    {
        vector< string >vstack;
        unsigned int    which;
        unsigned int    count;
        unsigned int    maxcount;
        vsUMap< string, bool >visited;

public: SystemIterator( string current_system, unsigned int max = 2 );
        bool done() const;
        QVector Position();
        string operator*();
        SystemIterator& next();
        SystemIterator& operator++();
    };

    class CachedSystemIterator
    {
public:
//typedef std::pair<string, QVector> SystemInfo;
        struct SystemInfo
        {
            string   name;
            QVector  position;
            std::vector< unsigned >lowerdestinations;
            GFXColor col;
            bool     part_of_path;
            std::set< NavPath* >paths;
            void     UpdateColor();
            string & GetName();
            const string& GetName() const;
            bool     isDrawable() const;
            QVector& Position();
            const QVector& Position() const;
            unsigned GetDestinationIndex( unsigned index ) const;
            unsigned GetDestinationSize() const;
            GFXColor GetColor();
            SystemInfo( const string &name );
            SystemInfo( const string &name,
                        const QVector &position,
                        const std::vector< std::string > &destinations,
                        CachedSystemIterator *csi );
            void loadData( map< string, unsigned > *index_table );
        };

private:
        friend struct SystemInfo;        //inner class needs to be friend in gcc-295
        vector< SystemInfo >systems;
        unsigned currentPosition;
        CachedSystemIterator( const CachedSystemIterator &other );       //May be really slow. Don't try this at home.
        CachedSystemIterator operator++( int );         //Also really slow because it has to use the copy constructor.

public: CachedSystemIterator();
        CachedSystemIterator( string current_system, unsigned max_systems = 2 );
        void init( string current_system, unsigned max_systems = 2 );
        bool seek( unsigned position = 0 );
        unsigned getIndex() const;
        unsigned size() const;
        bool done() const;
        SystemInfo& operator[]( unsigned pos );
        const SystemInfo& operator[]( unsigned pos ) const;
        SystemInfo& operator*();
        const SystemInfo    & operator*() const;
        SystemInfo* operator->();
        const SystemInfo* operator->() const;
        CachedSystemIterator& next();
        CachedSystemIterator& operator++();
    };

    class CachedSectorIterator
    {
public:
        class SectorInfo
        {
public:
            string  name;
            std::vector< unsigned >subsystems;
            string& GetName();
            const string& GetName() const;
            unsigned GetSubsystemIndex( unsigned index ) const;
            unsigned GetSubsystemSize() const;
            SectorInfo( const string &name );
            void AddSystem( unsigned index );
        };

private:
        friend class SectorInfo;  //inner class needs to be friend in gcc-295
        vector< SectorInfo >sectors;
        unsigned currentPosition;
        CachedSectorIterator( const CachedSectorIterator &other );       //May be really slow. Don't try this at home.
        CachedSectorIterator operator++( int );         //Also really slow because it has to use the copy constructor.

public: CachedSectorIterator();
        CachedSectorIterator( CachedSystemIterator &systemIter );
        void init( CachedSystemIterator &systemIter );
        bool seek( unsigned position = 0 );
        unsigned getIndex() const;
        unsigned size() const;
        bool done() const;
        SectorInfo& operator[]( unsigned pos );
        const SectorInfo& operator[]( unsigned pos ) const;
        SectorInfo& operator*();
        const SectorInfo    & operator*() const;
        SectorInfo* operator->();
        const SectorInfo* operator->() const;
        CachedSectorIterator& next();
        CachedSectorIterator& operator++();
    };

    PathManager *pathman;
private:
    friend class NavComputer;
    friend class CurrentPathNode;
    friend class TargetPathNode;
    friend class AbsolutePathNode;
    friend class CriteriaContains;
    friend class CriteriaOwnedBy;
    friend class CriteriaSector;
    friend class NavPath;
    NavComputer *navcomp;
    unsigned     currentsystemindex;
    unsigned     focusedsystemindex;
    unsigned     destinationsystemindex;
    unsigned     systemselectionindex;
    unsigned     sectorselectionindex;
    CachedSystemIterator   systemIter;
    CachedSectorIterator   sectorIter;
    std::vector< unsigned >path;
    class navscreenoccupied*screenoccupation;
    class Mesh*mesh[NAVTOTALMESHCOUNT];
    int   rotations; //tried to change to unsigned but gazillions of comparisons to int crop up --chuck_starchaser
    int   axis;

    float minimumitemscaledown;
    float maximumitemscaleup;

    NavMap galaxyCam;    // coherent orbit camera for the galaxy view
    NavMap systemCam;    // coherent orbit camera for the system view
    bool  galaxyNeedsRefit;   // re-fit the galaxy camera to its extent next draw
    bool  systemNeedsRefit;   // re-fit the system camera to its extent next draw

    int   path_view;
    enum PathType {PATH_OFF, PATH_ON, PATH_ONLY, PATH_MAXIMUM};

    signed int    scrolloffset;

    float         mouse_x_previous;
    float         mouse_y_previous;
    float         mouse_x_current;
    float         mouse_y_current;
    signed char   draw;
    bool          mouse_previous_state[5];
    bool          mouse_wentup[5];
    bool          mouse_wentdown[5];
    UnitContainer currentselection;
    GFXColor     *factioncolours;
    GFXColor      currentcol;
    GFXColor      destinationcol;
    GFXColor      selectcol;
    GFXColor      pathcol;

//DrawSectorList's scroll control variables
    unsigned      sectorOffset;
    unsigned      systemOffset;

    int whattodraw;
//bit 0 = undefined
//bit	1 = draw system screen / mission screen
//bit 2 = draw galaxy/system ship/mission screen
//bit 3 = draw sector list screen in mission mode

//coordinates done 'over left->right' by 'up bottom->top'
//values are 1/100 of the screen width and height
    float     screenskipby4[4]; //0 = x-small	1 = x-large	2 = y-small	3 = y-large
    float     buttonskipby4_1[4];
    float     buttonskipby4_2[4];
    float     buttonskipby4_3[4];
    float     buttonskipby4_4[4];
    float     buttonskipby4_5[4];
    float     buttonskipby4_6[4];
    float     buttonskipby4_7[4];
    TextPlane screen_objectives;
    float     meshcoordinate_x[NAVTOTALMESHCOUNT];
    float     meshcoordinate_y[NAVTOTALMESHCOUNT];
    float     meshcoordinate_z[NAVTOTALMESHCOUNT];
    float     meshcoordinate_z_delta[NAVTOTALMESHCOUNT];

    int buttonstates;   //bit0 = button1, bit1 = button2, etc
    float     system_item_scale;
    float     unselectedalpha;

//Drawing helper functions
//*************************
    void Adjust3dTransformation( bool is_system_not_galaxy );
    void RecordMinAndMax( const QVector &pos,
                          float &min_x,
                          float &max_x,
                          float &min_y,
                          float &max_y,
                          float &min_z,
                          float &max_z,
                          float &max_all );
    void DrawOriginOrientationTri( float center_nav_x, float center_nav_y, bool system_not_galaxy );

    bool CheckForSelectionQuery();
    void setCurrentSystemIndex( unsigned newSystemIndex );
    void setFocusedSystemIndex( unsigned newSystemIndex );
    void setDestinationSystemIndex( unsigned newSystemIndex );

    bool BFS( unsigned originIndex, unsigned destIndex );
    bool DoubleRootedBFS( unsigned originIndex, unsigned destIndex );
//*************************

public: NavigationSystem();
    ~NavigationSystem();
    static void DrawCircle( float x, float y, float size, const GFXColor &col );
    static void DrawHalfCircleTop( float x, float y, float size, const GFXColor &col );
    static void DrawHalfCircleBottom( float x, float y, float size, const GFXColor &col );
    static void DrawPlanet( float x, float y, float size, const GFXColor &col );
    static void DrawStation( float x, float y, float size, const GFXColor &col );
    static void DrawJump( float x, float y, float size, const GFXColor &col );
    static void DrawMissile( float x, float y, float size, const GFXColor &col );
    static void DrawTargetCorners( float x, float y, float size, const GFXColor &col );
    void setCurrentSystem( string newSystem );
    std::string getCurrentSystem();
    std::string getSelectedSystem();
    std::string getFocusedSystem();
    std::string getDestinationSystem();

    void DrawButton( float &x1, float &x2, float &y1, float &y2, int button_number, bool outline );
    void DrawButtonOutline( float &x1, float &x2, float &y1, float &y2, const GFXColor &col );
    void DrawCursor( float x, float y, float wid, float hei, const GFXColor &col );
    void DrawGrid( float &screen_x1, float &screen_x2, float &screen_y1, float &screen_y2, const GFXColor &col );

    bool TestIfInRange( float &x1, float &x2, float &y1, float &y2, float tx, float ty );
    bool TestIfInRangeBlk( float &x1, float &x2, float size, float tx, float ty );
    bool TestIfInRangeRad( float &x, float &y, float size, float tx, float ty );
    bool CheckDraw();
    void DrawSystem();
    void DrawGalaxy();
    void DrawMission();
    void DrawShip();
    void DrawSectorList();
    void DrawObjectives();
    void SetMouseFlipStatus();
    void ScreenToCoord( float &x );
    void IntersectBorder( float &x, float &y, const float &x1, const float &y1 ) const;
    void Draw();
    void Setup();
    void SetDraw( bool n );
    void ClearPriorities();
    void updatePath();

    void scroll( signed int scrollamt )
    {
        scrolloffset += scrollamt;
    }

    static int mousex;
    static int mousey;
    static int mousestat;
    static void mouseDrag( int x, int y );
    static void mouseMotion( int x, int y );
    static void mouseClick( int button, int state, int x, int y );
    static int getMouseButtonStatus()
    {
        return mousestat;
    }
    static class QVector dxyz( class QVector, double x_, double y_, double z_ );

//float Delta(float a, float b);
};

#endif
